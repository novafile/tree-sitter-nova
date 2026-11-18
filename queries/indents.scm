; Children of a line are indented exactly two spaces (spec §4.3).

; A line ending in `key:`, `- `, `@if …:`, `@for …:`, a table header or
; a block-string indicator opens an indented block.
(entry
  ":" @indent.begin)

(item) @indent.begin

(table) @indent.begin

(block_string) @indent.begin

(if_block) @indent.begin

(else_block) @indent.begin

(for_block) @indent.begin

(directive
  binding: (identifier)) @indent.begin

; Multi-line flow values.
[
  (flow_list)
  (flow_record)
  (tuple)
  (set)
  (map)
] @indent.begin

[
  "]"
  "}"
  ")"
] @indent.branch @indent.end

"@else" @indent.branch

(comment) @indent.auto

(block_string
  (string_content) @indent.ignore)
