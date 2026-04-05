---
description: "compaction tiers deep dive — tier 1 API microcompact, tier 2 client microcompact tool results, tier 3 SM-compact 10K-40K verbatim, tier 4 full LLM summarization"
---

# Compaction Tiers -- Arcanum Wiki

## Overview

Claude Code implements four compaction tiers, each targeting a different scale of context pressure. The tiers are designed to be tried in cheapest-first order: API microcompact and regular microcompact run proactively on every request, while session memory compact and full LLM compact fire only when the auto-compact threshold is breached. This article provides implementation-level detail for each tier.

## How It Works

### Tier 0: API Microcompact

**File**: `src/services/compact/apiMicrocompact.ts` (~154 lines)

API microcompact sends server-side instructions via the `context_management` parameter on API requests. The server handles clearing internally, meaning no client-side message mutation is required.

Two strategies are configured:

```typescript
// Strategy 1: Clear tool result content (Read, Bash, Glob, Grep, WebFetch, WebSearch)
{
  type: 'clear_tool_uses_20250919',
  trigger: { type: 'input_tokens', value: 180_000 },
  clear_at_least: { type: 'input_tokens', value: 140_000 },
  clear_tool_inputs: [Bash, Glob, Grep, Read, WebFetch, WebSearch],
}

// Strategy 2: Clear entire tool use blocks (everything except Edit, Write, NotebookEdit)
{
  type: 'clear_tool_uses_20250919',
  trigger: { type: 'input_tokens', value: 180_000 },
  clear_at_least: { type: 'input_tokens', value: 140_000 },
  exclude_tools: [Edit, Write, NotebookEdit],
}
```

Thinking block clearing is also managed here:

```typescript
{
  type: 'clear_thinking_20251015',
  keep: clearAllThinking ? { type: 'thinking_turns', value: 1 } : 'all',
}
```

The `clearAllThinking` flag activates when the session has been idle for over 1 hour (cache miss scenario), keeping only the last thinking turn.

**Important restriction**: Tool clearing via API microcompact is ant-only (`process.env.USER_TYPE !== 'ant'` guard). External users only receive thinking block management through this tier.

### Tier 1: Microcompact

**File**: `src/services/compact/microCompact.ts` (~531 lines)

Microcompact runs before every API request via `microcompactMessages()`. It operates through two active sub-paths, checked in order (first match wins):

**Time-Based Microcompact** fires when the gap since the last assistant message exceeds a configurable threshold (default 60 minutes, matching the server cache TTL). When the server cache is guaranteed cold, all tool results will be reprocessed anyway, so clearing old ones reduces what gets sent:

```typescript
const TIME_BASED_MC_CONFIG_DEFAULTS: TimeBasedMCConfig = {
  enabled: false,          // Controlled by GrowthBook flag tengu_slate_heron
  gapThresholdMinutes: 60, // Server cache TTL guaranteed expired
  keepRecent: 5,           // Keep last 5 compactable tool results
}
```

Content is replaced with `[Old tool result content cleared]`. The `keepRecent` value is floored at 1 because `slice(-0)` returns the full array (a JavaScript edge case that would paradoxically keep everything).

**Cached Microcompact** uses the cache editing API to remove tool results without invalidating the cached prefix. This is the primary optimization for active sessions:
- Does NOT modify local message content -- `cache_reference` and `cache_edits` are added at the API layer
- Tracks tool results via `CachedMCState` and queues deletions
- Only runs for the main thread (not subagents), only for supported models
- Gated by the `CACHED_MICROCOMPACT` feature flag

**Compactable tools** eligible for microcompact clearing:

```typescript
const COMPACTABLE_TOOLS = new Set<string>([
  FILE_READ_TOOL_NAME,    // Read
  ...SHELL_TOOL_NAMES,    // Bash, etc.
  GREP_TOOL_NAME,         // Grep
  GLOB_TOOL_NAME,         // Glob
  WEB_SEARCH_TOOL_NAME,   // WebSearch
  WEB_FETCH_TOOL_NAME,    // WebFetch
  FILE_EDIT_TOOL_NAME,    // Edit
  FILE_WRITE_TOOL_NAME,   // Write
])
```

MCP tools, Agent tool results, and other non-listed tools are never micro-compacted.

### Tier 2: Session Memory Compact

**File**: `src/services/compact/sessionMemoryCompact.ts` (~630 lines)

Session memory compact is the preferred auto-compact strategy because it avoids an LLM call entirely. Instead of generating a summary, it uses a pre-extracted session memory file that is maintained throughout the session by a separate background agent.

**Preconditions**:
- `tengu_session_memory` feature flag must be true
- `tengu_sm_compact` feature flag must be true
- Session memory content must exist and not be empty
- Can be force-enabled via `ENABLE_CLAUDE_CODE_SM_COMPACT` env var

**Configuration**:

```typescript
const DEFAULT_SM_COMPACT_CONFIG: SessionMemoryCompactConfig = {
  minTokens: 10_000,          // At least 10K tokens of messages kept
  minTextBlockMessages: 5,     // At least 5 messages with text kept
  maxTokens: 40_000,          // Hard cap: keep at most 40K tokens
}
```

