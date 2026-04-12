#!/usr/bin/env python3
"""OCR for screenshots and scanned images via Tesseract + pytesseract.

Produces sidecar .txt files so image content becomes greppable by agents.
Cached by (path, mtime, size).

Requirements:
    pip install pytesseract Pillow
    Tesseract binary: winget install UB-Mannheim.TesseractOCR
    (Default binary location baked in as fallback if not on PATH.)

Usage:
    # Directory (recursive)
    python tools/ocr_images.py "C:/Users/atayl/Desktop/IMPORTANT DOCS/Case_Reference/05_EVIDENCE_SCREENSHOTS"

    # Single file
    python tools/ocr_images.py path/to/screenshot.png

    # Custom output dir
    python tools/ocr_images.py <dir> --out ./ocr_cache/

    # Stats only - show what would be OCR'd
    python tools/ocr_images.py <dir> --stats

    # Force re-OCR
    python tools/ocr_images.py <dir> --force

    # Parallel workers
    python tools/ocr_images.py <dir> --workers 8

    # Minimum text length to keep (shorter results treated as "empty")
    python tools/ocr_images.py <dir> --min-chars 20

Output per image:
    <out>/files/<relpath>.txt   # OCR'd text, or marker file if empty/failed
    <out>/manifest.json         # cache state with char counts per file
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys
import time
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path

DEFAULT_CACHE_ROOT = Path(__file__).resolve().parents[1] / ".cache" / "ocr"
DEFAULT_EXTS = {".png", ".jpg", ".jpeg", ".tif", ".tiff", ".bmp", ".webp"}

# Tesseract binary fallback (winget install puts it here)
TESSERACT_FALLBACK = Path("C:/Program Files/Tesseract-OCR/tesseract.exe")


def configure_tesseract() -> str | None:
    """Set pytesseract.tesseract_cmd if the binary isn't on PATH. Returns the resolved path or None."""
    import pytesseract
    import shutil
    resolved = shutil.which("tesseract")
    if resolved:
        return resolved
    if TESSERACT_FALLBACK.exists():
        pytesseract.pytesseract.tesseract_cmd = str(TESSERACT_FALLBACK)
        return str(TESSERACT_FALLBACK)
    return None


def cache_dir_for(src: Path, custom: Path | None) -> Path:
    if custom:
        return custom
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
    (cache_dir / "manifest.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False), encoding="utf-8"
    )


def scan_source(src: Path, include_exts: set[str]) -> dict[str, dict]:
    if src.is_file():
        ext = src.suffix.lower()
        if ext in include_exts:
            st = src.stat()
            return {src.name: {"mtime": int(st.st_mtime), "size": st.st_size}}
        return {}
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
            out[rel] = {"mtime": int(st.st_mtime), "size": st.st_size}
    return out


