---
description: "API layer overview — Anthropic SDK streaming, services/api/claude.ts, retry logic, rate limits, authentication, model selection, token counting"
---

# API Layer Overview -- Arcanum Wiki

## What Is This?

The API layer in Claude Code is the boundary between the client application and the Anthropic Messages API (and its provider variants: AWS Bedrock, Google Vertex AI, Azure Foundry). It handles client construction, request signing, streaming, retry logic, error classification, prompt caching, and telemetry. The code lives in `src/services/api/`.

## How It Works

### Client Construction (`client.ts`)

Every API call begins by constructing an `Anthropic` SDK client via `getAnthropicClient()`. The function dynamically selects the provider based on environment variables:

| Env Var | Provider | SDK Class |
|---------|----------|-----------|
| `CLAUDE_CODE_USE_BEDROCK` | AWS Bedrock | `AnthropicBedrock` |
| `CLAUDE_CODE_USE_FOUNDRY` | Azure Foundry | `AnthropicFoundry` |
| `CLAUDE_CODE_USE_VERTEX` | Google Vertex AI | `AnthropicVertex` |
| (none) | First-party Anthropic | `Anthropic` |

Default headers injected on every request:
- `x-app: cli` -- identifies the caller
- `User-Agent` -- user agent string
- `X-Claude-Code-Session-Id` -- session UUID
- Custom headers from `ANTHROPIC_CUSTOM_HEADERS` env var (newline-separated `Name: Value` pairs)
- `x-anthropic-additional-protection` -- opt-in safety header
- Container and remote session headers when running in CCR

Authentication varies by provider:
- **First-party**: OAuth token (`authToken`) for Claude AI subscribers, API key for console users
- **Bedrock**: AWS STS credentials, optionally `AWS_BEARER_TOKEN_BEDROCK`
- **Vertex**: Google ADC via `google-auth-library`, with fallback project ID
- **Foundry**: Azure AD token or `ANTHROPIC_FOUNDRY_API_KEY`

A custom `fetch` wrapper is injected that adds a `x-client-request-id` UUID header to every request (first-party only), enabling timeout correlation with server logs.

### The Core Query Path (`claude.ts`)

The main entry point is `queryModel()`, an async generator that:

1. **Resolves the model** -- handles Bedrock inference profiles
2. **Assembles beta headers** via `getMergedBetas()` -- model-specific + feature-gated betas
3. **Configures tool search** -- filters deferred tools, adds `tool-search` beta
4. **Builds request parameters** via `paramsFromContext()`:
   - System prompt with cache control markers
   - Messages normalized for the API (`normalizeMessagesForAPI`)
   - Tool schemas converted via `toolToAPISchema`
   - Thinking config (adaptive, enabled, or disabled)
   - Output config (max tokens, effort, task budget)
   - Prompt caching with cache breakpoints at the last 2-4 user/assistant turns
5. **Makes the streaming API call** via the SDK's `beta.messages.stream()`
6. **Processes the SSE stream** -- yields `StreamEvent` chunks and builds the final `AssistantMessage`
7. **Falls back to non-streaming** on stream-level errors (empty stream, connection errors)
8. **Logs success/error metrics** via `logAPISuccessAndDuration` / `logAPIError`

### Streaming Implementation

Claude Code uses the SDK's built-in streaming support (`beta.messages.stream()`). The stream handler:

- Tracks time-to-first-token (TTFT)
- Accumulates content blocks (text, thinking, tool_use, connector_text)
- Handles `content_block_delta` events to yield incremental `StreamEvent` objects
- Detects stop reasons: `end_turn`, `tool_use`, `max_tokens`, `model_context_window_exceeded`
- On stream failure, falls back to `executeNonStreamingRequest()` with the same parameters

