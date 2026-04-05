---
description: "GrepTool — ripgrep rg content search, regex patterns, output_mode content files_with_matches count, context lines, multiline, head_limit offset pagination, type filter"
---

# GrepTool -- Arcanum Wiki

## Purpose

GrepTool searches file contents using regular expressions, powered by ripgrep (`rg`). It supports three output modes (content, files_with_matches, count), context lines, multiline matching, file type filtering, and automatic result pagination with head_limit/offset.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `pattern` | string | Yes | Regex pattern to search for |
| `path` | string | No | File or directory to search (defaults to CWD) |
| `glob` | string | No | Glob filter for files (e.g., `*.js`, `*.{ts,tsx}`) |
| `output_mode` | enum | No | `content`, `files_with_matches` (default), or `count` |
| `-B` | number | No | Lines before each match (content mode only) |
| `-A` | number | No | Lines after each match (content mode only) |
| `-C` | number | No | Alias for `context` |
| `context` | number | No | Lines before and after each match |
| `-n` | boolean | No | Show line numbers (default true, content mode only) |
| `-i` | boolean | No | Case insensitive search |
| `type` | string | No | ripgrep file type filter (js, py, rust, go, etc.) |
| `head_limit` | number | No | Limit output entries (default 250; pass 0 for unlimited) |
| `offset` | number | No | Skip first N entries before applying head_limit |
| `multiline` | boolean | No | Enable multiline matching (rg -U --multiline-dotall) |

## Execution Flow

1. **Validation**: Verifies path exists if provided. UNC path security check.
2. **Permission check**: Uses `checkReadPermissionForTool()`.
3. **Ripgrep invocation**: Builds argument array with flags for hidden files, VCS directory exclusion, max column width (500), multiline, case sensitivity, output mode, context, type filter, glob patterns, ignore patterns from permission context, and plugin cache exclusions.
4. **Post-processing by mode**:
   - **content**: Apply head_limit/offset, convert absolute paths to relative
   - **count**: Apply head_limit/offset, parse `filename:count` lines, sum totals
   - **files_with_matches**: Stat all files for mtime, sort by modification time (most recent first), apply head_limit/offset, relativize paths

## Key Implementation Details

### Default Head Limit
The `DEFAULT_HEAD_LIMIT` is 250 (src/tools/GrepTool/GrepTool.ts:108). This prevents unbounded content-mode greps from filling up to the 20KB persist threshold. The model can pass `head_limit=0` explicitly for unlimited results. The `appliedLimit` field is only set in the output when truncation actually occurred, so the model knows there may be more results.

### VCS Directory Exclusion
These version control directories are always excluded: `.git`, `.svn`, `.hg`, `.bzr`, `.jj`, `.sl` (src/tools/GrepTool/GrepTool.ts:96-102).

### Column Width Cap
`--max-columns 500` prevents base64-encoded or minified content from cluttering results.

### Glob Pattern Parsing
Glob patterns containing braces (e.g., `*.{ts,tsx}`) are kept intact; others are split on spaces and commas (src/tools/GrepTool/GrepTool.ts:393-409).

### Leading-Dash Pattern Safety
If the search pattern starts with a dash, ripgrep would interpret it as a flag. GrepTool handles this by using `-e` to explicitly mark it as a pattern (src/tools/GrepTool/GrepTool.ts:380-384):
```typescript
if (pattern.startsWith('-')) {
  args.push('-e', pattern)
} else {
  args.push(pattern)
}
```

### File Sorting (files_with_matches mode)
Uses `Promise.allSettled` to stat all files -- a single ENOENT (file deleted between ripgrep's scan and the stat) won't reject the whole batch. Failed stats sort as mtime 0. In test mode, files sort by name for determinism.

## Limits and Constraints

| Limit | Value |
|-------|-------|
| maxResultSizeChars | 20,000 |
| Default head_limit | 250 entries |
| Max column width | 500 characters |
| Concurrency safe | Yes |
| Read only | Yes |

## Permission Requirements

Uses `checkReadPermissionForTool()`. Concurrency-safe and read-only. Applies ignore patterns from `toolPermissionContext` and excludes orphaned plugin version directories.

## Error Handling

- **ENOENT on path**: Suggests alternatives via `suggestPathUnderCwd()`
- **RipgrepTimeoutError**: Propagates so Claude knows the search didn't complete (rather than thinking there were no matches)
- **No matches**: Returns "No files found" or "No matches found" depending on mode

## Interesting Findings

1. The 20,000 char persistence threshold is the lowest of any tool (vs 30K for Bash, 100K for most others), reflecting that grep results are typically less critical to preserve fully.

2. GrepTool's `userFacingName()` returns "Search" (not "Grep"), making it more user-friendly in the UI.

3. The `applyHeadLimit` function uses the explicit-0 escape hatch pattern: `head_limit=0` means unlimited, while `undefined` falls back to the 250 default. This prevents accidental unlimited queries.

4. WSL performance is noted in the code: "WSL has severe performance penalty for file reads (3-5x slower on WSL2)." The timeout is handled by ripgrep itself, not AbortController.
