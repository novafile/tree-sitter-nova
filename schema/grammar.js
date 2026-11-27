/**
 * @file NOVA schema language (`.novs`) grammar for tree-sitter
 * @author NOVA contributors
 * @license Apache-2.0
 *
 * Covers the schema language of the NOVA specification (§6.1): the
 * `@schema name@version`, `@import name@version[#fingerprint] as alias`,
 * `@root` and `@nova` directives, `record`, `enum`
 * and `type` definitions, fields, types (including generics, tuples,
 * `T[]`, `T?`, unions `A | B` and string literal types), quoted field names,
 * attributes, `@x-…` extension directives and defaults. Defaults are NOVA flow values, shared with the `nova` grammar
 * (`common/flow.js`).
 *
 * Line structure (fields and block-form enum variants indented exactly two
 * spaces) is handled by the external scanner in `schema/src/scanner.c`.
 */

/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

import { commaSep, flowRules, lexicalRules, mode } from '../common/flow.js';

const DATA = mode('data');

// Built-in types (§6.1).
const PRIMITIVE_TYPES = [
  'any', 'null', 'bool', 'float', 'decimal', 'rational', 'string', 'bytes',
  'timestamp', 'duration',
  'int', 'i8', 'i16', 'i32', 'i64', 'u8', 'u16', 'u32', 'u64',
];

const PREC = {
  ARRAY: 2,
  NULLABLE: 2,
};

