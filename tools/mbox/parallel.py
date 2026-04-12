"""Parallel mbox indexer — ProcessPoolExecutor with intra-file byte-range split.

Strategy:
    1. Plan chunks — large mboxes split into byte ranges (one worker per range),
       small mboxes get a whole-file chunk.
    2. Spawn N workers. Each writes a partial SQLite DB (schema identical to
       master; no FTS5 writes during parsing).
    3. Merge partial DBs into the master serially via `ATTACH DATABASE` +
       `INSERT OR IGNORE`. Dedup handled by the UNIQUE(message_id) constraint.
    4. Bulk-rebuild messages_fts from master in a single pass.
    5. ANALYZE + VACUUM + restore WAL.

Parallel safety:
    * Each worker owns its partial DB file — zero SQLite write contention.
    * The attachment CAS store tolerates concurrent writes via atomic
      .tmp+rename — same hash collides to same final path with identical bytes.
    * Master DB is only touched during the serial merge phase.

Merge trick:
    Partial DBs have their own auto-increment row IDs. Message-to-attachment
    links use those per-partial IDs. At merge time we JOIN through the RFC
    Message-ID (which IS stable across partials) to rewrite the link table
    into master row IDs.
"""

from __future__ import annotations

import multiprocessing as mp
import os
import shutil
import sys
import tempfile
import time
import traceback
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path

# Force UTF-8 on Windows stdio so worker output (decoded MIME subjects, emoji)
# doesn't crash cp1252.
if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass


# =========================================================================
# Byte-range mbox iterator (sibling of store.iter_mbox)
# =========================================================================


def iter_mbox_range(path: Path, start: int, end: int):
    """Yield (abs_byte_offset, raw_message_bytes) for messages whose ``From ``
    separator line falls in the half-open byte range [start, end).

    Guarantees each message is processed by exactly one worker: the one whose
    range contains the ``From `` line position. No message is dropped at chunk
    boundaries, and none is double-counted.
    """
    with open(path, "rb") as f:
        if start > 0:
            # Seek to start, discard mid-line bytes, scan forward to the next
            # 'From ' line. That line is the first message whose ownership
            # belongs to this chunk.
            f.seek(start)
            f.readline()  # potentially partial, drop
            while True:
                pos = f.tell()
                line = f.readline()
                if not line:
                    return
                if line.startswith(b"From "):
                    f.seek(pos)
                    break
        else:
            f.seek(0)

        buf = bytearray()
        msg_start = f.tell()
        first_from_seen = False

        while True:
            pos = f.tell()
            line = f.readline()
            if not line:
                break
            if line.startswith(b"From "):
                if buf:
                    yield msg_start, bytes(buf)
                    buf.clear()
                    # The NEW 'From ' is at `pos`. If that's past our end,
                    # stop: the next worker owns this message.
                    if pos >= end:
                        return
                msg_start = pos
                first_from_seen = True
                continue
            buf.extend(line)

        # Flush the last message in the chunk.
        if buf and msg_start < end:
            yield msg_start, bytes(buf)


# =========================================================================
# Chunk planning
# =========================================================================


