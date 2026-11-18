; Definitions are schema-wide; named types used in fields, aliases,
; variant payloads, type arguments and `@root` refer to them.

(schema) @local.scope

(type_definition
  name: (type_identifier) @local.definition.type)

(enum_definition
  name: (type_identifier) @local.definition.type)

(record_definition
  name: (type_identifier) @local.definition.type)

(field
  name: (identifier) @local.definition.field)

(directive
  type: (type_identifier) @local.reference)

(type_definition
  type: (type_identifier) @local.reference)

(field
  type: (type_identifier) @local.reference)

(enum_variant
  payload: (type_identifier) @local.reference)

(attributed_type
  type: (type_identifier) @local.reference)

(nullable_type
  type: (type_identifier) @local.reference)

(array_type
  element: (type_identifier) @local.reference)

(list_type
  element: (type_identifier) @local.reference)

(set_type
  element: (type_identifier) @local.reference)

(tuple_type
  element: (type_identifier) @local.reference)

(map_type
  key: (type_identifier) @local.reference)

(map_type
  value: (type_identifier) @local.reference)
