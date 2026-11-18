#include <napi.h>

typedef struct TSLanguage TSLanguage;

extern "C" TSLanguage *tree_sitter_nova();
extern "C" TSLanguage *tree_sitter_nova_schema();
extern "C" TSLanguage *tree_sitter_nova_lines();

// "tree-sitter", "language" hashed with BLAKE2
const napi_type_tag LANGUAGE_TYPE_TAG = {
    0x8AF2E5212AD58ABF, 0xD5006CAD83ABBA16
};

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    auto language = Napi::External<TSLanguage>::New(env, tree_sitter_nova());
    language.TypeTag(&LANGUAGE_TYPE_TAG);
    exports["language"] = language;

    auto schema_language = Napi::External<TSLanguage>::New(env, tree_sitter_nova_schema());
    schema_language.TypeTag(&LANGUAGE_TYPE_TAG);
    exports["schemaLanguage"] = schema_language;

    auto lines_language = Napi::External<TSLanguage>::New(env, tree_sitter_nova_lines());
    lines_language.TypeTag(&LANGUAGE_TYPE_TAG);
    exports["linesLanguage"] = lines_language;
    return exports;
}

NODE_API_MODULE(tree_sitter_nova_binding, Init)
