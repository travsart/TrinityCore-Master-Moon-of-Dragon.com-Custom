# API Layer & Streaming — Claude Code Internals Report

> v2.1.88 baseline + cli.js@2.1.97 grep refresh (2026-04-08)

## 2.1.97 Delta (summary)

| Change | Version | Type |
|---|---|---|
| **`X-Claude-Code-Session-Id` header** — already documented. cli.js@2.1.97 confirms: `"X-Claude-Code-Session-Id":N8()` in request-headers literal. Also confirms NEW `x-app` variant: background-mode sends `"cli-bg"` instead of `"cli"` (`DA6()?"cli-bg":"cli"`). | 2.1.86 | refines |
| **`x-client-request-id` header** — already documented. cli.js@2.1.97 has 2 literal matches; surfaced in API error messages (`API error x-client-request-id=${w} (give this to the API team for server-log lookup)`). | 2.1.84 | no-op |
| **`--print` SDK stdout streaming** (`--output-format=stream-json` + `--include-partial-messages`). Verified in cli.js@2.1.97 via the CLI option `"Include partial message chunks as they arrive (only works with --print and --output-format=stream-json)"`. Windows-disable claim not verifiable from grep alone (changelog only). | 2.1.78 / 2.1.81 | gap |
| **`CLAUDE_STREAM_IDLE_TIMEOUT_MS` env var** — already documented. Default 90000 ms; watchdog gated on `CLAUDE_ENABLE_STREAM_WATCHDOG`. cli.js@2.1.97: `parseInt(process.env.CLAUDE_STREAM_IDLE_TIMEOUT_MS||"",10)||90000`. | 2.1.84 | no-op |
| **`MAX_NON_STREAMING_TOKENS = 64_000`** — already documented. The 21k→64k raise happened in 2.1.83 by bypassing the SDK's 10-min timeout via a client-level timeout. cli.js@2.1.97: `CdY=64000`. | 2.1.83 | no-op |
| **`ANTHROPIC_DEFAULT_*_MODEL_*` env vars (12 total).** Correct name is `ANTHROPIC_DEFAULT_<TIER>_MODEL_SUPPORTED_CAPABILITIES`, not `_MODEL_SUPPORTS` as the changelog said. Full quad per tier × 3 tiers: `_MODEL`, `_MODEL_NAME`, `_MODEL_DESCRIPTION`, `_MODEL_SUPPORTED_CAPABILITIES`. Plus a 4-var custom model family: `ANTHROPIC_CUSTOM_MODEL_OPTION` + `_NAME` + `_DESCRIPTION` + `_SUPPORTED_CAPABILITIES` (16 total model-identity env vars). | 2.1.84 | gap |
| **`ANTHROPIC_CUSTOM_MODEL_OPTION`** (2.1.78) — lets the user inject an arbitrary model into the picker. Whitelisted for validity checks: `if (K === process.env.ANTHROPIC_CUSTOM_MODEL_OPTION) return {valid: !0}`. | 2.1.78 | gap |
| **429 Retry-After cap (NEW in 2.1.97).** When a server 429 sets `Retry-After > 60s` and the session is NOT in unattended-retry mode, the client immediately throws rather than waiting. Constant: `xs_=60000`. Telemetry: `tengu_api_retry_after_too_long` with `{delayMs, status, provider}`. v2.1.88 `withRetry.ts` has no equivalent branch. | 2.1.97 | invalidates |
| **Long-retry visibility fix (2.1.94).** During persistent-retry waits > 60s, the loop yields a `system/api_error` message with `{retryInMs, retryAttempt, maxRetries}` every 30s (`ms_=30000`) so the UI ticks down visibly instead of freezing. Telemetry: `tengu_api_persistent_retry_wait`. v2.1.88 had a single `await sleep(delayMs)` with no per-iteration yield — the "stuck agent" symptom. | 2.1.94 | invalidates |
| **`CLAUDE_CODE_USE_MANTLE=1`** — Amazon Bedrock powered by Mantle, 6th provider auth path. Provider detection: `B6(process.env.CLAUDE_CODE_USE_MANTLE)?"mantle"` added alongside existing `firstParty`/`bedrock`/`vertex`/`foundry`. Sibling skip-auth: `CLAUDE_CODE_SKIP_MANTLE_AUTH`. | 2.1.94 | gap |
| **6th provider `CLAUDE_CODE_USE_ANTHROPIC_AWS`** — auth string `"anthropicAws"`, sibling `CLAUDE_CODE_SKIP_ANTHROPIC_AWS_AUTH`. Not in changelog brief but present in cli.js@2.1.97. Update Section 14 from "Four provider paths" to **six providers**: firstParty, Bedrock, Vertex, Foundry, Mantle, anthropicAws. | 2.1.97 | gap |
| **Default effort bump medium→high** — server-side change for API-key / Bedrock / Vertex / Foundry / Team / Enterprise. Client doesn't explicitly send `effort` for these — the server resolves. cli.js@2.1.97 `Bo6()` only returns `"medium"` for Pro/Max on Opus 4.6 or ultrathink+pro/max+thinking-supported. Display fallback has been `"high"` since v2.1.88 (`getDisplayedEffortLevel` returns `'high'` when no client default). | 2.1.94 | gap |

