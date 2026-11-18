"""Write test/corpus/*.txt and schema/test/corpus/*.txt with empty expectations.

`tree-sitter test -u` and `tree-sitter test -p schema -u` fill them in.
"""
# SPDX-License-Identifier: Apache-2.0
# Regenerates the corpora from the NOVA examples; then run the two
# `tree-sitter test -u` commands above.
import os

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NOVA = os.environ.get('NOVA_REPO', os.path.join(REPO, '..', 'nova'))  # novafile/nova checkout
SPEC = os.environ.get('SPEC_REPO', os.path.join(REPO, '..', 'spec'))  # novafile/spec checkout
S = os.path.dirname(os.path.abspath(__file__))


def read(p):
    with open(p) as f:
        return f.read()


C = {}

C['scalars'] = [
    ('Plain scalars', 'name: hello world\nurl: http://example.com/a#frag\nunicode: grüße 🚀\ndash: -foo\n'),
    ('Plain scalar with trailing comment', 'a: hello   # a comment\nb: x#y\n'),
    ('Integers', 'a: 0\nb: -0\nc: 42\nd: -7\ne: 170141183460469231731687303715884105727\n'),
    ('Floats', 'a: 1.5\nb: -0.0\nc: 1e300\nd: 2.5E-3\ne: 1.0\n'),
    ('Rationals', 'a: 22/7\nb: -22/7\nc: 2/4\n'),
    ('Number-like strings', 'a: 1/0\nb: 1/02\nc: 1.2.3\nd: 08080\ne: 2026-11-17T16:00:00Z\nf: 11/17/2026\ng: 1e\nh: 5 apples\n'),
    ('Keywords', 'a: true\nb: false\nc: null\nd: trueish\ne: NO\nf: yes\n'),
]

C['strings'] = [
    ('Quoted strings', 'a: "hello"\nb: ""\nc: " padded "\n"odd key!": 1\n'),
    ('Escape sequences', 'a: "q\\"b\\\\s\\/n\\n\\t\\b\\f\\r"\nb: "\\u00e9 \\u{1F600} \\$HOME"\n'),
    ('Hash inside quoted string', 'a: "x #y"  # real comment\nb: "#start"\n'),
    ('Dollar inside quoted string', 'a: "$HOME"\nb: "cost $5"\nc: "$$"\n'),
]

C['block_strings'] = [
    ('Keep block string', 'script: |\n  echo "hello"\n    indented line\n  # not a comment\nnext: 1\n'),
    ('Strip block string', 'notes: |-\n  first\n  second\n'),
    ('Block string with comment after indicator', 'a: |   # keep: one final newline\n  text\n'),
    ('Block string with blank lines', 'a: |\n  one\n\n  two\n\n\nb: 2\n'),
    ('Empty block string', 'a: |\nb: 1\n'),
    ('Block string at end of input', 'a: |\n  last line'),
    ('Nested block string', 'outer:\n  inner: |\n    deep\n      deeper\n  sibling: x\n'),
    ('Block string in list item', 'items:\n  - |\n    block in list\n  - after\n'),
    ('Block string on dash line', 'items:\n  - id: a\n    script: |\n      run it\n      twice\n  - script: |\n      dash line\n    other: 1\n'),
    ('Block string ended by less-indented dash-line field', '- script: |\n  other: 1\n'),
]

C['flow'] = [
    ('Flow lists', 'a: [1, two, "three"]\nb: []\nc: [1, 2,]\nd: [[1], []]\n'),
    ('Flow records', 'a: {id: auth, "odd key": 1}\nb: {}\nc: {a:1}\nd: {a: [1, 2,], "b c": !ok(1)}\n'),
    ('Tuples', 'a: (1, 2)\nb: (1,)\nc: ()\nd: (1, 2,)\n'),
    ('Sets and maps', 'a: @set[a, b]\nb: @set[]\nc: @map{1 => one, (1, 2) => pair}\nd: @map{}\n'),
    ('Flow words', 'a: [ghcr.io/example/auth:v2, a:b, x#y, .5, -1x, ..]\n'),
    ('Multi-line flow value', 'a: [1,\n  2,  # two\n  3\n]\nb: {x: 1,\n    y: 2}\n'),
    ('Flow value as document body', '[1, {a: b}, (c,)]\n'),
    ('Scalar as document body', '42\n'),
    ('Quoted string as document body', '"hello"\n'),
]

