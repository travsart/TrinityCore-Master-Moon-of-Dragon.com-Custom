---
allowed-tools: Read, Grep, Glob, Bash(python3:*), Bash(python:*)
description: Case dashboard — show ADSCD countdown, deadlines, red flags, action items, and key contacts
---

# Case Status Dashboard

Display a compact operational dashboard for Adam's military legal case.

## Instructions

Read these files IN PARALLEL, then synthesize a compact dashboard:

### Required Files

1. `C:/Users/atayl/Desktop/IMPORTANT DOCS/Case_Reference/13_ANALYSIS_AND_BRIEFS/MASTER_ACTION_ITEMS.md`
2. `C:/Users/atayl/Desktop/IMPORTANT DOCS/Case_Reference/03_MEB_IDES/MASTER_MEB_TRACKER.md`
3. `C:/Users/atayl/Desktop/IMPORTANT DOCS/Case_Reference/10_TIMELINE_AND_NARRATIVES/MASTER_LEGAL_TRACKER.md`
4. `C:/Users/atayl/Desktop/IMPORTANT DOCS/Case_Reference/10_TIMELINE_AND_NARRATIVES/MASTER_TIMELINE.md`

### Dashboard Sections

#### 1. Countdown Clocks
Calculate days remaining from TODAY to each deadline:

| Deadline | Date | Purpose |
|----------|------|---------|
| TAP Initial Counseling | 31 Mar 2026 | MFRC Bldg 1433, 1400 |
| AFBCMR filing target | May 2026 | DD Form 149 |
| Retention request target | May 2026 | DoDI 1332.18 s7.7 |
| Section 1983 SOL | ~Sep 2026 | Rio Vista (2 yr from Oct 2024) |
| ADSCD | 10 Aug 2026 | HARD separation deadline |

Use Python to calculate:
```python
python3 -c "
from datetime import date
today = date.today()
deadlines = [
    ('TAP', date(2026, 3, 31)),
    ('AFBCMR target', date(2026, 5, 15)),
    ('Retention request', date(2026, 5, 15)),
    ('Section 1983 SOL', date(2026, 9, 23)),
    ('ADSCD', date(2026, 8, 10)),
]
for name, d in deadlines:
    delta = (d - today).days
    flag = ' !!!' if delta < 30 else ' !' if delta < 60 else ''
    print(f'{name}: {delta} days ({d.isoformat()}){flag}')
"
```

#### 2. Red Flags (from MEB Tracker)
Extract all items marked as NOT DONE, RED FLAG, or UNKNOWN from the MEB tracker. These are process violations or missing steps.

#### 3. Action Items by Urgency
From MASTER_ACTION_ITEMS.md, count items in each tier:
- THIS WEEK: list each with checkbox status
- THIS MONTH: count only
- BEFORE SEPARATION: count only

#### 4. Legal Lanes
From MASTER_LEGAL_TRACKER.md, show each active representative and their status:
- Tolin (civilian) — scope, last contact
- ADC Elliot Ko — status
- VLC — VACANT (flag this)
- ODC — engagement status
- VSO — engaged or not
- POD — intake status

#### 5. Recent Timeline
Last 5 entries from MASTER_TIMELINE.md.

### Output Format

```
## Case Status — [today's date]

### Deadlines
| Deadline | Days Left | Date | Alert |
|----------|-----------|------|-------|
| ...      | ...       | ...  | ...   |

### Red Flags
- [flag]: [detail]

### Action Items: X this week / Y this month / Z before sep
THIS WEEK:
- [ ] item 1
- [ ] item 2
...

### Legal Lanes
| Lane | Owner | Status | Last Contact |
|------|-------|--------|-------------|
| ...  | ...   | ...    | ...         |

### Recent Events
| Date | Event |
|------|-------|
| ...  | ...   |
```
