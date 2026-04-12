"""OCR pass for image-only redacted pages.

For pages that have visible black redaction rectangles but NO text layer
(i.e. scanned or rendered-as-image pages), render at 300 DPI and run
Tesseract OCR to extract any visible text around the redactions. That
extracted text then feeds into tools.unredact.reconstruct for cross-file
source matching.

Requires:
    - PyMuPDF (fitz)             — already installed
    - pytesseract Python wrapper — already installed
    - Tesseract binary            — installed at C:/Program Files/Tesseract-OCR/

Usage:
    python -m tools.unredact.ocr <pdf>
    python -m tools.unredact.ocr <pdf> --pages 266,267,268,280-290
    python -m tools.unredact.ocr <pdf_dir> --parallel 8
    python -m tools.unredact.ocr <pdf> --dpi 400 --only-image-pages
"""

from __future__ import annotations

import argparse
import multiprocessing as mp
import os
import sys
import time
import traceback
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path

if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

# Default tesseract binary location (winget installs here on Windows)
DEFAULT_TESSERACT = r"C:\Program Files\Tesseract-OCR\tesseract.exe"
DEFAULT_OUTPUT = Path(r"C:\Users\atayl\Desktop\Excluded\unredact\ocr_output")


def configure_tesseract():
    """Point pytesseract at the installed binary if needed."""
    try:
        import pytesseract
    except ImportError:
        print("ERROR: pytesseract not installed. pip install pytesseract", file=sys.stderr)
        return None

    binary = DEFAULT_TESSERACT
    if os.path.exists(binary):
        pytesseract.pytesseract.tesseract_cmd = binary
        return pytesseract

    # Fall back to PATH lookup
    try:
        version = pytesseract.get_tesseract_version()
        return pytesseract
    except Exception:
        print(f"ERROR: tesseract binary not found at {binary} or on PATH", file=sys.stderr)
        return None


def parse_page_range(spec: str) -> set[int]:
    """Parse '1,3,5-10,266,280-290' into a set of 1-indexed page numbers."""
    pages: set[int] = set()
    for part in spec.split(","):
        part = part.strip()
        if not part:
            continue
        if "-" in part:
            a, b = part.split("-", 1)
            pages.update(range(int(a), int(b) + 1))
        else:
            pages.add(int(part))
    return pages


def find_image_only_pages_with_redactions(pdf_path: Path) -> list[int]:
    """Return 1-indexed page numbers that are image-only AND have black rects."""
    import fitz
    doc = fitz.open(str(pdf_path))
    pages = []
    try:
        for i in range(len(doc)):
            page = doc[i]
            text = (page.get_text("text") or "").strip()
            if text:
                # has text layer — covered by Category 1, skip
                continue
            # Check for black rects
            has_black = False
            try:
                for d in page.get_drawings():
                    if d.get("type") in ("f", "fs"):
                        color = d.get("fill") or (0, 0, 0)
                        if all(c <= 0.15 for c in color[:3]):
                            for item in d.get("items", []):
                                if item and item[0] == "re":
                                    try:
                                        rect = item[1]
                                        if rect.width > 20 and rect.height > 5:
                                            has_black = True
                                            break
                                    except Exception:
                                        pass
                    if has_black:
                        break
            except Exception:
                pass
            if has_black:
                pages.append(i + 1)
    finally:
        doc.close()
    return pages


def ocr_page(pdf_path: Path, page_num: int, dpi: int) -> tuple[int, str, int]:
    """Render one page to PNG bytes and OCR it. Returns (page_num, text, char_count)."""
    import fitz
    import pytesseract
    from io import BytesIO
    from PIL import Image

    doc = fitz.open(str(pdf_path))
    try:
        if page_num < 1 or page_num > len(doc):
            return (page_num, "", 0)
        page = doc[page_num - 1]
        pix = page.get_pixmap(dpi=dpi)
        png_bytes = pix.tobytes("png")
        img = Image.open(BytesIO(png_bytes))
        text = pytesseract.image_to_string(img, lang="eng") or ""
        return (page_num, text, len(text))
    finally:
        doc.close()


def ocr_worker(task: dict) -> dict:
    """Worker: OCR a single page. Runs in a subprocess."""
    try:
        configure_tesseract()
        pno, text, chars = ocr_page(
            Path(task["pdf"]), task["page"], task["dpi"],
        )
        return {
            "pdf": task["pdf"],
            "page": pno,
            "chars": chars,
            "text": text,
            "ok": True,
        }
    except Exception as e:
        return {
            "pdf": task["pdf"],
            "page": task["page"],
            "ok": False,
            "error": f"{type(e).__name__}: {e}\n{traceback.format_exc()[:500]}",
        }


