from unittest import TestCase

from tree_sitter import Language, Parser
import tree_sitter_nova


class TestLanguage(TestCase):
    def test_can_load_grammar(self):
        try:
            Parser(Language(tree_sitter_nova.language()))
        except Exception:
            self.fail("Error loading NOVA grammar")

    def test_can_load_schema_grammar(self):
        parser = Parser(Language(tree_sitter_nova.language_nova_schema()))
        tree = parser.parse(b"record R:\n  a: list<int> = [1]\n")
        self.assertEqual(tree.root_node.type, "schema")
        self.assertFalse(tree.root_node.has_error)
        self.assertIn("@type", tree_sitter_nova.NOVA_SCHEMA_HIGHLIGHTS_QUERY)

    def test_can_load_lines_grammar(self):
        parser = Parser(Language(tree_sitter_nova.language_nova_lines()))
        tree = parser.parse(b"@columns {a, b{c}, ..}\n1, (2,), {d: 3}\n")
        self.assertEqual(tree.root_node.type, "stream")
        self.assertFalse(tree.root_node.has_error)
        self.assertIn("@keyword.directive", tree_sitter_nova.NOVA_LINES_HIGHLIGHTS_QUERY)

    def test_reports_wrong_indentation(self):
        parser = Parser(Language(tree_sitter_nova.language()))
        self.assertTrue(parser.parse(b"a:\n   b: 1\n").root_node.has_error)
