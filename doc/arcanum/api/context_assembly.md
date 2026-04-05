---
description: "context assembly — getSystemContext getUserContext, git status injection, CLAUDE.md loading, system prompt construction, context.ts"
---

# Context Assembly Pipeline -- Arcanum Wiki

## What Is This?

Context assembly is the process of building the complete payload sent to the API on each turn. This includes the system prompt (with all injected sections), the conversation messages, tool schemas, and metadata. The pipeline determines what gets included, in what order, and how token budgets are managed. Key files: `src/constants/prompts.ts`, `src/constants/systemPromptSections.ts`, `src/utils/systemPrompt.ts`, `src/context.ts`, and `src/services/api/claude.ts`.

## How It Works

### The Full Context Assembly Pipeline

On every API call, context is assembled in this order:

**1. System Prompt (first in the API payload)**

The system prompt is built from `SystemPromptSection` objects resolved by `resolveSystemPromptSections()`. Sections are either:
- **Cached** (`systemPromptSection()`) -- computed once, memoized until `/clear` or `/compact`
- **Volatile** (`DANGEROUS_uncachedSystemPromptSection()`) -- recomputed every turn, breaks prompt cache when value changes

The system prompt is structured as an array of text blocks:

```
Block 0: CLI system prompt prefix (getCLISyspromptPrefix)
Block 1: Identity + environment + date + OS + shell + git status
Block 2: Tool instructions (what each tool does)
Block 3: MCP server instructions (when MCP tools are present)
Block 4: Memory / CLAUDE.md content
Block 5: Notifications (system-reminder tags)
Block 6: Output style instructions
Block 7: Append system prompt (user's custom append)
```

Each block is a `TextBlockParam` with optional `cache_control`.

**2. Messages Array**

Messages are filtered and normalized:
- Only `user` and `assistant` messages pass through
- System messages, progress messages, and attachment messages are stripped
- Messages after the last compact boundary are used (earlier ones were summarized)
- Tool result pairing is enforced
- Media items are capped per request

**3. Tool Schemas**

Tools are serialized via `toolToAPISchema()`:
- Built-in tools (Bash, Read, Write, Edit, Glob, Grep, etc.)
- MCP tools from connected servers
- Agent tool (with dynamic agent listing)
- Skill tool (with dynamic command listing)
- Optional deferred tools (when tool search is enabled, only discovered tools are included)

**4. Cache Control Markers**

Applied to system prompt blocks and message breakpoints to maximize prompt cache hits.

### What Gets Included and In What Order

The `getSystemPrompt()` function in `prompts.ts` assembles sections in this order:

1. **Identity Section** -- "You are Claude Code, Anthropic's official CLI..." with environment info (OS, shell, cwd, git status, date)
2. **Tool Instructions** -- Per-tool descriptions and usage guidelines
3. **Agent/Skill Instructions** -- When AgentTool or SkillTool is available
4. **MCP Instructions** -- Server names, tool listings, resource access instructions
5. **Memory Content** -- CLAUDE.md files (project, user, enterprise) loaded via `loadMemoryPrompt()`
6. **Notifications** -- System reminders in `<system-reminder>` tags
7. **Output Style** -- Custom output formatting instructions
8. **Proactive Section** -- When proactive mode is active (feature-gated)
9. **Cyber Risk Instructions** -- Safety instructions (always included)

### How CLAUDE.md, Rules, Memory, and Git Status Are Injected

**CLAUDE.md injection:**
- Project-level: `CLAUDE.md` in the repo root
- User-level: `~/.claude/CLAUDE.md`
- Enterprise: `~/.claude/enterprise/CLAUDE.md`
- Rules directory: `.claude/rules/*.md` files
- All loaded by `loadMemoryPrompt()` from `src/memdir/memdir.ts`
- Content is wrapped in descriptive headers explaining source and authority

**Auto-memory (MEMORY.md):**
- Per-project memory at `~/.claude/projects/<path>/memory/MEMORY.md`
- Team memory (feature-gated) at `.claude/team-memory/MEMORY.md`
- Limited to 200 lines to keep the system prompt manageable
- Individual topic files are loaded on demand

