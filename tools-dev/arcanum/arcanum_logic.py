"""
Arcanum logic module — all index-building and query functions.

Separated from arcanum_server.py so the MCP entry point can hot-reload this
module via importlib.reload(arcanum_logic) without restarting Claude Code.
Edit freely; call the arcanum_reload() MCP tool to pick up changes.

IMPORTANT: arcanum_server.py must call these functions via module-attribute
lookup (`arcanum_logic.arcanum_search(...)`), NOT via `from arcanum_logic
import arcanum_search`. The former hot-reloads; the latter binds at import.
"""

import os
import re
from pathlib import Path
from collections import defaultdict

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

CASE_DIR = Path(os.environ.get(
    "CASE_DIR",
    Path.home() / "Desktop" / "IMPORTANT DOCS" / "Case_Reference"
))

IMPORTANT_DOCS_DIR = Path(os.environ.get(
    "IMPORTANT_DOCS_DIR",
    Path.home() / "Desktop" / "IMPORTANT DOCS"
))

# File extensions to index (beyond .md)
INDEX_EXTENSIONS = {".md", ".txt", ".log", ".csv", ".json", ".xml", ".html", ".htm"}

# ---------------------------------------------------------------------------
# Index state (module-level; reset by _build_index)
# ---------------------------------------------------------------------------

_index: dict[str, dict] = {}  # relative_path -> {path, title, headers, description, content, folder}
_folders: dict[str, list[str]] = defaultdict(list)  # folder -> [relative_paths]


def _index_file(prefix: str, base_dir: Path, filepath: Path):
    """Index a single file into the search index."""
    rel = filepath.relative_to(base_dir)
    key = f"{prefix}/{rel.as_posix()}"
    folder = f"{prefix}/{rel.parent.as_posix()}" if rel.parent != Path(".") else prefix

    try:
        content = filepath.read_text(encoding="utf-8", errors="replace")
    except Exception:
        content = ""

    # Extract title (first # heading for md, first non-empty line otherwise)
    title = rel.stem
    if filepath.suffix == ".md":
        title_match = re.search(r"^#\s+(.+)$", content, re.MULTILINE)
        if title_match:
            title = title_match.group(1).strip()
    else:
        for line in content.split("\n")[:5]:
            line = line.strip()
            if line and not line.startswith(("{", "<", "---")):
                title = line[:100]
                break

    # Extract all headers (markdown)
    headers = re.findall(r"^#{1,4}\s+(.+)$", content, re.MULTILINE)

    # Extract frontmatter description
    desc_match = re.search(r'^description:\s*["\']?(.+?)["\']?\s*$', content, re.MULTILINE)
    description = desc_match.group(1) if desc_match else ""

    # Extract frontmatter tags (for case files)
    tags_match = re.search(r'^tags:\s*\[(.+?)\]\s*$', content, re.MULTILINE)
    tags = [t.strip().strip('"\'') for t in tags_match.group(1).split(",")] if tags_match else []

    # Extract frontmatter people
    people_match = re.search(r'^people:\s*\[(.+?)\]\s*$', content, re.MULTILINE)
    people = [p.strip().strip('"\'') for p in people_match.group(1).split(",")] if people_match else []

    # Extract frontmatter date
    date_match = re.search(r'^date:\s*["\']?(\d{4}[-/]\d{2}[-/]\d{2})["\']?\s*$', content, re.MULTILINE)
    date = date_match.group(1) if date_match else ""

    # Extract frontmatter doc_type
    doctype_match = re.search(r'^doc_type:\s*["\']?(.+?)["\']?\s*$', content, re.MULTILINE)
    doc_type = doctype_match.group(1).strip() if doctype_match else ""

    # Extract frontmatter filing_relevance
    filing_match = re.search(r'^filing_relevance:\s*\[(.+?)\]\s*$', content, re.MULTILINE)
    filing_relevance = [f.strip().strip('"\'') for f in filing_match.group(1).split(",")] if filing_match else []

    # Extract > blockquote lines (often contain source/status)
    meta_lines = re.findall(r"^>\s+(.+)$", content, re.MULTILINE)

    _index[key] = {
        "path": key,
        "abs_path": str(filepath),
        "title": title,
        "headers": headers,
        "description": description,
        "tags": tags,
        "people": people,
        "date": date,
        "doc_type": doc_type,
        "filing_relevance": filing_relevance,
        "meta": meta_lines[:3],
        "content": content,
        "folder": folder,
        "size": len(content),
        "lines": content.count("\n") + 1,
    }
    _folders[folder].append(key)


