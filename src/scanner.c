/*
 * External scanner for tree-sitter-nova.
 *
 * Produces the context-sensitive tokens of NOVA text (spec §4):
 *
 *   _newline, _indent, _dedent   block structure (§4.3), Python-style. All
 *                                three are zero-width; the line breaks
 *                                themselves are skipped as extras. Blank and
 *                                comment-only lines never produce tokens.
 *   _key_start                   zero-width marker before `key:` /
 *                                `key[N]{…}:` (decides key vs. value).
 *   plain_scalar                 the rest of the line after `key: ` / `- `,
 *                                up to a comment (§4.4). Not produced when
 *                                the text is exactly a number, `true`,
 *                                `false` or `null`: the internal lexer then
 *                                produces that token instead.
 *   _block_string_begin[_dash]   zero-width; starts a block string (§4.6)
 *                                whose content is indented parent+2
 *                                (parent+4 for `- key: |`).
 *   _block_string_fragment       block-string content, split at `${`.
 *   _block_string_end            zero-width; ends a block string.
 *   _table_start                 zero-width before a top-level `[N]{` (§4.9).
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
  DEDENT,
  KEY_START,
  PLAIN_SCALAR,
  BLOCK_STRING_BEGIN,
  BLOCK_STRING_BEGIN_DASH,
  BLOCK_STRING_FRAGMENT,
  BLOCK_STRING_END,
  TABLE_START,
  ERROR_SENTINEL,
};

typedef struct {
  Array(uint16_t) indents;
  /* Content indentation of the block string being scanned, 0 if none. */
  uint16_t block_string_indent;
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

/* Characters that may not begin a plain scalar (§4.4). */
static bool is_reserved_start(int32_t c) {
  switch (c) {
    case '!': case '"': case '#': case '$': case '%': case '&': case '\'':
    case '*': case ',': case ':': case '<': case '=': case '>': case '?':
    case '@': case '[': case '\\': case ']': case '^': case '`': case '{':
    case '|': case '}': case '~': case '(': case ')':
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
    while (lexer->lookahead == ' ') {
      indent++;
      skip(lexer);
    }
    while (lexer->lookahead == '\t' || lexer->lookahead == '\f') skip(lexer);
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
    return;
  }
}

/* ------------------------------------------------------------------ */
/* Content at the current position: key marker, table start, plain     */
/* ------------------------------------------------------------------ */

enum KeyState { KS_DEAD, KS_IDENT, KS_TABLE_OPEN, KS_TABLE, KS_TABLE_CLOSE, KS_COLON };

static bool scan_quoted_key(TSLexer *lexer) {
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
  if (lexer->lookahead == '[') {
    advance(lexer);
    if (!is_digit(lexer->lookahead)) return false;
    while (!is_eol(lexer) && lexer->lookahead != '}') advance(lexer);
    if (lexer->lookahead != '}') return false;
    advance(lexer);
  }
  if (lexer->lookahead != ':') return false;
  advance(lexer);
  return is_eol(lexer) || is_inline_space(lexer->lookahead) || lexer->lookahead == '#';
}

