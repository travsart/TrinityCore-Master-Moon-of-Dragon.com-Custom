"""
Arcanum MCP Server — Knowledge base search for Claude Code internals.

Indexes all markdown files in doc/arcanum/ and provides search, read, and
index tools for instant recall during Claude Code sessions.

Tools:
  - arcanum_search: Full-text search across all arcanum docs
  - arcanum_read: Read a specific doc by path or topic
  - arcanum_index: Browse the topic tree
  - arcanum_lookup: Find docs by keyword in frontmatter/headers
"""

import os
import re
import sys
from pathlib import Path
from collections import defaultdict

# Force UTF-8 for stdio on Windows
if sys.platform == "win32":
    sys.stdin.reconfigure(encoding="utf-8")
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")

from fastmcp import FastMCP

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

ARCANUM_DIR = Path(os.environ.get(
    "ARCANUM_DIR",
    Path(__file__).resolve().parent.parent.parent / "doc" / "arcanum"
))

MEMORY_DIR = Path(os.environ.get(
    "MEMORY_DIR",
    Path.home() / ".claude" / "projects" / "C--Users-atayl-VoxCore" / "memory"
))

REPORTS_DIR = Path(os.environ.get(
    "REPORTS_DIR",
    Path(__file__).resolve().parent.parent.parent / "AI_Studio" / "Reports" / "ClaudeCodeInternals"
))

# ---------------------------------------------------------------------------
# Index builder
# ---------------------------------------------------------------------------

_index: dict[str, dict] = {}  # relative_path -> {path, title, headers, description, content, folder}
_folders: dict[str, list[str]] = defaultdict(list)  # folder -> [relative_paths]


def _build_index():
    """Scan arcanum, memory, and reports dirs. Build searchable index."""
    _index.clear()
    _folders.clear()

    sources = [
        ("arcanum", ARCANUM_DIR),
        ("memory", MEMORY_DIR),
        ("reports", REPORTS_DIR),
    ]

    for prefix, base_dir in sources:
        if not base_dir.exists():
            continue
        for md_file in base_dir.rglob("*.md"):
            rel = md_file.relative_to(base_dir)
            key = f"{prefix}/{rel.as_posix()}"
            folder = f"{prefix}/{rel.parent.as_posix()}" if rel.parent != Path(".") else prefix

            try:
                content = md_file.read_text(encoding="utf-8", errors="replace")
            except Exception:
                content = ""

            # Extract title (first # heading)
            title_match = re.search(r"^#\s+(.+)$", content, re.MULTILINE)
            title = title_match.group(1).strip() if title_match else rel.stem

            # Extract all headers
            headers = re.findall(r"^#{1,4}\s+(.+)$", content, re.MULTILINE)

            # Extract frontmatter description
            desc_match = re.search(r'^description:\s*["\']?(.+?)["\']?\s*$', content, re.MULTILINE)
            description = desc_match.group(1) if desc_match else ""

            # Extract > blockquote lines (often contain source/status)
            meta_lines = re.findall(r"^>\s+(.+)$", content, re.MULTILINE)

            _index[key] = {
                "path": key,
                "abs_path": str(md_file),
                "title": title,
                "headers": headers,
                "description": description,
                "meta": meta_lines[:3],
                "content": content,
                "folder": folder,
                "size": len(content),
                "lines": content.count("\n") + 1,
            }
            _folders[folder].append(key)


# Build on startup
_build_index()

# ---------------------------------------------------------------------------
# MCP Server
# ---------------------------------------------------------------------------

mcp = FastMCP(
    "arcanum",
    instructions=(
        "Arcanum knowledge base server. Provides instant search and retrieval "
        "across the Claude Code internals wiki (doc/arcanum/), memory files, "
        "and deep research reports. Use arcanum_search for full-text queries, "
        "arcanum_lookup for topic/keyword matching, arcanum_read for specific "
        "documents, and arcanum_index to browse the topic tree."
    ),
)


@mcp.tool()
def arcanum_search(query: str, scope: str = "all", max_results: int = 10) -> str:
    """Full-text search across all arcanum docs, memory files, and reports.

    Args:
        query: Search terms (case-insensitive). Supports multiple words (AND logic).
        scope: 'all', 'arcanum', 'memory', or 'reports' to limit search scope.
        max_results: Maximum number of results to return (default 10, max 50).
    """
    max_results = min(max_results, 50)
    terms = query.lower().split()
    if not terms:
        return "Error: empty query"

    results = []
    for key, doc in _index.items():
        # Scope filter
        if scope != "all" and not key.startswith(scope):
            continue

        content_lower = doc["content"].lower()
        title_lower = doc["title"].lower()
        headers_lower = " ".join(doc["headers"]).lower()

        # All terms must match somewhere
        if not all(t in content_lower for t in terms):
            continue

        # Score: title match > header match > content match
        score = 0
        for t in terms:
            if t in title_lower:
                score += 10
            if t in headers_lower:
                score += 5
            if t in doc["description"].lower():
                score += 3

        # Extract context snippet (first matching line)
        snippet = ""
        for line in doc["content"].split("\n"):
            if all(t in line.lower() for t in terms):
                snippet = line.strip()[:200]
                break
        if not snippet:
            for line in doc["content"].split("\n"):
                if any(t in line.lower() for t in terms):
                    snippet = line.strip()[:200]
                    break

        results.append((score, key, doc["title"], snippet, doc["lines"]))

    results.sort(key=lambda x: -x[0])
    results = results[:max_results]

    if not results:
        return f"No results for '{query}' in scope '{scope}'. Try broader terms or scope='all'."

    lines = [f"## Search: '{query}' ({len(results)} results)\n"]
    for score, key, title, snippet, line_count in results:
        lines.append(f"**{key}** ({line_count} lines)")
        lines.append(f"  Title: {title}")
        if snippet:
            lines.append(f"  Match: {snippet}")
        lines.append("")

    return "\n".join(lines)


