---
description: "API request lifecycle — streaming, beta headers, attribution cch token, retry logic, provider routing Bedrock Vertex, token counting, prompt caching"
---

# Guide: API Internals — What Happens When Claude Responds — Arcanum Wiki

> The complete request lifecycle from your message to Claude's response, including everything Claude Code does behind the scenes.

## The Request Lifecycle

```
You type a message and press Enter
  │
  ├── 1. UserPromptSubmit hooks fire
  │     (can inject additionalContext, block, modify)
  │
  ├── 2. Memory selector runs (Sonnet side-query)
  │     → Picks up to 5 topic files based on your message
  │
  ├── 3. System prompt assembled
  │     → Static sections (cacheable)
  │     → Dynamic boundary
  │     → Per-session sections (env, MCP, skills)
  │
  ├── 4. Context assembled
  │     → CLAUDE.md injected as user message
  │     → Rules files loaded (conditional check)
  │     → Memory files injected
  │     → Git status appended
  │     → Conversation history
  │
  ├── 5. Token budget check
  │     → If over threshold: trigger compaction, THEN send
  │
  ├── 6. API request constructed
  │     → Model selection (opus/sonnet/haiku + [1m] suffix)
  │     → Beta headers assembled (15+ possible headers)
  │     → Attribution header (version, fingerprint, entrypoint, attestation)
  │     → Prompt caching scope (global vs session)
  │     → Streaming mode enabled
  │
  ├── 7. Request sent to Anthropic API
  │     → First-party (api.anthropic.com)
  │     → Or Bedrock / Vertex (if configured)
  │
  ├── 8. Streaming response received
  │     → Text blocks rendered to terminal in real-time
  │     → Thinking blocks tracked (interleaved thinking)
  │     → Tool use blocks accumulated
  │
  ├── 9. Response complete
  │     → If tool calls: execute each tool (see below)
  │     → If text only: turn complete
  │     → Cost tracking updated
  │
  └── 10. If tool calls were executed:
        → Results assembled as user message
        → Go back to step 3 (loop continues)
```

## Beta Headers — What Gets Sent

Every API request includes a set of beta headers that enable features:

```
anthropic-beta: claude-code-20250219,
                interleaved-thinking-2025-05-14,
                context-1m-2025-08-07,
                web-search-2025-03-05,
                effort-2025-11-24,
                token-efficient-tools-2026-03-28,
                fast-mode-2026-02-01,
                ...
```

Not all headers are always sent. Some are conditional:
- `context-1m-*` only if model has `[1m]` suffix
- `fast-mode-*` only if `/fast` is active
- `afk-mode-*` only if TRANSCRIPT_CLASSIFIER feature enabled
- `cli-internal-*` only for Anthropic employees

**Source**: `constants/betas.ts`

## The Attribution Header

Every request includes a billing header:

```
x-anthropic-billing-header: cc_version=0.2.57.abc123;
                            cc_entrypoint=cli;
                            cch=00000;
                            cc_workload=interactive;
```

| Field | Purpose |
|-------|---------|
| `cc_version` | Claude Code version + fingerprint hash |
| `cc_entrypoint` | How Claude Code was started (cli/sdk/server) |
| `cch` | Client attestation token (Bun overwrites the zeros) |
| `cc_workload` | Routing hint (interactive vs cron) |

The `cch` field is fascinating: Claude Code writes `cch=00000` as a placeholder, then Bun's native HTTP stack (written in Zig) finds this exact byte sequence in the serialized request body and overwrites it with a computed hash. The server verifies this to confirm the request came from a real Claude Code binary.

The `cc_workload` field routes cron-initiated requests to a lower QoS API pool so they don't compete with interactive users.

**Source**: `constants/system.ts:73-95`

## Prompt Caching

Claude Code exploits Anthropic's prompt caching aggressively:

### Global Scope Caching
Everything BEFORE the `__SYSTEM_PROMPT_DYNAMIC_BOUNDARY__` marker is identical across all users. This means:
- The core system prompt
- Tool descriptions
- Code style instructions
- Git commit templates

These are all marked with `scope: 'global'` for prompt caching. When millions of users send requests, the server can serve this prefix from cache.

### Session Scope Caching
Content after the dynamic boundary is per-session but stable across turns within a session:
- Environment info
- MCP server instructions
- Memory directory path

These use session-level caching — cheaper across turns in the same conversation.

### Fork Agent Caching
When you spawn subagents in fork mode, the parent's full conversation prefix is shared. This means the subagent's request starts with identical bytes, triggering a massive cache hit.

## Streaming Implementation

Responses stream in real-time:

1. **Text blocks** — rendered character-by-character to the terminal
2. **Thinking blocks** — accumulated (shown in special thinking UI or hidden)
3. **Tool use blocks** — accumulated until the complete tool call is received
4. **Stop reason** — `end_turn` (done), `tool_use` (needs to call tools), `max_tokens` (hit limit)

### Interleaved Thinking

With the `interleaved-thinking-2025-05-14` beta, Claude can think between tool calls:

```
[thinking] I should check the database schema first...
[tool_use] Read("schema.sql")
[thinking] The column is called "faction" not "FactionID"...
[tool_use] Edit("query.sql", ...)
[text] I've updated the query to use the correct column name.
```

### Redacted Thinking

With `redact-thinking-2026-02-12`, thinking blocks can be hidden from the user while still being used by the model. This is useful for enterprise deployments where you don't want to expose the model's reasoning.

## Token Counting

Claude Code has NO local tokenizer. All token counting is done via the API:

```typescript
// From the source — every count goes through the API
const tokenCount = await api.countTokens(messages)
```

This means:
- Token counts are always accurate (no estimation drift)
- But counting requires an API call (latency)
- Counts are cached where possible to reduce calls

## Retry Logic

Failed API requests are retried with exponential backoff:

| Error Type | Retry? | Strategy |
|-----------|--------|----------|
| 429 (Rate limited) | Yes | Exponential backoff with jitter |
| 500 (Server error) | Yes | Up to 3 retries |
| 529 (Overloaded) | Yes | Longer backoff |
| Network error | Yes | Up to 3 retries |
| 400 (Bad request) | No | Fail immediately |
| 401 (Unauthorized) | No | Fail immediately |

## Cost Tracking

Claude Code tracks costs in real-time:

```
Input tokens × input price = input cost
Output tokens × output price = output cost
Cache read tokens × cache price = cache cost (cheaper)
Total = input + output + cache
```

View with `/cost` or `/stats`.

## Provider Support

Claude Code supports 3 API providers:

| Provider | Config | Notes |
|----------|--------|-------|
| **Anthropic (1P)** | Default, or `ANTHROPIC_API_KEY` | Full feature support |
| **AWS Bedrock** | `CLAUDE_CODE_USE_BEDROCK=true` | Limited beta headers (sent via extraBodyParams) |
| **Google Vertex** | `CLAUDE_CODE_USE_VERTEX=true` | Limited countTokens betas |

Bedrock and Vertex have restrictions on which beta headers they support. The source carefully manages this:
- Bedrock: Some betas go in body params, not headers
- Vertex: Only 3 betas allowed on countTokens API

## Cross-References

- [API Layer Overview](../api/api_overview.md) — full technical architecture
- [Messages Pipeline](../api/messages_pipeline.md) — message assembly details
- [Context Assembly](../api/context_assembly.md) — what gets included
- [System Prompt Anatomy](system_prompt_anatomy.md) — how the prompt is built
- [Beta Headers](../core/glossary.md#beta-headers) — complete catalog
