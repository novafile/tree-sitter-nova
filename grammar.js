/**
 * @file NOVA (Notation for Objects, Values & Agents) grammar for tree-sitter
 * @author NOVA contributors
 * @license Apache-2.0
 *
 * Covers the NOVA text syntax (spec §4) including text streams (§4.11),
 * the positional profile (§4.12), patch documents (§7.4) and template
 * documents (§9). Schema files (`.novs`) have their own grammar in
 * `schema/grammar.js`.
 *
 * A document whose first directive (after an optional `@nova`) is
 * `@template` is parsed with the template rules, everything else with the
 * data rules: template constructs exist only in templates (§9.2). Both rule
 * sets produce the same node names (see `common/flow.js`).
 *
 * Indentation, block-string content, plain scalars, table row and cell
 * counts and the "is this a key?" decision are handled by the external
 * scanner in `src/scanner.c`.
 */

/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

import {
  IDENT,
  commaSep1,
  flowRules,
  lexicalRules,
  mode,
  named,
} from './common/flow.js';

const DATA = mode('data');
const TEMPLATE = mode('template');

export default grammar({
  name: 'nova',

  externals: $ => [
    $._newline,
    $._indent,
    $._indent_dash,
    $._dedent,
    $._key_start,
    $.plain_scalar,
    $._tpl_plain_scalar,
    $._block_string_begin,
    $._block_string_begin_dash,
    $._block_string_fragment,
    $._tpl_block_string_fragment,
    $._block_string_end,
    $._table_start,
    $._row_start,
    // Ends a table on a dash line that has no rows, so that the scanner
    // state after the table never depends on the token that follows it.
    $._table_end,
    $.document_separator,
    // Never accepted by a rule: the scanner emits them where a line is
    // indented wrongly or a table row does not fit its header, so that the
    // tree has an ERROR there (see src/scanner.c).
    $.indentation_error,
    $.table_row_error,
    $._error_sentinel,
  ],

  extras: $ => [/[ \t\r\n\f﻿]/, $.comment],

  conflicts: $ => [
    // A template block made only of spread lines can be a record or a list.
    [$._tpl_record_line, $._tpl_list_line],
  ],

  rules: {
    // A text stream: documents separated by `---` lines (§4.11). A file
    // without separators is a stream of one document.
    stream: $ => seq(
      optional($._document),
      repeat(seq($.document_separator, optional($._document))),
    ),

    _document: $ => choice(
      $.document,
      alias($._template_document, $.document),
    ),

    // ------------------------------------------------------------------
    // Data documents (§4.2) and patch documents (§7.4)
    // ------------------------------------------------------------------

    document: $ => choice(
      seq(repeat1($._data_directive), optional(choice($._body, $._patch_section))),
      $._body,
      $._patch_section,
    ),

    _data_directive: $ => choice(
      $.directive,
      alias($._nova_directive, $.directive),
    ),

    directive: $ => seq(
      field('name', alias($._at_name, $.directive_name)),
      optional(field('argument', $.argument)),
      $._newline,
    ),

    _nova_directive: $ => seq(
      field('name', alias('@nova', $.directive_name)),
      optional(field('argument', $.argument)),
      $._newline,
    ),

    // Rest of the line, up to a comment (`#` preceded by whitespace).
    argument: _ => token(/[^ \t\r\n#(]([^ \t\r\n]|[ \t]+[^ \t\r\n#])*/),

    ...blockRules(DATA),
    ...flowRules(DATA),

    // ------------------------------------------------------------------
    // Template documents (§9)
    // ------------------------------------------------------------------

    _template_document: $ => seq(
      optional(alias($._nova_directive, $.directive)),
      alias($._template_directive, $.directive),
      repeat($._template_header_directive),
      optional($._tpl_body),
    ),

    _template_directive: $ => seq(
      field('name', alias('@template', $.directive_name)),
      optional(field('argument', $.argument)),
      $._newline,
    ),

    _template_header_directive: $ => choice(
      $.directive,
      alias($._param_directive, $.directive),
      alias($._import_directive, $.directive),
      alias($._let_directive, $.directive),
    ),

    // @param name[?]: type [= default]
    _param_directive: $ => seq(
      field('name', alias('@param', $.directive_name)),
      field('parameter', $.identifier),
      optional(field('optional', alias(token.immediate('?'), $.optional_marker))),
      token.immediate(':'),
      field('type', $.param_type),
      optional(seq('=', field('default', $._tpl_flow_value))),
      $._newline,
    ),

    // A type in schema syntax (§6.1): up to ` =` or a comment.
    param_type: _ => token(/[^ \t\r\n#=]([^ \t\r\n]|[ \t]+[^ \t\r\n#=])*/),

    // @import "path" as name
    _import_directive: $ => seq(
      field('name', alias('@import', $.directive_name)),
      field('source', $.string),
      'as',
      field('alias', $.identifier),
      $._newline,
    ),

    // @let name = flow-value   |   @let name: block-value
    _let_directive: $ => seq(
      field('name', alias('@let', $.directive_name)),
      field('binding', $.identifier),
      choice(
        seq('=', field('value', $._tpl_flow_value), $._newline),
        seq(':', blockValue($, TEMPLATE)),
      ),
    ),

    ...blockRules(TEMPLATE),
    ...flowRules(TEMPLATE),

    // $name.path[0]
    template_ref: $ => seq(
      '$',
      field('path', alias($._ref_path, $.path)),
    ),

    _ref_path: $ => seq(
      alias($._immediate_identifier, $.field),
      repeat($._path_segment),
    ),

    // `${path}` inside a quoted or block string.
    interpolation: $ => seq(
      choice(token.immediate(prec(2, '${')), '${'),
      field('path', $.path),
      '}',
    ),

    _tpl_spread_line: $ => seq('...', field('value', $._tpl_flow_value), $._newline),

    _tpl_dash_spread: $ => seq('...', field('value', $._tpl_flow_value)),

    _tpl_flow_spread: $ => seq('...', field('value', $._tpl_flow_value)),

    _tpl_record_if: $ => ifBlock($, 'record'),
    _tpl_list_if: $ => ifBlock($, 'list'),
    _tpl_record_else: $ => elseBlock($, 'record'),
    _tpl_list_else: $ => elseBlock($, 'list'),
    _tpl_record_for: $ => forBlock($, 'record'),
    _tpl_list_for: $ => forBlock($, 'list'),

    condition: $ => $._condition,

    _condition: $ => choice(
      $.binary_condition,
      $.not_condition,
      $.parenthesized_condition,
      $.comparison,
      $._tpl_flow_value,
    ),

    binary_condition: $ => choice(
      prec.left(1, seq(field('left', $._condition), field('operator', 'or'), field('right', $._condition))),
      prec.left(2, seq(field('left', $._condition), field('operator', 'and'), field('right', $._condition))),
    ),

    not_condition: $ => prec(3, seq('not', field('operand', $._condition))),

    comparison: $ => prec(4, seq(
      field('left', $._tpl_flow_value),
      field('operator', choice('==', '!=')),
      field('right', $._tpl_flow_value),
    )),

    parenthesized_condition: $ => seq('(', $._condition, ')'),

    // ------------------------------------------------------------------
    // Tables (§4.9) — shared header
    // ------------------------------------------------------------------

    table_header: $ => seq(
      '[',
      field('count', alias(token.immediate(/[0-9]+/), $.row_count)),
      token.immediate(']'),
      token.immediate('{'),
      commaSep1(field('column', choice(alias($.identifier, $.column), $.string))),
      '}',
    ),

    // ------------------------------------------------------------------
    // Paths (§7.1) and patch documents (§7.4)
    // ------------------------------------------------------------------

    path: $ => seq(
      choice(
        alias($.identifier, $.field),
        $.string,
        $.selector,
      ),
      repeat($._path_segment),
    ),

    _path_segment: $ => choice(
      seq(token.immediate('.'), choice(
        alias($._immediate_identifier, $.field),
        alias($._immediate_string, $.string),
        $.projection,
      )),
      alias($._immediate_selector, $.selector),
    ),

    selector: $ => seq('[', $._selector_body, ']'),

    _immediate_selector: $ => seq(token.immediate('['), $._selector_body, ']'),

    _selector_body: $ => choice(
      alias('*', $.wildcard),
      seq(
        field('field', alias($._selector_name, $.field)),
        optional(seq('=', field('value', $._flow_value))),
      ),
      field('value', $._flow_value),
      alias($._filter, $.filter),
    ),

    // A bare name in a selector: `[id=api]`, `[name]`.
    _selector_name: _ => token(prec(1, IDENT)),

    // `[?p op v]`, `[?p]`, `[?!p]` (queries only) — kept opaque.
    _filter: _ => token(seq('?', /[^\]\n]*/)),

    projection: $ => seq(
      token.immediate('{'),
      commaSep1(alias($.identifier, $.field)),
      '}',
    ),

    _immediate_string: $ => seq(
      token.immediate('"'),
      repeat(choice($.string_content, $.escape_sequence)),
      token.immediate('"'),
    ),

    _patch_section: $ => seq(
      alias($._patch_directive, $.directive),
      repeat($._data_directive),
      repeat($.patch_op),
    ),

    _patch_directive: $ => seq(
      field('name', alias('@patch', $.directive_name)),
      optional(field('argument', $.argument)),
      $._newline,
    ),

    patch_op: $ => choice(
      // path: old -> new
      seq(
        field('path', $.path),
        ':',
        field('old', $._flow_value),
        '->',
        field('new', $._flow_value),
        $._newline,
      ),
      // = path: value   + path: value
      seq(
        field('operator', choice('=', '+')),
        field('path', $.path),
        ':',
        field('value', $._flow_value),
        $._newline,
      ),
      // - path[: old]   ? path[: value]
      seq(
        field('operator', choice('-', '?')),
        field('path', $.path),
        optional(seq(':', field('value', $._flow_value))),
        $._newline,
      ),
    ),

    // ------------------------------------------------------------------
    // Lexical
    // ------------------------------------------------------------------

    ...lexicalRules,
  },
});

/**
 * Block structure (§4.3) in one mode.
 * @param {import('./common/flow.js').Mode} m
 */
function blockRules(m) {
  const r = m.ref;
  /** @param {GrammarSymbols<string>} $ @param {string} n */
  const hidden = ($, n) => $[m.name(n)];
  return named(m, {
    // The body (§4.2).
    _body: $ => choice(
      r($, 'record'),
      r($, 'list'),
      seq($._table_start, r($, 'table')),
      seq(r($, '_flow_value'), $._newline),
    ),

    _block: $ => choice(r($, 'record'), r($, 'list')),

    record: $ => repeat1(r($, '_record_line')),

    _record_line: $ => choice(
      r($, 'entry'),
      ...(m.tpl ? [
        alias($._tpl_spread_line, $.spread),
        alias($._tpl_record_if, $.if_block),
        alias($._tpl_record_for, $.for_block),
      ] : []),
    ),

    entry: $ => seq(
      $._key_start,
      field('key', r($, '_key')),
      choice(
        seq(':', blockValue($, m)),
        field('value', r($, 'table')),
      ),
    ),

    list: $ => repeat1(r($, '_list_line')),

    _list_line: $ => choice(
      r($, 'item'),
      ...(m.tpl ? [
        alias($._tpl_spread_line, $.spread),
        alias($._tpl_list_if, $.if_block),
        alias($._tpl_list_for, $.for_block),
      ] : []),
    ),

    item: $ => seq(
      '-',
      choice(
        // `-` then a block indented two spaces past the dash
        seq($._indent, field('value', r($, '_block')), $._dedent),
        // a record whose first field is on the dash line
        field('value', alias(hidden($, '_dash_record'), $.record)),
        seq(field('value', r($, '_inline_value')), $._newline),
        seq(field('value', r($, 'block_string')), $._newline),
        field('value', alias(hidden($, '_block_variant'), $.variant)),
      ),
    ),

    // `- key: value` followed by more fields indented two spaces past the
    // dash. The first field's own block is indented four spaces past it.
    _dash_record: $ => seq(
      choice(
        alias(hidden($, '_dash_entry'), $.entry),
        ...(m.tpl ? [alias($._tpl_dash_spread, $.spread)] : []),
      ),
      choice(
        $._newline,
        seq($._indent, repeat1(r($, '_record_line')), $._dedent),
      ),
    ),

    _dash_entry: $ => seq(
      $._key_start,
      field('key', r($, '_key')),
      choice(
        seq(':', choice(
          field('value', r($, '_inline_value')),
          field('value', alias(hidden($, '_dash_block_string'), $.block_string)),
          seq($._indent_dash, field('value', r($, '_block')), $._dedent),
          field('value', alias(hidden($, '_dash_block_variant'), $.variant)),
        )),
        field('value', alias(hidden($, '_dash_table'), $.table)),
      ),
    ),

    // A value after `key: ` or `- `: like a flow value, but a bare token is
    // a plain scalar that runs to the end of the line.
    _inline_value: $ => choice(
      r($, 'plain_scalar'),
      r($, '_scalar_literal'),
      r($, '_compound_flow_value'),
    ),

    block_string: $ => blockString($, m, false),

    _dash_block_string: $ => blockString($, m, true),

    // `key: !tag` followed by an indented block payload.
    _block_variant: $ => seq(
      '!',
      field('tag', alias($._immediate_identifier, $.tag)),
      $._indent,
      field('payload', r($, '_block')),
      $._dedent,
    ),

    _dash_block_variant: $ => seq(
      '!',
      field('tag', alias($._immediate_identifier, $.tag)),
      $._indent_dash,
      field('payload', r($, '_block')),
      $._dedent,
    ),

    // Tables (§4.9). The scanner checks the row count and the number of
    // cells in each row.
    table: $ => seq(
      field('header', $.table_header),
      ':',
      choice(
        $._newline, // `[0]{…}:`
        seq($._indent, repeat1(r($, 'row')), $._dedent),
      ),
    ),

    _dash_table: $ => seq(
      field('header', $.table_header),
      ':',
      choice(
        $._table_end, // `[0]{…}:`
        seq($._indent_dash, repeat1(r($, 'row')), $._dedent),
      ),
    ),

    row: $ => seq(
      $._row_start,
      choice(
        r($, 'cell'),
        seq(optional(r($, 'cell')), repeat1(seq(',', optional(r($, 'cell'))))),
      ),
      $._newline,
    ),

    cell: $ => r($, '_flow_value'),
  });
}

/**
 * The part of an entry after `key:`.
 * @param {GrammarSymbols<string>} $
 * @param {import('./common/flow.js').Mode} m
 */
function blockValue($, m) {
  const r = m.ref;
  return choice(
    seq(field('value', r($, '_inline_value')), $._newline),
    seq(field('value', r($, 'block_string')), $._newline),
    seq($._indent, field('value', r($, '_block')), $._dedent),
    field('value', alias($[m.name('_block_variant')], $.variant)),
  );
}

/**
 * @param {GrammarSymbols<string>} $
 * @param {import('./common/flow.js').Mode} m
 * @param {boolean} dash
 */
function blockString($, m, dash) {
  return seq(
    field('indicator', alias(choice('|', '|-'), $.block_string_indicator)),
    optional(seq(
      dash ? $._block_string_begin_dash : $._block_string_begin,
      repeat(choice(
        alias($[m.name('_block_string_fragment')], $.string_content),
        ...(m.tpl ? [$.interpolation] : []),
      )),
      $._block_string_end,
    )),
  );
}

/**
 * @param {GrammarSymbols<string>} $
 * @param {'record' | 'list'} kind
 */
function controlBody($, kind) {
  return seq(
    ':',
    $._indent,
    field('body', TEMPLATE.ref($, kind)),
    $._dedent,
  );
}

/**
 * @param {GrammarSymbols<string>} $
 * @param {'record' | 'list'} kind
 */
function ifBlock($, kind) {
  return seq(
    '@if',
    field('condition', $.condition),
    controlBody($, kind),
    optional(field('alternative', alias($[`_tpl_${kind}_else`], $.else_block))),
  );
}

/**
 * @param {GrammarSymbols<string>} $
 * @param {'record' | 'list'} kind
 */
function elseBlock($, kind) {
  return seq(
    '@else',
    choice(
      controlBody($, kind),
      seq(
        'if',
        field('condition', $.condition),
        controlBody($, kind),
        optional(field('alternative', alias($[`_tpl_${kind}_else`], $.else_block))),
      ),
    ),
  );
}

/**
 * @param {GrammarSymbols<string>} $
 * @param {'record' | 'list'} kind
 */
function forBlock($, kind) {
  return seq(
    '@for',
    optional(seq(field('index', $.identifier), ',')),
    field('variable', $.identifier),
    'in',
    field('iterable', $._tpl_flow_value),
    controlBody($, kind),
  );
}
