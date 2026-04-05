---
description: "compact service — autoCompact microCompact sessionMemoryCompact, 4-tier compaction, grouping, time-based config, 167K/967K thresholds, postCompactCleanup"
---

# Compact Service -- Arcanum Wiki

## What Is This?

The compact service rewrites the conversation history into a condensed summary when context approaches the model's limit. It replaces all messages before a boundary marker with a summary, preserving critical context while dramatically reducing token count. This is Claude Code's primary mechanism for handling long sessions.

## How It Works

### Auto-Compact Trigger

`isAutoCompactEnabled()` checks whether automatic compaction is active (default true). When enabled, the system monitors context size after each API response. If tokens exceed the threshold (based on model context window), compaction triggers automatically.

### Compaction Process

The compact flow in `compact.ts`:

1. **Pre-compact hooks** execute via `executePreCompactHooks()`
2. **Strip images** from messages (images are not needed for summary generation)
3. **Group messages by API round** via `groupMessagesByApiRound()` for coherent summarization
4. **Wait for session memory** extraction to complete (up to 15 seconds)
5. **Inject session memory** content into the compact prompt if available
6. **Build the compact prompt** with instructions to preserve:
   - Current task state and goals
   - File paths and key code snippets
   - Error history and what was tried
   - User preferences and corrections
   - Plan mode state
7. **Stream the summary** via `queryModelWithStreaming()` with `COMPACT_MAX_OUTPUT_TOKENS`
8. **Create a compact boundary message** marking where old messages were replaced
9. **Re-inject critical context** after the boundary:
   - Recently read files (up to 5 files, 5K tokens each, 50K total budget)
   - Active skill content (5K per skill, 25K total)
   - MCP instruction deltas
   - Deferred tools delta
   - Agent listing delta
   - Plan file content
   - Task output content
   - Memory file content
10. **Post-compact hooks** execute, including prompt cache break notification
11. **Session activity signal** sent if tracking is active

### Partial Compaction

For subagents and specific scenarios, `partialCompact()` summarizes only a subset of messages (e.g., from a specific point forward) rather than the entire conversation.

### Cached Microcompact (Incremental)

A separate system (`apiMicrocompact.ts`, `microCompact.ts`) provides incremental context management:
- Uses the API's `cache_edits` feature to delete message ranges from the server-side cache
- Avoids the cost of a full summary generation
- Controlled by the `CACHED_MICROCOMPACT` feature flag
- Time-based configuration via `timeBasedMCConfig.ts`

### Compact Warning System

`compactWarningHook.ts` and `compactWarningState.ts` track context usage and warn users when compaction is approaching, giving them a chance to save important context.

## Key Source Files

| File | Purpose |
|------|---------|
| `compact.ts` | Core compaction logic, message grouping, post-compact injection |
| `prompt.ts` | Compact prompt construction |
| `autoCompact.ts` | Auto-compact threshold and trigger logic |
| `grouping.ts` | Message grouping by API round |
| `microCompact.ts` | Cached microcompact state management |
| `apiMicrocompact.ts` | API-level context management integration |
| `postCompactCleanup.ts` | Post-compact system prompt section refresh |
| `sessionMemoryCompact.ts` | Session memory integration with compaction |
| `compactWarningHook.ts` | Context usage warnings |
| `timeBasedMCConfig.ts` | Time-based microcompact configuration |

## Configuration

- Auto-compact: enabled by default, respects settings
- `COMPACT_MAX_OUTPUT_TOKENS` -- max tokens for the summary
- `POST_COMPACT_TOKEN_BUDGET = 50,000` -- budget for re-injected content
- `POST_COMPACT_MAX_FILES_TO_RESTORE = 5` -- max files re-read after compact
- `POST_COMPACT_MAX_TOKENS_PER_FILE = 5,000` -- per-file token cap
- `MAX_COMPACT_STREAMING_RETRIES = 2` -- retries on streaming failure

## Interesting Findings

1. **Images are stripped before compaction** to prevent the compact API call itself from hitting prompt-too-long limits, especially in CCD sessions where users attach many images.

2. **Session memory is the compaction bridge.** When session memory exists and is non-empty, it is injected into the compact prompt, providing structured context that survives summarization. The `isSessionMemoryEmpty()` check prevents injecting just the template.

3. **Post-compact file re-injection uses `FILE_UNCHANGED_STUB`** detection to avoid re-reading files that were already cached in the Read file state cache.

4. **The compact prompt includes discovered tool names** from tool_reference blocks, so tool search state survives compaction.

5. **`notifyCompaction()` resets the prompt cache break detector's baseline**, since compaction legitimately reduces message count and would otherwise trigger a false-positive cache break warning.
