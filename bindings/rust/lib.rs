//! This crate provides NOVA language support for the [tree-sitter] parsing library.
//!
//! Typically, you will use the [`LANGUAGE`] constant to add this language to a
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
}

/// The tree-sitter [`LanguageFn`] for this grammar.
pub const LANGUAGE: LanguageFn = unsafe { LanguageFn::from_raw(tree_sitter_nova) };

/// The content of the [`node-types.json`] file for this grammar.
///
/// [`node-types.json`]: https://tree-sitter.github.io/tree-sitter/using-parsers/6-static-node-types
pub const NODE_TYPES: &str = include_str!("../../src/node-types.json");

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
