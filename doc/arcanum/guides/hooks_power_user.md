---
description: "hooks power user guide — PreToolUse PostToolUse UserPromptSubmit, shell command hooks, JSON protocol, decision allow deny ask, timestamp injector example"
---

# Guide: Hooks for Power Users — Arcanum Wiki

> How to exploit the hook system for workflow automation, safety guardrails, and custom behavior.

## What Are Hooks?

Hooks are shell commands (or prompts/agents/HTTP calls) that execute in response to Claude Code events. They can:
- **Block** tool calls before they execute
- **Modify** behavior after tool calls complete
- **Inject** context into user messages
- **Notify** you about events (compaction, agent spawns, etc.)

## Hook Types

| Type | Command Format | What It Does |
|------|---------------|-------------|
| `shell` | `{"type": "command", "command": "python script.py"}` | Runs a shell command. Receives JSON on stdin, returns JSON on stdout |
| `prompt` | `{"type": "prompt", "prompt": "Check if this is safe"}` | Sends a prompt to a side-query Claude |
| `agent` | `{"type": "agent", "agentType": "general-purpose"}` | Spawns an agent to evaluate |
| `http` | `{"type": "http", "url": "..."}` | Makes an HTTP request |

## Hook Events (27 total)

### Pre-execution hooks (can block)
| Event | When | JSON Input |
|-------|------|-----------|
| `PreToolUse` | Before any tool executes | `{"tool_name": "...", "tool_input": {...}}` |
| `UserPromptSubmit` | When user submits a message | `{"user_message": "..."}` |

### Post-execution hooks (observe only)
| Event | When | JSON Input |
|-------|------|-----------|
| `PostToolUse` | After any tool executes | `{"tool_name": "...", "tool_input": {...}, "tool_result": "..."}` |

### Lifecycle hooks
| Event | When |
|-------|------|
| `PreCompact` | Before compaction starts |
| `PostCompact` | After compaction completes |
| `Notification` | Various system notifications |
| `Stop` | Session ending |
| `SubagentStart` | Agent/subagent spawning |
| `ConfigChange` | Settings changed |
| `FileChanged` | When tracked files are modified |

## The Decision Protocol

PreToolUse hooks return a JSON decision:

```json
{
  "decision": "allow",
  "reason": "This operation is safe"
}
```

| Decision | Effect |
|----------|--------|
| `allow` | Permits the tool call (but does NOT bypass deny rules from other sources!) |
| `deny` | Blocks the tool call. Claude sees the reason. |
| `ask` | Prompts the user for confirmation |
| `warn` | Shows a warning but continues (advisory) |

**CRITICAL**: `allow` from a hook does NOT bypass `deny` from another hook or permission rule. The 4-way permission race means ALL sources must agree. This is defense-in-depth — hooks are additive, not overriding.

**Source**: `hooks/toolPermission/` — the permission merger evaluates all sources.

## Setting Up Hooks

Hooks are configured in `.claude/settings.local.json`:

```json
{
  "hooks": {
    "PreToolUse": [
      {
        "matcher": "Edit",
        "hooks": [
          {
            "type": "command",
            "command": "python .claude/hooks/verify_edit.py",
            "statusMessage": "Verifying edit..."
          }
        ]
      }
    ],
    "UserPromptSubmit": [
      {
        "matcher": "",
        "hooks": [
          {
            "type": "command",
            "command": "python .claude/hooks/timestamp_injector.py"
          }
        ]
      }
    ]
  }
}
```

### Matcher Patterns

| Pattern | Matches |
|---------|---------|
| `""` (empty) | ALL events of that type |
| `"Edit"` | Only Edit tool calls |
| `"Bash"` | Only Bash tool calls |
| `"Write"` | Only Write tool calls |

## Practical Hook Examples

### 1. Timestamp Injector (UserPromptSubmit)

Injects current datetime into every user message:

```python
#!/usr/bin/env python3
import sys, json
from datetime import datetime

data = json.load(sys.stdin)
now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
print(json.dumps({
    "decision": "allow",
    "additionalContext": f"Current date/time: {now}"
}))
```

