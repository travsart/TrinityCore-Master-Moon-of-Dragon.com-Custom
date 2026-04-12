"""
Case_Reference Frontmatter Tagger — CLI Entry Point.

Usage:
    python -m tools.case_tagger.cli --mode manifest    # dry-run: generate CSV manifest
    python -m tools.case_tagger.cli --mode apply        # write frontmatter + sidecars

Options:
    --root PATH          Archive root (default: ~/Desktop/VC Pipeline/Case_Reference/)
    --manifest-path PATH Manifest output (default: AI_Studio/Reports/case_tagger_manifest.csv)
    --vocab PATH         Vocabulary config (default: config/case_tagger/vocabulary.yaml)
    --verbose            Print per-file details
"""

import argparse
import csv
import json
import os
import re
import sys
import tempfile
from dataclasses import asdict
from pathlib import Path

# Allow running as `python -m tools.case_tagger.cli` from project root
PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(PROJECT_ROOT))

from tools.case_tagger.extractor import ExtractedMetadata, extract_metadata, load_vocabulary

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

ALLOWED_EXTENSIONS = {".md", ".txt", ".csv", ".json", ".xml", ".html", ".htm"}
MAX_FILE_SIZE = 1_048_576  # 1 MB

# Canonical YAML frontmatter key order
FRONTMATTER_KEYS = [
    "date", "doc_type", "people", "tags", "description",
    "filing_relevance", "evidence_strength",
]


# ---------------------------------------------------------------------------
# File scanner
# ---------------------------------------------------------------------------

def scan_files(root: Path) -> tuple[list[Path], list[tuple[Path, str]]]:
    """Walk archive, return (eligible_files, skipped_files_with_reason)."""
    eligible = []
    skipped = []

    for filepath in sorted(root.rglob("*")):
        if not filepath.is_file():
            continue

        ext = filepath.suffix.lower()

        # Skip binary/unsupported
        if ext not in ALLOWED_EXTENSIONS:
            skipped.append((filepath, f"unsupported extension: {ext}"))
            continue

        # Skip oversize
        try:
            size = filepath.stat().st_size
        except OSError:
            skipped.append((filepath, "stat failed"))
            continue

        if size > MAX_FILE_SIZE:
            skipped.append((filepath, f"oversize: {size:,} bytes"))
            continue

        # Skip sidecar files we created
        if filepath.name.endswith(".meta.json"):
            skipped.append((filepath, "sidecar file"))
            continue

        eligible.append(filepath)

    return eligible, skipped


# ---------------------------------------------------------------------------
# File decoder
# ---------------------------------------------------------------------------

def decode_file(filepath: Path) -> tuple[str, str]:
    """Read file with encoding detection. Returns (content, newline_style)."""
    raw = filepath.read_bytes()

    # Detect BOM
    if raw.startswith(b"\xef\xbb\xbf"):
        content = raw[3:].decode("utf-8", errors="replace")
    else:
        content = raw.decode("utf-8", errors="replace")

    # Detect dominant newline
    crlf = content.count("\r\n")
    lf = content.count("\n") - crlf
    newline = "\r\n" if crlf > lf else "\n"

    return content, newline


# ---------------------------------------------------------------------------
# Frontmatter handling
# ---------------------------------------------------------------------------

_FM_PATTERN = re.compile(r"^---\s*\n(.*?\n)---\s*\n", re.DOTALL)


def strip_existing_frontmatter(content: str) -> tuple[dict | None, str]:
    """If content starts with --- YAML frontmatter ---, strip and return (parsed, body).
    Returns (None, content) if no frontmatter found."""
    m = _FM_PATTERN.match(content)
    if not m:
        return None, content

    import yaml
    try:
        existing = yaml.safe_load(m.group(1))
    except Exception:
        existing = None

    body = content[m.end():]
    return existing, body


