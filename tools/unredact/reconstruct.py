"""Cross-file source matcher for redacted documents.

Uses the SAME `tools/mbox/`-style architecture: SQLite FTS5 + trigram tokenizer
+ unified index + bulk rebuild. Indexes every `.recovered.txt` file from:

    * tools/unredact/diagnose.py output (binder diagnostic)
    * tools/unredact/bulk.py output  (mbox attachment sweep + Case_Reference sweep)

Also pulls in:
    * mbox_index.db — all email bodies for text matching
    * .cache/extracted/* — PDF/DOCX extracted text
    * .cache/transcribed/* — audio transcripts

For each target PDF+page, searches the index for matching content in OTHER files,
to find alternate (less-redacted or unredacted) copies of the same document.

This is the strongest "unredact" lever because it doesn't try to recover pixels —
it finds the alternate source that was never redacted in the first place.

Usage:
    python -m tools.unredact.reconstruct build    # rebuild index from all sources
    python -m tools.unredact.reconstruct match <pdf>    # find matches for each page
    python -m tools.unredact.reconstruct match <pdf> --page 234
    python -m tools.unredact.reconstruct stats    # show index contents
    python -m tools.unredact.reconstruct search "<query>"    # ad-hoc FTS query

Index location: .cache/unredact/reconstruct.db
"""

from __future__ import annotations

import argparse
import hashlib
import re
import sqlite3
import sys
import time
from pathlib import Path

if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_DB = REPO_ROOT / ".cache" / "unredact" / "reconstruct.db"

# Sources to ingest
UNREDACT_OUTPUT_DIR = Path(r"C:\Users\atayl\Desktop\Excluded\unredact")
MBOX_DB = Path(r"C:\Users\atayl\Desktop\Excluded\mbox\mbox_index.db")
EXTRACTED_CACHE = REPO_ROOT / ".cache" / "extracted"
TRANSCRIBED_CACHE = REPO_ROOT / ".cache" / "transcribed"


SCHEMA = """
PRAGMA journal_mode=WAL;
PRAGMA synchronous=NORMAL;
PRAGMA temp_store=MEMORY;
PRAGMA mmap_size=30000000000;
PRAGMA cache_size=-2000000;
PRAGMA page_size=8192;

CREATE TABLE IF NOT EXISTS docs (
    id           INTEGER PRIMARY KEY,
    source_type  TEXT NOT NULL,   -- 'binder_recovered', 'mbox_recovered', 'case_recovered', 'mbox_email', 'extracted', 'transcribed'
    source_path  TEXT NOT NULL,   -- canonical path to the source file (or mbox message #)
    label        TEXT,             -- human-readable label
    page_num     INTEGER,          -- page number if applicable (NULL for emails)
    content_hash TEXT NOT NULL,    -- sha256 of text content for dedup
    char_count   INTEGER,
    added_at     INTEGER
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_docs_hash_src ON docs(content_hash, source_path, page_num);
CREATE INDEX IF NOT EXISTS idx_docs_source_type ON docs(source_type);
CREATE INDEX IF NOT EXISTS idx_docs_source_path ON docs(source_path);

CREATE VIRTUAL TABLE IF NOT EXISTS docs_fts USING fts5(
    label,
    source_path,
    content,
    content='',
    tokenize='trigram'
);

CREATE TABLE IF NOT EXISTS meta (
    key TEXT PRIMARY KEY,
    value TEXT
);
"""


def connect(db_path: Path = DEFAULT_DB) -> sqlite3.Connection:
    db_path.parent.mkdir(parents=True, exist_ok=True)
    conn = sqlite3.connect(str(db_path))
    conn.row_factory = sqlite3.Row
    conn.executescript(SCHEMA)
    return conn


