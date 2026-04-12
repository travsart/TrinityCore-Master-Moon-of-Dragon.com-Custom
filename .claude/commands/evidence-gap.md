---
allowed-tools: Bash(python3:*), Bash(python:*), Read, Grep, Glob, Agent
description: Compare a specific filing's requirements against available evidence — find what's missing before submission
---

# Evidence Gap Analysis

For a specific filing or legal action, compare what's REQUIRED against what ACTUALLY EXISTS in the case archive.

## Arguments

The user specifies a filing type:
- `/evidence-gap DD7050` — DoD IG Whistleblower Reprisal complaint
- `/evidence-gap AFBCMR` — Air Force Board for Correction of Military Records
- `/evidence-gap NPDB` — National Practitioner Data Bank dispute
- `/evidence-gap HIPAA` — HHS OCR HIPAA complaint
- `/evidence-gap Art138` — Article 138 complaint
- `/evidence-gap OSC` — Office of Special Counsel supplement

## Filing Requirements

### DD Form 7050 (DoD IG Whistleblower Reprisal)
Required elements:
1. **Protected disclosure** — proof that a protected communication was made (IG complaint, congressional, safety report)
2. **Knowledge** — proof command knew about the protected disclosure
3. **Adverse personnel action** — specific actions taken (each must be documented)
4. **Causal connection** — timing, statements, or pattern showing the action was because of the disclosure
5. **Contributing factor** — the disclosure was a contributing factor (lower standard than "but for")
6. **Harm** — documented impact (career, medical, financial)

### DD Form 149 (AFBCMR)
Required elements:
1. **Specific error or injustice** — what entry in records is wrong
2. **Relief requested** — exactly what correction is sought
3. **Supporting evidence** — documents proving the error/injustice
4. **Exhaustion of remedies** — proof other channels were tried first
5. **Timeliness** — filed within 3 years or justification for delay

### NPDB Dispute
Required elements:
1. **Report identification** — the specific NPDB report being challenged
2. **Basis for dispute** — factual or procedural errors in the report
3. **Evidence the report is inaccurate** — supporting documentation
4. **Process violations** — if applicable, proof the reporting process was flawed

## Analysis Pipeline

### Step 1: Identify Requirements
Based on the filing type, list every required element.

### Step 2: Search Archive
For each required element, search:
- `C:/Users/atayl/Desktop/IMPORTANT DOCS/Case_Reference/` (all folders)
- `C:/Users/atayl/Desktop/Claude_Browser_FINAL_*.md`
- `C:/Users/atayl/Desktop/IMPORTANT DOCS/Case_Reference/11_EMAILS/Takeout_Extracted/`
- `C:/Users/atayl/Desktop/IMPORTANT DOCS/Case_Reference/11_EMAILS/CROSS_REFERENCE_REPORT.md`

### Step 3: Rate Evidence Availability

| Status | Meaning |
|--------|---------|
| HAVE — STRONG | Multiple contemporaneous sources, ready to cite |
| HAVE — MODERATE | Source exists but may need corroboration |
| HAVE — WEAK | Only reconstructed or indirect evidence |
| PARTIAL | Some evidence exists but key element is missing |
| MISSING | No evidence found in archive |
| NEED TO RETRIEVE | Evidence referenced but not yet obtained (e.g., from mil email, FOIA) |

### Step 4: Report

```
# Evidence Gap Analysis: [Filing Type]
Date: [today]
Overall readiness: [READY / GAPS EXIST / NOT READY]

## Required Elements

| # | Requirement | Status | Source(s) | Gap |
|---|------------|--------|-----------|-----|
| 1 | [requirement] | [status] | [file paths] | [what's missing] |

## Action Items to Close Gaps
1. [action] — closes gap for requirement #X
2. [action] — strengthens weak evidence for #Y

## Strongest Evidence (ready to cite)
- [file]: [why it's strong]

## Evidence That Needs Work
- [file]: [what's wrong with it]
```
