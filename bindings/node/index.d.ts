type BaseNode = {
  type: string;
  named: boolean;
};

type ChildNode = {
  multiple: boolean;
  required: boolean;
  types: BaseNode[];
};

type NodeInfo =
  | (BaseNode & {
      subtypes: BaseNode[];
    })
  | (BaseNode & {
      fields: { [name: string]: ChildNode };
      children: ChildNode[];
    });

/** A tree-sitter language object. */
type LanguageObject = {
  /**
   * The inner language object.
   * @private
   */
  language: unknown;

  /**
   * The content of the `node-types.json` file for this grammar.
   *
   * @see {@linkplain https://tree-sitter.github.io/tree-sitter/using-parsers/6-static-node-types Static Node Types}
   */
  nodeTypeInfo: NodeInfo[];

  /** The syntax highlighting query for this grammar. */
  HIGHLIGHTS_QUERY?: string;

  /** The local variable query for this grammar. */
  LOCALS_QUERY?: string;
};

/**
 * The tree-sitter language object for the NOVA schema language (`.novs`).
 *
 * @example
 * import { schema } from "@novafile/tree-sitter-nova";
 *
 * const parser = new Parser();
 * parser.setLanguage(schema);
 */
export declare const schema: LanguageObject;

/**
 * The tree-sitter language object for NOVA text (`.nova`): data, template,
 * patch and query documents and text streams. The schema language is
 * available as its `schema` property and as the named export `schema`.
 *
 * @see {@linkcode https://tree-sitter.github.io/node-tree-sitter/interfaces/Language.html Parser.Language}
 *
 * @example
 * import Parser from "tree-sitter";
 * import Nova from "@novafile/tree-sitter-nova";
 *
 * const parser = new Parser();
 * parser.setLanguage(Nova);
 */
declare const binding: {
  /**
   * The inner language object.
   * @private
   */
  language: unknown;

  /**
   * The content of the `node-types.json` file for this grammar.
   *
   * @see {@linkplain https://tree-sitter.github.io/tree-sitter/using-parsers/6-static-node-types Static Node Types}
   */
  nodeTypeInfo: NodeInfo[];

  /** The syntax highlighting query for this grammar. */
  HIGHLIGHTS_QUERY?: string;

  /** The language injection query for this grammar. */
  INJECTIONS_QUERY?: string;

  /** The local variable query for this grammar. */
  LOCALS_QUERY?: string;

  /** The symbol tagging query for this grammar. */
  TAGS_QUERY?: string;

  /** The NOVA schema language (`.novs`). */
  schema: LanguageObject;
};

export default binding;
