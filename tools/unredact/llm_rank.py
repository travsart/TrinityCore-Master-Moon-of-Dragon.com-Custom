"""LLM-guided candidate ranking for redaction boxes.

Combines `tools.unredact.box_width` (width-based candidates) with a local
LLM (Ollama qwen3.5:27b by default) to rank those candidates by
contextual fit. The LLM sees the full visible text around each redaction
box and picks the most likely candidate from the CLOSED set produced by
box_width — it does NOT generate new content.

Flow:
    1. For each page with redaction boxes, extract the full page text
    2. For each box, locate the nearest visible text span (already done
       by box_width) and take a wider context window
    3. Build a prompt: visible context + "which of these candidates is
       most likely redacted here?"
    4. LLM returns JSON: ranked candidate order + confidence + reasoning
    5. Merge with box_width output to produce enhanced report

Usage:
    python -m tools.unredact.llm_rank <pdf>
    python -m tools.unredact.llm_rank <pdf> --page 5
    python -m tools.unredact.llm_rank <pdf> --model gemma4:26b
    python -m tools.unredact.llm_rank <pdf> --max-boxes 30

Consumes:
    C:/Users/atayl/Desktop/Excluded/unredact/box_width/<stem>.box_width.md

Produces:
    C:/Users/atayl/Desktop/Excluded/unredact/llm_rank/<stem>.llm_rank.md
"""

from __future__ import annotations

import argparse
import json
import sys
import time
import urllib.request
import urllib.error
from pathlib import Path

if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

from .box_width import DEFAULT_CANDIDATES, analyze_page
from .llm_match import CASE_CONTEXT_PRIMER, OLLAMA_URL

DEFAULT_MODEL = "qwen3.5:27b-q4_K_M"
DEFAULT_OUTPUT = Path(r"C:\Users\atayl\Desktop\Excluded\unredact\llm_rank")

RANK_SYSTEM = """You are a forensic document analyst helping identify the text hidden under black-rectangle redactions in a military legal case binder.

You will receive:
  1. The full visible text of a page with known redactions
  2. A specific redaction box's estimated character count and font
  3. A CLOSED LIST of candidate strings whose rendered width matches the box

Your job is to RANK those candidates by how likely each one is to be what was redacted, based ONLY on the visible context. You do NOT generate new candidates. You do NOT invent text. If no candidate plausibly fits the context, say so.

Output MUST be valid JSON only, no prose:
{
  "ranked_candidates": [
    {"candidate": "string from the list", "score": 0.0-1.0, "why": "1-sentence reason citing specific visible text"},
    ...
  ],
  "best_guess": "the top candidate or null",
  "confidence": "high | medium | low | none",
  "notes": "optional 1-sentence observation"
}

Rules:
- Only use candidates from the provided list
- Cite visible text as evidence in 'why'
- 'high' confidence requires BOTH: (a) top candidate is consistent with visible context AND (b) top candidate's score is significantly higher than the second
- If the visible context doesn't disambiguate, set confidence='low' or 'none' and explain"""


def ollama_generate(model: str, prompt: str, timeout: int = 180) -> str:
    payload = {
        "model": model,
        "prompt": prompt,
        "stream": False,
        "options": {"temperature": 0.1, "num_ctx": 8192},
    }
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(
        OLLAMA_URL, data=data,
        headers={"Content-Type": "application/json"},
    )
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        body = resp.read().decode("utf-8", errors="replace")
    try:
        return json.loads(body).get("response", "") or ""
    except json.JSONDecodeError:
        return body


def parse_json_response(raw: str) -> dict:
    start = raw.find("{")
    end = raw.rfind("}")
    if start < 0 or end < 0:
        return {"error": "no JSON in response", "raw": raw[:500]}
    try:
        return json.loads(raw[start:end + 1])
    except json.JSONDecodeError as e:
        return {"error": f"invalid JSON: {e}", "raw": raw[start:end + 1][:500]}


