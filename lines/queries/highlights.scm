; NOVA Lines syntax highlighting

(comment) @comment

; Directives -----------------------------------------------------------

(directive_name) @keyword.directive

(directive
  argument: (argument) @string.special)

; Column specs ---------------------------------------------------------

(key) @property

(column
  name: (string) @property)

(nested_column
  name: (string) @property)

(rest_column) @operator

(entry
  key: (string) @property)

; Scalars --------------------------------------------------------------

(word) @string

(string) @string

(string_content) @string

; A quoted variant tag (!"at start") is a tag, not string data. For the same
; node the later pattern wins, so this follows the capture above.
(tag (string_content) @constructor)

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
] @punctuation.delimiter

"=>" @punctuation.delimiter
