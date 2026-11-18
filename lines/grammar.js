/**
 * @file NOVA Lines (`.novl`) grammar for tree-sitter
 * @author NOVA contributors
 * @license Apache-2.0
 *
 * NOVA Lines (spec §4.13, proposal 0002 §4.2): a header of directive lines,
 * then one record per line. Blank lines and comment lines are ignored.
 *
 * Which mode a record line is in (value, table or positional) depends on
 * the directives in effect, which a parser does not track. So every record
 * line is a `row` of comma-separated, possibly empty cells: a value-mode or
 * positional line is a row of one cell. The cells are flow values, shared
 * with the `nova` grammar (`common/flow.js`).
 *
 * Not checked here (validators do this): the cell count per row, unique
 * column names, and that directives are in the header.
 */

/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

import { ARGUMENT, flowRules, lexicalRules, mode } from '../common/flow.js';

const DATA = mode('data');

export default grammar({
  name: 'nova_lines',

  // Newlines end records, so they are not extras. A record never spans
  // lines (§4.13), so no external scanner is needed.
  extras: $ => [/[ \t\f﻿]/, $.comment],

  word: $ => $.identifier,

  rules: {
    stream: $ => seq(
      optional($._line),
      repeat(seq($._newline, optional($._line))),
    ),

    _newline: _ => /\r?\n/,

    argument: _ => token(ARGUMENT),

    _line: $ => choice($.directive, $.row),

    // ------------------------------------------------------------------
    // Directives
    // ------------------------------------------------------------------

    // shortcut: a directive name is recognized after leading whitespace
    // and before any character, while the spec requires column 1 and a
    // following space or end of line: an indented `@nova 0.2` parses as a
    // directive, and a record like `@nova("x")` is an ERROR. Exact only
    // with an external scanner; upgrade if such records occur.
    directive: $ => choice(
      seq(
        field('name', alias('@columns', $.directive_name)),
        field('columns', $.columns),
      ),
      seq(
        field('name', alias(choice('@nova', '@schema', '@key', '@mode', '@end', $._extension_name), $.directive_name)),
        optional(field('argument', $.argument)),
      ),
    ),

    // `@x-…` directives are for extensions (§4.13).
    _extension_name: _ => token(prec(1, /@x-[A-Za-z0-9_-]*/)),

    // {name, name{…}, …, ..}: at least one entry, `..` only as the last.
    columns: $ => seq(
      '{',
      choice(
        seq(
          $._named_column,
          repeat(seq(',', $._named_column)),
          optional(seq(',', $.rest_column)),
        ),
        $.rest_column,
      ),
      '}',
    ),

    _named_column: $ => choice($.column, $.nested_column),

    column: $ => field('name', $._column_name),

    nested_column: $ => seq(
      field('name', $._column_name),
      field('columns', $.columns),
    ),

    rest_column: _ => '..',

    _column_name: $ => choice(alias($.identifier, $.key), $.string),

    // ------------------------------------------------------------------
    // Records
    // ------------------------------------------------------------------

    // One flow value, or cells separated by commas, where a cell may be
    // empty (`80, , false`) and a trailing comma means an empty last cell.
    row: $ => choice(
      field('cell', $._flow_value),
      seq(
        optional(field('cell', $._flow_value)),
        repeat1(seq(',', optional(field('cell', $._flow_value)))),
      ),
    ),

    ...flowRules(DATA),
    ...lexicalRules,
  },
});
