---
description: "notification hooks — Stop session end, PreCompact PostCompact, SessionStart, FileChanged, SubagentStart ConfigChange lifecycle events"
---

# Notification, Stop, and Compact Hooks -- Arcanum Wiki

## Overview

Beyond tool lifecycle hooks, Claude Code provides hooks for notifications, turn boundaries (Stop), compaction events, and several other lifecycle moments. These hooks enable observability, custom workflow gates, and integration with external systems at key points in the conversation lifecycle.

## How It Works

### Stop Hook

Fires before Claude concludes its response (the model has finished all tool calls and is about to deliver its final text). Exit code 2 shows stderr to the model and continues the conversation -- enabling "one more thing" patterns where a hook checks the model's work and requests corrections.

### PreCompact / PostCompact

**PreCompact** (matcher = `trigger`: manual/auto): Fires before conversation compaction. Exit 0: stdout appended as custom compact instructions. Exit 2: block compaction entirely. This is the hook mechanism for injecting VoxCore-specific preservation instructions.

**PostCompact**: Fires after compaction completes. Receives the `compactSummary`. Exit 0: stdout shown to user. Can display a `userDisplayMessage` for post-compact notifications.

### Notification Hook

Fires when notifications are sent. Matcher = `notification_type`:
- `permission_prompt` -- permission dialog displayed
- `idle_prompt` -- idle prompt shown
- `auth_success` -- authentication completed
- `elicitation_dialog` / `elicitation_complete` / `elicitation_response` -- MCP elicitation

### SessionStart / SessionEnd

**SessionStart** (matcher = `source`: startup/resume/clear/compact): Fires when a session begins. Exit 0: stdout shown to Claude. Can return `initialUserMessage` and `watchPaths` via JSON output. The `CLAUDE_ENV_FILE` is writable for this event.

**SessionEnd** (matcher = `reason`: clear/logout/prompt_input_exit/other): Tight 1500ms timeout. Overridable via `CLAUDE_CODE_SESSIONEND_HOOKS_TIMEOUT_MS`. Fire-and-forget semantics.

### UserPromptSubmit

Fires when the user submits a prompt. Exit 0: stdout shown to Claude as additional context. Exit 2: block submission, erase prompt, show stderr to user. This enables input validation and transformation.

### FileChanged / CwdChanged

**FileChanged** (matcher = pipe-separated filenames like `.envrc|.env`): Fires when a watched file changes on disk. Uses Chokidar with 500ms stability threshold. Can return `watchPaths` to dynamically update the watch list.

**CwdChanged**: Fires after the working directory changes. Receives `old_cwd` and `new_cwd`. Can write to `CLAUDE_ENV_FILE` for environment updates.

### Setup Hook

Fires during repository setup (matcher = `trigger`: init/maintenance). Exit 0: stdout shown to Claude. Useful for bootstrapping project-specific tooling.

### TeammateIdle / TaskCreated / TaskCompleted

Team coordination hooks. `TeammateIdle` exit 2 prevents idle (teammate continues working). `TaskCreated` exit 2 prevents task creation. `TaskCompleted` exit 2 prevents task completion marking.

## Key Source Files

| File | Purpose |
|------|---------|
| `src/utils/hooks.ts` | All event execution, `executeHooksOutsideREPL()` for non-REPL events |
| `src/utils/hooks/fileChangedWatcher.ts` | Chokidar-based file watcher |
| `src/utils/hooks/hooksConfigManager.ts` | Event metadata and semantics |

## Cross-References

- [Hooks Overview](overview.md) -- Full system architecture
- [Hook Types](hook_types.md) -- Command, prompt, agent, http
- [Compaction Instructions](../core/compaction_instructions.md) -- PreCompact hook integration

## Interesting Findings

**Stop hook exit 2 continues conversation.** Unlike most events where exit 2 means "block," for Stop hooks exit 2 means "show stderr to model and keep going." This enables verification patterns where a hook checks the model's output and forces corrections.

**FileChanged watcher supports dynamic paths.** Hooks can return `hookSpecificOutput.watchPaths` (array of absolute paths) to update the Chokidar watch list at runtime, enabling adaptive file monitoring.

**Outside-REPL execution is limited.** Events that fire outside the REPL loop (notifications, session end, config changes) use `executeHooksOutsideREPL()` which returns flat arrays instead of yielded results, logs errors only to debug, and rejects prompt/agent/function hooks with stubs.