C['typed_literals'] = [
    ('Typed literals', 'ts: @timestamp("2026-11-17T16:00:00Z")\ndur: @duration("1h30m")\niso: @duration("PT1H30M")\ndec: @decimal("12.50")\nrat: @rational("22/7")\nbytes: @bytes("AP8H")\nnan: @float("nan")\ninf: @float("-inf")\n'),
    ('Typed literal as document body', '@timestamp("2026-11-17T16:00:00Z")\n'),
    ('Typed literals in flow', 'a: [@decimal("1"), @float("inf")]\n'),
]

C['variants'] = [
    ('Unit and payload variants', 'a: !active\nb: !failed("disk full")\nc: !named(x)\nd: !_\ne: [!a, !b({c: 1})]\n'),
    ('Block payload variant', 'shape: !circle\n  radius: 2\n  center: (0, 0)\nnext: 1\n'),
    ('Block payload variant in list', '- !tag\n- !tag\n  a: 1\n- !list\n  - x\n'),
]

C['records'] = [
    ('Simple record', 'a: 1\nb: two\n'),
    ('Nested records', 'a:\n  b:\n    c: 1\n  d: 2\ne: 3\n'),
    ('Quoted keys', '"a b": 1\n"c": {"d e": 2}\n'),
    ('Keys with dashes and underscores', 'my-key: 1\n_private: 2\nk9: 3\n'),
    ('Comment after key before block', 'a:   # comment\n  b: 1\n'),
    ('No trailing newline', 'a: 1'),
    ('CRLF line endings', 'a: 1\r\nb:\r\n  c: x y\r\n'),
    ('Empty document', ''),
    ('Comment-only document', '# just a comment\n\n# another\n'),
]

C['lists'] = [
    ('Block list', '- a\n- 2\n- "three"\n- [4]\n'),
    ('Records in a list', 'services:\n  - id: auth\n    port: 8080\n\n  - id: api\n    port: 3000\n'),
    ('Nested block list', '-\n  - nested\n  - items\n- x\n'),
    ('Dash-line entry with nested block', '- key:\n    a: 1\n  b: 2\n- c: 3\n'),
    ('List in a record in a list', '- a:\n    - 1\n    - 2\n  b: x\n'),
]

C['tables'] = [
    ('Table', 'services[2]{id,image,port,replicas,enabled}:\n  auth, ghcr.io/example/auth:v2, 8080, 3, true\n  api,  ghcr.io/example/api:v5,  3000, 5, true\n'),
    ('Table with empty cells', 't[3]{a,b}:\n  1,\n  ,null\n  ,\n'),
    ('Table with quoted columns and cells', '"a table"[1]{"col a",b}:\n  "x, y", [1, 2]\n'),
    ('Top-level table', '[2]{id,replicas}:\n  auth,3\n  api,5\n'),
    ('Table with comments', 'table[2]{a,b}:  # header\n  1, x   # first row\n  # between\n  2, y\n'),
    ('Flow list is not a table', '[2]\n'),
    ('Table without rows', 't[0]{a,b}:\nx: 1\n'),
    ('Table cells with commas and line breaks', 't[2]{a,b}:\n  [1, 2], {a: 1, b: "x,y"}\n  "x, y", (1,\n    2)  # c\nx: 1\n'),
    ('Table in a list item', '- t[2]{a,b}:\n    1, 2\n    3, 4\n  after: x\n- u[0]{a}:\n  after: y\n'),
    ('Table at end of input', 't[2]{a,b}:\n  1, 2\n  3, 4'),
    # Row and cell counts (§4.9): each mismatch is one ERROR at the row.
    ('Error: too few rows', 't[3]{a,b}:\n  1, 2\n  3, 4\nx: 1\n'),
    ('Error: too few rows at end of input', 't[2]{a,b}:\n  1, 2'),
    ('Error: too few rows in a top-level table', '[2]{a,b}:\n  1, 2\n'),
    ('Error: too few rows in a list item', '- t[2]{a}:\n    1\n  y: 2\n'),
    ('Error: no rows', 't[2]{a,b}:\nx: 1\n'),
    ('Error: too many rows', 't[1]{a,b}:\n  1, 2\n  3, 4\nx: 1\n'),
    ('Error: rows after [0]', 't[0]{a,b}:\n  1, 2\nx: 1\n'),
    ('Error: too many cells', 't[2]{a,b}:\n  1, 2, 3\n  3, 4\nx: 1\n'),
    ('Error: too few cells', 't[2]{a,b}:\n  1, 2\n  3\nx: 1\n'),
    ('Error: row indented wrongly', 't[2]{a,b}:\n  1, 2\n   3, 4\nx: 1\n'),
]