def render_frontmatter(meta: ExtractedMetadata) -> str:
    """Render metadata as YAML frontmatter block with canonical key order."""
    lines = ["---"]
    d = asdict(meta)

    for key in FRONTMATTER_KEYS:
        val = d.get(key)
        if isinstance(val, list):
            if val:
                # Inline YAML list
                items = ", ".join(f'"{v}"' for v in val)
                lines.append(f"{key}: [{items}]")
            else:
                lines.append(f"{key}: []")
        elif isinstance(val, str):
            if val:
                # Quote if contains special chars
                if any(c in val for c in ":{}[]#&*!|>',\""):
                    lines.append(f'{key}: "{val}"')
                else:
                    lines.append(f"{key}: {val}")
            else:
                lines.append(f'{key}: ""')
        else:
            lines.append(f"{key}: {val}")

    lines.append("---")
    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------------------
# Writers
# ---------------------------------------------------------------------------

def atomic_write(filepath: Path, content: str, newline: str = "\n"):
    """Write content to file atomically using temp file + rename."""
    # Normalize newlines to target
    content = content.replace("\r\n", "\n")
    if newline == "\r\n":
        content = content.replace("\n", "\r\n")

    fd, tmp = tempfile.mkstemp(dir=filepath.parent, suffix=".tmp")
    try:
        with os.fdopen(fd, "w", encoding="utf-8", newline="") as f:
            f.write(content)
        # On Windows, need to remove target first
        if filepath.exists():
            filepath.unlink()
        Path(tmp).rename(filepath)
    except Exception:
        try:
            Path(tmp).unlink()
        except OSError:
            pass
        raise


def write_markdown_frontmatter(filepath: Path, meta: ExtractedMetadata, content: str, newline: str):
    """Write/replace frontmatter in a markdown file."""
    _, body = strip_existing_frontmatter(content)
    fm = render_frontmatter(meta)

    # Ensure blank line between frontmatter and body
    if body and not body.startswith("\n"):
        fm += "\n"

    atomic_write(filepath, fm + body, newline)


def write_sidecar(filepath: Path, meta: ExtractedMetadata):
    """Write adjacent .meta.json sidecar."""
    sidecar_path = filepath.parent / f"{filepath.name}.meta.json"
    d = asdict(meta)
    # Canonical key order
    ordered = {k: d[k] for k in FRONTMATTER_KEYS if k in d}
    ordered["source_file"] = filepath.name

    atomic_write(sidecar_path, json.dumps(ordered, indent=2, ensure_ascii=False) + "\n")


# ---------------------------------------------------------------------------
# Manifest writer
# ---------------------------------------------------------------------------