def plan_chunks(mbox_files: list[Path], workers: int,
                min_chunk_mb: int = 50, target_chunks_per_file: int | None = None) -> list[dict]:
    """Split files into chunks for parallel processing.

    For each file: target ~``workers`` chunks, but not smaller than
    ``min_chunk_mb``. This keeps every worker busy on the long-pole files
    (e.g. the 2.5 GB Archived-005) instead of letting half the pool idle.

    Files smaller than ``min_chunk_mb`` get a single whole-file chunk.
    Sorted largest-first so the pool gets the long poles started early.
    """
    if target_chunks_per_file is None:
        target_chunks_per_file = workers

    chunks: list[dict] = []
    for path in sorted(mbox_files, key=lambda p: -p.stat().st_size):
        size = path.stat().st_size
        if size == 0:
            continue
        if size > min_chunk_mb * 1024 * 1024 and workers > 1:
            # Aim for target_chunks_per_file, but cap so no chunk goes below min_chunk_mb.
            max_chunks_by_size = max(2, size // (min_chunk_mb * 1024 * 1024))
            n = min(target_chunks_per_file, max_chunks_by_size)
            chunk_size = size // n
            for i in range(n):
                start = i * chunk_size
                end = size if i == n - 1 else (i + 1) * chunk_size
                chunks.append({
                    "mbox_path": str(path),
                    "start": start,
                    "end": end,
                    "chunk_idx": i,
                    "total_chunks": n,
                })
        else:
            chunks.append({
                "mbox_path": str(path),
                "start": 0,
                "end": size,
                "chunk_idx": 0,
                "total_chunks": 1,
            })
    return chunks


# =========================================================================
# Worker entry point — runs in a forked/spawned subprocess
# =========================================================================


def worker_index_chunk(task: dict) -> dict:
    """Process one chunk into a partial SQLite DB. Runs in a subprocess."""
    # Imports inside the worker function so Windows spawn-based multiprocessing
    # doesn't re-run module-level side effects of the whole tools.mbox package.
    from pathlib import Path as _P

    from . import store as _store
    from .index import process_message as _process

    mbox_path = task["mbox_path"]
    start = task["start"]
    end = task["end"]
    chunk_idx = task["chunk_idx"]
    extract_attachments = task["extract_attachments"]
    attachment_dir = _P(task["attachment_dir"])
    partial_dir = _P(task["partial_dir"])

    partial_dir.mkdir(parents=True, exist_ok=True)
    safe_stem = _P(mbox_path).stem.replace(" ", "_").replace("/", "_")
    partial_db = partial_dir / f"partial_{os.getpid()}_{safe_stem}_{chunk_idx}.db"
    if partial_db.exists():
        partial_db.unlink()

    conn = _store.connect(partial_db)
    _store.apply_bulk_pragmas(conn)

    stats = {
        "processed": 0, "new": 0, "duplicate": 0,
        "atts_new": 0, "atts_dup": 0, "errors": 0,
        "last_error": None,
        "wall_seconds": 0.0,
    }

    t0 = time.time()
    try:
        for offset, raw in iter_mbox_range(_P(mbox_path), start, end):
            res = _process(
                conn, raw, mbox_path, offset,
                extract_attachments=extract_attachments,
                attachment_dir=attachment_dir,
                fts_sync=False,  # deferred — main process rebuilds in bulk
            )
            stats["processed"] += 1
            if res["status"] == "new":
                stats["new"] += 1
                stats["atts_new"] += res["atts_new"]
                stats["atts_dup"] += res["atts_dup"]
            elif res["status"] == "duplicate":
                stats["duplicate"] += 1
            else:
                stats["errors"] += 1
                if res.get("error") and not stats["last_error"]:
                    stats["last_error"] = str(res["error"])[:300]

            if stats["processed"] % 500 == 0:
                conn.commit()

        conn.commit()
    except Exception as e:
        stats["errors"] += 1
        stats["last_error"] = f"{type(e).__name__}: {e}\n{traceback.format_exc()[:500]}"
    finally:
        try:
            conn.close()
        except Exception:
            pass

    stats["wall_seconds"] = time.time() - t0

    return {
        "partial_db": str(partial_db),
        "mbox_path": mbox_path,
        "chunk_idx": chunk_idx,
        "total_chunks": task["total_chunks"],
        "byte_range": (start, end),
        "stats": stats,
    }


# =========================================================================
# Merge phase — serialized on the master DB
# =========================================================================


def merge_partials(master_conn, partial_dbs: list[str]) -> dict:
    """Serial merge of partial DBs into master without ATTACH.

    Reads rows from each partial via a separate connection and inserts into
    master via executemany(). Uses Python's default transaction mode
    (isolation_level=DEFERRED) with explicit commit() calls.

    Order: attachments (FK target) → messages (FK target for link table)
    → message_attachments (remapped via lookup on RFC message_id).
    """
    import sqlite3 as _sqlite3

    agg = {
        "attachments_inserted": 0,
        "messages_inserted": 0,
        "links_inserted": 0,
    }

    BATCH = 2000

    for partial in partial_dbs:
        if not Path(partial).exists():
            continue

        try:
            pconn = _sqlite3.connect(partial)
            pconn.row_factory = _sqlite3.Row
        except _sqlite3.DatabaseError as e:
            print(f"  [merge warn] can't open {partial}: {e}")
            continue

        try:
            # ----- 1. attachments -----
            a_rows = pconn.execute(
                "SELECT sha256, filename, mime_type, size_bytes, stored_path, "
                "extracted_text, first_seen_ts FROM attachments"
            ).fetchall()
            if a_rows:
                before = master_conn.execute("SELECT COUNT(*) FROM attachments").fetchone()[0]
                master_conn.executemany(
                    """
                    INSERT OR IGNORE INTO attachments
                        (sha256, filename, mime_type, size_bytes, stored_path, extracted_text, first_seen_ts)
                    VALUES (?, ?, ?, ?, ?, ?, ?)
                    """,
                    [tuple(r) for r in a_rows],
                )
                master_conn.commit()
                after = master_conn.execute("SELECT COUNT(*) FROM attachments").fetchone()[0]
                agg["attachments_inserted"] += after - before

            # ----- 2. messages -----
            m_rows = pconn.execute(
                """
                SELECT id, message_id, body_sha256, source_mbox, source_offset,
                       date_sent, date_raw, from_addr, from_name,
                       to_addrs, cc_addrs, subject,
                       in_reply_to, references_hdr, labels,
                       size_bytes, attachment_count, has_body,
                       body_text, body_html_stripped
                FROM messages
                """
            ).fetchall()

            if m_rows:
                before = master_conn.execute("SELECT COUNT(*) FROM messages").fetchone()[0]
                master_conn.executemany(
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
                    [tuple(r[1:]) for r in m_rows],  # skip partial's id column
                )
                master_conn.commit()
                after = master_conn.execute("SELECT COUNT(*) FROM messages").fetchone()[0]
                agg["messages_inserted"] += after - before

            # Build partial_id → master_id mapping
            partial_id_to_master_id: dict[int, int] = {}
            if m_rows:
                partial_msg_ids = [r["message_id"] for r in m_rows]
                master_lookup: dict[str, int] = {}
                for chunk_start in range(0, len(partial_msg_ids), 500):
                    batch = partial_msg_ids[chunk_start:chunk_start + 500]
                    placeholders = ",".join("?" * len(batch))
                    for row in master_conn.execute(
                        f"SELECT id, message_id FROM messages WHERE message_id IN ({placeholders})",
                        batch,
                    ):
                        master_lookup[row[1]] = row[0]
                for r in m_rows:
                    if r["message_id"] in master_lookup:
                        partial_id_to_master_id[r["id"]] = master_lookup[r["message_id"]]

            # ----- 3. message_attachments -----
            link_rows = pconn.execute(
                "SELECT message_id, attachment_sha256, filename_as_sent FROM message_attachments"
            ).fetchall()

            remapped = []
            for r in link_rows:
                master_id = partial_id_to_master_id.get(r["message_id"])
                if master_id is None:
                    continue
                remapped.append((master_id, r["attachment_sha256"], r["filename_as_sent"]))

            if remapped:
                before = master_conn.execute("SELECT COUNT(*) FROM message_attachments").fetchone()[0]
                master_conn.executemany(
                    """
                    INSERT OR IGNORE INTO message_attachments
                        (message_id, attachment_sha256, filename_as_sent)
                    VALUES (?, ?, ?)
                    """,
                    remapped,
                )
                master_conn.commit()
                after = master_conn.execute("SELECT COUNT(*) FROM message_attachments").fetchone()[0]
                agg["links_inserted"] += after - before

        finally:
            try:
                pconn.close()
            except Exception:
                pass

    return agg


# =========================================================================
# Main driver
# =========================================================================


def run_parallel(
    mbox_files: list[Path],
    db_path: Path,
    attachment_dir: Path,
    workers: int,
    extract_attachments: bool,
    force: bool = False,
    build_fts: bool = True,
) -> int:
    """Parallel indexing entry point. Returns 0 on success."""
    from . import store

    t_total_start = time.time()

    chunks = plan_chunks(mbox_files, workers)
    total_bytes = sum(c["end"] - c["start"] for c in chunks)
    total_files = len({c["mbox_path"] for c in chunks})

    print(f"[parallel] {workers} workers, {len(chunks)} chunks across {total_files} files "
          f"({store.format_bytes(total_bytes)} total)")
    big_files = [c for c in chunks if c["total_chunks"] > 1]
    if big_files:
        split_names = sorted({(Path(c["mbox_path"]).name, c["total_chunks"]) for c in big_files})
        for name, n in split_names:
            print(f"  split: {name} → {n} chunks")
    print()

    # Initialise master DB (ensures schema exists)
    master_conn = store.connect(db_path)
    store.apply_bulk_pragmas(master_conn)

    if force:
        print("  [force] wiping existing data...")
        # contentless FTS5 tables don't support DELETE; drop+recreate via rebuild_fts later.
        # attachments_fts follows the same rule.
        for t in ("message_attachments", "attachments", "messages", "ingest_progress"):
            try:
                master_conn.execute(f"DELETE FROM {t}")
            except Exception as e:
                print(f"  [force warn] {t}: {e}")
        try:
            master_conn.execute("DROP TABLE IF EXISTS messages_fts")
            master_conn.execute("DROP TABLE IF EXISTS attachments_fts")
        except Exception as e:
            print(f"  [force warn] fts drop: {e}")
        master_conn.commit()
        # Recreate FTS tables so subsequent phases find them
        master_conn.execute(
            """
            CREATE VIRTUAL TABLE IF NOT EXISTS messages_fts USING fts5(
                subject, from_addr, from_name, to_addrs, body,
                content='', tokenize='trigram'
            )
            """
        )
        master_conn.execute(
            """
            CREATE VIRTUAL TABLE IF NOT EXISTS attachments_fts USING fts5(
                filename, extracted_text,
                content='', tokenize='trigram'
            )
            """
        )
        master_conn.commit()

    master_conn.close()

    # Temp dir for partial DBs (outside the repo, on the system temp drive)
    partial_dir = Path(tempfile.mkdtemp(prefix="mbox_parallel_"))

    # Inject runtime parameters into each task
    for c in chunks:
        c["extract_attachments"] = extract_attachments
        c["attachment_dir"] = str(attachment_dir)
        c["partial_dir"] = str(partial_dir)

    # -------------------------------------------------------------------
    # Phase 1: workers parse messages into partial DBs
    # -------------------------------------------------------------------
    results = []
    t_parse_start = time.time()

    try:
        with ProcessPoolExecutor(max_workers=workers) as ex:
            futures = {ex.submit(worker_index_chunk, c): c for c in chunks}
            done_count = 0
            for fut in as_completed(futures):
                c = futures[fut]
                done_count += 1
                try:
                    r = fut.result()
                except Exception as e:
                    print(f"  [worker ERROR] {Path(c['mbox_path']).name}#{c['chunk_idx']}: {e}")
                    traceback.print_exc()
                    continue
                results.append(r)
                s = r["stats"]
                name = Path(r["mbox_path"]).name
                tag = f"#{r['chunk_idx']}" if c["total_chunks"] > 1 else ""
                print(
                    f"  [{done_count:>3}/{len(chunks)}] {name}{tag}: "
                    f"{s['processed']:>6} msgs "
                    f"({s['new']:>5} new, {s['duplicate']:>5} dup), "
                    f"{s['atts_new']:>4} att-new, "
                    f"{s['wall_seconds']:5.1f}s"
                )
    except KeyboardInterrupt:
        print("\n[interrupt] shutting down workers...")
        return 130

    t_parse = time.time() - t_parse_start
    print()
    print(f"[parse] {len(results)} chunks done in {t_parse:.1f}s "
          f"({(total_bytes / t_parse / 1024 / 1024):.0f} MB/s effective)")

    # -------------------------------------------------------------------
    # Phase 2: merge partial DBs into master (serial)
    # -------------------------------------------------------------------
    print()
    print(f"[merge] combining {len(results)} partial DBs into master...")
    t_merge_start = time.time()

    partial_paths = [r["partial_db"] for r in results]
    # Master uses normal transaction mode (sqlite3 default isolation_level).
    # The merge no longer uses ATTACH, so we don't need autocommit.
    master_conn = store.connect(db_path)
    for pragma in (
        "PRAGMA mmap_size=30000000000",
        "PRAGMA cache_size=-4000000",
        "PRAGMA temp_store=MEMORY",
        "PRAGMA synchronous=NORMAL",
    ):
        try:
            master_conn.execute(pragma)
        except Exception:
            pass

    try:
        agg = merge_partials(master_conn, partial_paths)
        print(f"  +{agg['messages_inserted']:,} messages, "
              f"+{agg['attachments_inserted']:,} attachments, "
              f"+{agg['links_inserted']:,} links")
    except Exception as e:
        print(f"  [merge ERROR] {e}")
        traceback.print_exc()
        master_conn.close()
        return 3

    t_merge = time.time() - t_merge_start
    print(f"  [merge done] in {t_merge:.1f}s")

    # -------------------------------------------------------------------
    # Phase 3: bulk-rebuild FTS5 from master
    # -------------------------------------------------------------------
    t_fts = 0.0
    fts_count = 0
    if build_fts:
        print()
        print("[fts] bulk-rebuilding messages_fts from master...")
        t_fts_start = time.time()
        fts_count = store.rebuild_fts(master_conn)
        t_fts = time.time() - t_fts_start
        print(f"  [fts done] {fts_count:,} rows in {t_fts:.1f}s")

    # -------------------------------------------------------------------
    # Phase 4: finalize (ANALYZE + VACUUM + WAL checkpoint)
    # -------------------------------------------------------------------
    print()
    print("[finalize] ANALYZE + VACUUM + WAL checkpoint...")
    t_fin_start = time.time()
    try:
        master_conn.execute("ANALYZE")
        master_conn.commit()
    except Exception as e:
        print(f"  [analyze warn] {type(e).__name__}: {e}")
    try:
        master_conn.commit()
        master_conn.execute("VACUUM")
    except Exception as e:
        print(f"  [vacuum warn] {type(e).__name__}: {e}")
    try:
        master_conn.execute("PRAGMA wal_checkpoint(TRUNCATE)")
    except Exception as e:
        print(f"  [checkpoint warn] {type(e).__name__}: {e}")
    t_fin = time.time() - t_fin_start
    print(f"  [finalize done] in {t_fin:.1f}s")

    # Write per-file ingest_progress (aggregated from chunk results)
    file_stats: dict[str, dict] = {}
    for r in results:
        f = r["mbox_path"]
        if f not in file_stats:
            file_stats[f] = {
                "processed": 0, "new": 0, "duplicate": 0,
                "atts_new": 0, "atts_dup": 0, "errors": 0,
                "last_error": None,
            }
        s = r["stats"]
        for k in ("processed", "new", "duplicate", "atts_new", "atts_dup", "errors"):
            file_stats[f][k] += s[k]
        if s["last_error"] and not file_stats[f]["last_error"]:
            file_stats[f]["last_error"] = s["last_error"]

    now_ts = int(time.time())
    for f, s in file_stats.items():
        try:
            size = Path(f).stat().st_size
        except OSError:
            size = 0
        master_conn.execute(
            """
            INSERT OR REPLACE INTO ingest_progress (
                mbox_file, size_bytes, last_byte_offset,
                messages_processed, messages_new, messages_duplicate,
                attachments_extracted, attachments_deduped,
                started_at, updated_at, completed_at,
                error_count, last_error
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                f, size, size,
                s["processed"], s["new"], s["duplicate"],
                s["atts_new"], s["atts_dup"],
                int(t_total_start), now_ts, now_ts,
                s["errors"], s["last_error"],
            ),
        )
    master_conn.commit()

    # Final stats
    total_msgs = master_conn.execute("SELECT COUNT(*) FROM messages").fetchone()[0]
    total_atts = master_conn.execute("SELECT COUNT(*) FROM attachments").fetchone()[0]
    att_bytes = master_conn.execute("SELECT COALESCE(SUM(size_bytes), 0) FROM attachments").fetchone()[0]
    master_conn.close()

    # Cleanup partial DBs
    try:
        shutil.rmtree(partial_dir)
    except Exception as e:
        print(f"  [cleanup warn] couldn't remove {partial_dir}: {e}")

    t_total = time.time() - t_total_start
    effective_mbps = (total_bytes / t_total / 1024 / 1024) if t_total > 0 else 0

    print()
    print("=" * 64)
    print(f"Done in {t_total:.1f}s ({effective_mbps:.0f} MB/s effective)")
    print(f"  parse:    {t_parse:5.1f}s  ({workers} workers)")
    print(f"  merge:    {t_merge:5.1f}s")
    if build_fts:
        print(f"  fts:      {t_fts:5.1f}s  ({fts_count:,} rows)")
    print(f"  finalize: {t_fin:5.1f}s")
    print("=" * 64)
    print(f"Unique messages:    {total_msgs:,}")
    print(f"Unique attachments: {total_atts:,} ({store.format_bytes(att_bytes)})")
    return 0