The `additionalContext` field appends text to the user message. Claude sees it but the user doesn't (it appears in `<user-prompt-submit-hook>` tags).

### 2. Release Gate Guard (PreToolUse)

Block `git push --tags` when release gate isn't PASS:

```python
#!/usr/bin/env python3
import sys, json

data = json.load(sys.stdin)
tool = data.get("tool_name", "")
tool_input = data.get("tool_input", {})

if tool == "Bash":
    cmd = tool_input.get("command", "")
    if "git push" in cmd and "--tags" in cmd:
        # Check release gate status
        try:
            with open(".claude/release-gate-status.json") as f:
                status = json.load(f)
                if status.get("status") != "PASS":
                    print(json.dumps({
                        "decision": "deny",
                        "reason": f"Release gate is {status.get('status')} — run /pre-ship first"
                    }))
                    sys.exit(0)
        except FileNotFoundError:
            print(json.dumps({
                "decision": "deny",
                "reason": "No release gate status found — run /pre-ship first"
            }))
            sys.exit(0)

print(json.dumps({"decision": "allow"}))
```

### 3. File Edit Verifier (PostToolUse)

Log all file edits for audit trail:

```python
#!/usr/bin/env python3
import sys, json
from datetime import datetime

data = json.load(sys.stdin)
tool = data.get("tool_name", "")
tool_input = data.get("tool_input", {})

if tool in ("Edit", "Write"):
    filepath = tool_input.get("file_path", "unknown")
    with open(".claude/edit_log.jsonl", "a") as f:
        f.write(json.dumps({
            "time": datetime.now().isoformat(),
            "tool": tool,
            "file": filepath
        }) + "\n")

# PostToolUse hooks are observe-only — no decision needed
sys.exit(0)
```

### 4. Dangerous Command Blocker (PreToolUse)

Block dangerous shell commands:

```python
#!/usr/bin/env python3
import sys, json, re

data = json.load(sys.stdin)
if data.get("tool_name") != "Bash":
    print(json.dumps({"decision": "allow"}))
    sys.exit(0)

cmd = data.get("tool_input", {}).get("command", "")

DANGEROUS = [
    r"rm\s+-rf\s+/",
    r"git\s+push\s+--force\s+(origin\s+)?(main|master)",
    r"DROP\s+DATABASE",
    r"DROP\s+TABLE",
    r"format\s+[a-z]:",
]

for pattern in DANGEROUS:
    if re.search(pattern, cmd, re.IGNORECASE):
        print(json.dumps({
            "decision": "deny",
            "reason": f"Blocked dangerous command matching: {pattern}"
        }))
        sys.exit(0)

print(json.dumps({"decision": "allow"}))
```

## Hook Gotchas

1. **Exit code matters**: Non-zero exit = hook failure = tool blocked. Always `sys.exit(0)` for allow.
2. **Timeout**: Hooks that take too long will be killed and treated as failures.
3. **JSON only**: stdin/stdout must be valid JSON. Print nothing else to stdout.
4. **statusMessage**: The `statusMessage` field in hook config shows text in the terminal spinner while the hook runs. Without it, hooks run silently.
5. **Hook allow != bypass**: A hook returning `allow` does NOT override a `deny` from another hook or permission rule. All sources must agree.
6. **Advisory mode**: Use `sys.exit(0)` with no JSON output for advisory-only hooks that shouldn't block anything (e.g., logging hooks).

## Advanced: Hook Testing

Test hooks by piping mock JSON:

```bash
echo '{"tool_name":"Bash","tool_input":{"command":"rm -rf /"}}' | python .claude/hooks/dangerous_blocker.py
```

Expected output:
```json
{"decision": "deny", "reason": "Blocked dangerous command matching: rm\\s+-rf\\s+/"}
```

## Cross-References

- [Hook System Overview](../hooks/overview.md) — full technical architecture
- [Permissions System](../permissions/overview.md) — how hooks interact with permissions
- [PreToolUse Deep Dive](../hooks/pretooluse.md) — the most powerful hook event
