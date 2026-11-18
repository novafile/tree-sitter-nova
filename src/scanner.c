/*
 * External scanner for tree-sitter-nova.
 *
 * Produces the context-sensitive tokens of NOVA text (spec §4):
 *
 *   _newline, _indent,           block structure (§4.3), Python-style. All
 *   _indent_dash, _dedent        four are zero-width; the line breaks
 *                                themselves are skipped as extras. Blank and
 *                                comment-only lines never produce tokens.
 *                                Indentation is exact: a nested block is
 *                                two spaces deeper than its parent line
 *                                (`_indent`), or four spaces deeper than a
 *                                list item's dash for the block of the
 *                                first field on the dash line
 *                                (`_indent_dash`, `- key:` + block). A line
 *                                at any other indentation, or with a tab in
 *                                its indentation, gets no token at all, so
 *                                the parser reports an error there.
 *   _key_start                   zero-width marker before `key:` /
 *                                `key[N]{…}:` (decides key vs. value).
 *   plain_scalar                 the rest of the line after `key: ` / `- `,
 *   (_tpl_plain_scalar)          up to a comment (§4.4). Not produced when
 *                                the text is exactly a number, `true`,
 *                                `false` or `null`: the internal lexer then
 *                                produces that token instead. In template
 *                                documents (`_tpl_plain_scalar`) `$` is
 *                                reserved for references (§9.2).
 *   _block_string_begin[_dash]   zero-width; starts a block string (§4.6)
 *                                whose content is indented parent+2
 *                                (parent+4 for `- key: |`).
 *   _block_string_fragment       block-string content; the template
 *   (_tpl_…)                     variant stops at each `${`.
 *   _block_string_end            zero-width; ends a block string.
 *   _table_start                 zero-width before a top-level `[N]{` (§4.9).
 *   _row_start                   zero-width before a table row.
 *   _table_end                   zero-width; ends a table without rows on
 *                                a dash line (`- t[0]{a}:`).
 *   document_separator           a `---` line between the documents of a
 *                                text stream (§4.11).
 *   indentation_error,           never accepted by a rule. Where a line is
 *   table_row_error              indented wrongly, or a table row does not
 *                                fit its header (row count, §4.9: exactly N
 *                                rows; cell count: one per column), the
 *                                structural token before the line carries a
 *                                pending error; the next scan emits one of
 *                                these over the line break and indentation,
 *                                so the tree has an ERROR there, and during
 *                                error recovery the scanner produces the
 *                                token the line starts with (_key_start,
 *                                _row_start), so the parser resumes on that
 *                                line. See scan_error_token.
 *
 * The scanner state (indentation stack, open block string, table counters,
 * pending error) is serialized after every external token, so incremental
 * reparsing re-lexes everything whose context changed. Every table ends
 * with an external token (_newline, _dedent or _table_end), so a reused
 * table never leaves a stale "rows expected" state behind.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tree_sitter/array.h"
#include "tree_sitter/parser.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

enum TokenType {
  NEWLINE,
  INDENT,
  INDENT_DASH,
  DEDENT,
  KEY_START,
  PLAIN_SCALAR,
  TPL_PLAIN_SCALAR,
  BLOCK_STRING_BEGIN,
  BLOCK_STRING_BEGIN_DASH,
  BLOCK_STRING_FRAGMENT,
  TPL_BLOCK_STRING_FRAGMENT,
  BLOCK_STRING_END,
  TABLE_START,
  ROW_START,
  TABLE_END,
  DOCUMENT_SEPARATOR,
  INDENTATION_ERROR,
  TABLE_ROW_ERROR,
  RESERVED_CHARACTER,
  ERROR_SENTINEL,
};

/* Error reporting (see scan_error_token). */
enum {
  ERR_NONE,
  ERR_PENDING_LINE, /* the next line is wrong: emit an error token before it */
  ERR_PENDING_ROW,  /* the same, for a table row (count or cells) */
  ERR_PENDING_NO_ROWS, /* the same, for a table without rows */
  ERR_NO_ROWS_NEXT, /* after _table_end of a table without rows: report it
                       with the next structural token */
  ERR_RECOVER_LINE, /* after the error token: resume at the line's key */
  ERR_RECOVER_ROW,  /* after the error token: resume at the row */
};

#define NO_TABLE (-1)
#define MAX_INDENT 0x7FF0
/* Marks an indentation level pushed by _indent_dash. */
#define DASH_LEVEL 0x8000
#define MAX_ROWS 0x7FFFFFFF

typedef struct {
  Array(uint16_t) indents;
  /* Content indentation of the block string being scanned, 0 if none. */
  uint16_t block_string_indent;
  /*
   * The table being read (§4.9): NO_TABLE, or the number of rows still
   * expected. `table_indent` is 0 between the header and the first row
   * (pending), and the row indentation afterwards (active).
   */
  int32_t table_rows;
  uint16_t table_cols;
  uint16_t table_indent;
  /* ERR_* */
  uint8_t error;
} Scanner;

#define CLASSIFY_BUFFER 128

static inline void advance(TSLexer *lexer) { lexer->advance(lexer, false); }

static inline void skip(TSLexer *lexer) { lexer->advance(lexer, true); }

static inline bool is_eol(TSLexer *lexer) {
  return lexer->eof(lexer) || lexer->lookahead == '\n' || lexer->lookahead == '\r';
}

