/**
 * @file Rules shared by the NOVA grammars (`nova` and `nova_schema`)
 * @author NOVA contributors
 * @license Apache-2.0
 *
 * Flow values (spec §4.5–4.8) and the lexical rules they need. A schema
 * default (§6.1) is a flow value of a data document, so `nova_schema` uses
 * the data-mode rules; `nova` uses them twice, once for data documents and
 * once for template documents (§9), where `$ref`, `${…}` and `...` are
 * recognized.
 */

/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

export const IDENT = /[A-Za-z_][A-Za-z0-9_-]*/;

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
export function commaSep(rule) {
  return optional(seq(rule, repeat(seq(',', rule)), optional(',')));
}

/**
 * Comma-separated, non-empty, no trailing comma.
 * @param {RuleOrLiteral} rule
 */
export function commaSep1(rule) {
  return seq(rule, repeat(seq(',', rule)));
}

/**
 * @typedef {object} Mode
 * @property {boolean} tpl  template document rules
 * @property {(name: string) => string} name  the rule name in this mode
 * @property {($: GrammarSymbols<string>, name: string) => Rule} ref
 *   a reference to a rule of this mode; template rules are hidden and
 *   aliased to the data rule's name, so both modes produce the same nodes
 */

/**
 * @param {'data' | 'template'} kind
 * @returns {Mode}
 */
export function mode(kind) {
  const tpl = kind === 'template';
  /** @param {string} n */
  const name = n => (!tpl ? n : n.startsWith('_') ? `_tpl${n}` : `_tpl_${n}`);
  return {
    tpl,
    name,
    ref: ($, n) => (tpl && !n.startsWith('_') ? alias($[name(n)], $[n]) : $[name(n)]),
  };
}

/**
 * Prefix every key of a rule object with the mode's naming.
 * @param {Mode} m
 * @param {Record<string, ($: GrammarSymbols<string>) => RuleOrLiteral>} rules
 */
export function named(m, rules) {
  return Object.fromEntries(Object.entries(rules).map(([k, v]) => [m.name(k), v]));
}

/**
 * Flow values in one mode.
 * @param {Mode} m
 */
export function flowRules(m) {
  const r = m.ref;
  return named(m, {
    _flow_value: $ => choice(
      $.word,
      // `$` is reserved in data documents (§9.2), but `[$x]` is read as a
      // word there (as `$x` after `key: ` is a plain scalar); validators
      // report it.
      ...(m.tpl ? [] : [alias($._dollar_word, $.word)]),
      r($, '_scalar_literal'),
      r($, '_compound_flow_value'),
    ),

    _scalar_literal: $ => choice(
      $.number,
      $.boolean,
      $.null,
      r($, 'string'),
    ),

    _compound_flow_value: $ => choice(
      r($, 'flow_list'),
      r($, 'flow_record'),
      r($, 'tuple'),
      r($, 'set'),
      r($, 'map'),
      $.typed_literal,
      r($, 'variant'),
      ...(m.tpl ? [$.template_ref] : []),
    ),

    // In templates, `${path}` interpolates (§9.3); in data documents `$`
    // is ordinary string content.
    string: $ => seq(
      '"',
      repeat(choice(
        $.string_content,
        $.escape_sequence,
        ...(m.tpl ? [$.interpolation] : []),
      )),
      token.immediate('"'),
    ),

    _key: $ => choice(alias($.identifier, $.key), r($, 'string')),

    flow_list: $ => seq(
      '[',
      commaSep(m.tpl ? choice(r($, '_flow_value'), alias($._tpl_flow_spread, $.spread)) : r($, '_flow_value')),
      ']',
    ),

    flow_record: $ => seq(
      '{',
      commaSep(m.tpl
        ? choice(alias($._tpl_flow_entry, $.entry), alias($._tpl_flow_spread, $.spread))
        : alias($._flow_entry, $.entry)),
      '}',
    ),

    _flow_entry: $ => seq(
      field('key', r($, '_key')),
      ':',
      field('value', r($, '_flow_value')),
    ),

    // `()`, `(a,)`, `(a, b)`, `(a, b,)` — `(a)` is not a tuple.
    tuple: $ => seq(
      '(',
      optional(seq(
        r($, '_flow_value'),
        ',',
        optional(seq(commaSep1(r($, '_flow_value')), optional(','))),
      )),
      ')',
    ),

    set: $ => seq('@set', token.immediate('['), commaSep(r($, '_flow_value')), ']'),

    map: $ => seq('@map', token.immediate('{'), commaSep(r($, 'map_entry')), '}'),

    map_entry: $ => seq(field('key', r($, '_flow_value')), '=>', field('value', r($, '_flow_value'))),

    // !tag  or  !tag(payload)
    variant: $ => seq(
      '!',
      field('tag', alias($._immediate_identifier, $.tag)),
      optional(seq(
        token.immediate('('),
        field('payload', r($, '_flow_value')),
        ')',
      )),
    ),
  });
}

/**
 * Mode-independent lexical rules and typed literals.
 */
export const lexicalRules = {
  // int | float | rational (§4.5). Declared before `word` so that an
  // exact number wins over a word of the same length.
  number: _ => token(/-?(0|[1-9][0-9]*)(\.[0-9]+([eE][+-]?[0-9]+)?|[eE][+-]?[0-9]+|\/[1-9][0-9]*)?/),

  boolean: _ => choice('true', 'false'),

  null: _ => 'null',

  _dollar_word: _ => token(seq('$', repeat(WORD_CHAR))),

  // A bare token in flow context (§4.7).
  word: _ => token(choice(
    seq(WORD_START, repeat(WORD_CHAR)),
    seq('.', optional(seq(WORD_CHAR_NO_DOT, repeat(WORD_CHAR)))),
    seq('..', optional(seq(WORD_CHAR_NO_DOT, repeat(WORD_CHAR)))),
  )),

  string_content: _ => choice(
    token.immediate(prec(1, /[^"\\$\n]+/)),
    token.immediate(prec(1, '$$')),
    token.immediate(prec(1, '$')),
  ),

  escape_sequence: _ => token.immediate(prec(1,
    /\\(["\\\/bfnrt$]|u[0-9a-fA-F]{4}|u\{[0-9a-fA-F]{1,6}\})/,
  )),

  // @timestamp("…"), @duration("…"), @decimal("…"), @rational("…"),
  // @bytes("…"), @float("…")  (§4.8). The argument is a string literal
  // in templates too.
  /** @param {GrammarSymbols<string>} $ */
  typed_literal: $ => seq(
    field('type', alias($._at_name, $.literal_type)),
    token.immediate('('),
    field('argument', $.string),
    ')',
  ),

  identifier: _ => IDENT,

  _immediate_identifier: _ => token.immediate(IDENT),

  _at_name: _ => token(seq('@', IDENT)),

  comment: _ => token(seq('#', /.*/)),
};
