"""
Arcanum MCP Server — thin entry point that registers tools with FastMCP.

All implementation lives in `arcanum_logic.py` so the MCP subprocess can
hot-reload logic edits via the `arcanum_reload` tool without restarting
Claude Code (avoiding the full session-restart latency).

Hot-reload pattern:
  - Tool wrappers here call `arcanum_logic.<func>(...)` via module-attribute
    lookup, NOT `from arcanum_logic import <func>`. The former picks up new
    function objects after `importlib.reload(arcanum_logic)`; the latter
    binds once at import time and stays stale.
  - Edit `arcanum_logic.py`, call `arcanum_reload()`, test. No restart.
  - If you edit THIS file (tool signatures / instructions), a full MCP
    restart is still required — but those edits should be rare.

Tools:
  - arcanum_search: Full-text search across all indexed docs
  - arcanum_read:   Read a specific doc by path or topic
  - arcanum_index:  Browse the topic tree
  - arcanum_lookup: Find docs by keyword in frontmatter/headers
  - arcanum_rebuild: Rebuild the data index after adding/modifying documents
  - arcanum_reload: Hot-reload arcanum_logic.py (code changes, not just data)
"""

import importlib
import sys
from pathlib import Path

# Force UTF-8 for stdio on Windows
if sys.platform == "win32":
    sys.stdin.reconfigure(encoding="utf-8")
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8", line_buffering=True)

# Ensure this directory is importable so `arcanum_logic` resolves regardless
# of the CWD the MCP launcher uses.
sys.path.insert(0, str(Path(__file__).resolve().parent))

from fastmcp import FastMCP
import arcanum_logic  # noqa: E402  (must come after sys.path mutation)

# ---------------------------------------------------------------------------
# MCP Server
# ---------------------------------------------------------------------------

mcp = FastMCP(
    "arcanum",
    instructions=(
        "Arcanum knowledge base server. Provides instant search and retrieval "
        "across the Claude Code internals wiki (doc/arcanum/), memory files, "
        "deep research reports, and the Case_Reference legal archive. "
        "Use arcanum_search for full-text queries (scope='case' for legal files), "
        "arcanum_lookup for topic/keyword matching, arcanum_read for specific "
        "documents, and arcanum_index to browse the topic tree."
    ),
)


@mcp.tool()
def arcanum_search(query: str, scope: str = "all", max_results: int = 10) -> str:
    """Full-text search across all indexed docs (arcanum, memory, reports, case).

    Args:
        query: Search terms (case-insensitive). Supports multiple words (AND logic).
        scope: 'all', 'arcanum', 'memory', 'reports', or 'case' to limit search scope.
        max_results: Maximum number of results to return (default 10, max 50).
    """
    return arcanum_logic.arcanum_search(query, scope, max_results)


@mcp.tool()
def arcanum_read(path: str) -> str:
    """Read a specific arcanum document by its path.

    Args:
        path: Document path like 'arcanum/core/compaction_overview.md' or
              'memory/MEMORY.md' or 'reports/01_compaction_engine.md'.
              Partial matches are supported — 'compaction' will find the first match.
    """
    return arcanum_logic.arcanum_read(path)


@mcp.tool()
def arcanum_index(folder: str = "") -> str:
    """Browse the arcanum topic tree. Shows folders and their documents.

    Args:
        folder: Optional folder to drill into (e.g. 'arcanum/core', 'memory').
                Empty string returns the top-level overview.
    """
    return arcanum_logic.arcanum_index(folder)


@mcp.tool()
def arcanum_lookup(keyword: str, max_results: int = 15) -> str:
    """Find docs by keyword match in titles, headers, tags, people, and descriptions.
    Faster than arcanum_search — checks metadata only, not full content.

    Args:
        keyword: Keyword to match (case-insensitive).
        max_results: Maximum results (default 15).
    """
    return arcanum_logic.arcanum_lookup(keyword, max_results)


@mcp.tool()
def arcanum_rebuild() -> str:
    """Rebuild the arcanum data index. Use after adding or modifying documents."""
    return arcanum_logic.arcanum_rebuild()


@mcp.tool()
def arcanum_mbox_search(
    query: str = "",
    sender: str = "",
    recipient: str = "",
    subject: str = "",
    label: str = "",
    since: str = "",
    until: str = "",
    has_attachment: bool = False,
    attachment: str = "",
    max_results: int = 20,
) -> str:
    """Search the mbox SQLite FTS5 index (Gmail takeout, ~10 GB, built by tools/mbox/index.py).

    Uses a trigram tokenizer, so substring matching works:
      - query='Warehm' hits 'Wareham'
      - query='NARSUM' matches case-insensitively
    Combine with structured filters for precise slices.

    Args:
        query: FTS5 trigram query string (substring + word match).
        sender: Filter by From address substring.
        recipient: Filter by To/Cc substring.
        subject: Filter by Subject substring.
        label: Filter by X-Gmail-Labels (e.g. 'Legal', 'Important').
        since: Start date YYYY-MM-DD (inclusive).
        until: End date YYYY-MM-DD (exclusive).
        has_attachment: Only return messages with >=1 attachment.
        attachment: Filter by attachment filename (glob: 'narsum*.pdf').
        max_results: Maximum results (default 20, capped at 100).
    """
    return arcanum_logic.arcanum_mbox_search(
        query=query, sender=sender, recipient=recipient, subject=subject,
        label=label, since=since, until=until, has_attachment=has_attachment,
        attachment=attachment, max_results=max_results,
    )


@mcp.tool()
def arcanum_mbox_read(message_id: int) -> str:
    """Read a full mbox message by its DB row id (returned from arcanum_mbox_search).

    Args:
        message_id: The numeric row id shown as '#N' in arcanum_mbox_search output.
                    This is NOT the RFC Message-ID string.
    """
    return arcanum_logic.arcanum_mbox_read(message_id)


@mcp.tool()
def arcanum_reload() -> str:
    """Hot-reload arcanum_logic.py to pick up code changes without restarting
    the MCP server (or Claude Code). Use after editing tool implementations in
    arcanum_logic.py so you can test changes in-session.

    Edits to THIS file (arcanum_server.py) — tool signatures, MCP instructions,
    the reload tool itself — still require a full MCP restart.
    """
    try:
        importlib.reload(arcanum_logic)
    except Exception as e:
        return f"Reload FAILED: {type(e).__name__}: {e}. arcanum_logic left in previous state."
    doc_count = len(arcanum_logic._index)
    folder_count = len(arcanum_logic._folders)
    return (
        f"arcanum_logic reloaded. Index rebuilt on reload: "
        f"{doc_count} documents in {folder_count} folders."
    )


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    mcp.run(transport="stdio")
