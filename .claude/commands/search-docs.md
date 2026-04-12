---
allowed-tools: mcp__arcanum__arcanum_search, mcp__arcanum__arcanum_lookup, mcp__arcanum__arcanum_read, Grep, Glob, Read
description: Search IMPORTANT DOCS — full-text search across Case_Reference, Angel_VA, Finances, Career, Resume, Brand, Ethical AI via arcanum index
argument-hint: <search query> [--case | --va | --all]
---

# Search IMPORTANT DOCS

Search across all 7 IMPORTANT DOCS folders using the arcanum MCP index. Falls back to grep for non-indexed file types.

## Usage

`$ARGUMENTS` — parse as: `<query> [--scope]`

Scope flags:
- `--case` → `scope="case"` (Case_Reference only — 1,760+ files, legal case archive)
- `--va` → search both `scope="case"` filtered to VA/TDIU/migraine AND `scope="important_docs"` filtered to Angel_VA
- `--all` or no flag → `scope="all"` (everything: arcanum wiki + memory + reports + case + important_docs)
- `--docs` → `scope="important_docs"` (Angel_VA, Finances, Career, Resume Stuff, Brand, Ethical_AI_Research — excludes Case_Reference)

## Instructions

### Step 1 — Arcanum search (primary)
Run `arcanum_search` with the query and appropriate scope. Request `max_results=15`.

### Step 2 — Arcanum lookup (supplement for people/tags)
If the query looks like a person's name or a tag, also run `arcanum_lookup` with keyword matching. This is faster and catches frontmatter metadata that full-text might miss.

### Step 3 — Grep fallback (for .docx/.pdf references)
Arcanum only indexes `.md`, `.txt`, `.csv`, `.json`, `.xml`, `.html`. If the user is searching for content that might be in `.docx` or `.pdf` files:
- Grep `.docx` filenames: `Glob("C:/Users/atayl/Desktop/IMPORTANT DOCS/**/*.docx")` and filter by name
- For `.pdf` files: list matching filenames only (can't grep PDF content without extraction)
- Note to user: "N .docx/.pdf files matched by filename but content not searchable without extraction. Run `/read-doc <path>` to inspect."

### Step 4 — Format results
Present results as a ranked table:

```
## Search Results: "<query>" (scope: <scope>)

| # | Score | File | Snippet |
|---|-------|------|---------|
| 1 | 95 | case/09_SECURITY_CLEARANCE/Taylor_Response_Notes_DCSA_SIR.txt | ...matching context... |
| 2 | 82 | important_docs/Angel_VA/_ACTION_PLAN_complete.md | ...matching context... |
| ... | | | |

### People matches (from frontmatter)
- Dean Sides: found in 7 files (MASTER_03, MASTER_TIMELINE, ...)

### .docx/.pdf filename matches (not content-searched)
- Career/Adam_Taylor_Resume.docx
```

## Rules

- Always run arcanum_search first — it's indexed and fast
- If arcanum returns 0 results, fall back to Grep on the raw filesystem
- Never claim "not found" without trying both arcanum AND grep
- For people searches, always also try `arcanum_lookup` (checks frontmatter `people` field)
- Show the absolute file path so the user can navigate directly
