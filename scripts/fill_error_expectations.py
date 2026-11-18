"""Fill the empty expectations of corpus tests whose parse has errors.

`tree-sitter test -u` does not write them. Run after it, from the
repository root:

    python3 scripts/fill_error_expectations.py            # test/corpus
    python3 scripts/fill_error_expectations.py -p schema  # schema/test/corpus

The trees are taken from `tree-sitter test` itself, so the input is exactly
the one the test runner parses. Review the result before committing.
"""
# SPDX-License-Identifier: Apache-2.0
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
grammar = sys.argv[sys.argv.index('-p') + 1] if '-p' in sys.argv else '.'
corpus = os.path.join(REPO, grammar, 'test', 'corpus')
PLACEHOLDER = '(placeholder)'

HEADER = re.compile(r'^(={3,})\n(.*?)\n\1\n', re.M)
DIVIDER = re.compile(r'^-{3,}\n', re.M)
ANSI = re.compile(r'\x1b\[[0-9;]*m')


def split(text):
    """(start, end, name, body before expectation, expectation) per test."""
    heads = list(HEADER.finditer(text))
    for i, head in enumerate(heads):
        end = heads[i + 1].start() if i + 1 < len(heads) else len(text)
        body = text[head.end():end]
        div = list(DIVIDER.finditer(body))[-1]  # inputs may contain `---` lines
        yield head.start(), end, head.group(2), text[head.start():head.end() + div.end()], body[div.end():]


def actual_trees(file_name):
    out = subprocess.run(
        ['npx', '--no-install', 'tree-sitter', 'test', '-p', grammar, '--file-name', file_name],
        cwd=REPO, capture_output=True, text=True,
    ).stdout
    trees, name, lines = {}, None, []
    started = False
    for raw in out.splitlines():
        if 'correct /' in raw:
            started = True
            continue
        if not started:
            continue
        m = re.match(r'^\s+\d+\. (.*):$', ANSI.sub('', raw))
        if m:
            if name:
                trees[name] = lines
            name, lines = m.group(1), []
            continue
        if name is None or '\x1b[32m' in raw:  # expected-only line
            continue
        line = ANSI.sub('', raw).rstrip()
        if line.strip() and not line.startswith('Total parses'):
            lines.append(line)
    if name:
        trees[name] = lines
    return trees


for file_name in sorted(os.listdir(corpus)):
    path = os.path.join(corpus, file_name)
    with open(path, newline='') as f:
        text = f.read()
    empty = [t for t in split(text) if not t[4].strip()]
    if not empty:
        continue
    with open(path, 'w', newline='') as f:
        f.write(''.join(
            t[3] + ('\n' + PLACEHOLDER + '\n\n' if not t[4].strip() else t[4]) for t in split(text)
        ).rstrip('\n') + '\n')
    trees = actual_trees(file_name)
    out = []
    for start, end, name, before, expected in split(text):
        if expected.strip():
            out.append(before + expected)
            continue
        tree = trees.get(name)
        if not tree:
            sys.exit(f'{file_name}: no tree for {name!r}')
        indent = min(len(line) - len(line.lstrip()) for line in tree)
        tree = '\n'.join(line[indent:] for line in tree)
        # The diff output can split a line that ends both trees; balance it.
        missing = tree.count('(') - tree.count(')')
        if missing:
            print(f'{file_name}: {name!r}: added {missing} closing parentheses, check it')
            tree += ')' * missing
        out.append(before + '\n' + tree + '\n\n')
    with open(path, 'w', newline='') as f:
        f.write(''.join(out).rstrip('\n') + '\n')
    print('updated', file_name, f'({len(empty)} tests)')
