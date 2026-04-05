---
description: "PostToolUse hook — runs after tool execution, context injection, MCP output replacement, observe-only no blocking, logging audit trail"
---

# PostToolUse Hook -- Arcanum Wiki

## Overview

PostToolUse hooks fire after a tool has completed execution. They can observe results, inject additional context into the conversation, replace MCP tool output, stop conversation continuation, and report blocking errors. Unlike PreToolUse, PostToolUse hooks cannot modify tool input or make permission decisions (the tool has already run).

## How It Works

### When It Fires

PostToolUse fires after `tool.call()` completes successfully. A sibling event `PostToolUseFailure` fires instead when the tool execution fails, providing error details. The matcher field filters by `tool_name`.

### Hook Input

PostToolUse receives the tool input AND result:

```json
{
  "tool_name": "Bash",
  "tool_input": { "command": "npm test" },
  "tool_result": "All 42 tests passed",
  "session_id": "...",
  "project_dir": "/path/to/project"
}
```

PostToolUseFailure additionally includes: `error`, `error_type`, `is_interrupt`, `is_timeout`.

### What PostToolUse Can Do

**Inject additional context** (shown to model alongside the tool result):
```json
{
  "hookSpecificOutput": {
    "hookEventName": "PostToolUse",
    "additionalContext": "Note: 3 tests were skipped due to missing fixtures"
  }
}
```

**Replace MCP tool output** (MCP tools only):
```json
{
  "hookSpecificOutput": {
    "hookEventName": "PostToolUse",
    "updatedMCPToolOutput": { "transformed": "result" }
  }
}
```

**Stop continuation** (prevent the model from taking further action):
```json
{ "continue": false, "stopReason": "Build failed, manual review needed" }
```

**Report blocking error** (exit code 2): Stderr is shown to the model as a hook_blocking_error attachment.

### Integration with Tool Pipeline

From `src/services/tools/toolHooks.ts`:

```
for await (result of executePostToolHooks(...)):
  blockingError -> yield hook_blocking_error attachment
  preventContinuation -> yield hook_stopped_continuation + return
  additionalContexts -> yield hook_additional_context attachment
  updatedMCPToolOutput -> yield replacement output (MCP tools only)
```

### Exit Code Semantics

| Exit Code | PostToolUse Behavior |
|-----------|---------------------|
| 0 | Stdout shown in transcript mode |
| 2 | Stderr shown to model as blocking error |
| Other | Stderr shown to user only |

## Key Source Files

| File | Purpose |
|------|---------|
| `src/services/tools/toolHooks.ts` | `runPostToolUseHooks()`, `runPostToolUseFailureHooks()` |
| `src/utils/hooks.ts` | Execution engine |

## Cross-References

- [PreToolUse](pretooluse.md) -- Pre-execution counterpart
- [Hooks Overview](overview.md) -- System architecture

## Interesting Findings

**MCP output replacement is PostToolUse-only.** The `updatedMCPToolOutput` field is only processed for PostToolUse, not PreToolUse. This means you can transform what the model sees from MCP results, but you cannot modify MCP tool input via hooks (only built-in tool input can be modified via PreToolUse's `updatedInput`).

**PostToolUseFailure distinguishes interrupts from errors.** The `is_interrupt` and `is_timeout` fields let hooks differentiate between user-cancelled operations (Ctrl+C), timeout-killed operations, and genuine failures.
