#!/usr/bin/env python3
"""
Frontmatter Tagger — infer YAML frontmatter for untagged .md files via local LLM.

Scans a directory for markdown files, reads each one, and asks Ollama
(Qwen 3.5 27B by default) to generate a short description, tags, and doc_type.
Writes the inferred frontmatter back to the file (after dry-run confirmation).

Usage:
    python tools/frontmatter_tagger.py <path>                      # dry-run
    python tools/frontmatter_tagger.py <path> --execute            # write back
    python tools/frontmatter_tagger.py <path> --limit 5            # test small batch
    python tools/frontmatter_tagger.py <path> --model gemma4:26b   # alt model
    python tools/frontmatter_tagger.py <path> --force              # re-tag files that already have frontmatter

Required fields inferred per file:
    - description: one sentence (<=140 chars)
    - tags:        array of 3-6 short keywords
    - doc_type:    one of [note, spec, reference, report, log, checklist, todo, draft]

Preserves any existing frontmatter fields — only fills in what's missing.
Only processes .md files. Skips files under 50 bytes or over 200 KB.
Requires Ollama running at http://localhost:11434 (override with OLLAMA_HOST env var).
"""

import argparse
import json
import os
import re
import sys
import urllib.error
import urllib.request
from pathlib import Path

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

OLLAMA_HOST = os.environ.get("OLLAMA_HOST", "http://localhost:11434")
DEFAULT_MODEL = "qwen3.5:27b-q4_K_M"

# File size guardrails — skip trivial or oversize content
MIN_FILE_BYTES = 50
MAX_FILE_BYTES = 200 * 1024  # 200 KB

# Truncate very long files before sending to the model (first N chars)
MAX_PROMPT_CHARS = 8000

REQUIRED_FIELDS = ("description", "tags", "doc_type")

# Valid doc_type values — constrains the model's output
DOC_TYPES = ["note", "spec", "reference", "report", "log", "checklist", "todo", "draft"]

SYSTEM_PROMPT = (
    "You tag markdown files. Given the content of a markdown document, "
    "return a STRICT JSON object with exactly three fields:\n"
    '  - "description": one sentence (<=140 chars) summarizing what the file is for\n'
    '  - "tags": array of 3-6 short lowercase keywords (no spaces; use-dashes-or_underscores)\n'
    '  - "doc_type": one of [note, spec, reference, report, log, checklist, todo, draft]\n'
    "Return only the JSON object — no prose, no code fences, no explanation."
)


# ---------------------------------------------------------------------------
# Frontmatter parsing / merging
# ---------------------------------------------------------------------------

FRONTMATTER_RE = re.compile(r"^---\s*\n(.*?)\n---\s*\n", re.DOTALL)


def parse_frontmatter(text: str) -> tuple[dict, str]:
    """Return (frontmatter_dict, body_without_frontmatter).

    Uses a minimal YAML-ish parser — handles `key: value` and `key: [a, b]`
    which covers every frontmatter pattern we care about for tagging.
    Returns ({}, text) if no frontmatter block is present.
    """
    m = FRONTMATTER_RE.match(text)
    if not m:
        return {}, text

    raw = m.group(1)
    body = text[m.end():]
    data: dict = {}
    for line in raw.splitlines():
        if not line.strip() or line.strip().startswith("#"):
            continue
        if ":" not in line:
            continue
        key, _, value = line.partition(":")
        key = key.strip()
        value = value.strip()
        # Array form: [a, b, c]
        if value.startswith("[") and value.endswith("]"):
            inner = value[1:-1]
            data[key] = [
                v.strip().strip("\"'") for v in inner.split(",") if v.strip()
            ]
        else:
            data[key] = value.strip("\"'")
    return data, body


def render_frontmatter(data: dict) -> str:
    """Render a frontmatter dict back to YAML. Stable key order."""
    # Preferred field order — unknown keys appended alphabetically
    order = ["description", "tags", "doc_type", "people", "date", "filing_relevance"]
    keys = [k for k in order if k in data] + sorted(
        k for k in data if k not in order
    )

    lines = ["---"]
    for k in keys:
        v = data[k]
        if isinstance(v, list):
            if not v:
                lines.append(f"{k}: []")
            else:
                escaped = ", ".join(f'"{item}"' for item in v)
                lines.append(f"{k}: [{escaped}]")
        else:
            # Quote any string that contains special YAML chars
            s = str(v)
            if any(ch in s for ch in ":#[]{}&*!|>'\""):
                s_escaped = s.replace('"', '\\"')
                lines.append(f'{k}: "{s_escaped}"')
            else:
                lines.append(f"{k}: {s}")
    lines.append("---")
    lines.append("")
    return "\n".join(lines)


