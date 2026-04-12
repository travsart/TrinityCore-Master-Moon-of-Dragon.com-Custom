"""Remote-AI-powered redaction candidate ranker (ChatGPT + Gemini).

Drop-in faster/stronger replacement for tools.unredact.llm_rank. Uses the
existing Triad API credentials (from tools/ai_studio/.env or
config/*.local.env) and batches many redaction boxes per API call.

Design decisions (after timing out on local 27B qwen @ 182s/box):
  * **Remote API** — gpt-5.4 or gemini-3.1-pro are ~100x faster and more
    accurate on constrained reasoning than local quantized 27B.
  * **Batch mode** — up to N boxes per single API call with a structured
    JSON output contract. One call handles a whole page's redactions.
  * **Short prompts** — case context is a compressed ~400-token primer
    instead of the full history.
  * **Structured output** — OpenAI's `response_format={type: json_object}`
    for reliable JSON parsing.
  * **Optional parallelism** — batches run concurrently via ThreadPoolExecutor

Usage:
    # ChatGPT (default)
    python -m tools.unredact.ai_rank <pdf>
    python -m tools.unredact.ai_rank <pdf> --backend chatgpt --model gpt-5.4
    python -m tools.unredact.ai_rank <pdf> --page 5
    python -m tools.unredact.ai_rank <pdf> --max-pages 10

    # Gemini
    python -m tools.unredact.ai_rank <pdf> --backend gemini --model gemini-3.1-pro

    # Parallelism
    python -m tools.unredact.ai_rank <pdf> --workers 4
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import time
import traceback
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

from .box_width import DEFAULT_CANDIDATES, analyze_page

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_OUTPUT = Path(r"C:\Users\atayl\Desktop\Excluded\unredact\ai_rank")

# =========================================================================
# Env loading (reuse Triad pattern, no dotenv dependency)
# =========================================================================


def load_env() -> None:
    candidates = [
        REPO_ROOT / "tools" / "ai_studio" / ".env",
        REPO_ROOT / "config" / "api_architect.local.env",
        REPO_ROOT / "config" / "gemini.local.env",
    ]
    for env_path in candidates:
        if not env_path.exists():
            continue
        try:
            for line in env_path.read_text(encoding="utf-8").splitlines():
                line = line.strip()
                if not line or line.startswith("#") or "=" not in line:
                    continue
                k, v = line.split("=", 1)
                os.environ.setdefault(k.strip(), v.strip().strip('"\''))
        except Exception:
            pass


# =========================================================================
# Compressed case context primer (~400 tokens — was ~1500)
# =========================================================================

CASE_PRIMER_SHORT = """Case: Capt Adam J. Taylor, USAF LCSW, Cannon AFB (27 SOMRS MH clinic). July 2024 QAI opened after SSgt Brittany Jarvis's unauthorized MH clinic recording. Col Danielle Cermak (AF-SG) coordinated summary suspension 14 Aug 2024; Col William Earles (PA) imposed full revocation 15 Oct 2024 overriding PRHP.

Known people: Cermak (AF-SG), Earles (PA), McMaster (27 SOMRS/CC), Johnston (27 SOW/CC), Garro, Corpening, Wheeler, Webber, Campbell, Lawrence, Iandoli, Daniel Ko (former VLC), Elliot Ko (ADC), Jarvis, Fain, Gebhardt, Burns, Wareham (attorney), Tolin (attorney), Stringer (DHA), Cermak's email: danielle.j.cermak.mil@health.mil.

Known documents in this binder: Continuous Vetting Incident Reports to DoD AVS, Summary Suspension notifications, PRHP exhibits, QAI interview MFRs, Disciplinary Recommendation email chain (Jul 24-Aug 2 2024), Cermak coordination emails (Jul 11 2024), DD Form 2005 (MH Eval), NPDB filing, Tolin Appeal (25 Oct 2024), Wareham litigation hold (17 Oct 2024)."""


SYSTEM_PROMPT = """You are a forensic document analyst. You are given a PDF page with visible text and one or more redaction boxes. For each box you receive a CLOSED list of candidate strings whose rendered width fits that box.

Your job: rank the candidates by how likely each one is to be what was actually redacted, based ONLY on the visible context on that same page. You do NOT invent content. You do NOT suggest candidates outside the provided list.

