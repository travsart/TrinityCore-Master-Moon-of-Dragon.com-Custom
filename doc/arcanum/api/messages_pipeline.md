---
description: "messages pipeline — conversation message assembly, system-reminder tags, message transformation, utils/messages/, history accumulation"
---

# Messages Pipeline -- Arcanum Wiki

## What Is This?

The messages pipeline is the machinery that transforms the internal conversation state into the exact shape the Anthropic Messages API expects. This includes normalizing messages, injecting the system prompt, formatting tool results, managing prompt cache breakpoints, handling images/PDFs, and pruning context to fit the window. The relevant code spans `src/utils/messages.ts`, `src/utils/messages/`, `src/services/api/claude.ts`, and `src/utils/api.ts`.

## How It Works

### Conversation Assembly for Each API Call

Every API call in `queryModel()` (claude.ts) follows this assembly pipeline:

1. **Filter messages** -- Only `user` and `assistant` type messages are sent to the API. System messages (progress, compact boundaries, informational) are excluded via `normalizeMessagesForAPI()`.

2. **Strip media excess** -- `stripExcessMediaItems()` enforces `API_MAX_MEDIA_PER_REQUEST` by removing oldest images/documents first.

3. **Normalize for API** -- `normalizeMessagesForAPI()` performs:
   - Filters to only user/assistant messages
   - Strips thinking and redacted_thinking blocks from assistant messages
   - Strips connector_text blocks (feature-gated)
   - Ensures proper user/assistant alternation
   - Handles tool result pairing via `ensureToolResultPairing()`
   - Strips advisor blocks from history

4. **Add cache breakpoints** -- `addCacheBreakpoints()` attaches `cache_control` markers:
   - On the last user message
   - On the second-to-last user message (when > 2 user messages)
   - On each assistant message adjacent to a cached user message
   - Cached microcompact can also inject `cache_edits` blocks

5. **Convert to API format** -- Each message is mapped:
   - User messages: `userMessageToMessageParam()` -- content preserved as-is or wrapped in text blocks for cache control
   - Assistant messages: `assistantMessageToMessageParam()` -- content preserved, thinking/redacted_thinking blocks excluded from cache markers

### System Prompt Injection

The system prompt is assembled in `src/constants/prompts.ts` as an array of `SystemPromptSection` objects:

```
[identity] + [environment] + [tool instructions] + [tool schemas] +
[MCP instructions] + [memory/CLAUDE.md] + [git status] + [notifications] +
[output style] + [custom append prompt]
```

Each section is created via `systemPromptSection()` (cached after first compute) or `DANGEROUS_uncachedSystemPromptSection()` (recomputed every turn, breaks cache).

The system prompt array is converted to `TextBlockParam[]` blocks with cache control markers via `splitSysPromptPrefix()` in `utils/api.ts`. The last block gets the cache control marker, and if global scope is enabled, it gets `scope: 'global'`.

Priority system for effective system prompt (`buildEffectiveSystemPrompt()`):
1. Override system prompt (loop mode) -- replaces everything
2. Coordinator system prompt (coordinator mode)
3. Agent system prompt (when `mainThreadAgentDefinition` is set) -- replaces default
4. Custom system prompt (`--system-prompt` flag) -- replaces default
5. Default system prompt (standard Claude Code prompt)
6. Append system prompt is always added at the end (except override)

### Tool Result Formatting

Tool results are sent as `tool_result` content blocks within user messages:

```typescript
{
  type: 'tool_result',
  tool_use_id: '<matching tool_use block id>',
  content: '<string or content block array>',
  is_error: boolean
}
```

`ensureToolResultPairing()` ensures every `tool_use` block in an assistant message has a corresponding `tool_result` in the following user message. Orphaned tool_use blocks (from interrupted turns) get synthetic error results.

Tool results can contain nested media (images, documents) within their content arrays. These are subject to the same media stripping limits.

### Message Truncation and Context Pruning

Context management operates at multiple levels:

**Pre-call pruning:**
- `stripExcessMediaItems()` -- enforces per-request media limits
- Images are validated and resized before inclusion via `validateImagesForAPI()`
- PDFs have size limits (`PDF_TARGET_RAW_SIZE`, `API_PDF_MAX_PAGES`)