## Overview

The API layer is the nerve center of Claude Code, responsible for every interaction between the client and the Anthropic Messages API. It handles model selection, authentication across four providers (firstParty, Bedrock, Vertex, Foundry), streaming SSE parsing, retry/backoff logic, token counting, cost tracking, prompt caching, and beta header negotiation. The system is engineered for resilience: streaming failures fall back to non-streaming, 529 overloads trigger exponential backoff with model fallback, and stale connections are detected and recycled.

The implementation is split across ~20 files in `src/services/api/` with the primary orchestrator being `claude.ts` (3,400+ lines) — a single `queryModel()` async generator that constructs the API request, manages the streaming loop, accumulates content blocks, and tracks usage/cost. The retry layer (`withRetry.ts`) wraps every API call with configurable retry counts, exponential backoff, fast-mode fallback, and persistent retry for unattended sessions. The conversation loop in `query.ts` (1,700+ lines) drives the agentic tool-use cycle on top of this foundation.

This is arguably the most critical infrastructure in the codebase. Every token of input and output, every dollar of cost, every millisecond of latency flows through these files.

## Architecture

### Key Files

| File | Size | Purpose |
|------|------|---------|
| `src/services/api/claude.ts` | 129KB | Main API orchestrator — request construction, streaming, cost tracking |
| `src/services/api/withRetry.ts` | 29KB | Retry/backoff logic, model fallback, persistent retry |
| `src/services/api/client.ts` | 16KB | Anthropic SDK client factory (4 providers) |
| `src/services/api/errors.ts` | 43KB | Error classification, user-facing error messages |
| `src/services/api/logging.ts` | 25KB | Analytics logging for API calls (success/error/gateway detection) |
| `src/services/api/promptCacheBreakDetection.ts` | 27KB | Detects and diagnoses prompt cache breaks |
| `src/services/tokenEstimation.ts` | 17KB | Token counting (API-based and rough estimation) |
| `src/cost-tracker.ts` | 11KB | Session cost accumulation and display |
| `src/utils/modelCost.ts` | 7KB | Per-model pricing tables |
| `src/utils/betas.ts` | 14KB | Beta header assembly per model/provider |
| `src/utils/context.ts` | ~7KB | Context window sizes, 1M detection |
| `src/constants/betas.ts` | 2KB | Beta header string constants |
| `src/query.ts` | 70KB | Conversation loop (agentic tool-use cycle) |
| `src/QueryEngine.ts` | 48KB | SDK/headless query engine |

### Data Flow

```
User Input
    │
    ▼
query.ts (queryLoop)
    │ Prepends userContext, builds systemPrompt
    │ Runs microcompact, autocompact, snip
    │ Calls deps.callModel (→ queryModelWithStreaming)
    ▼
claude.ts (queryModel)
    │ Assembles betas, tools, system prompt blocks
    │ Configures effort, thinking, task budget
    │ Adds cache breakpoints
    ▼
withRetry.ts (withRetry)
    │ Creates Anthropic client (client.ts)
    │ Manages retry loop with backoff
    ▼
Anthropic SDK (beta.messages.create)
    │ Streaming: { stream: true } + .withResponse()
    │ Non-streaming fallback on error
    ▼
SSE Stream (for await of stream)
    │ message_start → content_block_start →
    │ content_block_delta → content_block_stop →
    │ message_delta → message_stop
    ▼
AssistantMessage objects yielded back through generators
    │
    ▼
query.ts: tool execution → tool_result → next API call
```

## Key Implementation Details

### 1. API Call Construction

The core request is built in `paramsFromContext()` inside `claude.ts:1538-1729`. This closure captures the full query context and is called on each retry attempt:

```typescript
// claude.ts:1699-1729
return {
  model: normalizeModelStringForAPI(options.model),
  messages: addCacheBreakpoints(messagesForAPI, enablePromptCaching, ...),
  system,
  tools: allTools,
  tool_choice: options.toolChoice,
  ...(useBetas && { betas: betasParams }),
  metadata: getAPIMetadata(),
  max_tokens: maxOutputTokens,
  thinking,
  ...(temperature !== undefined && { temperature }),
  ...(contextManagement && useBetas &&
    betasParams.includes(CONTEXT_MANAGEMENT_BETA_HEADER) && {
    context_management: contextManagement,
  }),
  ...extraBodyParams,
  ...(Object.keys(outputConfig).length > 0 && { output_config: outputConfig }),
  ...(speed !== undefined && { speed }),
}
```

