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

func TestCanLoadSchemaGrammar(t *testing.T) {
	language := tree_sitter.NewLanguage(tree_sitter_nova.LanguageNovaSchema())
	if language == nil {
		t.Fatalf("Error loading NOVA schema grammar")
	}
	parser := tree_sitter.NewParser()
	defer parser.Close()
	if err := parser.SetLanguage(language); err != nil {
		t.Fatal(err)
	}
	tree := parser.Parse([]byte("record R:\n  a: list<int> = [1]\n"), nil)
	defer tree.Close()
	root := tree.RootNode()
	if root.Kind() != "schema" || root.HasError() {
		t.Errorf("unexpected tree: %s", root.ToSexp())
	}
}

func TestCanLoadLinesGrammar(t *testing.T) {
	language := tree_sitter.NewLanguage(tree_sitter_nova.LanguageNovaLines())
	if language == nil {
		t.Fatalf("Error loading NOVA Lines grammar")
	}
	parser := tree_sitter.NewParser()
	defer parser.Close()
	if err := parser.SetLanguage(language); err != nil {
		t.Fatal(err)
	}
	tree := parser.Parse([]byte("@columns {a, b{c}, ..}\n1, (2,), {d: 3}\n"), nil)
	defer tree.Close()
	root := tree.RootNode()
	if root.Kind() != "stream" || root.HasError() {
		t.Errorf("unexpected tree: %s", root.ToSexp())
	}
}
