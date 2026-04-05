---
description: "GlobTool — file pattern matching, glob search, modification time sort, permission ignore patterns, fast read-only file discovery, **/*.ts patterns"
---

# GlobTool -- Arcanum Wiki

## Purpose

GlobTool finds files matching glob patterns (e.g., `**/*.ts`, `src/**/*.test.js`), returning results sorted by modification time (most recent first). It is a fast, read-only file discovery tool that respects permission ignore patterns.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `pattern` | string | Yes | Glob pattern to match files against |
| `path` | string | No | Directory to search in (defaults to CWD) |

## Execution Flow

1. **Validation**: If `path` is provided, verifies it exists and is a directory. Skips filesystem checks for UNC paths.
2. **Permission check**: Uses `checkReadPermissionForTool()`.
3. **Execution**: Calls the internal `glob()` utility with the pattern, resolved path, a limit (default 100 from `globLimits?.maxResults`), and the abort signal. Passes `toolPermissionContext` for filtering.
4. **Result**: Relativizes all paths under CWD to save tokens, returns filenames, count, duration, and truncation flag.

## Key Implementation Details

### Result Limit
The default limit is 100 files, configurable via `context.globLimits?.maxResults`. Results are truncated with a note: "(Results are truncated. Consider using a more specific path or pattern.)"

### Path Relativization
All returned file paths are converted to relative paths via `toRelativePath()`, saving tokens in the model's context window. This is the same approach used by GrepTool.

### Sorting
Files are sorted by modification time (most recently modified first) inside the `glob()` utility, giving the model the most relevant files first.

## Limits and Constraints

| Limit | Value |
|-------|-------|
| maxResultSizeChars | 100,000 |
| Default result limit | 100 files |
| Concurrency safe | Yes |
| Read only | Yes |

## Permission Requirements

Uses `checkReadPermissionForTool()`. Concurrency-safe and read-only -- safe for parallel execution.

## Error Handling

- **ENOENT**: Directory doesn't exist -- suggests alternatives via `suggestPathUnderCwd()`
- **Not a directory**: Returns validation error with errorCode 2
- **No results**: Returns "No files found" (not an error)

## Interesting Findings

1. GlobTool is one of the simplest tools in the codebase at under 200 lines including imports. Its entire `call()` method is 20 lines.

2. The `searchHint` is "find files by name pattern or wildcard", helping ToolSearch discover it when the model needs file discovery.

3. GlobTool reuses GrepTool's UI rendering (`UI.tsx:65`) for result display, showing filenames joined by newlines.
