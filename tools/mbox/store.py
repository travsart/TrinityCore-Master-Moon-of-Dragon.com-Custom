"""Shared storage + utility helpers for the mbox indexer.

Centralises:
    * DB connection + pragma tuning (mmap, cache, WAL)
    * Schema bootstrap
    * Content-addressable attachment store
    * Text/HTML extraction + normalization
    * Message-ID normalization + synthetic-ID fallback
"""

from __future__ import annotations

import hashlib
import html
import os
import re
import sqlite3
import time
from email.header import decode_header, make_header
from email.utils import getaddresses, parsedate_to_datetime
from pathlib import Path
from typing import Iterable

# =========================================================================
# Defaults
# =========================================================================

DEFAULT_MBOX_DIR = Path(r"C:\Users\atayl\Desktop\Excluded\mbox")
DEFAULT_DB_PATH = DEFAULT_MBOX_DIR / "mbox_index.db"
DEFAULT_ATTACHMENT_DIR = DEFAULT_MBOX_DIR / "attachments"

SCHEMA_PATH = Path(__file__).parent / "schema.sql"

# Pragma targets — tuned for 128 GB RAM / NVMe workstation.
# mmap_size=30 GB lets SQLite memory-map the whole DB; cache_size is a
# fallback for uncached pages. Negative value = KiB units.
PRAGMAS_INIT = [
    ("page_size", 8192),                 # must be set before tables on empty DB
    ("journal_mode", "WAL"),             # persistent across connections
    ("synchronous", "NORMAL"),
    ("temp_store", "MEMORY"),
    ("mmap_size", 30_000_000_000),       # 30 GB
    ("cache_size", -2_000_000),          # 2 GB page cache
    ("foreign_keys", "ON"),
]

PRAGMAS_RUNTIME = [
    ("journal_mode", "WAL"),
    ("synchronous", "NORMAL"),
    ("temp_store", "MEMORY"),
    ("mmap_size", 30_000_000_000),
    ("cache_size", -2_000_000),
    ("foreign_keys", "ON"),
]

# Bulk-load pragmas — trades crash safety for write speed during indexing.
# Any crash during bulk load just means rebuild from source mboxes (cheap).
#
# `journal_mode=MEMORY` keeps the rollback journal in RAM (fast, no on-disk
# journal file), and cleanly releases all file handles on close — important
# for the parallel merge phase which ATTACHes partial DBs. `OFF` is faster
# but leaves the DB in a state where subsequent ATTACH fails on Windows.
#
# `synchronous=OFF` skips fsync on commit (data only at risk on power loss).
# Applied temporarily; finalize_db() restores WAL + NORMAL at the end.
PRAGMAS_BULK_LOAD = [
    ("journal_mode", "MEMORY"),
    ("synchronous", "OFF"),
    ("temp_store", "MEMORY"),
    ("mmap_size", 30_000_000_000),
    ("cache_size", -4_000_000),  # 4 GB page cache during bulk load
    ("foreign_keys", "OFF"),      # skip FK checks; dedup on UNIQUE handles integrity
]

# =========================================================================
# Connection + schema
# =========================================================================


def connect(db_path: Path | str = DEFAULT_DB_PATH, *, read_only: bool = False,
            autocommit: bool = False) -> sqlite3.Connection:
    """Open (or create) the mbox index DB with all pragmas applied.

    Args:
        read_only: Open in URI read-only mode.
        autocommit: Disable Python's implicit transaction management
            (isolation_level=None). Required for ATTACH DATABASE statements,
            which can't run inside an implicit transaction. Use for the
            merge phase in parallel mode.
    """
    db_path = Path(db_path)
    db_path.parent.mkdir(parents=True, exist_ok=True)
    is_new = not db_path.exists() or db_path.stat().st_size == 0

    connect_kwargs = {}
    if autocommit:
        connect_kwargs["isolation_level"] = None

    if read_only:
        uri = f"file:{db_path.as_posix()}?mode=ro"
        conn = sqlite3.connect(uri, uri=True, **connect_kwargs)
    else:
        conn = sqlite3.connect(str(db_path), **connect_kwargs)

    # Apply pragmas
    pragmas = PRAGMAS_INIT if is_new and not read_only else PRAGMAS_RUNTIME
    for key, val in pragmas:
        conn.execute(f"PRAGMA {key}={val}")

    if is_new and not read_only:
        schema_sql = SCHEMA_PATH.read_text(encoding="utf-8")
        conn.executescript(schema_sql)
        if not autocommit:
            conn.commit()

    conn.row_factory = sqlite3.Row
    return conn