def content_hash(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8", "replace")).hexdigest()


# =========================================================================
# Source ingesters
# =========================================================================


def iter_recovered_text_files() -> list[tuple[str, Path, str]]:
    """Yield (source_type, path, label) for every recovered.txt under the unredact outputs."""
    results: list[tuple[str, Path, str]] = []
    if not UNREDACT_OUTPUT_DIR.exists():
        return results

    for txt in UNREDACT_OUTPUT_DIR.rglob("*.recovered.txt"):
        # Classify by parent folder name
        parts = txt.relative_to(UNREDACT_OUTPUT_DIR).parts
        if not parts:
            continue
        top = parts[0].lower()
        if top == "output":
            source_type = "binder_recovered"  # direct diagnostic output
        elif top == "bulk_mbox":
            source_type = "mbox_recovered"
        elif top == "bulk_case":
            source_type = "case_recovered"
        else:
            source_type = "unknown_recovered"
        label = txt.stem.replace(".recovered", "")
        results.append((source_type, txt, label))
    return results


def iter_mbox_messages(limit: int | None = None):
    """Yield (message_id, from_addr, subject, body_text, source_mbox) from mbox_index.db."""
    if not MBOX_DB.exists():
        return
    conn = sqlite3.connect(str(MBOX_DB))
    conn.row_factory = sqlite3.Row
    q = (
        "SELECT id, message_id, from_addr, subject, body_text, source_mbox, date_sent "
        "FROM messages WHERE has_body = 1"
    )
    if limit:
        q += f" LIMIT {limit}"
    for row in conn.execute(q):
        yield row
    conn.close()


def iter_extracted_cache():
    """Yield (path, label, text) for all extracted PDF/DOCX/EML text files."""
    if not EXTRACTED_CACHE.exists():
        return
    for txt in EXTRACTED_CACHE.rglob("*.txt"):
        try:
            text = txt.read_text(encoding="utf-8", errors="replace")
        except Exception:
            continue
        if text.strip():
            yield txt, txt.stem, text


def iter_transcribed_cache():
    """Yield (path, label, text) for all transcribed audio text files."""
    if not TRANSCRIBED_CACHE.exists():
        return
    for txt in TRANSCRIBED_CACHE.rglob("*.txt"):
        try:
            text = txt.read_text(encoding="utf-8", errors="replace")
        except Exception:
            continue
        if text.strip():
            yield txt, txt.stem, text


# =========================================================================
# Page splitter for recovered text (which uses "=== Page N ===" separators)
# =========================================================================


PAGE_SEP_RE = re.compile(r"^===\s*Page\s+(\d+)\s*===\s*$", re.MULTILINE)


def split_recovered_by_page(text: str) -> list[tuple[int, str]]:
    """Split a recovered.txt into (page_num, text) tuples.

    The diagnose.py dump format uses ``=== Page N ===`` separators followed by the
    page text. If the file has no separators, return it as a single page 0 block.
    """
    out: list[tuple[int, str]] = []
    # Find all separator match positions
    matches = list(PAGE_SEP_RE.finditer(text))
    if not matches:
        return [(0, text.strip())] if text.strip() else []
    for i, m in enumerate(matches):
        page = int(m.group(1))
        start = m.end()
        end = matches[i + 1].start() if i + 1 < len(matches) else len(text)
        chunk = text[start:end].strip()
        if chunk:
            out.append((page, chunk))
    return out


# =========================================================================
# Indexing
# =========================================================================


def insert_doc(conn: sqlite3.Connection, source_type: str, source_path: str,
               label: str, page_num: int | None, text: str) -> int:
    text = (text or "").strip()
    if len(text) < 20:
        return 0
    h = content_hash(text)
    try:
        cur = conn.execute(
            "INSERT OR IGNORE INTO docs (source_type, source_path, label, page_num, content_hash, char_count, added_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?)",
            (source_type, source_path, label, page_num, h, len(text), int(time.time())),
        )
        if cur.rowcount == 0:
            return 0
        rowid = cur.lastrowid
        conn.execute(
            "INSERT INTO docs_fts (rowid, label, source_path, content) VALUES (?, ?, ?, ?)",
            (rowid, label or "", source_path, text),
        )
        return 1
    except sqlite3.DatabaseError:
        return 0


def build_index(db_path: Path = DEFAULT_DB, rebuild: bool = False) -> dict:
    """Ingest every known source into the reconstruction index."""
    if rebuild and db_path.exists():
        db_path.unlink()

    conn = connect(db_path)
    stats = {
        "binder_recovered": 0,
        "mbox_recovered": 0,
        "case_recovered": 0,
        "mbox_email": 0,
        "extracted": 0,
        "transcribed": 0,
    }

    # 1. Recovered-text files from diagnose + bulk sweeps
    print("[1/4] recovered.txt files...")
    for source_type, txt_path, label in iter_recovered_text_files():
        try:
            text = txt_path.read_text(encoding="utf-8", errors="replace")
        except Exception:
            continue
        pages = split_recovered_by_page(text)
        for page_num, chunk in pages:
            added = insert_doc(conn, source_type, str(txt_path), label, page_num, chunk)
            stats[source_type] += added
    conn.commit()
    print(f"   binder_recovered: {stats['binder_recovered']}")
    print(f"   mbox_recovered:   {stats['mbox_recovered']}")
    print(f"   case_recovered:   {stats['case_recovered']}")

    # 2. mbox emails — every email body becomes a searchable doc
    print("[2/4] mbox email bodies...")
    if MBOX_DB.exists():
        for row in iter_mbox_messages():
            body = row["body_text"] or ""
            if not body.strip():
                continue
            label = f"{row['subject'] or '(no subject)'} — {row['from_addr'] or ''}"
            source_path = f"mbox://{row['id']}"
            added = insert_doc(conn, "mbox_email", source_path, label[:200], None, body)
            stats["mbox_email"] += added
    conn.commit()
    print(f"   mbox_email: {stats['mbox_email']}")

    # 3. Extracted PDF/DOCX text from .cache/extracted
    print("[3/4] extracted cache...")
    for path, label, text in iter_extracted_cache():
        added = insert_doc(conn, "extracted", str(path), label, None, text)
        stats["extracted"] += added
    conn.commit()
    print(f"   extracted: {stats['extracted']}")

    # 4. Audio transcripts
    print("[4/4] transcribed cache...")
    for path, label, text in iter_transcribed_cache():
        added = insert_doc(conn, "transcribed", str(path), label, None, text)
        stats["transcribed"] += added
    conn.commit()
    print(f"   transcribed: {stats['transcribed']}")

    conn.execute("INSERT INTO docs_fts(docs_fts) VALUES('optimize')")
    conn.commit()
    conn.close()
    return stats


# =========================================================================
# Matching
# =========================================================================


def extract_search_phrases(text: str, max_phrases: int = 3,
                           min_len: int = 40, max_len: int = 200) -> list[str]:
    """Pick the most distinctive phrases from a chunk of recovered text.

    Prefers sentences with uncommon word patterns (longer, more unique).
    Skips generic boilerplate lines.
    """
    if not text:
        return []
    # Split into candidate lines/sentences
    parts = re.split(r"[\n\r]+|(?<=[.!?])\s{2,}", text)
    candidates = []
    boilerplate_markers = (
        "CUI", "Privacy Act", "UNCLASSIFIED", "Page", "From:", "To:", "Cc:", "Subject:",
        "Sent:", "Date:", "Attachments:",
    )
    for p in parts:
        p = p.strip()
        if len(p) < min_len or len(p) > max_len:
            continue
        if any(p.startswith(m) for m in boilerplate_markers):
            continue
        if not any(c.isalpha() for c in p):
            continue
        candidates.append(p)

    # Score by length (longer = more specific) + number of unique words
    def score(s: str) -> int:
        words = set(s.lower().split())
        return len(s) + len(words) * 3

    candidates.sort(key=score, reverse=True)
    return candidates[:max_phrases]


def fts_escape(query: str) -> str:
    """Escape an FTS5 query string to treat it as a literal phrase."""
    # Strip chars that break FTS5 parsing, wrap in quotes
    clean = re.sub(r'[\"\'\(\)\[\]\{\}:]', " ", query)
    clean = re.sub(r"\s+", " ", clean).strip()
    return f'"{clean}"' if clean else ""


def find_matches(conn: sqlite3.Connection, target_text: str,
                 max_results: int = 10, exclude_path: str | None = None,
                 exclude_content_hash: str | None = None,
                 target_char_count: int | None = None) -> list[dict]:
    """Find other documents in the index matching the target text.

    Excludes self-matches by both source path and content hash. Ranks by
    how much MORE visible content the match has than the target, since the
    whole point is to find less-redacted alternate sources.
    """
    phrases = extract_search_phrases(target_text)
    if not phrases:
        return []

    # Exclude self by content hash (the same text chunk indexed from multiple
    # bulk_case and bulk_mbox sweeps will share a content_hash).
    target_hash = content_hash(target_text.strip())

    all_hits: dict[int, dict] = {}
    for phrase in phrases:
        q = fts_escape(phrase)
        if not q:
            continue
        try:
            rows = conn.execute(
                """
                SELECT d.id, d.source_type, d.source_path, d.label, d.page_num,
                       d.char_count, d.content_hash, rank
                FROM docs d
                JOIN docs_fts f ON f.rowid = d.id
                WHERE docs_fts MATCH ?
                ORDER BY rank LIMIT ?
                """,
                (q, max_results * 3),  # over-fetch since we'll filter
            ).fetchall()
        except sqlite3.DatabaseError:
            continue
        for r in rows:
            # Filter 1: skip exact path match (same file re-indexed under bulk)
            if exclude_path and str(r["source_path"]) == exclude_path:
                continue
            # Filter 2: skip exact content hash match (same content, different path)
            if r["content_hash"] == target_hash:
                continue
            if exclude_content_hash and r["content_hash"] == exclude_content_hash:
                continue
            # Filter 3: skip matches within the same binder file that came
            # from a different "copy" of the bulk sweep
            src = str(r["source_path"])
            if exclude_path:
                ex_stem = Path(exclude_path).stem.split(".recovered")[0]
                src_stem = Path(src).stem.split(".recovered")[0]
                if ex_stem == src_stem:
                    continue

            key = r["id"]
            if key not in all_hits:
                match_chars = r["char_count"] or 0
                delta = match_chars - (target_char_count or 0)
                all_hits[key] = {
                    "id": r["id"],
                    "source_type": r["source_type"],
                    "source_path": src,
                    "label": r["label"],
                    "page_num": r["page_num"],
                    "char_count": match_chars,
                    "char_delta": delta,
                    "matched_phrases": [],
                    "content_hash": r["content_hash"],
                }
            all_hits[key]["matched_phrases"].append(phrase[:80])

    # Rank: prefer matches with MORE visible content than the target, then by
    # number of matching phrases, then by absolute char count.
    ranked = sorted(
        all_hits.values(),
        key=lambda h: (
            -max(h["char_delta"], 0),                  # positive delta wins
            -len(h["matched_phrases"]),
            -(h["char_count"] or 0),
        ),
    )
    return ranked[:max_results]


def get_doc_content(conn: sqlite3.Connection, doc_id: int) -> str:
    row = conn.execute(
        "SELECT content FROM docs_fts WHERE rowid = ?", (doc_id,)
    ).fetchone()
    return row["content"] if row else ""


# =========================================================================
# Target PDF page-by-page matching
# =========================================================================


def match_pdf(pdf_path: Path, db_path: Path = DEFAULT_DB,
              output_dir: Path | None = None,
              only_page: int | None = None,
              max_matches_per_page: int = 5) -> dict:
    """For each page of ``pdf_path`` that has recoverable text, find cross-file matches."""
    import fitz
    conn = connect(db_path)

    if output_dir is None:
        output_dir = UNREDACT_OUTPUT_DIR / "reconstruct" / pdf_path.stem
    output_dir.mkdir(parents=True, exist_ok=True)

    try:
        doc = fitz.open(str(pdf_path))
    except Exception as e:
        return {"error": f"open failed: {e}"}

    total_pages = len(doc)
    pages_with_matches = 0
    total_matches = 0
    page_results: list[dict] = []

    for i in range(total_pages):
        if only_page is not None and (i + 1) != only_page:
            continue
        page = doc[i]
        text = (page.get_text("text") or "").strip()
        if len(text) < 100:
            continue

        matches = find_matches(
            conn, text, max_results=max_matches_per_page,
            exclude_path=str(pdf_path),
            target_char_count=len(text),
        )
        if matches:
            pages_with_matches += 1
            total_matches += len(matches)
            page_results.append({
                "page": i + 1,
                "chars": len(text),
                "matches": matches,
            })

    doc.close()

    # Write summary
    summary_lines = [
        f"# Reconstruction matches for `{pdf_path.name}`",
        "",
        f"- Total pages:           {total_pages}",
        f"- Pages with text:       {sum(1 for p in page_results) + 0}",
        f"- Pages with matches:    {pages_with_matches}",
        f"- Total match records:   {total_matches}",
        "",
    ]

    # Only show pages with cross-file matches that add content
    useful_pages = [r for r in page_results if any(m["char_delta"] > 0 for m in r["matches"])]
    summary_lines.append(f"- Pages with DELTA > 0 (better sources found): {len(useful_pages)}")
    summary_lines.append("")

    for r in page_results:
        better = [m for m in r["matches"] if m["char_delta"] > 0]
        if not better and not r["matches"]:
            continue
        flag = " 🎯" if better else ""
        summary_lines.append(f"## Page {r['page']} ({r['chars']:,} chars of recoverable text){flag}")
        summary_lines.append("")
        # Sort matches: positive delta first
        sorted_matches = sorted(r["matches"], key=lambda m: -m["char_delta"])
        for i, m in enumerate(sorted_matches, 1):
            delta_str = f"{m['char_delta']:+,}"
            if m["char_delta"] > 0:
                delta_str = f"**{delta_str}**"
            summary_lines.append(
                f"{i}. **{m['source_type']}** · `{m['label'][:80]}` "
                f"(page {m['page_num'] or '-'}, {m['char_count']:,} chars, "
                f"Δ {delta_str}, {len(m['matched_phrases'])} phrase hits)"
            )
            summary_lines.append(f"   `{m['source_path']}`")
        summary_lines.append("")

    out_path = output_dir / f"{pdf_path.stem}.matches.md"
    out_path.write_text("\n".join(summary_lines), encoding="utf-8")

    # Also dump full matched source texts for the top match per page
    for r in page_results:
        if not r["matches"]:
            continue
        top = r["matches"][0]
        content = get_doc_content(conn, top["id"])
        if content:
            page_out = output_dir / f"page_{r['page']:04d}.matched_source.txt"
            header = (
                f"Source: {top['source_type']} — {top['label']}\n"
                f"Path:   {top['source_path']}\n"
                f"Chars:  {top['char_count']}\n"
                f"Matched phrases: {len(top['matched_phrases'])}\n"
                + "-" * 60 + "\n\n"
            )
            page_out.write_text(header + content, encoding="utf-8")

    conn.close()
    return {
        "pdf": str(pdf_path),
        "total_pages": total_pages,
        "pages_with_matches": pages_with_matches,
        "total_matches": total_matches,
        "summary": str(out_path),
        "output_dir": str(output_dir),
    }


# =========================================================================
# CLI
# =========================================================================


def cmd_build(args) -> int:
    t0 = time.time()
    stats = build_index(Path(args.db), rebuild=args.force)
    elapsed = time.time() - t0
    total = sum(stats.values())
    print()
    print(f"Built in {elapsed:.1f}s: {total} unique documents indexed")
    for k, v in stats.items():
        print(f"  {k:<20} {v:>6}")
    return 0


def cmd_match(args) -> int:
    pdf = Path(args.pdf).resolve()
    if not pdf.exists():
        print(f"ERROR: {pdf} not found", file=sys.stderr)
        return 2
    r = match_pdf(pdf, Path(args.db), only_page=args.page,
                  max_matches_per_page=args.max_per_page)
    if "error" in r:
        print(f"ERROR: {r['error']}", file=sys.stderr)
        return 3
    print(f"PDF:              {r['pdf']}")
    print(f"Total pages:      {r['total_pages']}")
    print(f"Pages w/ matches: {r['pages_with_matches']}")
    print(f"Total matches:    {r['total_matches']}")
    print(f"Summary:          {r['summary']}")
    print(f"Output:           {r['output_dir']}")
    return 0


def cmd_stats(args) -> int:
    conn = connect(Path(args.db))
    for row in conn.execute(
        "SELECT source_type, COUNT(*) as n, SUM(char_count) as total_chars FROM docs GROUP BY source_type"
    ):
        print(f"{row['source_type']:<22} {row['n']:>7}  chars={row['total_chars'] or 0:>12,}")
    total = conn.execute("SELECT COUNT(*) FROM docs").fetchone()[0]
    print(f"{'TOTAL':<22} {total:>7}")
    conn.close()
    return 0


def cmd_search(args) -> int:
    conn = connect(Path(args.db))
    q = fts_escape(args.query)
    rows = conn.execute(
        """
        SELECT d.source_type, d.source_path, d.label, d.page_num, d.char_count, rank
        FROM docs d JOIN docs_fts f ON f.rowid = d.id
        WHERE docs_fts MATCH ? ORDER BY rank LIMIT ?
        """,
        (q, args.limit),
    ).fetchall()
    if not rows:
        print("No results.")
        return 0
    for i, r in enumerate(rows, 1):
        print(f"{i}. [{r['source_type']}] {r['label'][:80]}")
        print(f"   {r['source_path']}  (page {r['page_num'] or '-'}, {r['char_count'] or 0:,} chars)")
    conn.close()
    return 0


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="python -m tools.unredact.reconstruct")
    p.add_argument("--db", default=str(DEFAULT_DB), help="reconstruction DB path")

    sp = p.add_subparsers(dest="cmd", required=True)

    bp = sp.add_parser("build", help="Build / rebuild the reconstruction index")
    bp.add_argument("--force", action="store_true", help="Drop + rebuild from scratch")
    bp.set_defaults(func=cmd_build)

    mp = sp.add_parser("match", help="Find cross-file matches for each page of a PDF")
    mp.add_argument("pdf")
    mp.add_argument("--page", type=int, default=None)
    mp.add_argument("--max-per-page", type=int, default=5)
    mp.set_defaults(func=cmd_match)

    stp = sp.add_parser("stats", help="Show index contents")
    stp.set_defaults(func=cmd_stats)

    srp = sp.add_parser("search", help="Ad-hoc FTS5 query")
    srp.add_argument("query")
    srp.add_argument("--limit", type=int, default=20)
    srp.set_defaults(func=cmd_search)

    args = p.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
