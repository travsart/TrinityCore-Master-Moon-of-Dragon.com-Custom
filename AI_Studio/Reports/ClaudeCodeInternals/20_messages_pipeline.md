# Message Assembly Pipeline -- Claude Code Internals Report

> v2.1.88 baseline + cli.js@2.1.97 grep refresh (2026-04-08)

## 2.1.97 Delta (summary)

| Change | Version | Type |
|---|---|---|
| **Nested CLAUDE.md dedup** uses `loadedNestedMemoryPaths` Set on the tool-use context (NOT the 100-entry LRU `readFileState`) to prevent re-injection. `readFileState` is an LRU that drops entries in busy sessions; without this Set, every eviction cycle would re-inject the same CLAUDE.md. The Set is cleared on `/clear` and on auto/partial compact. Already in v2.1.88 `src/utils/attachments.ts:1718-1731`, `src/screens/REPL.tsx:1964-1967`, `src/QueryEngine.ts:198`, and `src/commands/clear/conversation.ts:132`. cli.js@2.1.97 has 14 matches for `loadedNestedMemoryPaths`. Cross-ref Report 02. | fixed ≤ 2.1.88 | refines |
| **Read tool dedup on unchanged re-reads.** Already in v2.1.88 with extensive comments at `src/tools/FileReadTool/FileReadTool.ts:518-573`. Feature returns a stub instead of re-sending full content when `(path, offset, limit)` matches and mtime unchanged. Killswitch: `tengu_read_dedup_killswitch`. In 2.1.97 the stricter "Wasted call" message toggle added via `tengu_noreread_q7m_velvet`. Telemetry: `tengu_file_read_dedup`. Cross-ref Report 06. | 2.1.86 | refines |
| **Token overhead reduced for `@file` mentions.** File path mentions in user text parse into path references. Already in v2.1.88. | 2.1.86 | no-op |
| **`[Image #N]` chip on paste; trailing space fix.** `[Image #1]`, `[Image #2]` chips inserted at cursor on paste. 2.1.89 fix: no trailing space after the chip. Already in v2.1.88 source; the 2.1.89 change is a one-character fix. | 2.1.83 / 2.1.89 | no-op |
| **Session transcript size reduction — skip empty hook entries.** NEW in 2.1.97. When a hook fires but emits no stdout/stderr and takes no action, the transcript writer now skips it entirely instead of recording an empty entry. Previously every hook fire wrote a `{}`-shaped transcript row. Aggregate effect: long sessions with many `PreCompact`/`PostCompact`/`PermissionRequest` hooks shrink meaningfully. Changelog only — the fix is a conditional `continue` buried in the transcript serializer. | 2.1.97 | gap |
| **`message_delta` handler now updates ALL yielded messages in the turn** (fix). Before 2.1.97, only the last message (`newMessages.at(-1)`) received the `usage`/`stop_reason` update from the final `message_delta` event. If an API turn yielded multiple assistant messages (e.g., compact-then-continue inside one turn), only the last one showed real token usage and stop reason; earlier ones had stale/missing fields. cli.js@2.1.97 now loops: `for(let g1 of R6) g1.message.usage=Z6, g1.message.stop_reason=F6;`. Transcript accuracy fix. | 2.1.97 | gap |

---

## Overview

The message assembly pipeline is the central nervous system of Claude Code. Every time the model is called, the pipeline assembles a complete conversation payload from five distinct layers: a multi-block system prompt, a user context preamble (CLAUDE.md files and date), a system context suffix (git status), the normalized conversation history (messages, tool results, attachments), and per-turn attachment injections (memory, diagnostics, skill discovery, plan mode, etc.). These layers are stitched together in `src/query.ts` (the query loop), `src/services/api/claude.ts` (the API request builder), and `src/utils/api.ts` (context helpers), with message normalization happening in `src/utils/messages.ts`.

The pipeline implements a sophisticated prompt caching strategy that splits the system prompt into static (cross-org cacheable) and dynamic (session-specific) partitions using a boundary marker. User context (CLAUDE.md content) is injected as a synthetic user message at position 0 wrapped in `<system-reminder>` tags, NOT in the system prompt, to preserve cache stability. Tool results exceeding 50K characters are persisted to disk and replaced with a preview + file path reference. Multiple compaction and truncation strategies (auto-compact, snip, microcompact, context collapse, reactive compact) progressively reduce context when approaching limits.