export default grammar({
  name: 'nova_schema',

  externals: $ => [
    $._newline,
    // The end of a field or block-form enum variant line.
    $._member_newline,
    $._indent,
    $._dedent,
    // Never accepted by a rule: emitted where a line is indented wrongly,
    // so that the tree has an ERROR there (see schema/src/scanner.c).
    $.indentation_error,
    $._error_sentinel,
  ],

  extras: $ => [/[ \t\r\n\f﻿]/, $.comment],

  word: $ => $.identifier,

  supertypes: $ => [
    $._definition,
  ],

  // On a field, `type` or `@root` line, attributes after a union member
  // belong to that member if another `|` follows, otherwise to the line.
  conflicts: $ => [
    [$._line_type, $.attributed_type],
    [$._line_union_type, $.attributed_type],
  ],

  rules: {
    schema: $ => repeat(choice(
      $.directive,
      $._definition,
    )),

    // ------------------------------------------------------------------
    // Directives
    // ------------------------------------------------------------------

    directive: $ => choice(
      // @schema name@MAJOR.MINOR.PATCH
      seq(
        field('name', alias('@schema', $.directive_name)),
        field('schema', $.schema_ref),
        $._newline,
      ),
      // @import name@version[#fingerprint] as alias (proposal 0008)
      seq(
        field('name', alias('@import', $.directive_name)),
        field('schema', $.schema_ref),
        'as',
        field('alias', alias($.identifier, $.module)),
        $._newline,
      ),
      // @root T attributes*: any type, e.g. `@root Event[] @len(1..)`
      seq(
        field('name', alias('@root', $.directive_name)),
        field('type', $._line_type),
        repeat(field('attribute', $.attribute)),
        $._newline,
      ),
      // @nova MAJOR.MINOR
      seq(
        field('name', alias('@nova', $.directive_name)),
        field('version', $.version),
        $._newline,
      ),
      // @x-… extension directives (proposal 0004 §0004.7), e.g.
      // `@x-lint case=kebab`. Other unknown directives stay errors.
      seq(
        field('name', alias(token(prec(1, /@x-[A-Za-z0-9_-]*/)), $.directive_name)),
        optional(field('argument', $.argument)),
        $._newline,
      ),
    ),

    // The rest of the line, up to a comment. Unlike common ARGUMENT it may
    // start with `(`: schemas have no `@if(…)`-style directives to exclude.
    argument: _ => token(/[^ \t\r\n#]([^ \t\r\n]|[ \t]+[^ \t\r\n#])*/),

    // name@version, with a fingerprint in @import lines.
    schema_ref: $ => seq(
      field('name', $.schema_name),
      token.immediate('@'),
      field('version', alias($._immediate_version, $.version)),
      optional(field('fingerprint', $.fingerprint)),
    ),

    // A qualified name rooted in a domain (`novafile.dev/std`) or a local
    // name (`pddl`, `infra/net`), spec §6.7.
    schema_name: _ => /[a-z0-9][a-z0-9._-]*(\/[a-z0-9][a-z0-9._-]*)*/,

    // `#` directly after the version. Lexical precedence keeps it from
    // being read as a comment, which would be the longer match.
    fingerprint: _ => token.immediate(prec(1, /#[0-9A-Fa-f]+/)),

    // SemVer 2.0.0 (§6.7), or MAJOR.MINOR for @nova.
    version: _ => /[0-9]+\.[0-9]+(\.[0-9]+(-[0-9A-Za-z.-]+)?(\+[0-9A-Za-z.-]+)?)?/,

    _immediate_version: _ => token.immediate(/[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.-]+)?(\+[0-9A-Za-z.-]+)?/),

    // ------------------------------------------------------------------
    // Definitions
    // ------------------------------------------------------------------

    _definition: $ => choice(
      $.type_definition,
      $.enum_definition,
      $.record_definition,
    ),

    // type Name = T attributes*
    type_definition: $ => seq(
      'type',
      field('name', alias($.identifier, $.type_identifier)),
      '=',
      field('type', $._line_type),
      repeat(field('attribute', $.attribute)),
      $._newline,
    ),

    // enum Name = a | b(T)   |   enum Name: + one variant per line
    enum_definition: $ => seq(
      'enum',
      field('name', alias($.identifier, $.type_identifier)),
      // A class, `@secret` or `@personal` (proposal 0009).
      repeat(field('attribute', $.attribute)),
      choice(
        seq(
          '=',
          field('variant', $.enum_variant),
          repeat(seq('|', field('variant', $.enum_variant))),
          $._newline,
        ),
        seq(
          ':',
          $._indent,
          repeat1(seq(field('variant', $.enum_variant), $._member_newline)),
          $._dedent,
        ),
      ),
    ),

    // The name may be quoted, like a field name (spec §6.1): "at start"(T).
    enum_variant: $ => seq(
      field('name', choice($.identifier, $.string)),
      optional(seq(
        token.immediate('('),
        field('payload', $._type_argument),
        ')',
      )),
      // @id(n), like on fields (spec §6.1).
      repeat(field('attribute', $.attribute)),
    ),

    // record Name [@open]: + one field per line
    record_definition: $ => seq(
      'record',
      field('name', alias($.identifier, $.type_identifier)),
      repeat(field('attribute', $.attribute)),
      ':',
      choice(
        $._newline,
        seq($._indent, repeat1($.field), $._dedent),
      ),
    ),

    // name[?]: type attributes* [= default]; the name may be quoted.
    field: $ => seq(
      field('name', choice($.identifier, $.string)),
      optional(field('optional', alias(token.immediate('?'), $.optional_marker))),
      token.immediate(':'),
      field('type', $._line_type),
      repeat(field('attribute', $.attribute)),
      optional(seq('=', field('default', $._flow_value))),
      $._member_newline,
    ),

    // ------------------------------------------------------------------
    // Types
    // ------------------------------------------------------------------

    _type: $ => choice(
      $.primitive_type,
      alias($.identifier, $.type_identifier),
      $.qualified_type,
      $.list_type,
      $.set_type,
      $.map_type,
      $.tuple_type,
      $.array_type,
      $.nullable_type,
      // A string literal type, "total-time" (proposal 0004 §0004.3).
      $.string,
    ),

    primitive_type: _ => choice(...PRIMITIVE_TYPES),

    // alias.Name: a definition of an imported schema (proposal 0008).
    qualified_type: $ => seq(
      field('module', alias($.identifier, $.module)),
      token.immediate('.'),
      field('name', alias($._immediate_identifier, $.type_identifier)),
    ),

    // A type argument may carry type attributes: list<u16 @range(1..1024)>.
    _type_argument: $ => choice($._type, $.attributed_type, $.union_type),

    // The type of a field, `type` or `@root` line, which is followed by the
    // line's own attributes.
    _line_type: $ => choice($._type, alias($._line_union_type, $.union_type)),

    // A | B | …: `|` binds more loosely than `[]`, `?` and type attributes,
    // which belong to the member they follow.
    union_type: $ => seq(
      repeat1(seq($._union_member, '|')),
      $._union_member,
    ),

    // As union_type, but attributes after the last member belong to the line.
    _line_union_type: $ => seq(
      repeat1(seq($._union_member, '|')),
      field('member', $._type),
    ),

    _union_member: $ => field('member', choice($._type, $.attributed_type)),

    attributed_type: $ => seq(
      field('type', $._type),
      repeat1(field('attribute', $.attribute)),
    ),

    list_type: $ => seq('list', token.immediate('<'), field('element', $._type_argument), '>'),

    set_type: $ => seq('set', token.immediate('<'), field('element', $._type_argument), '>'),

    map_type: $ => seq(
      'map',
      token.immediate('<'),
      field('key', $._type_argument),
      ',',
      field('value', $._type_argument),
      '>',
    ),

    // (A, B, …)
    tuple_type: $ => seq('(', commaSep(field('element', $._type_argument)), ')'),

    // T[]
    array_type: $ => prec(PREC.ARRAY, seq(field('element', $._type), token.immediate('[]'))),

    // T?
    nullable_type: $ => prec(PREC.NULLABLE, seq(field('type', $._type), token.immediate('?'))),

    // ------------------------------------------------------------------
    // Attributes (§6.1)
    // ------------------------------------------------------------------

    attribute: $ => choice(
      // @range(lo..hi)
      seq(
        field('name', alias('@range', $.attribute_name)),
        token.immediate('('),
        field('range', $.range),
        ')',
      ),
      // @len(lo..hi[, unit])
      seq(
        field('name', alias('@len', $.attribute_name)),
        token.immediate('('),
        field('range', $.range),
        optional(seq(',', field('unit', alias($.identifier, $.unit)))),
        ')',
      ),
      // @pattern("p"): an I-Regexp (proposal 0004 §0004.2)
      seq(
        field('name', alias('@pattern', $.attribute_name)),
        token.immediate('('),
        field('pattern', $.string),
        ')',
      ),
      // @id(n)
      seq(
        field('name', alias('@id', $.attribute_name)),
        token.immediate('('),
        field('id', $.number),
        ')',
      ),
      // Record rules (proposal 0010): @together(a, b), @one_of(…), @any_of(…)
      seq(
        field('name', alias(choice('@together', '@one_of', '@any_of'), $.attribute_name)),
        token.immediate('('),
        commaSep(field('field', alias($.identifier, $.field_name))),
        ')',
      ),
      // @key, @open, @deprecated, and the classes @secret and @personal
      field('name', alias($._at_name, $.attribute_name)),
    ),

    // Inclusive bounds; either side may be omitted.
    range: $ => seq(
      optional(field('min', $.number)),
      '..',
      optional(field('max', $.number)),
    ),

    // ------------------------------------------------------------------
    // Defaults: NOVA flow values
    // ------------------------------------------------------------------

    ...flowRules(DATA),
    ...lexicalRules,
  },
});
