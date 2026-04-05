---
description: "hooks system overview — 27 event types, execution pipeline, PreToolUse PostToolUse, permission precedence, 4 command types shell prompt agent http"
---

# Hook System Architecture -- Arcanum Wiki

## Overview

The hook system is Claude Code's extensibility backbone, allowing user-defined shell commands, LLM prompts, agentic verifiers, and HTTP webhooks to fire at 27 distinct lifecycle events. Hooks can observe, modify input, block operations, inject context, and make permission decisions. They execute in parallel with strict precedence rules (deny > ask > allow > passthrough) and support both sync and async execution modes.

## How It Works

### The 27 Event Types

**Tool lifecycle** (matcher = `tool_name`): PreToolUse, PostToolUse, PostToolUseFailure, PermissionRequest, PermissionDenied

**Session lifecycle**: SessionStart (matcher = `source`), SessionEnd (matcher = `reason`), Setup (matcher = `trigger`)

**Turn boundaries**: UserPromptSubmit, Stop, StopFailure (matcher = `error`)

**Agent events** (matcher = `agent_type`): SubagentStart, SubagentStop

**Compaction** (matcher = `trigger`): PreCompact, PostCompact

**Team/task**: TeammateIdle, TaskCreated, TaskCompleted

**MCP** (matcher = `mcp_server_name`): Elicitation, ElicitationResult

**Config/environment**: ConfigChange (matcher = `source`), CwdChanged, FileChanged (matcher = filename), InstructionsLoaded (matcher = `load_reason`), WorktreeCreate, WorktreeRemove

**Notification** (matcher = `notification_type`): Notification

### Execution Pipeline

```
Settings + plugins + session hooks + registered hooks
  -> captureHooksConfigSnapshot() at startup
  -> getHooksConfig() merges all sources
  -> getMatchingHooks() filters by matcher + if-condition
  -> executeHooks() runs all in parallel
  -> processHookJSONOutput() handles structured results
  -> AggregatedHookResult yielded to caller
```

### Config Sources

1. **Snapshot hooks** -- Frozen at startup from settings files
2. **Registered hooks** -- SDK callbacks and plugin native hooks
3. **Session hooks** -- Per-session in-memory hooks from agent frontmatter
4. **Function hooks** -- TypeScript callbacks for validation

Policy controls: `disableAllHooks` kills everything; `allowManagedHooksOnly` restricts to policy settings.

### Permission Behavior Precedence

When multiple hooks run in parallel, their decisions aggregate:
```
deny > ask > allow > passthrough
```
A single `deny` overrides any number of `allow` hooks.

## Key Source Files

| File | Purpose |
|------|---------|
| `src/utils/hooks.ts` | Central execution engine (~4900 lines) |
| `src/schemas/hooks.ts` | Zod schemas for 4 hook command types |
| `src/utils/hooks/hooksConfigSnapshot.ts` | Startup snapshot, policy controls |
| `src/utils/hooks/sessionHooks.ts` | Per-session hooks from agent frontmatter |
| `src/services/tools/toolHooks.ts` | Bridge between tool execution and hooks |

## Cross-References

- [Hook Types](hook_types.md) -- Shell, prompt, agent, http
- [PreToolUse](pretooluse.md) -- The most powerful hook
- [PostToolUse](posttooluse.md) -- Post-execution hooks
- [Notification Hooks](notification_hooks.md) -- Stop, compact, notification

## Interesting Findings

**Internal callback fast-path saves 70%.** When all matching hooks are internal callbacks, the engine skips span creation, progress messages, abort signal setup, and JSON processing: 6.01us -> 1.8us per PostToolUse hit.

**Hooks require workspace trust.** All hooks are skipped in interactive mode without trust dialog acceptance, preventing RCE in untrusted workspaces.

**Policy changes can never be blocked.** ConfigChange hooks for `policy_settings` always have `blocked: false` forced, regardless of exit code.