C['indentation'] = [
    ('Dash-line nesting', '- a: 1\n  b:\n    c: 2\n  d:\n    - 3\n- key:\n    x: 1\n  y: 2\n- s: |\n    text\n  t[1]{a}:\n    1\n  v: !tag\n    p: 1\n- !tag\n  q: 1\n-\n  - nested\n'),
    ('Comment lines do not count', 'a:\n      # deep comment\n  b: 1\n # shallow comment\n  c: 2\n'),
    # Wrong indentation (§4.3): an ERROR at the line, the line still parses.
    ('Error: block indented three spaces', 'a:\n   b: 1\nc: 2\n'),
    ('Error: block indented four spaces', 'a:\n    b: 1\nc: 2\n'),
    ('Error: line indented one space too far', 'a:\n  b: 1\n   c: 2\n  d: 3\n'),
    ('Error: dedent between levels', 'a:\n  b:\n    c: 1\n   d: 2\ne: 3\n'),
    ('Error: dash-line record field misaligned', '- a: 1\n   b: 2\n- c: 3\n'),
    ('Error: dash-line field block at two spaces', '- key:\n  a: 1\n- c: 3\n'),
    ('Error: list item indented too far', 'items:\n  - x\n   - y\n  - z\n'),
    ('Error: variant payload indented wrongly', 'a: !tag\n   x: 1\nb: 2\n'),
    ('Error: tab in indentation', 'a:\n\tb: 1\nc: 2\n'),
    ('Error: indented first line', '  a: 1\nb: 2\n'),
    ('Error: one bad line in a larger document', read(f'{S}/indentation.nova')),
]

C['streams'] = [
    ('Stream of documents', 'a: 1\n---\n- x\n---\n@template\nx: $y\n---\n[1, 2]\n'),
    ('Leading, trailing and empty documents', '---\na: 1\n---\n---\n# comment only\n---\n'),
    ('Separator after nested blocks', 'a:\n  b: |\n    text\n  t[1]{c}:\n    1\n---\nd: 2\n'),
    ('Dashes that are not separators', 'a: ---\nb: "---"\nc: |\n  ---\nd: [---]\n'),
    ('Error: separator with trailing text', 'a: 1\n--- x\nb: 2\n'),
    ('Error: an error stays in its document', 'a: [1,\nb: 2\n---\nc: 3\n---\nd: "open\n---\ne: 4\n'),
]

C['comments'] = [
    ('Comments everywhere', '# header\n@schema s@1.0.0  # trailing\n\n# before body\na: 1 # after value\nb:\n  # inside block\n  c: 2\n  # at end of inner block\n# back at top\nd: [1, # in flow\n  2]\n# trailing\n'),
    ('Deep comment before dedent', 'a:\n  b:\n    c: 1\n    # deep\n  # mid\n# top\nd: 2\n'),
]

C['directives'] = [
    ('Data directives', '@nova 1.0\n@schema infrastructure@1.2.0#8c21a0f3d5e6b7c8\n@revision sha256:91cf16db\n@scope services[id=api]\n@envelope result\n@compat json\n@x-custom some value  # comment\na: 1\n'),
    ('Directives only', '@nova 1.0\n'),
    ('Query document', '@get services[*].{id,replicas}\n'),
    ('Query response', '@revision sha256:91cf16db\n@scope services[id=api]\n[2]{id,replicas}:\n  auth,3\n  api,5\n'),
]

