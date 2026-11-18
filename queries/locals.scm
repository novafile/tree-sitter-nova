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

(interpolation
  path: (path
    .
    (field) @local.reference))