The attachment system is the primary mechanism for injecting dynamic per-turn context. On every tool-use iteration, `getAttachmentMessages()` evaluates ~30 attachment providers concurrently, producing typed attachment objects that are later converted to `UserMessage` objects (mostly wrapped in `<system-reminder>` tags) by `normalizeAttachmentForAPI()` in `messages.ts`. This is how memory files, plan mode instructions, diagnostics, queued commands, skill suggestions, and team coordination messages enter the conversation.

## Architecture

### Key Files

| File | Role |
|------|------|
| `src/query.ts` | Main query loop -- orchestrates the full turn lifecycle |
| `src/services/api/claude.ts` | Builds and sends the API request (system prompt blocks, tool schemas, message normalization) |
| `src/utils/api.ts` | `prependUserContext()`, `appendSystemContext()`, `splitSysPromptPrefix()`, `toolToAPISchema()` |
| `src/context.ts` | `getSystemContext()` (git status), `getUserContext()` (CLAUDE.md files) |
| `src/constants/prompts.ts` | `getSystemPrompt()` -- builds the full system prompt array |
| `src/constants/systemPromptSections.ts` | Section registry with memoization and cache-break control |
| `src/utils/messages.ts` | `normalizeMessagesForAPI()`, `normalizeAttachmentForAPI()`, `wrapInSystemReminder()`, `getMessagesAfterCompactBoundary()` |
| `src/utils/attachments.ts` | `getAttachments()`, `getAttachmentMessages()`, all attachment providers |
| `src/utils/claudemd.ts` | CLAUDE.md file discovery, loading, merging, and formatting |
| `src/memdir/memdir.ts` | Auto-memory (MEMORY.md) prompt building |
| `src/utils/toolResultStorage.ts` | Large tool result persistence to disk |
| `src/constants/toolLimits.ts` | Size limits for tool results |
| `src/utils/messages/mappers.ts` | SDK-to-internal and internal-to-SDK message conversions |
| `src/utils/messages/systemInit.ts` | `buildSystemInitMessage()` for SDK stream metadata |

### Data Flow

```
User Input
    |
    v
processUserInput() --> creates UserMessage + extracts @mentions, images
    |
    v
query() loop entry
    |
    +--> getMessagesAfterCompactBoundary(messages) -- slice to post-compact window
    |
    +--> applyToolResultBudget() -- persist oversized per-message tool results
    |
    +--> snipCompactIfNeeded() -- history snip (feature-gated)
    |
    +--> microcompact() -- remove stale thinking blocks, etc.
    |
    +--> contextCollapse.applyCollapsesIfNeeded() -- (feature-gated)
    |
    +--> appendSystemContext(systemPrompt, systemContext) -- git status appended
    |
    +--> autocompact() -- full conversation summary if over threshold
    |
    v
queryModel() in claude.ts
    |
    +--> normalizeMessagesForAPI(messages, tools) -- filter/merge/repair
    |
    +--> prependUserContext(messagesForAPI, userContext) -- CLAUDE.md as msg[0]
    |
    +--> buildSystemPromptBlocks(systemPrompt) -- split into cache partitions
    |
    +--> Build API params: system, messages, tools, thinking, betas
    |
    v
API Request (streaming)
    |
    v
Stream response --> assistant messages + tool_use blocks
    |
    v
Tool execution (parallel via StreamingToolExecutor)
    |
    +--> processToolResultBlock() -- persist large results to disk
    |
    v
getAttachmentMessages() -- inject per-turn attachments
    |
    v
Loop continues (or terminal)
```

## Key Implementation Details

### 1. System Prompt Construction (`src/constants/prompts.ts`)

The system prompt is built by `getSystemPrompt()` (line 444) as an array of strings. It has two zones separated by a boundary marker:

**Static zone** (globally cacheable):
1. `getSimpleIntroSection()` -- identity, cyber risk instruction
2. `getSimpleSystemSection()` -- tool permissions, system-reminder explanation, hooks
3. `getSimpleDoingTasksSection()` -- coding style, task execution, comment policy
4. `getActionsSection()` -- reversibility, blast radius, confirmation rules
5. `getUsingYourToolsSection()` -- tool preference guidance (Read over cat, etc.)
6. `getSimpleToneAndStyleSection()` -- no emojis, file:line references
7. `getOutputEfficiencySection()` -- conciseness rules, prose style