def rank_box(page_text: str, box: dict, model: str) -> dict:
    """Ask the LLM to rank candidates for one redaction box."""
    candidates = box.get("candidates") or []
    if not candidates:
        return {"skip": "no candidates from box_width"}

    context = (box.get("nearest_intact_span") or "").strip()
    candidate_lines = "\n".join(
        f"  - {c['candidate']}  (est {c.get('est_width', 0)}px, width delta {c.get('delta_pct', 0)}%)"
        for c in candidates
    )

    prompt = f"""{RANK_SYSTEM}

{CASE_CONTEXT_PRIMER}

---

FULL VISIBLE PAGE TEXT (may have gaps where other boxes were redacted):
{page_text[:3500]}

---

TARGET REDACTION BOX:
- Size:       {box.get('width')} x {box.get('height')} pixels
- Est chars:  ~{box.get('est_chars')}
- Font:       {box.get('font')} {box.get('size')}pt
- Nearest intact text (immediate context): "{context[:200]}"

CLOSED LIST OF WIDTH-FITTING CANDIDATES:
{candidate_lines}

Return ONLY valid JSON matching the schema above."""

    try:
        raw = ollama_generate(model, prompt)
    except urllib.error.URLError as e:
        return {"error": f"Ollama unreachable: {e}"}
    except Exception as e:
        return {"error": f"LLM call failed: {e}"}

    return parse_json_response(raw)


