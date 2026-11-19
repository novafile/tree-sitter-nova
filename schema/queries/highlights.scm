; NOVA schema language (.novs) syntax highlighting

(comment) @comment

; Directives -----------------------------------------------------------

(directive_name) @keyword.directive

(schema_ref
  name: (identifier) @module)

(schema_ref
  "@" @punctuation.delimiter)

(version) @string.special

(directive
  argument: (argument) @string.special)

; Types ----------------------------------------------------------------
; (Generic captures first: for the same node, later patterns win.)

(type_identifier) @type

(primitive_type) @type.builtin

[
  "list"
  "set"
  "map"
] @type.builtin

(nullable_type
  "?" @punctuation.special)

; Definitions ----------------------------------------------------------

[
  "type"
  "enum"
  "record"
] @keyword.type

(type_definition
  name: (type_identifier) @type.definition)

(enum_definition
  name: (type_identifier) @type.definition)

(record_definition
  name: (type_identifier) @type.definition)

(field
  name: (identifier) @property)

(enum_variant
  name: (identifier) @constructor)

(optional_marker) @punctuation.special

; Attributes -----------------------------------------------------------

(attribute_name) @attribute

(unit) @constant.builtin

(range
  ".." @operator)

; Defaults (NOVA flow values) -------------------------------------------

(number) @number

(boolean) @boolean

(null) @constant.builtin

(string) @string

(string_content) @string

(escape_sequence) @string.escape

(word) @string

(key) @property

(typed_literal
  type: (literal_type) @type.builtin)

[
  "@set"
  "@map"
] @type.builtin

(variant
  "!" @constructor
  tag: (tag) @constructor)

; Punctuation ----------------------------------------------------------

[
  "("
  ")"
  "["
  "]"
  "{"
  "}"
  "<"
  ">"
  "[]"
] @punctuation.bracket

[
  ","
  ":"
  "|"
  "=>"
] @punctuation.delimiter

"=" @operator