static inline bool is_inline_space(int32_t c) { return c == ' ' || c == '\t'; }

static inline bool is_digit(int32_t c) { return c >= '0' && c <= '9'; }

static inline bool is_ident_start(int32_t c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

static inline bool is_ident_char(int32_t c) { return is_ident_start(c) || is_digit(c) || c == '-'; }

/* Characters that may not begin a plain scalar (§4.4). `$` begins a
   reference in templates and is reserved in data documents (§9.2). */
static bool is_reserved_start(int32_t c, bool template) {
  switch (c) {
    case '!': case '"': case '#': case '%': case '&': case '\'':
    case '*': case ',': case ':': case '<': case '=': case '>': case '?':
    case '@': case '[': case '\\': case ']': case '^': case '`': case '{':
    case '|': case '}': case '~': case '(': case ')':
      return true;
    case '$': /* reserved in data documents too (§9.2) */
      return true;
    default:
      return false;
  }
}

/* The number grammar of §4.5, matched exactly. */
static bool is_number(const char *s, size_t n) {
  size_t i = 0;
  if (i < n && s[i] == '-') i++;
  if (i >= n) return false;
  if (s[i] == '0') {
    i++;
  } else if (s[i] >= '1' && s[i] <= '9') {
    while (i < n && is_digit(s[i])) i++;
  } else {
    return false;
  }
  if (i == n) return true;
  if (s[i] == '/') {
    i++;
    if (i >= n || s[i] < '1' || s[i] > '9') return false;
    while (i < n && is_digit(s[i])) i++;
    return i == n;
  }
  if (s[i] == '.') {
    i++;
    if (i >= n || !is_digit(s[i])) return false;
    while (i < n && is_digit(s[i])) i++;
  }
  if (i < n && (s[i] == 'e' || s[i] == 'E')) {
    i++;
    if (i < n && (s[i] == '+' || s[i] == '-')) i++;
    if (i >= n || !is_digit(s[i])) return false;
    while (i < n && is_digit(s[i])) i++;
  }
  return i == n;
}

static bool is_keyword(const char *s, size_t n) {
  return (n == 4 && memcmp(s, "true", 4) == 0) || (n == 5 && memcmp(s, "false", 5) == 0) ||
         (n == 4 && memcmp(s, "null", 4) == 0);
}

static inline uint16_t level(uint16_t v) { return v & (uint16_t)~DASH_LEVEL; }

static inline uint16_t top_indent(Scanner *scanner) { return level(*array_back(&scanner->indents)); }

static bool scan_content(Scanner *scanner, TSLexer *lexer, const bool *valid_symbols);

static inline void clear_table(Scanner *scanner) {
  scanner->table_rows = NO_TABLE;
  scanner->table_cols = 0;
  scanner->table_indent = 0;
}

/* ------------------------------------------------------------------ */
/* Lines ahead                                                         */
/* ------------------------------------------------------------------ */

typedef struct {
  /* Indentation of the first non-blank line (comment or not), -1 if none. */
  int32_t first_nonblank;
  /* Indentation of the first comment-only line, -1 if none. */
  int32_t first_comment;
  /* Indentation of the next content line (0 at end of input). */
  uint32_t content;
  /* The content line's indentation contains a tab (§4.1). */
  bool tab;
  bool eof;
} LinesAhead;

/*
 * From an end of line, skip line breaks, blank lines and comment-only lines
 * and measure the next content line. Everything is skipped: the caller emits
 * only zero-width tokens (or a token that starts at the content line).
 */
static void scan_lines_ahead(TSLexer *lexer, LinesAhead *info) {
  info->first_nonblank = -1;
  info->first_comment = -1;
  info->content = 0;
  info->tab = false;
  info->eof = false;
  for (;;) {
    if (lexer->eof(lexer)) {
      info->eof = true;
      info->content = 0;
      return;
    }
    if (lexer->lookahead == '\r') skip(lexer);
    if (lexer->lookahead == '\n') skip(lexer);
    uint32_t indent = 0;
    bool tab = false;
    for (;;) {
      if (lexer->lookahead == ' ') {
        indent++;
      } else if (lexer->lookahead == '\t' || lexer->lookahead == '\f') {
        indent++;
        tab = true;
      } else {
        break;
      }
      skip(lexer);
    }
    if (lexer->eof(lexer)) {
      info->eof = true;
      info->content = 0;
      return;
    }
    if (lexer->lookahead == '\n' || lexer->lookahead == '\r') continue;
    if (info->first_nonblank < 0) info->first_nonblank = (int32_t)indent;
    if (lexer->lookahead == '#') {
      if (info->first_comment < 0) info->first_comment = (int32_t)indent;
      while (!lexer->eof(lexer) && lexer->lookahead != '\n' && lexer->lookahead != '\r') skip(lexer);
      continue;
    }
    info->content = indent;
    info->tab = tab;
    return;
  }
}

/* At the first character of a line: is it exactly `---`? Consumes it. */
static bool scan_separator(TSLexer *lexer) {
  for (int i = 0; i < 3; i++) {
    if (lexer->lookahead != '-') return false;
    advance(lexer);
  }
  lexer->mark_end(lexer);
  if (lexer->lookahead == '\r') {
    advance(lexer);
    return lexer->eof(lexer) || lexer->lookahead == '\n';
  }
  return is_eol(lexer);
}

/* ------------------------------------------------------------------ */
/* Tables                                                              */
/* ------------------------------------------------------------------ */

/*
 * At `[`: read `[N]{col, …}` through the closing `}`, counting the rows
 * and the columns (commas outside quoted column names).
 */
static bool scan_table_header(TSLexer *lexer, int32_t *rows, uint16_t *cols) {
  if (lexer->lookahead != '[') return false;
  advance(lexer);
  if (!is_digit(lexer->lookahead)) return false;
  int64_t n = 0;
  while (is_digit(lexer->lookahead)) {
    if (n < MAX_ROWS) n = n * 10 + (lexer->lookahead - '0');
    advance(lexer);
  }
  if (lexer->lookahead != ']') return false;
  advance(lexer);
  if (lexer->lookahead != '{') return false;
  advance(lexer);
  uint32_t c = 1;
  for (;;) {
    if (is_eol(lexer)) return false;
    int32_t ch = lexer->lookahead;
    advance(lexer);
    if (ch == '}') break;
    if (ch == ',') {
      c++;
    } else if (ch == '"') {
      while (!is_eol(lexer) && lexer->lookahead != '"') {
        if (lexer->lookahead == '\\') advance(lexer);
        if (!is_eol(lexer)) advance(lexer);
      }
      if (lexer->lookahead != '"') return false;
      advance(lexer);
    }
  }
  *rows = n > MAX_ROWS ? MAX_ROWS : (int32_t)n;
  *cols = c > 0xFFFF ? 0xFFFF : (uint16_t)c;
  return true;
}

static inline bool is_flow_delimiter(int32_t c) {
  return c == ',' || c == '[' || c == ']' || c == '{' || c == '}' || c == '(' || c == ')' || c == '"';
}

/*
 * At the first character of a table row: count its cells (top-level
 * commas + 1). A cell is a flow value, so brackets may span lines and
 * strings may contain commas; comments start at a token boundary (§4.1).
 */
static uint32_t count_cells(TSLexer *lexer) {
  uint32_t cells = 1;
  uint32_t depth = 0;
  bool boundary = true;
  for (;;) {
    if (lexer->eof(lexer)) break;
    int32_t c = lexer->lookahead;
    if (c == '\n' || c == '\r') {
      if (depth == 0) break;
      advance(lexer);
      boundary = true;
      continue;
    }
    if (c == '#' && boundary) {
      while (!is_eol(lexer)) advance(lexer);
      continue;
    }
    if (c == '"') {
      advance(lexer);
      while (!is_eol(lexer) && lexer->lookahead != '"') {
        if (lexer->lookahead == '\\') advance(lexer);
        if (!is_eol(lexer)) advance(lexer);
      }
      if (lexer->lookahead == '"') advance(lexer);
      boundary = true;
      continue;
    }
    if (c == '[' || c == '{' || c == '(') {
      depth++;
    } else if (c == ']' || c == '}' || c == ')') {
      if (depth > 0) depth--;
    } else if (c == ',' && depth == 0) {
      cells++;
    }
    boundary = is_inline_space(c) || is_flow_delimiter(c);
    advance(lexer);
  }
  return cells;
}

/* ------------------------------------------------------------------ */
/* Content at the current position: key marker, table start, plain     */
/* ------------------------------------------------------------------ */

enum KeyState {
  KS_DEAD,
  KS_IDENT,
  KS_ROWS_FIRST,
  KS_ROWS,
  KS_BRACE,
  KS_COLUMNS,
  KS_COLUMN_STRING,
  KS_COLUMN_ESCAPE,
  KS_TABLE_CLOSE,
  KS_COLON,
};

static void emit_key_start(Scanner *scanner, TSLexer *lexer, bool table, int64_t rows, uint32_t cols) {
  if (table) {
    scanner->table_rows = rows > MAX_ROWS ? MAX_ROWS : (int32_t)rows;
    scanner->table_cols = cols > 0xFFFF ? 0xFFFF : (uint16_t)cols;
    scanner->table_indent = 0;
  } else {
    clear_table(scanner);
  }
  lexer->result_symbol = KEY_START;
}

static bool scan_quoted_key(Scanner *scanner, TSLexer *lexer) {
  advance(lexer); /* opening quote */
  for (;;) {
    if (is_eol(lexer)) return false;
    int32_t c = lexer->lookahead;
    advance(lexer);
    if (c == '\\') {
      if (is_eol(lexer)) return false;
      advance(lexer);
    } else if (c == '"') {
      break;
    }
  }
  bool table = false;
  int32_t rows = 0;
  uint16_t cols = 0;
  if (lexer->lookahead == '[') {
    if (!scan_table_header(lexer, &rows, &cols)) return false;
    table = true;
  }
  if (lexer->lookahead != ':') return false;
  advance(lexer);
  if (!(is_eol(lexer) || is_inline_space(lexer->lookahead) || lexer->lookahead == '#')) return false;
  emit_key_start(scanner, lexer, table, rows, cols);
  return true;
}

static bool scan_content(Scanner *scanner, TSLexer *lexer, const bool *valid_symbols) {
  lexer->mark_end(lexer);
  int32_t c = lexer->lookahead;

  if (c == '[') {
    if (!valid_symbols[TABLE_START]) return false;
    int32_t rows;
    uint16_t cols;
    if (!scan_table_header(lexer, &rows, &cols)) return false;
    scanner->table_rows = rows;
    scanner->table_cols = cols;
    scanner->table_indent = 0;
    lexer->result_symbol = TABLE_START;
    return true;
  }

  bool want_key = valid_symbols[KEY_START];
  bool template = valid_symbols[TPL_PLAIN_SCALAR];
  bool want_plain = valid_symbols[PLAIN_SCALAR] || template;

  if (c == '"') {
    if (want_key && scan_quoted_key(scanner, lexer)) return true;
    return false;
  }

  if (!want_key && !want_plain) return false;
  /* `$x` in a data document (§9.2): report the token itself, so the error
     stays on this value instead of swallowing the surrounding record. */
  if (c == '$' && valid_symbols[PLAIN_SCALAR]) {
    while (!is_eol(lexer) && !is_inline_space(lexer->lookahead)) advance(lexer);
    lexer->mark_end(lexer);
    lexer->result_symbol = RESERVED_CHARACTER;
    return true;
  }
  if (is_reserved_start(c, template)) return false;

  enum KeyState ks = (want_key && is_ident_start(c)) ? KS_IDENT : KS_DEAD;
  if (!want_plain && ks == KS_DEAD) return false;

  char buf[CLASSIFY_BUFFER];
  size_t len = 0;         /* characters stored in buf */
  size_t content_len = 0; /* length up to the last non-space character */
  bool has_content = false; /* any non-space character consumed (incl. non-ASCII) */
  bool classifiable = true;
  bool prev_space = false;
  /* A non-space character was consumed but not marked (a key was still possible). */
  bool unmarked = false;
  bool table = false;
  int64_t rows = 0;
  uint32_t cols = 0;

  for (;;) {
    bool eol = is_eol(lexer);
    c = lexer->lookahead;
    bool space = !eol && is_inline_space(c);

    if (ks == KS_COLON) {
      if (eol || space || c == '#') {
        emit_key_start(scanner, lexer, table, rows, cols);
        return true; /* zero-width: mark_end is still at the start */
      }
      ks = KS_DEAD;
    }

    if (eol) break;
    if (c == '#' && prev_space && ks != KS_COLUMN_STRING) break;

    switch (ks) {
      case KS_IDENT:
        if (c == ':') ks = KS_COLON;
        else if (c == '[') ks = KS_ROWS_FIRST;
        else if (!is_ident_char(c)) ks = KS_DEAD;
        break;
      case KS_ROWS_FIRST:
      case KS_ROWS:
        if (is_digit(c)) {
          if (rows < MAX_ROWS) rows = rows * 10 + (c - '0');
          ks = KS_ROWS;
        } else if (c == ']' && ks == KS_ROWS) {
          ks = KS_BRACE;
        } else {
          ks = KS_DEAD;
        }
        break;
      case KS_BRACE:
        if (c == '{') {
          ks = KS_COLUMNS;
          cols = 1;
        } else {
          ks = KS_DEAD;
        }
        break;
      case KS_COLUMNS:
        if (c == '}') {
          ks = KS_TABLE_CLOSE;
          table = true;
        } else if (c == ',') {
          cols++;
        } else if (c == '"') {
          ks = KS_COLUMN_STRING;
        }
        break;
      case KS_COLUMN_STRING:
        if (c == '\\') ks = KS_COLUMN_ESCAPE;
        else if (c == '"') ks = KS_COLUMNS;
        break;
      case KS_COLUMN_ESCAPE:
        ks = KS_COLUMN_STRING;
        break;
      case KS_TABLE_CLOSE:
        ks = c == ':' ? KS_COLON : KS_DEAD;
        break;
      default:
        break;
    }

    if (!want_plain && ks == KS_DEAD) return false;

    /* The key candidate just died: the end of the previous character is a
       valid end for the plain scalar. */
    if (ks == KS_DEAD && unmarked && !prev_space) {
      lexer->mark_end(lexer);
      unmarked = false;
    }

    if (len < CLASSIFY_BUFFER && c < 128) {
      buf[len++] = (char)c;
    } else {
      classifiable = false;
    }
    advance(lexer);

    if (space) {
      prev_space = true;
    } else {
      prev_space = false;
      has_content = true;
      content_len = len;
      /* While a key is still possible the token must stay zero-width. */
      if (ks == KS_DEAD) {
        lexer->mark_end(lexer);
        unmarked = false;
      } else {
        unmarked = true;
      }
    }
  }

  if (!want_plain) return false;
  if (!has_content) return false;
  if (unmarked) lexer->mark_end(lexer);

  /* Spellings that are not plain scalars (§4.4). */
  if (len >= 3 && buf[0] == '.' && buf[1] == '.' && buf[2] == '.') return false;
  if (len >= 1 && buf[0] == '-' && (content_len == 1 || buf[1] == ' ' || buf[1] == '\t')) return false;

  /* Exact numbers and keywords are lexed by the internal lexer. */
  if (classifiable && (is_number(buf, content_len) || is_keyword(buf, content_len))) return false;

  lexer->result_symbol = template ? TPL_PLAIN_SCALAR : PLAIN_SCALAR;
  return true;
}

/* ------------------------------------------------------------------ */
/* Block strings                                                       */
/* ------------------------------------------------------------------ */

/*
 * From an end of line inside a block string: is there more content? Leaves
 * the lexer at the first character after the next content line's
 * indentation. `consume` selects advance (part of the token) or skip.
 */
static bool block_string_continues(TSLexer *lexer, uint16_t indent, bool consume) {
  for (;;) {
    if (lexer->eof(lexer)) return false;
    if (lexer->lookahead == '\r') lexer->advance(lexer, !consume);
    if (lexer->lookahead == '\n') lexer->advance(lexer, !consume);
    uint32_t spaces = 0;
    while (lexer->lookahead == ' ') {
      spaces++;
      lexer->advance(lexer, !consume);
    }
    if (lexer->eof(lexer)) return false;
    int32_t c = lexer->lookahead;
    if (c == '\n' || c == '\r') continue; /* blank line */
    if (spaces >= indent) return true;
    /* whitespace-only line shorter than the indentation is blank */
    bool blank = true;
    while (!is_eol(lexer)) {
      if (!is_inline_space(lexer->lookahead)) {
        blank = false;
        break;
      }
      lexer->advance(lexer, !consume);
    }
    if (!blank) return false;
  }
}

static bool scan_block_string(Scanner *scanner, TSLexer *lexer, const bool *valid_symbols) {
  uint16_t indent = scanner->block_string_indent;
  bool template = valid_symbols[TPL_BLOCK_STRING_FRAGMENT];
  enum TokenType fragment = template ? TPL_BLOCK_STRING_FRAGMENT : BLOCK_STRING_FRAGMENT;
  bool has = false;
  lexer->mark_end(lexer);
  for (;;) {
    if (is_eol(lexer)) {
      if (has) lexer->mark_end(lexer);
      if (!block_string_continues(lexer, indent, has)) {
        if (has) {
          lexer->result_symbol = fragment;
          return true;
        }
        if (valid_symbols[BLOCK_STRING_END]) {
          scanner->block_string_indent = 0;
          lexer->result_symbol = BLOCK_STRING_END;
          return true;
        }
        return false;
      }
      continue;
    }
    int32_t c = lexer->lookahead;
    if (c == '$' && template) {
      advance(lexer);
      if (lexer->lookahead == '{') {
        /* `${` starts an interpolation: stop before the `$`. */
        if (has) {
          lexer->result_symbol = fragment;
          return true;
        }
        return false;
      }
      if (lexer->lookahead == '$') advance(lexer); /* `$$` */
      has = true;
      lexer->mark_end(lexer);
      continue;
    }
    advance(lexer);
    has = true;
    lexer->mark_end(lexer);
  }
}

/* ------------------------------------------------------------------ */
/* Line structure                                                      */
/* ------------------------------------------------------------------ */

/*
 * May the block on top of the stack end at a line indented `content`? The
 * line must return to an enclosing block's indentation; a block pushed by
 * _indent_dash (`- key:` + block) may also return to the fields of the
 * dash-line record, two spaces past the dash.
 */
static bool valid_dedent_target(Scanner *scanner, uint32_t content) {
  uint32_t below = scanner->indents.size > 1 ? level(*array_get(&scanner->indents, scanner->indents.size - 2)) : 0;
  if (content <= below) return true;
  return (*array_back(&scanner->indents) & DASH_LEVEL) && content == below + 2;
}

static bool emit(Scanner *scanner, TSLexer *lexer, enum TokenType symbol, uint8_t error) {
  scanner->error = error;
  lexer->result_symbol = symbol;
  return true;
}

static bool emit_indent(Scanner *scanner, TSLexer *lexer, enum TokenType symbol, uint32_t indent, uint8_t error) {
  if (indent >= MAX_INDENT) return false;
  array_push(&scanner->indents, (uint16_t)(indent | (symbol == INDENT_DASH ? DASH_LEVEL : 0)));
  return emit(scanner, lexer, symbol, error);
}

static inline bool table_active(Scanner *scanner) {
  return scanner->table_rows != NO_TABLE && scanner->table_indent != 0;
}

/*
 * The next content line is a row of the active table (the lexer is at its
 * first character). Is it a row the table can take, with one cell per
 * column? Otherwise the token before it carries ERR_PENDING_ROW, and the
 * row is reported. A table that ends early is reported at its last row:
 * after counting the cells, look at the line that follows the row.
 */
static uint8_t check_row(Scanner *scanner, TSLexer *lexer, bool bad) {
  if (bad || scanner->table_rows <= 0) return ERR_PENDING_ROW;
  if (count_cells(lexer) != scanner->table_cols) return ERR_PENDING_ROW;
  if (scanner->table_rows > 1) {
    LinesAhead next;
    scan_lines_ahead(lexer, &next);
    if (next.eof || next.content < scanner->table_indent) {
      scanner->table_rows = 1; /* report the missing rows once, here */
      return ERR_PENDING_ROW;
    }
  }
  return ERR_NONE;
}

/* The error reported for a line with bad indentation. */
static inline uint8_t line_error(Scanner *scanner, uint32_t content) {
  return table_active(scanner) && content >= scanner->table_indent ? ERR_PENDING_ROW : ERR_PENDING_LINE;
}

/*
 * At an end of line: the structural token for the next content line.
 * `no_rows` is set when the previous token ended a table that declares
 * rows but has none.
 */
static bool scan_line_end(Scanner *scanner, TSLexer *lexer, const bool *valid_symbols, bool no_rows) {
  bool begin = valid_symbols[BLOCK_STRING_BEGIN];
  bool begin_dash = valid_symbols[BLOCK_STRING_BEGIN_DASH];
  bool want_line = valid_symbols[NEWLINE] || valid_symbols[INDENT] || valid_symbols[INDENT_DASH] ||
                   valid_symbols[DEDENT] || valid_symbols[KEY_START] || valid_symbols[TABLE_START] ||
                   valid_symbols[ROW_START] || valid_symbols[TABLE_END] || valid_symbols[DOCUMENT_SEPARATOR];
  if (!begin && !begin_dash && !want_line) return false;

  LinesAhead info;
  scan_lines_ahead(lexer, &info);
  uint32_t top = top_indent(scanner);
  bool eof = info.eof;
  uint32_t content = eof ? 0 : info.content;

  if (begin || begin_dash) {
    uint32_t required = top + (begin ? 2 : 4);
    if (info.first_nonblank >= 0 && (uint32_t)info.first_nonblank >= required && required < MAX_INDENT) {
      scanner->block_string_indent = (uint16_t)required;
      lexer->result_symbol = begin ? BLOCK_STRING_BEGIN : BLOCK_STRING_BEGIN_DASH;
      return true;
    }
  }

  /* A tab in the indentation of a content line is an error (§4.1). */
  bool tab = info.tab && !eof;

  /* Between a table header and its first row. */
  if (scanner->table_rows != NO_TABLE && scanner->table_indent == 0) {
    enum TokenType rows_token = valid_symbols[INDENT_DASH] ? INDENT_DASH : INDENT;
    uint32_t row_indent = top + (rows_token == INDENT_DASH ? 4 : 2);
    uint32_t shallowest = rows_token == INDENT_DASH ? top + 3 : top + 1;
    if (!eof && valid_symbols[rows_token] && content >= shallowest && content < MAX_INDENT) {
      scanner->table_indent = (uint16_t)content;
      uint8_t error = check_row(scanner, lexer, tab || content != row_indent);
      return emit_indent(scanner, lexer, rows_token, content, error);
    }
    no_rows = scanner->table_rows > 0;
    clear_table(scanner);
    if (valid_symbols[TABLE_END]) {
      /* A table on a dash line ends with its own token. */
      return emit(scanner, lexer, TABLE_END, no_rows ? ERR_NO_ROWS_NEXT : ERR_NONE);
    }
  }
  if (no_rows) {
    /* The table has no rows: report it on the next line. */
    if (!eof && valid_symbols[INDENT] && content == top + 2) {
      return emit_indent(scanner, lexer, INDENT, content, ERR_PENDING_NO_ROWS);
    }
    if ((!eof && content > top) || !valid_symbols[NEWLINE]) return false;
    return emit(scanner, lexer, NEWLINE, ERR_PENDING_NO_ROWS);
  }

  if (!eof && content > top) {
    uint8_t error = tab ? ERR_PENDING_LINE : ERR_NONE;
    if (valid_symbols[INDENT] && content == top + 2) return emit_indent(scanner, lexer, INDENT, content, error);
    if (valid_symbols[INDENT_DASH] && content == top + 4) {
      return emit_indent(scanner, lexer, INDENT_DASH, content, error);
    }
    /*
     * Deeper than the current block, but not by the required amount.
     * Open the block (or end the line) anyway, then report the
     * indentation with an error token, so that the line still parses.
     */
    if (valid_symbols[INDENT] || valid_symbols[INDENT_DASH]) {
      enum TokenType symbol =
        valid_symbols[INDENT] && (!valid_symbols[INDENT_DASH] || content < top + 4) ? INDENT : INDENT_DASH;
      return emit_indent(scanner, lexer, symbol, content, ERR_PENDING_LINE);
    }
    if (valid_symbols[NEWLINE]) return emit(scanner, lexer, NEWLINE, line_error(scanner, content));
    /* A line starts here (e.g. after blank lines at the start of the
       input), but deeper than the block: an error token at the line, then
       the line's own tokens. */
    if (valid_symbols[KEY_START] || valid_symbols[TABLE_START] || valid_symbols[DOCUMENT_SEPARATOR]) {
      if (info.first_comment >= 0) return false;
      lexer->mark_end(lexer);
      return emit(scanner, lexer, INDENTATION_ERROR, ERR_RECOVER_LINE);
    }
    return false;
  }

  if (content < top) {
    if (valid_symbols[DEDENT] && (info.first_comment < 0 || (uint32_t)info.first_comment < top)) {
      uint8_t error = ERR_NONE;
      if (!valid_dedent_target(scanner, content)) error = ERR_PENDING_LINE;
      if (scanner->table_rows != NO_TABLE && scanner->table_indent >= top) {
        if (scanner->table_rows > 0) error = ERR_PENDING_LINE; /* rows missing */
        clear_table(scanner);
      }
      (void)array_pop(&scanner->indents);
      /* A tab error belongs to the last dedent before the line. */
      if (tab && top_indent(scanner) <= content) error = ERR_PENDING_LINE;
      return emit(scanner, lexer, DEDENT, error);
    }
    if (valid_symbols[NEWLINE]) return emit(scanner, lexer, NEWLINE, ERR_NONE);
    if (valid_symbols[DEDENT] || !(valid_symbols[KEY_START] || valid_symbols[ROW_START] ||
                                   valid_symbols[TABLE_START] || valid_symbols[DOCUMENT_SEPARATOR])) {
      return false;
    }
    /*
     * A line starts here, but the parser has no block to close: after
     * error recovery the indentation stack can hold levels of blocks that
     * ended up inside an ERROR. Drop them, so that one bad line does not
     * turn the rest of the document into errors.
     */
    while (scanner->indents.size > 1 && top_indent(scanner) > content) (void)array_pop(&scanner->indents);
    if (scanner->table_rows != NO_TABLE && scanner->table_indent > content) clear_table(scanner);
    top = top_indent(scanner);
    if (content != top) return false;
  }

  if (valid_symbols[NEWLINE]) {
    uint8_t error = tab ? line_error(scanner, content) : ERR_NONE;
    if (!eof && table_active(scanner) && content == scanner->table_indent && error == ERR_NONE) {
      error = check_row(scanner, lexer, false);
    }
    return emit(scanner, lexer, NEWLINE, error);
  }

  /* Comment lines come first: let the internal lexer take them. */
  if (info.first_comment >= 0 || eof) return false;

  if (content == 0 && lexer->lookahead == '-') {
    if (valid_symbols[DOCUMENT_SEPARATOR] && scan_separator(lexer)) {
      return emit(scanner, lexer, DOCUMENT_SEPARATOR, ERR_NONE);
    }
    return false;
  }

  if (valid_symbols[ROW_START] && table_active(scanner)) {
    if (scanner->table_rows > 0) scanner->table_rows--;
    return emit(scanner, lexer, ROW_START, ERR_NONE);
  }

  if (valid_symbols[KEY_START] || valid_symbols[TABLE_START]) {
    return scan_content(scanner, lexer, valid_symbols);
  }
  return false;
}

/*
 * After a token that carries ERR_PENDING_*: an error token over the line
 * break and the indentation of the line that caused it. No rule accepts
 * it, so the parser reports an ERROR there, and recovers on that line.
 */
static bool scan_error_token(Scanner *scanner, TSLexer *lexer, uint8_t pending) {
  for (;;) {
    while (lexer->lookahead == ' ' || lexer->lookahead == '\t' || lexer->lookahead == '\f') advance(lexer);
    if (lexer->eof(lexer)) break;
    if (lexer->lookahead == '\r' || lexer->lookahead == '\n') {
      advance(lexer);
      continue;
    }
    /* Comment lines come first: they keep the pending error. */
    if (lexer->lookahead == '#') return false;
    break;
  }
  lexer->mark_end(lexer);
  if (pending == ERR_PENDING_ROW) return emit(scanner, lexer, TABLE_ROW_ERROR, ERR_RECOVER_ROW);
  if (pending == ERR_PENDING_NO_ROWS) return emit(scanner, lexer, TABLE_ROW_ERROR, ERR_RECOVER_LINE);
  return emit(scanner, lexer, INDENTATION_ERROR, ERR_RECOVER_LINE);
}

/*
 * During error recovery (all symbols valid). Right after an error token:
 * produce the token that the line starts with (_key_start or _row_start),
 * so that the parser resumes there. Zero-width tokens are only accepted
 * during error recovery if they change the scanner state; clearing `error`
 * does. Otherwise only a document separator is produced.
 */
static bool scan_recovery(Scanner *scanner, TSLexer *lexer, uint8_t error) {
  lexer->mark_end(lexer);
  if (error < ERR_RECOVER_LINE) {
    /*
     * A `---` line ends the document, whatever error it has (§4.11: an
     * error in one document does not prevent reading the others).
     */
    while (is_inline_space(lexer->lookahead)) skip(lexer);
    if (!is_eol(lexer)) return false;
    LinesAhead info;
    scan_lines_ahead(lexer, &info);
    if (info.eof || info.content != 0 || lexer->lookahead != '-' || !scan_separator(lexer)) return false;
    array_clear(&scanner->indents);
    array_push(&scanner->indents, 0);
    scanner->block_string_indent = 0;
    clear_table(scanner);
    return emit(scanner, lexer, DOCUMENT_SEPARATOR, ERR_NONE);
  }
  if (error == ERR_RECOVER_ROW) {
    if (scanner->table_rows > 0) scanner->table_rows--;
    return emit(scanner, lexer, ROW_START, ERR_NONE);
  }
  if (error == ERR_RECOVER_LINE) {
    bool valid[ERROR_SENTINEL + 1] = {false};
    valid[KEY_START] = true;
    while (is_inline_space(lexer->lookahead)) skip(lexer);
    return scan_content(scanner, lexer, valid);
  }
  return false;
}

/*
 * At the start of the input (the only line start the scanner sees without
 * coming from an end of line): a `---` separator, or a first line that is
 * indented, which is an error (§4.2: the body starts at column 1).
 */
static bool scan_input_start(Scanner *scanner, TSLexer *lexer, const bool *valid_symbols, bool *handled) {
  *handled = false;
  if (scanner->indents.size != 1 || !valid_symbols[DOCUMENT_SEPARATOR]) return false;
  int32_t c = lexer->lookahead;
  if (c != ' ' && c != '\t' && c != '-' && c != 0xFEFF) return false;
  if (lexer->get_column(lexer) != 0) return false;
  if (lexer->lookahead == 0xFEFF) skip(lexer);
  bool indented = false;
  while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
    indented = true;
    advance(lexer);
  }
  if (is_eol(lexer)) return false; /* blank first line: handled as an end of line */
  *handled = true;
  if (lexer->lookahead == '#') return false;
  if (indented) {
    lexer->mark_end(lexer);
    return emit(scanner, lexer, INDENTATION_ERROR, ERR_RECOVER_LINE);
  }
  if (lexer->lookahead == '-') {
    if (scan_separator(lexer)) return emit(scanner, lexer, DOCUMENT_SEPARATOR, ERR_NONE);
    return false;
  }
  return scan_content(scanner, lexer, valid_symbols);
}