**Boundary marker** (line 573):
```typescript
...(shouldUseGlobalCacheScope() ? [SYSTEM_PROMPT_DYNAMIC_BOUNDARY] : []),
```

**Dynamic zone** (session-specific, registry-managed):
- `session_guidance` -- agent tool, skill, fork, verification instructions
- `memory` -- auto-memory prompt from `loadMemoryPrompt()`
- `ant_model_override` -- internal-only model overrides
- `env_info_simple` -- working directory, platform, shell, OS, model name, knowledge cutoff
- `language` -- user's language preference
- `output_style` -- output style configuration
- `mcp_instructions` -- MCP server instructions (DANGEROUS: recomputes every turn)
- `scratchpad` -- scratchpad directory instructions
- `frc` -- function result clearing section
- `summarize_tool_results` -- tool result summarization guidance
- Conditional: `numeric_length_anchors`, `token_budget`, `brief`

Dynamic sections use `systemPromptSection()` which memoizes after first compute, or `DANGEROUS_uncachedSystemPromptSection()` which recomputes every turn (cache-breaking).

### 2. System Prompt Blocks and Cache Partitioning (`src/utils/api.ts`)

`splitSysPromptPrefix()` (line 321) transforms the flat system prompt array into `SystemPromptBlock[]` with cache scope annotations. Three strategies:

1. **MCP tools present**: Attribution header (no cache) -> prefix (org cache) -> rest (org cache). No global cache because MCP tools are per-user.

2. **Global cache with boundary** (1P only): Attribution (no cache) -> prefix (no cache) -> static content before boundary (global cache) -> dynamic content after boundary (no cache).

3. **Default (3P/boundary missing)**: Attribution (no cache) -> prefix (org cache) -> rest (org cache).

`buildSystemPromptBlocks()` in `claude.ts:3213` converts these into `TextBlockParam[]` with `cache_control` annotations.

### 3. User Context Injection -- The `<system-reminder>` Pattern (`src/utils/api.ts`)

CLAUDE.md content does NOT go in the system prompt. It goes in a synthetic user message prepended to position 0:

```typescript
// src/utils/api.ts:462
export function prependUserContext(messages, context) {
  return [
    createUserMessage({
      content: `<system-reminder>\nAs you answer the user's questions, you can use the following context:\n${
        Object.entries(context)
          .map(([key, value]) => `# ${key}\n${value}`)
          .join('\n')
      }
      IMPORTANT: this context may or may not be relevant to your tasks. You should not respond to this context unless it is highly relevant to your task.\n</system-reminder>\n`,
      isMeta: true,
    }),
    ...messages,
  ]
}
```

This is called in the streaming loop at `query.ts:660`:
```typescript
deps.callModel({
  messages: prependUserContext(messagesForQuery, userContext),
  systemPrompt: fullSystemPrompt,
  ...
})
```

The `userContext` object comes from `getUserContext()` in `context.ts:155` which returns:
- `claudeMd` -- the merged CLAUDE.md content (if not disabled)
- `currentDate` -- today's date string

### 4. System Context Injection (`src/context.ts`)

`getSystemContext()` (line 116) is memoized for the session. It returns:
- `gitStatus` -- current branch, main branch, git user, short status (max 2000 chars), recent 5 commits
- `cacheBreaker` -- internal debugging injection (ant-only)

This is appended to the system prompt via `appendSystemContext()` at `query.ts:449`:
```typescript
const fullSystemPrompt = asSystemPrompt(
  appendSystemContext(systemPrompt, systemContext),
)
```

So git status ends up as the LAST block in the system prompt, formatted as `gitStatus: This is the git status at the start...`.

### 5. CLAUDE.md File Loading Priority (`src/utils/claudemd.ts`)

Files are loaded in this order (lowest to highest priority):
1. **Managed** (`/etc/claude-code/CLAUDE.md`) -- global admin instructions
2. **User** (`~/.claude/CLAUDE.md`) -- user's global instructions
3. **Project** (`CLAUDE.md`, `.claude/CLAUDE.md`, `.claude/rules/*.md` in project roots) -- checked into repo
4. **Local** (`CLAUDE.local.md`) -- private project-specific, not checked in
5. **AutoMem** (`~/.claude/projects/<slug>/memory/MEMORY.md`) -- auto-memory entrypoint