C['templates'] = [
    ('Template header', '@template 0.1\n@param env: string\n@param owner?: string\n@param replicas: u16 @range(1..50) = 2   # comment\n@param tags: list<string> = [a, b]\n@import "base.nova" as base\n@let registry = "ghcr.io/example"\n@let defaults:\n  enabled: true\n  replicas: $replicas\n@let banner: |\n  Hello ${name}!\n\nx: 1\n'),
    ('References', '@template\na: $env\nb: $base.region\nc: $svc.ports[0]\nd: [$a, {k: $b}]\ne: $m["key"].x\n'),
    ('Interpolation', '@template\nimage: "${base.registry}/${name}:v2"\n"app-${name}": $name\nlit: "$${not} \\$x"\nbs: |\n  ${a} and ${b.c[0]}\n  cost $$5 $${lit}\n'),
    ('Spreads', '@template\nrec:\n  ...$defaults\n  id: auth\nlist:\n  ...$xs\n  - extra\nitems:\n  - ...$defaults\n    id: $name\nflow: [...$xs, 1]\nfrec: {...$r, a: 1}\n'),
    ('If and else in a list', '@template\nitems:\n  @if $metrics and $env == production:\n    - id: metrics\n  @else if $env == staging:\n    - id: metrics-lite\n  @else:\n    - id: noop\n'),
    ('If in a record', '@template\nlabels:\n  @if $owner != null:\n    owner: $owner\n  @if not ($a or $b) and (1, 2) == $t:\n    x: 1\n'),
    ('For loops', '@template\nservices:\n  @for name in $services:\n    - id: $name\nlabels:\n  @for i, name in [a, b]:\n    "k-${i}": $name\n'),
    ('Control lines at top level', '@template\n@if $x:\n  a: 1\n@else:\n  b: 2\nc: 3\n'),
    # Template constructs are recognized only after @template (§9.2).
    ('Template syntax in a data document', 'a: $x\nb: "${y}"\nc: |\n  ${z} $$\n'),
    ('@template after @nova', '@nova 1.0\n@template 0.1\nx: $y\n'),
    ('@template after another directive is not a template', '@schema s@1.0.0\n@template\n@let x = 1\nx: $y\n'),
    ('Error: spread in a data document', 'a:\n  ...$x\nb: 1\n'),
    ('Error: control line in a data document', 'a:\n  @if $x:\n    b: 1\nc: 2\n'),
    ('Reference-like word in a data flow value', 'a: [$x]\nb: 1\n'),
]

C['patches'] = [
    ('Patch operations', '@patch infrastructure@1.0.0 sha256:91cf16db2f4a\n@scope services[id=api]\nreplicas: 5 -> 8\n+ tags: canary\n? enabled: true\n= image: "ghcr.io/example/api:v6"\n- legacy\n- old: {a: 1}\n? present\n'),
    ('Patch with base and multi-line value', '@nova 1.0\n@patch infrastructure@1.0.0\n@base sha256:abc\n\n# comment\nservices[0].ports[-1]: [1,\n  2] -> 3\nm["k"].x: a -> b\n[0].a: 1 -> 2\n'),
]

C['positional'] = [
    ('Positional profile',
     '@schema infrastructure@1.0.0#baf8ad5c15586601\n@mode positional\n- production\n- eu-central-1\n- @timestamp("2026-11-17T16:00:00Z")\n- !active\n-\n  - (auth,ghcr.io/example/auth:v2,8080,3,true,!_)\n  - (api,ghcr.io/example/api:v5,3000,5,true,@set[public,edge])\n'),
]

C['examples'] = [
    ('examples/base.nova', read(f'{NOVA}/examples/base.nova')),
    ('examples/infrastructure.nova', read(f'{NOVA}/examples/infrastructure.nova')),
    ('examples/service.tpl.nova', read(f'{NOVA}/examples/service.tpl.nova')),
    ('examples/scale-api.patch', read(f'{NOVA}/examples/scale-api.patch')),
    ('novafile-text tests/commented.nova', read(f'{NOVA}/crates/novafile-text/tests/commented.nova')),
    ('Reference writer output (human profile)', read(f'{S}/zoo.nova')),
]


# Schema files (§6.1): the `nova_schema` grammar.
SC = {}

SC['schemas'] = [
    ('examples/infrastructure.novs', read(f'{NOVA}/examples/infrastructure.novs')),
    ('spec meta/nova-schema.novs', read(f'{SPEC}/meta/nova-schema.novs')),
    ('spec conformance/schema/infrastructure', read(f'{SPEC}/conformance/schema/infrastructure/schema.novs')),
    ('spec conformance/schema/numbers', read(f'{SPEC}/conformance/schema/numbers/schema.novs')),
    ('spec conformance/schema/strings', read(f'{SPEC}/conformance/schema/strings/schema.novs')),
]