static bool scan(Scanner *scanner, TSLexer *lexer, const bool *valid_symbols) {
  uint8_t error = scanner->error;
  scanner->error = ERR_NONE;

  if (valid_symbols[ERROR_SENTINEL]) return scan_recovery(scanner, lexer, error);

  /* Recovered to a state before the error token: resume at the row. (A
     key is found by the normal path below.) */
  if (error == ERR_RECOVER_ROW && valid_symbols[ROW_START]) {
    lexer->mark_end(lexer);
    if (scanner->table_rows > 0) scanner->table_rows--;
    return emit(scanner, lexer, ROW_START, ERR_NONE);
  }

  if (scanner->block_string_indent > 0 &&
      (valid_symbols[BLOCK_STRING_FRAGMENT] || valid_symbols[TPL_BLOCK_STRING_FRAGMENT] ||
       valid_symbols[BLOCK_STRING_END])) {
    return scan_block_string(scanner, lexer, valid_symbols);
  }

  lexer->mark_end(lexer);

  if (error == ERR_PENDING_LINE || error == ERR_PENDING_ROW || error == ERR_PENDING_NO_ROWS) {
    while (is_inline_space(lexer->lookahead)) skip(lexer);
    if (is_eol(lexer)) return scan_error_token(scanner, lexer, error);
  }

  bool handled;
  bool found = scan_input_start(scanner, lexer, valid_symbols, &handled);
  if (handled) return found;

  while (is_inline_space(lexer->lookahead) || lexer->lookahead == '\f' || lexer->lookahead == 0xFEFF) {
    skip(lexer);
  }

  if (!is_eol(lexer)) {
    if (lexer->lookahead == '#') return false;
    return scan_content(scanner, lexer, valid_symbols);
  }

  return scan_line_end(scanner, lexer, valid_symbols, error == ERR_NO_ROWS_NEXT);
}

