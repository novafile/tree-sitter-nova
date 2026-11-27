; NOVA syntax highlighting

(comment) @comment

(document_separator) @punctuation.special

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

; A quoted variant tag (!"at start") is a tag, not string data. For the same
; node the later pattern wins, so this follows the general capture above.
(tag (string_content) @constructor)

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

; Paths in interpolations and expressions, `${svc.port}`; patch paths
; below are captured again as properties (later patterns win).
(path
  (field) @variable)

; Template language 0.2 (proposal 0006): calls, @def, operators, comprehensions.
(call
  "$" @punctuation.special)

; Also `upper(x)` at the head of an interpolation, which has no `$`.
(call
  function: (function) @function.call)

(parenthesized_expression
  "$" @punctuation.special)

(directive
  function: (function) @function)

(def_parameter
  name: (identifier) @variable.parameter)

(binary_expression
  operator: _ @operator)

[
  "[for"
  "{for"
] @keyword.repeat

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

(list_comprehension
  index: (identifier) @variable)

(list_comprehension
  variable: (identifier) @variable)

(record_comprehension
  index: (identifier) @variable)

(record_comprehension
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

; The root path `.` (spec §7.1).
(path (root) @punctuation.special)

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
