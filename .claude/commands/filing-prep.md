---
allowed-tools: Bash(python3:*), Bash(python:*), Read, Grep, Glob, Agent, Write, Edit
description: Prepare a legal filing — pull requirements, search evidence, draft narrative with citations, flag gaps
---

# Filing Prep

Prepare a draft legal filing by pulling form requirements, searching the case archive for relevant evidence, and drafting the narrative sections with citations.

## Arguments

The user specifies:
- `/filing-prep DD7050` — DoD IG Whistleblower Reprisal complaint
- `/filing-prep AFBCMR` — AFBCMR application (DD Form 149)
- `/filing-prep NPDB dispute` — NPDB report challenge
- `/filing-prep HIPAA complaint` — HHS OCR complaint
- `/filing-prep OSC supplement` — OSC complaint update
- `/filing-prep congressional [recipient]` — Congressional inquiry letter

## Pipeline

### Phase 1: Requirements
Read the specific filing requirements for the requested form type. Key regulatory references:
- **DD 7050**: 10 U.S.C. 1034, DoDD 7050.06, DAFI 90-301. Two-part burden-shifting test: (1) complainant shows protected disclosure was "a contributing factor" in unfavorable action, (2) agency must prove by clear and convincing evidence it would have taken same action absent the disclosure
- **DD 149**: 10 U.S.C. 1552, DAFI 36-2603. 3-year filing window from discovery of error. Board can waive in interest of justice
- **NPDB**: 45 C.F.R. Part 60, specifically 45 CFR 60.21 (formal dispute process). Filing triggers 60-day dialogue with reporting entity + opens HHS Secretary review pathway. Subject Statement (narrative only) is DIFFERENT from Formal Dispute (triggers investigation)
- **HIPAA**: 45 C.F.R. Parts 160, 164. File at ocrportal.hhs.gov. 180-day filing window from violation
- **Privilege Due Process**: DHA-PM 6025.13 Vol 3, Enclosure 3 — key paragraphs:
  - Para 2.b.(1)(b): "Clinical privileging actions are NOT a disciplinary tool"
  - Para 2.p.(1)(g): No PCS during clinical due process (conflicts with DoDI 6495.02 ET)
  - Para 2.p.(6)(a): CDE required before adverse action
  - Para 2.p.(14)(b): NPDB Revision-to-Action (never filed by PA)
  - Para 2.p.(15)(d): PA can submit secret MFR to DHA (member denied copy)
  - Para 2.p.(15)(f): No provision for provider to appear at DHA panel
  - Para 13(b): PA shall NOT rely on facts outside hearing record
- **IDES/MEB**: DoDM 1332.18 Vol 1, Section 4.3 — PEBLO assignment within 3 calendar days
- **SAPR**: DoDI 6495.02 — expedited transfer rights for sexual assault victims
- **VLC**: 10 USC 1044e — right to Victims' Legal Counsel

Use the **regulation-lookup agent** to search `_DHAPM_SIGNED_full_text.txt` on Desktop for exact text when needed.

### Phase 2: Evidence Gathering
Fan out agents to search the case archive for evidence supporting each required element:

1. **case-researcher agent**: Search `Case_Reference/` and MASTER docs (`99_MASTER_SYNTHESIS_OUTPUT/`) for relevant evidence
2. **regulation-lookup agent**: Search regulatory text files for exact paragraph citations needed in the filing
3. **Grep the email extracts**: `11_EMAILS/Takeout_Extracted/Legal/txt/` and `UNOPENED_EVIDENCE/txt/`
4. **Check the cross-reference report**: `11_EMAILS/CROSS_REFERENCE_REPORT.md`
5. **Read the discrepancy analysis**: `00_COMPLETE_DISCREPANCY_ANALYSIS.md`
6. **Read the evidence validation report**: `99_MASTER_SYNTHESIS_OUTPUT/00_EVIDENCE_VALIDATION_REPORT.md`

### Phase 3: Draft Narrative
Write the narrative section of the filing:
- Use formal legal writing style (not argumentative, not casual)
- Cite specific evidence for every factual claim: `(See Exhibit [X], [description])`
- Organize chronologically within each legal element
- Keep paragraphs focused — one claim per paragraph with its supporting evidence
- Use regulatory language that matches the filing requirements

### Phase 4: Exhibit List
Generate a proposed exhibit list:
```
Exhibit A: [document name] — [what it proves]
Exhibit B: [document name] — [what it proves]
```
Map each exhibit to its file path in the case archive.

### Phase 5: Gap Analysis
Flag anything that's:
- Claimed in the narrative but has no exhibit
- Referenced in the archive but not included in the draft
- Needed but not yet obtained (e.g., FOIA responses, records from mil email)

## Output

Write the draft to: `C:/Users/atayl/Desktop/IMPORTANT DOCS/Case_Reference/13_ANALYSIS_AND_BRIEFS/DRAFT_[filing_type]_[date].md`

```
# DRAFT — [Filing Type]
Prepared: [date]
Status: DRAFT — requires attorney review

## I. Summary of Complaint / Request for Relief
[1-2 paragraph summary]

## II. Statement of Facts
[Chronological narrative with exhibit citations]

## III. Legal Basis
[Statutory and regulatory framework]

## IV. Evidence of [specific legal element]
[For each required element, narrative + citations]

## V. Relief Requested
[Specific, measurable relief]

## VI. Proposed Exhibit List
| Exhibit | Document | Source Path | Proves |
|---------|----------|-------------|--------|

## VII. Gaps / Items Needed
- [ ] [missing item]
```

## Rules

- ALWAYS mark output as DRAFT requiring attorney review
- Do not fabricate or embellish facts — only cite what's in the archive
- Use the evidence rating from `/evidence-gap` — don't cite WEAK evidence as if it's STRONG
- For reconstructed accounts, note they are "Complainant's account" not established fact
- Flag any potential CUI/PII that should be handled carefully in the filing