def ocr_pdf(pdf_path: Path, output_dir: Path, dpi: int = 300,
            page_filter: set[int] | None = None,
            only_image_pages: bool = True,
            workers: int = 1) -> dict:
    """OCR the relevant pages of a PDF, write per-page and aggregate text files."""
    output_dir.mkdir(parents=True, exist_ok=True)

    # Decide which pages to OCR
    if page_filter is not None:
        pages = sorted(page_filter)
        kind = "explicit"
    elif only_image_pages:
        pages = find_image_only_pages_with_redactions(pdf_path)
        kind = "image-only with redactions"
    else:
        import fitz
        doc = fitz.open(str(pdf_path))
        pages = list(range(1, len(doc) + 1))
        doc.close()
        kind = "all"

    if not pages:
        print(f"  {pdf_path.name}: no pages to OCR ({kind})")
        return {"pdf": str(pdf_path), "pages_ocrd": 0, "total_chars": 0}

    print(f"  {pdf_path.name}: OCR {len(pages)} {kind} page(s) @ {dpi} DPI")
    t0 = time.time()
    results: list[dict] = []

    if workers > 1 and len(pages) > 1:
        tasks = [{"pdf": str(pdf_path), "page": p, "dpi": dpi} for p in pages]
        with ProcessPoolExecutor(max_workers=workers) as ex:
            futures = {ex.submit(ocr_worker, t): t for t in tasks}
            for fut in as_completed(futures):
                try:
                    results.append(fut.result())
                except Exception as e:
                    t = futures[fut]
                    results.append({"pdf": t["pdf"], "page": t["page"], "ok": False,
                                    "error": str(e)})
    else:
        configure_tesseract()
        for p in pages:
            try:
                pno, text, chars = ocr_page(pdf_path, p, dpi)
                results.append({"pdf": str(pdf_path), "page": pno, "chars": chars,
                                "text": text, "ok": True})
            except Exception as e:
                results.append({"pdf": str(pdf_path), "page": p, "ok": False,
                                "error": str(e)})

    elapsed = time.time() - t0

    # Write per-page files
    results.sort(key=lambda r: r["page"])
    total_chars = 0
    ok_pages = 0
    err_pages = 0

    base = pdf_path.stem.replace(" ", "_")[:60]
    per_page_dir = output_dir / base
    per_page_dir.mkdir(parents=True, exist_ok=True)

    agg_lines = [
        f"# OCR output: {pdf_path.name}",
        "",
        f"- PDF:       {pdf_path}",
        f"- Pages OCR'd: {len(results)}",
        f"- DPI:       {dpi}",
        f"- Elapsed:   {elapsed:.1f}s",
        "",
        "---",
        "",
    ]

    for r in results:
        if not r.get("ok"):
            err_pages += 1
            agg_lines.append(f"## Page {r['page']} — ERROR")
            agg_lines.append(f"`{r.get('error', 'unknown')}`")
            agg_lines.append("")
            continue
        ok_pages += 1
        total_chars += r["chars"]
        page_file = per_page_dir / f"page_{r['page']:04d}.ocr.txt"
        page_file.write_text(r["text"] or "", encoding="utf-8", newline="\n")
        agg_lines.append(f"## Page {r['page']} ({r['chars']:,} chars)")
        agg_lines.append("")
        # Include the full OCR text in the aggregate, with a sensible cap
        preview = r["text"] or ""
        if len(preview) > 3000:
            agg_lines.append(preview[:3000])
            agg_lines.append(f"\n_({len(preview) - 3000:,} more chars in {page_file.name})_")
        else:
            agg_lines.append(preview)
        agg_lines.append("")

    agg_path = output_dir / f"{base}.ocr.md"
    agg_path.write_text("\n".join(agg_lines), encoding="utf-8")

    print(f"    {ok_pages} ok, {err_pages} errors, {total_chars:,} chars in {elapsed:.1f}s")
    print(f"    → {agg_path}")
    return {
        "pdf": str(pdf_path),
        "pages_ocrd": ok_pages,
        "errors": err_pages,
        "total_chars": total_chars,
        "elapsed": elapsed,
        "aggregate": str(agg_path),
        "per_page_dir": str(per_page_dir),
    }


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="python -m tools.unredact.ocr")
    p.add_argument("target", help="PDF file or directory")
    p.add_argument("--output", default=str(DEFAULT_OUTPUT), help="Output directory")
    p.add_argument("--dpi", type=int, default=300, help="Render DPI (default 300)")
    p.add_argument("--pages", help="Explicit page range, e.g. '1,3,5-10'")
    p.add_argument("--only-image-pages", action="store_true", default=True,
                   help="(default) Only OCR pages without a text layer AND with black rects")
    p.add_argument("--all-pages", action="store_true",
                   help="OCR every page regardless")
    p.add_argument("--parallel", type=int, default=1, help="Parallel worker count")
    args = p.parse_args(argv)

    if configure_tesseract() is None:
        return 2

    target = Path(args.target).resolve()
    output_dir = Path(args.output).resolve()

    page_filter = parse_page_range(args.pages) if args.pages else None
    only_image = not args.all_pages

    if target.is_file():
        files = [target]
    elif target.is_dir():
        files = sorted(target.rglob("*.pdf"))
    else:
        print(f"ERROR: {target} not found", file=sys.stderr)
        return 2

    if not files:
        print("No PDFs found.", file=sys.stderr)
        return 2

    print(f"Output: {output_dir}")
    print(f"Files:  {len(files)}")
    print()

    t0 = time.time()
    total_pages = 0
    total_chars = 0
    for f in files:
        r = ocr_pdf(
            f, output_dir, dpi=args.dpi,
            page_filter=page_filter,
            only_image_pages=only_image,
            workers=args.parallel,
        )
        total_pages += r.get("pages_ocrd", 0)
        total_chars += r.get("total_chars", 0)

    print()
    print("=" * 60)
    print(f"Done in {time.time() - t0:.1f}s")
    print(f"Total pages OCR'd: {total_pages}")
    print(f"Total chars:       {total_chars:,}")
    print("=" * 60)
    return 0


if __name__ == "__main__":
    sys.exit(main())