**Git status:**
- Current branch, recent commits, working tree status
- Injected via `getIsGit()` check and `getSystemContext()`
- Placed in the environment section of the identity block

**Rules (.claude/rules/):**
- Each `.md` file becomes a separate system prompt section
- Loaded at session start, cached until `/clear`

### Token Budgets

Token budgets are managed at several levels:

**Model Context Window:**
- Each model has a max context window (200K for most, 1M with beta)
- `getModelMaxOutputTokens()` returns the output token cap per model
- `CAPPED_DEFAULT_MAX_TOKENS` limits default output tokens

**System Prompt Budget:**
- No explicit hard cap -- system prompt grows with tools and memory
- MCP tool descriptions contribute significant tokens
- Tool search (deferred loading) reduces upfront tool schema cost

**Message Budget:**
- Messages are the remainder after system prompt and tools
- Auto-compact triggers when context usage approaches the limit
- Session memory extraction keeps a rolling summary

**Compaction Budget:**
- `COMPACT_MAX_OUTPUT_TOKENS` limits the summary length
- `POST_COMPACT_TOKEN_BUDGET = 50,000` tokens for re-injected file content
- `POST_COMPACT_MAX_TOKENS_PER_FILE = 5,000` per individual file
- `POST_COMPACT_MAX_TOKENS_PER_SKILL = 5,000` per skill
- `POST_COMPACT_SKILLS_TOKEN_BUDGET = 25,000` total for skills

**Thinking Budget:**
- `getMaxThinkingTokensForModel()` returns per-model thinking budget
- Thinking tokens count against output, not input
- Adaptive thinking lets the model decide how much to think

**Task Budget (API-side):**
- `output_config.task_budget` tells the API the total and remaining tokens for a multi-turn task
- Requires `task-budgets-*` beta header

## Key Source Files

| File | Purpose |
|------|---------|
| `src/constants/prompts.ts` | System prompt section definitions and assembly |
| `src/constants/systemPromptSections.ts` | Section caching/volatility primitives |
| `src/utils/systemPrompt.ts` | Priority resolution (agent vs custom vs default) |
| `src/context.ts` | React context providers (UI state, not API context) |
| `src/utils/context.ts` | Max output tokens, thinking tokens, context limits |
| `src/utils/api.ts` | Cache control, system prompt splitting, tool schema conversion |
| `src/memdir/memdir.ts` | Memory file loading (CLAUDE.md, MEMORY.md) |
| `src/services/api/claude.ts` | `paramsFromContext()` -- final API payload assembly |

## Configuration

| Setting | Effect |
|---------|--------|
| `CLAUDE.md` | Project instructions injected into system prompt |
| `.claude/rules/*.md` | Rule files injected as system prompt sections |
| `~/.claude/CLAUDE.md` | User-level instructions |
| `--system-prompt` | Replaces default system prompt entirely |
| `--append-system-prompt` | Appends after all other sections |
| `ANTHROPIC_MODEL` | Changes model (affects context window size) |
| `DISABLE_PROMPT_CACHING` | Disables all cache_control markers |

## Interesting Findings

1. **System prompt sections use a generation-based cache** stored in `bootstrap/state.ts`. Clearing sections (`/clear`, `/compact`) also resets beta header latches so fresh evaluation happens.

2. **The volatile section system exists because some data must change per-turn** (like notifications), but each change breaks the prompt cache. The `DANGEROUS_` prefix is intentional -- it forces developers to document why cache-breaking is necessary.

3. **Tool schemas are part of the prompt cache key.** Adding or removing a single MCP tool busts the entire cache. This is why tool search (deferred loading) was implemented -- it reduces the upfront tool set.

4. **The `getCLISyspromptPrefix()` prefix** is a special first block that identifies the request as coming from Claude Code CLI. It is required for certain API-side behaviors and is always present regardless of custom system prompts.

5. **Memory file loading has a 200-line cap on MEMORY.md** to prevent runaway index files from consuming the context window. Individual topic files referenced from the index are loaded separately and can be larger.

6. **Skill content is truncated at 5000 tokens per skill** during post-compact re-injection, with a total budget of 25,000 tokens. This was added because some skills (verify, claude-api) are 18-20KB and were consuming 5-10K tokens per compact cycle.