**Headers sent on every request** (`client.ts:105-116`):
- `x-app: cli`
- `User-Agent`: Custom user agent string
- `X-Claude-Code-Session-Id`: Session UUID
- `x-client-request-id`: Per-request UUID (first-party only)
- Custom headers from `ANTHROPIC_CUSTOM_HEADERS` env var
- Optional `x-anthropic-additional-protection: true`
- Optional `x-claude-remote-container-id` and `x-claude-remote-session-id`

**Metadata** (`claude.ts:503-528`): The `metadata.user_id` field is a JSON string containing `device_id`, `account_uuid`, `session_id`, plus any `CLAUDE_CODE_EXTRA_METADATA`.

**Extra body params** (`claude.ts:272-331`): Parsed from `CLAUDE_CODE_EXTRA_BODY` env var. Also includes anti-distillation `fake_tools` opt-in for first-party CLI and beta headers for Bedrock.

### 2. Streaming Implementation

Claude Code uses **raw streams** instead of the SDK's `BetaMessageStream` to avoid O(n^2) partial JSON parsing:

```typescript
// claude.ts:1818-1836
// Use raw stream instead of BetaMessageStream to avoid O(n²) partial JSON parsing
// BetaMessageStream calls partialParse() on every input_json_delta, which we don't need
const result = await anthropic.beta.messages
  .create(
    { ...params, stream: true },
    {
      signal,
      ...(clientRequestId && {
        headers: { [CLIENT_REQUEST_ID_HEADER]: clientRequestId },
      }),
    },
  )
  .withResponse()
```

The streaming loop (`claude.ts:1940-2304`) processes SSE events:

- **`message_start`**: Captures `partialMessage`, records TTFB, extracts initial usage
- **`content_block_start`**: Initializes content block by type (text, tool_use, server_tool_use, thinking). Text is initialized to empty string to avoid SDK duplication bug
- **`content_block_delta`**: Accumulates deltas — `text_delta` appends to `.text`, `input_json_delta` appends to `.input` (string accumulation), `thinking_delta` appends to `.thinking`, `signature_delta` sets `.signature`
- **`content_block_stop`**: Creates an `AssistantMessage` from the completed block and yields it
- **`message_delta`**: Final usage/stop_reason. Updates cost tracking via `addToTotalSessionCost()`. Checks for refusal, max_tokens, model_context_window_exceeded

**Tool input accumulation**: Tool use inputs are accumulated as raw JSON strings during streaming (`contentBlock.input += delta.partial_json`), then parsed as a complete JSON object at `content_block_stop` via `normalizeContentFromAPI()`.

**Stream idle watchdog** (`claude.ts:1874-1928`): When `CLAUDE_ENABLE_STREAM_WATCHDOG` is set, a configurable idle timeout (`CLAUDE_STREAM_IDLE_TIMEOUT_MS`, default 90s) kills hung streams. A warning fires at half the timeout. The watchdog uses `setTimeout` with timer reset on each chunk.

**Stall detection** (`claude.ts:1936-1966`): Independently tracks inter-chunk delays. Any gap >30s is logged as a streaming stall with analytics.

### 3. Non-Streaming Fallback

When streaming fails (any error except user abort), the system falls back to non-streaming:

```typescript
// claude.ts:2504-2569
logForDebugging('Error streaming, falling back to non-streaming mode')
didFallBackToNonStreaming = true
const result = yield* executeNonStreamingRequest(...)
```

Non-streaming has its own timeout: 120s for remote sessions, 300s otherwise (`claude.ts:807-811`). Max tokens are capped at `MAX_NON_STREAMING_TOKENS = 64_000` (`claude.ts:3354`).

The fallback can be disabled via `CLAUDE_CODE_DISABLE_NONSTREAMING_FALLBACK` env var or GrowthBook flag `tengu_disable_streaming_to_non_streaming_fallback`.

A special case handles 404 errors from gateways that don't support streaming endpoints (`claude.ts:2612-2749`).

### 4. Retry Logic and Error Handling

The `withRetry()` generator (`withRetry.ts:170-517`) wraps every API call:

**Configuration**:
- `DEFAULT_MAX_RETRIES = 10` (configurable via `CLAUDE_CODE_MAX_RETRIES`)
- `BASE_DELAY_MS = 500`
- `MAX_529_RETRIES = 3` (before model fallback)
- `FLOOR_OUTPUT_TOKENS = 3000` (minimum viable output after context overflow adjustment)

**Exponential backoff** (`withRetry.ts:530-548`):
```typescript
const baseDelay = Math.min(BASE_DELAY_MS * Math.pow(2, attempt - 1), maxDelayMs)
const jitter = Math.random() * 0.25 * baseDelay
return baseDelay + jitter
```
Default max: 32s. Persistent mode max: 5 minutes. Server `Retry-After` header bypasses the computed delay.

