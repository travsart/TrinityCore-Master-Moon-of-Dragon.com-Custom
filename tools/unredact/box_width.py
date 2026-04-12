"""Box-width inference for redacted name candidates.

For each black-rectangle redaction in a PDF:
  1. Measure its width
  2. Find the surrounding intact text on the same line (same y-coord)
  3. Compute the local average character width from that intact text
     in the same font
  4. Estimate character count of the hidden content = rect_width / char_width
  5. Enumerate candidates from a known-name list whose rendered width
     fits within tolerance (± 15%)

The output is per-box candidate lists — NOT certainty, but investigative
narrowing. A 60px box on a line in Times Roman 11pt might contain any of
three or four candidate names from the case, rather than any of 20.

Usage:
    python -m tools.unredact.box_width <pdf>
    python -m tools.unredact.box_width <pdf> --names names.txt
    python -m tools.unredact.box_width <pdf> --tolerance 0.2
"""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

# Candidates from the case — people AND short phrases that might be redacted
DEFAULT_CANDIDATES = [
    # People (LAST NAME, can be tried alone or with rank/first name)
    "Taylor", "Campbell", "Webber", "Wheeler", "Lawrence", "Iandoli",
    "Earles", "McMaster", "Johnston", "Garro", "Corpening", "Ko",
    "Gebhardt", "Burns", "Fain", "Tolin", "Wareham", "Stringer", "Via",
    "Hinton", "Cermak", "Jarvis", "Pience", "Goeken", "Lam", "Griffin",
    "Sparks",
    # NEW from Pt 2 recordings (2026-04-09 addition)
    "Wiley",
    # CORRECTED: "Graham" in transcript was actually Lt Col Grandin
    "Grandin",
    # Additional candidates supplied by user 2026-04-09 — likely appear in binders
    "Morales", "Aranda", "Martinez", "Rossi", "Plomedahl",
    "Nanci", "Ramsey", "Wilson", "Greiman", "Meadows",
    "Riley", "Everson", "Rio", "Vista",
    # Common redaction content words (not names)
    "transfer",
    # Known from mbox search / memory but missing from original list
    "Delgado", "Cusibichan", "Delgado-Cusibichan",
    # Fuller name variants (initials + surname)
    "Adam Taylor", "Adam J. Taylor", "Capt Taylor", "Capt Adam Taylor",
    "Col Cermak", "Danielle Cermak", "Danielle J. Cermak",
    "Col Earles", "William Earles", "Jon Earles",
    "Col Johnston", "Chad Johnston",
    "Col McMaster", "John McMaster",
    "Capt Campbell", "Johnny Campbell",
    "MSgt Webber", "Samantha Webber",
    "MSgt Wheeler", "Jenality Wheeler",
    "Capt Lawrence", "Anthony Lawrence",
    "Laura Iandoli",
    "Lt Col Garro",
    "Lt Col Corpening",
    "Capt Daniel Ko", "Daniel Ko",
    "Capt Elliot Ko", "Elliot Ko",
    "TSgt Gebhardt",
    "Jackie Burns",
    "SSgt Fain",
    "Jason Wareham",
    "Joshua Tolin",
    "Col Stringer",
    # NEW named actors from Pt 2 recordings — rank+surname variants
    "Major Wiley", "Maj Wiley",
    "Lt Col Grandin", "Lieutenant Colonel Grandin",
    "Victor Delgado", "Victor Delgado Cusibichan",
    # FOIA office context
    "27 SOW/IPK", "cannon.foia@us.af.mil",
    # Emails
    "adam.taylor.20@us.af.mil",
    "ataylor7176@gmail.com",
    "danielle.j.cermak.mil@health.mil",
    "jenality.wheeler@us.af.mil",
    "jwareham@allen-vellone.com",
    # Locations / orgs
    "Cannon", "Kirtland", "Offutt", "27 SOMRS", "27 SOMDG", "27 SOW",
    "AFSOC", "AF-SG", "DHA", "NPDB", "AFBCMR",
    # Incident / case terms
    "QAI", "NARSUM", "PRHP", "IHPP", "DCSA", "PCS",
    # Dates (common formats)
    "September 4, 2024", "August 14, 2024", "July 11, 2024", "October 15, 2024",
]


def load_candidates(path: Path | None) -> list[str]:
    if not path:
        return DEFAULT_CANDIDATES
    names = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            names.append(line)
    return names or DEFAULT_CANDIDATES


# =========================================================================
# Per-span character width measurement
# =========================================================================