Output format: STRICT JSON only. No prose, no markdown code fences.

Schema:
{
  "boxes": [
    {
      "box_id": "string matching the id in the input",
      "best_guess": "the top candidate or null if none fits",
      "confidence": "high|medium|low|none",
      "ranked": [{"candidate": "str", "score": 0.0, "why": "cite visible text briefly"}],
      "notes": "optional 1-sentence observation"
    }
  ]
}

Rules:
- Only pick from the provided candidate list for each box
- "high" requires BOTH: top candidate is consistent with visible context AND its score is at least 0.25 higher than the second
- "why" must cite specific visible text fragments (no invention)
- If context is insufficient to distinguish candidates, use confidence "low" or "none" """


# =========================================================================
# Backend adapters
# =========================================================================


def call_chatgpt(prompt: str, model: str = "gpt-5.4", timeout: int = 120) -> str:
    from openai import OpenAI
    api_key = os.environ.get("OPENAI_API_KEY", "")
    if not api_key:
        raise RuntimeError("OPENAI_API_KEY not set")
    client = OpenAI(api_key=api_key, timeout=timeout)
    resp = client.chat.completions.create(
        model=model,
        messages=[
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user", "content": prompt},
        ],
        response_format={"type": "json_object"},
        temperature=0.1,
    )
    return resp.choices[0].message.content or ""


def call_gemini(prompt: str, model: str = "gemini-3.1-pro", timeout: int = 120) -> str:
    from google import genai
    from google.genai import types
    api_key = os.environ.get("GOOGLE_API_KEY", "")
    if not api_key:
        raise RuntimeError("GOOGLE_API_KEY not set")
    client = genai.Client(api_key=api_key)
    full = SYSTEM_PROMPT + "\n\n" + prompt
    try:
        resp = client.models.generate_content(
            model=model,
            contents=full,
            config=types.GenerateContentConfig(
                temperature=0.1,
                response_mime_type="application/json",
            ),
        )
    except Exception:
        # Try older model name if the requested one isn't available
        resp = client.models.generate_content(
            model="gemini-2.5-pro",
            contents=full,
            config=types.GenerateContentConfig(
                temperature=0.1,
                response_mime_type="application/json",
            ),
        )
    return (resp.text or "").strip()


BACKENDS = {
    "chatgpt": call_chatgpt,
    "gemini": call_gemini,
}

DEFAULT_MODELS = {
    "chatgpt": "gpt-5.4",
    "gemini": "gemini-3.1-pro",
}


# =========================================================================
# Batching
# =========================================================================


def build_batch_prompt(page_num: int, page_text: str, boxes: list[dict]) -> str:
    box_lines = []
    for i, b in enumerate(boxes, 1):
        candidates = b.get("candidates") or []
        cand_str = ", ".join(f'"{c["candidate"]}"' for c in candidates[:15])
        context = (b.get("nearest_intact_span") or "").strip()[:180]
        box_lines.append(
            f'- box_id="p{page_num}_b{i}"\n'
            f'  size: {b.get("width")}x{b.get("height")} px, est ~{b.get("est_chars")} chars, '
            f'font {b.get("font")} {b.get("size")}pt\n'
            f'  nearest text: "{context}"\n'
            f"  candidates: [{cand_str}]"
        )
    boxes_block = "\n".join(box_lines)

    return f"""{CASE_PRIMER_SHORT}

PAGE {page_num} VISIBLE TEXT (may have gaps where redactions are):
```
{page_text[:3500]}
```

REDACTION BOXES ON THIS PAGE ({len(boxes)} total):
{boxes_block}

