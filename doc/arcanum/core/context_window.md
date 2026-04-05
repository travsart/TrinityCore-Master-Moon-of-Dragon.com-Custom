---
description: "context window — 200K default, 1M via [1m] suffix, budget allocation system tools conversation, context suggestions, auto-compact threshold 967K"
---

# Context Window Management -- Arcanum Wiki

## Overview

Claude Code manages context windows ranging from 200K to 1M tokens across different model families. The context window is divided into conceptual regions -- system prompt, tool definitions, CLAUDE.md/memory, conversation messages, output reserve, and auto-compact buffer -- though these are not formal API partitions. Client-side logic tracks token usage and triggers compaction when thresholds are breached.

## How It Works

### Context Window Sizes

From `src/utils/context.ts`:

```typescript
export const MODEL_CONTEXT_WINDOW_DEFAULT = 200_000
```

The 1M context is available for `claude-sonnet-4` and `opus-4-6` models. Resolution order for context window size:

1. `CLAUDE_CODE_MAX_CONTEXT_TOKENS` env var (ant-only)
2. `[1m]` suffix in model name -> 1,000,000
3. API `ModelCapability.max_input_tokens` (from `/models` endpoint, cached)
4. `CONTEXT_1M_BETA_HEADER` beta + model supports 1M -> 1,000,000
5. Sonnet 4.6 1M experiment flag -> 1,000,000
6. Ant-only model override config -> custom value
7. Default: **200,000**

### Budget Allocation

The context window is conceptually divided as follows:

```
+------------------------------------------+
|  System Prompt + Tool Definitions        |  ~15-25K tokens (variable)
|  CLAUDE.md + Memory Files                |  ~5-15K tokens (variable)
|  System Context (git status, etc.)       |  ~1-2K tokens
+------------------------------------------+
|  Conversation Messages                   |  Bulk of context
|    - User messages + tool results        |
|    - Assistant messages + thinking        |
+------------------------------------------+
|  Reserved for Output                     |  20K tokens (capped)
+------------------------------------------+
|  Autocompact Buffer                      |  13K tokens
+------------------------------------------+
```

For a 200K model, the available space for conversation is approximately 130-145K tokens. For 1M, it is approximately 950-965K tokens.

### Threshold Calculations

```
effectiveContextWindow = contextWindow - min(maxOutputTokens, 20_000)
autoCompactThreshold   = effectiveContextWindow - 13_000
warningThreshold       = autoCompactThreshold - 20_000
blockingLimit          = effectiveContextWindow - 3_000
```

| Model | Context | Effective | Autocompact | Warning | Blocking |
|-------|---------|-----------|-------------|---------|----------|
| Opus 4.6 (200K) | 200,000 | 180,000 | 167,000 | 147,000 | 177,000 |
| Opus 4.6 [1m] | 1,000,000 | 980,000 | 967,000 | 947,000 | 977,000 |

### Max Output Tokens

| Model | Default | Upper Limit |
|-------|---------|-------------|
| Opus 4.6 | 64,000 | 128,000 |
| Sonnet 4.6 | 32,000 | 128,000 |
| Opus 4.5 / Sonnet 4 / Haiku 4 | 32,000 | 64,000 |

A slot reservation optimization caps the default at 8,000 tokens (`CAPPED_DEFAULT_MAX_TOKENS`) because the p99 output is only 4,911 tokens. Requests that exceed 8K get one retry at the full limit.

### The `/context` Command

The `/context` command uses `analyzeContext.ts` (~900 lines) to provide a detailed breakdown:
- System prompt tokens (per-section)
- Memory file tokens (per-file)
- Tool definition tokens (always-loaded vs deferred, per-tool)
- MCP tool tokens (per-server, per-tool)
- Agent definition tokens
- Slash command tokens
- Skill frontmatter tokens
- Conversation message tokens (by type)
- Usage percentages and grid visualization

### Context Suggestions

`contextSuggestions.ts` generates warnings when context is bloated:

| Trigger | Threshold | Suggested Action | Est. Savings |
|---------|-----------|-----------------|--------------|
| Large Bash results | >15% of context, >10K tokens | Pipe through head/tail/grep | 50% |
| Large Read results | >5% of context | Use offset and limit | 30% |
| Large Grep results | >15% of context | More specific patterns | 30% |
| Large WebFetch | >15% of context | Extract specific info | 40% |
| Memory files | >5% of context, >5K tokens | Prune stale entries | 30% |
| Near capacity | >80% | Compact or start new conversation | - |

### Duplicate File Read Tracking

The system specifically tracks duplicate file reads. Each time the same file is re-read, the wasted tokens are calculated as `averageTokensPerRead * (count - 1)`. This is reported in telemetry as `duplicate_read_percent`.

## Key Source Files

| File | Purpose |
|------|---------|
| `src/utils/context.ts` | Context window sizes, model max output, 1M detection |
| `src/services/compact/autoCompact.ts` | Threshold calculations and trigger logic |
| `src/utils/analyzeContext.ts` | Full context breakdown for `/context` command |
| `src/utils/contextAnalysis.ts` | Token statistics by message type |
| `src/utils/contextSuggestions.ts` | Actionable optimization suggestions |
| `src/utils/tokenBudget.ts` | User-specified token budgets (+500k syntax) |

## Configuration

| Variable | Effect |
|----------|--------|
| `CLAUDE_CODE_MAX_CONTEXT_TOKENS` | Override context window (ant-only) |
| `CLAUDE_CODE_DISABLE_1M_CONTEXT=1` | Disable 1M context |
| `CLAUDE_CODE_AUTO_COMPACT_WINDOW` | Cap effective window for compaction |

## Cross-References

- [Token Counting](token_counting.md) -- How tokens are measured
- [Compaction Overview](compaction_overview.md) -- What happens when thresholds are breached

## Interesting Findings

**Thinking tokens count toward context.** Extended thinking blocks are included in the token estimate. With Opus 4.6's adaptive thinking, long chains of reasoning accumulate across turns and can push toward the auto-compact threshold.

**No explicit system prompt budget.** There is no reserved allocation for the system prompt. It consumes whatever tokens it needs, and auto-compaction summarizes conversation history (not the system prompt) when the threshold is approached. The system prompt is regenerated fresh each turn.

**Token budget syntax.** Users can specify token budgets in messages: `+500k` (shorthand at start), `use 2M tokens` (verbose form). This is parsed by `parseTokenBudget()` and used for continuation messages like "Stopped at 87% of token target."
