"""LLM-assisted document identification via local Ollama.

Given a redacted page with some visible text, asks a local LLM to identify
WHAT kind of document it is, based on context clues. This is NOT for
generating fake text — it's for classifying ambiguous pages so we know which
known source document to look for as the unredacted alternate.

The LLM is explicitly instructed to:
    * Never invent content that isn't in the visible context
    * Reply with a structured guess at document type + confidence
    * Flag uncertainty rather than hallucinate

Usage:
    python -m tools.unredact.llm_match <pdf>
    python -m tools.unredact.llm_match <pdf> --page 234
    python -m tools.unredact.llm_match <pdf> --model qwen3.5:27b-q4_K_M

Requires:
    Ollama running on localhost:11434 with the model pulled.
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

OLLAMA_URL = "http://localhost:11434/api/generate"
DEFAULT_MODEL = "qwen3.5:27b-q4_K_M"
DEFAULT_OUTPUT = Path(r"C:\Users\atayl\Desktop\Excluded\unredact\llm_matches")

SYSTEM_INSTRUCTION = """You are a forensic document analyst reviewing partially-redacted pages from a legal case archive. Your sole job is to IDENTIFY what kind of document each page is and, if possible, which SPECIFIC document from a known archive it matches. You must NEVER generate, guess, or invent content that is not in the visible text provided. If you are uncertain, say so explicitly.

Your output must be valid JSON with this exact schema:
{
  "document_type": "short classification — 'email', 'form', 'memo', 'letter', 'report', 'transcript', 'unknown', etc.",
  "specific_match": "name of a known document this matches, or null",
  "confidence": "high | medium | low | unknown",
  "reasoning": "1-3 sentences citing SPECIFIC phrases from the visible text. No invention.",
  "redacted_fields_likely": ["list of fields that appear to be redacted — 'sender', 'recipient', 'subject', etc."],
  "suggested_search": "a 4-8 word phrase to search other archives for the unredacted source"
}

Do NOT output anything other than the JSON."""


CASE_CONTEXT_PRIMER = """This is the case archive context:

Case: Capt Adam J. Taylor, USAF, Licensed Clinical Social Worker (LCSW), Mental Health clinic at 27 SOMRS / 27 SOMDG, Cannon Air Force Base, New Mexico. A Quality Assurance Investigation (QAI) was opened in July 2024 after SSgt Brittany Jarvis made an unauthorized audio recording of a clinical encounter at the MH clinic. Col Danielle Cermak (AF Surgeon General's office) coordinated summary suspension on 14 August 2024. Col William Earles (Privileging Authority) imposed full revocation on 15 October 2024 overriding the PRHP panel. Documents from this period include:

- Continuous Vetting Incident Reports to DoD AVS
- Summary Suspension notifications (14 Aug 2024)
- PRHP (Peer Review Hearing Panel) exhibits
- QAI interview MFRs
- Email chains: "Capt Taylor Disciplinary Recommendation" (Jul-Aug 2024)
- Cermak coordination emails (Jul 11 2024)
- DD Form 2005 (MH Evaluation Request) printouts
- NPDB (Notice to National Practitioner Data Bank) filings
- Tolin Appeal (25 Oct 2024)
- Wareham litigation hold letter (Oct 17 2024)

Known people:
Col Danielle Cermak (AF-SG), Col William Earles (PA), Col John McMaster (27 SOMRS/CC), Col Chad Johnston (27 SOW/CC), Lt Col Garro, Lt Col Corpening, MSgt Jenality Wheeler, MSgt Samantha Webber, Capt Johnny Campbell, Capt Anthony Lawrence, Laura Iandoli LCSW, Capt Daniel Ko (former VLC), Capt Elliot Ko (ADC), SSgt Brittany Jarvis, SSgt Fain, TSgt Gebhardt, Jackie Burns, Jason Wareham (Allen Vellone), Joshua Tolin (Veritas Military Law)."""


def ollama_generate(model: str, prompt: str, timeout: int = 120) -> str:
    """Call Ollama's /api/generate (non-streaming) and return the full response string."""
    payload = {
        "model": model,
        "prompt": prompt,
        "stream": False,
        "options": {
            "temperature": 0.1,  # low for deterministic identification
            "num_ctx": 8192,
        },
    }
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(
        OLLAMA_URL, data=data,
        headers={"Content-Type": "application/json"},
    )
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        body = resp.read().decode("utf-8", errors="replace")
    try:
        obj = json.loads(body)
    except json.JSONDecodeError:
        return body
    return obj.get("response", "") or ""


def parse_llm_response(raw: str) -> dict:
    """Parse the LLM's JSON response, tolerating wrapping text."""
    # Find first { and last }
    start = raw.find("{")
    end = raw.rfind("}")
    if start < 0 or end < 0 or end <= start:
        return {"error": "no JSON in response", "raw": raw[:500]}
    try:
        return json.loads(raw[start:end + 1])
    except json.JSONDecodeError as e:
        return {"error": f"invalid JSON: {e}", "raw": raw[start:end + 1][:500]}


def identify_page(page_text: str, byte_residue_hits: list[str],
                  model: str = DEFAULT_MODEL) -> dict:
    """Ask the LLM to identify one page."""
    if not page_text.strip():
        return {"error": "empty page text"}

    residue_note = ""
    if byte_residue_hits:
        residue_note = f"\n\nByte-residue hits (known names found in raw bytes): {', '.join(byte_residue_hits[:10])}"

    prompt = f"""{SYSTEM_INSTRUCTION}

{CASE_CONTEXT_PRIMER}

---

VISIBLE TEXT FROM REDACTED PAGE (may have gaps where content was blacked out):
{page_text[:4000]}{residue_note}

---

Return ONLY valid JSON matching the schema above."""

    try:
        raw = ollama_generate(model, prompt)
    except urllib.error.URLError as e:
        return {"error": f"Ollama unreachable: {e}"}
    except Exception as e:
        return {"error": f"LLM call failed: {e}"}

    return parse_llm_response(raw)


def process_pdf(pdf_path: Path, output_dir: Path, model: str,
                only_page: int | None = None) -> dict:
    import fitz
    output_dir.mkdir(parents=True, exist_ok=True)

    doc = fitz.open(str(pdf_path))
    total_pages = len(doc)
    results: list[dict] = []
    t0 = time.time()

    try:
        for i in range(total_pages):
            if only_page is not None and (i + 1) != only_page:
                continue
            page = doc[i]
            text = (page.get_text("text") or "").strip()
            if len(text) < 80:
                # Not enough context for LLM to do anything useful
                continue
            print(f"  [{i+1}/{total_pages}] ", end="", flush=True)
            t_page = time.time()
            resp = identify_page(text, [], model=model)
            elapsed = time.time() - t_page
            print(f"{elapsed:.1f}s  ", end="")
            if "error" in resp:
                print(f"ERROR: {resp['error']}")
                results.append({"page": i + 1, "error": resp["error"]})
                continue
            print(f"type={resp.get('document_type', '?')}  "
                  f"confidence={resp.get('confidence', '?')}")
            results.append({"page": i + 1, **resp})
    finally:
        doc.close()

    elapsed_total = time.time() - t0
    lines = [
        f"# LLM Identification: `{pdf_path.name}`",
        "",
        f"- Model:       {model}",
        f"- Pages:       {total_pages}",
        f"- Processed:   {len(results)}",
        f"- Elapsed:     {elapsed_total:.1f}s",
        "",
        "---",
        "",
    ]
    for r in results:
        lines.append(f"## Page {r['page']}")
        if "error" in r:
            lines.append(f"- **ERROR**: {r['error']}")
        else:
            lines.append(f"- **Document type**: {r.get('document_type', '?')}")
            if r.get('specific_match'):
                lines.append(f"- **Specific match**: {r['specific_match']}")
            lines.append(f"- **Confidence**: {r.get('confidence', '?')}")
            if r.get('redacted_fields_likely'):
                lines.append(f"- **Likely-redacted fields**: {', '.join(r['redacted_fields_likely'])}")
            if r.get('suggested_search'):
                lines.append(f"- **Suggested search**: `{r['suggested_search']}`")
            lines.append(f"- **Reasoning**: {r.get('reasoning', '')}")
        lines.append("")

    out_path = output_dir / f"{pdf_path.stem}.llm_match.md"
    out_path.write_text("\n".join(lines), encoding="utf-8")
    return {
        "pdf": str(pdf_path),
        "total_pages": total_pages,
        "processed": len(results),
        "elapsed": elapsed_total,
        "report": str(out_path),
    }


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="python -m tools.unredact.llm_match")
    p.add_argument("pdf")
    p.add_argument("--output", default=str(DEFAULT_OUTPUT))
    p.add_argument("--model", default=DEFAULT_MODEL)
    p.add_argument("--page", type=int, default=None,
                   help="Only process this specific 1-indexed page")
    args = p.parse_args(argv)

    pdf = Path(args.pdf).resolve()
    if not pdf.exists():
        print(f"ERROR: {pdf} not found", file=sys.stderr)
        return 2

    # Ping Ollama
    try:
        with urllib.request.urlopen("http://localhost:11434/api/tags", timeout=5) as r:
            tags = json.loads(r.read())
        model_names = [m.get("name") for m in tags.get("models", [])]
        if args.model not in model_names:
            print(f"WARN: model {args.model} not in Ollama. Available: {model_names}")
            # Try to find a similar one
            matches = [m for m in model_names if "qwen" in m.lower() or "gemma" in m.lower()]
            if matches:
                print(f"  using {matches[0]} instead")
                args.model = matches[0]
    except Exception as e:
        print(f"ERROR: Ollama not reachable at localhost:11434: {e}", file=sys.stderr)
        return 2

    output_dir = Path(args.output).resolve()
    print(f"PDF:    {pdf}")
    print(f"Model:  {args.model}")
    print(f"Output: {output_dir}")
    print()

    r = process_pdf(pdf, output_dir, args.model, only_page=args.page)
    print()
    print("=" * 60)
    print(f"Done in {r['elapsed']:.1f}s ({r['processed']} pages processed)")
    print(f"Report: {r['report']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
