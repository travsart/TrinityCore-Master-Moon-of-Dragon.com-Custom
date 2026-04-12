---
description: "NotebookEditTool — Jupyter notebook editing, cell replace insert delete, cell_id lookup, nbformat versioning, .ipynb files, execution state cleanup"
title: "NotebookEditTool -- Arcanum Wiki"
tags: [tools, jupyter-notebook, cellid-lookup, nbformat-versioning, ipynb-files, execution-state]
---

# NotebookEditTool -- Arcanum Wiki

## Purpose

NotebookEditTool edits Jupyter notebook (.ipynb) cells with support for replacing cell content, inserting new cells, and deleting cells. It handles cell ID lookup, nbformat versioning, and execution state cleanup.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `notebook_path` | string | Yes | Absolute path to the .ipynb file |
| `cell_id` | string | No | Cell ID to edit (or insert after). Required for replace/delete. |
| `new_source` | string | Yes | New source content for the cell |
| `cell_type` | enum | No | `code` or `markdown`. Required for insert mode. |
| `edit_mode` | enum | No | `replace` (default), `insert`, or `delete` |

## Execution Flow

1. **Validation**: Verifies `.ipynb` extension, valid edit mode, cell_type required for insert, read-before-edit enforcement (like FileEditTool), staleness check, cell ID existence.
2. **Cell lookup**: First tries matching by actual cell ID, then falls back to numeric index parsing (`cell-N` format via `parseCellId()`).
3. **Edit application**: Modifies the notebook JSON in memory -- splice for insert/delete, direct assignment for replace.
4. **Code cell cleanup**: On replace, resets `execution_count` to null and clears `outputs` array.
5. **Write**: Serializes with 1-space indent (`IPYNB_INDENT = 1`), writes preserving original encoding and line endings.
6. **State update**: Updates `readFileState` with new content and mtime.

## Key Implementation Details

### Cell ID Generation
For nbformat >= 4.5, new cells get a random ID via `Math.random().toString(36).substring(2, 15)`.

### Non-Memoized JSON Parse
The tool explicitly uses `jsonParse()` instead of `safeParseJSON()` because the latter caches by content string. Since the notebook is mutated in place (cells.splice, source assignment), using the memoized version would poison the cache.

### Replace-to-Insert Fallback
If `edit_mode` is `replace` but `cellIndex` equals the cell count (one past the end), it automatically converts to `insert` mode with a default cell type of `code`.

## Limits and Constraints

| Limit | Value |
|-------|-------|
| maxResultSizeChars | 100,000 |
| `shouldDefer` | true |
| JSON indent | 1 space |

## Permission Requirements

Uses `checkWritePermissionForTool()`. Not concurrency-safe, not read-only.

## Interesting Findings

1. The tool tracks file history for undo support, just like FileEditTool and FileWriteTool.
2. The `searchHint` is "edit Jupyter notebook cells (.ipynb)", which helps ToolSearch discover this tool when the model encounters notebook files.
3. The readFileState update uses `offset: undefined` specifically to break FileReadTool's dedup match -- without this, a quick Read-Edit-Read sequence would incorrectly return the stale pre-edit content.
