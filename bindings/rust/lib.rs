//! This crate provides NOVA language support for the [tree-sitter] parsing library.
//!
//! It contains two grammars: [`LANGUAGE`] (also [`LANGUAGE_NOVA`]) for NOVA
//! text — data, template, patch and query documents and text streams — and
//! [`LANGUAGE_NOVA_SCHEMA`] for the schema language (`.novs` files).
//!
//! Typically, you will use one of these constants to add the language to a
//! tree-sitter [`Parser`], and then use the parser to parse some code:
//!
//! ```
//! let code = r#"
//! environment: production
//! services:
//!   - id: auth
//!     port: 8080
//! "#;
//! let mut parser = tree_sitter::Parser::new();
//! let language = tree_sitter_nova::LANGUAGE;
//! parser
//!     .set_language(&language.into())
//!     .expect("Error loading NOVA parser");
//! let tree = parser.parse(code, None).unwrap();
//! assert!(!tree.root_node().has_error());
//! ```
//!
//! [`Parser`]: https://docs.rs/tree-sitter/0.27.2/tree_sitter/struct.Parser.html
//! [tree-sitter]: https://tree-sitter.github.io/

use tree_sitter_language::LanguageFn;

unsafe extern "C" {
    fn tree_sitter_nova() -> *const ();
    fn tree_sitter_nova_schema() -> *const ();
}

/// The tree-sitter [`LanguageFn`] for the NOVA text grammar (`.nova`).
pub const LANGUAGE: LanguageFn = unsafe { LanguageFn::from_raw(tree_sitter_nova) };

/// The tree-sitter [`LanguageFn`] for the NOVA text grammar (`.nova`); the
/// same as [`LANGUAGE`].
pub const LANGUAGE_NOVA: LanguageFn = LANGUAGE;

/// The tree-sitter [`LanguageFn`] for the NOVA schema language (`.novs`).
pub const LANGUAGE_NOVA_SCHEMA: LanguageFn = unsafe { LanguageFn::from_raw(tree_sitter_nova_schema) };

/// The content of the [`node-types.json`] file for the NOVA text grammar.
///
/// [`node-types.json`]: https://tree-sitter.github.io/tree-sitter/using-parsers/6-static-node-types
pub const NODE_TYPES: &str = include_str!("../../src/node-types.json");

/// The content of the [`node-types.json`] file for the schema grammar.
///
/// [`node-types.json`]: https://tree-sitter.github.io/tree-sitter/using-parsers/6-static-node-types
pub const NOVA_SCHEMA_NODE_TYPES: &str = include_str!("../../schema/src/node-types.json");

/// The syntax highlighting query for the schema grammar.
pub const NOVA_SCHEMA_HIGHLIGHTS_QUERY: &str = include_str!("../../schema/queries/highlights.scm");

/// The local variable query for the schema grammar.
pub const NOVA_SCHEMA_LOCALS_QUERY: &str = include_str!("../../schema/queries/locals.scm");

#[cfg(with_highlights_query)]
/// The syntax highlighting query for this grammar.
pub const HIGHLIGHTS_QUERY: &str = include_str!("../../queries/highlights.scm");

#[cfg(with_injections_query)]
/// The language injection query for this grammar.
pub const INJECTIONS_QUERY: &str = include_str!("../../queries/injections.scm");

#[cfg(with_locals_query)]
/// The local variable query for this grammar.
pub const LOCALS_QUERY: &str = include_str!("../../queries/locals.scm");

#[cfg(with_tags_query)]
/// The symbol tagging query for this grammar.
pub const TAGS_QUERY: &str = include_str!("../../queries/tags.scm");

#[cfg(test)]
mod tests {
    #[test]
    fn test_can_load_grammar() {
        let mut parser = tree_sitter::Parser::new();
        parser
            .set_language(&super::LANGUAGE.into())
            .expect("Error loading NOVA parser");
    }

    #[test]
    fn test_parses_samples_without_errors() {
        let mut parser = tree_sitter::Parser::new();
        parser.set_language(&super::LANGUAGE.into()).unwrap();
        for (name, src) in [
            ("data", include_str!("../../test/highlight/data.nova")),
            ("template", include_str!("../../test/highlight/template.nova")),
            ("patch", include_str!("../../test/highlight/patch.nova")),
        ] {
            let tree = parser.parse(src, None).unwrap();
            assert!(!tree.root_node().has_error(), "{name}: {}", tree.root_node().to_sexp());
        }
    }

    #[test]
    fn test_can_load_schema_grammar() {
        let mut parser = tree_sitter::Parser::new();
        parser
            .set_language(&super::LANGUAGE_NOVA_SCHEMA.into())
            .expect("Error loading NOVA schema parser");
        let tree = parser
            .parse(include_str!("../../test/highlight/schema.novs"), None)
            .unwrap();
        assert!(!tree.root_node().has_error(), "{}", tree.root_node().to_sexp());
        assert_eq!(tree.root_node().kind(), "schema");
    }

    #[test]
    fn test_reports_wrong_indentation() {
        let mut parser = tree_sitter::Parser::new();
        parser.set_language(&super::LANGUAGE.into()).unwrap();
        let tree = parser.parse("a:\n   b: 1\nc: 2\n", None).unwrap();
        assert!(tree.root_node().has_error());
    }

    #[test]
    fn test_schema_queries_compile() {
        let language = super::LANGUAGE_NOVA_SCHEMA.into();
        for (name, src) in [
            ("highlights", super::NOVA_SCHEMA_HIGHLIGHTS_QUERY),
            ("folds", include_str!("../../schema/queries/folds.scm")),
            ("indents", include_str!("../../schema/queries/indents.scm")),
            ("locals", super::NOVA_SCHEMA_LOCALS_QUERY),
        ] {
            tree_sitter::Query::new(&language, src).unwrap_or_else(|e| panic!("{name}: {e}"));
        }
    }

    #[test]
    fn test_queries_compile() {
        let language = super::LANGUAGE.into();
        for (name, src) in [
            ("highlights", include_str!("../../queries/highlights.scm")),
            ("folds", include_str!("../../queries/folds.scm")),
            ("indents", include_str!("../../queries/indents.scm")),
            ("locals", include_str!("../../queries/locals.scm")),
        ] {
            tree_sitter::Query::new(&language, src).unwrap_or_else(|e| panic!("{name}: {e}"));
        }
    }
}