**Error classification** (`withRetry.ts:696-787`):
| Status | Action |
|--------|--------|
| 401 | Clear API key cache, retry (OAuth refresh in main loop) |
| 403 (token revoked) | Retry with token refresh |
| 408, 409 | Always retry |
| 429 | Retry for non-subscriber or enterprise; NOT for Max/Pro subscribers |
| 529 / overloaded | Retry foreground sources; drop background sources immediately |
| 5xx | Always retry (ants can ignore `x-should-retry: false` for 5xx) |
| ECONNRESET/EPIPE | Disable keep-alive, reconnect |

**529 cascade protection** (`withRetry.ts:62-88`): Background query sources (summaries, titles, suggestions, classifiers) bail immediately on 529 to avoid amplifying capacity cascades. Only foreground sources (`repl_main_thread`, `sdk`, `agent:*`, `compact`, etc.) retry.

**Model fallback** (`withRetry.ts:326-365`): After `MAX_529_RETRIES` consecutive 529s on Opus, throws `FallbackTriggeredError` which `query.ts` catches to switch to the fallback model (Sonnet).

**Fast mode fallback** (`withRetry.ts:267-314`): On 429/529 during fast mode:
- Short retry-after (<threshold): sleep and retry with fast mode still active (preserves prompt cache)
- Long retry-after: trigger cooldown (switches to standard speed), minimum 30-minute cooldown
- Overage rejection: permanently disable fast mode

**Persistent retry** (`withRetry.ts:96-104`): For unattended sessions (`CLAUDE_CODE_UNATTENDED_RETRY`), 429/529 retries indefinitely with up to 5-minute backoff and 6-hour reset cap. Periodic heartbeat yields every 30s to prevent idle detection.

**Context overflow recovery** (`withRetry.ts:388-427`): When the API returns "input length and `max_tokens` exceed context limit", the retry loop parses the error, computes available context with 1000-token safety buffer, and adjusts `maxTokensOverride` for the next attempt.

### 5. Token Counting

**API-based counting** (`tokenEstimation.ts:124-201`):
```typescript
// Uses the countTokens API endpoint
const response = await anthropic.beta.messages.countTokens({
  model: normalizeModelStringForAPI(model),
  messages: messages.length > 0 ? messages : [{ role: 'user', content: 'foo' }],
  tools,
  ...(filteredBetas.length > 0 && { betas: filteredBetas }),
  ...(containsThinking && { thinking: { type: 'enabled', budget_tokens: 1024 } }),
})
return response.input_tokens
```

**Haiku fallback** (`tokenEstimation.ts:251-325`): Uses Haiku (or Sonnet when Haiku is unavailable) to count tokens by making a `max_tokens: 1` request and reading `input_tokens + cache_creation + cache_read` from usage.

**Bedrock counting** (`tokenEstimation.ts:437-495`): Uses `CountTokensCommand` from `@aws-sdk/client-bedrock-runtime` with the request body serialized as JSON.

**Rough estimation** (`tokenEstimation.ts:203-242`):
```typescript
function roughTokenCountEstimation(content: string, bytesPerToken: number = 4): number {
  return Math.round(content.length / bytesPerToken)
}
```
JSON files use `bytesPerToken = 2` (denser single-character tokens). Images/PDFs estimate 2000 tokens. Tool use inputs are stringified for estimation.

### 6. Beta Headers Mechanism

Beta headers are assembled per-model in `utils/betas.ts` via `getAllModelBetas()` (memoized). The key headers:

```typescript
// constants/betas.ts
CLAUDE_CODE_20250219_BETA_HEADER = 'claude-code-20250219'        // Core CC behavior
INTERLEAVED_THINKING_BETA_HEADER = 'interleaved-thinking-2025-05-14'
CONTEXT_1M_BETA_HEADER = 'context-1m-2025-08-07'                // 1M context window
CONTEXT_MANAGEMENT_BETA_HEADER = 'context-management-2025-06-27' // API-side thinking preservation
STRUCTURED_OUTPUTS_BETA_HEADER = 'structured-outputs-2025-12-15'
WEB_SEARCH_BETA_HEADER = 'web-search-2025-03-05'
EFFORT_BETA_HEADER = 'effort-2025-11-24'
TASK_BUDGETS_BETA_HEADER = 'task-budgets-2026-03-13'
PROMPT_CACHING_SCOPE_BETA_HEADER = 'prompt-caching-scope-2026-01-05'
FAST_MODE_BETA_HEADER = 'fast-mode-2026-02-01'
REDACT_THINKING_BETA_HEADER = 'redact-thinking-2026-02-12'
TOKEN_EFFICIENT_TOOLS_BETA_HEADER = 'token-efficient-tools-2026-03-28'
AFK_MODE_BETA_HEADER = 'afk-mode-2026-01-31'                    // Auto/AFK mode
ADVISOR_BETA_HEADER = 'advisor-tool-2026-03-01'
```