Discovery traverses from CWD upward to root. Files closer to CWD load later (higher priority). The `@include` directive allows files to reference other files.

`getClaudeMds()` (line 1153) formats all files with provenance labels:
```
Contents of /path/to/CLAUDE.md (project instructions, checked into the codebase):
<content>
```

Each file type gets a description suffix:
- Project: "(project instructions, checked into the codebase)"
- Local: "(user's private project instructions, not checked in)"
- User: "(user's private global instructions for all projects)"
- AutoMem: "(user's auto-memory, persists across conversations)"
- TeamMem: "(shared team memory, synced across the organization)"

All entries are prefixed with the instruction prompt:
```
Codebase and user instructions are shown below. Be sure to adhere to these instructions. IMPORTANT: These instructions OVERRIDE any default behavior and you MUST follow them exactly as written.
```

### 6. Memory File Injection (`src/memdir/memdir.ts`)

Auto-memory (`MEMORY.md`) is loaded via `loadMemoryPrompt()` as a system prompt section (memoized once per session). It provides behavioral instructions for the memory system including the four-type taxonomy (user / feedback / project / reference).

The MEMORY.md entrypoint is truncated to:
- **MAX_ENTRYPOINT_LINES**: 200 lines
- **MAX_ENTRYPOINT_BYTES**: 25,000 bytes

Relevant memories are also surfaced dynamically via `startRelevantMemoryPrefetch()` in `attachments.ts:2361`. This runs a side-query at the start of each user turn to find topic files relevant to the user's prompt, then injects them as attachment messages. These are filtered to avoid duplicates with files already read via FileRead/Write/Edit.

### 7. Message Normalization for API (`src/utils/messages.ts:1989`)

`normalizeMessagesForAPI()` is the critical transform that converts the internal message array into API-compatible format:

1. **Reorder attachments** -- bubbles attachment messages up until they hit a tool result or assistant message
2. **Filter virtual messages** -- removes display-only messages (REPL inner tool calls)
3. **Strip error-triggered media** -- removes PDF/image blocks that previously caused errors
4. **Merge consecutive user messages** -- Bedrock compatibility (1P API merges them server-side)
5. **Strip tool_reference blocks** -- removes when tool search is disabled
6. **Inject turn boundary text** -- prevents capybara models from sampling stop sequences
7. **Convert attachment messages** -- calls `normalizeAttachmentForAPI()` per attachment type
8. **Merge system messages** -- converts `local_command` system messages to user messages
9. **Handle empty content** -- replaces with `(no content)` placeholder
10. **Normalize tool inputs** -- strips internal fields via `normalizeToolInputForAPI()`
11. **Smoosh system-reminders** -- merges adjacent `<system-reminder>` text blocks into tool_result content

### 8. Attachment System (`src/utils/attachments.ts`)

`getAttachments()` (line 743) evaluates ~30 attachment providers concurrently across three categories:

**User input attachments** (processed first):
- `at_mentioned_files` -- @-mentioned files
- `mcp_resources` -- MCP resource attachments
- `agent_mentions` -- agent invocation mentions
- `skill_discovery` -- turn-0 skill discovery

**Thread-safe attachments** (all threads):
- `queued_commands` -- mid-turn message queue drain
- `date_change` -- date crossing detection
- `ultrathink_effort` -- effort level hint
- `deferred_tools_delta` -- new/removed deferred tool announcements
- `agent_listing_delta` -- new/removed agent type announcements
- `mcp_instructions_delta` -- new/removed MCP server instructions
- `changed_files` -- files changed since last turn
- `nested_memory` -- conditional memory rules triggered by file operations
- `dynamic_skill` -- skill directories discovered during file operations
- `skill_listing` -- available skills listing
- `plan_mode` / `plan_mode_exit` -- plan mode instructions
- `auto_mode` / `auto_mode_exit` -- auto mode instructions
- `todo_reminders` / `task_reminder` -- task management nudges
- `teammate_mailbox` / `team_context` -- agent swarm coordination
- `agent_pending_messages` -- pending agent messages
- `critical_system_reminder` -- critical system reminders
- `compaction_reminder` -- compaction nudge
- `context_efficiency` -- context usage hint

