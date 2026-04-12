"""Parallel bulk sweep driver for the unredact diagnostic.

Runs tools.unredact.diagnose.diagnose_file across N workers on either:
  - a directory of PDFs, or
  - the PDF subset of the mbox attachment CAS store (via SQLite query)

Produces a unified _BULK_SUMMARY.md ranking every file by recoverability.

Usage:
    python -m tools.unredact.bulk <dir>
    python -m tools.unredact.bulk <dir> --parallel 16
    python -m tools.unredact.bulk --mbox-db <path> --output <dir>
    python -m tools.unredact.bulk --mbox-db <default> --parallel 24

Default mbox DB: C:/Users/atayl/Desktop/Excluded/mbox/mbox_index.db
"""

from __future__ import annotations

import argparse
import multiprocessing as mp
import os
import sqlite3
import sys
import time
import traceback
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path

# Force UTF-8 on Windows stdio.
if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

DEFAULT_MBOX_DB = Path(r"C:\Users\atayl\Desktop\Excluded\mbox\mbox_index.db")
DEFAULT_OUTPUT = Path(r"C:\Users\atayl\Desktop\Excluded\unredact\bulk_output")


def worker_diagnose(args: tuple) -> dict:
    """Worker entry point: run diagnose_file on one PDF."""
    from .diagnose import diagnose_file, DEFAULT_KNOWN_NAMES
    from pathlib import Path as _P

    pdf_path_str, output_dir_str, label = args
    pdf_path = _P(pdf_path_str)
    output_dir = _P(output_dir_str)

    if not pdf_path.exists():
        return {"path": pdf_path_str, "label": label, "error": "file not found"}

    # Verify it's actually a PDF (first 4 bytes == %PDF)
    try:
        with open(pdf_path, "rb") as f:
            header = f.read(5)
        if not header.startswith(b"%PDF"):
            return {"path": pdf_path_str, "label": label, "error": "not a PDF"}
    except Exception as e:
        return {"path": pdf_path_str, "label": label, "error": f"read failed: {e}"}

    try:
        # Per-file subdir so thousands of attachments don't collide
        sub = output_dir / label / (pdf_path.stem[:40] or "unnamed")
        sub.mkdir(parents=True, exist_ok=True)
        r = diagnose_file(pdf_path, sub, DEFAULT_KNOWN_NAMES, dump_text=True)
        r["label"] = label
        return r
    except Exception as e:
        return {
            "path": pdf_path_str,
            "label": label,
            "error": f"{type(e).__name__}: {e}\n{traceback.format_exc()[:500]}",
        }


def gather_from_dir(root: Path) -> list[tuple[str, str, str]]:
    """Return [(pdf_path, output_label, display_name), ...] from a directory."""
    tasks = []
    for pdf in sorted(root.rglob("*.pdf")):
        label = "fs"
        tasks.append((str(pdf), label, pdf.name))
    return tasks


def gather_from_mbox_db(db_path: Path) -> list[tuple[str, str, str]]:
    """Return PDF tasks by querying the mbox attachment table."""
    if not db_path.exists():
        raise FileNotFoundError(f"mbox DB not found: {db_path}")

    conn = sqlite3.connect(str(db_path))
    conn.row_factory = sqlite3.Row

    # Pull everything that looks PDF-ish: mime-type OR filename ends in .pdf
    rows = conn.execute(
        """
        SELECT sha256, filename, mime_type, stored_path, size_bytes
        FROM attachments
        WHERE mime_type LIKE '%pdf%'
           OR filename LIKE '%.pdf'
           OR filename LIKE '%.PDF'
        ORDER BY size_bytes DESC
        """
    ).fetchall()
    conn.close()

    tasks = []
    for r in rows:
        stored = r["stored_path"]
        if not stored:
            continue
        tasks.append((stored, "mbox", r["filename"] or r["sha256"][:16]))
    return tasks


def run_bulk(tasks: list[tuple[str, str, str]], output_dir: Path,
             workers: int) -> list[dict]:
    """Run diagnose_file across a pool of workers. Returns list of results."""
    output_dir.mkdir(parents=True, exist_ok=True)

    print(f"Workers:      {workers}")
    print(f"Output:       {output_dir}")
    print(f"PDFs to scan: {len(tasks)}")
    print()

    results: list[dict] = []
    t0 = time.time()

    # Pack output_dir into each task so workers don't need extra params
    worker_args = [(p, str(output_dir), label) for p, label, _ in tasks]

    with ProcessPoolExecutor(max_workers=workers) as ex:
        futures = {ex.submit(worker_diagnose, args): i for i, args in enumerate(worker_args)}
        done = 0
        for fut in as_completed(futures):
            idx = futures[fut]
            display = tasks[idx][2]
            try:
                r = fut.result()
            except Exception as e:
                r = {"path": tasks[idx][0], "label": tasks[idx][1], "error": str(e)}
            results.append(r)
            done += 1
            if done % 50 == 0 or done == len(tasks):
                elapsed = time.time() - t0
                rate = done / elapsed if elapsed > 0 else 0
                print(f"  [{done:>5}/{len(tasks)}] {rate:5.1f} files/s  | {display[:60]}")

    return results


