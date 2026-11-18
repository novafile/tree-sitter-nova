"""NOVA (Notation for Objects, Values & Agents) grammar for tree-sitter

Two languages: ``language()`` for NOVA text (``.nova``) and
``language_nova_schema()`` for the schema language (``.novs``).
"""

from ._binding import language, language_nova_schema


def _get_query(name, file):
    files = globals().get("_files")
    if files is None:
        from importlib.resources import files
        globals()["_files"] = files
    try:
        query = files(f"{__package__}") / file
        globals()[name] = query.read_text()
    except FileNotFoundError:
        globals()[name] = None
    return globals()[name]


def __getattr__(name):
    if name == "HIGHLIGHTS_QUERY":
        return _get_query("HIGHLIGHTS_QUERY", "queries/highlights.scm")
    if name == "INJECTIONS_QUERY":
        return _get_query("INJECTIONS_QUERY", "queries/injections.scm")
    if name == "LOCALS_QUERY":
        return _get_query("LOCALS_QUERY", "queries/locals.scm")
    if name == "TAGS_QUERY":
        return _get_query("TAGS_QUERY", "queries/tags.scm")
    if name == "NOVA_SCHEMA_HIGHLIGHTS_QUERY":
        return _get_query("NOVA_SCHEMA_HIGHLIGHTS_QUERY", "queries/schema/highlights.scm")
    if name == "NOVA_SCHEMA_LOCALS_QUERY":
        return _get_query("NOVA_SCHEMA_LOCALS_QUERY", "queries/schema/locals.scm")

    raise AttributeError(f"module {__name__!r} has no attribute {name!r}")


__all__ = [
    "language",
    "language_nova_schema",
    "NOVA_SCHEMA_HIGHLIGHTS_QUERY",
    "NOVA_SCHEMA_LOCALS_QUERY",
    "HIGHLIGHTS_QUERY",
    "INJECTIONS_QUERY",
    "LOCALS_QUERY",
    "TAGS_QUERY",
]


def __dir__():
    return sorted(__all__ + [
        "__all__", "__builtins__", "__cached__", "__doc__", "__file__",
        "__loader__", "__name__", "__package__", "__path__", "__spec__",
    ])
