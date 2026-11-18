/*
 * External scanner for tree-sitter-nova-schema (the `.novs` schema
 * language, NOVA spec §6.1).
 *
 * Schema files are line-oriented with exactly two levels: definitions and
 * directives at column 1, and record fields or block-form enum variants
 * indented exactly two spaces. The scanner produces:
 *
 *   _newline,           end of a top-level line, and of a field or variant
 *   _member_newline     line (zero-width; the line break itself is skipped
 *                       as an extra). Blank and comment-only lines never
 *                       produce tokens.
 *   _indent, _dedent    zero-width; open and close the body of a `record`
 *                       or `enum` definition.
 *   indentation_error   never accepted by a rule: emitted over the line
 *                       break and indentation of a line that is indented
 *                       wrongly (or with a tab), followed by the structural
 *                       token it replaced, so that the parser reports an
 *                       ERROR there and resumes on that line.
 *
 * The current level does not need to be stored: the parser asks for the
 * token of the line it is in. The only state is the token to resume with
 * after an error token.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tree_sitter/alloc.h"
#include "tree_sitter/parser.h"

#include <stdbool.h>
#include <stdint.h>

enum TokenType {
  NEWLINE,
  MEMBER_NEWLINE,
  INDENT,
  DEDENT,
  INDENTATION_ERROR,
  ERROR_SENTINEL,
};

#define BODY_INDENT 2

typedef struct {
  /*
   * After an indentation_error: 1 + the structural token it replaced,
   * which is produced next (so that the parser resumes on that line);
   * 0 if none.
   */
  uint8_t resume;
} Scanner;

static inline void advance(TSLexer *lexer) { lexer->advance(lexer, false); }

static inline void skip(TSLexer *lexer) { lexer->advance(lexer, true); }

static inline bool is_eol(TSLexer *lexer) {
  return lexer->eof(lexer) || lexer->lookahead == '\n' || lexer->lookahead == '\r';
}

typedef struct {
  /* Indentation of the first comment-only line, -1 if none. */
  int32_t first_comment;
  /* Indentation of the next content line. */
  uint32_t content;
  /* The content line's indentation contains a tab. */
  bool tab;
  bool eof;
} LinesAhead;

/*
 * From an end of line, look past blank and comment-only lines and measure
 * the next content line. The text is advanced over, not skipped: tokens
 * that end here are zero-width (their end is marked before), and the
 * error token ends at the content line.
 */
static void scan_lines_ahead(TSLexer *lexer, LinesAhead *info) {
  info->first_comment = -1;
  info->content = 0;
  info->tab = false;
  info->eof = false;
  for (;;) {
    if (lexer->eof(lexer)) {
      info->eof = true;
      return;
    }
    if (lexer->lookahead == '\r') advance(lexer);
    if (lexer->lookahead == '\n') advance(lexer);
    uint32_t indent = 0;
    bool tab = false;
    while (lexer->lookahead == ' ' || lexer->lookahead == '\t' || lexer->lookahead == '\f') {
      if (lexer->lookahead != ' ') tab = true;
      indent++;
      advance(lexer);
    }
    if (lexer->eof(lexer)) {
      info->eof = true;
      return;
    }
    if (lexer->lookahead == '\n' || lexer->lookahead == '\r') continue;
    if (lexer->lookahead == '#') {
      if (info->first_comment < 0) info->first_comment = (int32_t)indent;
      while (!is_eol(lexer)) advance(lexer);
      continue;
    }
    info->content = indent;
    info->tab = tab;
    return;
  }
}

static bool emit(Scanner *scanner, TSLexer *lexer, enum TokenType symbol, uint8_t resume) {
  scanner->resume = resume;
  lexer->result_symbol = symbol;
  return true;
}

static bool scan(Scanner *scanner, TSLexer *lexer, const bool *valid_symbols) {
  uint8_t resume = scanner->resume;
  scanner->resume = 0;

  /*
   * Right after an indentation_error, during error recovery (all symbols
   * valid) or after the parser recovered to the state before it: the
   * structural token the error token replaced. It is zero-width, so it is
   * only accepted during error recovery because it changes the state.
   */
  if (resume > 0 && (valid_symbols[ERROR_SENTINEL] || valid_symbols[resume - 1])) {
    lexer->mark_end(lexer);
    return emit(scanner, lexer, (enum TokenType)(resume - 1), 0);
  }
  if (valid_symbols[ERROR_SENTINEL]) return false;
  if (!valid_symbols[NEWLINE] && !valid_symbols[MEMBER_NEWLINE] && !valid_symbols[INDENT] &&
      !valid_symbols[DEDENT]) {
    return false;
  }

  lexer->mark_end(lexer);
  while (lexer->lookahead == ' ' || lexer->lookahead == '\t' || lexer->lookahead == '\f' ||
         lexer->lookahead == 0xFEFF) {
    skip(lexer);
  }
  if (!is_eol(lexer)) return false;

  LinesAhead info;
  scan_lines_ahead(lexer, &info);
  bool eof = info.eof;
  uint32_t content = eof ? 0 : info.content;
  bool tab = info.tab && !eof;

  enum TokenType symbol;
  bool bad;
  if (valid_symbols[INDENT] && !eof && content > 0) {
    /* After `record Name:` or `enum Name:`: the body, two spaces deeper. */
    symbol = INDENT;
    bad = tab || content != BODY_INDENT;
  } else if (valid_symbols[NEWLINE]) {
    /* The end of a top-level line: the next line starts at column 1. */
    symbol = NEWLINE;
    bad = !eof && (tab || content != 0);
  } else if (valid_symbols[MEMBER_NEWLINE]) {
    /* The end of a field or variant: the next line is another one, or a
       line at column 1 (after which _dedent closes the body). */
    symbol = MEMBER_NEWLINE;
    bad = !eof && (tab || content > BODY_INDENT);
  } else if (valid_symbols[DEDENT] && content < BODY_INDENT) {
    /* Comment lines indented like the body stay in the body. */
    if (info.first_comment >= BODY_INDENT) return false;
    symbol = DEDENT;
    bad = !eof && (tab || content != 0);
  } else {
    return false;
  }
  if (!bad) return emit(scanner, lexer, symbol, 0);

  /* Comment lines come first; the line after them is checked again. */
  if (info.first_comment >= 0) return false;

  /*
   * The line is indented wrongly: an error token over its line break and
   * indentation, which no rule accepts, then the structural token.
   */
  lexer->mark_end(lexer);
  return emit(scanner, lexer, INDENTATION_ERROR, (uint8_t)(symbol + 1));
}

void *tree_sitter_nova_schema_external_scanner_create(void) {
  Scanner *scanner = ts_calloc(1, sizeof(Scanner));
  return scanner;
}

void tree_sitter_nova_schema_external_scanner_destroy(void *payload) { ts_free(payload); }

unsigned tree_sitter_nova_schema_external_scanner_serialize(void *payload, char *buffer) {
  Scanner *scanner = (Scanner *)payload;
  if (scanner->resume == 0) return 0;
  buffer[0] = (char)scanner->resume;
  return 1;
}

void tree_sitter_nova_schema_external_scanner_deserialize(void *payload, const char *buffer, unsigned length) {
  Scanner *scanner = (Scanner *)payload;
  scanner->resume = length > 0 ? (uint8_t)buffer[0] : 0;
}

bool tree_sitter_nova_schema_external_scanner_scan(void *payload, TSLexer *lexer, const bool *valid_symbols) {
  return scan((Scanner *)payload, lexer, valid_symbols);
}