def _build_index():
    """Scan all source dirs. Build searchable index."""
    _index.clear()
    _folders.clear()

    # Markdown-only sources (existing behavior)
    md_sources = [
        ("arcanum", ARCANUM_DIR),
        ("memory", MEMORY_DIR),
        ("reports", REPORTS_DIR),
    ]

    for prefix, base_dir in md_sources:
        if not base_dir.exists():
            continue
        for md_file in base_dir.rglob("*.md"):
            _index_file(prefix, base_dir, md_file)

    # Case archive — index all text-readable files
    if CASE_DIR.exists():
        for filepath in CASE_DIR.rglob("*"):
            if filepath.is_file() and filepath.suffix.lower() in INDEX_EXTENSIONS:
                _index_file("case", CASE_DIR, filepath)

    # IMPORTANT DOCS — index all 7 folders (Angel_VA, Brand, Career, etc.)
    # Excludes Case_Reference (already indexed above with its own prefix)
    if IMPORTANT_DOCS_DIR.exists():
        for filepath in IMPORTANT_DOCS_DIR.rglob("*"):
            if filepath.is_file() and filepath.suffix.lower() in INDEX_EXTENSIONS:
                # Skip Case_Reference subtree (already indexed as "case" scope)
                try:
                    filepath.relative_to(CASE_DIR)
                    continue  # inside Case_Reference, skip
                except ValueError:
                    pass  # not inside Case_Reference, index it
                _index_file("important_docs", IMPORTANT_DOCS_DIR, filepath)


# ---------------------------------------------------------------------------
# Tool implementations (plain functions; arcanum_server.py wraps with @mcp.tool)
# ---------------------------------------------------------------------------


def arcanum_search(query: str, scope: str = "all", max_results: int = 10) -> str:
    """Full-text search across all indexed docs (arcanum, memory, reports, case, important_docs)."""
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

        # Score: title > tags/people > header > description > content
        score = 0
        tags_lower = " ".join(doc.get("tags", [])).lower()
        people_lower = " ".join(doc.get("people", [])).lower()
        doc_type = doc.get("doc_type", "").lower()
        filing_lower = " ".join(doc.get("filing_relevance", [])).lower()

        for t in terms:
            if t in title_lower:
                score += 10
            if t in tags_lower:
                score += 8  # frontmatter tag match
            if t in people_lower:
                score += 8  # frontmatter people match
            if t in headers_lower:
                score += 5
            if t in doc["description"].lower():
                score += 3
            if t in doc_type:
                score += 2
            if t in filing_lower:
                score += 6  # filing relevance match

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


def arcanum_read(path: str) -> str:
    """Read a specific arcanum document by its path."""
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


def arcanum_index(folder: str = "") -> str:
    """Browse the arcanum topic tree. Shows folders and their documents."""
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


def arcanum_lookup(keyword: str, max_results: int = 15) -> str:
    """Find docs by keyword match in titles, headers, tags, people, and descriptions."""
    keyword_lower = keyword.lower()
    results = []

    for key, doc in _index.items():
        score = 0
        if keyword_lower in doc["title"].lower():
            score += 10
        if keyword_lower in " ".join(doc.get("tags", [])).lower():
            score += 8
        if keyword_lower in " ".join(doc.get("people", [])).lower():
            score += 8
        if keyword_lower in doc["description"].lower():
            score += 5
        if any(keyword_lower in h.lower() for h in doc["headers"]):
            score += 3
        if keyword_lower in key.lower():
            score += 2
        if keyword_lower in " ".join(doc.get("filing_relevance", [])).lower():
            score += 6

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
# Mbox archive search — proxies to the SQLite FTS5 DB built by tools/mbox/
#
# The mbox index is NOT loaded into the in-memory _index above. At ~9.8 GB of
# mbox → ~3–4 GB unique text, cramming it into the Python dict would blow RAM
# and defeat the purpose of a trigram FTS. Instead we query the SQLite DB
# on-demand using a short-lived connection and return formatted markdown.
# ---------------------------------------------------------------------------

