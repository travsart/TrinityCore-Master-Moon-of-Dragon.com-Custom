---
description: "FileEditTool — exact string replacement, old_string new_string, replace_all, uniqueness enforcement, quote normalization, line ending preservation, staleness detection"
---

# FileEditTool -- Arcanum Wiki

## Purpose

FileEditTool performs exact string replacements within existing files. It supports single replacements (uniqueness enforced) and replace-all mode, with quote normalization, line ending preservation, and staleness detection.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `file_path` | string | Yes | Absolute path to the file to modify |
| `old_string` | string | Yes | The text to replace (must be unique unless `replace_all` is true) |
| `new_string` | string | Yes | The replacement text (must differ from `old_string`) |
| `replace_all` | boolean | No | Replace all occurrences (default false) |

## Execution Flow

### 1. Validation (extensive -- 20+ checks)
- Rejects edits to team memory files with secrets
- Rejects `old_string === new_string` (no-op edit)
- Checks deny rules for the path
- Skips filesystem operations for UNC paths (NTLM security)
- **File size cap**: 1 GiB (`MAX_EDIT_FILE_SIZE`) to prevent V8/Bun OOM (src/tools/FileEditTool/FileEditTool.ts:84)
- Reads the file into memory (handles UTF-16LE BOM detection)
- For nonexistent files: empty `old_string` means new file creation; non-empty means error
- For existing files with empty `old_string`: only valid if file content is empty
- Blocks `.ipynb` files (redirects to NotebookEditTool)
- Requires read-before-edit (checks `readFileState`)
- Staleness check with Windows mtime fallback (content comparison)
- **String matching**: Uses `findActualString()` which handles quote normalization (curly quotes vs straight quotes)
- **Uniqueness check**: If multiple matches and `replace_all` is false, returns error with match count
- **Settings validation**: Additional validation for Claude settings files via `validateInputForSettingsFileEdit()`

### 2. Execution
- Discovers and activates conditional skills for the path
- Notifies `diagnosticTracker.beforeFileEdited()`
- Creates parent directory, creates file history backup
- Re-reads file synchronously (critical section for staleness)
- Applies quote normalization to `new_string` via `preserveQuoteStyle()`
- Generates patch via `getPatchForEdit()`
- Writes to disk via `writeTextContent()` preserving original encoding and line endings
- Notifies LSP server and VS Code
- Updates `readFileState`

### 3. Result
Returns the file path, old/new strings, original file content, structured patch, user-modified flag, and replace-all flag.

## Key Implementation Details

### Quote Normalization
`findActualString()` (in `utils.ts`) handles the common case where the model sends straight quotes (`"`) but the file contains curly/smart quotes, or vice versa. If the literal `old_string` isn't found, it tries normalized versions. The `preserveQuoteStyle()` function then ensures `new_string` uses the same quote style as the matched text in the file.

### Uniqueness Enforcement
When `replace_all` is false, the tool counts occurrences of the matched string. If there are multiple matches, it returns errorCode 9 with the match count and instructs the model to either provide more context for uniqueness or set `replace_all` to true (src/tools/FileEditTool/FileEditTool.ts:332-343).

### Line Ending Preservation
Unlike FileWriteTool (which always writes LF), FileEditTool preserves the original file's line endings. The `readFileSyncWithMetadata()` function detects the line ending type, and `writeTextContent()` applies it to the output.

### Input Equivalence
FileEditTool implements `inputsEquivalent()` for deduplication of identical edits. This is used by the speculation system to avoid re-running identical edits.

### File Size Guard
```typescript
// src/tools/FileEditTool/FileEditTool.ts:84
const MAX_EDIT_FILE_SIZE = 1024 * 1024 * 1024 // 1 GiB (stat bytes)
```
V8/Bun's string length limit is ~2^30 characters. For ASCII files, 1 byte = 1 character, so 1 GiB is the safe byte-level guard. Multi-byte UTF-8 files can be larger on disk per character.

## Limits and Constraints

| Limit | Value |
|-------|-------|
| maxResultSizeChars | 100,000 |
| MAX_EDIT_FILE_SIZE | 1 GiB |
| `strict` mode | true |

## Permission Requirements

Uses `checkWritePermissionForTool()`. Not concurrency-safe, not read-only. The model-facing tool result reports whether the user modified the proposed changes before accepting (`userModified` flag).

## Error Handling

Error codes in validation:
- 0: Secret in team memory file
- 1: old_string equals new_string
- 2: Path denied by permission settings
- 3: File exists but empty old_string (can't create -- file already exists)
- 4: File not found (with similar file suggestions)
- 5: File is .ipynb (use NotebookEditTool)
- 6: File not read yet
- 7: File modified since read (staleness)
- 8: String not found in file
- 9: Multiple matches but replace_all is false
- 10: File too large (over 1 GiB)

## Interesting Findings

1. The empty `old_string` + nonexistent file path combination is intentionally allowed as a file creation mechanism. Edit with `old_string=""` and `new_string="content"` creates a new file. This is the only way FileEditTool creates files.

2. The `_simulatedSedEdit` in BashTool and FileEditTool's validation share the `FILE_UNEXPECTEDLY_MODIFIED_ERROR` constant, suggesting they were designed as complementary file modification mechanisms.

3. The `validateInputForSettingsFileEdit()` call applies additional validation when editing Claude's own settings files, likely preventing self-modification that could break the session.

4. FileEditTool logs the byte lengths of `old_string` and `new_string` to analytics (`tengu_edit_string_lengths`), which helps Anthropic understand typical edit sizes.
