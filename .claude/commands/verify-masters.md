# Verify Masters — Automated QA for ___MASTER and ___INDEX files

## Arguments
- `$ARGUMENTS` — optional: path to a specific folder to check (default: all of IMPORTANT DOCS)

## Instructions

Run 4 automated verification passes on all `___MASTER_*.md` and `___INDEX_*.json` files in `C:\Users\atayl\Desktop\IMPORTANT DOCS\`. Report results as a single table.

### Pass A: Filename Existence
Every filename in backticks inside a `___MASTER_*.md` file must exist somewhere on disk under IMPORTANT DOCS. Skip: memory paths (`~/`, `C:/Users/atayl/VoxCore/`), meta files (`___MASTER`, `___INDEX`), known intentionally-missing files (rule files not yet created, template placeholders), glob patterns (`*`), slash-separated display names, and continuation shorthand (`+ pt2.docx`).

```python
import re, os
from pathlib import Path

ROOT = Path("C:/Users/atayl/Desktop/IMPORTANT DOCS")
KNOWN_MISSING = {
    'research_n.md', 'research_n_output.md', 'harmony-rules.md', 'anchor-rules.md',
    'forge-rules.md', 'compass-rules.md', 'prompt-context-injector.py', 'session_state.md',
}
all_files = {p.name.lower() for p in ROOT.rglob("*") if p.is_file()}
errors = []
checked = 0
for master in ROOT.rglob("___MASTER_*.md"):
    folder = master.parent
    rel = folder.relative_to(ROOT)
    text = master.read_text(encoding='utf-8', errors='replace')
    names = re.findall(r'`([^`\n]+\.\w{1,5})`', text)
    for fname in names:
        if any(x in fname for x in ['memory/', '.claude/', '.cache/', 'VoxCore/', 'AI_Studio/', 'http', '~/']):
            continue
        if fname.startswith('___') or fname in ('_WHY_ARCHIVED.md',):
            continue
        if fname.lower() in KNOWN_MISSING or '/' in fname or '*' in fname:
            continue
        checked += 1
        if fname.lower() not in all_files:
            base = re.sub(r'^\d+_', '', fname)
            if base.lower() not in all_files:
                errors.append(f"[{rel}] `{fname}`")
print(f"Pass A — Filenames: {checked} checked, {len(errors)} not found")
for e in errors: print(f"  {e}")
if not errors: print("  ALL CLEAN")
```

### Pass B: Path Resolution
Every full path (`C:/` or `~/` prefixed) in backticks inside a `___MASTER_*.md` must resolve on disk.

```python
import re, os
from pathlib import Path
ROOT = Path("C:/Users/atayl/Desktop/IMPORTANT DOCS")
home = os.path.expanduser("~")
errors = []
checked = 0
for master in ROOT.rglob("___MASTER_*.md"):
    rel = master.parent.relative_to(ROOT)
    text = master.read_text(encoding='utf-8', errors='replace')
    paths = re.findall(r'`((?:C:/|~/)[^`]+)`', text)
    for p in paths:
        checked += 1
        expanded = p.replace('~/', home + '/').replace('/', os.sep)
        if not os.path.exists(expanded) and not os.path.exists(expanded.rstrip(os.sep)):
            errors.append(f"[{rel}] `{p}`")
print(f"Pass B — Paths: {checked} checked, {len(errors)} broken")
for e in errors: print(f"  {e}")
if not errors: print("  ALL CLEAN")
```

### Pass C: File Count Accuracy
Compare file counts claimed in Document Control sections against `___INDEX_*.json` entry counts.

```python
import re, json
from pathlib import Path
ROOT = Path("C:/Users/atayl/Desktop/IMPORTANT DOCS")
errors = []
for master in ROOT.rglob("___MASTER_*.md"):
    folder = master.parent
    rel = folder.relative_to(ROOT)
    text = master.read_text(encoding='utf-8', errors='replace')
    counts = re.findall(r'(?:File count|file count)[^\d]*(\d+)', text, re.IGNORECASE)
    if not counts: continue
    claimed = int(counts[0])
    idx = list(folder.glob("___INDEX_*.json"))
    if not idx: continue
    data = json.loads(idx[0].read_text(encoding='utf-8'))
    actual = len(data.get('entries', []))
    if abs(claimed - actual) > 3:
        errors.append(f"[{rel}] claims {claimed}, INDEX has {actual}")
print(f"Pass C — Counts: {sum(1 for _ in ROOT.rglob('___MASTER_*.md'))} files checked")
print(f"  Mismatches (>3): {len(errors)}")
for e in errors: print(f"  {e}")
if not errors: print("  ALL CLEAN")
```

### Pass D: Duration Arithmetic
Check day-count claims against known anchor dates pinned to today.

```python
import re
from datetime import date
from pathlib import Path
ROOT = Path("C:/Users/atayl/Desktop/IMPORTANT DOCS")
TODAY = date.today()
ANCHORS = {
    'Aug 10': date(2026, 8, 10), 'Aug 14': date(2026, 8, 14),
    'Aug 25': date(2026, 8, 25), 'Jun 26': date(2026, 6, 26),
    'Jan 10': date(2026, 1, 10), 'Jan 30': date(2026, 1, 30),
    'Apr 15': date(2026, 4, 15),
}
errors = []
checked = 0
for master in ROOT.rglob("___MASTER_*.md"):
    rel = master.parent.relative_to(ROOT)
    text = master.read_text(encoding='utf-8', errors='replace')
    for match in re.finditer(r'(\d+)\s+days?\b', text):
        claimed = int(match.group(1))
        ctx = text[max(0,match.start()-80):match.end()+40]
        for label, target in ANCHORS.items():
            if label.lower() in ctx.lower():
                checked += 1
                actual = abs((target - TODAY).days)
                if abs(claimed - actual) > 5:
                    errors.append(f"[{rel}] '{label}' claims {claimed}d, actual={actual}d from {TODAY}")
                break
print(f"Pass D — Durations: {checked} anchor-matched claims")
print(f"  Errors (>5 day variance): {len(errors)}")
for e in errors: print(f"  {e}")
if not errors: print("  ALL CLEAN")
```

### Output

Run all 4 scripts and report a summary table:

| Pass | What | Checked | Errors |
|------|------|---------|--------|
| A | Filenames exist | N | N |
| B | Paths resolve | N | N |
| C | File counts match | N | N |
| D | Duration math | N | N |

If any errors, list them below the table. If all clean, report "All 4 passes clean — ___MASTER files verified."
