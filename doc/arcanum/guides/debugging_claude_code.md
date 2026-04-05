---
description: "debugging Claude Code — CLAUDE.md ignored, memory not loading, permission loops, MCP failures, truncated results, YOLO denial tracking, /doctor"
---

# Guide: Debugging Claude Code Itself — Arcanum Wiki

> When Claude Code behaves unexpectedly, here's how to diagnose what's happening using your knowledge of internals.

## Common Issues & Root Causes

### "Claude keeps ignoring my CLAUDE.md rules"

**Root cause**: CLAUDE.md is injected as a user message with `<system-reminder>` tags and "may or may not be relevant" hedging. The model treats it as lower authority than the actual system prompt.

**Fix**:
- Use emphatic language: "MUST", "ALWAYS", "NEVER", "CRITICAL"
- Put the most important rules first (attention decays)
- Keep CLAUDE.md focused — don't bury rules in walls of text
- Use `.claude/rules/` for domain-specific rules (they're also user messages, but shorter/focused)

### "Claude forgets context mid-session"

**Root cause**: Compaction fired. Check if the conversation was summarized.

**Diagnosis**:
- Use `/cost` or `/stats` to see token usage
- If token count dropped significantly, compaction happened
- Check if `## Compaction Instructions` is in your CLAUDE.md

**Fix**:
- Add compaction instructions
- Write critical context to files before it's lost
- Use `/checkpoint` before topic shifts
- Enable 1M context (`[1m]` suffix) to delay compaction

### "Memory files aren't being loaded"

**Root cause**: The Sonnet memory selector only sees filenames + `description` frontmatter.

**Diagnosis**:
- Check if the file has a `description` in its YAML frontmatter
- Check if the filename is descriptive enough
- Check if you have >200 memory files (oldest by mtime become invisible)

**Fix**:
- Add keyword-rich `description` frontmatter
- Use descriptive filenames
- `touch` important files to keep them within the 200-file window
- Run `/memory-audit` to find issues

### "Permission keeps asking even though I said allow"

**Root cause**: Permission system has 8 rule sources. `allow` from one source doesn't override `deny`/`ask` from another.

**Diagnosis**:
- Check all permission config files:
  - `~/.claude/settings.json`
  - `~/.claude/settings.local.json`
  - `.claude/settings.json`
  - `.claude/settings.local.json`
- Check PreToolUse hooks — a hook might be returning `ask`
- Check your permission mode (`/permissions`)

**Fix**:
- Add explicit `allow` rules for the specific pattern
- Check hooks for unintended blocking
- Switch to a more permissive mode if appropriate

### "Subagents can't find files that exist"

**Root cause**: Subagents inherit the parent's CWD and environment, but they start fresh conversations. They don't have your conversation history.

**Diagnosis**:
- Check if you gave the agent absolute file paths
- Check if the files are in the CWD or a subdirectory

**Fix**:
- Always give agents ABSOLUTE file paths, not relative
- Include relevant context in the agent's prompt
- Use `isolation: "worktree"` if the agent needs a clean git state

### "MCP server tools aren't showing up"

**Root cause**: MCP server failed to start, or its tool registration failed.

**Diagnosis**:
- Check server process: `ps aux | grep mcp`
- Check MCP config: `.claude/settings.local.json` or `.claude/mcp.json`
- Try `/mcp` command to see server status

**Fix**:
- Verify the command and args in MCP config
- Check that the server script exists and is executable
- Check for Python/Node.js dependency issues
- Restart Claude Code (MCP servers connect at startup)

### "Tool results seem truncated"

**Root cause**: Tool results >50K characters are persisted to disk and replaced with a preview.

**Diagnosis**:
- The result will say "Full output saved to: [path]"
- The model gets a preview + the file path

**Fix**:
- This is by design — it prevents context bloat
- Use `offset` and `limit` parameters on Read to get smaller chunks
- For Bash, pipe through `head` or `tail`
- The full result IS on disk if you need it

### "Claude is being too cautious / asking too many permissions"

**Root cause**: YOLO classifier consecutive-denial tracking. After several denials, it backs off and asks for everything.

**Diagnosis**:
- Think about whether you recently denied several operations
- Check your permission mode

**Fix**:
- Approve a few safe operations to reset the denial counter
- Add explicit allow rules for common patterns
- Switch to a more permissive mode

## Diagnostic Commands

| Command | What It Shows |
|---------|-------------|
| `/stats` | Token usage, model, cache hits |
| `/cost` | Running cost for the session |
| `/context` | What's in the current context |
| `/ctx_viz` | Visual context breakdown (hidden command) |
| `/permissions` | Current permission mode and rules |
| `/mcp` | MCP server connection status |
| `/hooks` | Active hooks |
| `/skills` | Loaded skills |
| `/status` | Overall system status |
| `/doctor` | Diagnostic health check |

## Environment Variables for Debugging

| Variable | Purpose |
|----------|---------|
| `CLAUDE_CODE_DEBUG=1` | Enable debug logging |
| `CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC=1` | Disable telemetry (cleaner logs) |
| `CLAUDE_CODE_LOG_LEVEL=debug` | Verbose logging |

## Understanding Error Messages

### "Tool result too large, persisted to disk"
Not an error. This is the 50K char / 100K token limit working as intended.

### "Context window approaching limit, compacting..."
Compaction is firing. Your conversation will be summarized.

### "MCP server [name] failed to connect"
The MCP server process couldn't start or didn't respond to the initialize handshake.

### "Permission denied by hook"
A PreToolUse hook returned `{"decision": "deny"}`. Check your hooks.

### "Rate limited, retrying in X seconds"
Anthropic API rate limit hit. Exponential backoff will retry automatically.

## The /doctor Command

The `/doctor` command (found in `src/commands/doctor/`) runs a health check:
- Verifies API connectivity
- Checks configuration validity
- Tests MCP server connections
- Validates hook scripts
- Reports version information

Use it when something feels wrong but you can't pinpoint what.

## Cross-References

- [Permission System](permissions_deep_dive.md) — how permissions work
- [Compaction Survival](compaction_survival.md) — surviving context compression
- [Memory Mastery](memory_mastery.md) — memory file troubleshooting
- [Hooks Guide](hooks_power_user.md) — hook debugging
- [MCP Servers](mcp_servers.md) — MCP troubleshooting