**Main thread only:**
- `ide_selection` / `ide_opened_file` -- IDE integration
- `output_style` -- output style reminders
- `diagnostics` / `lsp_diagnostics` -- diagnostic issues
- `unified_tasks` -- unified task status
- `token_usage` / `budget_usd` / `output_token_usage` -- token/budget tracking
- `verify_plan_reminder` -- plan verification nudge

Each attachment type has a dedicated handler in `normalizeAttachmentForAPI()` (line 3453) that converts it to one or more `UserMessage` objects, most wrapped in `<system-reminder>` tags via `wrapMessagesInSystemReminder()`.

### 9. Tool Result Formatting and Persistence (`src/utils/toolResultStorage.ts`)

Tool results flow through `processToolResultBlock()` which:
1. Calls the tool's `mapToolResultToToolResultBlockParam()` to get the API format
2. Checks if the result exceeds the persistence threshold
3. If too large: writes to `~/.claude/projects/<slug>/<sessionId>/tool-results/<toolUseId>.txt`
4. Returns a `<persisted-output>` reference with preview

Key constants from `src/constants/toolLimits.ts`:
- `DEFAULT_MAX_RESULT_SIZE_CHARS`: 50,000 characters (per-tool cap)
- `MAX_TOOL_RESULT_TOKENS`: 100,000 tokens (~400KB)
- `MAX_TOOL_RESULTS_PER_MESSAGE_CHARS`: 200,000 characters (per-message aggregate cap)
- `PREVIEW_SIZE_BYTES`: 2,000 bytes (preview in reference message)

The persisted output format:
```
<persisted-output>
Output too large (59.2KB). Full output saved to: /path/to/tool-results/toolu_xxx.txt

Preview (first 2KB):
[first 2KB of content]
...
</persisted-output>
```

Empty tool results get a placeholder `(toolName completed with no output)` to prevent capybara models from sampling the stop sequence on bare `</function_results>\n\n` patterns.

### 10. The Query Loop (`src/query.ts`)

The `queryLoop()` generator (line 241) is the main orchestration loop. Key phases per iteration:

1. **Pre-processing**: `getMessagesAfterCompactBoundary()` slices to post-compact window
2. **Tool result budget**: `applyToolResultBudget()` persists oversized per-message results
3. **Snip compact**: removes old history segments (feature-gated: `HISTORY_SNIP`)
4. **Microcompact**: removes stale thinking blocks, cached microcompact edits
5. **Context collapse**: collapses old tool result regions (feature-gated: `CONTEXT_COLLAPSE`)
6. **System prompt assembly**: `appendSystemContext(systemPrompt, systemContext)` adds git status
7. **Auto-compact**: full conversation summary if over token threshold
8. **Blocking limit check**: prevents API call if at hard blocking limit
9. **API call**: `callModel()` with `prependUserContext()` for CLAUDE.md injection
10. **Streaming**: process stream events, yield assistant messages
11. **Tool execution**: via `StreamingToolExecutor` or sequential `runTools()`
12. **Attachment injection**: `getAttachmentMessages()` + memory prefetch + skill discovery
13. **Continue or terminal**: based on tool_use presence, max turns, budget, etc.

### 11. Message Compaction Strategies

The pipeline uses five compaction strategies in order of aggressiveness:

1. **Snip compact** (`HISTORY_SNIP`): Removes oldest message segments, replacing with a snip boundary marker. Preserves recent context.

2. **Microcompact**: Strips stale thinking blocks from old assistant messages. Also handles cached microcompact (cache editing) which manipulates the API's cache state.

3. **Context collapse** (`CONTEXT_COLLAPSE`): Collapses tool result regions into summary messages. A read-time projection over the full history -- collapses persist across turns.

4. **Auto-compact**: Full conversation summarization when token count exceeds threshold. Uses a separate API call (typically Haiku) to generate a summary. Produces a compact boundary marker that subsequent `getMessagesAfterCompactBoundary()` calls use to slice.

5. **Reactive compact**: Triggered by actual API 413 errors. Drains staged collapses first, then falls back to full auto-compact. Only fires on real errors, not preempted ones.

### 12. Image and File Handling

Images in messages are handled through the standard `ContentBlockParam` types:
- `image` blocks: base64-encoded image data with media type
- `document` blocks: PDF content (multi-page support)

The pipeline applies `stripExcessMediaItems()` in `claude.ts:1312` to enforce the API's 100-media-item limit per request, silently dropping the oldest items.

