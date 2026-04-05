---
description: "token counting — no local tokenizer, API countTokens, 3-strategy counting, 4 bytes per token estimate, tool result size limits 50K chars 100K tokens"
---

# Token Counting -- Arcanum Wiki

## Overview

Claude Code does NOT use a local tokenizer. There is no tiktoken, no sentencepiece, and no character-level tokenization library. Instead, token counting is done through three strategies in order of preference: the Anthropic API's `countTokens` endpoint, a Haiku fallback that measures via a minimal API call, and a rough character-based estimation. The canonical measurement function is `tokenCountWithEstimation()`, which is the single authoritative source for all threshold comparisons.

## How It Works

### Three Counting Strategies

**1. Anthropic API `countTokens` endpoint** -- The primary and most accurate path. Sends the messages and tool schemas to the API for exact counting. Available for direct Anthropic API access.

**2. Haiku Fallback** -- If `countTokens` fails or is unavailable (Bedrock, Vertex), sends messages to Haiku (or Sonnet for Vertex/thinking) with `max_tokens: 1` and reads the `input_tokens` from the usage response. This produces an actual token count but at the cost of a small API call.

**3. Rough Estimation** -- A character-based approximation with no network calls:

```typescript
// src/services/tokenEstimation.ts
export function roughTokenCountEstimation(
  content: string,
  bytesPerToken: number = 4,
): number {
  return Math.round(content.length / bytesPerToken)
}
```

Special cases:
- JSON files: `bytesPerToken = 2` (dense single-character tokens like `{`, `"`, `:`)
- Images and PDFs: flat **2,000 tokens** regardless of actual size
- Microcompact estimation: padded by 4/3 (33%) to be conservative

### The Canonical Measurement Function

`tokenCountWithEstimation()` in `src/utils/tokens.ts` is the ONLY function used for threshold checks:

```typescript
export function tokenCountWithEstimation(messages: readonly Message[]): number {
  // 1. Find the last assistant message with API usage data
  // 2. Walk back to first sibling with same message.id (parallel tool calls)
  // 3. Return: usage.input_tokens + cache_creation + cache_read + output_tokens
  //          + roughEstimate(messages added since that response)
}
```

It works by combining the last API response's actual token count with a rough estimate for any messages appended after that response (user messages, tool results). This hybrid approach provides accuracy for the bulk of context (from the API) with fast estimation for the delta.

**Critical detail for parallel tool calls**: When the model makes parallel tool calls, streaming emits separate assistant records per content block, all sharing the same `message.id`. The function walks back to the FIRST sibling to avoid undercounting interleaved tool_results.

**Source code warning**: "Do NOT use `messageTokenCountFromLastAPIResponse` for threshold comparisons. It only counts `output_tokens`. Use `tokenCountWithEstimation()` instead."

### Microcompact Token Estimation

For microcompact operations, a separate estimator with a conservative padding factor is used:

```typescript
// src/services/compact/microCompact.ts
export function estimateMessageTokens(messages: Message[]): number {
  // Counts text, tool_use, tool_result, thinking, etc.
  // Pad estimate by 4/3 to be conservative
  return Math.ceil(totalTokens * (4 / 3))
}
```

The 33% padding ensures microcompact never underestimates and accidentally leaves too much content in context.

### Tool Result Size Limits

Tool results are capped before they enter the context:

| Limit | Value | Source |
|-------|-------|--------|
| Default max result chars | 50,000 | `constants/toolLimits.ts` |
| Max tool result tokens | 100,000 | `constants/toolLimits.ts` |
| Max tool result bytes | 400,000 (400KB) | `constants/toolLimits.ts` |
| Per-message aggregate | 200,000 chars | `constants/toolLimits.ts` |
| Max file read tokens | 25,000 | `tools/FileReadTool/limits.ts` |
| Persisted output preview | 2,000 bytes | `utils/toolResultStorage.ts` |

When a tool result exceeds the threshold, it is persisted to disk and replaced with a preview:

```
<persisted-output>
Output too large (51.4KB). Full output saved to: /path/to/file.txt

Preview (first 2KB):
[first 2KB of content]
...
</persisted-output>
```

The per-message aggregate budget (200K chars) prevents parallel tools from collectively overwhelming the context -- "This prevents N parallel tools from each hitting the per-tool max and collectively producing e.g. 10 x 40K = 400K in one turn's user message."

## Key Source Files

| File | Purpose |
|------|---------|
| `src/utils/tokens.ts` | `tokenCountWithEstimation()` -- canonical measurement |
| `src/services/tokenEstimation.ts` | `roughTokenCountEstimation()`, `bytesPerTokenForFileType()`, API-based counting |
| `src/constants/toolLimits.ts` | Tool result size constants |
| `src/tools/FileReadTool/limits.ts` | File read output limits |
| `src/utils/toolResultStorage.ts` | Large result persistence |

## Configuration

| Variable | Effect |
|----------|--------|
| `CLAUDE_CODE_FILE_READ_MAX_OUTPUT_TOKENS` | Override max file read tokens |
| `tengu_amber_wren` (GrowthBook) | Remote config for file read limits |

## Cross-References

- [Context Window](context_window.md) -- How token counts relate to thresholds
- [Compaction Overview](compaction_overview.md) -- What happens when counts exceed limits

## Interesting Findings

**JSON is counted at 2 bytes per token.** The `bytesPerTokenForFileType()` function returns 2 for JSON/JSONL/JSONC files, versus 4 for all other text. This reflects the high density of single-character tokens (`{`, `}`, `"`, `:`, `,`) in JSON content.

**Images are always 2,000 tokens regardless of size.** Whether it is a 10KB thumbnail or a 5MB screenshot, the estimation assigns a flat 2,000 tokens. This matches the API's approximate cost for vision processing.

**The canonical function is hybrid.** It is not purely API-based or purely estimated. It anchors on the last API response's usage data and adds rough estimates only for the delta since then. This means immediately after an API response, the count is highly accurate, but it degrades in accuracy as more user messages and tool results accumulate before the next API call.