**Post-overflow handling:**
- If the API returns a 400 "input length and max_tokens exceed context limit", `parseMaxTokensContextOverflowError()` extracts the numbers and adjusts `max_tokens` down (minimum 3000 tokens)
- If `stop_reason` is `model_context_window_exceeded`, the extended context window beta handles it

**Compaction (the nuclear option):**
- When context exceeds thresholds, the compact service rewrites the conversation into a summary
- Messages before the compact boundary are replaced with the summary
- Post-compact, key files and skill content are re-injected

**Cached Microcompact (incremental):**
- Uses `cache_edits` to delete message ranges from the server-side cache without a full compact
- Controlled by `consumePendingCacheEdits()` / `pinCacheEdits()`

### Image Handling

Images flow through several stages:
1. **Attachment parsing** -- images from user input, file reads, or tool results
2. **Validation** (`imageValidation.ts`) -- size checks against `ImageSizeError` thresholds
3. **Resizing** (`imageResizer.ts`) -- large images are resized down, with `ImageResizeError` if they exceed limits after resize
4. **API formatting** -- converted to `BetaImageBlockParam` with base64 content and media type
5. **Stripping** -- oldest images removed first when count exceeds `API_MAX_MEDIA_PER_REQUEST`

### SDK Message Mapping (`messages/mappers.ts`)

For SDK consumers, messages are mapped between internal and SDK formats:
- `toInternalMessages()` -- SDK messages to internal `Message[]`
- `toSDKMessages()` -- internal messages to `SDKMessage[]`
- Handles compact boundary messages with metadata translation
- Local command output is converted to synthetic assistant messages for mobile app compatibility
- ANSI codes are stripped from command output

### System Init Message (`messages/systemInit.ts`)

The first message on the SDK stream is a `system/init` message containing:
- Current working directory
- Available tool names
- MCP server status
- Model name
- Permission mode
- Slash commands
- API key source
- Beta headers
- Claude Code version
- Output style
- Agent types
- Skills
- Plugins
- Fast mode state

## Key Source Files

| File | Purpose |
|------|---------|
| `src/services/api/claude.ts` | Core query assembly -- `queryModel()`, `paramsFromContext()` |
| `src/utils/messages.ts` | Message normalization, tool result pairing, creation helpers |
| `src/utils/messages/mappers.ts` | SDK-to-internal and internal-to-SDK message mapping |
| `src/utils/messages/systemInit.ts` | SDK system/init message builder |
| `src/utils/api.ts` | System prompt splitting, tool schema conversion, cache control |
| `src/constants/prompts.ts` | System prompt section assembly |
| `src/constants/systemPromptSections.ts` | Memoized/volatile section primitives |
| `src/utils/systemPrompt.ts` | Effective system prompt priority resolution |
| `src/utils/imageValidation.ts` | Image size validation |
| `src/utils/imageResizer.ts` | Image resize logic |

## Configuration

| Setting | Effect |
|---------|--------|
| `API_MAX_MEDIA_PER_REQUEST` | Max images + documents per API call |
| `API_PDF_MAX_PAGES` | Max pages for PDF documents |
| `PDF_TARGET_RAW_SIZE` | Target raw size for PDF content |
| `DISABLE_PROMPT_CACHING` | Turns off all cache_control markers |
| `--system-prompt` flag | Replaces default system prompt |
| `--append-system-prompt` flag | Appends to system prompt |

## Interesting Findings

1. **Message alternation is strictly enforced** -- the API requires user/assistant/user/assistant alternation. `normalizeMessagesForAPI()` handles edge cases like consecutive same-role messages by merging or inserting synthetic messages.

2. **Thinking blocks are never cached** -- `assistantMessageToMessageParam()` explicitly skips cache_control on `thinking`, `redacted_thinking`, and `connector_text` blocks.

3. **Tool result content is cloned before modification** -- `userMessageToMessageParam()` clones array content to prevent `splice` mutations from `insertCacheEditsBlock` from contaminating the original message across multiple calls.

4. **The system prompt is split at specific points** for cache control via `splitSysPromptPrefix()`. The CLI system prompt prefix (`getCLISyspromptPrefix()`) is always the first block and gets its own cache control marker.

5. **Prompt suggestions deliberately avoid overriding any API parameter** to preserve prompt cache sharing with the main thread. Even setting `effortValue` caused a 45x spike in cache writes (PR #18143).

6. **MEMORY.md has a 200-line truncation limit** -- lines after 200 are dropped from the system prompt to keep the index concise.
