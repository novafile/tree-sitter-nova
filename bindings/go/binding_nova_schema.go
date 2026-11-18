package tree_sitter_nova

// The NOVA schema language (.novs) is compiled in its own cgo preamble, so
// that its generated parser does not clash with the NOVA text parser.

// #cgo CFLAGS: -std=c11 -fPIC
// #include "../../schema/src/parser.c"
// #include "../../schema/src/scanner.c"
import "C"

import "unsafe"

// Get the tree-sitter Language for the NOVA schema language (.novs).
func LanguageNovaSchema() unsafe.Pointer {
	return unsafe.Pointer(C.tree_sitter_nova_schema())
}
