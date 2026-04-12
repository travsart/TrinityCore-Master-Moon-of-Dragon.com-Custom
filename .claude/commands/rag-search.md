---
allowed-tools: Bash(python:*), Read
description: Natural-language semantic search over IMPORTANT DOCS — queries local Ollama RAG (nomic-embed-text + ChromaDB) built from extracted/transcribed/OCR'd case content
---

# /rag-search — Semantic Search Over IMPORTANT DOCS

Query the local RAG vector store built by `tools/rag_build.py`. Uses Ollama's `nomic-embed-text` (768-dim) embeddings and ChromaDB for vector search. Covers all extracted text (PDF/DOCX/EML), audio transcripts, and OCR'd screenshots.

## Invocation

`/rag-search <query>` — e.g. `/rag-search who signed the final revocation?`

If no arguments provided, ask the user what to search for.

## What's in the index

- **extracted**: all PDF/DOCX/EML files that went through `extract_cache.py` — legal records, correspondence, filings
- **transcribed**: the 4 .m4a audio transcripts (Cannon 6, Cannon 11, Lt Taylor pt1+pt2)
- **ocr**: OCR'd screenshots from 05_EVIDENCE_SCREENSHOTS (phone screenshots, document photos)
- **extracted_legacy**: the pre-cache sweep extraction results at AI_Studio/Reports/sme_importantdocs/extracted/

## Execution

Run the query tool with a reasonable default top-k:

```bash
python tools/rag_query.py "$ARGUMENTS" --top 8 --format full
```

If the user asks for a specific doc type, pass `--doc-type`:
- "find in emails" or "in transcripts" → `--doc-type transcribed` or matching
- "in screenshots" or "in images" → `--doc-type ocr`
- "in PDFs" or "in filings" → `--doc-type extracted`

If the user asks to narrow by path, pass `--rel-path-glob`:
- "in case reference" → `--rel-path-glob "case_reference"`
- "in finances" → `--rel-path-glob "finances"`

## Output

The tool returns top-K chunks with:
- doc_type, rel_path, chunk_idx
- similarity score (0-1, higher is better)
- the chunk text

After running, **synthesize** the results for the user:
1. Answer the question directly from the top 3 hits
2. Cite the source files (not just chunk IDs)
3. Flag if the top hits disagree or if there's a gap
4. Don't dump raw chunks at the user — summarize

## Example

User: `/rag-search who signed the final revocation`

```bash
python tools/rag_query.py "who signed the final revocation" --top 8 --format full
```

Then respond with a synthesized answer:
> **Col Jon D. Earles** signed the final revocation on Oct 15 2025, overriding the PRHP panel's Sep 30 2025 majority reinstatement recommendation.
>
> Sources:
> - `case_reference/01_APPEALS_AND_QAI/Final_Decision_Letter_20251015.pdf.txt` (score 0.87)
> - `case_reference/__MASTER DOCUMENTS/MASTER_03_COMPLAINT_TRAIL.md` (score 0.82)
> - `Cannon Air Force Base 6.m4a.txt` (score 0.71) — for context, the Aug 19 2024 meeting referred to the PA as "Col Tomek" which may be a transcription artifact or indicate the PA chain changed between Aug 2024 and Mar 2025

## Constraints

- Always run the tool — do NOT claim results without invocation
- Do NOT invent citations
- If Ollama or ChromaDB is not reachable, report the error and suggest:
  - Ollama not running: check `curl http://localhost:11434/api/tags`
  - Collection missing: run `python tools/rag_build.py` first
- For multi-part queries, run separate queries and synthesize
- Keep the response focused — 3-5 sentences + sources list, not a wall of text