def span_char_widths(page) -> dict:
    """Extract character width statistics from each text span on a page.

    Returns dict keyed by (font, size) → list of per-character widths derived
    from the span's bbox width / character count. This gives us an empirical
    font metric without needing to parse font tables.
    """
    widths: dict = {}
    try:
        d = page.get_text("dict")
    except Exception:
        return widths
    for block in d.get("blocks", []):
        if block.get("type") != 0:
            continue
        for line in block.get("lines", []):
            for span in line.get("spans", []):
                text = span.get("text", "")
                bbox = span.get("bbox")
                font = span.get("font", "?")
                size = span.get("size", 0)
                if not text.strip() or not bbox or size <= 0:
                    continue
                span_w = bbox[2] - bbox[0]
                if span_w <= 0 or len(text) == 0:
                    continue
                per_char = span_w / len(text)
                key = (font, round(size, 1))
                widths.setdefault(key, []).append(per_char)
    return widths


def estimate_candidate_width(candidate: str, font: str, size: float,
                             char_widths: dict) -> float | None:
    """Estimate the rendered width of ``candidate`` in the given font/size.

    Uses the per-char widths measured from intact spans on the same page.
    """
    key = (font, round(size, 1))
    samples = char_widths.get(key)
    if not samples:
        # Fall back to any available sample in the same size
        for (f, s), v in char_widths.items():
            if round(s, 1) == round(size, 1):
                samples = v
                break
    if not samples:
        return None
    avg = sum(samples) / len(samples)
    return avg * len(candidate)


# =========================================================================
# Per-page analysis
# =========================================================================


def analyze_page(page, candidates: list[str], tolerance: float = 0.15) -> list[dict]:
    """Find black rectangles on a page and match candidates by width."""
    results: list[dict] = []
    char_widths = span_char_widths(page)
    # Average char width across all spans (fallback if we can't figure out font)
    all_samples = [v for arr in char_widths.values() for v in arr]
    global_avg = sum(all_samples) / len(all_samples) if all_samples else None

    # Get spans as a dict for line-level lookup
    try:
        d = page.get_text("dict")
    except Exception:
        return results
    span_bboxes = []
    for block in d.get("blocks", []):
        if block.get("type") != 0:
            continue
        for line in block.get("lines", []):
            for span in line.get("spans", []):
                bbox = span.get("bbox")
                if not bbox:
                    continue
                span_bboxes.append({
                    "bbox": bbox,
                    "font": span.get("font", "?"),
                    "size": span.get("size", 0),
                    "text": span.get("text", ""),
                })

    # Collect black rectangles
    try:
        drawings = page.get_drawings()
    except Exception:
        drawings = []

    for dr in drawings:
        if dr.get("type") not in ("f", "fs"):
            continue
        color = dr.get("fill") or (0, 0, 0)
        if not all(c <= 0.15 for c in color[:3]):
            continue
        for item in dr.get("items", []):
            if not item or item[0] != "re":
                continue
            try:
                rect = item[1]
                rw = rect.width
                rh = rect.height
            except Exception:
                continue
            if rw < 20 or rh < 5:
                continue

            # Find the nearest intact span on the same y-line
            rect_cx = (rect.x0 + rect.x1) / 2
            rect_cy = (rect.y0 + rect.y1) / 2
            nearest = None
            best_dist = float("inf")
            for sp in span_bboxes:
                sb = sp["bbox"]
                span_cy = (sb[1] + sb[3]) / 2
                y_dist = abs(span_cy - rect_cy)
                if y_dist > max(rh, 15):  # not on same line
                    continue
                x_dist = min(
                    abs(sb[0] - rect.x1),  # span starts after rect
                    abs(sb[2] - rect.x0),  # span ends before rect
                )
                total = y_dist * 2 + x_dist
                if total < best_dist:
                    best_dist = total
                    nearest = sp

            # Compute local char width
            local_avg = None
            font_used = "unknown"
            size_used = 0
            if nearest and nearest["text"].strip():
                span_w = nearest["bbox"][2] - nearest["bbox"][0]
                if len(nearest["text"]) > 0:
                    local_avg = span_w / len(nearest["text"])
                    font_used = nearest["font"]
                    size_used = nearest["size"]
            if local_avg is None:
                local_avg = global_avg

            if not local_avg:
                continue

            est_chars = rw / local_avg

            # Enumerate candidates that fit
            fitting = []
            for cand in candidates:
                # Use candidate length as quick filter
                cand_est_w = estimate_candidate_width(cand, font_used, size_used, char_widths)
                if cand_est_w is None:
                    # fallback to global
                    cand_est_w = len(cand) * local_avg
                delta = abs(cand_est_w - rw) / rw
                if delta <= tolerance:
                    fitting.append({
                        "candidate": cand,
                        "est_width": round(cand_est_w, 1),
                        "delta_pct": round(delta * 100, 1),
                    })
            # Rank by closest fit
            fitting.sort(key=lambda x: x["delta_pct"])

            results.append({
                "bbox": [round(rect.x0, 1), round(rect.y0, 1),
                         round(rect.x1, 1), round(rect.y1, 1)],
                "width": round(rw, 1),
                "height": round(rh, 1),
                "est_chars": round(est_chars, 1),
                "font": font_used,
                "size": round(size_used, 1),
                "nearest_intact_span": (nearest["text"][:80] if nearest else None),
                "candidates": fitting[:10],
            })

    return results


