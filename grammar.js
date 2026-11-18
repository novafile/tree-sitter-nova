/**
 * @file NOVA (Notation for Objects, Values & Agents) grammar for tree-sitter
 * @author NOVA contributors
 * @license Apache-2.0
 *
 * Covers the NOVA text syntax (spec §4), the positional profile (§4.12),
 * patch documents (§7.4) and template documents (§9).
 *
 * Indentation, block-string content, plain scalars and the "is this a key?"
 * decision are handled by the external scanner in `src/scanner.c`.
 */

/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

const IDENT = /[A-Za-z_][A-Za-z0-9_-]*/;

// Characters that may not begin a flow word (spec §4.4), plus `.`, which is
// handled separately so that a word never begins with `...`.
const WORD_START = /[^\s!"#$%&'*,:<=>?@\[\\\]^`{|}~().]/;
// A character inside a flow word: anything but whitespace and `, [ ] { } ( ) "`;
// a `:` only when it is followed by another word character.
const WORD_CHAR = /[^\s,\[\]{}()":]|:[^\s,\[\]{}()":]/;
const WORD_CHAR_NO_DOT = /[^\s,\[\]{}()":.]/;

/**
 * Comma-separated list with an optional trailing comma.
 * @param {RuleOrLiteral} rule
 */
function commaSep(rule) {
  return optional(seq(rule, repeat(seq(',', rule)), optional(',')));
}

/**
 * Comma-separated, non-empty, no trailing comma.
 * @param {RuleOrLiteral} rule
 */
function commaSep1(rule) {
  return seq(rule, repeat(seq(',', rule)));
}

export default grammar({
  name: 'nova',

  externals: $ => [
    $._newline,
    $._indent,
    $._dedent,
    $._key_start,
    $.plain_scalar,
    $._block_string_begin,
    $._block_string_begin_dash,
    $._block_string_fragment,
    $._block_string_end,
    $._table_start,
    $._error_sentinel,
  ],

  extras: $ => [/[ \t\r\n\f\uFEFF]/, $.comment],

  conflicts: $ => [
    // `key: !tag` is a unit variant, unless an indented block follows.
    [$.variant, $._block_variant],
    // A block made only of spread lines can be a record or a list.
    [$._record_line, $._list_line],
  ],

  rules: {
    document: $ => seq(
      repeat($._directive),
      optional(choice(
        $._body,
        $._patch_section,
      )),
    ),

    // ------------------------------------------------------------------
    // Directives (§4.2, §9.2, §7.4)
    // ------------------------------------------------------------------

    _directive: $ => choice(
      $.directive,
      alias($._param_directive, $.directive),
      alias($._import_directive, $.directive),
      alias($._let_directive, $.directive),
    ),

    directive: $ => seq(
      field('name', alias($._at_name, $.directive_name)),
      optional(field('argument', $.argument)),
      $._newline,
    ),

    // Rest of the line, up to a comment (`#` preceded by whitespace).
    argument: _ => token(/[^ \t\r\n#(]([^ \t\r\n]|[ \t]+[^ \t\r\n#])*/),

    // @param name[?]: type [= default]
    _param_directive: $ => seq(
      field('name', alias('@param', $.directive_name)),
      field('parameter', $.identifier),
      optional(field('optional', alias(token.immediate('?'), $.optional_marker))),
      token.immediate(':'),
      field('type', $.param_type),
      optional(seq('=', field('default', $._flow_value))),
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
        seq('=', field('value', $._flow_value), $._newline),
        seq(':', blockValue($, false)),
      ),
    ),

    // ------------------------------------------------------------------
    // Body (§4.2)
    // ------------------------------------------------------------------

    _body: $ => choice(
      $.record,
      $.list,
      seq($._table_start, $.table),
      seq($._flow_value, $._newline),
    ),

    _block: $ => choice($.record, $.list),

    // ------------------------------------------------------------------
    // Block records (§4.3)
    // ------------------------------------------------------------------

    record: $ => repeat1($._record_line),

    _record_line: $ => choice(
      $.entry,
      alias($._spread_line, $.spread),
      alias($._record_if, $.if_block),
      alias($._record_for, $.for_block),
    ),

    entry: $ => entryRule($, false),

    // The first entry of a record that starts on a list item's dash line;
    // its block-string content is indented two more spaces.
    _dash_entry: $ => entryRule($, true),

    _key: $ => choice(alias($.identifier, $.key), $.string),

    // ------------------------------------------------------------------
    // Block lists (§4.3)
    // ------------------------------------------------------------------

    list: $ => repeat1($._list_line),

    _list_line: $ => choice(
      $.item,
      alias($._spread_line, $.spread),
      alias($._list_if, $.if_block),
      alias($._list_for, $.for_block),
    ),

    item: $ => seq(
      '-',
      choice(
        // `-` then an indented block
        seq($._newline, $._indent, field('value', $._block), $._dedent),
        // a record whose first field is on the dash line
        field('value', alias($._dash_record, $.record)),
        seq(field('value', $._inline_value), $._newline),
        seq(field('value', $.block_string), $._newline),
        field('value', alias($._block_variant, $.variant)),
      ),
    ),

    _dash_record: $ => seq(
      choice(
        alias($._dash_entry, $.entry),
        alias($._spread_line, $.spread),
      ),
      optional(seq($._indent, repeat1($._record_line), $._dedent)),
    ),

    // ------------------------------------------------------------------
    // Inline values (§4.4–4.8)
    // ------------------------------------------------------------------

    // A value after `key: ` or `- `: like a flow value, but a bare token is
    // a plain scalar that runs to the end of the line.
    _inline_value: $ => choice(
      $.plain_scalar,
      $._scalar_literal,
      $._compound_flow_value,
    ),

    _flow_value: $ => choice(
      $.word,
      $._scalar_literal,
      $._compound_flow_value,
    ),

    _scalar_literal: $ => choice(
      $.number,
      $.boolean,
      $.null,
      $.string,
    ),

    _compound_flow_value: $ => choice(
      $.flow_list,
      $.flow_record,
      $.tuple,
      $.set,
      $.map,
      $.typed_literal,
      $.variant,
      $.template_ref,
    ),

    // int | float | rational (§4.5). Declared before `word` so that an
    // exact number wins over a word of the same length.
    number: _ => token(/-?(0|[1-9][0-9]*)(\.[0-9]+([eE][+-]?[0-9]+)?|[eE][+-]?[0-9]+|\/[1-9][0-9]*)?/),

    boolean: _ => choice('true', 'false'),

    null: _ => 'null',

    // A bare token in flow context (§4.7).
    word: _ => token(choice(
      seq(WORD_START, repeat(WORD_CHAR)),
      seq('.', optional(seq(WORD_CHAR_NO_DOT, repeat(WORD_CHAR)))),
      seq('..', optional(seq(WORD_CHAR_NO_DOT, repeat(WORD_CHAR)))),
    )),

    // ------------------------------------------------------------------
    // Strings (§4.6, §9.3)
    // ------------------------------------------------------------------

    string: $ => seq(
      '"',
      repeat(choice(
        $.string_content,
        $.escape_sequence,
        $.interpolation,
      )),
      token.immediate('"'),
    ),

    string_content: _ => choice(
      token.immediate(prec(1, /[^"\\$\n]+/)),
      token.immediate(prec(1, '$$')),
      token.immediate(prec(1, '$')),
    ),

    escape_sequence: _ => token.immediate(prec(1,
      /\\(["\\\/bfnrt$]|u[0-9a-fA-F]{4}|u\{[0-9a-fA-F]{1,6}\})/,
    )),

    // `${path}` inside a quoted or block string (templates).
    interpolation: $ => seq(
      choice(token.immediate(prec(2, '${')), '${'),
      field('path', $.path),
      '}',
    ),

    block_string: $ => blockString($, false),

    _dash_block_string: $ => blockString($, true),

    // ------------------------------------------------------------------
    // Flow values (§4.7)
    // ------------------------------------------------------------------

    flow_list: $ => seq('[', commaSep(choice($._flow_value, alias($._flow_spread, $.spread))), ']'),

    flow_record: $ => seq(
      '{',
      commaSep(choice(alias($._flow_entry, $.entry), alias($._flow_spread, $.spread))),
      '}',
    ),

    _flow_entry: $ => seq(
      field('key', $._key),
      ':',
      field('value', $._flow_value),
    ),

    // `()`, `(a,)`, `(a, b)`, `(a, b,)` — `(a)` is not a tuple.
    tuple: $ => seq(
      '(',
      optional(seq(
        $._flow_value,
        ',',
        optional(seq(commaSep1($._flow_value), optional(','))),
      )),
      ')',
    ),

    set: $ => seq('@set', token.immediate('['), commaSep($._flow_value), ']'),

    map: $ => seq('@map', token.immediate('{'), commaSep($.map_entry), '}'),

    map_entry: $ => seq(field('key', $._flow_value), '=>', field('value', $._flow_value)),

    // @timestamp("…"), @duration("…"), @decimal("…"), @rational("…"),
    // @bytes("…"), @float("…")  (§4.8)
    typed_literal: $ => seq(
      field('type', alias($._at_name, $.literal_type)),
      token.immediate('('),
      field('argument', $.string),
      ')',
    ),

    // !tag  or  !tag(payload)
    variant: $ => seq(
      '!',
      field('tag', alias($._immediate_identifier, $.tag)),
      optional(seq(
        token.immediate('('),
        field('payload', $._flow_value),
        ')',
      )),
    ),

    // `key: !tag` followed by an indented block payload.
    _block_variant: $ => seq(
      '!',
      field('tag', alias($._immediate_identifier, $.tag)),
      $._newline,
      $._indent,
      field('payload', $._block),
      $._dedent,
    ),

    // ------------------------------------------------------------------
    // Tables (§4.9)
    // ------------------------------------------------------------------

    table: $ => seq(
      field('header', $.table_header),
      ':',
      $._newline,
      $._indent,
      repeat1($.row),
      $._dedent,
    ),

    table_header: $ => seq(
      '[',
      field('count', alias(token.immediate(/[0-9]+/), $.row_count)),
      token.immediate(']'),
      token.immediate('{'),
      commaSep1(field('column', choice(alias($.identifier, $.column), $.string))),
      '}',
    ),

    row: $ => seq(
      choice(
        $.cell,
        seq(optional($.cell), repeat1(seq(',', optional($.cell)))),
      ),
      $._newline,
    ),

    cell: $ => $._flow_value,

    // ------------------------------------------------------------------
    // Templates (§9)
    // ------------------------------------------------------------------

    // $name.path[0]
    template_ref: $ => seq(
      '$',
      field('path', alias($._ref_path, $.path)),
    ),

    _ref_path: $ => seq(
      alias($._immediate_identifier, $.field),
      repeat($._path_segment),
    ),

    _spread_line: $ => seq('...', field('value', $._flow_value), $._newline),

    _flow_spread: $ => seq('...', field('value', $._flow_value)),

    _record_if: $ => ifBlock($, 'record'),
    _list_if: $ => ifBlock($, 'list'),
    _record_else: $ => elseBlock($, 'record'),
    _list_else: $ => elseBlock($, 'list'),
    _record_for: $ => forBlock($, 'record'),
    _list_for: $ => forBlock($, 'list'),

    condition: $ => $._condition,

    _condition: $ => choice(
      $.binary_condition,
      $.not_condition,
      $.parenthesized_condition,
      $.comparison,
      $._flow_value,
    ),

    binary_condition: $ => choice(
      prec.left(1, seq(field('left', $._condition), field('operator', 'or'), field('right', $._condition))),
      prec.left(2, seq(field('left', $._condition), field('operator', 'and'), field('right', $._condition))),
    ),

    not_condition: $ => prec(3, seq('not', field('operand', $._condition))),

    comparison: $ => prec(4, seq(
      field('left', $._flow_value),
      field('operator', choice('==', '!=')),
      field('right', $._flow_value),
    )),

    parenthesized_condition: $ => seq('(', $._condition, ')'),

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
      repeat($._directive),
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

    identifier: _ => IDENT,

    _immediate_identifier: _ => token.immediate(IDENT),

    _at_name: _ => token(seq('@', IDENT)),

    comment: _ => token(seq('#', /.*/)),
  },
});

/**
 * The part of an entry after `key:`.
 * @param {GrammarSymbols<string>} $
 * @param {boolean} dash
 */
function blockValue($, dash) {
  return choice(
    seq(field('value', $._inline_value), $._newline),
    seq(
      field('value', dash ? alias($._dash_block_string, $.block_string) : $.block_string),
      $._newline,
    ),
    seq($._newline, $._indent, field('value', $._block), $._dedent),
    field('value', alias($._block_variant, $.variant)),
  );
}

/**
 * @param {GrammarSymbols<string>} $
 * @param {boolean} dash
 */
function entryRule($, dash) {
  return seq(
    $._key_start,
    field('key', $._key),
    choice(
      seq(':', blockValue($, dash)),
      field('value', $.table),
    ),
  );
}

/**
 * @param {GrammarSymbols<string>} $
 * @param {boolean} dash
 */
function blockString($, dash) {
  return seq(
    field('indicator', alias(choice('|', '|-'), $.block_string_indicator)),
    optional(seq(
      dash ? $._block_string_begin_dash : $._block_string_begin,
      repeat(choice(
        alias($._block_string_fragment, $.string_content),
        $.interpolation,
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
    $._newline,
    $._indent,
    field('body', $[kind]),
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
    optional(field('alternative', alias($[`_${kind}_else`], $.else_block))),
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
        optional(field('alternative', alias($[`_${kind}_else`], $.else_block))),
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
    field('iterable', $._flow_value),
    controlBody($, kind),
  );
}
