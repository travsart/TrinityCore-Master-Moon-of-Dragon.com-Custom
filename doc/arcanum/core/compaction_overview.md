---
description: "compaction overview — 4-tier system, trigger thresholds 83% context, API microcompact, client microcompact, session memory compact, full LLM compact"
---

# Compaction System Overview -- Arcanum Wiki

## Overview

Claude Code's compaction engine is a multi-tiered context management system that prevents conversations from exceeding the model's context window. It operates across four distinct tiers, from lightweight per-request optimizations to full LLM-powered conversation summarization. The system is designed around a key insight: most conversations grow incrementally, so cheaper compaction strategies should be tried first, with expensive ones used only as fallbacks.

The compaction engine is not a single module but a coordinated set of strategies spread across roughly 16 source files under `src/services/compact/` and `src/utils/`. Each tier addresses a different scale of context pressure, and the system chains them in a cheapest-first order.

## How It Works

### The Four Tiers

The compaction system operates in four tiers, ordered from cheapest to most expensive:

**Tier 0 -- API Microcompact** (`apiMicrocompact.ts`). Server-side context management that piggybacks on regular API requests. Uses native API parameters (`context_management`) to instruct the server to clear old tool results and thinking blocks without any client-side message mutation. This is the lightest touch -- the client sends metadata alongside its normal request, and the server handles the actual clearing.

**Tier 1 -- Microcompact** (`microCompact.ts`). A per-turn, pre-request optimization that runs before every API call. It has two active sub-paths: time-based microcompact (clears old tool results when the server cache has expired after 60+ minutes of inactivity) and cached microcompact (uses the `cache_edits` API to delete tool results without invalidating the prompt cache). Legacy microcompact has been removed -- the code comment states "tengu_cache_plum_violet is always true."

**Tier 2 -- Session Memory Compact** (`sessionMemoryCompact.ts`). Uses a pre-extracted session memory summary as the compaction summary, avoiding an LLM call entirely. This is tried first by `autoCompactIfNeeded()` before falling back to Tier 3. It keeps recent messages intact (10-40K tokens) while replacing older messages with the session memory content. Requires both `tengu_session_memory` and `tengu_sm_compact` feature flags.

**Tier 3 -- Full LLM Compact** (`compact.ts`). The most expensive tier. Forks the conversation to a summarizer model that generates a structured summary of the entire conversation. The summary replaces all old messages, and post-compact hooks re-inject critical context (CLAUDE.md, skill content, recent files). Also supports partial compaction where only messages before or after a user-selected pivot point are summarized.

### Auto-Compact Trigger Flow

The auto-compaction pipeline runs after every API response:

```
query loop turn completes
  -> tokenCountWithEstimation() measures current context size
  -> shouldAutoCompact() compares against threshold
  -> autoCompactIfNeeded()
     -> trySessionMemoryCompaction()  [Tier 2 -- tried first, no LLM call]
     -> compactConversation()         [Tier 3 -- fallback with LLM summarization]
        -> streamCompactSummary()
           -> runForkedAgent()        [primary: cache-sharing fork]
           -> queryModelWithStreaming() [fallback if fork fails]
```

Tier 0 and Tier 1 run proactively on every request, independent of the auto-compact threshold. Tiers 2 and 3 only fire when the token count exceeds the calculated threshold.

### Threshold Calculation

For a standard 200K context model (e.g., Opus 4.6):
- Raw context window: **200,000 tokens**
- Output reserve: **-20,000** (capped at model's max output or 20K, whichever is smaller)
- Effective context window: **180,000**
- Auto-compact buffer: **-13,000**
- **Auto-compact threshold: 167,000 tokens** (83.5% of raw window)

For 1M context models (Opus 4.6 [1m]):
- Effective: **980,000**, threshold: **967,000** (96.7%)

### Circuit Breaker

After 3 consecutive auto-compact failures, the system stops attempting for the remainder of the session. This was added because telemetry showed 1,279 sessions with 50+ consecutive failures, wasting approximately 250K API calls per day globally (`autoCompact.ts`).

### Recursion Guards

Auto-compact is suppressed when `querySource` is `session_memory`, `compact`, or `marble_origami` (the context-collapse agent). This prevents deadlocks where compaction triggers itself.

## Key Source Files

| File | Purpose |
|------|---------|
| `src/services/compact/autoCompact.ts` | Threshold calculation, `shouldAutoCompact`, circuit breaker |
| `src/services/compact/compact.ts` | Full compaction orchestration (~1700 lines) |
| `src/services/compact/microCompact.ts` | Pre-request tool result clearing |
| `src/services/compact/apiMicrocompact.ts` | Server-side context_management edits |
| `src/services/compact/sessionMemoryCompact.ts` | Session-memory-based compaction (no LLM) |
| `src/services/compact/grouping.ts` | Groups messages by API round-trip boundaries |
| `src/services/compact/prompt.ts` | Compaction prompt templates |
| `src/services/compact/postCompactCleanup.ts` | Cache/state invalidation after compaction |
| `src/services/compact/timeBasedMCConfig.ts` | Time-based microcompact configuration |

## Configuration

| Variable / Setting | Effect |
|--------------------|--------|
| `DISABLE_COMPACT` | Disables ALL compaction (auto + manual) |
| `DISABLE_AUTO_COMPACT` | Disables auto-compact, keeps manual `/compact` |
| `CLAUDE_CODE_AUTO_COMPACT_WINDOW` | Override effective context window size |
| `CLAUDE_AUTOCOMPACT_PCT_OVERRIDE` | Override threshold as percentage (0-100) |
| `CLAUDE_CODE_BLOCKING_LIMIT_OVERRIDE` | Override the blocking limit |
| `autoCompactEnabled` in global config | User setting to disable auto-compact |

## Cross-References

- [Compaction Tiers](compaction_tiers.md) -- Deep dive on each tier's implementation
- [Compaction Instructions](compaction_instructions.md) -- How CLAUDE.md customization works
- [Context Window](context_window.md) -- Context window sizes and budget allocation
- [Token Counting](token_counting.md) -- How tokens are measured

## Interesting Findings

**MCP tool results are never micro-compacted.** The compactable tools set includes Read, Bash, Grep, Glob, WebSearch, WebFetch, Edit, and Write -- but NOT MCP tools. This means database queries and server status checks from custom MCP servers accumulate until full compaction fires, pushing toward the threshold faster than built-in tool results would.

**The transcript escape hatch.** After compaction, the model is told it can recover pre-compaction details by reading the full transcript file at a specific path. This means the model can theoretically reconstruct lost context on demand, though in practice this is rarely exercised.

**Warning and error thresholds are identical.** Both `WARNING_THRESHOLD_BUFFER_TOKENS` and `ERROR_THRESHOLD_BUFFER_TOKENS` are set to 20,000. The distinction exists in the type system but the numeric values produce the same trigger point (~147K tokens for 200K context).