MBOX_DB_PATH = Path(os.environ.get(
    "MBOX_DB_PATH",
    Path.home() / "Desktop" / "Excluded" / "mbox" / "mbox_index.db"
))


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
    """Search the mbox SQLite FTS5 index built by ``tools/mbox/index.py``.

    Args:
        query: FTS5 trigram query (substring match: 'Warehm' → 'Wareham').
        sender: Filter by From address substring (case-insensitive).
        recipient: Filter by To/Cc substring.
        subject: Filter by Subject substring.
        label: Filter by X-Gmail-Labels substring (e.g. 'Legal', 'Important').
        since: Start date YYYY-MM-DD (inclusive).
        until: End date YYYY-MM-DD (exclusive).
        has_attachment: Only return messages with at least one attachment.
        attachment: Filter by attachment filename glob (e.g. 'narsum*.pdf').
        max_results: Maximum results to return (default 20, max 100).
    """
    import sqlite3
    import datetime as _dt

    if not MBOX_DB_PATH.exists():
        return (
            f"Mbox DB not found at {MBOX_DB_PATH}. "
            f"Run `python -m tools.mbox.index` to build it."
        )

    max_results = max(1, min(max_results, 100))

    def _parse_date(s: str) -> int | None:
        if not s:
            return None
        try:
            return int(_dt.datetime.strptime(s, "%Y-%m-%d").timestamp())
        except ValueError:
            return None

    where: list[str] = []
    params: list = []
    joins = ""

    if query:
        joins = "JOIN messages_fts f ON f.rowid = m.id"
        where.append("messages_fts MATCH ?")
        params.append(query)
    if sender:
        where.append("m.from_addr LIKE ?")
        params.append(f"%{sender.lower()}%")
    if recipient:
        where.append("(m.to_addrs LIKE ? OR m.cc_addrs LIKE ?)")
        params.extend([f"%{recipient.lower()}%", f"%{recipient.lower()}%"])
    if subject:
        where.append("m.subject LIKE ?")
        params.append(f"%{subject}%")
    if label:
        where.append("m.labels LIKE ?")
        params.append(f"%{label}%")
    if since:
        ts = _parse_date(since)
        if ts is not None:
            where.append("m.date_sent >= ?")
            params.append(ts)
    if until:
        ts = _parse_date(until)
        if ts is not None:
            where.append("m.date_sent < ?")
            params.append(ts)
    if has_attachment:
        where.append("m.attachment_count > 0")
    if attachment:
        joins += (
            " JOIN message_attachments ma ON ma.message_id = m.id"
            " JOIN attachments a ON a.sha256 = ma.attachment_sha256"
        )
        like = attachment.replace("*", "%").replace("?", "_")
        if "%" not in like:
            like = f"%{like}%"
        where.append("(ma.filename_as_sent LIKE ? OR a.filename LIKE ?)")
        params.extend([like, like])

    where_sql = ("WHERE " + " AND ".join(where)) if where else ""
    order = "ORDER BY m.date_sent DESC" if not query else "ORDER BY rank"
    sql = f"""
        SELECT DISTINCT m.id, m.message_id, m.date_sent, m.date_raw,
               m.from_addr, m.from_name, m.to_addrs, m.subject,
               m.labels, m.attachment_count, m.body_text, m.source_mbox
        FROM messages m
        {joins}
        {where_sql}
        {order}
        LIMIT {max_results}
    """

    try:
        uri = f"file:{MBOX_DB_PATH.as_posix()}?mode=ro"
        conn = sqlite3.connect(uri, uri=True)
        conn.row_factory = sqlite3.Row
        # Apply runtime pragmas so the query benefits from mmap/cache.
        for pragma in (
            "PRAGMA mmap_size=30000000000",
            "PRAGMA cache_size=-2000000",
            "PRAGMA temp_store=MEMORY",
        ):
            try:
                conn.execute(pragma)
            except sqlite3.DatabaseError:
                pass
        rows = conn.execute(sql, params).fetchall()
    except sqlite3.DatabaseError as e:
        return f"Mbox DB query failed: {e}"
    finally:
        try:
            conn.close()
        except Exception:
            pass

    if not rows:
        return f"No mbox results for query='{query}' (filters applied)."

    def _fmt_ts(ts):
        if not ts:
            return "(no date)"
        try:
            return _dt.datetime.fromtimestamp(ts).strftime("%Y-%m-%d %H:%M")
        except (ValueError, OSError, OverflowError):
            return f"(ts={ts})"

    def _snippet(body: str, needle: str, width: int = 120) -> str:
        if not body:
            return ""
        idx = body.lower().find(needle.lower()) if needle else -1
        if idx < 0:
            out = body[: width * 2]
        else:
            start = max(0, idx - width)
            end = min(len(body), idx + len(needle) + width)
            out = ("…" if start > 0 else "") + body[start:end] + ("…" if end < len(body) else "")
        return " ".join(out.replace("\n", " ").replace("\r", " ").split())

    lines = [f"## Mbox search: {len(rows)} result(s)\n"]
    if query:
        lines.append(f"Query: `{query}`")
    filter_parts = []
    for name, val in [
        ("from", sender), ("to", recipient), ("subject", subject),
        ("label", label), ("since", since), ("until", until),
        ("attachment", attachment),
    ]:
        if val:
            filter_parts.append(f"{name}={val}")
    if has_attachment:
        filter_parts.append("has_attachment")
    if filter_parts:
        lines.append("Filters: " + ", ".join(filter_parts))
    lines.append("")

    for r in rows:
        atts = f" [{r['attachment_count']} att]" if r['attachment_count'] else ""
        lines.append(
            f"**#{r['id']}** · {_fmt_ts(r['date_sent'])} · "
            f"{(r['from_addr'] or '')[:60]}{atts}"
        )
        lines.append(f"  Subject: {(r['subject'] or '(no subject)')[:120]}")
        if r['labels']:
            lines.append(f"  Labels: {r['labels'][:100]}")
        snip = _snippet(r['body_text'] or '', query)
        if snip:
            lines.append(f"  > {snip[:300]}")
        lines.append("")

    lines.append(
        f"_Use `arcanum_mbox_read(message_id={rows[0]['id']})` for full message + attachments._"
    )
    return "\n".join(lines)