def analyze_pdf(pdf_path: Path, output_dir: Path, candidates: list[str],
                tolerance: float = 0.15, only_pages: set[int] | None = None) -> dict:
    import fitz

    output_dir.mkdir(parents=True, exist_ok=True)
    doc = fitz.open(str(pdf_path))
    total_boxes = 0
    total_pages = len(doc)
    pages_analyzed = 0

    lines = [
        f"# Box-width inference: `{pdf_path.name}`",
        "",
        f"- Pages:       {total_pages}",
        f"- Tolerance:   ±{tolerance*100:.0f}%",
        f"- Candidates:  {len(candidates)}",
        "",
        "---",
        "",
    ]

    try:
        for i in range(total_pages):
            if only_pages and (i + 1) not in only_pages:
                continue
            page = doc[i]
            page_results = analyze_page(page, candidates, tolerance)
            if not page_results:
                continue
            pages_analyzed += 1
            lines.append(f"## Page {i+1}  —  {len(page_results)} redaction box(es)")
            lines.append("")
            for j, box in enumerate(page_results, 1):
                total_boxes += 1
                lines.append(
                    f"### Box {j}  ({box['width']}×{box['height']}px, "
                    f"est ~{box['est_chars']:.0f} chars, font {box['font']} {box['size']}pt)"
                )
                if box["nearest_intact_span"]:
                    lines.append(f"- **Context**: `{box['nearest_intact_span']}`")
                if box["candidates"]:
                    lines.append(f"- **Candidates fitting the width** ({len(box['candidates'])}):")
                    for c in box["candidates"]:
                        lines.append(
                            f"  - `{c['candidate']}`  (est {c['est_width']}px, Δ{c['delta_pct']}%)"
                        )
                else:
                    lines.append("- _No candidates fit within tolerance_")
                lines.append("")
    finally:
        doc.close()

    summary_lines = [
        f"- Pages with redaction boxes: {pages_analyzed}",
        f"- Total redaction boxes:      {total_boxes}",
        "",
    ]
    lines[5:5] = summary_lines  # inject after header
    out_path = output_dir / f"{pdf_path.stem}.box_width.md"
    out_path.write_text("\n".join(lines), encoding="utf-8")

    return {
        "pdf": str(pdf_path),
        "pages_analyzed": pages_analyzed,
        "total_boxes": total_boxes,
        "report": str(out_path),
    }


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="python -m tools.unredact.box_width")
    p.add_argument("target", help="PDF file or directory")
    p.add_argument("--output", default=r"C:\Users\atayl\Desktop\Excluded\unredact\box_width")
    p.add_argument("--names", help="Text file with candidate names (one per line)")
    p.add_argument("--tolerance", type=float, default=0.15,
                   help="Width tolerance (0.15 = +/- 15 pct)")
    p.add_argument("--pages", help="Comma/range list of pages, e.g. '5,7,266-279'")
    args = p.parse_args(argv)

    target = Path(args.target).resolve()
    output_dir = Path(args.output).resolve()
    candidates = load_candidates(Path(args.names).resolve() if args.names else None)

    only_pages = None
    if args.pages:
        only_pages = set()
        for part in args.pages.split(","):
            part = part.strip()
            if "-" in part:
                a, b = part.split("-", 1)
                only_pages.update(range(int(a), int(b) + 1))
            else:
                only_pages.add(int(part))

    if target.is_file():
        files = [target]
    elif target.is_dir():
        files = sorted(target.rglob("*.pdf"))
    else:
        print(f"ERROR: {target} not found", file=sys.stderr)
        return 2

    print(f"Output:       {output_dir}")
    print(f"Candidates:   {len(candidates)}")
    print(f"Tolerance:    ±{args.tolerance*100:.0f}%")
    print(f"Files:        {len(files)}")
    print()

    t0 = time.time()
    for f in files:
        print(f"[{f.name}]")
        r = analyze_pdf(f, output_dir, candidates, tolerance=args.tolerance,
                        only_pages=only_pages)
        print(f"  {r['total_boxes']} boxes across {r['pages_analyzed']} pages")
        print(f"  → {r['report']}")
    print()
    print(f"Done in {time.time() - t0:.1f}s")
    return 0


if __name__ == "__main__":
    sys.exit(main())
