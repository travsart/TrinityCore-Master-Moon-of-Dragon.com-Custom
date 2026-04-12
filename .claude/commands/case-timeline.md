---
allowed-tools: Bash(python3:*), Bash(python:*), Read, Grep, Glob, Agent, Write, Edit
description: Generate or update the master case timeline from all evidence sources
---

# Case Timeline

Generate a comprehensive chronological timeline from all evidence sources, or update the existing master timeline with new evidence.

## Arguments

- `/case-timeline` — full regeneration from all sources
- `/case-timeline update` — read existing timeline, search for events not yet included
- `/case-timeline 2024-07` — timeline for a specific month
- `/case-timeline NJP` — timeline filtered to a specific topic

## Source Files (read in parallel)

1. `C:/Users/atayl/Desktop/IMPORTANT DOCS/Case_Reference/10_TIMELINE_AND_NARRATIVES/MASTER_TIMELINE.md` — existing master timeline
2. `C:/Users/atayl/Desktop/IMPORTANT DOCS/Case_Reference/00_COMPLETE_DISCREPANCY_ANALYSIS.md` — 60 discrepancies with dates
3. `C:/Users/atayl/Desktop/IMPORTANT DOCS/Case_Reference/11_EMAILS/CROSS_REFERENCE_REPORT.md` — email-derived timeline events
4. `C:/Users/atayl/Desktop/IMPORTANT DOCS/Case_Reference/11_EMAILS/Takeout_Extracted/EXTRACTION_REPORT.md` — email dates
5. `C:/Users/atayl/Desktop/IMPORTANT DOCS/Case_Reference/11_EMAILS/Takeout_Extracted/MASTER_EMAIL_INDEX.csv` — all email metadata
6. `C:/Users/atayl/Desktop/IMPORTANT DOCS/Case_Reference/SESSION_FINDINGS_*.md` — session findings
7. `C:/Users/atayl/Desktop/Claude_Browser_FINAL_*.md` — FINAL case documents (especially 02 Theory, 03 Evidence Map)

## Timeline Entry Format

Each entry must have:
- **Date** (YYYY-MM-DD, or YYYY-MM if day unknown)
- **Event** — what happened (factual, not argumentative)
- **Source** — file path or email reference proving this event
- **Category** — one of: MST, IG, CWI, QAI, NJP, CDE, CLINICAL_PRIVILEGES, PCS, INPATIENT, SECURITY_CLEARANCE, CONGRESSIONAL, MEB, LEGAL, SAPR, OTHER
- **Significance** — CRITICAL / HIGH / MODERATE / LOW

## Output Format

```
# Master Case Timeline — Capt Adam J. Taylor
Updated: [today's date]
Events: [count]
Sources: [count of unique source files]

## [YYYY-MM — Month Name Year]

| Date | Event | Category | Sig | Source |
|------|-------|----------|-----|--------|
| YYYY-MM-DD | [event] | [cat] | [sig] | [source file] |
```

## Rules

- Every entry MUST cite a source. No unsourced timeline entries.
- When updating, preserve existing entries — only add new ones or correct errors.
- If a date is uncertain, use `~` prefix (e.g., `~2024-08-19`).
- Flag any timeline conflicts (different sources give different dates for the same event).
- Separate FACTS from ALLEGATIONS with clear language ("Command states..." vs "Taylor alleges...").
