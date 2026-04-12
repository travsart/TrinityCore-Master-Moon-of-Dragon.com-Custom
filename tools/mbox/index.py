"""Streaming mbox indexer.

Usage:
    python -m tools.mbox.index                              # index default dir
    python -m tools.mbox.index <mbox_dir>                   # index a directory
    python -m tools.mbox.index <single.mbox>                # index one file
    python -m tools.mbox.index --db <path> --attachments <path> <dir>
    python -m tools.mbox.index --no-attachments <dir>       # skip attachment CAS
    python -m tools.mbox.index --force <dir>                # reindex even if completed
    python -m tools.mbox.index --status                     # show per-mbox progress

Design notes:
    * Streams via tools.mbox.store.iter_mbox (line-based, no mailbox.mbox O(file) scan)
    * Dedups by RFC Message-ID; falls back to a synthetic hash-based ID
    * Attachments hashed by SHA256 → content-addressable store (two-char shard)
    * Checkpoints every COMMIT_EVERY messages into ingest_progress
    * Resumable: completed mboxes are skipped; partial ones restart from 0 but
      the dedup INSERT OR IGNORE skips already-seen Message-IDs cheaply
    * One transaction per batch — FTS5 writes dominate at small batch sizes
"""

from __future__ import annotations

import argparse
import email
import email.parser
import hashlib
import os
import sys
import time
import traceback
from email.policy import compat32
from pathlib import Path

# Force UTF-8 on Windows stdio so progress printing doesn't choke on
# MIME-decoded subjects (emoji, CJK, etc.).
if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

from . import store
from .store import (
    DEFAULT_ATTACHMENT_DIR,
    DEFAULT_DB_PATH,
    DEFAULT_MBOX_DIR,
    connect,
    decode_mime_header,
    extract_address,
    extract_addresses,
    extract_body_and_attachments,
    format_bytes,
    fts_insert,
    iter_mbox,
    normalize_message_id,
    now_ts,
    parse_date,
    store_attachment,
    synthetic_message_id,
)

COMMIT_EVERY = 500          # messages per transaction
PROGRESS_EVERY = 1000       # print progress every N messages
MAX_BODY_BYTES = 4_000_000  # cap on body text stored (4 MB per message)


# =========================================================================
# Single-message processor
# =========================================================================