def missing_required(fm: dict) -> list[str]:
    """List REQUIRED_FIELDS that are missing or empty in the frontmatter dict."""
    missing = []
    for field in REQUIRED_FIELDS:
        if field not in fm:
            missing.append(field)
            continue
        v = fm[field]
        if v is None or v == "" or v == []:
            missing.append(field)
    return missing


# ---------------------------------------------------------------------------
# Ollama client
# ---------------------------------------------------------------------------


def ollama_generate(prompt: str, model: str, system: str = "") -> str:
    """Call Ollama /api/generate with format=json and think=false.

    Returns the model's response string (should be valid JSON). Raises
    RuntimeError with a clear message if Ollama is down or the request fails.
    """
    payload = {
        "model": model,
        "prompt": prompt,
        "system": system,
        "stream": False,
        "think": False,  # disable thinking mode for Qwen 3.5
        "format": "json",  # force JSON output
        "options": {
            "temperature": 0,
            "num_predict": 512,
        },
    }
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(
        f"{OLLAMA_HOST}/api/generate",
        data=data,
        headers={"Content-Type": "application/json"},
    )
    try:
        with urllib.request.urlopen(req, timeout=180) as resp:
            body = resp.read().decode("utf-8")
    except urllib.error.URLError as e:
        raise RuntimeError(f"Ollama request failed: {e}") from e

    try:
        parsed = json.loads(body)
    except json.JSONDecodeError as e:
        raise RuntimeError(f"Ollama returned non-JSON envelope: {e}") from e

    return parsed.get("response", "")


def infer_frontmatter(content: str, model: str) -> dict:
    """Ask the local LLM for {description, tags, doc_type}. Returns a dict.

    Validates the returned JSON against our expected schema and normalizes
    values (clamps description length, lowercases tags, restricts doc_type
    to the allow-list). Raises ValueError on unrecoverable bad output.
    """
    snippet = content[:MAX_PROMPT_CHARS]
    if len(content) > MAX_PROMPT_CHARS:
        snippet += f"\n\n[... truncated; full file is {len(content)} chars ...]"

    user_prompt = (
        "Here is the markdown file content. Return the JSON object now:\n\n"
        f"{snippet}"
    )

    raw = ollama_generate(user_prompt, model=model, system=SYSTEM_PROMPT)
    raw = raw.strip()
    if not raw:
        raise ValueError("empty response from model")

    try:
        data = json.loads(raw)
    except json.JSONDecodeError as e:
        raise ValueError(f"model output was not valid JSON: {e}\n--- raw ---\n{raw[:400]}") from e

    if not isinstance(data, dict):
        raise ValueError(f"model output was not a JSON object: {type(data).__name__}")

    result: dict = {}

    desc = data.get("description", "")
    if isinstance(desc, str) and desc.strip():
        result["description"] = desc.strip()[:140]

    tags = data.get("tags", [])
    if isinstance(tags, list):
        cleaned = []
        for t in tags:
            if not isinstance(t, str):
                continue
            tag = t.strip().lower().replace(" ", "-")
            if tag and tag not in cleaned:
                cleaned.append(tag)
        if cleaned:
            result["tags"] = cleaned[:6]

    doc_type = data.get("doc_type", "")
    if isinstance(doc_type, str) and doc_type.strip().lower() in DOC_TYPES:
        result["doc_type"] = doc_type.strip().lower()

    if not result:
        raise ValueError(f"model output had no usable fields: {raw[:200]}")

    return result


# ---------------------------------------------------------------------------
# File processing
# ---------------------------------------------------------------------------


def should_skip(filepath: Path) -> str | None:
    """Return a reason string if the file should be skipped, None otherwise."""
    try:
        size = filepath.stat().st_size
    except OSError as e:
        return f"stat failed: {e}"
    if size < MIN_FILE_BYTES:
        return f"too small ({size}B)"
    if size > MAX_FILE_BYTES:
        return f"too large ({size}B > {MAX_FILE_BYTES}B)"
    return None


