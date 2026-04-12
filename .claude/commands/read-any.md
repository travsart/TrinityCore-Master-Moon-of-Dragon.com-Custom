---
allowed-tools: Read, Bash(python3:*), Bash(python:*), Glob
description: Read any document type — PDF, DOCX, EML, MSG, TXT — unified extraction with automatic fallbacks
---

# Read Any Document

## Arguments

`$ARGUMENTS` — path to a file, partial filename, or glob pattern. If partial, search common directories.

## Instructions

Unified document reader. Detects file type and extracts text using the best available method with automatic fallbacks.

### Step 1: Resolve Path

If `$ARGUMENTS` is a full path, use it directly.

If it's a partial name or search term, Glob for it in these directories (in order):
1. `C:/Users/atayl/Desktop/`
2. `C:/Users/atayl/Desktop/IMPORTANT DOCS/Finances/` (recursive)
3. `C:/Users/atayl/Desktop/IMPORTANT DOCS/Case_Reference/` (recursive)
4. `C:/Users/atayl/Desktop/Excluded/` (recursive)
5. Current working directory (recursive)

If multiple matches, show them and ask which one.

### Step 2: Extract by Type

| Extension | Primary Method | Fallback |
|-----------|---------------|----------|
| `.pdf` | Read tool (native PDF support) | `python3 -c "import fitz,sys; [print(p.get_text()) for p in fitz.open(sys.argv[1])]" PATH` |
| `.docx` | python-docx extraction (see below) | — |
| `.doc` | Note: pre-2007 format, limited support | Try `python3 -c "import textract; print(textract.process(sys.argv[1]).decode())" PATH` |
| `.eml` | Python email stdlib (see below) | — |
| `.msg` | extract-msg (see below) | — |
| `.txt` `.md` `.csv` `.log` | Read tool (native) | — |
| `.rtf` | `python3 -c "from striprtf.striprtf import rtf_to_text; print(rtf_to_text(open(sys.argv[1]).read()))" PATH` | — |
| `.html` `.htm` | Read tool, then strip tags if needed | — |

**PDF extraction** (when Read tool fails):
```python
python3 -c "
import fitz, sys
doc = fitz.open(sys.argv[1])
for i, page in enumerate(doc):
    text = page.get_text()
    if text.strip():
        print(f'--- Page {i+1} ---')
        print(text)
" "$ARGUMENTS"
```

**DOCX extraction**:
```python
python3 -c "
from docx import Document
import sys
doc = Document(sys.argv[1])
for p in doc.paragraphs:
    if p.text.strip():
        print(p.text)
for table in doc.tables:
    for row in table.rows:
        print('\t'.join(cell.text for cell in row.cells))
" "$ARGUMENTS"
```

**EML extraction**:
```python
python3 -c "
import email, sys
from email import policy
with open(sys.argv[1], 'r', errors='replace') as f:
    msg = email.message_from_file(f, policy=policy.default)
print(f'From: {msg[\"from\"]}')
print(f'To: {msg[\"to\"]}')
print(f'Date: {msg[\"date\"]}')
print(f'Subject: {msg[\"subject\"]}')
print('---')
body = msg.get_body(preferencelist=('plain', 'html'))
if body:
    print(body.get_content())
" "$ARGUMENTS"
```

**MSG extraction**:
```python
python3 -c "
import extract_msg, sys
msg = extract_msg.Message(sys.argv[1])
print(f'From: {msg.sender}')
print(f'To: {msg.to}')
print(f'Date: {msg.date}')
print(f'Subject: {msg.subject}')
print('---')
print(msg.body)
msg.close()
" "$ARGUMENTS"
```

### Step 3: Output

1. Display the full extracted text
2. Note the file type and extraction method used
3. If tables were found (DOCX), mention it
4. If the file had attachments (EML/MSG), list their filenames
5. If extraction partially failed (some pages unreadable, encoding issues), note which parts are missing

### Optional: Save as TXT

If the user asked for a copy or if the original is hard to re-extract (large PDF, complex DOCX):
- Save as `.txt` with same base name to the same directory
- Tell the user where the text version was saved
