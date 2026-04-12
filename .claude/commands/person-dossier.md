---
allowed-tools: Bash(python3:*), Bash(python:*), Read, Grep, Glob, Agent
description: Build a dossier on a specific person — every mention across all case files, emails, and documents
---

# Person Dossier

Generate a comprehensive dossier on a named individual across the entire case archive.

## Arguments

The user provides a person's name (or partial name). Examples:
- `/person-dossier Wheeler` — everything about MSgt Wheeler
- `/person-dossier Earles` — Col Earles' actions and decisions
- `/person-dossier Wareham` — attorney correspondence history

## Search Locations

Search ALL of these in parallel:

1. `C:/Users/atayl/Desktop/IMPORTANT DOCS/Case_Reference/` — all 16 folders recursively (.md, .txt files)
2. `C:/Users/atayl/Desktop/Claude_Browser_FINAL_*.md` — 6 FINAL case documents
3. `C:/Users/atayl/Desktop/IMPORTANT DOCS/Case_Reference/11_EMAILS/Takeout_Extracted/` — email txt extracts
4. `C:/Users/atayl/Desktop/IMPORTANT DOCS/Case_Reference/11_EMAILS/CROSS_REFERENCE_REPORT.md` — cross-reference report
5. `.docx` files via python-docx extraction

## Search Strategy

1. **Grep all text files** for the person's name (case-insensitive)
2. **Search .docx files** using python-docx:
```python
python3 -c "
import glob, os
from docx import Document
name = 'SEARCH_NAME'
base = r'C:\Users\atayl\Desktop\Case_Reference'
for path in glob.glob(os.path.join(base, '**', '*.docx'), recursive=True):
    try:
        doc = Document(path)
        for i, p in enumerate(doc.paragraphs):
            if name.lower() in p.text.lower():
                print(f'{path}:{i+1}: {p.text[:200]}')
    except Exception:
        pass
"
```
3. **Search email metadata** in `MASTER_EMAIL_INDEX.csv` for email addresses containing the name
4. **Check filenames** — Glob for files containing the name
5. **Search the QAI OCR text** — `01_APPEALS_AND_QAI/QAI_REPORT_50_PAGES_OCR.txt`

## Cache Results

After generating the dossier, **always save it** to a persistent file:
```
C:\Users\atayl\.claude\projects\C--Users-atayl-VoxCore\memory\dossier_[lastname_lowercase].md
```

**Cache rules:**
- If a cached dossier already exists for this person, read it FIRST
- Check file modification dates in Case_Reference vs cache date
- If new files exist since cache date, do an incremental search (only new/modified files) and merge results
- If no new files, return the cached version with a note: "Cached from [date]. No new files detected. Run with `--force` to re-scan."
- Always update the cache after a fresh or incremental scan
- Include a `Last scanned: [date]` header in the cached file

This prevents re-searching 40+ files every time the same person is queried.

## Output Format

```
# Dossier: [Full Name, Rank/Title]

## Identity
- **Name**: [full name as it appears in records]
- **Rank/Title**: [military rank or civilian title]
- **Organization**: [unit, office, or firm]
- **Email(s)**: [all email addresses found]
- **Role in case**: [1-sentence summary]

## Timeline of Involvement
| Date | Action/Event | Source |
|------|-------------|--------|
| ...  | ...         | [file path] |

## Key Actions (adverse/favorable/neutral)
- [action]: [detail with source citation]

## Statements/Admissions
- "[direct quote]" — [source file:line]

## Emails Involving This Person
| Date | Direction | Subject | File |
|------|-----------|---------|------|
| ...  | From/To/CC | ...   | ... |

## Documents Referencing This Person
- [file path]: [context of reference]

## Assessment
- **Role**: [respondent / witness / legal rep / support / neutral]
- **Evidence strength**: [how well-documented are this person's actions]
- **Gaps**: [what we DON'T have documentation for]
```
