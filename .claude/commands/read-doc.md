---
allowed-tools: Bash(python3:*), Bash(python:*), Read, Write, Glob
description: Read .docx files — extract text and optionally save a copy to Desktop
---

# Read Document

Read a .docx file by extracting its text content via python-docx.

## Arguments

The user provides a file path or partial name. If a full path, use it directly. If a partial name or search term, use Glob to find matching .docx files under these directories (in order):
1. `C:/Users/atayl/Desktop/IMPORTANT DOCS/Case_Reference/`
2. `C:/Users/atayl/Desktop/`
3. `C:/Users/atayl/Documents/Personal/Legal/`
4. Current working directory

## Extraction

Run this Python snippet (adjust path):

```
python3 -c "
from docx import Document
import sys
doc = Document(sys.argv[1])
text = '\n'.join(p.text for p in doc.paragraphs)
for table in doc.tables:
    for row in table.rows:
        text += '\n' + '\t'.join(cell.text for cell in row.cells)
print(text)
" "/path/to/file.docx"
```

## Output

1. Display the full extracted text to the user
2. If the user asked for a copy on Desktop, save as `.txt` with the same base name to `C:/Users/atayl/Desktop/`
3. Note any tables found in the document

## Notes

- python-docx is already installed (`C:\Python314\Lib\site-packages`)
- This does NOT handle old `.doc` format (pre-2007 Word) — only `.docx`
- For PDFs, use the Read tool directly (it has native PDF support)
- If the document has images, note that they won't be extracted (text only)