**The `calculateMessagesToKeepIndex` algorithm**:
1. Start from the message after `lastSummarizedMessageId` (the boundary between summarized and new content)
2. If below `minTokens` or `minTextBlockMessages`, expand backwards to include more messages
3. Stop expanding if `maxTokens` is reached
4. Never expand past the last compact boundary
5. Run `adjustIndexToPreserveAPIInvariants()` to prevent splitting tool_use/tool_result pairs or thinking block merges

After building the post-compact messages, the system checks whether the estimated token count still exceeds the threshold. If it does, session memory compaction is abandoned and control falls through to Tier 3.

### Tier 3: Full LLM Compact

**File**: `src/services/compact/compact.ts` (~1700 lines)

Full compaction is the most expensive tier. It forks the conversation context and sends it to a model for structured summarization. The process:

1. Execute PreCompact hooks (user can inject custom instructions)
2. Strip images from messages (replaced with `[image]` markers)
3. Strip re-injected attachments (skill_discovery, skill_listing)
4. Generate summary via `streamCompactSummary()`:
   - **Primary**: `runForkedAgent()` -- forks conversation to reuse prompt cache
   - **Fallback**: `queryModelWithStreaming()` -- direct API call if fork fails
5. Clear file state cache
6. Generate post-compact file attachments (up to 5 files, 50K token budget)
7. Re-inject: plan files, skill content, deferred tool schemas, MCP instructions, agent listings
8. Execute SessionStart hooks (re-inject CLAUDE.md etc.)
9. Execute PostCompact hooks
10. Build result: `[boundary, summary, messagesToKeep, attachments, hookResults]`

**Post-compact file restoration limits**:

```typescript
export const POST_COMPACT_MAX_FILES_TO_RESTORE = 5
export const POST_COMPACT_TOKEN_BUDGET = 50_000
export const POST_COMPACT_MAX_TOKENS_PER_FILE = 5_000
export const POST_COMPACT_MAX_TOKENS_PER_SKILL = 5_000
export const POST_COMPACT_SKILLS_TOKEN_BUDGET = 25_000
```

Files are restored in recency order (most recently read files survive). Plan files and CLAUDE.md/memory files are excluded from this restoration because they are re-injected separately by SessionStart hooks.

**Prompt-too-long retry**: If the compaction API call itself exceeds the context window, `truncateHeadForPTLRetry()` drops the oldest API-round groups and retries up to 3 times. It drops enough groups to cover the token gap, or 20% of groups if the gap is unparseable.

**Partial compaction** supports two directions:
- `from`: Summarize messages AFTER a pivot, keep earlier messages (preserves prompt cache)
- `up_to`: Summarize messages BEFORE a pivot, keep later messages (invalidates prompt cache)

## Key Source Files

| File | Purpose |
|------|---------|
| `src/services/compact/apiMicrocompact.ts` | Server-side context_management edits |
| `src/services/compact/microCompact.ts` | Client-side pre-request tool result clearing |
| `src/services/compact/sessionMemoryCompact.ts` | Session-memory-based compaction |
| `src/services/compact/compact.ts` | Full LLM-powered compaction |
| `src/services/compact/grouping.ts` | API round-trip boundary detection |
| `src/services/compact/postCompactCleanup.ts` | Post-compaction cache invalidation |

## Configuration

| Setting | Default | Effect |
|---------|---------|--------|
| `tengu_slate_heron` (GrowthBook) | `{enabled: false}` | Time-based microcompact config |
| `CACHED_MICROCOMPACT` (feature flag) | Build-time | Enable cached microcompact |
| `tengu_session_memory` (GrowthBook) | `false` | Enable session memory system |
| `tengu_sm_compact` (GrowthBook) | `false` | Enable session memory compaction |
| `tengu_compact_cache_prefix` (GrowthBook) | `true` | Enable forked-agent cache sharing |

## Cross-References

- [Compaction Overview](compaction_overview.md) -- High-level system architecture
- [Compaction Instructions](compaction_instructions.md) -- Custom instructions integration
- [Context Window](context_window.md) -- Threshold calculations

## Interesting Findings

**Post-compact cleanup is subagent-aware.** Subagents run in the same process and share module-level state. Only main-thread compacts reset main-thread state (context-collapse, memory file cache). Without this guard, a subagent compacting would corrupt the main thread's state. The check is in `runPostCompactCleanup()` where `querySource` is validated before resetting shared caches.

**Skills survive compaction differently than files.** While files are restored by recency (max 5, 5K tokens each), skills are sorted by most-recent invocation and each truncated to 5K tokens with a total 25K token budget. The truncation marker tells the model it can Read the full file if needed. The `sentSkillNames` set is intentionally NOT cleared during compaction because re-injecting the full skill listing (~4K tokens) would be pure cache_creation with marginal benefit.

**Message grouping uses API round boundaries, not human turns.** The `groupMessagesByApiRound()` function creates groups based on `message.id` boundaries. Parallel tool calls from the same API response share an ID and stay in one group. This design enables reactive compact to work on single-prompt agentic sessions where the entire workload is one human turn.
