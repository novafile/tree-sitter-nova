import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";

const root = fileURLToPath(new URL("../..", import.meta.url));

// A `bun build --compile` executable has no package directory for node-gyp-build to search, so
// require the prebuild directly. Its path has to stay static for Bun to embed the addon.
const binding = typeof process.versions.bun === "string" && Bun.isStandaloneExecutable
  ? require(`../../prebuilds/${process.platform}-${process.arch}/tree-sitter-nova.node`)
  : (await import("node-gyp-build")).default(root);

// The second grammar: the NOVA schema language (`.novs`). Like the default
// export, it is a language object for `Parser.setLanguage`.
const schema = { language: binding.schemaLanguage };
delete binding.schemaLanguage;
binding.schema = schema;

try {
  // Bun only embeds static paths, and `import()` rejects absolute Windows paths like `root`.
  const nodeTypes = await import("../../src/node-types.json", { with: { type: "json" } });
  binding.nodeTypeInfo = nodeTypes.default;
} catch { }

try {
  const nodeTypes = await import("../../schema/src/node-types.json", { with: { type: "json" } });
  schema.nodeTypeInfo = nodeTypes.default;
} catch { }

if (typeof process.versions.bun === "string" && Bun.isStandaloneExecutable) {
  // Static paths so Bun embeds the queries. Grammars often lack some of them, and inside a `try` a
  // missing file is skipped instead of failing the build.
  try {
    binding.HIGHLIGHTS_QUERY = (await import("../../queries/highlights.scm", { with: { type: "text" } })).default;
  } catch { }
  try {
    binding.INJECTIONS_QUERY = (await import("../../queries/injections.scm", { with: { type: "text" } })).default;
  } catch { }
  try {
    binding.LOCALS_QUERY = (await import("../../queries/locals.scm", { with: { type: "text" } })).default;
  } catch { }
  try {
    binding.TAGS_QUERY = (await import("../../queries/tags.scm", { with: { type: "text" } })).default;
  } catch { }
  try {
    schema.HIGHLIGHTS_QUERY = (await import("../../schema/queries/highlights.scm", { with: { type: "text" } })).default;
  } catch { }
  try {
    schema.LOCALS_QUERY = (await import("../../schema/queries/locals.scm", { with: { type: "text" } })).default;
  } catch { }
} else {
  const queries = [
    [binding, "HIGHLIGHTS_QUERY", `${root}/queries/highlights.scm`],
    [binding, "INJECTIONS_QUERY", `${root}/queries/injections.scm`],
    [binding, "LOCALS_QUERY", `${root}/queries/locals.scm`],
    [binding, "TAGS_QUERY", `${root}/queries/tags.scm`],
    [schema, "HIGHLIGHTS_QUERY", `${root}/schema/queries/highlights.scm`],
    [schema, "LOCALS_QUERY", `${root}/schema/queries/locals.scm`],
  ];

  for (const [target, prop, path] of queries) {
    Object.defineProperty(target, prop, {
      configurable: true,
      enumerable: true,
      get() {
        delete target[prop];
        try {
          target[prop] = readFileSync(path, "utf8");
        } catch { }
        return target[prop];
      }
    });
  }
}

export { schema };
export default binding;
