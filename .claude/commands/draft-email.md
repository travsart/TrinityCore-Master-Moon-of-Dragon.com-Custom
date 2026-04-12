---
allowed-tools: Read, Write, Bash(python3:*), Grep, Glob, Agent
description: Draft a plain-text email ready to paste into Gmail — no markdown, no formatting disasters
---

# Draft Email

## Arguments

`$ARGUMENTS` — who the email is to and what it's about (e.g., "Erasmo about OWF resume", "DAV about Angel's TDIU", "attorney re: AFBCMR deadline")

## Instructions

Draft an email and save it as a **plain text `.txt` file** ready to copy-paste directly into Gmail.

### Formatting Rules — NON-NEGOTIABLE

1. **NO MARKDOWN** — no `**bold**`, no `- bullets`, no `# headers`, no `[links](url)`, no `> quotes`
2. Use plain text formatting only:
   - Dashes (--) for list items instead of bullet points
   - ALL CAPS for emphasis instead of bold/italic
   - Blank lines for paragraph breaks
   - Plain URLs on their own line (no markdown link syntax)
   - Numbers (1. 2. 3.) for ordered lists
3. No tables — use aligned plain text if tabular data is needed
4. No special characters that Gmail might mangle
5. Keep line length under 80 characters where possible (Gmail wraps well but some clients don't)

### Research Phase

Before drafting, search for context about the recipient and topic:
1. `memory/` files — any prior contact or context
2. `Desktop/IMPORTANT DOCS/Finances/` — financial/VA/employment context
3. `Desktop/IMPORTANT DOCS/Case_Reference/` — legal case context
4. Previous emails to/from this person (search `11_EMAILS/` if available)

Pull any relevant: names, dates, reference numbers, phone numbers, prior commitments.

### Tone Guide

- Professional but human — not robotic, not overly casual
- Direct and clear — state what you need in the first 2 sentences
- Respectful of the recipient's time
- If following up, reference the specific prior contact/date
- Sign off as "Adam Taylor" (or "Capt Adam Taylor" for military/official correspondence)

### Output File

Save to the most relevant folder:
- Employment/OWF → `C:\Users\atayl\Desktop\Finances\03_OWF_E2I_Employment\`
- VA/Benefits → `C:\Users\atayl\Desktop\Finances\02_VA_Benefits_Income\`
- Legal/Case → `C:\Users\atayl\Desktop\Case_Reference\`
- General → `C:\Users\atayl\Desktop\`

Filename: `Email_to_[Name]_COPY_PASTE.txt`

### What to Tell the User

After saving:
1. Where the file was saved (full path)
2. The **subject line** to use in Gmail
3. The **recipient email** if known from research
4. Any **attachments** they should add
5. Anything to **double-check** before sending (dates, names, reference numbers)
6. Display the full email text so user can review before opening the file