/* ------------------------------------------------------------------ */
/* tree-sitter API                                                     */
/* ------------------------------------------------------------------ */

void *tree_sitter_nova_external_scanner_create(void) {
  Scanner *scanner = ts_calloc(1, sizeof(Scanner));
  array_init(&scanner->indents);
  array_push(&scanner->indents, 0);
  clear_table(scanner);
  return scanner;
}

void tree_sitter_nova_external_scanner_destroy(void *payload) {
  Scanner *scanner = (Scanner *)payload;
  array_delete(&scanner->indents);
  ts_free(scanner);
}

static inline void put16(char *buffer, unsigned *size, uint16_t v) {
  buffer[(*size)++] = (char)(v & 0xFF);
  buffer[(*size)++] = (char)(v >> 8);
}

static inline uint16_t get16(const char *buffer, unsigned i) {
  return (uint16_t)((uint8_t)buffer[i] | ((uint8_t)buffer[i + 1] << 8));
}

/* Layout: block_string_indent, table_rows (2 × 16 bits), table_cols,
   table_indent, error, then the indentation stack without its bottom 0. */
#define HEADER_SIZE 12

unsigned tree_sitter_nova_external_scanner_serialize(void *payload, char *buffer) {
  Scanner *scanner = (Scanner *)payload;
  unsigned size = 0;
  uint32_t rows = (uint32_t)scanner->table_rows;
  put16(buffer, &size, scanner->block_string_indent);
  put16(buffer, &size, (uint16_t)(rows & 0xFFFF));
  put16(buffer, &size, (uint16_t)(rows >> 16));
  put16(buffer, &size, scanner->table_cols);
  put16(buffer, &size, scanner->table_indent);
  put16(buffer, &size, scanner->error);
  for (uint32_t i = 1; i < scanner->indents.size && size + 2 <= TREE_SITTER_SERIALIZATION_BUFFER_SIZE; i++) {
    put16(buffer, &size, *array_get(&scanner->indents, i));
  }
  return size;
}

void tree_sitter_nova_external_scanner_deserialize(void *payload, const char *buffer, unsigned length) {
  Scanner *scanner = (Scanner *)payload;
  array_clear(&scanner->indents);
  array_push(&scanner->indents, 0);
  scanner->block_string_indent = 0;
  clear_table(scanner);
  scanner->error = ERR_NONE;
  if (length < HEADER_SIZE) return;
  scanner->block_string_indent = get16(buffer, 0);
  scanner->table_rows = (int32_t)((uint32_t)get16(buffer, 2) | ((uint32_t)get16(buffer, 4) << 16));
  scanner->table_cols = get16(buffer, 6);
  scanner->table_indent = get16(buffer, 8);
  scanner->error = (uint8_t)get16(buffer, 10);
  for (unsigned i = HEADER_SIZE; i + 1 < length; i += 2) {
    array_push(&scanner->indents, get16(buffer, i));
  }
}

bool tree_sitter_nova_external_scanner_scan(void *payload, TSLexer *lexer, const bool *valid_symbols) {
  return scan((Scanner *)payload, lexer, valid_symbols);
}
