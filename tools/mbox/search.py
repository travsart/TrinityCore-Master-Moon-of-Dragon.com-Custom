"""FTS5 search CLI for the mbox index.

Usage:
    # Full-text (trigram) — substring and word match both work
    python -m tools.mbox.search "Wheeler NARSUM"
    python -m tools.mbox.search Wareham

    # Structured filters (combine freely)
    python -m tools.mbox.search --from tolin
    python -m tools.mbox.search --to adam.taylor
    python -m tools.mbox.search --subject "privilege"
    python -m tools.mbox.search --since 2026-03-01 --until 2026-04-01
    python -m tools.mbox.search --attachments "narsum*.pdf"
    python -m tools.mbox.search --label Legal
    python -m tools.mbox.search --has-attachment

    # Full message view
    python -m tools.mbox.search --id 12345
    python -m tools.mbox.search --message-id '<abc@example.com>'

    # Output format
    python -m tools.mbox.search "query" --json
    python -m tools.mbox.search "query" --limit 50 --context 120
"""

from __future__ import annotations

import argparse
import datetime as _dt
import json
import sys
import textwrap
from pathlib import Path

# Force UTF-8 on Windows stdio so decoded MIME headers (emoji, CJK, etc.)
# print cleanly instead of crashing cp1252.
if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

from .store import DEFAULT_DB_PATH, connect, format_bytes


def parse_date_cli(s: str) -> int:
    """Accept YYYY-MM-DD or a unix timestamp."""
    if not s:
        return 0
    if s.isdigit():
        return int(s)
    try:
        dt = _dt.datetime.strptime(s, "%Y-%m-%d")
    except ValueError:
        dt = _dt.datetime.strptime(s, "%Y-%m-%dT%H:%M:%S")
    return int(dt.timestamp())


def fmt_ts(ts: int | None) -> str:
    if not ts:
        return "(no date)"
    try:
        return _dt.datetime.fromtimestamp(ts).strftime("%Y-%m-%d %H:%M")
    except (ValueError, OSError, OverflowError):
        return f"(ts={ts})"


def snippet(body: str, needle: str, context: int = 80) -> str:
    if not body:
        return ""
    idx = body.lower().find(needle.lower()) if needle else -1
    if idx < 0:
        out = body[: context * 2]
    else:
        start = max(0, idx - context)
        end = min(len(body), idx + len(needle) + context)
        out = ("…" if start > 0 else "") + body[start:end] + ("…" if end < len(body) else "")
    out = out.replace("\n", " ").replace("\r", " ")
    return " ".join(out.split())


# =========================================================================
# Query builder
# =========================================================================


def build_query(args) -> tuple[str, list]:
    """Compose the SQL. Returns (sql, params)."""
    where = []
    params: list = []
    joins = ""

    # Full-text search via FTS5
    if args.query:
        joins = "JOIN messages_fts f ON f.rowid = m.id"
        where.append("messages_fts MATCH ?")
        params.append(args.query)

    # Structured filters
    if args.from_:
        where.append("m.from_addr LIKE ?")
        params.append(f"%{args.from_.lower()}%")
    if args.to:
        where.append("(m.to_addrs LIKE ? OR m.cc_addrs LIKE ?)")
        params.append(f"%{args.to.lower()}%")
        params.append(f"%{args.to.lower()}%")
    if args.subject:
        where.append("m.subject LIKE ?")
        params.append(f"%{args.subject}%")
    if args.label:
        where.append("m.labels LIKE ?")
        params.append(f"%{args.label}%")
    if args.since:
        where.append("m.date_sent >= ?")
        params.append(parse_date_cli(args.since))
    if args.until:
        where.append("m.date_sent < ?")
        params.append(parse_date_cli(args.until))
    if args.has_attachment:
        where.append("m.attachment_count > 0")
    if args.attachments:
        joins += (
            " JOIN message_attachments ma ON ma.message_id = m.id"
            " JOIN attachments a ON a.sha256 = ma.attachment_sha256"
        )
        where.append("(ma.filename_as_sent LIKE ? OR a.filename LIKE ?)")
        like = args.attachments.replace("*", "%").replace("?", "_")
        if "%" not in like:
            like = f"%{like}%"
        params.append(like)
        params.append(like)

    where_sql = ("WHERE " + " AND ".join(where)) if where else ""
    order = "ORDER BY m.date_sent DESC" if not args.query else "ORDER BY rank"
    limit = f"LIMIT {args.limit} OFFSET {args.offset}"

    sql = f"""
        SELECT DISTINCT m.id, m.message_id, m.date_sent, m.from_addr, m.from_name,
               m.to_addrs, m.subject, m.labels, m.attachment_count, m.body_text,
               m.source_mbox
        FROM messages m
        {joins}
        {where_sql}
        {order}
        {limit}
    """
    return sql, params


# =========================================================================
# Single message view
# =========================================================================


