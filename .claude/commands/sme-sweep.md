---
allowed-tools: Read, Grep, Glob, Write, Edit, Bash(python:*), Bash(ls:*), Bash(find:*), Bash(mkdir:*), Bash(wc:*), Bash(du:*), Agent, TaskCreate, TaskUpdate, mcp__arcanum__*
description: Full 6-pass SME sweep of a document folder — manifests, cached extraction, OCR, audio, parallel content mapping, contradictions vs memory, memory edits, gap analysis
---

# /sme-sweep — Full SME Sweep of a Document Folder

Orchestrates a 6-pass SME sweep that turns a document folder into actionable knowledge: manifests → cached extraction → OCR → audio transcription → parallel content mapping → contradiction detection → memory updates → gap analysis.

## Invocation

`/sme-sweep <folder-path>` — e.g. `/sme-sweep "C:/Users/atayl/Desktop/IMPORTANT DOCS"`

If no folder is provided, ask the user which folder to sweep.

## Output Location

`AI_Studio/Reports/sme_<slug>/` where `<slug>` is derived from the folder name (replace spaces with underscore, lowercase, strip punctuation).

Existing outputs are NOT deleted — passes are incremental and re-runnable.

## Phase 0: Setup + Arcanum Check

Before running any extraction, **check what's already indexed in arcanum**:

```
mcp__arcanum__arcanum_index(folder="")
mcp__arcanum__arcanum_index(folder="<relevant top folder>")
```

If the target folder is already in arcanum (e.g. `case/`, `important_docs/`, `memory/`), report the file count and **use arcanum_search / arcanum_lookup for text queries** instead of walking the tree yourself. Arcanum is typically a curated text/markdown subset — you'll still need extraction for raw PDFs/DOCX/EML, but arcanum is the right first stop for "find the file that mentions X".

Create output dir:
```bash
mkdir -p "AI_Studio/Reports/sme_<slug>/manifests"
```

## Phase 1: Manifests (foundation)

For each top-level subfolder of the target (or the whole folder if shallow), build a JSON manifest:

```bash
python tools/folder_index.py "<folder>" -o "AI_Studio/Reports/sme_<slug>/manifests/<sub>.json" --preview 200
```

Report file counts and total size per subfolder. This tells you how big each parallel agent's task will be.

## Phase 2: Cached Extraction (PDF/DOCX/EML/MSG)

Use the persistent cache so repeat sweeps skip unchanged files:

```bash
python tools/extract_cache.py "<folder>"
```

Output lands at `.cache/extracted/<slug>_<hash>/files/` (mirror tree). First run extracts everything, subsequent runs only handle changed files.

Report: X new, Y changed, Z cached, W deleted.

## Phase 3: OCR on Images (only if images present)

Check for PNG/JPG/JPEG files:
```bash
find "<folder>" -type f \( -iname "*.png" -o -iname "*.jpg" -o -iname "*.jpeg" \) | wc -l
```

If > 0, run OCR:
```bash
python tools/ocr_images.py "<folder>" --workers 8
```

Output: `.cache/ocr/<slug>_<hash>/files/` (sidecar .txt per image). Screenshots become grep-able.

## Phase 4: Audio Transcription (only if audio present)

Check for audio:
```bash
find "<folder>" -type f \( -iname "*.m4a" -o -iname "*.mp3" -o -iname "*.wav" \) | wc -l
```

If > 0, run transcription:
```bash
python tools/audio_transcribe.py "<folder>" --model base --language en
```

**Note**: on CPU (torch 2.11.0+cpu) this is ~1x realtime. A 30-minute file takes ~30 minutes. Consider running in background via `run_in_background=true` or flagging for the user to run overnight. Document which files are pending transcription if you skip.

## Phase 5: Context Pack

Before launching parallel agents, build a single "context pack" file so agents don't each re-read the same memory files. Write it to `AI_Studio/Reports/sme_<slug>/context_pack.md`:

1. Relevant memory files (read each, quote key facts)
2. Pass 4 contradiction register from prior sweep if one exists
3. Brief summary of what was found in arcanum from Phase 0

Each Pass 6 agent prompt can then include "read context_pack.md for baseline knowledge" instead of listing 5 memory files.

