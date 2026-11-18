from typing import Final
from typing_extensions import CapsuleType

HIGHLIGHTS_QUERY: Final[str] | None
"""The syntax highlighting query for this grammar."""

INJECTIONS_QUERY: Final[str] | None
"""The language injection query for this grammar."""

LOCALS_QUERY: Final[str] | None
"""The local variable query for this grammar."""

TAGS_QUERY: Final[str] | None
"""The symbol tagging query for this grammar."""

NOVA_SCHEMA_HIGHLIGHTS_QUERY: Final[str] | None
"""The syntax highlighting query for the schema grammar."""

NOVA_SCHEMA_LOCALS_QUERY: Final[str] | None
"""The local variable query for the schema grammar."""

NOVA_LINES_HIGHLIGHTS_QUERY: Final[str] | None
"""The syntax highlighting query for the NOVA Lines grammar."""

def language() -> CapsuleType:
    """The tree-sitter language function for NOVA text (.nova)."""

def language_nova_schema() -> CapsuleType:
    """The tree-sitter language function for the NOVA schema language (.novs)."""

def language_nova_lines() -> CapsuleType:
    """The tree-sitter language function for NOVA Lines (.novl)."""
