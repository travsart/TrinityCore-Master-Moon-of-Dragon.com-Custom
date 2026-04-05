---
description: "SessionMemory service — tengu_session_memory feature gate, custom templates, session-scoped memory, config/ template.md prompt.md"
---

# Session Memory Service -- Arcanum Wiki

## What Is This?

Session Memory automatically maintains a structured markdown file with notes about the current conversation. It runs periodically in the background using a forked subagent to extract key information (task state, file paths, error history, workflow commands) without interrupting the main conversation flow. The session memory file persists across compactions, serving as the model's long-term memory within a single session.

## How It Works

Session Memory registers as a **post-sampling hook** via `registerPostSamplingHook()`. After each model response on the main REPL thread, the hook evaluates whether extraction should run based on two thresholds:

**Initialization threshold**: Context must reach `minimumMessageTokensToInit` (default 10,000 tokens) before session memory activates. This prevents premature extraction on short conversations.

**Update threshold** (both must be met):
- Token threshold: `minimumTokensBetweenUpdate` (default 5,000 tokens of context growth since last extraction)
- Tool call threshold: `toolCallsBetweenUpdates` (default 3 tool calls since last update)

Alternatively, extraction triggers at "natural conversation breaks" -- when the token threshold is met AND the last assistant turn has no tool calls.

**Extraction process:**
1. Creates the session memory directory at `~/.claude/session-memory/`
2. Reads the current memory file (or initializes from template)
3. Builds an update prompt with the current notes and any section size warnings
4. Runs a forked subagent (`runForkedAgent`) with `querySource: 'session_memory'`
5. The subagent can only use `Edit` on the exact memory file path
6. Records extraction token count and advances the cursor

The memory file uses a fixed template with sections: Session Title, Current State, Task Specification, Files and Functions, Workflow, Errors & Corrections, Codebase Documentation, Learnings, Key Results, and Worklog. Section headers and italic descriptions are immutable -- only content below them is updated.

**Section size management**: Each section is capped at ~2,000 tokens. The total file is capped at 12,000 tokens. When limits are exceeded, the prompt instructs the agent to aggressively condense.

**Custom templates**: Users can place custom templates at `~/.claude/session-memory/config/template.md` and custom prompts at `~/.claude/session-memory/config/prompt.md`.

## Key Source Files

| File | Purpose |
|------|---------|
| `sessionMemory.ts` | Core service: hook registration, threshold checks, extraction orchestration |
| `prompts.ts` | Template, update prompt, section analysis, variable substitution |
| `sessionMemoryUtils.ts` | Stateless utilities: config, cursors, wait-for-extraction |

## Configuration

- Feature gate: `tengu_session_memory` (GrowthBook, cached)
- Remote config: `tengu_sm_config` (GrowthBook, cached) -- overrides default thresholds
- Requires auto-compact to be enabled
- Disabled in remote mode
- Only runs on `repl_main_thread` query source

## Interesting Findings

1. **Extraction is serialized** via `sequential()` wrapper -- only one extraction runs at a time. A manual `/summary` command uses a separate code path but the same underlying mechanism.

2. **The memory file survives compaction.** During compact, session memory content is injected into the compact prompt so the summary includes session context. The `truncateSessionMemoryForCompact()` function caps each section before injection.

3. **`waitForSessionMemoryExtraction()`** with a 15-second timeout is called before compaction to ensure the latest notes are available. Stale extractions (>1 minute old) are ignored.

4. **Config is lazily initialized** -- remote config values are read from GrowthBook's disk cache (non-blocking) on first hook invocation, not at startup. This avoids blocking the REPL on network calls.
