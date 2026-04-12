---
allowed-tools: Read, Glob, Bash(python:*), Bash(python3:*), Bash(curl:*)
description: Infer YAML frontmatter (description/tags/doc_type) for untagged .md files via local Qwen 27B
---

# Frontmatter Tagger

Scans a directory for markdown files that are missing frontmatter and uses the local Ollama instance (Qwen 3.5 27B on RTX 5090) to infer `description`, `tags`, and `doc_type`. Preserves any existing frontmatter fields — only fills what's missing.

## Arguments

`$ARGUMENTS` — one of:
- A directory or file path (e.g., `doc/arcanum/`, `memory/`, `AI_Studio/Reports/`)
- `<path> execute` — actually write frontmatter back (default is dry-run)
- `<path> limit N` — process at most N files (for testing large trees)
- `<path> force` — re-tag files that already have frontmatter
- No arguments — default to `doc/arcanum/` dry-run

## Preflight

1. **Check Ollama is running**: `curl -s http://localhost:11434/api/tags` — should list models. If it fails, tell the user to start Ollama and stop.
2. **Verify Qwen model is installed**: grep for `qwen3.5:27b-q4_K_M` in the response. If missing, tell the user `ollama pull qwen3.5:27b-q4_K_M`.

## Process

### Step 1: Dry-run first (always)

Run the tagger in dry-run mode against the target path:

```bash
python tools/frontmatter_tagger.py <path>
```

The script reads each untagged .md file, sends the content to Ollama, and prints what frontmatter it would add — without writing anything. Output shows:
- Files that will be tagged with inferred description/tags/doc_type
- Files already tagged (skipped)
- Files too small/large (skipped)
- Any errors (Ollama down, bad model output, etc.)

For large trees, pass `--limit 5` or `--limit 20` first to validate the output quality before running against hundreds of files.

### Step 2: Show summary and ask

Report to the user:
- N files would be tagged
- N already tagged (skipped)
- N errors (show the first 3 if any)
- Sample of inferred frontmatter for 3 files (description + tags + doc_type)

Ask: "Ready to write these back? Say `/frontmatter <path> execute` to apply."

### Step 3: Execute when confirmed

```bash
python tools/frontmatter_tagger.py <path> --execute
```

The script writes frontmatter back in place. It preserves the body verbatim and only prepends the YAML block.

## Field Semantics

- **description**: one sentence, ≤140 chars, summarizing the file's purpose
- **tags**: 3–6 lowercase keywords (dashes for spaces)
- **doc_type**: one of `note | spec | reference | report | log | checklist | todo | draft`

Any existing `people`, `date`, `filing_relevance`, or other frontmatter fields are preserved unchanged. Only the three required fields (description/tags/doc_type) are inferred.

## Safety

- **Dry-run by default** — never writes without `--execute`
- **Existing frontmatter wins** — a file with `description:` already set won't have it overwritten unless `--force` is passed
- **Body is byte-preserved** — only the frontmatter block is added/modified
- **Size guards** — skips files under 50B or over 200 KB
- **Truncates long files** — only sends the first 8000 chars to the model (enough for a summary; the full body still lands on disk unchanged)
- **Strict JSON output** — uses Ollama `format=json` + `think=false` so Qwen returns parseable structured data
- **Allow-list for doc_type** — model output outside the 8 valid values is dropped

## Typical Targets

| Path | Why |
|------|-----|
| `doc/arcanum/` | Wiki files — frontmatter drives arcanum_search relevance scoring |
| `memory/` | Auto-memory — tags make `arcanum_lookup` and topic filtering work |
| `AI_Studio/Reports/` | Research reports — doc_type=report helps filter |
| `Case_Reference/` | Legal files — but only for non-PII .md notes; DO NOT tag raw evidence |

## Don't Run Against

- `src/`, `sql/`, `tools/` — code/SQL directories have no narrative .md worth tagging
- `Case_Reference/raw evidence/` — evidence files should keep their literal names, not get overwritten with LLM-inferred metadata
- Any directory with sensitive PII — the file content is sent to localhost (never leaves the machine), but you still want deliberate control
