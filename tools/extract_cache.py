#!/usr/bin/env python3
"""Persistent extraction cache wrapper.

Extracts text from PDF/DOCX/EML/MSG files into a cache directory, keyed by
(path, mtime, size). Only re-extracts files that have changed since the last
run. Dramatically faster for repeated sweeps of the same source tree.

Shares extraction functions with tools/bulk_extract.py — any extractor added
there is automatically picked up here.

Usage:
    # Extract into default cache location
    python tools/extract_cache.py "C:/Users/atayl/Desktop/IMPORTANT DOCS"

    # Custom output dir
    python tools/extract_cache.py <src> --out .cache/my_cache/

    # Only show stats, don't extract
    python tools/extract_cache.py <src> --stats

    # Force full re-extraction (ignore cache)
    python tools/extract_cache.py <src> --force

    # Clean stale entries (files deleted from source)
    python tools/extract_cache.py <src> --clean

Cache layout:
    <cache_dir>/
        manifest.json              # {rel_path: {mtime, size, out_file, status}}
        files/                     # mirror of source tree, one .txt per file
            subdir1/
                file1.pdf.txt
            subdir2/
                file2.docx.txt
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

# Reuse extractors from bulk_extract.py — add tools/ to sys.path so it imports
TOOLS_DIR = Path(__file__).resolve().parent
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))
from bulk_extract import EXTRACTORS  # noqa: E402


DEFAULT_CACHE_ROOT = Path(__file__).resolve().parents[1] / ".cache" / "extracted"


def cache_dir_for(src: Path, custom: Path | None = None) -> Path:
    """Return a stable cache dir for a source tree."""
    if custom:
        return custom
    # Hash the absolute source path so different sources don't collide
    src_abs = str(src.resolve()).replace("\\", "/")
    h = hashlib.sha1(src_abs.encode("utf-8")).hexdigest()[:10]
    slug = src.name.replace(" ", "_") or "root"
    return DEFAULT_CACHE_ROOT / f"{slug}_{h}"


def load_manifest(cache_dir: Path) -> dict:
    mf = cache_dir / "manifest.json"
    if not mf.exists():
        return {"source": "", "entries": {}}
    try:
        return json.loads(mf.read_text(encoding="utf-8"))
    except Exception:
        return {"source": "", "entries": {}}


def save_manifest(cache_dir: Path, manifest: dict) -> None:
    cache_dir.mkdir(parents=True, exist_ok=True)
    mf = cache_dir / "manifest.json"
    mf.write_text(json.dumps(manifest, indent=2, ensure_ascii=False), encoding="utf-8")


def scan_source(src: Path, include_exts: set[str]) -> dict[str, dict]:
    """Walk the source tree, return {rel_path: {mtime, size}} for matching files."""
    out = {}
    for dirpath, _dirs, filenames in os.walk(src):
        for fn in filenames:
            ext = os.path.splitext(fn)[1].lower()
            if ext not in include_exts:
                continue
            abs_path = os.path.join(dirpath, fn)
            try:
                st = os.stat(abs_path)
            except OSError:
                continue
            rel = os.path.relpath(abs_path, src).replace("\\", "/")
            out[rel] = {
                "mtime": int(st.st_mtime),
                "size": st.st_size,
            }
    return out


def extract_one(src: Path, rel_path: str, cache_files_dir: Path) -> tuple[str, bool, str]:
    """Extract a single file, write to cache. Returns (rel_path, ok, message)."""
    abs_src = src / rel_path
    ext = abs_src.suffix.lower()
    extractor = EXTRACTORS.get(ext)
    if not extractor:
        return rel_path, False, f"no extractor for {ext}"
    try:
        text = extractor(str(abs_src))
    except Exception as e:
        return rel_path, False, f"extraction error: {e}"

    out_file = cache_files_dir / (rel_path + ".txt")
    out_file.parent.mkdir(parents=True, exist_ok=True)
    header = f"Source: {rel_path}\n\n"
    out_file.write_text(header + text, encoding="utf-8", newline="\n")
    return rel_path, True, "ok"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("source", help="Source directory to extract")
    parser.add_argument("--out", help="Cache directory (default: .cache/extracted/<slug>_<hash>/)")
    parser.add_argument("--types", default=".pdf,.docx,.eml,.msg,.doc",
                        help="Comma-separated extensions (default: all bulk_extract types)")
    parser.add_argument("--workers", type=int, default=8, help="Parallel workers (default: 8)")
    parser.add_argument("--stats", action="store_true", help="Show cache stats, no extraction")
    parser.add_argument("--force", action="store_true", help="Ignore cache, re-extract everything")
    parser.add_argument("--clean", action="store_true", help="Remove stale cache entries (files deleted from source)")
    args = parser.parse_args()

    src = Path(args.source).resolve()
    if not src.is_dir():
        print(f"ERROR: {src} is not a directory", file=sys.stderr)
        return 1

    cache_dir = cache_dir_for(src, Path(args.out).resolve() if args.out else None)
    files_dir = cache_dir / "files"
    files_dir.mkdir(parents=True, exist_ok=True)

    include_exts = {e.strip().lower() for e in args.types.split(",") if e.strip()}
    missing_exts = include_exts - set(EXTRACTORS)
    if missing_exts:
        print(f"WARN: no extractor registered for: {sorted(missing_exts)}", file=sys.stderr)

    print(f"Source: {src}", file=sys.stderr)
    print(f"Cache:  {cache_dir}", file=sys.stderr)

    manifest = load_manifest(cache_dir)
    manifest["source"] = str(src)
    entries = manifest.get("entries", {})

    t0 = time.perf_counter()
    current = scan_source(src, include_exts)
    scan_elapsed = time.perf_counter() - t0

    # Classify
    new_files = []
    changed = []
    unchanged = []
    for rel, meta in current.items():
        prev = entries.get(rel)
        if args.force or prev is None:
            new_files.append(rel)
        elif prev.get("mtime") != meta["mtime"] or prev.get("size") != meta["size"]:
            changed.append(rel)
        else:
            unchanged.append(rel)

    deleted = [rel for rel in entries.keys() if rel not in current]

    print(f"Scan:      {len(current):>6} files ({scan_elapsed:.2f}s)", file=sys.stderr)
    print(f"  new:     {len(new_files):>6}", file=sys.stderr)
    print(f"  changed: {len(changed):>6}", file=sys.stderr)
    print(f"  cached:  {len(unchanged):>6}", file=sys.stderr)
    print(f"  deleted: {len(deleted):>6}", file=sys.stderr)

    if args.stats:
        return 0

    # Clean stale
    if args.clean or deleted:
        for rel in deleted:
            out_file = files_dir / (rel + ".txt")
            try:
                if out_file.exists():
                    out_file.unlink()
            except OSError:
                pass
            entries.pop(rel, None)
        if deleted:
            print(f"Cleaned {len(deleted)} stale entries", file=sys.stderr)

    # Extract new + changed
    to_extract = new_files + changed
    if not to_extract:
        print("Nothing to extract - cache is current.", file=sys.stderr)
        manifest["entries"] = entries
        manifest["last_run"] = int(time.time())
        save_manifest(cache_dir, manifest)
        return 0

    t0 = time.perf_counter()
    ok = 0
    failed = 0
    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        futs = {pool.submit(extract_one, src, rel, files_dir): rel for rel in to_extract}
        for fut in as_completed(futs):
            rel, success, msg = fut.result()
            meta = current[rel]
            entries[rel] = {
                "mtime": meta["mtime"],
                "size": meta["size"],
                "ok": success,
                "msg": msg if not success else "ok",
            }
            if success:
                ok += 1
            else:
                failed += 1
    elapsed = time.perf_counter() - t0
    rate = (ok + failed) / elapsed if elapsed else 0
    print(f"Extracted: {ok} ok, {failed} failed in {elapsed:.1f}s ({rate:.1f} files/s)", file=sys.stderr)

    manifest["entries"] = entries
    manifest["last_run"] = int(time.time())
    save_manifest(cache_dir, manifest)
    print(f"Cache ready: {files_dir}", file=sys.stderr)
    return 0 if failed == 0 else 2


if __name__ == "__main__":
    sys.exit(main())
