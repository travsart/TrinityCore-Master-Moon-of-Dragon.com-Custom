"""Non-destructive redaction recovery diagnostic for PDFs.

Runs six passes on each input PDF and emits a markdown report per file
under the output directory. Does NOT modify the input PDFs in any way.

Passes:
    1. Metadata — doc info, XMP, creation tool, author (Category 5 recovery)
    2. Text-layer extraction per page — detects Category 1 redaction theater
       (visible black rectangles over intact text stream)
    3. OCR layer inspection — detects Category 2 (OCR done before rendering)
    4. PDF Optional Content Groups (OCGs / layers) — detects Category 3
    5. Redaction annotations — the proper /Redact subtype, if present
    6. Byte-level residue scan — looks for known-name strings in raw bytes
       (Category 4)

Usage:
    python -m tools.unredact.diagnose <pdf_or_dir>
    python -m tools.unredact.diagnose <pdf_or_dir> --output <dir>
    python -m tools.unredact.diagnose <pdf_or_dir> --names names.txt

Outputs (per input file):
    <output_dir>/<basename>.report.md       human-readable findings
    <output_dir>/<basename>.recovered.txt   full text layer (if present)
    <output_dir>/<basename>.pages/          per-page text files for review

Read-only on input files. Writes ONLY to the output directory.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
import time
from pathlib import Path

# Force UTF-8 on Windows stdio so arrows, emoji, and decoded names render cleanly.
if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

# Default known-name list for the case — used in Category 4 byte-residue scan.
# These are public names from the case archive (memory/case-researcher CLAUDE.md).
# Keep lowercase for case-insensitive search.
DEFAULT_KNOWN_NAMES = [
    "taylor", "campbell", "webber", "wheeler", "lawrence", "iandoli",
    "earles", "mcmaster", "johnston", "garro", "corpening", "ko",
    "gebhardt", "burns", "fain", "tolin", "wareham", "stringer",
    "via", "hinton", "cermak", "veritas", "dha", "afbcmr", "prhp",
    "qai", "narsum", "sapr", "sapro", "dcsa", "npdb", "moody",
    "cannon", "kirtland", "offutt",
]


def load_names(path: Path | None) -> list[str]:
    if not path:
        return DEFAULT_KNOWN_NAMES
    names = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            names.append(line.lower())
    return names or DEFAULT_KNOWN_NAMES


def fmt_bytes(n: int) -> str:
    for unit in ("B", "KB", "MB", "GB"):
        if n < 1024:
            return f"{n:.1f} {unit}"
        n /= 1024
    return f"{n:.1f} TB"


# =========================================================================
# Pass 1 — metadata + XMP
# =========================================================================


def pass_metadata(doc) -> dict:
    result = {"metadata": {}, "xmp": "", "producer_notes": []}
    try:
        result["metadata"] = dict(doc.metadata or {})
    except Exception as e:
        result["metadata"] = {"error": str(e)}
    # PyMuPDF's xref_xml_metadata() returns an xref number (int), not the XML.
    # xref 0 means no XMP stream present; otherwise read the stream bytes.
    try:
        xref = doc.xref_xml_metadata()
        if isinstance(xref, int) and xref > 0:
            try:
                xmp_bytes = doc.xref_stream(xref) or b""
                result["xmp"] = xmp_bytes.decode("utf-8", errors="replace")
            except Exception as e:
                result["xmp_error"] = f"xref_stream({xref}): {e}"
        # If xref_xml_metadata already returned a string on some build, keep it.
        elif isinstance(xref, (str, bytes)):
            result["xmp"] = xref if isinstance(xref, str) else xref.decode("utf-8", errors="replace")
    except Exception as e:
        result["xmp_error"] = str(e)

    # Flag anything interesting
    for k, v in result["metadata"].items():
        if not v:
            continue
        v_str = str(v)
        if any(marker in v_str.lower() for marker in ("redact", "adobe", "acrobat", "nuance", "kofax", "foxit")):
            result["producer_notes"].append(f"{k}: {v_str}")

    return result


# =========================================================================
# Pass 2 — text-layer extraction + redaction-theater detection
#
# The key signal: if get_text() returns meaningful content on a page that
# *visibly* appears redacted (lots of filled black rectangles), the text
# layer was never removed → Category 1 recovery.
# =========================================================================


def pass_text_layer(doc) -> dict:
    result = {
        "pages": [],
        "total_chars": 0,
        "pages_with_text": 0,
        "pages_with_redaction_marks": 0,
        "pages_recoverable": 0,
    }
    for i, page in enumerate(doc):
        try:
            text = page.get_text("text") or ""
        except Exception as e:
            text = ""
            page_err = str(e)
        else:
            page_err = None

        # Count filled rectangles that could be redaction covers
        # (Solid-fill rectangles, black-ish, reasonably large)
        fill_rects = 0
        try:
            drawings = page.get_drawings()
            for d in drawings:
                if d.get("type") in ("f", "fs"):  # filled (or filled+stroked)
                    color = d.get("fill") or (0, 0, 0)
                    # "Black-ish": any channel <= 0.15 counts as suspicious
                    if all(c <= 0.15 for c in color[:3]):
                        for item in d.get("items", []):
                            if item and item[0] == "re":  # rectangle
                                try:
                                    rect = item[1]
                                    # Skip tiny fills (bullet points, etc.)
                                    if (rect.width > 20 and rect.height > 5):
                                        fill_rects += 1
                                except Exception:
                                    pass
        except Exception:
            pass

        # Count explicit /Redact annotations (the PROPER way to redact)
        redact_annots = 0
        try:
            for annot in page.annots() or []:
                if annot.type[0] == 12:  # Redact
                    redact_annots += 1
        except Exception:
            pass

        page_info = {
            "page": i + 1,
            "chars": len(text),
            "words": len(text.split()) if text else 0,
            "black_rects": fill_rects,
            "redact_annots": redact_annots,
            "err": page_err,
        }
        result["pages"].append(page_info)
        result["total_chars"] += len(text)
        if text.strip():
            result["pages_with_text"] += 1
        if fill_rects > 0 or redact_annots > 0:
            result["pages_with_redaction_marks"] += 1
            # Category 1: has redaction marks AND has text → likely recoverable
            if text.strip() and len(text) > 50:
                result["pages_recoverable"] += 1

    return result


# =========================================================================
# Pass 3 — OCR layer inspection (detect searchable-scan under redaction)
# =========================================================================


def pass_ocr_layer(doc) -> dict:
    """Check if pages are image-based but have an invisible OCR text layer."""
    result = {"ocr_pages": 0, "image_only_pages": 0, "text_only_pages": 0, "mixed_pages": 0}
    for page in doc:
        try:
            has_images = len(page.get_images()) > 0
            text = page.get_text("text") or ""
            has_text = bool(text.strip())

            if has_images and has_text:
                result["mixed_pages"] += 1
                # Heuristic: if it's mostly image with a little text, probably OCR
                if len(text) < 2000:
                    result["ocr_pages"] += 1
            elif has_images and not has_text:
                result["image_only_pages"] += 1
            elif has_text and not has_images:
                result["text_only_pages"] += 1
        except Exception:
            pass
    return result


# =========================================================================
# Pass 4 — Optional Content Groups (OCG) / layers
# =========================================================================


def pass_ocgs(doc) -> dict:
    result = {"ocgs": [], "layer_configs": []}
    try:
        ocgs = doc.get_ocgs() or {}
        for xref, info in ocgs.items():
            name = info.get("name", "?")
            result["ocgs"].append({"xref": xref, "name": name, "on": info.get("on")})
    except Exception as e:
        result["error"] = str(e)
    try:
        cfg = doc.layer_ui_configs() or []
        result["layer_configs"] = [{"name": c.get("text"), "type": c.get("type"), "on": c.get("on")} for c in cfg]
    except Exception:
        pass
    return result


# =========================================================================
# Pass 5 — redaction annotations (properly-marked /Redact subtype)
# =========================================================================


def pass_redact_annots(doc) -> dict:
    result = {"total": 0, "by_page": []}
    for i, page in enumerate(doc):
        try:
            count = 0
            for annot in page.annots() or []:
                if annot.type[0] == 12:  # Redact
                    count += 1
            if count > 0:
                result["by_page"].append({"page": i + 1, "count": count})
                result["total"] += count
        except Exception:
            pass
    return result


# =========================================================================
# Pass 6 — byte-level residue scan for known names
# =========================================================================


def pass_byte_residue(pdf_path: Path, known_names: list[str]) -> dict:
    """Scan raw file bytes for printable strings matching known-name patterns.

    Uses word boundaries so short names (Ko, Via) don't false-positive on
    random byte patterns like '55jkO' or '1xnVIA'. A word boundary here is
    a non-alphanumeric ASCII byte (or string start/end).
    """
    result = {"hits": {}, "total_hits": 0}
    try:
        data = pdf_path.read_bytes()
    except Exception as e:
        return {"error": str(e)}

    # Extract printable strings of length >= 6
    strings = re.findall(rb"[\x20-\x7E]{6,}", data)

    for needle in known_names:
        needle_bytes = re.escape(needle.encode("utf-8", "ignore"))
        # Short names (< 4 chars) need strict whitespace/delimiter boundaries
        # to avoid false positives in binary PDF content. Longer names can
        # use any non-alphanumeric as word boundary.
        if len(needle) < 4:
            pattern = re.compile(
                rb"(?:^|[\s,;:()\[\]])" + needle_bytes + rb"(?=$|[\s,;:()\[\]\.!?])",
                re.IGNORECASE,
            )
        else:
            pattern = re.compile(
                rb"(?:^|[^A-Za-z0-9])" + needle_bytes + rb"(?=$|[^A-Za-z0-9])",
                re.IGNORECASE,
            )
        count = 0
        samples = []
        for s in strings:
            matches = pattern.findall(s)
            if matches:
                count += len(matches)
                if len(samples) < 5:
                    try:
                        samples.append(s.decode("utf-8", "replace")[:200])
                    except Exception:
                        pass
        if count > 0:
            result["hits"][needle] = {"count": count, "samples": samples}
            result["total_hits"] += count
    return result


# =========================================================================
# Report writer
# =========================================================================


def write_report(
    pdf_path: Path, output_dir: Path,
    meta: dict, text: dict, ocr: dict, ocgs: dict,
    redact: dict, residue: dict, elapsed: float,
) -> Path:
    base = pdf_path.stem
    report_path = output_dir / f"{base}.report.md"
    output_dir.mkdir(parents=True, exist_ok=True)

    # Verdict
    if text["pages_recoverable"] > 0:
        verdict = "**RECOVERABLE** — text layer survives under redaction marks (Category 1)"
    elif residue["total_hits"] > 10:
        verdict = "**PARTIAL** — byte-level residue contains known names (Category 4)"
    elif ocr.get("ocr_pages", 0) > 0 and text["pages_with_redaction_marks"] > 0:
        verdict = "**LIKELY RECOVERABLE** — OCR text layer present under image redactions (Category 2)"
    elif ocgs.get("ocgs"):
        verdict = "**POSSIBLE** — Optional Content Groups found; try toggling layers (Category 3)"
    else:
        verdict = "**LIKELY NOT RECOVERABLE** — no text layer under redaction marks, no OCR layer, no OCGs"

    lines = [
        f"# Redaction Diagnostic: `{pdf_path.name}`",
        "",
        f"- **Path**: `{pdf_path}`",
        f"- **Size**: {fmt_bytes(pdf_path.stat().st_size)}",
        f"- **Analyzed in**: {elapsed:.1f}s",
        f"- **Verdict**: {verdict}",
        "",
        "---",
        "",
        "## Pass 1 — Metadata",
        "",
    ]
    for k, v in (meta.get("metadata") or {}).items():
        if v:
            lines.append(f"- **{k}**: {v}")
    if meta.get("producer_notes"):
        lines.append("")
        lines.append("**Producer notes** (redaction-software fingerprints):")
        for n in meta["producer_notes"]:
            lines.append(f"- {n}")
    if meta.get("xmp"):
        lines.append("")
        lines.append("**XMP present** (truncated to 2 KB):")
        lines.append("```xml")
        lines.append(meta["xmp"][:2000])
        lines.append("```")

    lines.extend([
        "",
        "## Pass 2 — Text Layer (Category 1)",
        "",
        f"- **Total text characters**: {text['total_chars']:,}",
        f"- **Pages with text**: {text['pages_with_text']}",
        f"- **Pages with black rects / redact annots**: {text['pages_with_redaction_marks']}",
        f"- **Pages where text survives under redactions**: **{text['pages_recoverable']}**",
        "",
    ])
    if text["pages_recoverable"] > 0:
        lines.append("**KEY FINDING**: text layer exists under pages that visually appear redacted. "
                     "Extract via `fitz.get_text()` or `pdftotext` to recover.")
        lines.append("")
    # Per-page table (top 20 most-interesting pages: those with redactions)
    interesting = [p for p in text["pages"] if p["black_rects"] > 0 or p["redact_annots"] > 0]
    if interesting:
        lines.append("**Interesting pages** (have redaction marks):")
        lines.append("")
        lines.append("| Page | Chars | Words | Black rects | /Redact annots |")
        lines.append("|-----:|------:|------:|------------:|---------------:|")
        for p in interesting[:50]:
            lines.append(f"| {p['page']} | {p['chars']} | {p['words']} | {p['black_rects']} | {p['redact_annots']} |")

    lines.extend([
        "",
        "## Pass 3 — OCR Layer (Category 2)",
        "",
        f"- **Image-only pages**: {ocr.get('image_only_pages', 0)}",
        f"- **Text-only pages**: {ocr.get('text_only_pages', 0)}",
        f"- **Mixed image+text pages**: {ocr.get('mixed_pages', 0)}",
        f"- **Likely OCR pages**: {ocr.get('ocr_pages', 0)}",
        "",
        "## Pass 4 — Optional Content Groups (Category 3)",
        "",
    ])
    if ocgs.get("ocgs"):
        lines.append(f"**Found {len(ocgs['ocgs'])} OCG(s):**")
        lines.append("")
        for g in ocgs["ocgs"]:
            lines.append(f"- `{g['name']}` (xref={g['xref']}, on={g.get('on')})")
        lines.append("")
        lines.append("**Try toggling these off and re-extracting text.** See `pymupdf` `set_layer_ui_config`.")
    else:
        lines.append("No OCGs found.")

    lines.extend([
        "",
        "## Pass 5 — Proper `/Redact` Annotations",
        "",
        f"- **Total redact annotations**: {redact['total']}",
    ])
    if redact["total"] > 0:
        lines.append("")
        lines.append("_If these are unapplied (the PDF still contains them as annotations rather than"
                     " having applied the redactions), the covered content is trivially recoverable._")

    lines.extend([
        "",
        "## Pass 6 — Byte Residue (Category 4)",
        "",
        f"- **Known-name hits in raw bytes**: {residue.get('total_hits', 0)}",
        "",
    ])
    hits = residue.get("hits") or {}
    if hits:
        lines.append("| Name | Count | Sample context |")
        lines.append("|------|------:|----------------|")
        for name, info in sorted(hits.items(), key=lambda x: -x[1]["count"]):
            sample = info["samples"][0] if info["samples"] else ""
            sample = sample.replace("|", "\\|")[:120]
            lines.append(f"| `{name}` | {info['count']} | `{sample}` |")

    report_path.write_text("\n".join(lines), encoding="utf-8")
    return report_path


# =========================================================================
# Main driver
# =========================================================================


def diagnose_file(pdf_path: Path, output_dir: Path, known_names: list[str],
                  dump_text: bool = True) -> dict:
    import fitz  # PyMuPDF
    t0 = time.time()
    try:
        doc = fitz.open(str(pdf_path))
    except Exception as e:
        return {"path": str(pdf_path), "error": f"open failed: {e}"}

    try:
        meta = pass_metadata(doc)
        text = pass_text_layer(doc)
        ocr = pass_ocr_layer(doc)
        ocgs = pass_ocgs(doc)
        redact = pass_redact_annots(doc)
        residue = pass_byte_residue(pdf_path, known_names)

        # Dump recovered text if any
        if dump_text and text["total_chars"] > 0:
            rec_path = output_dir / f"{pdf_path.stem}.recovered.txt"
            with rec_path.open("w", encoding="utf-8", newline="\n") as f:
                for i, page in enumerate(doc):
                    try:
                        t = page.get_text("text") or ""
                    except Exception:
                        t = ""
                    if t.strip():
                        f.write(f"=== Page {i+1} ===\n{t}\n\n")
    finally:
        doc.close()

    elapsed = time.time() - t0
    report = write_report(pdf_path, output_dir, meta, text, ocr, ocgs, redact, residue, elapsed)

    return {
        "path": str(pdf_path),
        "report": str(report),
        "verdict_recoverable_pages": text["pages_recoverable"],
        "verdict_total_pages": len(text["pages"]),
        "residue_hits": residue.get("total_hits", 0),
        "elapsed": elapsed,
    }


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="python -m tools.unredact.diagnose")
    p.add_argument("target", help="PDF file or directory")
    p.add_argument(
        "--output", default="C:/Users/atayl/Desktop/Excluded/unredact/output",
        help="Output directory (default: Desktop/Excluded/unredact/output)",
    )
    p.add_argument("--names", help="Text file with known names, one per line")
    p.add_argument("--no-text-dump", action="store_true",
                   help="Skip dumping recovered text to <basename>.recovered.txt")
    args = p.parse_args(argv)

    try:
        import fitz  # noqa: F401
    except ImportError:
        print("ERROR: PyMuPDF not installed. pip install pymupdf", file=sys.stderr)
        return 2

    target = Path(args.target).resolve()
    output_dir = Path(args.output).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    known_names = load_names(Path(args.names).resolve() if args.names else None)

    if target.is_file():
        files = [target]
    elif target.is_dir():
        files = sorted(target.rglob("*.pdf"))
    else:
        print(f"ERROR: {target} not found", file=sys.stderr)
        return 2

    if not files:
        print(f"No PDF files found at {target}", file=sys.stderr)
        return 2

    print(f"Output:       {output_dir}")
    print(f"Known names:  {len(known_names)}")
    print(f"Files:        {len(files)}")
    print()

    summary = []
    for i, f in enumerate(files, 1):
        print(f"[{i}/{len(files)}] {f.name}")
        r = diagnose_file(f, output_dir, known_names, dump_text=not args.no_text_dump)
        summary.append(r)
        if r.get("error"):
            print(f"  ERROR: {r['error']}")
        else:
            print(
                f"  {r['verdict_recoverable_pages']}/{r['verdict_total_pages']} pages recoverable, "
                f"{r['residue_hits']} byte-residue hits, {r['elapsed']:.1f}s"
            )
            print(f"  → {r['report']}")

    # Aggregate summary
    total_recoverable = sum(s.get("verdict_recoverable_pages", 0) for s in summary)
    total_pages = sum(s.get("verdict_total_pages", 0) for s in summary)
    total_residue = sum(s.get("residue_hits", 0) for s in summary)

    print()
    print("=" * 60)
    print(f"Files analyzed:        {len(summary)}")
    print(f"Total pages:           {total_pages}")
    print(f"Recoverable pages:     {total_recoverable}")
    print(f"Byte-residue hits:     {total_residue}")
    print("=" * 60)

    # Write aggregate report
    agg_path = output_dir / "_SUMMARY.md"
    agg_lines = [
        "# Unredact Diagnostic Summary",
        "",
        f"- Files: {len(summary)}",
        f"- Total pages: {total_pages}",
        f"- **Recoverable pages (Category 1)**: {total_recoverable}",
        f"- Byte-residue hits: {total_residue}",
        "",
        "## Per-file",
        "",
        "| File | Pages | Recoverable | Residue hits | Report |",
        "|------|------:|------------:|-------------:|--------|",
    ]
    for s in summary:
        if s.get("error"):
            agg_lines.append(f"| {Path(s['path']).name} | - | ERROR | - | {s['error'][:60]} |")
        else:
            agg_lines.append(
                f"| {Path(s['path']).name} | {s['verdict_total_pages']} | "
                f"**{s['verdict_recoverable_pages']}** | {s['residue_hits']} | "
                f"[{Path(s['report']).name}]({Path(s['report']).name}) |"
            )
    agg_path.write_text("\n".join(agg_lines), encoding="utf-8")
    print(f"\nSummary: {agg_path}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