def process_pdf(pdf_path: Path, output_dir: Path, model: str,
                only_page: int | None = None,
                max_boxes: int | None = None,
                min_candidates: int = 2) -> dict:
    """For each redaction box on each page of the PDF, ask the LLM to rank candidates."""
    import fitz
    from .box_width import load_candidates
    output_dir.mkdir(parents=True, exist_ok=True)

    doc = fitz.open(str(pdf_path))
    candidates_list = DEFAULT_CANDIDATES
    total_pages = len(doc)
    pages_processed = 0
    boxes_processed = 0
    results: list[dict] = []
    t0 = time.time()

    try:
        for i in range(total_pages):
            if only_page is not None and (i + 1) != only_page:
                continue
            page = doc[i]
            text = (page.get_text("text") or "").strip()
            if not text:
                continue
            boxes = analyze_page(page, candidates_list, tolerance=0.15)
            if not boxes:
                continue
            # Focus on boxes with multiple candidates (ambiguous — LLM adds most value)
            to_rank = [b for b in boxes if len(b.get("candidates") or []) >= min_candidates]
            if not to_rank:
                continue

            pages_processed += 1
            print(f"  [page {i+1}] {len(to_rank)} boxes to rank ({len(boxes) - len(to_rank)} skipped)")

            for j, box in enumerate(to_rank):
                if max_boxes is not None and boxes_processed >= max_boxes:
                    break
                t_box = time.time()
                resp = rank_box(text, box, model)
                elapsed = time.time() - t_box
                print(f"    box {j+1}: {elapsed:.1f}s  ", end="")
                if "error" in resp:
                    print(f"ERROR: {resp['error']}")
                elif "skip" in resp:
                    print(f"SKIP: {resp['skip']}")
                else:
                    best = resp.get("best_guess") or "(none)"
                    conf = resp.get("confidence", "?")
                    print(f"best={best[:40]!r}  conf={conf}")
                results.append({
                    "page": i + 1,
                    "box_idx": j + 1,
                    "box": box,
                    "llm": resp,
                    "elapsed": round(elapsed, 1),
                })
                boxes_processed += 1
            if max_boxes is not None and boxes_processed >= max_boxes:
                break
    finally:
        doc.close()

    elapsed_total = time.time() - t0

    lines = [
        f"# LLM-ranked redaction candidates: `{pdf_path.name}`",
        "",
        f"- Model:        {model}",
        f"- Pages scanned: {total_pages}",
        f"- Pages with ranked boxes: {pages_processed}",
        f"- Boxes ranked: {boxes_processed}",
        f"- Elapsed:      {elapsed_total:.1f}s",
        "",
        "---",
        "",
    ]

    # Group by page
    by_page: dict[int, list[dict]] = {}
    for r in results:
        by_page.setdefault(r["page"], []).append(r)

    for page_num in sorted(by_page.keys()):
        page_results = by_page[page_num]
        lines.append(f"## Page {page_num}")
        lines.append("")
        for r in page_results:
            box = r["box"]
            llm = r["llm"]
            lines.append(
                f"### Box {r['box_idx']}  ({box['width']}×{box['height']}px, "
                f"est ~{box['est_chars']:.0f} chars)"
            )
            if box.get("nearest_intact_span"):
                lines.append(f"- **Context**: `{box['nearest_intact_span'][:120]}`")
            if "error" in llm:
                lines.append(f"- **LLM ERROR**: {llm['error']}")
            elif "skip" in llm:
                lines.append(f"- **Skipped**: {llm['skip']}")
            else:
                best = llm.get("best_guess")
                conf = llm.get("confidence", "?")
                lines.append(f"- **Best guess**: `{best}`  (confidence: **{conf}**)")
                if llm.get("notes"):
                    lines.append(f"- **Notes**: {llm['notes']}")
                ranked = llm.get("ranked_candidates") or []
                if ranked:
                    lines.append(f"- **LLM ranking**:")
                    for rc in ranked[:5]:
                        score = rc.get("score", 0)
                        lines.append(
                            f"  - `{rc.get('candidate', '?')}`  "
                            f"(score {score:.2f}) — {rc.get('why', '')[:150]}"
                        )
            lines.append("")

    out_path = output_dir / f"{pdf_path.stem}.llm_rank.md"
    out_path.write_text("\n".join(lines), encoding="utf-8")
    return {
        "pdf": str(pdf_path),
        "pages_processed": pages_processed,
        "boxes_ranked": boxes_processed,
        "elapsed": elapsed_total,
        "report": str(out_path),
    }


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="python -m tools.unredact.llm_rank")
    p.add_argument("pdf")
    p.add_argument("--output", default=str(DEFAULT_OUTPUT))
    p.add_argument("--model", default=DEFAULT_MODEL)
    p.add_argument("--page", type=int, default=None)
    p.add_argument("--max-boxes", type=int, default=None,
                   help="Cap total boxes processed (LLM is slow)")
    p.add_argument("--min-candidates", type=int, default=2,
                   help="Skip boxes with fewer than this many width-candidates")
    args = p.parse_args(argv)

    pdf = Path(args.pdf).resolve()
    if not pdf.exists():
        print(f"ERROR: {pdf} not found", file=sys.stderr)
        return 2

    try:
        with urllib.request.urlopen("http://localhost:11434/api/tags", timeout=5) as r:
            tags = json.loads(r.read())
        model_names = [m.get("name") for m in tags.get("models", [])]
        if args.model not in model_names:
            matches = [m for m in model_names if "qwen" in m.lower() or "gemma" in m.lower()]
            if matches:
                print(f"WARN: {args.model} not in Ollama. Using {matches[0]}")
                args.model = matches[0]
    except Exception as e:
        print(f"ERROR: Ollama unreachable: {e}", file=sys.stderr)
        return 2

    output_dir = Path(args.output).resolve()
    print(f"PDF:     {pdf}")
    print(f"Model:   {args.model}")
    print(f"Output:  {output_dir}")
    print()

    r = process_pdf(pdf, output_dir, args.model,
                    only_page=args.page,
                    max_boxes=args.max_boxes,
                    min_candidates=args.min_candidates)
    print()
    print("=" * 60)
    print(f"Done in {r['elapsed']:.1f}s")
    print(f"  pages: {r['pages_processed']}, boxes ranked: {r['boxes_ranked']}")
    print(f"Report: {r['report']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
