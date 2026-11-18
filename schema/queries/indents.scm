; Fields and block-form enum variants are indented exactly two spaces
; (spec §6.1).

(record_definition
  ":" @indent.begin)

(enum_definition
  ":" @indent.begin)

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

(comment) @indent.auto
