---
description: "secret undocumented features — Buddy tamagotchi, voice, computer use Chicago, Kairos autonomous, AFK mode, stickers, thinkback, 170+ env vars, undercover mode"
---

# Guide: Secret & Undocumented Features — Arcanum Wiki

> Features found in the source code that aren't documented, aren't in /help, or are feature-gated.

## Confirmed Hidden Features

### 1. Buddy (Tamagotchi Pet)

**Status**: In source code (`src/buddy/`, 6 files). Feature-gated.

Claude Code contains a virtual pet system. You can care for a digital creature with personality traits. The pet has needs (hunger, happiness, etc.) and responds to interaction.

Access: `/buddy` command (if feature flag is enabled)

### 2. Stickers

**Status**: In source code (`src/commands/stickers/`).

Claude Code has a sticker system. Details in the commands catalog.

Access: `/stickers`

### 3. Good Claude

**Status**: In source code (`src/commands/good-claude/`).

A command for giving Claude positive feedback. Likely tied to RLHF/analytics.

Access: `/good-claude`

### 4. Thinkback / Thinkback-Play

**Status**: In source code (`src/commands/thinkback/`, `src/commands/thinkback-play/`).

Replay or review Claude's internal thinking process from previous turns. "Thinkback-play" suggests an animated replay mode.

Access: `/thinkback`, `/thinkback-play`

### 5. BTW (By The Way)

**Status**: In source code (`src/commands/btw/`).

An informal way to add additional context or instructions. Name suggests it was designed for casual mid-conversation additions.

Access: `/btw`

### 6. Bughunter

**Status**: In source code (`src/commands/bughunter/`).

An automated bug hunting mode. Likely spawns agents to search for common bug patterns.

Access: `/bughunter`

### 7. Context Visualizer

**Status**: In source code (`src/commands/ctx_viz/`).

Visualizes the current context window — shows what's consuming tokens, how much space is left, what sections are loaded.

Access: `/ctx_viz`

### 8. Summary

**Status**: In source code (`src/commands/summary/`).

Generates a summary of the current conversation or session.

Access: `/summary`

### 9. Rewind

**Status**: In source code (`src/commands/rewind/`).

Undo or rewind to a previous state in the conversation. Like Ctrl+Z for conversations.

Access: `/rewind`

### 10. Tag

**Status**: In source code (`src/commands/tag/`).

Tag or label conversations/messages for later retrieval.

Access: `/tag`

## Feature-Gated Systems (Not Yet Released)

### Kairos (Autonomous Agent Mode)

**Feature flags**: `PROACTIVE`, `KAIROS`, `KAIROS_BRIEF`

Kairos is a proactive agent mode where Claude takes initiative without waiting for user prompts. It can:
- Monitor files for changes
- Suggest actions based on context
- Run maintenance tasks automatically

The BriefTool is part of Kairos — it generates concise status updates.

**Source**: `src/proactive/`, `tools/BriefTool/`

### Computer Use ("Chicago")

**Feature flag**: Not yet publicly available

Full computer automation — Claude can see your screen, move the mouse, click buttons, type text. Uses screenshot capture and coordinate mapping.

13 source files in `utils/computerUse/`:
- Screenshot capture and analysis
- Coordinate system (screen → model → action)
- Action types (click, type, scroll, drag)
- Platform-specific backends

### AFK Mode

**Feature flag**: `TRANSCRIPT_CLASSIFIER`

A mode where Claude continues working while you're away. Uses a transcript classifier to determine if human intervention is needed.

### Skill Search (Experimental)

**Feature flag**: `EXPERIMENTAL_SKILL_SEARCH`

Instead of loading all skill descriptions into the prompt, skills are searched on-demand. A DiscoverSkills tool lets the model find relevant skills by querying a search index.

### Cached Microcompact

**Feature flag**: `CACHED_MICROCOMPACT`

An optimization for the microcompact tier — caches the compaction configuration so it doesn't need to be recomputed.

### Connector Text Summarization

**Feature flag**: `CONNECTOR_TEXT`

Summarizes "connector text" (the non-tool-result parts of conversations) to save context.

### Native Client Attestation

**Feature flag**: `NATIVE_CLIENT_ATTESTATION`

Bun's native HTTP stack computes a hash attestation token (`cch`) that proves the request came from a real Claude Code binary, not a third-party client.

## Undocumented Environment Variables

Found via grep across the source:

| Variable | Purpose |
|----------|---------|
| `CLAUDE_CODE_ATTRIBUTION_HEADER` | Set to "false" to disable attribution header |
| `CLAUDE_CODE_ENTRYPOINT` | Override the reported entrypoint (cli/sdk/server) |
| `MCP_TOOL_TIMEOUT` | Override MCP tool timeout (default ~27.8h) |
| `USER_TYPE` | Set to "ant" for Anthropic employee features |
| `CLAUDE_CODE_MAX_OUTPUT_TOKENS` | Override max output token limit |
| `CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC` | Disable telemetry and analytics |
| `CLAUDE_CODE_USE_BEDROCK` | Use AWS Bedrock instead of direct API |
| `CLAUDE_CODE_USE_VERTEX` | Use Google Vertex instead of direct API |

## Undocumented Settings Keys

Found in schemas and source:

| Key | Type | Purpose |
|-----|------|---------|
| `enableAllProjectMcpServers` | boolean | Auto-trust all MCP servers in .claude/mcp.json |
| `scratchpadEnabled` | boolean | Enable a scratchpad directory for temp files |
| `scratchpadDir` | string | Path to scratchpad directory |
| `languagePreference` | string | Force response language |

## The Undercover Mode

**Source**: `utils/undercover.ts`

A mode where Claude Code hides that it's Claude Code. The `isUndercover()` function returns true in certain contexts, and various prompt sections are modified or omitted.

This is likely used for testing or benchmarking where knowing it's Claude Code would bias results.

## Internal Anthropic Features

These only activate when `USER_TYPE === 'ant'`:

| Feature | What |
|---------|------|
| `CLI_INTERNAL_BETA_HEADER` | Sends `cli-internal-2026-02-09` beta |
| Model override section | Ant employees get additional system prompt content |
| Debug tools | Additional debugging commands |

## The ToolSearch / Deferred Tools System

Instead of including all 40+ tool definitions in the system prompt (costing 3-5K tokens), tool search defers tools the model hasn't used recently. The model gets a ToolSearch tool that lets it "discover" tools on demand:

```
Model: "I need to search for files"
→ Calls ToolSearch("file search")
→ System returns GlobTool and GrepTool definitions
→ Model now has those tools available
```

This is gated by beta headers (`advanced-tool-use-2025-11-20`) and feature flags.

## Practical Implications

1. **Try hidden commands**: `/ctx_viz`, `/rewind`, `/summary`, `/tag` — they may or may not work in your version, but they exist in source
2. **Watch for Kairos**: When it ships, it'll be a paradigm shift — proactive Claude
3. **Computer Use is coming**: Full GUI automation will change what Claude Code can do
4. **AFK mode**: The transcript classifier suggests Claude will be able to work while you're gone
5. **Undercover mode**: If you're doing benchmarks, be aware this exists

## Cross-References

- [Buddy System](../hidden/buddy_system.md) — full Tamagotchi deep dive
- [Computer Use](../hidden/computer_use.md) — Chicago feature details
- [Voice Mode](../hidden/voice_mode.md) — push-to-talk features
- [UltraPlan](../hidden/ultraplan.md) — enhanced planning mode
- [Feature Flags Catalog](../config/feature_flags.md) — all tengu_* flags
- [Commands Catalog](../commands/) — every slash command documented