def apply_bulk_pragmas(conn: sqlite3.Connection) -> None:
    """Switch a live connection into bulk-load mode. Call before heavy writes."""
    for key, val in PRAGMAS_BULK_LOAD:
        try:
            conn.execute(f"PRAGMA {key}={val}")
        except sqlite3.DatabaseError:
            pass


def apply_runtime_pragmas(conn: sqlite3.Connection) -> None:
    """Restore WAL + normal synchronous. Call after bulk load finishes."""
    for key, val in PRAGMAS_RUNTIME:
        try:
            conn.execute(f"PRAGMA {key}={val}")
        except sqlite3.DatabaseError:
            pass


def rebuild_fts(conn: sqlite3.Connection) -> int:
    """Bulk-rebuild messages_fts from the messages table in one pass.

    Dramatically faster than per-message FTS5 writes because the trigram
    tokenizer can batch internally and SQLite avoids the per-row overhead.
    Returns the row count inserted.

    Uses DROP + CREATE because contentless FTS5 tables don't support
    ``DELETE FROM``, and the `'delete-all'` command only works on FTS5
    tables with an external content source. DROP+CREATE is idempotent and
    simpler.
    """
    conn.execute("DROP TABLE IF EXISTS messages_fts")
    conn.execute(
        """
        CREATE VIRTUAL TABLE messages_fts USING fts5(
            subject,
            from_addr,
            from_name,
            to_addrs,
            body,
            content='',
            tokenize='trigram'
        )
        """
    )
    conn.execute(
        """
        INSERT INTO messages_fts(rowid, subject, from_addr, from_name, to_addrs, body)
        SELECT id,
               COALESCE(subject, ''),
               COALESCE(from_addr, ''),
               COALESCE(from_name, ''),
               COALESCE(to_addrs, ''),
               COALESCE(body_text, '')
        FROM messages
        """
    )
    count = conn.execute("SELECT COUNT(*) FROM messages_fts").fetchone()[0]
    conn.execute("INSERT INTO messages_fts(messages_fts) VALUES('optimize')")
    conn.commit()
    return int(count)


def finalize_db(conn: sqlite3.Connection, vacuum: bool = True) -> None:
    """Restore normal pragmas, ANALYZE, and optionally VACUUM. Call once at end."""
    apply_runtime_pragmas(conn)
    try:
        conn.execute("ANALYZE")
        conn.commit()
    except sqlite3.DatabaseError:
        pass
    if vacuum:
        try:
            conn.execute("VACUUM")
        except sqlite3.DatabaseError:
            pass


# =========================================================================
# Attachment content-addressable store
# =========================================================================


def attachment_path(sha256: str, att_dir: Path = DEFAULT_ATTACHMENT_DIR) -> Path:
    """Two-char sharded path: <att_dir>/<sha256[:2]>/<sha256>."""
    return att_dir / sha256[:2] / sha256


def store_attachment(payload: bytes, att_dir: Path = DEFAULT_ATTACHMENT_DIR) -> tuple[str, Path, bool]:
    """Hash payload, write to CAS if new. Returns (sha256, path, is_new)."""
    digest = hashlib.sha256(payload).hexdigest()
    dest = attachment_path(digest, att_dir)
    if dest.exists() and dest.stat().st_size == len(payload):
        return digest, dest, False
    dest.parent.mkdir(parents=True, exist_ok=True)
    tmp = dest.with_suffix(".tmp")
    tmp.write_bytes(payload)
    tmp.replace(dest)
    return digest, dest, True


# =========================================================================
# Message-ID normalization + synthetic fallback
# =========================================================================

