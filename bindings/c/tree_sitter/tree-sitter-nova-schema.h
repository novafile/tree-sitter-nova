#ifndef TREE_SITTER_NOVA_SCHEMA_H_
#define TREE_SITTER_NOVA_SCHEMA_H_

typedef struct TSLanguage TSLanguage;

#ifdef __cplusplus
extern "C" {
#endif

// The NOVA schema language (.novs). Part of the tree-sitter-nova library.
const TSLanguage *tree_sitter_nova_schema(void);

#ifdef __cplusplus
}
#endif

#endif // TREE_SITTER_NOVA_SCHEMA_H_