static bool scan_content(TSLexer *lexer, const bool *valid_symbols) {
  lexer->mark_end(lexer);
  int32_t c = lexer->lookahead;

  if (c == '[') {
    if (!valid_symbols[TABLE_START]) return false;
    advance(lexer);
    if (!is_digit(lexer->lookahead)) return false;
    while (is_digit(lexer->lookahead)) advance(lexer);
    if (lexer->lookahead != ']') return false;
    advance(lexer);
    if (lexer->lookahead != '{') return false;
    lexer->result_symbol = TABLE_START;
    return true;
  }

  bool want_key = valid_symbols[KEY_START];
  bool want_plain = valid_symbols[PLAIN_SCALAR];

  if (c == '"') {
    if (want_key && scan_quoted_key(lexer)) {
      lexer->result_symbol = KEY_START;
      return true;
    }
    return false;
  }

  if (!want_key && !want_plain) return false;
  if (is_reserved_start(c)) return false;

  enum KeyState ks = (want_key && is_ident_start(c)) ? KS_IDENT : KS_DEAD;
  if (!want_plain && ks == KS_DEAD) return false;

  char buf[CLASSIFY_BUFFER];
  size_t len = 0;         /* characters stored in buf */
  size_t content_len = 0; /* length up to the last non-space character */
  bool classifiable = true;
  bool prev_space = false;
  /* A non-space character was consumed but not marked (a key was still possible). */
  bool unmarked = false;

  for (;;) {
    bool eol = is_eol(lexer);
    c = lexer->lookahead;
    bool space = !eol && is_inline_space(c);

    if (ks == KS_COLON) {
      if (eol || space || c == '#') {
        lexer->result_symbol = KEY_START;
        return true; /* zero-width: mark_end is still at the start */
      }
      ks = KS_DEAD;
    }

    if (eol) break;
    if (c == '#' && prev_space) break;

    switch (ks) {
      case KS_IDENT:
        if (c == ':') ks = KS_COLON;
        else if (c == '[') ks = KS_TABLE_OPEN;
        else if (!is_ident_char(c)) ks = KS_DEAD;
        break;
      case KS_TABLE_OPEN:
        ks = is_digit(c) ? KS_TABLE : KS_DEAD;
        break;
      case KS_TABLE:
        if (c == '}') ks = KS_TABLE_CLOSE;
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
  if (content_len == 0) return false;
  if (unmarked) lexer->mark_end(lexer);

  /* Spellings that are not plain scalars (§4.4). */
  if (len >= 3 && buf[0] == '.' && buf[1] == '.' && buf[2] == '.') return false;
  if (buf[0] == '-' && (content_len == 1 || buf[1] == ' ' || buf[1] == '\t')) return false;

  /* Exact numbers and keywords are lexed by the internal lexer. */
  if (classifiable && (is_number(buf, content_len) || is_keyword(buf, content_len))) return false;

  lexer->result_symbol = PLAIN_SCALAR;
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
  bool has = false;
  lexer->mark_end(lexer);
  for (;;) {
    if (is_eol(lexer)) {
      if (has) lexer->mark_end(lexer);
      if (!block_string_continues(lexer, indent, has)) {
        if (has) {
          lexer->result_symbol = BLOCK_STRING_FRAGMENT;
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
    if (c == '$') {
      advance(lexer);
      if (lexer->lookahead == '{') {
        /* `${` starts an interpolation: stop before the `$`. */
        if (has) {
          lexer->result_symbol = BLOCK_STRING_FRAGMENT;
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
/* Entry point                                                         */
/* ------------------------------------------------------------------ */

static bool scan(Scanner *scanner, TSLexer *lexer, const bool *valid_symbols) {
  if (valid_symbols[ERROR_SENTINEL]) return false;

  if (scanner->block_string_indent > 0 &&
      (valid_symbols[BLOCK_STRING_FRAGMENT] || valid_symbols[BLOCK_STRING_END])) {
    return scan_block_string(scanner, lexer, valid_symbols);
  }

  lexer->mark_end(lexer);
  while (is_inline_space(lexer->lookahead) || lexer->lookahead == '\f' || lexer->lookahead == 0xFEFF) {
    skip(lexer);
  }

  if (!is_eol(lexer)) {
    if (lexer->lookahead == '#') return false;
    return scan_content(lexer, valid_symbols);
  }

  bool begin = valid_symbols[BLOCK_STRING_BEGIN];
  bool begin_dash = valid_symbols[BLOCK_STRING_BEGIN_DASH];
  bool want_line = valid_symbols[NEWLINE] || valid_symbols[INDENT] || valid_symbols[DEDENT] ||
                   valid_symbols[KEY_START] || valid_symbols[TABLE_START];
  if (!begin && !begin_dash && !want_line) return false;

  LinesAhead info;
  scan_lines_ahead(lexer, &info);
  uint16_t top = *array_back(&scanner->indents);

  if (begin || begin_dash) {
    uint32_t required = (uint32_t)top + (begin ? 2 : 4);
    if (info.first_nonblank >= 0 && (uint32_t)info.first_nonblank >= required && required < 0xFFFF) {
      scanner->block_string_indent = (uint16_t)required;
      lexer->result_symbol = begin ? BLOCK_STRING_BEGIN : BLOCK_STRING_BEGIN_DASH;
      return true;
    }
  }

  if (valid_symbols[INDENT] && !info.eof && info.content > top && info.content < 0xFFFF) {
    array_push(&scanner->indents, (uint16_t)info.content);
    lexer->result_symbol = INDENT;
    return true;
  }

  if (valid_symbols[DEDENT] && info.content < top &&
      (info.first_comment < 0 || (uint32_t)info.first_comment < top)) {
    array_pop(&scanner->indents);
    lexer->result_symbol = DEDENT;
    return true;
  }

  if (valid_symbols[NEWLINE]) {
    lexer->result_symbol = NEWLINE;
    return true;
  }

  /* Comment lines come first: let the internal lexer take them. */
  if (info.first_comment >= 0 || info.eof) return false;

  if (valid_symbols[KEY_START] || valid_symbols[TABLE_START]) {
    return scan_content(lexer, valid_symbols);
  }
  return false;
}

/* ------------------------------------------------------------------ */
/* tree-sitter API                                                     */
/* ------------------------------------------------------------------ */

void *tree_sitter_nova_external_scanner_create(void) {
  Scanner *scanner = ts_calloc(1, sizeof(Scanner));
  array_init(&scanner->indents);
  array_push(&scanner->indents, 0);
  return scanner;
}

void tree_sitter_nova_external_scanner_destroy(void *payload) {
  Scanner *scanner = (Scanner *)payload;
  array_delete(&scanner->indents);
  ts_free(scanner);
}

unsigned tree_sitter_nova_external_scanner_serialize(void *payload, char *buffer) {
  Scanner *scanner = (Scanner *)payload;
  unsigned size = 0;
  buffer[size++] = (char)(scanner->block_string_indent & 0xFF);
  buffer[size++] = (char)(scanner->block_string_indent >> 8);
  for (uint32_t i = 1; i < scanner->indents.size && size + 2 <= TREE_SITTER_SERIALIZATION_BUFFER_SIZE; i++) {
    uint16_t v = *array_get(&scanner->indents, i);
    buffer[size++] = (char)(v & 0xFF);
    buffer[size++] = (char)(v >> 8);
  }
  return size;
}

void tree_sitter_nova_external_scanner_deserialize(void *payload, const char *buffer, unsigned length) {
  Scanner *scanner = (Scanner *)payload;
  array_clear(&scanner->indents);
  array_push(&scanner->indents, 0);
  scanner->block_string_indent = 0;
  if (length >= 2) {
    scanner->block_string_indent = (uint16_t)((uint8_t)buffer[0] | ((uint8_t)buffer[1] << 8));
  }
  for (unsigned i = 2; i + 1 < length; i += 2) {
    uint16_t v = (uint16_t)((uint8_t)buffer[i] | ((uint8_t)buffer[i + 1] << 8));
    array_push(&scanner->indents, v);
  }
}

bool tree_sitter_nova_external_scanner_scan(void *payload, TSLexer *lexer, const bool *valid_symbols) {
  return scan((Scanner *)payload, lexer, valid_symbols);
}
