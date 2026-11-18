; NOVA syntax highlighting

(comment) @comment

; Directives -----------------------------------------------------------

(directive_name) @keyword.directive

(directive
  argument: (argument) @string.special)

(directive
  parameter: (identifier) @variable.parameter)

(directive
  binding: (identifier) @variable)

(directive
  alias: (identifier) @module)

(param_type) @type

(optional_marker) @punctuation.special

"as" @keyword.import

; Keys -----------------------------------------------------------------

(key) @property

(entry
  key: (string) @property)

(column) @property

(table_header
  column: (string) @property)

(row_count) @number

; Scalars --------------------------------------------------------------

(plain_scalar) @string

(word) @string

(string) @string

(string_content) @string

(block_string) @string

(block_string_indicator) @punctuation.special

(escape_sequence) @string.escape

(number) @number

(boolean) @boolean

(null) @constant.builtin

; Typed literals and collections ----------------------------------------

(typed_literal
  type: (literal_type) @type.builtin)

[
  "@set"
  "@map"
] @type.builtin

; Variants -------------------------------------------------------------

(variant
  "!" @constructor
  tag: (tag) @constructor)

; Templates ------------------------------------------------------------

(template_ref
  "$" @punctuation.special)

(template_ref
  path: (path
    (field) @variable))

(interpolation
  [
    "${"
    "}"
  ] @punctuation.special)

(interpolation
  path: (path
    (field) @variable))

"..." @operator

[
  "@if"
  "@else"
  "if"
] @keyword.conditional

[
  "@for"
  "in"
] @keyword.repeat

(for_block
  index: (identifier) @variable)

(for_block
  variable: (identifier) @variable)

[
  "and"
  "or"
  "not"
] @keyword.operator

[
  "=="
  "!="
] @operator

; Paths and patches ----------------------------------------------------

(patch_op
  path: (path
    (field) @property))

(selector
  field: (field) @property)

(projection
  (field) @property)

(wildcard) @character.special

(filter) @string.special

(patch_op
  [
    "="
    "+"
    "-"
    "?"
  ] @operator)

"->" @operator

; Punctuation ----------------------------------------------------------

[
  "["
  "]"
  "{"
  "}"
  "("
  ")"
] @punctuation.bracket

[
  ","
  ":"
  "."
] @punctuation.delimiter

"=>" @punctuation.delimiter

(item
  "-" @punctuation.special)