_MSG_ID_RE = re.compile(r"<([^>]+)>")


def normalize_message_id(raw: str | None) -> str | None:
    """Strip angle brackets, lowercase, collapse whitespace."""
    if not raw:
        return None
    m = _MSG_ID_RE.search(raw)
    ident = m.group(1) if m else raw.strip()
    ident = ident.strip().strip("<>").strip()
    return ident.lower() if ident else None


def synthetic_message_id(body_text: str, from_addr: str, date_raw: str, subject: str) -> str:
    """Deterministic ID for messages missing a Message-ID header."""
    h = hashlib.sha256()
    h.update((from_addr or "").encode("utf-8", "replace"))
    h.update(b"\x00")
    h.update((date_raw or "").encode("utf-8", "replace"))
    h.update(b"\x00")
    h.update((subject or "").encode("utf-8", "replace"))
    h.update(b"\x00")
    h.update((body_text or "").encode("utf-8", "replace"))
    return f"synthetic-{h.hexdigest()}"


# =========================================================================
# Header extraction
# =========================================================================


def parse_date(raw: str | None) -> int | None:
    """Return unix timestamp or None if unparseable."""
    if not raw:
        return None
    try:
        dt = parsedate_to_datetime(raw)
        return int(dt.timestamp()) if dt is not None else None
    except (TypeError, ValueError, OverflowError):
        return None


def decode_mime_header(raw: str | None) -> str:
    """Decode an RFC 2047 encoded header value (=?UTF-8?B?...?=) to plain text."""
    if not raw:
        return ""
    try:
        return str(make_header(decode_header(raw)))
    except Exception:
        # Fallback: try per-fragment decode, drop anything that blows up
        try:
            parts = decode_header(raw)
            out = []
            for text, charset in parts:
                if isinstance(text, bytes):
                    out.append(text.decode(charset or "utf-8", errors="replace"))
                else:
                    out.append(text)
            return "".join(out)
        except Exception:
            return raw


def extract_address(raw: str | None) -> tuple[str, str]:
    """Return (email_lower, display_name) for the first address in raw."""
    if not raw:
        return "", ""
    try:
        pairs = getaddresses([raw])
    except Exception:
        return "", ""
    if not pairs:
        return "", ""
    name, addr = pairs[0]
    return (addr or "").strip().lower(), decode_mime_header((name or "").strip())


def extract_addresses(raw: str | None) -> str:
    """Return comma-joined lowercased emails from a header."""
    if not raw:
        return ""
    try:
        pairs = getaddresses([raw])
    except Exception:
        return ""
    return ", ".join((addr or "").strip().lower() for _, addr in pairs if addr)


# =========================================================================
# Body text extraction + HTML strip
# =========================================================================

_HTML_TAG_RE = re.compile(r"<[^>]+>")
_HTML_WS_RE = re.compile(r"[ \t]+\n")
_MULTI_BLANK_RE = re.compile(r"\n{3,}")


def strip_html(raw: str) -> str:
    """Minimal HTML → text. Not perfect but fast and dependency-free."""
    # Drop <script>/<style> contents
    raw = re.sub(r"<(script|style)[^>]*>.*?</\1>", "", raw, flags=re.DOTALL | re.IGNORECASE)
    # Insert newlines at block boundaries
    raw = re.sub(r"</(p|div|br|tr|li|h[1-6])[^>]*>", "\n", raw, flags=re.IGNORECASE)
    raw = re.sub(r"<br[^>]*>", "\n", raw, flags=re.IGNORECASE)
    # Strip tags
    raw = _HTML_TAG_RE.sub("", raw)
    # Decode entities
    raw = html.unescape(raw)
    # Collapse whitespace
    raw = _HTML_WS_RE.sub("\n", raw)
    raw = _MULTI_BLANK_RE.sub("\n\n", raw)
    return raw.strip()


def decode_payload(payload: bytes, charset: str | None) -> str:
    """Decode bytes → str, falling back to utf-8 replace."""
    if not payload:
        return ""
    for cs in (charset, "utf-8", "latin-1"):
        if not cs:
            continue
        try:
            return payload.decode(cs, errors="replace")
        except (LookupError, UnicodeDecodeError):
            continue
    return payload.decode("utf-8", errors="replace")