PDF files referenced via @-mention are checked for size -- files exceeding `PDF_AT_MENTION_INLINE_THRESHOLD` pages get a lightweight reference attachment instead of inline content.

## Configuration & Settings

### Environment Variables
| Variable | Effect |
|----------|--------|
| `CLAUDE_CODE_DISABLE_CLAUDE_MDS` | Hard disable all CLAUDE.md loading |
| `CLAUDE_CODE_DISABLE_ATTACHMENTS` | Disable all per-turn attachments |
| `CLAUDE_CODE_SIMPLE` | Minimal system prompt, no attachments |
| `CLAUDE_CODE_DISABLE_THINKING` | Disable extended thinking |
| `CLAUDE_CODE_DISABLE_ADAPTIVE_THINKING` | Force budget-based thinking |
| `CLAUDE_CODE_DISABLE_EXPERIMENTAL_BETAS` | Strip experimental API fields (defer_loading, strict, eager_input_streaming) |
| `CLAUDE_CODE_ENABLE_FINE_GRAINED_TOOL_STREAMING` | Force FGTS regardless of feature flag |
| `CLAUDE_CODE_REMOTE` | Skip git status (CCR overhead), shorter API timeouts |
| `API_TIMEOUT_MS` | Override non-streaming fallback timeout |

### Feature Flags (GrowthBook)
| Flag | Controls |
|------|----------|
| `tengu_moth_copse` | Skip AutoMem/TeamMem from user context; enable relevant memory prefetch |
| `tengu_paper_halyard` | Skip Project/Local CLAUDE.md from user context |
| `tengu_satin_quoll` | Per-tool persistence threshold overrides (JSON map) |
| `tengu_hawthorn_window` | Per-message aggregate tool result budget override |
| `tengu_toolref_defer_j8m` | Relocate tool_reference turn boundary siblings |
| `tengu_tool_pear` | Enable strict mode for tool schemas |
| `tengu_fgts` | Enable fine-grained tool streaming |
| `tengu_hive_evidence` | Enable verification agent |
| `HISTORY_SNIP` | Enable snip compact |
| `CONTEXT_COLLAPSE` | Enable context collapse |
| `REACTIVE_COMPACT` | Enable reactive compact |
| `CACHED_MICROCOMPACT` | Enable cache editing microcompact |
| `EXPERIMENTAL_SKILL_SEARCH` | Enable skill search and discovery |
| `TOKEN_BUDGET` | Enable +Nk token budget feature |

### Key Constants
| Constant | Value | Location |
|----------|-------|----------|
| `DEFAULT_MAX_RESULT_SIZE_CHARS` | 50,000 | `src/constants/toolLimits.ts` |
| `MAX_TOOL_RESULT_TOKENS` | 100,000 | `src/constants/toolLimits.ts` |
| `MAX_TOOL_RESULTS_PER_MESSAGE_CHARS` | 200,000 | `src/constants/toolLimits.ts` |
| `PREVIEW_SIZE_BYTES` | 2,000 | `src/utils/toolResultStorage.ts` |
| `MAX_STATUS_CHARS` | 2,000 | `src/context.ts` |
| `MAX_ENTRYPOINT_LINES` | 200 | `src/memdir/memdir.ts` |
| `MAX_ENTRYPOINT_BYTES` | 25,000 | `src/memdir/memdir.ts` |
| `MAX_MEMORY_CHARACTER_COUNT` | 40,000 | `src/utils/claudemd.ts` |
| `MAX_OUTPUT_TOKENS_RECOVERY_LIMIT` | 3 | `src/query.ts` |
| `API_MAX_MEDIA_PER_REQUEST` | 100 | `src/services/api/claude.ts` |
| `MAX_NON_STREAMING_TOKENS` | 64,000 | `src/services/api/claude.ts` |

## Exploitation Opportunities

### 1. CLAUDE.md Priority Exploitation
Since files closer to CWD load later and have higher priority, and the instruction says they "OVERRIDE any default behavior", placing critical rules in the project-level `CLAUDE.md` (not user-level) maximizes their weight. The `.claude/rules/*.md` files are the highest-priority project-level instructions.