Return ONLY valid JSON matching the schema above. Include one entry in `boxes` for every box_id above."""


def parse_json_lenient(raw: str) -> dict:
    raw = (raw or "").strip()
    # Strip code fences if present
    if raw.startswith("```"):
        lines = raw.splitlines()
        if lines[0].startswith("```"):
            lines = lines[1:]
        if lines and lines[-1].startswith("```"):
            lines = lines[:-1]
        raw = "\n".join(lines)
    start = raw.find("{")
    end = raw.rfind("}")
    if start < 0 or end < 0:
        return {"error": "no JSON", "raw": raw[:500]}
    try:
        return json.loads(raw[start:end + 1])
    except json.JSONDecodeError as e:
        return {"error": f"bad JSON: {e}", "raw": raw[start:end + 1][:800]}


# =========================================================================
# Per-page worker
# =========================================================================


def rank_page(backend_fn, model: str, page_num: int, page_text: str,
              boxes: list[dict]) -> dict:
    t0 = time.time()
    try:
        prompt = build_batch_prompt(page_num, page_text, boxes)
        raw = backend_fn(prompt, model=model)
        parsed = parse_json_lenient(raw)
    except Exception as e:
        return {
            "page": page_num,
            "error": f"{type(e).__name__}: {e}",
            "elapsed": round(time.time() - t0, 1),
        }
    return {
        "page": page_num,
        "boxes": boxes,
        "llm_result": parsed,
        "elapsed": round(time.time() - t0, 1),
    }


# =========================================================================
# Main driver
# =========================================================================


def process_pdf(pdf_path: Path, output_dir: Path, *,
                backend: str, model: str,
                only_page: int | None = None,
                max_pages: int | None = None,
                min_candidates: int = 2,
                workers: int = 1) -> dict:
    import fitz

    load_env()
    output_dir.mkdir(parents=True, exist_ok=True)
    backend_fn = BACKENDS[backend]

    doc = fitz.open(str(pdf_path))
    total_pages = len(doc)
    tasks = []

    try:
        for i in range(total_pages):
            page_num = i + 1
            if only_page is not None and page_num != only_page:
                continue
            page = doc[i]
            text = (page.get_text("text") or "").strip()
            if not text:
                continue
            boxes = analyze_page(page, DEFAULT_CANDIDATES, tolerance=0.15)
            to_rank = [b for b in boxes if len(b.get("candidates") or []) >= min_candidates]
            if not to_rank:
                continue
            tasks.append((page_num, text, to_rank))
            if max_pages is not None and len(tasks) >= max_pages:
                break
    finally:
        doc.close()

    if not tasks:
        return {"pdf": str(pdf_path), "error": "no pages with rankable boxes"}

    total_boxes = sum(len(t[2]) for t in tasks)
    print(f"Queued {len(tasks)} pages, {total_boxes} total boxes, {workers} worker(s)")
    t_total = time.time()
    results: list[dict] = []

    if workers > 1:
        with ThreadPoolExecutor(max_workers=workers) as ex:
            futures = {
                ex.submit(rank_page, backend_fn, model, p, t, b): p
                for p, t, b in tasks
            }
            done = 0
            for fut in as_completed(futures):
                r = fut.result()
                results.append(r)
                done += 1
                if r.get("error"):
                    print(f"  [{done}/{len(tasks)}] page {r['page']}: ERROR {r['error']}")
                else:
                    n_box = len(r.get("boxes") or [])
                    print(f"  [{done}/{len(tasks)}] page {r['page']}: {n_box} boxes in {r['elapsed']}s")
    else:
        for p, t, b in tasks:
            r = rank_page(backend_fn, model, p, t, b)
            results.append(r)
            if r.get("error"):
                print(f"  page {r['page']}: ERROR {r['error']}")
            else:
                print(f"  page {r['page']}: {len(r.get('boxes') or [])} boxes in {r['elapsed']}s")

    results.sort(key=lambda r: r["page"])
    elapsed_total = time.time() - t_total

    # Write report
    lines = [
        f"# AI-ranked redactions: `{pdf_path.name}`",
        "",
        f"- Backend:     {backend}",
        f"- Model:       {model}",
        f"- Pages ranked: {len(results)}",
        f"- Total boxes: {total_boxes}",
        f"- Elapsed:     {elapsed_total:.1f}s  ({total_boxes / elapsed_total:.1f} boxes/sec)",
        "",
        "---",
        "",
    ]

    high_conf_summary: list[tuple[int, str, str, str]] = []  # (page, box_id, best, why)

    for r in results:
        page = r["page"]
        lines.append(f"## Page {page}  ({r.get('elapsed', 0)}s)")
        lines.append("")
        if r.get("error"):
            lines.append(f"- **ERROR**: {r['error']}")
            lines.append("")
            continue
        llm = r.get("llm_result") or {}
        if "error" in llm:
            lines.append(f"- **PARSE ERROR**: {llm['error']}")
            lines.append("")
            continue
        boxes_out = llm.get("boxes") or []
        box_by_id = {f"p{page}_b{i+1}": b for i, b in enumerate(r.get("boxes") or [])}
        for lb in boxes_out:
            bid = lb.get("box_id", "?")
            orig = box_by_id.get(bid) or {}
            best = lb.get("best_guess")
            conf = lb.get("confidence", "?")
            lines.append(
                f"### {bid}  ({orig.get('width', '?')}×{orig.get('height', '?')}px, "
                f"est ~{orig.get('est_chars', '?')} chars)"
            )
            context = orig.get("nearest_intact_span") or ""
            if context:
                lines.append(f"- **Context**: `{context[:120]}`")
            lines.append(f"- **Best**: `{best}`  (confidence: **{conf}**)")
            if lb.get("notes"):
                lines.append(f"- **Notes**: {lb['notes']}")
            ranked = lb.get("ranked") or []
            if ranked:
                lines.append("- **Ranked candidates**:")
                for rc in ranked[:5]:
                    score = rc.get("score", 0)
                    why = (rc.get("why") or "")[:180]
                    lines.append(f"  - `{rc.get('candidate', '?')}` (score {score:.2f}) — {why}")
            lines.append("")
            if conf == "high" and best:
                high_conf_summary.append((page, bid, best, ranked[0].get("why", "") if ranked else ""))

    # Prepend high-confidence summary
    if high_conf_summary:
        summary_block = [
            "## 🎯 High-confidence identifications",
            "",
            "| Page | Box | Best guess | Reasoning |",
            "|-----:|-----|-----------|-----------|",
        ]
        for p, bid, best, why in high_conf_summary:
            summary_block.append(f"| {p} | {bid} | `{best}` | {why[:120]} |")
        summary_block.append("")
        summary_block.append("---")
        summary_block.append("")
        lines[9:9] = summary_block

    out_path = output_dir / f"{pdf_path.stem}.ai_rank.md"
    out_path.write_text("\n".join(lines), encoding="utf-8")
    return {
        "pdf": str(pdf_path),
        "pages_processed": len(results),
        "total_boxes": total_boxes,
        "elapsed": elapsed_total,
        "high_confidence": len(high_conf_summary),
        "report": str(out_path),
    }


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="python -m tools.unredact.ai_rank")
    p.add_argument("pdf")
    p.add_argument("--output", default=str(DEFAULT_OUTPUT))
    p.add_argument("--backend", choices=list(BACKENDS.keys()), default="chatgpt")
    p.add_argument("--model", default=None,
                   help="Model name (default: gpt-5.4 or gemini-3.1-pro)")
    p.add_argument("--page", type=int, default=None)
    p.add_argument("--max-pages", type=int, default=None)
    p.add_argument("--min-candidates", type=int, default=2)
    p.add_argument("--workers", type=int, default=4,
                   help="Concurrent API calls (default 4)")
    args = p.parse_args(argv)

    model = args.model or DEFAULT_MODELS[args.backend]
    pdf = Path(args.pdf).resolve()
    if not pdf.exists():
        print(f"ERROR: {pdf} not found", file=sys.stderr)
        return 2

    output_dir = Path(args.output).resolve()
    print(f"PDF:     {pdf}")
    print(f"Backend: {args.backend}  ({model})")
    print(f"Output:  {output_dir}")
    print(f"Workers: {args.workers}")
    print()

    r = process_pdf(
        pdf, output_dir,
        backend=args.backend, model=model,
        only_page=args.page, max_pages=args.max_pages,
        min_candidates=args.min_candidates, workers=args.workers,
    )
    if "error" in r:
        print(f"ERROR: {r['error']}", file=sys.stderr)
        return 3
    print()
    print("=" * 60)
    print(f"Done in {r['elapsed']:.1f}s")
    print(f"  pages: {r['pages_processed']}, boxes: {r['total_boxes']}")
    print(f"  high-confidence IDs: {r['high_confidence']}")
    print(f"Report: {r['report']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
