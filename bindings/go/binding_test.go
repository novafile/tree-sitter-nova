package tree_sitter_nova_test

import (
	"testing"

	tree_sitter "github.com/tree-sitter/go-tree-sitter"
	tree_sitter_nova "github.com/novafile/tree-sitter-nova/bindings/go"
)

func TestCanLoadGrammar(t *testing.T) {
	language := tree_sitter.NewLanguage(tree_sitter_nova.Language())
	if language == nil {
		t.Errorf("Error loading NOVA grammar")
	}
}
