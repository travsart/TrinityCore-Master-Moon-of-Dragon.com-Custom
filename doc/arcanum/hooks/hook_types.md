---
description: "hook command types — shell command, prompt side-query, agent spawn, http request, JSON protocol, statusMessage, additionalContext injection"
---

# Hook Command Types -- Arcanum Wiki

## Overview

Claude Code supports four user-configurable hook command types -- shell commands, LLM prompts, agentic verifiers, and HTTP webhooks -- plus two internal types (callbacks and function hooks). Each type has different execution mechanics, timeout behaviors, and output processing rules.

## How It Works

### Command Hook (Shell)

The most common type. Executes a shell command with hook input piped to stdin as JSON:

```json
{ "type": "command", "command": "check-git.sh", "timeout": 60 }
```

Shell resolution: On Windows, bash hooks spawn via Git Bash. PowerShell hooks use `pwsh -NoProfile -NonInteractive -Command`. The `CLAUDE_CODE_SHELL_PREFIX` env var wraps bash commands only.

Environment variables injected: `CLAUDE_PROJECT_DIR`, `CLAUDE_PLUGIN_ROOT`, `CLAUDE_PLUGIN_DATA`, `CLAUDE_PLUGIN_OPTION_*`, `CLAUDE_ENV_FILE`.

Exit code semantics: 0 = success (stdout processed), 2 = blocking error (stderr shown to model), other = non-blocking error (stderr shown to user only).

### Prompt Hook (LLM)

Sends the hook input to a small fast model with a custom prompt:

```json
{ "type": "prompt", "prompt": "Check if $ARGUMENTS contains sensitive data" }
```

The `$ARGUMENTS` placeholder is replaced with the JSON hook input. Requires a `ToolUseContext` to run. Default model: small/fast.

### Agent Hook (Agentic Verifier)

Spawns a sub-agent with the given prompt and full conversation context:

```json
{ "type": "agent", "prompt": "Verify the code change is correct", "timeout": 60 }
```

Default model: Haiku. Default timeout: 60 seconds. Requires both `ToolUseContext` and `messages` array.

### HTTP Hook (Webhook)

POSTs JSON to a URL and expects a JSON response:

```json
{ "type": "http", "url": "https://api.example.com/hook", "headers": { "Authorization": "Bearer $TOKEN" } }
```

Protected by SSRF guard (blocks private/internal IPs), URL allowlist, and header injection prevention (CR/LF/NUL stripped). Only `allowedEnvVars` are interpolated into headers. Blocked for SessionStart and Setup events to prevent deadlocks in headless mode.

### Shared Features

All types support: `if` condition (permission rule syntax filter), `timeout` (seconds), `statusMessage` (custom spinner text), `once` (run once then remove), `async` (background execution), `asyncRewake` (background + rewake on exit 2).

## Key Source Files

| File | Purpose |
|------|---------|
| `src/schemas/hooks.ts` | Zod schemas for all 4 types |
| `src/utils/hooks/execPromptHook.ts` | Prompt hook executor |
| `src/utils/hooks/execAgentHook.ts` | Agent hook executor |
| `src/utils/hooks/execHttpHook.ts` | HTTP hook executor with SSRF guard |

## Cross-References

- [Hooks Overview](overview.md) -- System architecture
- [PreToolUse](pretooluse.md) -- Where hooks are most powerful

## Interesting Findings

**Async hooks can rewake the model.** When `asyncRewake` is set and the background hook exits with code 2, it enqueues a task-notification that wakes the model either at idle or mid-query. This enables long-running checks that only interrupt when problems are found.

**Default timeout is 10 minutes** (`TOOL_HOOK_EXECUTION_TIMEOUT_MS = 600,000`). Session end hooks have a much tighter 1.5 second default.