**1M context activation** (`betas.ts:254-256`): The `context-1m-2025-08-07` header is added when `has1mContext(model)` returns true. This checks for `[1m]` suffix in the model string (e.g., `claude-sonnet-4-6[1m]`). The context window is set to 1,000,000 tokens when this beta is active.

Additionally, there is a Sonnet 1M experiment path (`claude.ts:1541-1547`) that dynamically appends the 1M beta if `getSonnet1mExpTreatmentEnabled(retryContext.model)` returns true.

**Beta header latching** (`claude.ts:1405-1456`): To prevent prompt cache breaks from mid-session toggles, several headers use sticky-on latches:
- `afkHeaderLatched`: Once auto mode activates, AFK header stays on
- `fastModeHeaderLatched`: Once fast mode activates, header stays on
- `cacheEditingHeaderLatched`: Once cache editing activates, header stays on
- `thinkingClearLatched`: Once cache TTL expires, thinking clear stays on

Latches are cleared on `/clear` and `/compact`.

**Provider-specific filtering**:
- Bedrock: Some betas go in `extraBodyParams.anthropic_beta` instead of headers (`BEDROCK_EXTRA_PARAMS_HEADERS` set)
- Vertex: `countTokens` only allows a subset of betas (`VERTEX_COUNT_TOKENS_ALLOWED_BETAS`)
- Foundry: Treated like firstParty for most betas

**User-specified betas**: `ANTHROPIC_BETAS` env var is split by commas and appended regardless of model.

### 7. Model Selection and Fallback

**Model resolution chain**:
1. `getRuntimeMainLoopModel()` selects model based on permission mode and token count
2. `normalizeModelStringForAPI()` converts user-facing names to API model IDs
3. Bedrock inference profiles are resolved to backing models (`getInferenceProfileBackingModel`)
4. `parseUserSpecifiedModel()` handles user-provided model strings

**Fallback mechanism** (`query.ts:650-708`): The query loop catches `FallbackTriggeredError`:
```typescript
while (attemptWithFallback) {
  attemptWithFallback = false
  try {
    // ... streaming call ...
  } catch (error) {
    if (error instanceof FallbackTriggeredError) {
      currentModel = error.fallbackModel
      attemptWithFallback = true
    }
  }
}
```

### 8. Cost Tracking

**Per-request cost** (`claude.ts:2251-2256`): Calculated at `message_delta` using `calculateUSDCost()`:
```typescript
const costUSDForPart = calculateUSDCost(resolvedModel, usage)
costUSD += addToTotalSessionCost(costUSDForPart, usage, options.model)
```

**Pricing tiers** (`modelCost.ts:36-69`):
| Tier | Input $/Mtok | Output $/Mtok | Models |
|------|-------------|--------------|--------|
| COST_HAIKU_35 | $0.80 | $4 | Haiku 3.5 |
| COST_HAIKU_45 | $1 | $5 | Haiku 4.5 |
| COST_TIER_3_15 | $3 | $15 | All Sonnet variants |
| COST_TIER_5_25 | $5 | $25 | Opus 4.5, Opus 4.6 (normal) |
| COST_TIER_15_75 | $15 | $75 | Opus 4, Opus 4.1 |
| COST_TIER_30_150 | $30 | $150 | Opus 4.6 (fast mode) |

Cache write costs are 1.25x input; cache read costs are 0.1x input. Web search: $0.01/request.

**Session persistence** (`cost-tracker.ts:87-175`): Costs are saved to project config (`lastCost`, `lastAPIDuration`, `lastModelUsage`) and restored when resuming the same session.

**Advisor cost tracking** (`cost-tracker.ts:304-321`): When the advisor tool is used, its token usage is extracted from the main response's usage and tracked separately with its own model's pricing.

### 9. Prompt Caching Mechanism

**Cache control blocks** (`claude.ts:358-374`):
```typescript
function getCacheControl({ scope, querySource }) {
  return {
    type: 'ephemeral',
    ...(should1hCacheTTL(querySource) && { ttl: '1h' }),
    ...(scope === 'global' && { scope }),
  }
}
```

**1-hour TTL** (`claude.ts:393-434`): Available for ant users or subscribers not in overage, gated by GrowthBook allowlist patterns. Eligibility is latched at session start to prevent mid-session cache busting from overage flips.

**Cache breakpoint placement** (`claude.ts:3063-3211`): Exactly one message-level `cache_control` marker per request, placed on the last message (or second-to-last for `skipCacheWrite` forks). This is a critical optimization:

> "Mycro's turn-to-turn eviction frees local-attention KV pages at any cached prefix position NOT in cache_store_int_token_boundaries. With two markers the second-to-last position is protected and its locals survive an extra turn even though nothing will ever resume from there"