### 2. System-Reminder Injection Points
Everything wrapped in `<system-reminder>` is treated as system context by the model. Since attachments produce these, you can influence the model's behavior by:
- Structuring CLAUDE.md rules to appear authoritative (the prefix says "IMPORTANT: These instructions OVERRIDE any default behavior")
- Using the `/wrap-up` skill to inject end-of-session reminders
- Queuing commands via the message queue (appears as `queued_commands` attachment)

### 3. Tool Result Persistence Path
Large tool results are saved to `~/.claude/projects/<slug>/<sessionId>/tool-results/`. The model sees a `<persisted-output>` tag with the file path. This means:
- You can reference persisted output by reading the file path shown in the preview
- The 50K character threshold is per-tool, configurable via `tengu_satin_quoll`
- FileReadTool has `maxResultSizeChars: Infinity` -- it NEVER persists, relying on its own line/byte limits

### 4. Memory Surfacing Control
The `tengu_moth_copse` flag controls relevant memory prefetch. When enabled, AutoMem/TeamMem files are removed from the user context preamble (position 0) and instead surfaced dynamically via side-query. This means memory files might NOT appear if the side-query doesn't match them to the current prompt.

### 5. Attachment Timing
Attachments are injected AFTER tool results on every iteration. This means the model sees fresh diagnostic data, skill suggestions, and plan mode instructions between each tool call, not just at the start of the conversation.

### 6. Git Status Is a Snapshot
`getGitStatus()` is memoized for the entire session. The git status shown to the model is captured once at conversation start and never updates. The model is explicitly told: "Note that this status is a snapshot in time, and will not update during the conversation."

## Edge Cases & Gotchas

### 1. Empty Tool Results Cause Stop Sequence Sampling
Empty tool_result content at the prompt tail causes capybara models to emit the `\n\nHuman:` stop sequence. The pipeline injects `(toolName completed with no output)` to prevent this (see `toolResultStorage.ts:287`).

### 2. Attachment Reordering
`reorderAttachmentsForAPI()` bubbles attachment messages upward until they hit a tool result or assistant message. This means the order you see in the REPL is NOT the order the API receives.

### 3. Consecutive User Message Merging
Multiple user messages in a row are merged into a single user turn for Bedrock compatibility. This is transparent -- the 1P API already merges them server-side.

### 4. Thinking Block Lifetime Rules
The "rules of thinking" (documented in `query.ts:152` with wizard humor):
1. A message with thinking/redacted_thinking must be in a query with `max_thinking_length > 0`
2. A thinking block may not be the last message in a block
3. Thinking blocks must be preserved for the assistant trajectory duration

Violation causes "an entire day of debugging and hair pulling."

### 5. Tool Search Dynamic Loading
When tool search is enabled, deferred tools are NOT included in the API request until they've been discovered via `tool_reference` blocks in message history. This means the model's tool set can grow mid-conversation.

### 6. System Prompt Injection (Internal Only)
`setSystemPromptInjection()` in `context.ts` allows injecting arbitrary text into the system context for cache breaking. This is ant-only and gated behind `BREAK_CACHE_COMMAND`.

### 7. Cache Stability Latches
Several API headers (AFK mode, fast mode, cache editing, thinking clear) use sticky-on latches. Once first sent, they keep being sent for the rest of the session. This prevents mid-session GrowthBook flips from changing the server-side cache key and busting 50-70K tokens of cached content. Latches are cleared on `/clear` and `/compact`.

### 8. MEMORY.md Truncation
The MEMORY.md entrypoint is hard-capped at 200 lines AND 25,000 bytes. Content beyond these limits is truncated with a warning. Long index entries (observed: 197KB under 200 lines) are caught by the byte cap.

## Cross-References

- **Report 01**: Compaction Engine -- details auto-compact, snip, microcompact, reactive compact
- **Report 02**: System Prompt Assembly -- deeper dive into static/dynamic partition, cache warming
- **Report 03**: Context Window -- token counting, budget tracking, escalation
- **Report 04**: Memory Pipeline -- MEMORY.md, topic files, memory surfacing
- **Report 06**: Tool Pipeline -- tool execution, result mapping, streaming tool executor
- **Report 09**: Hooks System -- hooks that run on tool calls, instructions loaded hooks
- **Report 11**: Skills System -- skill discovery, skill search prefetch
- **Report 12**: MCP Client -- MCP tool integration, instructions injection
- **Report 21**: Feature Flags -- GrowthBook flags controlling pipeline behavior
