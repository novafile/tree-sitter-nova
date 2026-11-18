# tree-sitter-nova

[tree-sitter](https://tree-sitter.github.io/) grammar for **NOVA** — *Notation for Objects, Values & Agents*.

- Specification: [novafile/spec](https://github.com/novafile/spec) (`SPEC.md`)
- Reference implementation (`nvf`, `nvf-lsp`): [novafile/nova](https://github.com/novafile/nova)

The repository holds two grammars:

| Grammar | Files | Covers | Sources |
| --- | --- | --- | --- |
| `nova` | `.nova` | NOVA text: data, template, patch and query documents, query responses, text streams | `grammar.js`, `src/`, `queries/` |
| `nova_schema` | `.novs` | the schema language (spec §6.1) | `schema/grammar.js`, `schema/src/`, `schema/queries/` |

Rules both grammars need — flow values, which are also schema defaults —
live in `common/flow.js`. Each grammar has an external scanner
(`src/scanner.c`, `schema/src/scanner.c`) for the line structure; the one of
`nova` also handles block strings, plain scalars, the key-vs-value decision
and table row/cell counts, so the tree follows the reference parser rather
than a regex approximation.

## Supported syntax

### `nova`

| Spec | Construct | Nodes |
| --- | --- | --- |
| §4.11 | Text streams: documents separated by `---` lines; a file without separators is a stream of one document. An error stays in its document. | `stream`, `document`, `document_separator` |
| §4.1 | `#` comments at token boundaries (a `#` inside a word or plain scalar is text) | `comment` |
| §4.2 | Directives `@name argument` before the body | `directive`, `directive_name`, `argument` |
| §4.3 | Block records, `key:` + indented block, quoted keys | `record`, `entry`, `key` |
| §4.3 | Block lists, records starting on the dash line, `-` + nested block | `list`, `item` |
| §4.3 | Exact indentation: a block is two spaces deeper than its parent line; on a dash line, further fields are two spaces past the dash and the first field's own block (and table rows) four spaces past it, as in the reference parser. Tabs in indentation are errors. | `ERROR` with `indentation_error` |
| §4.4 | Plain scalars (rest of line, comment at ` #`) | `plain_scalar` |
| §4.5 | Numbers (int, float, rational), `true`/`false`/`null` — classified exactly as in the spec, so `1.2.3`, `08080` and `1/0` stay strings | `number`, `boolean`, `null` |
| §4.6 | Quoted strings with JSON escapes, `\u{…}`, `\$` | `string`, `string_content`, `escape_sequence` |
| §4.6 | Block strings `\|` / `\|-` (content at parent+2, parent+4 on a dash line) | `block_string`, `block_string_indicator` |
| §4.7 | Flow lists, records, tuples `()` `(a,)` `(a, b)`, words, multi-line flow values | `flow_list`, `flow_record`, `tuple`, `word` |
| §4.7 | `@set[…]`, `@map{k => v}` | `set`, `map`, `map_entry` |
| §4.7 | Variants `!tag`, `!tag(payload)`, and `key: !tag` + indented block payload | `variant`, `tag` |
| §4.8 | Typed literals `@timestamp("…")`, `@duration`, `@decimal`, `@rational`, `@bytes`, `@float` | `typed_literal`, `literal_type` |
| §4.9 | Tables `key[N]{a,b}:` and top-level `[N]{…}:`, empty cells; exactly `N` rows, each with one cell per column (cells may contain quoted commas and span lines inside brackets) | `table`, `table_header`, `row_count`, `column`, `row`, `cell`; `ERROR` with `table_row_error` |
| §4.12 | Positional profile (ordinary NOVA text) | — |
| §7.1 | Paths with fields, selectors (`[0]`, `[id=api]`, `["k"]`, `[*]`, filters), projections | `path`, `field`, `selector`, `wildcard`, `filter`, `projection` |
| §7.3 | Query documents (`@get …`) and responses | `directive` |
| §7.4 | Patch documents: `path: old -> new`, `= p: v`, `+ p: v`, `- p[: old]`, `? p[: v]` | `patch_op` |
| §9.2 | Template documents: the first directive (after an optional `@nova`) is `@template`. Only there are the template constructs below recognized; in a data document `"${x}"` is a plain string, `$x` a plain scalar and `@param`/`@let`/`@import` ordinary directives. | `document` |
| §9.2 | `@template`, `@param name[?]: type [= default]`, `@import "…" as x`, `@let x = v` / `@let x:` + block | `directive` with `parameter`/`type`/`default`, `source`/`alias`, `binding`/`value` fields |
| §9.3 | References `$a.b[0]`, interpolation `${…}` in quoted and block strings, `...` spreads (lines and flow elements) | `template_ref`, `interpolation`, `spread` |
| §9.3 | `@if` / `@else if` / `@else` / `@for [i,] x in …:` in records and lists | `if_block`, `else_block`, `for_block`, `condition`, `comparison`, `binary_condition`, `not_condition`, `parenthesized_condition` |

Queries: `queries/highlights.scm`, `folds.scm`, `indents.scm`, `locals.scm`
(template scopes) and an empty `injections.scm`.

### `nova_schema`

| Spec | Construct | Nodes |
| --- | --- | --- |
| §6.1 | `@schema name@MAJOR.MINOR.PATCH` (SemVer with pre-release/build), `@root Type`, `@nova MAJOR.MINOR` | `directive`, `directive_name`, `schema_ref`, `version` |
| §6.1 | `type Name = T attrs`, `enum Name = a \| b(T)` and `enum Name:` + variant lines, `record Name [@open]:` + field lines (exactly two spaces deeper; a record may be empty) | `type_definition`, `enum_definition`, `enum_variant`, `record_definition` |
| §6.1 | Fields `name[?]: type attrs* [= default]`; the default is a NOVA flow value | `field`, `optional_marker` |
| §6.1 | Types: built-ins, names, `list<T>`, `set<T>`, `map<K, V>`, tuples `(A, B)` `(A,)` `()`, `T[]`, `T?` (also combined: `int[]?`) | `primitive_type`, `type_identifier`, `list_type`, `set_type`, `map_type`, `tuple_type`, `array_type`, `nullable_type` |
| §6.1 | Attributes `@range(lo..hi)`, `@len(lo..hi[, unit])`, `@id(n)`, `@key`, `@open`, `@deprecated`, also on type arguments: `list<string @len(1..64)>` | `attribute`, `attribute_name`, `range`, `unit`, `attributed_type` |
| §6.1 | Comments; wrongly indented lines | `comment`; `ERROR` with `indentation_error` |

Queries: `schema/queries/highlights.scm`, `folds.scm`, `indents.scm` and
`locals.scm` (definitions and type references).

### Errors

Wrong indentation and table row/cell counts are not context-free, so the
scanners check them and report them as an `ERROR` node that contains an
`indentation_error` or `table_row_error` token over the line break and
indentation of the offending line (row). The line itself is still parsed,
so one bad line does not turn the rest of the document into errors:

```
a:
  b: 1
   c: 2     ->  (record (entry (key) (number)) (ERROR (indentation_error)) (entry (key) (number)) …)
  d: 3
```

A table with too few rows is reported at its last row, one with too many at
the first extra row, and a row with the wrong number of cells at that row.
The scanner state (indentation stack, table counters, pending errors) is
serialized after every external token, and `tree-sitter fuzz` checks that
incremental reparsing gives the same trees.

### Known gaps

The grammars are deliberately more lenient than the spec where an error
cannot be expressed in a tree-sitter grammar, or would make error recovery
worse. Validators (`nvf check`, `nvf-lsp`) report these; the tree does not:

- Duplicate keys (in block and flow records, and duplicate table columns)
  are not detected: that is not context-free, and `nvf-lsp` reports them.
  The same holds for the checks of a schema beyond its syntax: unknown type
  names, `@id` uniqueness, alias cycles, `@key` on optional fields, unknown
  `@len` units, definition names starting with an uppercase letter.
- Continuation lines of a multi-line flow value must be indented deeper
  than the line the value starts on (§4.7); this is not checked (inside
  brackets the scanner is not consulted), so a `---` line inside an
  unclosed bracket is not a separator either.
- `(x)` is a syntax error, but `a: b: c` style plain scalars and plain
  scalars ending in `:` are accepted.
- In a data document, `$x` after `key: ` is a plain scalar and `[$x]` a word
  (NOVA rejects a token starting with `$` outside templates); `...` spreads
  and `@if`/`@for` lines are errors there. A `@template` directive that is
  not the first one (after `@nova`) makes an ordinary data document with an
  unknown directive; unknown directives are not rejected.
- Error positions: a table that declares rows but ends right after its
  header at the end of the input (no line break), and an indented first
  line of a document that follows blank lines, are reported by a
  zero-width error, which may wrap more of the tree.
- Patch operations are recognized only after a `@patch` directive. `.patch`
  is not registered as a file type (it clashes with diffs); map your patch
  files to `nova` in the editor.
- Directive arguments other than `@param`/`@import`/`@let`/`@patch` are kept
  as a single `argument` token (paths in `@scope`/`@get` are not split).
- Schema files: block-form enum variants must be indented exactly two
  spaces, like fields (the reference parser also accepts deeper variant
  lines); an indented first line of a `.novs` file is not reported; a
  `#fingerprint` after `@schema name@version` reads as a comment (schemas
  declare a version, not a fingerprint).
- More than about 500 nested indentation levels exceed the scanner's
  serialization buffer.

## Editor setup

File types: `.nova` → `nova` (data, template, patch and query documents and
streams), `.novs` → `nova_schema`.

### Neovim (nvim-treesitter)

```lua
vim.filetype.add({ extension = { nova = "nova", novs = "novs" } })

local parsers = require("nvim-treesitter.parsers").get_parser_configs()
parsers.nova = {
  install_info = {
    url = "https://github.com/novafile/tree-sitter-nova",
    files = { "src/parser.c", "src/scanner.c" },
    branch = "main",
  },
  filetype = "nova",
}
parsers.nova_schema = {
  install_info = {
    url = "https://github.com/novafile/tree-sitter-nova",
    location = "schema",
    files = { "src/parser.c", "src/scanner.c" },
    branch = "main",
  },
  filetype = "novs",
}
```

Then `:TSInstall nova nova_schema`, and copy (or symlink) `queries/*.scm` to
`~/.config/nvim/queries/nova/` and `schema/queries/*.scm` to
`~/.config/nvim/queries/nova_schema/`. For the language server:

```lua
vim.lsp.config("novafile_lsp", { cmd = { "nvf-lsp" }, filetypes = { "nova", "novs" }, root_markers = { ".git" } })
vim.lsp.enable("novafile_lsp")
```

### Helix

`languages.toml`:

```toml
[language-server.nvf-lsp]
command = "nvf-lsp"

[[language]]
name = "nova"
scope = "source.nova"
file-types = ["nova"]
comment-token = "#"
indent = { tab-width = 2, unit = "  " }
language-servers = ["nvf-lsp"]
grammar = "nova"

[[grammar]]
name = "nova"
source = { git = "https://github.com/novafile/tree-sitter-nova", rev = "main" }

[[language]]
name = "nova-schema"
scope = "source.novs"
file-types = ["novs"]
comment-token = "#"
indent = { tab-width = 2, unit = "  " }
language-servers = ["nvf-lsp"]
grammar = "nova_schema"

[[grammar]]
name = "nova_schema"
source = { git = "https://github.com/novafile/tree-sitter-nova", rev = "main", subpath = "schema" }
```

Then `hx --grammar fetch && hx --grammar build`, and copy `queries/*.scm` to
`~/.config/helix/runtime/queries/nova/` and `schema/queries/*.scm` to
`~/.config/helix/runtime/queries/nova-schema/`. Install the server with
`cargo install --git https://github.com/novafile/nova novafile-lsp`.

### Zed

In a Zed extension's `extension.toml`:

```toml
[grammars.nova]
repository = "https://github.com/novafile/tree-sitter-nova"
rev = "<commit sha>"

[grammars.nova_schema]
repository = "https://github.com/novafile/tree-sitter-nova"
rev = "<commit sha>"
path = "schema"
```

with `languages/nova/config.toml`:

```toml
name = "NOVA"
grammar = "nova"
path_suffixes = ["nova"]
line_comments = ["# "]
tab_size = 2
```

and `languages/nova-schema/config.toml`:

```toml
name = "NOVA Schema"
grammar = "nova_schema"
path_suffixes = ["novs"]
line_comments = ["# "]
tab_size = 2
```

and the query files from `queries/` in `languages/nova/` and from
`schema/queries/` in `languages/nova-schema/`.

## Development

```sh
npm install                          # installs tree-sitter-cli
npx tree-sitter generate             # regenerate src/ after editing grammar.js or common/
(cd schema && npx tree-sitter generate)   # regenerate schema/src/
npx tree-sitter test                 # nova corpus (test/corpus) and all highlight tests (test/highlight)
npx tree-sitter test -p schema       # nova_schema corpus (schema/test/corpus)
npx tree-sitter parse file.nova file.novs
npx tree-sitter fuzz                 # incremental reparsing (also: cd schema && npx tree-sitter fuzz)
cargo test                           # Rust binding (npm test, go test ./bindings/go/, … for the others)
```

`scripts/gen_corpus.py` rewrites both corpora from the examples and inline
cases with empty expectations; `tree-sitter test -u` (and `-p schema -u`)
fills them in, except for the tests whose tree has errors, which
`scripts/fill_error_expectations.py [-p schema]` fills from the test
runner's output. Review the diff before committing.

Bindings for C, Go, Node, Python, Rust and Swift were generated by
`tree-sitter init` and extended to expose both languages:

| Binding | NOVA text | Schema language |
| --- | --- | --- |
| C | `tree_sitter_nova()` (`tree-sitter-nova.h`) | `tree_sitter_nova_schema()` (`tree-sitter-nova-schema.h`), same library |
| Rust | `LANGUAGE` / `LANGUAGE_NOVA` | `LANGUAGE_NOVA_SCHEMA`, `NOVA_SCHEMA_NODE_TYPES`, `NOVA_SCHEMA_HIGHLIGHTS_QUERY` |
| Node | default export | `schema` (named export, and a property of the default export) |
| Python | `language()` | `language_nova_schema()`, `NOVA_SCHEMA_HIGHLIGHTS_QUERY` |
| Go | `Language()` | `LanguageNovaSchema()` |
| Swift | `tree_sitter_nova()` | `tree_sitter_nova_schema()` |

The generated parsers in `src/` and `schema/src/` are committed; CI checks
that they are up to date.

## License

The grammar, queries, scanner, generated parser and bindings are licensed
under the [Apache License 2.0](LICENSE).

The corpus and highlight tests under `test/` and `schema/test/` — including
the example NOVA documents and schemas copied from the specification and
reference implementation repositories — are dedicated to the public domain
under [CC0-1.0](https://creativecommons.org/publicdomain/zero/1.0/)
(see `REUSE.toml`). The NOVA specification itself is licensed under
Apache-2.0.