def arcanum_mbox_read(message_id: int) -> str:
    """Read a single mbox message by its DB row id (returned by arcanum_mbox_search).

    Args:
        message_id: The row id from the messages table (not the RFC Message-ID).
    """
    import sqlite3

    if not MBOX_DB_PATH.exists():
        return f"Mbox DB not found at {MBOX_DB_PATH}."

    try:
        uri = f"file:{MBOX_DB_PATH.as_posix()}?mode=ro"
        conn = sqlite3.connect(uri, uri=True)
        conn.row_factory = sqlite3.Row
        row = conn.execute("SELECT * FROM messages WHERE id = ?", (message_id,)).fetchone()
        if not row:
            return f"No message with id={message_id}."
        atts = conn.execute(
            """
            SELECT ma.filename_as_sent, a.sha256, a.mime_type, a.size_bytes, a.stored_path
            FROM message_attachments ma
            JOIN attachments a ON a.sha256 = ma.attachment_sha256
            WHERE ma.message_id = ?
            """,
            (message_id,),
        ).fetchall()
    except sqlite3.DatabaseError as e:
        return f"Mbox DB query failed: {e}"
    finally:
        try:
            conn.close()
        except Exception:
            pass

    import datetime as _dt
    try:
        date_str = _dt.datetime.fromtimestamp(row['date_sent']).strftime("%Y-%m-%d %H:%M") if row['date_sent'] else "(no date)"
    except (ValueError, OSError, OverflowError):
        date_str = "(bad date)"

    out = [
        f"# Message #{row['id']}",
        f"- **Message-ID**: `{row['message_id']}`",
        f"- **Date**: {date_str}  (raw: {row['date_raw']})",
        f"- **From**: {row['from_name']} <{row['from_addr']}>",
        f"- **To**: {row['to_addrs']}",
    ]
    if row['cc_addrs']:
        out.append(f"- **Cc**: {row['cc_addrs']}")
    out.append(f"- **Subject**: {row['subject']}")
    if row['labels']:
        out.append(f"- **Labels**: {row['labels']}")
    if row['in_reply_to']:
        out.append(f"- **In-Reply-To**: `{row['in_reply_to']}`")
    out.append(f"- **Source**: {row['source_mbox']} @ {row['source_offset']}")

    if atts:
        out.append(f"\n## Attachments ({len(atts)})\n")
        for a in atts:
            size_kb = (a['size_bytes'] or 0) / 1024
            out.append(f"- `{a['filename_as_sent']}` — {a['mime_type']}, {size_kb:.1f} KB")
            out.append(f"  - sha256: `{a['sha256']}`")
            out.append(f"  - path: `{a['stored_path']}`")

    out.append("\n## Body\n")
    body = row['body_text'] or "(no body)"
    # Cap at 20k chars to prevent overflow
    if len(body) > 20_000:
        out.append(body[:20_000])
        out.append(f"\n\n_({len(body) - 20_000:,} more chars truncated)_")
    else:
        out.append(body)

    return "\n".join(out)


# Build on module load (initial import AND every importlib.reload())
_build_index()