The non-streaming fallback uses a bounded timeout (`getNonstreamingFallbackTimeoutMs`): 120s for remote sessions (to stay under CCR's container idle-kill), 300s otherwise.

### Retry Logic (`withRetry.ts`)

The retry system is an async generator (`withRetry`) that yields `SystemAPIErrorMessage` during waits so the UI stays responsive.

Key parameters:
- `DEFAULT_MAX_RETRIES = 10` (overridable via `CLAUDE_CODE_MAX_RETRIES`)
- `BASE_DELAY_MS = 500` with exponential backoff (2^attempt * 500ms, capped at 32s)
- Respects `retry-after` header from the server

Retry decision matrix:
- **529 (overloaded)**: Retries up to `MAX_529_RETRIES = 3` for foreground sources, then triggers model fallback. Background sources (`prompt_suggestion`, `session_memory`, etc.) bail immediately to avoid amplifying capacity cascades.
- **429 (rate limit)**: Retries for API key users and enterprise subscribers. Max/Pro Claude AI subscribers do NOT retry (their retry-after is hours).
- **401**: Clears API key cache, refreshes OAuth tokens, retries
- **403 "token revoked"**: Refreshes OAuth token, retries
- **408/409**: Retries (timeout/lock conflicts)
- **5xx**: Retries
- **Connection errors (ECONNRESET/EPIPE)**: Disables HTTP keep-alive and retries

**Persistent retry mode** (`CLAUDE_CODE_UNATTENDED_RETRY`): For unattended CI sessions, retries 429/529 indefinitely with up to 5-minute backoff and 30-second heartbeat yields.

**Fast mode handling**: On 429/529 during fast mode, short retry-after (<20s) preserves fast mode; longer delays trigger a 10-minute minimum cooldown that falls back to standard speed.

**Model fallback**: After 3 consecutive 529 errors, if `fallbackModel` is configured, throws `FallbackTriggeredError` to switch models.

### Beta Headers

Beta headers are assembled in `getMergedBetas()` and injected as the `betas` parameter on every API call. They enable preview features:

| Header | Purpose |
|--------|---------|
| `context-1m-*` | Extended context window |
| `prompt-caching-*` | Prompt caching |
| `prompt-caching-scope-*` | Global/org-scoped caching |
| `fast-mode-*` | Fast mode (priority routing) |
| `afk-mode-*` | Auto-mode (latched sticky-on) |
| `effort-*` | Reasoning effort control |
| `structured-outputs-*` | JSON schema output |
| `task-budgets-*` | API-side token budgets |
| `redact-thinking-*` | Redacted thinking blocks |
| `context-management-*` | Cached microcompact |
| `advisor-*` | Server-side advisor tool |
| `tool-search-*` | Deferred tool loading |

Several betas use "sticky-on latching" -- once activated in a session, they stay active even if conditions change, to avoid busting the prompt cache.

### Prompt Caching

Prompt caching is implemented by attaching `cache_control` markers to system prompt blocks and the last few user/assistant messages. Key details:

- **TTL**: 5 minutes by default; 1 hour for eligible users (Ant employees, subscribers not using overage, 3P Bedrock with opt-in)
- **Scope**: `global` scope for first-party users via `shouldUseGlobalCacheScope()`
- **Cache breakpoint strategy**: System prompt blocks get cache markers; the last 2-4 message turns get breakpoints
- **Prompt cache break detection** (`promptCacheBreakDetection.ts`): A sophisticated 2-phase system tracks what changed (system prompt, tools, model, betas, effort, etc.) and correlates with actual cache read token drops to diagnose breaks

### Token Counting

Token counting happens at multiple layers:
- **Pre-request**: `tokenCountWithEstimation()` estimates context size using heuristics (character count / 4)
- **Post-response**: Actual token counts from API response `usage` object
- **Cost tracking**: `calculateUSDCost()` computes dollar cost per request

### Error Handling (`errors.ts`, `errorUtils.ts`)

Errors are classified into user-facing categories:
- Rate limits with specific messages for Max/Pro/Enterprise/API key users
- Image/PDF size errors with actionable guidance
- Prompt-too-long errors with token gap information
- SSL/TLS errors with proxy troubleshooting hints
- Connection errors with code extraction from cause chains
- HTML error page sanitization (CloudFlare pages stripped to title)

The `formatAPIError()` function walks the error cause chain to extract connection codes, detects SSL errors from a 30+ code set, and returns user-friendly messages.

### Request Logging and Analytics

Every API call logs:
- `tengu_api_query` -- pre-request (model, message count, betas, permission mode)
- `tengu_api_success` -- post-response (tokens, cost, TTFT, cache stats, gateway detection)
- `tengu_api_error` -- on failure (error type, status, request ID)
- `tengu_api_retry` -- on each retry attempt
- `tengu_prompt_cache_break` -- when cache breaks are detected

Gateway detection identifies proxy/gateway middleware (LiteLLM, Helicone, Portkey, CloudFlare AI Gateway, Kong, Braintrust, Databricks) from response headers.

## Key Source Files

| File | Purpose |
|------|---------|
| `client.ts` | Anthropic SDK client construction for all 4 providers |
| `claude.ts` | Core query pipeline -- streaming, non-streaming, parameter assembly |
| `withRetry.ts` | Retry logic with backoff, fallback, fast mode handling |
| `errors.ts` | Error classification and user-facing messages |
| `errorUtils.ts` | Connection error parsing, SSL detection, HTML sanitization |
| `logging.ts` | Telemetry -- success/error/query analytics events |
| `bootstrap.ts` | Bootstrap API call for client_data and model options |
| `usage.ts` | Rate limit utilization fetch (`/api/oauth/usage`) |
| `promptCacheBreakDetection.ts` | 2-phase prompt cache break detection and diagnosis |
| `dumpPrompts.ts` | Debug prompt dumping (Ant-only) via custom fetch wrapper |
| `sessionIngress.ts` | Remote session log persistence with optimistic concurrency |
| `filesApi.ts` | Files API for upload/download of session attachments |
| `adminRequests.ts` | Org admin request creation (limit increase, seat upgrade) |
| `grove.ts` | Grove privacy settings and notice management |
| `firstTokenDate.ts` | User's first Claude Code token date tracking |

## Configuration

| Variable | Purpose |
|----------|---------|
| `ANTHROPIC_API_KEY` | API key for direct access |
| `ANTHROPIC_BASE_URL` | Custom API base URL |
| `ANTHROPIC_MODEL` | Override default model |
| `ANTHROPIC_SMALL_FAST_MODEL` | Override small/fast model (Haiku) |
| `API_TIMEOUT_MS` | Request timeout (default 600s) |
| `CLAUDE_CODE_MAX_RETRIES` | Max retry attempts (default 10) |
| `DISABLE_PROMPT_CACHING` | Disable all prompt caching |
| `CLAUDE_CODE_USE_BEDROCK` | Route through AWS Bedrock |
| `CLAUDE_CODE_USE_VERTEX` | Route through Google Vertex AI |
| `CLAUDE_CODE_USE_FOUNDRY` | Route through Azure Foundry |
| `CLAUDE_CODE_EXTRA_BODY` | Extra JSON body params for API calls |
| `CLAUDE_CODE_UNATTENDED_RETRY` | Enable persistent retry for CI |
| `CLAUDE_CODE_ADDITIONAL_PROTECTION` | Enable additional safety protection header |
| `ANTHROPIC_CUSTOM_HEADERS` | Custom headers (newline-separated) |

## Interesting Findings

1. **The SDK client is recreated on auth errors**, not cached. The comment notes "we have always been lying about the return type" for Bedrock/Vertex/Foundry clients, casting them to the base Anthropic type.

2. **Anti-distillation**: First-party CLI builds can inject `fake_tools` via `anti_distillation` in the request body, controlled by a GrowthBook flag.

3. **Non-streaming fallback max tokens are capped at 16384** (`MAX_NON_STREAMING_TOKENS`) regardless of the model's actual limit, because non-streaming is the emergency path.

4. **The 529 error detection is fragile**: The SDK sometimes fails to pass the 529 status code during streaming, so the code also checks for `"type":"overloaded_error"` in the error message string.

5. **Cache break detection has a minimum token drop threshold of 2000 tokens** -- small variations are ignored to avoid false positives.

6. **Gateway detection is purely header-based** for most gateways, but Databricks uses hostname suffix matching since they use provider-owned domains.