def process_message(
    conn,
    raw: bytes,
    source_mbox: str,
    source_offset: int,
    *,
    extract_attachments: bool,
    attachment_dir: Path,
    fts_sync: bool = True,
) -> dict:
    """Parse, dedup, and insert a single raw message.

    Args:
        fts_sync: If True (default), write to messages_fts per-message. If
            False, skip FTS writes — caller must call store.rebuild_fts()
            in bulk at the end of the indexing run. False is ~2× faster
            for bulk loads.

    Returns a dict: {status: 'new'|'duplicate'|'error', atts_new, atts_dup, error}
    """
    result = {"status": "error", "atts_new": 0, "atts_dup": 0, "error": None}
    try:
        msg = email.message_from_bytes(raw, policy=compat32)
    except Exception as e:
        result["error"] = f"parse: {e}"
        return result

    try:
        # Headers
        subject = decode_mime_header((msg.get("Subject") or "").strip())
        from_addr, from_name = extract_address(msg.get("From"))
        to_addrs = extract_addresses(msg.get("To"))
        cc_addrs = extract_addresses(msg.get("Cc"))
        date_raw = (msg.get("Date") or "").strip()
        date_sent = parse_date(date_raw)
        in_reply_to = (msg.get("In-Reply-To") or "").strip()
        references_hdr = (msg.get("References") or "").strip()
        labels = (msg.get("X-Gmail-Labels") or "").strip()
        raw_mid = msg.get("Message-ID") or msg.get("Message-Id")
        message_id = normalize_message_id(raw_mid)

        # Body + attachments
        body_text, was_html, attachments = extract_body_and_attachments(msg)
        if body_text and len(body_text) > MAX_BODY_BYTES:
            body_text = body_text[:MAX_BODY_BYTES]

        # Synthetic Message-ID fallback
        if not message_id:
            message_id = synthetic_message_id(body_text, from_addr, date_raw, subject)

        body_sha256 = hashlib.sha256(body_text.encode("utf-8", "replace")).hexdigest() if body_text else None
        size_bytes = len(raw)

        # Insert message (dedup by Message-ID UNIQUE)
        cur = conn.execute(
            """
            INSERT OR IGNORE INTO messages (
                message_id, body_sha256, source_mbox, source_offset,
                date_sent, date_raw, from_addr, from_name,
                to_addrs, cc_addrs, subject,
                in_reply_to, references_hdr, labels,
                size_bytes, attachment_count, has_body,
                body_text, body_html_stripped
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                message_id, body_sha256, source_mbox, source_offset,
                date_sent, date_raw, from_addr, from_name,
                to_addrs, cc_addrs, subject,
                in_reply_to, references_hdr, labels,
                size_bytes, len(attachments), 1 if body_text else 0,
                body_text, 1 if was_html else 0,
            ),
        )

        if cur.rowcount == 0:
            # Duplicate — Message-ID already present
            result["status"] = "duplicate"
            return result

        message_row_id = cur.lastrowid

        # FTS5 sync (skippable in bulk-load mode)
        if fts_sync:
            fts_insert(conn, message_row_id, subject, from_addr, from_name, to_addrs, body_text)

        # Attachments
        if extract_attachments and attachments:
            for filename_as_sent, mime_type, payload in attachments:
                try:
                    digest, dest, is_new = store_attachment(payload, attachment_dir)
                except Exception as e:
                    # Don't fail the whole message on attachment write error
                    conn.execute(
                        "UPDATE ingest_progress SET error_count = error_count + 1, last_error = ? "
                        "WHERE mbox_file = ?",
                        (f"attachment write: {e}", source_mbox),
                    )
                    continue

                # Always INSERT OR IGNORE into the DB — `is_new` refers to the
                # on-disk CAS store, not the per-process partial DB. Without
                # this, workers can end up with message_attachments rows whose
                # sha256 exists in no attachments row, breaking FK constraints
                # during the parallel merge.
                if is_new:
                    result["atts_new"] += 1
                else:
                    result["atts_dup"] += 1
                conn.execute(
                    """
                    INSERT OR IGNORE INTO attachments
                        (sha256, filename, mime_type, size_bytes, stored_path, first_seen_ts)
                    VALUES (?, ?, ?, ?, ?, ?)
                    """,
                    (digest, filename_as_sent, mime_type, len(payload), str(dest), now_ts()),
                )

                conn.execute(
                    """
                    INSERT OR IGNORE INTO message_attachments
                        (message_id, attachment_sha256, filename_as_sent)
                    VALUES (?, ?, ?)
                    """,
                    (message_row_id, digest, filename_as_sent),
                )

        result["status"] = "new"
        return result

    except Exception as e:
        result["error"] = f"process: {e}\n{traceback.format_exc()}"
        return result


# =========================================================================
# Per-mbox driver
# =========================================================================


def index_mbox_file(
    conn,
    path: Path,
    *,
    extract_attachments: bool,
    attachment_dir: Path,
    force: bool,
    fts_sync: bool = True,
) -> dict:
    """Index a single mbox file. Returns stats dict.

    Args:
        fts_sync: propagated to process_message. False defers FTS5 construction
            to a single bulk pass; caller must invoke store.rebuild_fts() at
            the end. Caller should also apply_bulk_pragmas() before this loop.
    """
    path = path.resolve()
    size = path.stat().st_size

    # Check progress row
    row = conn.execute(
        "SELECT completed_at, messages_processed FROM ingest_progress WHERE mbox_file = ?",
        (str(path),),
    ).fetchone()

    if row and row["completed_at"] and not force:
        print(f"  [skip] {path.name} — already completed ({row['messages_processed']} msgs)")
        return {"skipped": True}

    # Initialise progress
    conn.execute(
        """
        INSERT INTO ingest_progress (mbox_file, size_bytes, started_at, updated_at)
        VALUES (?, ?, ?, ?)
        ON CONFLICT(mbox_file) DO UPDATE SET
            started_at = excluded.started_at,
            updated_at = excluded.updated_at,
            size_bytes = excluded.size_bytes,
            completed_at = NULL,
            error_count = 0,
            last_error = NULL
        """,
        (str(path), size, now_ts(), now_ts()),
    )
    conn.commit()

    processed = 0
    new_msgs = 0
    dup_msgs = 0
    atts_new_total = 0
    atts_dup_total = 0
    errors = 0
    last_error = None
    last_offset = 0
    start_ts = time.time()
    last_print_ts = start_ts

    print(f"  [index] {path.name} ({format_bytes(size)})")

    try:
        for offset, raw in iter_mbox(path):
            last_offset = offset
            res = process_message(
                conn, raw, str(path), offset,
                extract_attachments=extract_attachments,
                attachment_dir=attachment_dir,
                fts_sync=fts_sync,
            )
            processed += 1
            if res["status"] == "new":
                new_msgs += 1
                atts_new_total += res["atts_new"]
                atts_dup_total += res["atts_dup"]
            elif res["status"] == "duplicate":
                dup_msgs += 1
            else:
                errors += 1
                if res["error"] and not last_error:
                    last_error = res["error"][:500]

            if processed % COMMIT_EVERY == 0:
                conn.execute(
                    """
                    UPDATE ingest_progress SET
                        last_byte_offset = ?,
                        messages_processed = ?,
                        messages_new = ?,
                        messages_duplicate = ?,
                        attachments_extracted = ?,
                        attachments_deduped = ?,
                        error_count = ?,
                        last_error = ?,
                        updated_at = ?
                    WHERE mbox_file = ?
                    """,
                    (last_offset, processed, new_msgs, dup_msgs,
                     atts_new_total, atts_dup_total, errors, last_error,
                     now_ts(), str(path)),
                )
                conn.commit()

            if processed % PROGRESS_EVERY == 0:
                now = time.time()
                elapsed = now - start_ts
                rate = processed / elapsed if elapsed > 0 else 0
                pct = (offset / size * 100) if size else 100
                mb_rate = (offset / elapsed / 1024 / 1024) if elapsed > 0 else 0
                print(
                    f"    {processed:>7} msgs | {new_msgs:>7} new | {dup_msgs:>7} dup | "
                    f"{atts_new_total:>5} att-new | {pct:5.1f}% | {rate:6.0f} msg/s | {mb_rate:5.1f} MB/s"
                )
                last_print_ts = now

    except KeyboardInterrupt:
        print("\n  [interrupt] committing and exiting...")
    except Exception as e:
        errors += 1
        last_error = f"{type(e).__name__}: {e}"
        print(f"  [error] {last_error}")
        traceback.print_exc()

    # Final commit + mark complete
    conn.execute(
        """
        UPDATE ingest_progress SET
            last_byte_offset = ?,
            messages_processed = ?,
            messages_new = ?,
            messages_duplicate = ?,
            attachments_extracted = ?,
            attachments_deduped = ?,
            error_count = ?,
            last_error = ?,
            updated_at = ?,
            completed_at = ?
        WHERE mbox_file = ?
        """,
        (last_offset, processed, new_msgs, dup_msgs,
         atts_new_total, atts_dup_total, errors, last_error,
         now_ts(), now_ts(), str(path)),
    )
    conn.commit()

    elapsed = time.time() - start_ts
    print(
        f"  [done]  {path.name}: {processed} msgs "
        f"({new_msgs} new, {dup_msgs} dup) "
        f"{atts_new_total} new atts / {atts_dup_total} dup atts "
        f"in {elapsed:.1f}s ({errors} errors)"
    )
    return {
        "skipped": False,
        "processed": processed,
        "new": new_msgs,
        "duplicate": dup_msgs,
        "attachments_new": atts_new_total,
        "attachments_dup": atts_dup_total,
        "errors": errors,
        "elapsed": elapsed,
    }


# =========================================================================
# Directory driver
# =========================================================================


def collect_mbox_files(target: Path) -> list[Path]:
    if target.is_file():
        return [target]
    if target.is_dir():
        return sorted(target.glob("*.mbox"))
    return []


def print_status(db_path: Path) -> None:
    conn = connect(db_path, read_only=True)
    rows = conn.execute(
        """
        SELECT mbox_file, size_bytes, messages_processed, messages_new,
               messages_duplicate, attachments_extracted, attachments_deduped,
               completed_at, error_count
        FROM ingest_progress ORDER BY mbox_file
        """
    ).fetchall()
    if not rows:
        print("No ingest progress recorded.")
        return

    print(f"{'file':<50} {'size':>10} {'msgs':>8} {'new':>8} {'dup':>8} {'atts':>8} {'status':>10}")
    print("-" * 108)
    for r in rows:
        name = Path(r["mbox_file"]).name[:50]
        status = "DONE" if r["completed_at"] else "PARTIAL"
        if r["error_count"]:
            status += f" ({r['error_count']} err)"
        print(
            f"{name:<50} {format_bytes(r['size_bytes'] or 0):>10} "
            f"{r['messages_processed']:>8} {r['messages_new']:>8} {r['messages_duplicate']:>8} "
            f"{r['attachments_extracted']:>8} {status:>10}"
        )

    total = conn.execute("SELECT COUNT(*) FROM messages").fetchone()[0]
    atts = conn.execute("SELECT COUNT(*) FROM attachments").fetchone()[0]
    att_bytes = conn.execute("SELECT COALESCE(SUM(size_bytes), 0) FROM attachments").fetchone()[0]
    print("-" * 108)
    print(f"Unique messages: {total:,}")
    print(f"Unique attachments: {atts:,} ({format_bytes(att_bytes)})")


# =========================================================================
# Entry point
# =========================================================================


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="python -m tools.mbox.index")
    p.add_argument("target", nargs="?", default=str(DEFAULT_MBOX_DIR),
                   help="mbox file or directory (default: Desktop/Excluded/mbox)")
    p.add_argument("--db", default=str(DEFAULT_DB_PATH), help="SQLite DB path")
    p.add_argument("--attachments", default=str(DEFAULT_ATTACHMENT_DIR),
                   help="attachment CAS directory")
    p.add_argument("--no-attachments", action="store_true",
                   help="skip attachment extraction (headers/body only)")
    p.add_argument("--force", action="store_true",
                   help="reindex mboxes even if already marked complete")
    p.add_argument("--status", action="store_true",
                   help="print per-mbox progress and exit")
    p.add_argument("--parallel", type=int, default=0, metavar="N",
                   help="Worker count: 0=auto (default), 1=single-threaded, N=explicit. "
                        "Auto picks min(16, cpu_count//2) when total size >= 100 MB.")
    p.add_argument("--no-fts", action="store_true",
                   help="Skip the final FTS5 rebuild (debug/bench only — search won't work)")
    args = p.parse_args(argv)

    db_path = Path(args.db)

    if args.status:
        print_status(db_path)
        return 0

    target = Path(args.target)
    mbox_files = collect_mbox_files(target)
    if not mbox_files:
        print(f"No .mbox files found at {target}", file=sys.stderr)
        return 2

    attachment_dir = Path(args.attachments)
    extract_atts = not args.no_attachments
    total_size = sum(f.stat().st_size for f in mbox_files)

    # Auto-pick worker count
    workers = args.parallel
    if workers == 0:
        cpu = os.cpu_count() or 4
        if total_size < 100 * 1024 * 1024 and len(mbox_files) <= 2:
            workers = 1
        else:
            workers = min(16, max(2, cpu // 2))

    print(f"DB:             {db_path}")
    print(f"Attachments:    {attachment_dir if extract_atts else '(disabled)'}")
    print(f"Files to index: {len(mbox_files)}")
    print(f"Total size:     {format_bytes(total_size)}")
    print(f"Workers:        {workers}")
    print()

    # Parallel path (best for >100 MB or many files)
    if workers > 1:
        from .parallel import run_parallel
        rc = run_parallel(
            mbox_files=mbox_files,
            db_path=db_path,
            attachment_dir=attachment_dir,
            workers=workers,
            extract_attachments=extract_atts,
            force=args.force,
            build_fts=not args.no_fts,
        )
        if rc == 0:
            print()
            print_status(db_path)
        return rc

    # Single-threaded path — still uses bulk pragmas + deferred FTS5 rebuild
    # for a ~1.5-2× speedup over the naive per-message FTS5 insert path.
    import time as _time
    t_parse_start = _time.time()
    conn = connect(db_path)
    store.apply_bulk_pragmas(conn)
    try:
        for i, mbox in enumerate(mbox_files, 1):
            print(f"[{i}/{len(mbox_files)}] {mbox.name}")
            index_mbox_file(
                conn, mbox,
                extract_attachments=extract_atts,
                attachment_dir=attachment_dir,
                force=args.force,
                fts_sync=False,
            )
        t_parse = _time.time() - t_parse_start

        if not args.no_fts:
            print()
            print("[fts] bulk-rebuilding messages_fts from master...")
            t_fts_start = _time.time()
            count = store.rebuild_fts(conn)
            t_fts = _time.time() - t_fts_start
            print(f"  [fts done] {count:,} rows in {t_fts:.1f}s")

        print()
        print("[finalize] ANALYZE + VACUUM + WAL restore...")
        t_fin_start = _time.time()
        store.finalize_db(conn)
        t_fin = _time.time() - t_fin_start
        print(f"  [finalize done] in {t_fin:.1f}s")
    finally:
        conn.close()

    print()
    print_status(db_path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
