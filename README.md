# tree-sitter-nova

[tree-sitter](https://tree-sitter.github.io/) grammar for **NOVA** — *Notation for Objects, Values & Agents*.

- Specification: [novafile/spec](https://github.com/novafile/spec) (`SPEC.md`)
- Reference implementation (`nvf`, `nvf-lsp`): [novafile/nova](https://github.com/novafile/nova)

One grammar (`nova`) covers data documents, template documents, patch
documents and query documents/responses. An external scanner
(`src/scanner.c`) handles indentation, block strings, plain scalars and the
key-vs-value decision, so the tree follows the reference parser rather than
a regex approximation.

## Supported syntax

| Spec | Construct | Nodes |
| --- | --- | --- |
| §4.1 | `#` comments at token boundaries (a `#` inside a word or plain scalar is text) | `comment` |
| §4.2 | Directives `@name argument` before the body | `directive`, `directive_name`, `argument` |
| §4.3 | Block records, `key:` + indented block, quoted keys | `record`, `entry`, `key` |
| §4.3 | Block lists, records starting on the dash line, `-` + nested block | `list`, `item` |
| §4.4 | Plain scalars (rest of line, comment at ` #`) | `plain_scalar` |
| §4.5 | Numbers (int, float, rational), `true`/`false`/`null` — classified exactly as in the spec, so `1.2.3`, `08080` and `1/0` stay strings | `number`, `boolean`, `null` |
| §4.6 | Quoted strings with JSON escapes, `\u{…}`, `\$` | `string`, `string_content`, `escape_sequence` |
| §4.6 | Block strings `\|` / `\|-` (content at parent+2, parent+4 on a dash line) | `block_string`, `block_string_indicator` |
| §4.7 | Flow lists, records, tuples `()` `(a,)` `(a, b)`, words, multi-line flow values | `flow_list`, `flow_record`, `tuple`, `word` |
| §4.7 | `@set[…]`, `@map{k => v}` | `set`, `map`, `map_entry` |
| §4.7 | Variants `!tag`, `!tag(payload)`, and `key: !tag` + indented block payload | `variant`, `tag` |
| §4.8 | Typed literals `@timestamp("…")`, `@duration`, `@decimal`, `@rational`, `@bytes`, `@float` | `typed_literal`, `literal_type` |
| §4.9 | Tables `key[N]{a,b}:` and top-level `[N]{…}:`, empty cells | `table`, `table_header`, `row_count`, `column`, `row`, `cell` |
| §4.12 | Positional profile (ordinary NOVA text) | — |
| §7.1 | Paths with fields, selectors (`[0]`, `[id=api]`, `["k"]`, `[*]`, filters), projections | `path`, `field`, `selector`, `wildcard`, `filter`, `projection` |
| §7.3 | Query documents (`@get …`) and responses | `directive` |
| §7.4 | Patch documents: `path: old -> new`, `= p: v`, `+ p: v`, `- p[: old]`, `? p[: v]` | `patch_op` |
| §9.2 | `@template`, `@param name[?]: type [= default]`, `@import "…" as x`, `@let x = v` / `@let x:` + block | `directive` with `parameter`/`type`/`default`, `source`/`alias`, `binding`/`value` fields |
| §9.3 | References `$a.b[0]`, interpolation `${…}` in quoted and block strings, `...` spreads (lines and flow elements) | `template_ref`, `interpolation`, `spread` |
| §9.3 | `@if` / `@else if` / `@else` / `@for [i,] x in …:` in records and lists | `if_block`, `else_block`, `for_block`, `condition`, `comparison`, `binary_condition`, `not_condition`, `parenthesized_condition` |

Queries: `queries/highlights.scm`, `folds.scm`, `indents.scm`, `locals.scm`
(template scopes) and an empty `injections.scm`.

### Known gaps

The grammar is deliberately more lenient than the spec where an error cannot
be expressed in a context-free way. Validators (`nvf check`, `nvf-lsp`)
report these; the tree does not:

- Indentation must be *deeper*, not exactly two spaces deeper (block strings
  are the exception: their content indentation is exact).
- Table row counts and cell counts are not checked; duplicate keys are not
  detected; `(x)` is a syntax error but `a: b: c` style plain scalars and
  plain scalars ending in `:` are accepted.
- Template constructs (`$ref`, `${…}`, `...`, `@if`/`@for`) are recognized in
  every document, not only after `@template`; in a data document,
  `"${x}"` therefore shows an interpolation where NOVA reads literal text.
- Patch operations are recognized only after a `@patch` directive. `.patch`
  is not registered as a file type (it clashes with diffs); map your patch
  files to `nova` in the editor.
- Text streams (documents separated by `---`, §4.11) and schema files
  (`.novs`, §6.1) are not covered.
- Directive arguments other than `@param`/`@import`/`@let`/`@patch` are kept
  as a single `argument` token (paths in `@scope`/`@get` are not split).

## Editor setup

### Neovim (nvim-treesitter)

```lua
vim.filetype.add({ extension = { nova = "nova" } })

local parsers = require("nvim-treesitter.parsers").get_parser_configs()
parsers.nova = {
  install_info = {
    url = "https://github.com/novafile/tree-sitter-nova",
    files = { "src/parser.c", "src/scanner.c" },
    branch = "main",
  },
  filetype = "nova",
}
```

Then `:TSInstall nova` and copy (or symlink) `queries/*.scm` to
`~/.config/nvim/queries/nova/`. For the language server:

```lua
vim.lsp.config("novafile_lsp", { cmd = { "nvf-lsp" }, filetypes = { "nova" }, root_markers = { ".git" } })
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
```

Then `hx --grammar fetch && hx --grammar build` and copy `queries/*.scm` to
`~/.config/helix/runtime/queries/nova/`. Install the server with
`cargo install --git https://github.com/novafile/nova novafile-lsp`.

### Zed

In a Zed extension's `extension.toml`:

```toml
[grammars.nova]
repository = "https://github.com/novafile/tree-sitter-nova"
rev = "<commit sha>"
```

with `languages/nova/config.toml`:

```toml
name = "NOVA"
grammar = "nova"
path_suffixes = ["nova"]
line_comments = ["# "]
tab_size = 2
```

and the query files from `queries/` in `languages/nova/`.

## Development

```sh
npm install                 # installs tree-sitter-cli
npx tree-sitter generate    # regenerate src/ after editing grammar.js
npx tree-sitter test        # corpus (test/corpus) and highlight (test/highlight) tests
npx tree-sitter parse file.nova
cargo test                  # Rust binding
```

Bindings for C, Go, Node, Python, Rust and Swift are generated by
`tree-sitter init`. The generated parser in `src/` is committed; CI checks
that it is up to date.

## License

The grammar, queries, scanner, generated parser and bindings are licensed
under the [Apache License 2.0](LICENSE).

The corpus and highlight tests under `test/` — including the example NOVA
documents copied from the specification repository — are dedicated to the
public domain under [CC0-1.0](https://creativecommons.org/publicdomain/zero/1.0/)
(see `REUSE.toml`). The NOVA specification itself is licensed under
Apache-2.0.