def ocr_one(src_root_str: str, rel: str, files_dir_str: str, min_chars: int, tess_cmd: str | None) -> tuple[str, bool, int, str]:
    """Worker: OCR one image. Returns (rel, ok, char_count, msg). Must be top-level for ProcessPoolExecutor."""
    try:
        import pytesseract
        from PIL import Image
        if tess_cmd:
            pytesseract.pytesseract.tesseract_cmd = tess_cmd
    except ImportError as e:
        return rel, False, 0, f"import error: {e}"

    src_root = Path(src_root_str)
    files_dir = Path(files_dir_str)
    abs_src = src_root / rel if src_root.is_dir() else src_root

    try:
        with Image.open(abs_src) as img:
            # Convert to grayscale for better OCR on screenshots
            if img.mode != "L":
                img = img.convert("L")
            text = pytesseract.image_to_string(img)
    except Exception as e:
        return rel, False, 0, f"ocr error: {e}"

    text = text.strip()
    char_count = len(text)

    out_file = files_dir / (rel + ".txt")
    out_file.parent.mkdir(parents=True, exist_ok=True)

    if char_count < min_chars:
        out_file.write_text(
            f"Source: {rel}\nOCR result: EMPTY (less than {min_chars} chars)\n",
            encoding="utf-8", newline="\n",
        )
        return rel, True, char_count, "empty"

    out_file.write_text(
        f"Source: {rel}\nChars: {char_count}\n\n{text}",
        encoding="utf-8", newline="\n",
    )
    return rel, True, char_count, "ok"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("source", help="Image file or directory")
    parser.add_argument("--out", help="Cache directory (default: .cache/ocr/<slug>/)")
    parser.add_argument("--types", default=",".join(sorted(DEFAULT_EXTS)),
                        help="Comma-separated image extensions")
    parser.add_argument("--workers", type=int, default=4, help="Parallel workers (default: 4)")
    parser.add_argument("--min-chars", type=int, default=10,
                        help="Minimum chars to keep as non-empty (default: 10)")
    parser.add_argument("--stats", action="store_true", help="Show cache stats, no OCR")
    parser.add_argument("--force", action="store_true", help="Ignore cache, re-OCR everything")
    args = parser.parse_args()

    src = Path(args.source).resolve()
    if not src.exists():
        print(f"ERROR: {src} does not exist", file=sys.stderr)
        return 1

    tess_cmd = configure_tesseract()
    if tess_cmd is None:
        print("ERROR: tesseract binary not found. Install with:", file=sys.stderr)
        print("  winget install UB-Mannheim.TesseractOCR", file=sys.stderr)
        return 1
    print(f"Tesseract: {tess_cmd}", file=sys.stderr)

    cache_dir = cache_dir_for(src, Path(args.out).resolve() if args.out else None)
    files_dir = cache_dir / "files"
    files_dir.mkdir(parents=True, exist_ok=True)

    include_exts = {e.strip().lower() for e in args.types.split(",") if e.strip()}

    print(f"Source: {src}", file=sys.stderr)
    print(f"Cache:  {cache_dir}", file=sys.stderr)

    manifest = load_manifest(cache_dir)
    manifest["source"] = str(src)
    entries = manifest.get("entries", {})

    current = scan_source(src, include_exts)

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

    print(f"Scan:      {len(current):>5} images", file=sys.stderr)
    print(f"  new:     {len(new_files):>5}", file=sys.stderr)
    print(f"  changed: {len(changed):>5}", file=sys.stderr)
    print(f"  cached:  {len(unchanged):>5}", file=sys.stderr)

    if args.stats:
        return 0

    to_process = new_files + changed
    if not to_process:
        print("Nothing to OCR - cache is current.", file=sys.stderr)
        manifest["entries"] = entries
        manifest["last_run"] = int(time.time())
        save_manifest(cache_dir, manifest)
        return 0

    t0 = time.perf_counter()
    ok_count = 0
    empty_count = 0
    failed = 0
    src_root_str = str(src)
    files_dir_str = str(files_dir)

    with ProcessPoolExecutor(max_workers=args.workers) as pool:
        futs = {
            pool.submit(ocr_one, src_root_str, rel, files_dir_str, args.min_chars, tess_cmd): rel
            for rel in to_process
        }
        for fut in as_completed(futs):
            rel, success, chars, msg = fut.result()
            meta = current[rel]
            entries[rel] = {
                "mtime": meta["mtime"],
                "size": meta["size"],
                "ok": success,
                "chars": chars,
                "msg": msg,
            }
            if not success:
                failed += 1
            elif msg == "empty":
                empty_count += 1
            else:
                ok_count += 1

    elapsed = time.perf_counter() - t0
    rate = len(to_process) / elapsed if elapsed else 0
    print(
        f"OCR: {ok_count} with text, {empty_count} empty, {failed} failed in {elapsed:.1f}s ({rate:.1f} img/s)",
        file=sys.stderr,
    )

    manifest["entries"] = entries
    manifest["last_run"] = int(time.time())
    save_manifest(cache_dir, manifest)
    print(f"Cache ready: {files_dir}", file=sys.stderr)
    return 0 if failed == 0 else 2


if __name__ == "__main__":
    sys.exit(main())