@mcp.tool()
def arcanum_read(path: str) -> str:
    """Read a specific arcanum document by its path.

    Args:
        path: Document path like 'arcanum/core/compaction_overview.md' or
              'memory/MEMORY.md' or 'reports/01_compaction_engine.md'.
              Partial matches are supported — 'compaction' will find the first match.
    """
    # Exact match
    if path in _index:
        doc = _index[path]
        return f"# {doc['title']}\n**Path**: {doc['path']} ({doc['lines']} lines)\n\n{doc['content']}"

    # Partial match — search by filename or path substring
    matches = []
    path_lower = path.lower().replace("\\", "/")
    for key, doc in _index.items():
        if path_lower in key.lower() or path_lower in doc["title"].lower():
            matches.append((key, doc))

    if not matches:
        return f"No document found for '{path}'. Use arcanum_index() to browse available docs."

    if len(matches) == 1:
        key, doc = matches[0]
        return f"# {doc['title']}\n**Path**: {doc['path']} ({doc['lines']} lines)\n\n{doc['content']}"

    # Multiple matches — list them
    lines = [f"Multiple matches for '{path}' — be more specific:\n"]
    for key, doc in matches[:10]:
        lines.append(f"  - **{key}** — {doc['title']}")
    return "\n".join(lines)


@mcp.tool()
def arcanum_index(folder: str = "") -> str:
    """Browse the arcanum topic tree. Shows folders and their documents.

    Args:
        folder: Optional folder to drill into (e.g. 'arcanum/core', 'memory').
                Empty string returns the top-level overview.
    """
    if not folder:
        # Top-level overview
        lines = ["# Arcanum Knowledge Base\n"]

        # Group by top-level prefix
        prefixes = defaultdict(lambda: {"folders": set(), "files": 0, "lines": 0})
        for key, doc in _index.items():
            prefix = key.split("/")[0]
            prefixes[prefix]["files"] += 1
            prefixes[prefix]["lines"] += doc["lines"]
            parts = key.split("/")
            if len(parts) > 2:
                prefixes[prefix]["folders"].add("/".join(parts[:2]))

        for prefix in sorted(prefixes):
            info = prefixes[prefix]
            folder_count = len(info["folders"])
            lines.append(f"**{prefix}/** — {info['files']} files, {info['lines']:,} lines"
                        + (f", {folder_count} subfolders" if folder_count else ""))

        lines.append(f"\n**Total**: {len(_index)} documents")
        lines.append("\nUse `arcanum_index(folder='arcanum/core')` to drill into a folder.")
        return "\n".join(lines)

    # Drill into specific folder
    folder_clean = folder.rstrip("/")
    matching = []
    subfolders = set()

    for key, doc in _index.items():
        if key.startswith(folder_clean + "/"):
            remaining = key[len(folder_clean) + 1:]
            if "/" in remaining:
                subfolders.add(remaining.split("/")[0])
            else:
                matching.append((key, doc))

    if not matching and not subfolders:
        return f"No folder '{folder}' found. Use arcanum_index() for top-level."

    lines = [f"# {folder}/\n"]

    if subfolders:
        lines.append("**Subfolders:**")
        for sf in sorted(subfolders):
            lines.append(f"  - {folder}/{sf}/")
        lines.append("")

    if matching:
        lines.append("**Documents:**")
        for key, doc in sorted(matching, key=lambda x: x[0]):
            status = ""
            if "STUB" in doc["content"][:200]:
                status = " [STUB]"
            lines.append(f"  - **{key.split('/')[-1]}** — {doc['title']}{status} ({doc['lines']}L)")
        lines.append("")

    return "\n".join(lines)


@mcp.tool()
def arcanum_lookup(keyword: str, max_results: int = 15) -> str:
    """Find docs by keyword match in titles, headers, and descriptions.
    Faster than arcanum_search — checks metadata only, not full content.

    Args:
        keyword: Keyword to match (case-insensitive).
        max_results: Maximum results (default 15).
    """
    keyword_lower = keyword.lower()
    results = []

    for key, doc in _index.items():
        score = 0
        if keyword_lower in doc["title"].lower():
            score += 10
        if keyword_lower in doc["description"].lower():
            score += 5
        if any(keyword_lower in h.lower() for h in doc["headers"]):
            score += 3
        if keyword_lower in key.lower():
            score += 2

        if score > 0:
            results.append((score, key, doc["title"], doc["description"][:100]))

    results.sort(key=lambda x: -x[0])
    results = results[:max_results]

    if not results:
        return f"No docs match keyword '{keyword}'. Try arcanum_search for full-text."

    lines = [f"## Lookup: '{keyword}' ({len(results)} matches)\n"]
    for score, key, title, desc in results:
        lines.append(f"- **{key}** — {title}")
        if desc:
            lines.append(f"  {desc}")
    return "\n".join(lines)


@mcp.tool()
def arcanum_rebuild() -> str:
    """Rebuild the arcanum index. Use after adding or modifying documents."""
    _build_index()
    folder_count = len(_folders)
    doc_count = len(_index)
    total_lines = sum(d["lines"] for d in _index.values())
    stubs = sum(1 for d in _index.values() if "STUB" in d["content"][:200])
    return (f"Index rebuilt: {doc_count} documents in {folder_count} folders, "
            f"{total_lines:,} total lines. {stubs} stubs, {doc_count - stubs} populated.")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    mcp.run(transport="stdio")