def write_bulk_summary(results: list[dict], output_dir: Path,
                       source_label: str) -> Path:
    """Write a sorted _BULK_SUMMARY.md highlighting the best recovery candidates."""
    summary_path = output_dir / "_BULK_SUMMARY.md"

    # Filter and sort: recoverable pages first, then residue hits
    usable = [r for r in results if not r.get("error")]
    errored = [r for r in results if r.get("error")]

    recoverable = [r for r in usable if r.get("verdict_recoverable_pages", 0) > 0]
    recoverable.sort(key=lambda r: -r.get("verdict_recoverable_pages", 0))

    residue_only = [r for r in usable if r.get("verdict_recoverable_pages", 0) == 0
                    and r.get("residue_hits", 0) > 5]
    residue_only.sort(key=lambda r: -r.get("residue_hits", 0))

    total_recoverable_pages = sum(r.get("verdict_recoverable_pages", 0) for r in usable)
    total_pages = sum(r.get("verdict_total_pages", 0) for r in usable)

    lines = [
        f"# Bulk Unredact Sweep — `{source_label}`",
        "",
        f"- Source:                 {source_label}",
        f"- Files scanned:          {len(results)}",
        f"- Successful diagnostics: {len(usable)}",
        f"- Errored:                {len(errored)}",
        f"- Total pages:            {total_pages:,}",
        f"- **Recoverable pages (Category 1)**: **{total_recoverable_pages:,}**",
        f"- Files with recoverable content:     **{len(recoverable)}**",
        f"- Files with residue-only hits:       {len(residue_only)}",
        "",
        "---",
        "",
        "## Top recoverable files (Category 1 — text layer intact)",
        "",
    ]
    if recoverable:
        lines.append("| Rank | Recoverable Pages | Total Pages | Residue | File | Report |")
        lines.append("|-----:|------------------:|------------:|--------:|------|--------|")
        for i, r in enumerate(recoverable[:100], 1):
            fname = Path(r["path"]).name[:80]
            report_rel = ""
            if r.get("report"):
                try:
                    report_rel = Path(r["report"]).relative_to(output_dir).as_posix()
                except ValueError:
                    report_rel = Path(r["report"]).name
            lines.append(
                f"| {i} | **{r.get('verdict_recoverable_pages', 0)}** | "
                f"{r.get('verdict_total_pages', 0)} | {r.get('residue_hits', 0)} | "
                f"`{fname}` | [{report_rel.split('/')[-1] if report_rel else '-'}]({report_rel}) |"
            )
    else:
        lines.append("_None found — no files have surviving text under visible redactions._")

    lines.extend(["", "## Files with byte-residue hits only (no Category 1 recovery)", ""])
    if residue_only:
        lines.append("| Residue Hits | File |")
        lines.append("|-------------:|------|")
        for r in residue_only[:50]:
            fname = Path(r["path"]).name[:80]
            lines.append(f"| {r.get('residue_hits', 0)} | `{fname}` |")
    else:
        lines.append("_None._")

    if errored:
        lines.extend(["", "## Errors", ""])
        for r in errored[:20]:
            lines.append(f"- `{Path(r['path']).name}`: {r.get('error', 'unknown')[:200]}")
        if len(errored) > 20:
            lines.append(f"- ... and {len(errored) - 20} more")

    summary_path.write_text("\n".join(lines), encoding="utf-8")
    return summary_path


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="python -m tools.unredact.bulk")
    src = p.add_mutually_exclusive_group(required=True)
    src.add_argument("target", nargs="?", help="Directory to scan recursively for PDFs")
    src.add_argument("--mbox-db", nargs="?", const=str(DEFAULT_MBOX_DB),
                     help=f"Query PDF attachments from mbox SQLite DB "
                          f"(default: {DEFAULT_MBOX_DB})")
    p.add_argument("--output", default=str(DEFAULT_OUTPUT), help="Output directory")
    p.add_argument("--parallel", type=int, default=0,
                   help="Workers (0=auto = min(16, cpu_count()//2))")
    args = p.parse_args(argv)

    output_dir = Path(args.output).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    if args.mbox_db:
        db = Path(args.mbox_db).resolve()
        print(f"Source: mbox DB {db}")
        tasks = gather_from_mbox_db(db)
        source_label = f"mbox DB ({db})"
    else:
        root = Path(args.target).resolve()
        if not root.exists():
            print(f"ERROR: {root} not found", file=sys.stderr)
            return 2
        print(f"Source: directory {root}")
        tasks = gather_from_dir(root)
        source_label = f"directory ({root})"

    if not tasks:
        print("No PDF tasks found.", file=sys.stderr)
        return 2

    workers = args.parallel
    if workers == 0:
        workers = min(16, max(2, (os.cpu_count() or 4) // 2))

    t_start = time.time()
    results = run_bulk(tasks, output_dir, workers)
    t_total = time.time() - t_start

    summary = write_bulk_summary(results, output_dir, source_label)

    print()
    print("=" * 60)
    print(f"Done in {t_total:.1f}s ({len(results)} files, {workers} workers)")
    print(f"Summary: {summary}")
    print("=" * 60)

    usable = [r for r in results if not r.get("error")]
    recoverable = [r for r in usable if r.get("verdict_recoverable_pages", 0) > 0]
    total_pages = sum(r.get("verdict_recoverable_pages", 0) for r in usable)
    print(f"Recoverable files: {len(recoverable)}")
    print(f"Recoverable pages: {total_pages}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