def show_message(conn, m_id: int | None, message_id: str | None) -> int:
    if m_id:
        row = conn.execute("SELECT * FROM messages WHERE id = ?", (m_id,)).fetchone()
    else:
        mid = (message_id or "").strip().strip("<>").lower()
        row = conn.execute("SELECT * FROM messages WHERE message_id = ?", (mid,)).fetchone()
    if not row:
        print("Not found.", file=sys.stderr)
        return 1

    print(f"id:          {row['id']}")
    print(f"message_id:  {row['message_id']}")
    print(f"date:        {fmt_ts(row['date_sent'])}  (raw: {row['date_raw']})")
    print(f"from:        {row['from_name']} <{row['from_addr']}>")
    print(f"to:          {row['to_addrs']}")
    if row["cc_addrs"]:
        print(f"cc:          {row['cc_addrs']}")
    print(f"subject:     {row['subject']}")
    if row["labels"]:
        print(f"labels:      {row['labels']}")
    if row["in_reply_to"]:
        print(f"in-reply-to: {row['in_reply_to']}")
    print(f"source:      {row['source_mbox']} @ {row['source_offset']}")

    atts = conn.execute(
        """
        SELECT ma.filename_as_sent, a.sha256, a.mime_type, a.size_bytes, a.stored_path
        FROM message_attachments ma
        JOIN attachments a ON a.sha256 = ma.attachment_sha256
        WHERE ma.message_id = ?
        """,
        (row["id"],),
    ).fetchall()
    if atts:
        print(f"attachments: {len(atts)}")
        for a in atts:
            print(f"  - {a['filename_as_sent']} [{a['mime_type']}, {format_bytes(a['size_bytes'] or 0)}]")
            print(f"    sha256: {a['sha256']}")
            print(f"    path:   {a['stored_path']}")

    print()
    print("---")
    body = row["body_text"] or "(no body)"
    print(body[:20_000])
    if len(body) > 20_000:
        print(f"\n... ({len(body) - 20_000} more chars truncated)")
    return 0


# =========================================================================
# Main
# =========================================================================


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="python -m tools.mbox.search")
    p.add_argument("query", nargs="?", default="", help="FTS5 query (trigram — substring OK)")
    p.add_argument("--db", default=str(DEFAULT_DB_PATH), help="SQLite DB path")
    p.add_argument("--from", dest="from_", help="filter by sender (substring, case-insensitive)")
    p.add_argument("--to", help="filter by recipient (To or Cc)")
    p.add_argument("--subject", help="filter by subject substring")
    p.add_argument("--label", help="filter by Gmail X-Gmail-Labels substring")
    p.add_argument("--since", help="start date (YYYY-MM-DD)")
    p.add_argument("--until", help="end date (YYYY-MM-DD, exclusive)")
    p.add_argument("--has-attachment", action="store_true", help="require at least one attachment")
    p.add_argument("--attachments", help="filter by attachment filename (glob: *.pdf)")
    p.add_argument("--limit", type=int, default=25)
    p.add_argument("--offset", type=int, default=0)
    p.add_argument("--context", type=int, default=80, help="snippet context width")
    p.add_argument("--json", action="store_true", help="output JSON lines")
    p.add_argument("--id", type=int, help="show single message by row id")
    p.add_argument("--message-id", help="show single message by RFC Message-ID")
    p.add_argument("--count", action="store_true", help="return match count only")
    args = p.parse_args(argv)

    db_path = Path(args.db)
    if not db_path.exists():
        print(f"DB not found: {db_path}", file=sys.stderr)
        print(f"Run: python -m tools.mbox.index  first", file=sys.stderr)
        return 2

    conn = connect(db_path, read_only=True)

    if args.id or args.message_id:
        return show_message(conn, args.id, args.message_id)

    sql, params = build_query(args)

    if args.count:
        count_sql = f"SELECT COUNT(DISTINCT m.id) AS c FROM messages m " + \
                    sql.split("FROM messages m", 1)[1].split("ORDER BY")[0]
        total = conn.execute(count_sql, params).fetchone()["c"]
        print(total)
        return 0

    rows = conn.execute(sql, params).fetchall()

    if args.json:
        for r in rows:
            print(json.dumps({
                "id": r["id"],
                "message_id": r["message_id"],
                "date": fmt_ts(r["date_sent"]),
                "date_sent": r["date_sent"],
                "from": r["from_addr"],
                "from_name": r["from_name"],
                "to": r["to_addrs"],
                "subject": r["subject"],
                "labels": r["labels"],
                "attachments": r["attachment_count"],
                "source_mbox": Path(r["source_mbox"]).name,
                "snippet": snippet(r["body_text"] or "", args.query, args.context),
            }, ensure_ascii=False))
        return 0

    if not rows:
        print("(no results)")
        return 0

    print(f"{len(rows)} results:\n")
    for r in rows:
        atts = f" [{r['attachment_count']} att]" if r["attachment_count"] else ""
        print(f"#{r['id']:<6} {fmt_ts(r['date_sent']):<16} {r['from_addr'][:38]:<38} {atts}")
        subj = (r["subject"] or "(no subject)")[:100]
        print(f"        {subj}")
        snip = snippet(r["body_text"] or "", args.query, args.context)
        if snip:
            for line in textwrap.wrap(snip, width=100, initial_indent="        ", subsequent_indent="        "):
                print(line)
        print()

    return 0


if __name__ == "__main__":
    sys.exit(main())