def write_manifest(results: list[tuple[Path, ExtractedMetadata]], manifest_path: Path, root: Path):
    """Write CSV manifest for human review."""
    manifest_path.parent.mkdir(parents=True, exist_ok=True)

    with open(manifest_path, "w", encoding="utf-8", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([
            "file_path", "date", "doc_type", "tags", "people_count",
            "description", "filing_relevance", "evidence_strength",
        ])

        for filepath, meta in results:
            rel = filepath.relative_to(root)
            writer.writerow([
                rel.as_posix(),
                meta.date,
                meta.doc_type,
                "|".join(meta.tags),
                len(meta.people),
                meta.description,
                "|".join(meta.filing_relevance),
                meta.evidence_strength,
            ])


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="Case_Reference Frontmatter Tagger")
    parser.add_argument(
        "--mode", choices=["manifest", "apply"], default="manifest",
        help="manifest = dry-run CSV; apply = write frontmatter/sidecars",
    )
    parser.add_argument(
        "--root", type=Path,
        default=Path.home() / "Desktop" / "VC Pipeline" / "Case_Reference",
        help="Archive root directory",
    )
    parser.add_argument(
        "--manifest-path", type=Path,
        default=PROJECT_ROOT / "AI_Studio" / "Reports" / "case_tagger_manifest.csv",
        help="Manifest CSV output path",
    )
    parser.add_argument(
        "--vocab", type=Path,
        default=PROJECT_ROOT / "config" / "case_tagger" / "vocabulary.yaml",
        help="Vocabulary config path",
    )
    parser.add_argument("--verbose", action="store_true", help="Per-file output")
    args = parser.parse_args()

    # Validate
    if not args.root.exists():
        print(f"ERROR: Archive root not found: {args.root}", file=sys.stderr)
        sys.exit(1)
    if not args.vocab.exists():
        print(f"ERROR: Vocabulary config not found: {args.vocab}", file=sys.stderr)
        sys.exit(1)

    vocab = load_vocabulary(args.vocab)

    # Scan
    print(f"Scanning: {args.root}")
    eligible, skipped = scan_files(args.root)
    print(f"  Eligible: {len(eligible)} files")
    print(f"  Skipped:  {len(skipped)} files")

    # Extract metadata
    results: list[tuple[Path, ExtractedMetadata]] = []
    decode_errors = 0
    md_count = 0
    non_md_count = 0

    for filepath in eligible:
        try:
            content, newline = decode_file(filepath)
        except Exception as e:
            decode_errors += 1
            if args.verbose:
                print(f"  DECODE ERROR: {filepath.name}: {e}")
            continue

        meta = extract_metadata(filepath, content, vocab)
        results.append((filepath, meta))

        if filepath.suffix.lower() == ".md":
            md_count += 1
        else:
            non_md_count += 1

        if args.verbose:
            print(f"  {filepath.relative_to(args.root).as_posix()}: "
                  f"type={meta.doc_type}, tags={len(meta.tags)}, "
                  f"people={len(meta.people)}, strength={meta.evidence_strength}")

    # Mode: manifest (dry-run)
    if args.mode == "manifest":
        write_manifest(results, args.manifest_path, args.root)
        print(f"\nManifest written: {args.manifest_path}")
        print(f"  {len(results)} rows")

    # Mode: apply
    elif args.mode == "apply":
        md_written = 0
        sidecars_written = 0

        for filepath, meta in results:
            content, newline = decode_file(filepath)

            if filepath.suffix.lower() == ".md":
                write_markdown_frontmatter(filepath, meta, content, newline)
                md_written += 1
            else:
                write_sidecar(filepath, meta)
                sidecars_written += 1

        # Also write manifest as a record
        write_manifest(results, args.manifest_path, args.root)

        print(f"\nApply complete:")
        print(f"  Markdown files updated: {md_written}")
        print(f"  Sidecar files written:  {sidecars_written}")
        print(f"  Manifest written:       {args.manifest_path}")

    # Summary
    print(f"\n--- Run Summary ---")
    print(f"  Total eligible:    {len(eligible)}")
    print(f"  Processed:         {len(results)}")
    print(f"  Decode errors:     {decode_errors}")
    print(f"  Skipped files:     {len(skipped)}")

    # Breakdown of skips
    skip_reasons: dict[str, int] = {}
    for _, reason in skipped:
        bucket = reason.split(":")[0]
        skip_reasons[bucket] = skip_reasons.get(bucket, 0) + 1
    for reason, count in sorted(skip_reasons.items()):
        print(f"    {reason}: {count}")

    # Tag stats
    all_tags: dict[str, int] = {}
    for _, meta in results:
        for t in meta.tags:
            all_tags[t] = all_tags.get(t, 0) + 1
    if all_tags:
        print(f"\n  Tag distribution (top 15):")
        for tag, count in sorted(all_tags.items(), key=lambda x: -x[1])[:15]:
            print(f"    {tag}: {count}")

    # Doc type distribution
    dt_dist: dict[str, int] = {}
    for _, meta in results:
        dt_dist[meta.doc_type] = dt_dist.get(meta.doc_type, 0) + 1
    if dt_dist:
        print(f"\n  Doc type distribution:")
        for dt, count in sorted(dt_dist.items(), key=lambda x: -x[1]):
            print(f"    {dt}: {count}")


if __name__ == "__main__":
    main()
