---
allowed-tools: Bash(python3:*), Bash(python:*), Read
description: Show countdown days to all case deadlines with urgency flags
---

# Case Deadlines

Calculate and display days remaining for all active case deadlines.

## Instructions

### Step 1: Read current deadlines from master file
Read `C:/Users/atayl/Desktop/IMPORTANT DOCS/Case_Reference/13_ANALYSIS_AND_BRIEFS/MASTER_ACTION_ITEMS.md` to check for any new deadlines not in the hardcoded list.

### Step 2: Calculate countdowns

```python
python3 -c "
from datetime import date
today = date.today()
deadlines = [
    ('TAP Initial Counseling (1400, MFRC Bldg 1433)', date(2026, 3, 31), 'HARD', 'Appointment scheduled'),
    ('POD follow-up (if no response)', date(2026, 3, 22), 'SOFT', '7 days after Mar 15 intake'),
    ('CARE Event JBSA', date(2026, 4, 20), 'INVITED', 'Includes Caregiver program for Angel'),
    ('AFBCMR DD-149 filing', date(2026, 5, 15), 'TARGET', '10 USC 1552'),
    ('Retention request past ADSCD', date(2026, 5, 15), 'TARGET', 'DoDI 1332.18 s7.7'),
    ('ADSCD (separation)', date(2026, 8, 10), 'HARD', 'Active duty ends'),
    ('Section 1983 SOL (Rio Vista)', date(2026, 9, 23), 'HARD', '2yr from Oct 23 2024 transfer'),
    ('SEAD 9 trigger (1yr clearance susp)', date(2026, 11, 26), 'SOFT', 'Appellate review if still AD'),
    ('DoD IG 1034 filing window', date(2025, 8, 14), 'HARD', '1yr from Aug 14 2024 - MAY BE EXPIRED, argue continuing violations'),
    ('HHS OCR HIPAA (180 days)', date(2025, 4, 23), 'SOFT', 'From Oct 2024 Wheeler access - request good cause extension'),
]
print(f'CASE DEADLINES as of {today.isoformat()}')
print('=' * 80)
past = []
active = []
for name, d, cat, note in deadlines:
    delta = (d - today).days
    if delta < 0:
        past.append((name, d, cat, note, delta))
    else:
        active.append((name, d, cat, note, delta))

if past:
    print()
    print('PAST DUE:')
    for name, d, cat, note, delta in sorted(past, key=lambda x: x[4]):
        print(f'  {abs(delta):>4}d ago | {cat:<7} | {name}')
        print(f'           | Note: {note}')

print()
urgent = sum(1 for _, _, _, _, d in active if d < 30)
print(f'ACTIVE ({len(active)} deadlines, {urgent} under 30 days):')
for name, d, cat, note, delta in sorted(active, key=lambda x: x[4]):
    if delta < 14:
        flag = '>>> CRITICAL'
    elif delta < 30:
        flag = '>>  URGENT'
    elif delta < 60:
        flag = '>   SOON'
    else:
        flag = '    OK'
    print(f'  {delta:>4}d | {flag} | {cat:<7} | {name} ({d.isoformat()})')
    if delta < 60:
        print(f'         |          |         | Note: {note}')
"
```

### Step 3: Output
Display the Python output directly. No additional commentary needed.
