---
description: "PreToolUse hook — runs before tool execution, input modification, permission decisions allow deny ask, if-conditions, most powerful hook type"
---

# PreToolUse Hook -- Arcanum Wiki

## Overview

PreToolUse is the most powerful hook event in Claude Code. It fires before every tool execution and can observe, modify input, block operations, inject additional context, and make permission decisions. It is the primary extension point for security policies, input validation, and custom workflow gates.

## How It Works

### When It Fires

PreToolUse fires after the model selects a tool and provides input, but before the tool's `call()` method executes. The matcher field filters by `tool_name` (e.g., `"Bash"`, `"Write|Edit"`, `"mcp__.*"`).

### Hook Input

The hook receives JSON on stdin with the tool invocation details:

```json
{
  "tool_name": "Bash",
  "tool_input": { "command": "npm install lodash", "description": "Install lodash" },
  "session_id": "...",
  "project_dir": "/path/to/project"
}
```

### What PreToolUse Can Do

**Block the tool** (exit code 2): Stderr is shown to the model as an error message, and the tool call is cancelled. The model sees the rejection reason and can adjust its approach.

**Modify input** (JSON output with `updatedInput`):
```json
{
  "hookSpecificOutput": {
    "hookEventName": "PreToolUse",
    "updatedInput": { "command": "npm install lodash --save-exact" }
  }
}
```
The modified input replaces the model's original input before the tool executes.

**Make permission decisions** (JSON output with `permissionDecision`):
```json
{
  "hookSpecificOutput": {
    "hookEventName": "PreToolUse",
    "permissionDecision": "allow",
    "permissionDecisionReason": "Git commands are pre-approved"
  }
}
```
Decisions: `allow`, `deny`, or implicit `ask`.

**Inject additional context** (JSON output with `additionalContext`):
```json
{
  "hookSpecificOutput": {
    "hookEventName": "PreToolUse",
    "additionalContext": "Warning: this file was recently modified by another user"
  }
}
```

**Stop continuation** (JSON output with `continue: false`):
```json
{ "continue": false, "stopReason": "Manual review required" }
```

### Permission Resolution

Hook `allow` does NOT bypass settings.json deny/ask rules. The resolution flow:

```
hook behavior === 'allow':
  -> If tool requiresUserInteraction and hook provided updatedInput
     -> treat interaction as satisfied
  -> If requiresInteraction or requireCanUseTool
     -> still go through canUseTool()
  -> checkRuleBasedPermissions() still applies
     -> deny rule overrides hook allow
     -> ask rule requires dialog despite hook approval
  -> Only if no rules match -> hook allow takes effect

hook behavior === 'deny':
  -> immediate deny

hook behavior === 'ask':
  -> force dialog with hook's ask message
```

### The `if` Condition

PreToolUse hooks can have an `if` condition using permission rule syntax:

```json
{
  "matcher": "Bash",
  "hooks": [{
    "type": "command",
    "command": "check-git.sh",
    "if": "Bash(git *)"
  }]
}
```

The `if` condition is evaluated before spawning the hook process, avoiding unnecessary work for non-matching commands. The evaluation uses `prepareIfConditionMatcher()` which does expensive work once (tool lookup, Zod validation, tree-sitter parsing for Bash) then returns a closure.

### Integration with Tool Pipeline

From `src/services/tools/toolHooks.ts`, the pre-tool hook bridge:

```
for await (result of executePreToolHooks(...)):
  blockingError -> yield deny PermissionResult
  preventContinuation -> yield stop
  permissionBehavior -> yield hookPermissionResult
  updatedInput -> yield modified input
  additionalContexts -> yield context attachment
  aborted -> yield cancelled + stop
```

## Key Source Files

| File | Purpose |
|------|---------|
| `src/utils/hooks.ts` | Core execution engine, matcher filtering |
| `src/services/tools/toolHooks.ts` | `runPreToolUseHooks()`, permission resolution |
| `src/schemas/hooks.ts` | Hook schema definitions |

## Cross-References

- [Hooks Overview](overview.md) -- System architecture
- [PostToolUse](posttooluse.md) -- The complementary post-execution hook
- [Permissions Overview](../permissions/overview.md) -- How hook decisions interact with permissions

## Interesting Findings

**Hook allow is the weakest override.** A single deny rule in settings.json overrides any number of hook `allow` decisions. This is a deliberate safety design -- hooks cannot bypass explicitly configured security policies.

**The `if` condition prevents unnecessary process spawning.** For Bash hooks, tree-sitter parsing is used to extract the command from the input, so a hook with `if: "Bash(git *)"` only fires for git commands, not every Bash invocation.