## Phase 6: Parallel Content Mapping (Pass 2 equivalent)

For each top-level subfolder, launch a parallel `general-purpose` agent. If a single subfolder has > 300 files, split it across multiple agents by sub-cluster.

Each agent gets:
- Path to its manifest JSON
- Path to its extracted text dir
- Path to the context pack
- Path to the existing memory file for its topic (if any)
- Strict output path: `AI_Studio/Reports/sme_<slug>/pass6_<subfolder>.md`
- Max 500 lines per report
- "Write incrementally, do not accumulate" instruction
- Topic-specific report structure (purpose, inventory, key docs, contradictions vs memory, gaps)

Launch all agents in parallel in a single message.

## Phase 7: Contradiction Register (Pass 4 equivalent)

Single agent reads all Pass 6 reports + all relevant memory files and produces a contradiction register at `AI_Studio/Reports/sme_<slug>/contradictions.md`. Categorize by severity:
- CRITICAL (affects deadlines or active filings)
- HIGH (factual error)
- MEDIUM (drift)
- LOW (minor)

Include an explicit "Pass 8 Edit Plan" section listing exact edits to apply per memory file.

## Phase 8: Apply Memory Edits

Single agent executes the Pass 8 Edit Plan. Reads each memory file, applies edits, writes a summary to `AI_Studio/Reports/sme_<slug>/edits_applied.md` showing before → after for each change.

**Constraint**: Never invent edits the Pass 7 plan doesn't specify. If ambiguous, go back to source and resolve.

## Phase 9: Gap Analysis

Single agent produces ranked action plan at `AI_Studio/Reports/sme_<slug>/gaps_and_actions.md`:
- Deadline calendar (today + 150 days)
- Evidence gaps per active filing
- Blocked filings with blockers
- Folder hygiene (moves/renames/deletes from Pass 6)
- Top 20 ranked action items (P0/P1/P2)
- Cross-folder dependencies

## Phase 10: README

Write `AI_Studio/Reports/sme_<slug>/README.md` linking all pass outputs and summarizing scope, counts, key findings, and whether additional passes (person dossiers, timeline, PII scan) are warranted.

## Phases You Can SKIP

If the user says "quick sweep" or "just Pass 6-9":
- Skip arcanum check (Phase 0) if already verified
- Skip OCR/audio if no relevant files
- Skip Phase 5 context pack if no memory files touch this topic
- Always do Phase 6 (content mapping) and Phase 7 (contradictions) as minimum

## What NOT to Do

- Do NOT read PNG/JPG files into the conversation context — they go through ocr_images.py
- Do NOT re-extract files that are already in `.cache/extracted/` — the cache handles incremental
- Do NOT accumulate all content then write the report — write incrementally
- Do NOT launch agents sequentially when they can run in parallel
- Do NOT invent new memory claims — every edit needs a source file citation
- Do NOT treat arcanum as complete — it's a curated subset, not a mirror

## Tool Dependencies

| Tool | Installed | Notes |
|------|-----------|-------|
| tools/folder_index.py | yes | builds JSON manifest |
| tools/extract_cache.py | yes | incremental PDF/DOCX/EML extraction |
| tools/ocr_images.py | yes | Tesseract at `C:/Program Files/Tesseract-OCR/` |
| tools/audio_transcribe.py | yes | Whisper, CPU-only until torch nightly with CUDA 12.8 is installed |
| tools/bulk_extract.py | yes | legacy, still works for one-shot extracts |
| arcanum MCP | yes | text/MD search, use FIRST before walking trees |

## Example Session

```
User: /sme-sweep "C:/Users/atayl/Desktop/IMPORTANT DOCS/Angel_VA"

Claude: [Phase 0] Arcanum check: important_docs/Angel_VA/ has 14 indexed .md files
        [Phase 1] Manifest: 65 files, 27 MB, 48 extractable
        [Phase 2] Cached extract: 48 cached (instant)
        [Phase 3] OCR: no images
        [Phase 4] Audio: no audio
        [Phase 5] Context pack: memory/angel-va.md merged
        [Phase 6] Launching 1 agent (small folder) on Angel_VA content
        ...
        [Phase 10] Sweep complete. 3 contradictions found, 2 applied.
```
