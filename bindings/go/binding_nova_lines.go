package tree_sitter_nova

// NOVA Lines (.novl) is compiled in its own cgo preamble, so that its
// generated parser does not clash with the other parsers. It has no
// external scanner.

// #cgo CFLAGS: -std=c11 -fPIC
// #include "../../lines/src/parser.c"
import "C"

import "unsafe"

// Get the tree-sitter Language for NOVA Lines (.novl).
func LanguageNovaLines() unsafe.Pointer {
	return unsafe.Pointer(C.tree_sitter_nova_lines())
}