def process_file(
    filepath: Path,
    model: str,
    execute: bool,
    force: bool,
) -> str:
    """Process a single file. Returns a status string for the summary."""
    skip_reason = should_skip(filepath)
    if skip_reason:
        return f"SKIP ({skip_reason})"

    try:
        text = filepath.read_text(encoding="utf-8", errors="replace")
    except OSError as e:
        return f"ERROR read: {e}"

    existing_fm, body = parse_frontmatter(text)

    if not force:
        missing = missing_required(existing_fm)
        if not missing:
            return "SKIP (already tagged)"
    else:
        missing = list(REQUIRED_FIELDS)

    try:
        inferred = infer_frontmatter(text, model=model)
    except (RuntimeError, ValueError) as e:
        return f"ERROR infer: {e}"

    # Merge: existing fields win unless --force; inferred fills gaps
    merged = dict(existing_fm)
    for field in REQUIRED_FIELDS:
        if field in inferred and (force or field in missing):
            merged[field] = inferred[field]

    added_preview = {
        k: merged[k]
        for k in REQUIRED_FIELDS
        if k in merged and (k not in existing_fm or force)
    }

    if not added_preview:
        return "SKIP (model returned nothing usable)"

    if not execute:
        preview_parts = []
        if "description" in added_preview:
            preview_parts.append(f'desc="{added_preview["description"][:60]}..."')
        if "tags" in added_preview:
            preview_parts.append(f"tags={added_preview['tags']}")
        if "doc_type" in added_preview:
            preview_parts.append(f"doc_type={added_preview['doc_type']}")
        return "DRY-RUN " + " ".join(preview_parts)

    # Write back: frontmatter + body (preserves body verbatim)
    new_text = render_frontmatter(merged) + body
    try:
        filepath.write_text(new_text, encoding="utf-8")
    except OSError as e:
        return f"ERROR write: {e}"

    tagged_fields = sorted(added_preview.keys())
    return f"TAGGED ({', '.join(tagged_fields)})"


def main():
    parser = argparse.ArgumentParser(
        description="Infer YAML frontmatter for untagged .md files via local LLM",
    )
    parser.add_argument("path", help="File or directory to scan")
    parser.add_argument(
        "--execute",
        action="store_true",
        help="Actually write frontmatter (default: dry-run)",
    )
    parser.add_argument(
        "--model",
        default=DEFAULT_MODEL,
        help=f"Ollama model tag (default: {DEFAULT_MODEL})",
    )
    parser.add_argument(
        "--limit",
        type=int,
        default=0,
        help="Process at most N files (0 = unlimited)",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Re-tag files that already have complete frontmatter",
    )
    args = parser.parse_args()

    root = Path(args.path)
    if not root.exists():
        print(f"ERROR: path not found: {root}", file=sys.stderr)
        sys.exit(1)

    if root.is_file():
        files = [root] if root.suffix.lower() == ".md" else []
    else:
        files = sorted(root.rglob("*.md"))

    if not files:
        print(f"No .md files found under {root}")
        sys.exit(0)

    if args.limit > 0:
        files = files[: args.limit]

    print(f"Scanning {len(files)} .md files under {root}")
    print(f"Model: {args.model}")
    print(f"Mode: {'EXECUTE (writing back)' if args.execute else 'DRY-RUN'}")
    if args.force:
        print("Force: re-tagging files that already have frontmatter")
    print()

    stats = {"tagged": 0, "dry": 0, "skipped": 0, "errors": 0}
    for i, filepath in enumerate(files, 1):
        rel = filepath.relative_to(root) if filepath != root else filepath.name
        status = process_file(filepath, args.model, args.execute, args.force)
        print(f"[{i:4d}/{len(files)}] {rel}")
        print(f"           {status}")

        if status.startswith("TAGGED"):
            stats["tagged"] += 1
        elif status.startswith("DRY-RUN"):
            stats["dry"] += 1
        elif status.startswith("SKIP"):
            stats["skipped"] += 1
        else:
            stats["errors"] += 1

    print()
    print("--- Summary ---")
    print(f"  Tagged:  {stats['tagged']}")
    print(f"  Dry-run: {stats['dry']}")
    print(f"  Skipped: {stats['skipped']}")
    print(f"  Errors:  {stats['errors']}")

    if not args.execute and stats["dry"]:
        print("\nThis was a DRY RUN. Add --execute to write frontmatter back to files.")


if __name__ == "__main__":
    main()
