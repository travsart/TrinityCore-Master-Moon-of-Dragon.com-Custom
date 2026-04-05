---
description: "BashTool — shell command execution, timeout, background tasks, sandboxing, git detection, progress streaming, run_in_background, dangerouslyDisableSandbox, ~750 lines core"
---

# BashTool -- Arcanum Wiki

## Purpose

BashTool executes shell commands in a child process, with support for timeouts, background execution, sandboxing, progress streaming, and automatic detection of git operations. It is the most complex tool in Claude Code, spanning ~750 lines of core logic plus 15 supporting modules.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `command` | string | Yes | The shell command to execute |
| `timeout` | number | No | Timeout in milliseconds (max from `getMaxTimeoutMs()`) |
| `description` | string | No | Human-readable description shown in UI |
| `run_in_background` | boolean | No | Run as a background task (hidden when `CLAUDE_CODE_DISABLE_BACKGROUND_TASKS` is set) |
| `dangerouslyDisableSandbox` | boolean | No | Override sandbox mode for this command |
| `_simulatedSedEdit` | object | No | INTERNAL ONLY: pre-computed sed edit result, hidden from model schema |

The `_simulatedSedEdit` field is deliberately omitted from the model-facing schema (src/tools/BashTool/BashTool.tsx:250-259). It is injected by the sed edit preview permission dialog after user approval. Exposing it would let the model bypass permission checks by pairing an innocuous command with an arbitrary file write.

## Execution Flow

### 1. Validation (`validateInput`)
- Detects `sleep N` patterns (N >= 2 seconds) and blocks them with a suggestion to use `run_in_background` or Monitor tool (src/tools/BashTool/BashTool.tsx:524-537)
- Returns immediately for all other commands

### 2. Permission Check (`checkPermissions`)
- Delegates to `bashToolHasPermission()` in `bashPermissions.ts`
- For compound commands (`ls && git push`), parses the AST to extract subcommands
- Each subcommand is checked against permission rules independently

### 3. Execution (`call`)
- If `_simulatedSedEdit` is present, applies the pre-computed edit directly via `applySedEdit()` instead of running the command
- Creates an `EndTruncatingAccumulator` for stdout
- Launches the command via `runShellCommand()` async generator
- Streams progress updates to `onProgress` callback every tick
- After completion, calls `trackGitOperations()` to detect commits, pushes, and PR creation
- Interprets exit codes via `interpretCommandResult()` for semantic meaning
- Resets CWD if the command navigated outside the project (main thread only)
- Annotates output with sandbox violations if any

### 4. Large Output Handling
- If the output file exceeds the internal accumulator size, the full output is persisted to the tool-results directory
- Files over 64 MB (`MAX_PERSISTED_SIZE`) are truncated after copying (src/tools/BashTool/BashTool.tsx:732)
- The model receives a preview with a path to the full output

## Key Implementation Details

### Background Execution
When `run_in_background: true`, the command is spawned as a `LocalShellTask`. The tool returns immediately with a `backgroundTaskId` and output file path. The model can later check results via `TaskOutput` tool. Auto-backgrounding kicks in after `ASSISTANT_BLOCKING_BUDGET_MS` (15 seconds) in assistant mode (src/tools/BashTool/BashTool.tsx:57).

### Progress Streaming
Progress is shown after `PROGRESS_THRESHOLD_MS` (2 seconds). Each progress event includes: current output chunk, full accumulated output, elapsed time, total lines, total bytes, task ID, and timeout value.

### Command Classification
BashTool classifies commands for UI purposes:

- **Search commands**: `find`, `grep`, `rg`, `ag`, `ack`, `locate`, `which`, `whereis` -- collapsed in non-verbose UI
- **Read commands**: `cat`, `head`, `tail`, `less`, `more`, `wc`, `stat`, `file`, `strings`, `jq`, `awk`, `cut`, `sort`, `uniq`, `tr`
- **List commands**: `ls`, `tree`, `du` -- shown as "Listed N directories"
- **Silent commands**: `mv`, `cp`, `rm`, `mkdir`, `chmod`, `touch`, `cd` -- show "Done" instead of "(No output)"
- **Semantic-neutral**: `echo`, `printf`, `true`, `false`, `:` -- skipped when classifying pipelines

For pipelines (`cat file | jq`), ALL non-neutral parts must be search/read/list for the whole command to be considered collapsible (src/tools/BashTool/BashTool.tsx:95-172).

### Sed Edit Interception
When the command is a `sed -i` edit, BashTool's permission dialog shows a file diff preview. If approved, the edit is applied via `applySedEdit()` which directly writes the new content instead of running sed. This ensures what the user previewed is exactly what gets written (src/tools/BashTool/BashTool.tsx:360-418).

## Limits and Constraints

| Limit | Value | Source |
|-------|-------|--------|
| maxResultSizeChars | 30,000 | Tool result persistence threshold |
| Progress threshold | 2,000 ms | `PROGRESS_THRESHOLD_MS` |
| Auto-background budget | 15,000 ms | `ASSISTANT_BLOCKING_BUDGET_MS` |
| Max persisted output | 64 MB | `MAX_PERSISTED_SIZE` |
| Sleep block threshold | 2 seconds | `detectBlockedSleepPattern` |
| Auto-background timeout | 120,000 ms | `getAutoBackgroundMs()` (when enabled) |

## Permission Requirements

BashTool uses the most sophisticated permission system of any tool. The `preparePermissionMatcher` parses the command AST to extract individual subcommands. Compound commands like `FOO=bar git push` are parsed to extract `git push` for matching against `Bash(git *)` rules. If AST parsing fails (too complex, unavailable), the matcher falls through to `() => true` (fail-safe: run the hook).

## Error Handling

- **ShellError**: Thrown when the command exits with a non-zero code that `interpretCommandResult` classifies as an error. Contains stdout, stderr, exit code, and interrupted flag.
- **Sandbox violations**: Annotated into the output via `SandboxManager.annotateStderrWithSandboxFailures()`
- **Git index.lock**: Detected specifically for analytics tracking
- **CWD reset**: If the shell changes CWD outside the project, it's reset back with a warning message

## Interesting Findings

1. The `_simulatedSedEdit` field is a security-critical hidden parameter that bypasses the normal command execution path entirely. Its omission from the model schema is intentional -- exposing it would allow arbitrary file writes.

2. BashTool's `isConcurrencySafe` is dynamically determined: it's only true when `isReadOnly()` returns true, which requires parsing the command to determine it doesn't modify state.

3. The `detectBlockedSleepPattern` function is surprisingly nuanced: it allows sub-2-second sleeps (rate limiting), float durations, and sleep inside pipelines/subshells. Only standalone `sleep N` (N >= 2) as the first subcommand is blocked.

4. Git operation tracking covers not just `git` and `gh` but also `glab mr create` (GitLab) and `curl` POST to PR endpoints (any Git forge's REST API).

5. The `DISALLOWED_AUTO_BACKGROUND_COMMANDS` list currently contains only `sleep` -- it's the only command that should never be auto-backgrounded.
