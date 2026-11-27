; Template scopes (spec §9): parameters, imports and @let bindings are
; document-wide; @for variables are scoped to the loop body.

(document) @local.scope

(for_block) @local.scope

(directive
  parameter: (identifier) @local.definition.parameter)

(directive
  binding: (identifier) @local.definition.var)

(directive
  alias: (identifier) @local.definition.import)

(for_block
  index: (identifier) @local.definition.var)

(for_block
  variable: (identifier) @local.definition.var)

(template_ref
  path: (path
    .
    (field) @local.reference))

; The head of an interpolation implies `$` (proposal 0006): `${name}`,
; `${name.port + 1}`.
(interpolation
  (path
    .
    (field) @local.reference))

(interpolation
  (binary_expression
    left: (path
      .
      (field) @local.reference)))

; Template language 0.2: @def functions and their parameters, and
; comprehension variables, which are scoped to the comprehension.
(directive
  function: (function) @local.definition.function)

(def_parameter) @local.scope

(def_parameter
  name: (identifier) @local.definition.parameter)

(list_comprehension) @local.scope

(record_comprehension) @local.scope

(list_comprehension
  variable: (identifier) @local.definition.var)

(list_comprehension
  index: (identifier) @local.definition.var)

(record_comprehension
  variable: (identifier) @local.definition.var)

(record_comprehension
  index: (identifier) @local.definition.var)

(call
  function: (function) @local.reference)
