---
allowed-tools: Bash(python3:*), Bash(python:*), Read, Grep, Glob, Agent
description: Cross-reference a factual claim against the case archive to find supporting or missing evidence
---

# Evidence Cross-Reference

Fact-check a specific claim or assertion against the case archive. Determines whether source documentation exists, is missing, or contradicts the claim.

## Arguments

The user provides a factual claim or assertion to verify. Examples:
- `/evidence-xref "Command contacted gaining unit about discipline in June 2024"`
- `/evidence-xref "CDE found no impairment on Aug 1, 2024"`
- `/evidence-xref "SrA documented the NJP alteration"`

## Verification Pipeline

### Step 1: Parse the Claim
Extract the key elements: WHO, WHAT, WHEN, WHERE. Identify the specific factual assertion that needs source documentation.

### Step 2: Check FINAL Documents
Search `C:/Users/atayl/Desktop/Claude_Browser_FINAL_*.md` (all 6) for this claim or similar language. Note which FINAL doc cites it and what source it points to.

Known FINAL docs:
- `FINAL_01` — Case Brief (1-page summary)
- `FINAL_02` — Theory of Case (legal brief)
- `FINAL_03` — Evidence Map and Exhibit Guide (THIS is the source-of-truth for what evidence exists where)
- `FINAL_04` — Complaint Trail
- `FINAL_05` — Status and Deadlines
- `FINAL_06` — Execution Playbook

### Step 3: Trace to Source
Follow the citation in FINAL 03's evidence map to the actual source file. Verify:
- Does the cited file exist at the stated path?
- Does it actually contain what FINAL 03 says it contains?
- Is the source contemporaneous (created at the time of events) or reconstructed (written later)?

### Step 4: Search Broadly
If FINAL 03 doesn't cite a source, search the full archive:
1. Grep across `C:/Users/atayl/Desktop/IMPORTANT DOCS/Case_Reference/` (text files)
2. Search .docx files via python-docx
3. Check email attachments in `11_EMAILS/Takeout_Extracted/`
4. Check screenshot filenames in `05_EVIDENCE_SCREENSHOTS/`

### Step 5: Classify the Evidence

| Rating | Meaning |
|--------|---------|
| **VERIFIED** | Source document found, contemporaneous, contains the claimed fact |
| **SUPPORTED** | Source found but indirect (e.g., referenced in another document, not primary source) |
| **CITED BUT UNVERIFIED** | FINAL docs cite a source, but that source file doesn't exist or doesn't contain the claim |
| **UNCITED** | Claim appears in FINAL docs but no source is cited |
| **MISSING** | No source found anywhere in the archive |
| **CONTRADICTED** | Source found that contradicts the claim |

## Output Format

```
CLAIM: [the assertion being checked]
RATING: [VERIFIED/SUPPORTED/CITED BUT UNVERIFIED/UNCITED/MISSING/CONTRADICTED]

SOURCE TRAIL:
- FINAL doc: [which FINAL doc mentions this, if any]
- Cited source: [what path FINAL 03 points to]
- Actual source: [what was actually found]
- Source type: [contemporaneous / reconstructed / third-party]

EVIDENCE:
[Quote the relevant passage from the source document]

GAPS:
[What additional evidence would strengthen this claim, if any]
```

## Important Notes

- Contemporaneous documents (written at the time of events) are Tier 1 evidence
- Reconstructed accounts (written after the fact) are weaker — note the distinction
- AI-generated FINAL documents are synthesis, NOT source evidence — always trace to the underlying source
- If a claim is in FINAL 02 (Theory of Case) but has no source in the archive, it needs one before being submitted to AFBCMR
