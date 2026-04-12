---
allowed-tools: Read, Grep, Glob, Bash(python3:*), Bash(python:*), Bash(ls:*)
description: VA claims dashboard — show Angel + Adam ratings, pending filings, evidence gaps, appeal deadlines, monthly $ impact
---

# VA Claims Dashboard

Build a compact status dashboard for VA disability claims covering both Angel (spouse) and Adam (user).

## Instructions

Read these sources IN PARALLEL, then synthesize:

### Required files
1. `C:/Users/atayl/.claude/projects/C--Users-atayl-VoxCore/memory/angel-va.md` — Angel's VA state
2. `C:/Users/atayl/.claude/projects/C--Users-atayl-VoxCore/memory/finances-overview.md` — cash flow context
3. `ls C:/Users/atayl/Desktop/IMPORTANT DOCS/Angel_VA/` — current file set
4. `ls C:/Users/atayl/Desktop/IMPORTANT DOCS/Finances/02_VA_Benefits_Income/` — joint VA income folder

### Optional (if they exist)
- `C:/Users/atayl/Desktop/IMPORTANT DOCS/Case_Reference/06_CLINICAL_RECORDS/` — may have Adam's C&P / DBQ
- `C:/Users/atayl/.claude/projects/C--Users-atayl-VoxCore/memory/case-status.md` — Adam's service-connected picture

## Compute deadlines in Python

```python
python3 -c "
from datetime import date
today = date.today()
# Known VA-specific deadlines — update as claims resolve
deadlines = [
    ('Angel tinnitus/radic appeal window', date(2026, 8, 14), 'HARD — 1yr from 8/13/2025 denial'),
    ('Angel migraine 50% supplemental filing TARGET', date(2026, 5, 1), 'SOFT — ASAP'),
    ('Angel TDIU 21-8940 filing TARGET', date(2026, 5, 1), 'SOFT — every month = -\$1,738'),
]
print(f'VA Deadlines as of {today.isoformat()}')
print('=' * 70)
for name, d, cat in sorted(deadlines, key=lambda x: x[1]):
    delta = (d - today).days
    flag = 'PAST' if delta < 0 else 'CRIT' if delta < 30 else 'URGE' if delta < 60 else 'OK'
    print(f'  {delta:>4}d | {flag:<4} | {name}')
    print(f'         {cat}')
"
```

## Output Format

```
## VA Claims Dashboard — [today]

### Angel — Current State
- Combined rating: XX% (paid at XX%), ~\$X,XXX/mo
- Monthly impact of pending TDIU: +\$1,738/mo (target 100% rate ~\$3,938/mo)
- Accredited VSO: DAV

### Angel — Pending Filings (DRAFTED but NOT FILED)
| Filing | Status | Blocker | $ Impact |
|---|---|---|---|
| TDIU (VA Form 21-8940) | Draft complete | Evidence gathering | +\$1,738/mo |
| Migraine 0%→50% supplemental | Draft complete | Headache diary, neuro notes | +~\$700/mo |

### Angel — Appeal Windows
- Tinnitus (denied 8/13/2025) — expires 8/14/2026, NEEDS NEXUS
- L/R cervical radiculopathy (denied 8/13/2025) — expires 8/14/2026

### Angel — Evidence Gaps (blockers for filing)
- [ ] Headache diary started
- [ ] MHS Genesis neurology notes pulled
- [ ] Neurologist nexus letter requested
- [ ] Roosevelt General records requested
- [ ] Buddy statement(s) collected

### Adam — VA State (if documented)
- (Adam's VA state from Case_Reference/06_CLINICAL_RECORDS or case-status.md)
- SSDI filed 3/3/2026 — confirmation doc status: [check Finances]

### Urgent Actions (highest ROI first)
1. File Angel TDIU — every month of delay costs \$1,738
2. Gather migraine evidence (diary + neurology notes) — unblocks 50% upgrade
3. Secure tinnitus nexus before 8/14/2026 appeal deadline

### Deadlines
[Python table output above]
```

## Rules

- Be BRUTALLY HONEST about money impact — don't soften "\$1,738/mo lost per month of delay"
- If angel-va.md is missing a fact, say "unknown" not a guess
- Do NOT fabricate rating percentages or claim numbers
- Flag any appeal window <60 days as URGENT