def extract_body_and_attachments(msg) -> tuple[str, bool, list[tuple[str, str, bytes]]]:
    """Walk a message, return (body_text, was_html_stripped, [(filename, mime, payload), ...])."""
    body_parts: list[str] = []
    html_parts: list[str] = []
    attachments: list[tuple[str, str, bytes]] = []
    was_html = False

    if msg.is_multipart():
        for part in msg.walk():
            if part.is_multipart():
                continue
            fn = part.get_filename()
            ct = (part.get_content_type() or "").lower()
            disp = (part.get("Content-Disposition") or "").lower()

            if fn or "attachment" in disp:
                payload = part.get_payload(decode=True)
                if payload is None:
                    continue
                attachments.append((fn or f"inline-{ct.replace('/', '_')}", ct, payload))
                continue

            payload = part.get_payload(decode=True)
            if payload is None:
                continue
            text = decode_payload(payload, part.get_content_charset())
            if ct == "text/plain":
                body_parts.append(text)
            elif ct == "text/html":
                html_parts.append(text)
    else:
        payload = msg.get_payload(decode=True) or b""
        text = decode_payload(payload, msg.get_content_charset())
        ct = (msg.get_content_type() or "").lower()
        if ct == "text/html":
            html_parts.append(text)
        else:
            body_parts.append(text)

    if body_parts:
        body_text = "\n\n".join(p for p in body_parts if p).strip()
    elif html_parts:
        body_text = strip_html("\n\n".join(html_parts))
        was_html = True
    else:
        body_text = ""

    return body_text, was_html, attachments


# =========================================================================
# Streaming mbox iterator (no mailbox.mbox indexing overhead)
# =========================================================================


def iter_mbox(path: Path) -> Iterable[tuple[int, bytes]]:
    """Yield (byte_offset, raw_message_bytes) from an mbox file.

    Splits on lines starting with ``From `` (mbox-O format as used by Gmail
    Takeout). This avoids mailbox.mbox's upfront file scan, which is O(file
    size) and prohibitively slow on multi-GB mboxes.
    """
    with open(path, "rb") as f:
        buf = bytearray()
        msg_start = 0
        pos = 0
        while True:
            line_start = pos
            line = f.readline()
            if not line:
                break
            if line.startswith(b"From ") and buf:
                yield msg_start, bytes(buf)
                buf.clear()
                msg_start = line_start
                pos += len(line)
                continue
            if line.startswith(b"From ") and not buf:
                msg_start = line_start
                pos += len(line)
                continue
            buf.extend(line)
            pos += len(line)
        if buf:
            yield msg_start, bytes(buf)


# =========================================================================
# FTS5 sync
# =========================================================================


def fts_insert(conn: sqlite3.Connection, rowid: int, subject: str, from_addr: str,
               from_name: str, to_addrs: str, body: str) -> None:
    """Insert a row into the contentless messages_fts table."""
    conn.execute(
        "INSERT INTO messages_fts(rowid, subject, from_addr, from_name, to_addrs, body) "
        "VALUES (?, ?, ?, ?, ?, ?)",
        (rowid, subject or "", from_addr or "", from_name or "", to_addrs or "", body or ""),
    )


def fts_delete(conn: sqlite3.Connection, rowid: int) -> None:
    """Remove a row from contentless FTS (delete-insert pattern)."""
    conn.execute(
        "INSERT INTO messages_fts(messages_fts, rowid, subject, from_addr, from_name, to_addrs, body) "
        "VALUES ('delete', ?, '', '', '', '', '')",
        (rowid,),
    )


# =========================================================================
# Progress helpers
# =========================================================================


def now_ts() -> int:
    return int(time.time())


def format_bytes(n: int) -> str:
    for unit in ("B", "KB", "MB", "GB", "TB"):
        if n < 1024:
            return f"{n:.1f} {unit}"
        n /= 1024
    return f"{n:.1f} PB"
