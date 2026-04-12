---
allowed-tools: Bash(python3:*), Bash(python:*), Read, Grep, Glob, Agent
description: Search the Case_Reference archive and FINAL documents by keyword or topic
---

# Case Search

Search across Adam's case archive for evidence, documents, or references matching a keyword or topic.

## Search Locations (priority order)

1. **FINAL docs**: `C:/Users/atayl/Desktop/Claude_Browser_FINAL_*.md` — the 6-document case suite
2. **Case_Reference**: `C:/Users/atayl/Desktop/IMPORTANT DOCS/Case_Reference/` — 341 files, 16 folders, 668 pages OCR'd
3. **Discrepancy Analysis**: `C:/Users/atayl/Desktop/IMPORTANT DOCS/Case_Reference/00_COMPLETE_DISCREPANCY_ANALYSIS.md`
4. **Extracted emails**: `C:/Users/atayl/Desktop/IMPORTANT DOCS/Case_Reference/11_EMAILS/`

## Folder Structure (Case_Reference)

```
00_  — Discrepancy analysis
01_  — Appeals and QAI (668-page OCR'd investigation binders)
02_  — IG and Whistleblower filings
03_  — MEB/IDES
04_  — Legal correspondence
05_  — Evidence screenshots
06_  — Mental health records
07_  — Military records
08_  — Congressional correspondence
09_  — Security clearance
10_  — Timeline and narratives
11_  — Emails (Takeout extracted)
12_  — Financial impact
13_  — Support letters
14_  — Command actions
15_  — NJP and discipline
```

## Search Strategy

1. **Text files first** (.md, .txt): Use Grep with the keyword across all search locations
2. **FINAL docs**: Always search these — they're the synthesized case documents
3. **OCR text**: `01_APPEALS_AND_QAI/QAI_REPORT_50_PAGES_OCR.txt` is the full investigation binder
4. **.docx files**: Use python-docx to search inside Word documents:

```python
python3 -c "
import glob, os
from docx import Document

keyword = 'SEARCH_TERM'
base = r'C:\Users\atayl\Desktop\Case_Reference'
for path in glob.glob(os.path.join(base, '**', '*.docx'), recursive=True):
    try:
        doc = Document(path)
        for i, p in enumerate(doc.paragraphs):
            if keyword.lower() in p.text.lower():
                print(f'{path}:{i+1}: {p.text[:200]}')
    except Exception:
        pass
"
```

5. **Screenshots**: Note .png files in `05_EVIDENCE_SCREENSHOTS/` — can't search text inside them, but filenames are descriptive. Use Glob to list matching filenames.

## Output Format

Present results grouped by source:
- **FINAL docs**: quote the relevant passage with document name
- **Case_Reference**: show folder, file, and matching excerpt
- **Count**: how many files matched, how many locations searched
- **Missing**: if the keyword appears in FINAL docs as a claim but has no source in Case_Reference, flag it

## Arguments

The user provides a keyword, name, topic, or phrase to search for. Examples:
- `/case-search Article 138` — find all references to Article 138
- `/case-search Wheeler` — find all mentions of MSgt Wheeler
- `/case-search IHPP coercion` — find evidence of IHPP coercion
- `/case-search smoking gun email` — find the Aug 8 email
