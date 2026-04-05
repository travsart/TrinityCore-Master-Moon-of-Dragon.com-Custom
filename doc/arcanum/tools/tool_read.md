---
description: "FileReadTool — file reading, images PNG JPG PDF ipynb, token limits, offset limit pagination, 2000 line default, image compression, deduplication, pages parameter"
---

# FileReadTool -- Arcanum Wiki

## Purpose

FileReadTool reads files from the local filesystem with support for text files, images (PNG/JPG/GIF/WebP), PDFs, and Jupyter notebooks (.ipynb). It applies token limits, deduplication, and image compression to keep output within context budgets.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `file_path` | string | Yes | Absolute path to the file to read |
| `offset` | number | No | Line number to start reading from (1-based, default 1) |
| `limit` | number | No | Number of lines to read |
| `pages` | string | No | Page range for PDFs (e.g., "1-5", "3", "10-20"). Max 20 pages per request. |

## Execution Flow

### 1. Validation
- Validates PDF `pages` parameter format and range (max `PDF_MAX_PAGES_PER_READ` pages)
- Expands path and checks deny rules (no filesystem I/O)
- Blocks UNC paths (`\\` or `//`) to prevent NTLM credential leaks
- Rejects binary extensions (except PDFs, images, SVGs)
- Blocks dangerous device files: `/dev/zero`, `/dev/random`, `/dev/stdin`, `/dev/tty`, `/dev/console`, and `/proc/*/fd/0-2` (src/tools/FileReadTool/FileReadTool.ts:98-128)

### 2. Dedup Check
Before reading, checks if this exact file+range was already read and the file hasn't changed (mtime comparison). If so, returns a `file_unchanged` stub instead of re-sending content. This saves ~2.64% of fleet `cache_creation` tokens. Only applies to entries created by Read (offset is set); Edit/Write entries (offset=undefined) are excluded to avoid pointing the model at pre-edit content. Controlled by killswitch `tengu_read_dedup_killswitch` (src/tools/FileReadTool/FileReadTool.ts:536-573).

### 3. Type-Specific Reading

**Notebooks (.ipynb)**: Read and parse JSON, validate token count, return cell array.

**Images** (PNG/JPG/GIF/WebP): Single read into buffer, standard resize via `sharp`, then token budget check. If over budget, aggressive compression (quality reduction + dimension cap). Fallback: 400x400 JPEG at quality 20 (src/tools/FileReadTool/FileReadTool.ts:1166-1170).

**PDFs**: Two paths:
- With `pages` parameter: extract specific pages via `poppler-utils`, convert to JPEG images
- Without `pages`: if over `PDF_AT_MENTION_INLINE_THRESHOLD` pages, throw error requiring `pages` parameter. Otherwise read full PDF as base64 document block.

**Text files**: Read via `readFileInRange()` with byte cap (`maxSizeBytes`, default 256 KB for total file size) and token cap (`maxTokens`, default 25,000). Content gets line numbers via `addLineNumbers()`.

### 4. Post-Read Processing
- Updates `readFileState` cache with content, mtime, offset, limit
- Triggers skill discovery for the file path
- Activates conditional skills matching the path
- Notifies file read listeners (registered callbacks)
- Appends `CYBER_RISK_MITIGATION_REMINDER` for malware analysis (skipped for `claude-opus-4-6`)

## Key Implementation Details

### File Read Limits (src/tools/FileReadTool/limits.ts)

Two caps apply to text reads:

| Limit | Default | Check Method | On Overflow |
|-------|---------|--------------|-------------|
| `maxSizeBytes` | 256 KB (`MAX_OUTPUT_SIZE`) | Total file size via stat | Throws pre-read |
| `maxTokens` | 25,000 | Actual output token count via API | Throws post-read |

Precedence for `maxTokens`: env var (`CLAUDE_CODE_FILE_READ_MAX_OUTPUT_TOKENS`) > GrowthBook > hardcoded default. Limits are memoized at first call to avoid mid-session changes.

### macOS Screenshot Path Resolution
macOS uses either regular space or thin space (U+202F) before AM/PM in screenshot filenames depending on the OS version. If the file isn't found, the tool tries swapping the space character before giving up (src/tools/FileReadTool/FileReadTool.ts:147-159).

### Memory File Freshness
For auto-memory files (CLAUDE.md, etc.), the tool captures mtime via a WeakMap side-channel and prefixes the content with a freshness note. The WeakMap auto-GCs when the data object is unreachable.

## Limits and Constraints

| Limit | Value |
|-------|-------|
| maxResultSizeChars | Infinity (never persisted to disk) |
| Default maxTokens | 25,000 tokens |
| Default maxSizeBytes | 256 KB (total file size) |
| PDF max pages per read | 20 (`PDF_MAX_PAGES_PER_READ`) |
| Image fallback compression | 400x400 JPEG, quality 20 |
| Blocked device paths | 12 paths (/dev/zero, /dev/random, etc.) |
| Token estimate threshold | maxTokens/4 (below this, skip API token count) |

## Permission Requirements

Uses `checkReadPermissionForTool()` from the filesystem permission module. Read-only tool -- `isReadOnly()` always returns true, `isConcurrencySafe()` always returns true. Safe for parallel execution.

## Error Handling

- **ENOENT**: Suggests similar filenames via `findSimilarFile()` and `suggestPathUnderCwd()`
- **Binary files**: Blocked with extension-based check (except PDFs and images)
- **Token overflow**: `MaxFileReadTokenExceededError` with token count and limit
- **Empty files**: Returns `<system-reminder>Warning: the file exists but the contents are empty.</system-reminder>`
- **Offset past EOF**: Returns a warning with the actual total line count

## Interesting Findings

1. `maxResultSizeChars: Infinity` is unique to FileReadTool. The comment explains: persisting Read output to a file the model would then Read creates a circular dependency. Read already self-bounds via maxTokens/maxSizeBytes.

2. The dedup feature was validated via BQ proxy showing ~18% of Read calls are same-file collisions, with an internal soak test showing 1,734 dedup hits in 2 hours and no error regression.

3. The `CYBER_RISK_MITIGATION_REMINDER` is a `<system-reminder>` appended to every text file read (except for Opus 4.6), instructing Claude to analyze but not improve malware code.

4. Session file detection (`detectSessionFileType`) specifically identifies reads of `~/.claude/session-memory/*.md` and `~/.claude/projects/*/*.jsonl` for analytics, tracking how often the model reads its own memory and transcripts.

5. The image token budget check uses a rough estimate: `base64.length * 0.125`. Only if this exceeds maxTokens does it attempt aggressive compression.
