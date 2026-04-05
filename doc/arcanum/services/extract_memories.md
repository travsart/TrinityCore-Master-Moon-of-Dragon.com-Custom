---
description: "extractMemories service — automatic memory extraction from conversations, memory file writing, duplicate detection, consolidation triggers"
---

# Extract Memories Service -- Arcanum Wiki

## What Is This?

Extract Memories is the background agent that automatically saves durable memories from the conversation to the auto-memory directory (`~/.claude/projects/<path>/memory/`). It runs at the end of each complete query loop (when the model produces a final response with no tool calls) and writes structured memory files with YAML frontmatter. This is the system behind Claude Code's persistent cross-session memory.

## How It Works

### Lifecycle

`initExtractMemories()` creates a closure-scoped state machine with:
- A UUID cursor (`lastMemoryMessageUuid`) tracking the last processed message
- An overlap guard (`inProgress`) preventing concurrent extractions
- A pending context stash for trailing runs
- A turn counter for throttling

The extractor fires via `handleStopHooks` at the end of each complete query loop.

### Extraction Decision

Before running, the extractor checks:
1. Not a subagent (only main agent extracts)
2. Feature gate `tengu_passport_quail` is enabled
3. Auto-memory is enabled in settings
4. Not in remote mode
5. Turn throttle: extractions only run every N eligible turns (`tengu_bramble_lintel`, default 1)

### Mutual Exclusion with Main Agent

The main agent's system prompt has full memory save instructions. When the main agent writes to memory files directly, the background extractor detects this via `hasMemoryWritesSince()` and skips that range, advancing the cursor past the main agent's writes. This prevents duplicate memory saves.

### The Extraction Agent

The forked agent receives:
- A prompt built by `buildExtractAutoOnlyPrompt()` or `buildExtractCombinedPrompt()` (with team memory)
- A pre-computed manifest of existing memory files (avoids a wasted `ls` turn)
- Permission to use: Read, Grep, Glob, read-only Bash, and Edit/Write only within the memory directory
- A 5-turn hard cap to prevent rabbit-holes

The prompt taxonomy has four memory types (defined in `memdir/memoryTypes.ts`):
- User preferences and workflow patterns
- Project-specific technical details
- Error corrections and what NOT to do
- Key decisions and their rationale

### Memory File Format

Each memory file uses YAML frontmatter:
```yaml
---
type: preference | technical | correction | decision
title: Short descriptive title
---
Actual memory content...
```

The `MEMORY.md` index file contains one-line pointers: `- [Title](file.md) -- one-line hook`.

### Coalescing and Trailing Runs

If an extraction is in progress when the next turn completes, the context is stashed. After the current extraction finishes, a trailing run processes the stashed context. Only the latest stashed context is kept (newer context has more messages).

## Key Source Files

| File | Purpose |
|------|---------|
| `extractMemories.ts` | Core service: initialization, extraction orchestration, tool permissions |
| `prompts.ts` | Extraction prompt templates (auto-only and combined team memory) |

## Configuration

- Feature gate: `tengu_passport_quail` (GrowthBook)
- Turn throttle: `tengu_bramble_lintel` (GrowthBook, default 1)
- Skip index: `tengu_moth_copse` (GrowthBook) -- skips MEMORY.md index updates
- Team memory: `TEAMMEM` feature flag
- Auto-memory must be enabled in project settings
- Disabled in remote mode

## Interesting Findings

1. **The state is closure-scoped, not module-level.** `initExtractMemories()` creates a fresh closure each call, and tests call it in `beforeEach` for natural isolation. This follows the same pattern as `confidenceRating.ts`.

2. **The `drainPendingExtraction()` function** exists for `print.ts` (non-interactive mode) to await in-flight extractions before the 5-second graceful shutdown failsafe kills the process.

3. **REPL tool compatibility**: When REPL mode is enabled (Ant default), primitive tools are hidden. The forked agent calls REPL instead, which internally re-invokes `canUseTool` for each inner primitive, so the permission checks still gate actual operations.

4. **The turn budget of 5 is intentionally tight.** The efficient pattern is: Turn 1 = parallel Read calls for all files to update; Turn 2 = parallel Write/Edit calls. Anything beyond that is a rabbit-hole.

5. **Memory file writes from the extraction agent trigger `createMemorySavedMessage()`** which surfaces as a system message in the conversation ("Memory saved: ..."), giving the user visibility into what was saved.
