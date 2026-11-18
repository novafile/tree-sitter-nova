import assert from "node:assert";
import { test } from "node:test";
import Parser from "tree-sitter";

test("can load grammar", async () => {
  const parser = new Parser();
  await assert.doesNotReject(async () => {
    const { default: language } = await import("./index.js");
    parser.setLanguage(language);
  });
});

test("can load the schema grammar", async () => {
  const parser = new Parser();
  const { default: language, schema } = await import("./index.js");
  assert.strictEqual(language.schema, schema);
  parser.setLanguage(schema);
  const tree = parser.parse("record R:\n  a: list<int> = [1]\n");
  assert.strictEqual(tree.rootNode.type, "schema");
  assert.ok(!tree.rootNode.hasError);
  assert.ok(schema.nodeTypeInfo.length > 0);
  assert.ok(schema.HIGHLIGHTS_QUERY.includes("@type"));
});

test("can load the NOVA Lines grammar", async () => {
  const parser = new Parser();
  const { default: language, lines } = await import("./index.js");
  assert.strictEqual(language.lines, lines);
  parser.setLanguage(lines);
  const tree = parser.parse("@columns {a, b{c}, ..}\n1, (2,), {d: 3}\n");
  assert.strictEqual(tree.rootNode.type, "stream");
  assert.ok(!tree.rootNode.hasError);
  assert.ok(lines.nodeTypeInfo.length > 0);
  assert.ok(lines.HIGHLIGHTS_QUERY.includes("@keyword.directive"));
});

test("reports wrong indentation", async () => {
  const parser = new Parser();
  const { default: language } = await import("./index.js");
  parser.setLanguage(language);
  assert.ok(parser.parse("a:\n   b: 1\n").rootNode.hasError);
  assert.ok(!parser.parse("a:\n  b: 1\n---\nc: 2\n").rootNode.hasError);
});