**System prompt caching** (`claude.ts:3213-3237`): System prompt blocks get `cache_control` with optional global scope. Global scope is firstParty-only and disabled when MCP tools are present (they're per-user, breaking global cache).

**Cache editing (cached microcompact)** (`claude.ts:1188-1205`): An ant-only feature that uses `cache_edits` blocks with `delete` operations and `cache_reference` on `tool_result` blocks to surgically remove stale content from the server-side cache without busting the entire prefix.

**Prompt cache break detection** (`promptCacheBreakDetection.ts`): Tracks hashes of system prompt, tool schemas, model, betas, cache control, and other cache-key-affecting parameters. When `cache_read_input_tokens` drops unexpectedly, generates a diff to diagnose what changed.

### 10. The Conversation Loop (query.ts)

The `queryLoop()` function (`query.ts:241-`) is an infinite `while(true)` async generator:

**Each iteration**:
1. Destructure mutable `state` (messages, autoCompactTracking, etc.)
2. Apply tool result budgets (`applyToolResultBudget`)
3. Apply snip compaction, microcompact, context collapse
4. Build full system prompt with context
5. Run autocompact if needed
6. Check blocking limits (skip if reactive compact is enabled)
7. Call `deps.callModel()` (→ `queryModelWithStreaming`)
8. Process streamed messages:
   - AssistantMessages with tool_use blocks → execute tools
   - Stream events → yield to UI
   - Error messages → handle recovery
9. Execute tools (parallel or streaming via `StreamingToolExecutor`)
10. Collect tool results
11. Check stop conditions (no tool use, max turns, stop hooks)
12. If tools were used → continue loop with tool results appended

**Max output tokens recovery** (`query.ts:164`): Limited to 3 attempts (`MAX_OUTPUT_TOKENS_RECOVERY_LIMIT`). On `max_tokens` or `model_context_window_exceeded`, the loop can either escalate `max_tokens` to 64K or trigger reactive compact.

**Task budget tracking** (`query.ts:282-291`): `taskBudgetRemaining` decrements across compaction boundaries. While context is uncompacted, the server handles the countdown from `total`. After compact, `remaining` is sent to tell the server about the pre-compact usage that was summarized away.

### 11. Timeout Handling

**Client-level timeout** (`client.ts:144`):
```typescript
timeout: parseInt(process.env.API_TIMEOUT_MS || String(600 * 1000), 10)
```
Default: 600 seconds (10 minutes). Configurable via `API_TIMEOUT_MS`.

**Non-streaming fallback timeout** (`claude.ts:807-811`):
- Remote sessions: 120s (under CCR's ~5min container idle-kill)
- Local sessions: 300s

**Stream idle timeout** (`claude.ts:1877-1878`):
```typescript
const STREAM_IDLE_TIMEOUT_MS = parseInt(process.env.CLAUDE_STREAM_IDLE_TIMEOUT_MS || '', 10) || 90_000
```
Only active when `CLAUDE_ENABLE_STREAM_WATCHDOG` is set.

**SDK internal timeout**: The SDK itself can throw `APIUserAbortError` from its internal timeout. Claude Code distinguishes this from user aborts by checking `signal.aborted` (`claude.ts:2434-2461`).

### 12. Thinking Configuration

**Adaptive vs. budget** (`claude.ts:1601-1630`):
```typescript
if (modelSupportsAdaptiveThinking(options.model)) {
  thinking = { type: 'adaptive' }
} else {
  let thinkingBudget = getMaxThinkingTokensForModel(options.model)
  thinkingBudget = Math.min(maxOutputTokens - 1, thinkingBudget)
  thinking = { budget_tokens: thinkingBudget, type: 'enabled' }
}
```

Comment warns: "Do not change the adaptive-vs-budget thinking selection below without notifying the model launch DRI and research. This is a sensitive setting that can greatly affect model quality and bashing."

**Thinking is disabled** when `CLAUDE_CODE_DISABLE_THINKING` is set. Temperature is only sent when thinking is disabled (the API requires `temperature: 1` when thinking is enabled).

**Redacted thinking** (`betas.ts:270-277`): Interactive sessions (non-SDK) get `redact-thinking` beta to suppress thinking summaries in ctrl+o display. SDK/print-mode callers keep summaries.

### 13. Effort Configuration

**Effort levels** (`claude.ts:440-466`):
```typescript
function configureEffortParams(effortValue, outputConfig, extraBodyParams, betas, model) {
  if (typeof effortValue === 'string') {
    outputConfig.effort = effortValue  // 'low', 'medium', 'high'
    betas.push(EFFORT_BETA_HEADER)
  } else if (process.env.USER_TYPE === 'ant') {
    // Numeric effort override (ant-only, uses anthropic_internal)
    extraBodyParams.anthropic_internal = { effort_override: effortValue }
  }
}
```

### 14. Authentication Architecture

**Four provider paths** (`client.ts:88-316`):

1. **First-party API**: Uses `apiKey` or `authToken` (OAuth). Staging OAuth uses `getOauthConfig().BASE_API_URL`.

2. **AWS Bedrock**: `AnthropicBedrock` SDK with region-specific routing. Supports bearer token auth (`AWS_BEARER_TOKEN_BEDROCK`), cached AWS credentials, or skip-auth for testing.

3. **Azure Foundry**: `AnthropicFoundry` SDK with Azure AD token provider or API key auth. Falls back to `DefaultAzureCredential`.

4. **Google Vertex**: `AnthropicVertex` SDK with `GoogleAuth`. Careful project ID fallback to avoid 12-second metadata server timeouts.

**OAuth refresh**: On 401 or "token revoked" 403, the retry loop calls `handleOAuth401Error(failedAccessToken)` to force a token refresh before retrying.

## Configuration & Settings

### Environment Variables

| Variable | Default | Purpose |
|----------|---------|---------|
| `API_TIMEOUT_MS` | 600000 (10min) | Client-level API timeout |
| `CLAUDE_CODE_MAX_RETRIES` | 10 | Maximum retry attempts |
| `CLAUDE_CODE_EXTRA_BODY` | — | JSON object merged into API request body |
| `CLAUDE_CODE_EXTRA_METADATA` | — | JSON object merged into metadata.user_id |
| `CLAUDE_CODE_MAX_OUTPUT_TOKENS` | — | Override max output tokens |
| `CLAUDE_CODE_DISABLE_THINKING` | — | Disable thinking entirely |
| `CLAUDE_CODE_DISABLE_ADAPTIVE_THINKING` | — | Force budget-based thinking |
| `CLAUDE_CODE_DISABLE_1M_CONTEXT` | — | Disable 1M context (HIPAA compliance) |
| `CLAUDE_CODE_MAX_CONTEXT_TOKENS` | — | Override context window size (ant-only) |
| `DISABLE_PROMPT_CACHING` | — | Globally disable prompt caching |
| `DISABLE_PROMPT_CACHING_HAIKU` | — | Disable caching for Haiku |
| `DISABLE_PROMPT_CACHING_SONNET` | — | Disable caching for Sonnet |
| `DISABLE_PROMPT_CACHING_OPUS` | — | Disable caching for Opus |
| `DISABLE_INTERLEAVED_THINKING` | — | Disable interleaved thinking beta |
| `CLAUDE_CODE_DISABLE_EXPERIMENTAL_BETAS` | — | Strip first-party-only betas |
| `CLAUDE_CODE_UNATTENDED_RETRY` | — | Infinite retry on 429/529 (ant-only) |
| `CLAUDE_CODE_DISABLE_NONSTREAMING_FALLBACK` | — | Disable streaming→non-streaming fallback |
| `CLAUDE_ENABLE_STREAM_WATCHDOG` | — | Enable stream idle timeout |
| `CLAUDE_STREAM_IDLE_TIMEOUT_MS` | 90000 | Stream idle timeout (ms) |
| `ANTHROPIC_BETAS` | — | Comma-separated additional beta headers |
| `ANTHROPIC_CUSTOM_HEADERS` | — | Newline-separated custom headers |
| `ANTHROPIC_BASE_URL` | — | Override API base URL |
| `ANTHROPIC_MODEL` | — | Override main loop model |
| `ANTHROPIC_SMALL_FAST_MODEL` | — | Override small/fast model |
| `CLAUDE_CODE_ADDITIONAL_PROTECTION` | — | Send additional protection header |
| `ENABLE_PROMPT_CACHING_1H_BEDROCK` | — | Enable 1h cache TTL on Bedrock |
| `USE_API_CONTEXT_MANAGEMENT` | — | Enable API-side tool clearing (ant-only) |
| `USE_CONNECTOR_TEXT_SUMMARIZATION` | — | Tri-state: force on/off or defer to GB |
| `FALLBACK_FOR_ALL_PRIMARY_MODELS` | — | Enable 529 fallback for non-Opus models |

### GrowthBook Feature Flags

| Flag | Purpose |
|------|---------|
| `tengu-off-switch` | Kill switch for Opus queries (non-subscriber) |
| `tengu_disable_streaming_to_non_streaming_fallback` | Disable streaming fallback |
| `tengu_disable_keepalive_on_econnreset` | Disable keep-alive on stale connections |
| `tengu_prompt_cache_1h_config` | Allowlist for 1h cache TTL |
| `tengu_otk_slot_v1` | Enable max tokens capping (8K default) |
| `tengu_tool_pear` | Enable strict tool use |
| `tengu_amber_json_tools` | Enable token-efficient JSON tools |
| `tengu_slate_prism` | Enable connector text summarization |
| `tengu_auto_mode_config` | Auto mode model allowlist |
| `tengu_anti_distill_fake_tool_injection` | Anti-distillation fake tools |

## Exploitation Opportunities

1. **Prompt caching optimization**: The system places exactly one `cache_control` marker per request. Knowing this, we can structure conversations to maximize cache hits by keeping early messages stable.

2. **1M context activation**: Simply appending `[1m]` to a supported model name (e.g., `claude-sonnet-4-6[1m]`) activates the 1M context beta. The check is at `context.ts:39`: `/\[1m\]/i.test(model)`.

3. **Cost optimization**: Cache read tokens cost 0.1x input tokens. With 1h TTL (available on first-party for non-overage subscribers), long sessions can achieve 90%+ cache hit rates, cutting effective input cost by ~90%.

4. **Fast mode**: When `speed: 'fast'` is active on Opus 4.6, pricing triples ($30/$150 per Mtok). The cooldown mechanism is 30 minutes minimum, so rate-limit-triggered fallback to standard speed persists for a significant duration.

5. **Extra body params**: `CLAUDE_CODE_EXTRA_BODY` can inject arbitrary JSON into the API request body, enabling undocumented API features without code changes.

6. **Token budget tracking**: The `task_budget` parameter (`configureTaskBudgetParams`, beta `task-budgets-2026-03-13`) tells the model about remaining token budget so it can pace itself. This is distinct from the client-side auto-continue mechanism.

7. **Stream watchdog**: The idle timeout watchdog (`CLAUDE_ENABLE_STREAM_WATCHDOG`) is currently opt-in. For production deployments, enabling it prevents indefinite hangs on silently dropped connections.

8. **Non-streaming max tokens**: The 64K cap on non-streaming fallback (`MAX_NON_STREAMING_TOKENS`) means streaming failures on long outputs will produce truncated results.

## Edge Cases & Gotchas

1. **SDK text duplication bug**: The SDK sometimes returns text in `content_block_start` then again in `content_block_delta`. The code initializes text blocks with empty string to work around this (`claude.ts:2024-2028`).

2. **529 status code masking**: "The SDK sometimes fails to properly pass the 529 status code during streaming" — the code falls back to checking for `"type":"overloaded_error"` in the error message string (`withRetry.ts:619`).

3. **Usage accumulation**: The streaming API provides **cumulative** usage totals, not incremental deltas. `message_delta` events may send explicit 0 values for input fields, which the `updateUsage()` function guards against with `> 0` checks (`claude.ts:2924-2987`).

4. **Temperature constraint**: Temperature is only sent when thinking is disabled. When thinking is enabled, the API requires `temperature: 1` (the default), so sending it explicitly would be redundant but not harmful.

5. **Non-streaming thinking adjustment**: When falling back to non-streaming, thinking budget is capped to `max_tokens - 1` to maintain the API constraint (`claude.ts:3364-3392`).

6. **Cache break from beta header toggles**: Multiple beta headers are latched session-stable to prevent cache breaks. But if `ANTHROPIC_BETAS` env var is changed mid-session (via shell), the cache will break with no warning.

7. **Stream resource leaks**: The `releaseStreamResources()` function explicitly cancels the Response body and aborts the stream controller. This is called in `finally` blocks because native TLS/socket buffers live outside the V8 heap and can leak without explicit cleanup (GH #32920).

8. **Tombstone messages**: When streaming fallback occurs, partial assistant messages from the failed stream are yielded as tombstones to remove them from UI and transcript. This prevents invalid thinking signatures from causing "thinking blocks cannot be modified" API errors.

9. **Anti-distillation**: First-party CLI sends `anti_distillation: ['fake_tools']` in the request body (when enabled via GrowthBook), which causes the API to inject fake tool definitions to poison model distillation attempts.

10. **Gateway detection**: The logging layer fingerprints proxy gateways (LiteLLM, Helicone, Portkey, Cloudflare AI Gateway, Kong, Braintrust, Databricks) from response headers and base URL patterns (`logging.ts:56-139`).

## Cross-References

- **Report 01 (Compaction Engine)**: Autocompact is called from query.ts before each API call; microcompact runs before autocompact
- **Report 02 (System Prompt Assembly)**: System prompt blocks are built by `buildSystemPromptBlocks()` with cache control
- **Report 03 (Context Window)**: Context window sizes determined by `getContextWindowForModel()` feed into autocompact thresholds
- **Report 06 (Tool Pipeline)**: Tool schemas are assembled via `toolToAPISchema()` with deferred loading support
- **Report 21 (Feature Flags)**: GrowthBook gates control many API behaviors (fallback, caching, betas)
- **1M Context Deep Dive**: Detailed analysis of `context-1m-2025-08-07` beta activation paths