SC['definitions'] = [
    ('Directives', '@nova 1.0\n@schema nova-schema@0.1.0\n@root Schema\n@schema pre@2.1.0-rc.1+build.5\n'),
    ('Type aliases', 'type Port = u16 @range(1..65535)\ntype Id = string\n'),
    ('Enums', 'enum Status:\n  active\n  draining  # comment\n\n  failed(string)\nenum Kind = any | null | bool | named\nenum Shape = circle(float @range(0..)) | none\n'),
    ('Records', 'record Empty:\nrecord Bag @open:\n  known?: int\n# between\n\nrecord Service:\n  id: string @key @id(1)\n  tags?: set<string> @id(6) @deprecated\n'),
    ('Field names that are keywords', 'record Def:\n  type: Type\n  record?: RecordDef\n  enum?: list<VariantDef>\n  key: bool = false\n'),
]

SC['types'] = [
    ('Built-in and named types', 'record R:\n  a: any\n  b: null\n  c: i64\n  d: timestamp\n  e: Service\n'),
    ('Generic types', 'record R:\n  a: list<int>\n  b: set<string>\n  c: map<string, list<Service>>\n'),
    ('Tuples', 'record R:\n  a: (string, u8)\n  b: (int,)\n  c: ()\n'),
    ('Arrays and nullable types', 'record R:\n  a: Service[]\n  b: int?\n  c: int[]?\n  d: int?[]\n  e: list<int?>?\n'),
    ('Attributes on type arguments', 'type Ports = list<u16 @range(1..1024)>\ntype M = map<string @len(1..8), int>\nrecord R:\n  g: list<string @len(1..1, graphemes)>\n  t: (int @range(0..), string)\n'),
]

SC['attributes'] = [
    ('Ranges and lengths', 'record R:\n  f: float @range(0..0.1)\n  q?: decimal @range(1/3..)\n  n: int @range(-5..5)\n  b: string @len(..4, utf8)\n  s?: string @len(..4)\n  l: list<int> @len(1..)\n'),
    ('Defaults', 'record R:\n  a: bool = true\n  b: Status = !active\n  c: list<string> = [a, "b c"]\n  d: map<int, string> = @map{1 => one}\n  e: timestamp = @timestamp("2026-11-17T16:00:00Z")\n  f: (int, int) = (1, 2)\n  g: string = "a#b"  # comment\n  h: Rec = {x: 1, "y z": [2]}\n'),
]

SC['errors'] = [
    ('Error: field indented three spaces', 'record R:\n   a: int\n  b: int\nrecord S:\n  c: int\n'),
    ('Error: field indented four spaces', 'record R:\n  a: int\n    b: int\n  c: int\n'),
    ('Error: field indented one space', 'record R:\n  a: int\n b: int\nrecord S:\n  c: int\n'),
    ('Error: indented definition', 'type A = int\n  type B = int\nrecord S:\n  c: int\n'),
    ('Error: tab in indentation', 'record R:\n\ta: int\n  b: int\n'),
    ('Error: enum variant indented wrongly', 'enum E:\n  a\n   b\n  c\n'),
    ('Error: unknown directive', '@x-ext foo\nrecord R:\n  a: int\n'),
    ('Error: unclosed attribute', 'record R:\n  a: int @id(1\n  c: int\n'),
]


def emit(name, cases, directory='test/corpus'):
    out = []
    for title, src in cases:
        out.append('=' * 80 + '\n' + title + '\n' + '=' * 80 + '\n\n' + src)
        if not src.endswith('\n'):
            out.append('\n')
        out.append('\n' + '-' * 80 + '\n\n\n')
    with open(f'{REPO}/{directory}/{name}.txt', 'w', newline='') as f:
        f.write(''.join(out).rstrip('\n') + '\n')


for name, cases in C.items():
    emit(name, cases)
for name, cases in SC.items():
    emit(name, cases, 'schema/test/corpus')
print(sum(len(v) for v in C.values()), 'nova cases,', sum(len(v) for v in SC.values()), 'nova_schema cases')
