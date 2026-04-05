---
description: "context window optimization — token budget, 1M context, conditional rules, frontmatter, gitignore, tool result limits, compaction instructions"
---

# Guide: Optimizing Your Context Window — Arcanum Wiki

> How to get the most out of your 200K or 1M context window, based on source code analysis.

## Understanding Your Context Budget

Claude Code's context window is split into these sections, each consuming tokens:

```
┌─────────────────────────────────────────────┐
│ System Prompt (static, cacheable)           │  ~5-15K tokens
│   - Core instructions                       │
│   - Tool descriptions (40+ tools)           │
│   - Skill descriptions                      │
│   - MCP server instructions                 │
├─────────────────────────────────────────────┤
│ CLAUDE.md content (as user message)         │  Variable (your content)
│ Rules files (.claude/rules/)                │
│ Memory files (MEMORY.md + up to 5 topics)   │
│ Git status (capped at 2K chars)             │
├─────────────────────────────────────────────┤
│ Conversation history                        │  Grows with each turn
│   - Your messages                           │
│   - Claude's responses                      │
│   - Tool calls and results                  │
└─────────────────────────────────────────────┘
```

The system prompt is the tax you pay every turn. The conversation history grows. When total hits ~83% of your context window, compaction kicks in.

## Key Numbers

| Metric | 200K Context | 1M Context |
|--------|-------------|------------|
| Auto-compact threshold | ~167K tokens | ~967K tokens |
| Effective working space | ~150K tokens | ~950K tokens |
| System prompt overhead | ~10-20K tokens | Same |
| Max tool result (single) | 50K chars / 100K tokens | Same |
| Max tool results (per turn) | 200K chars aggregate | Same |

## Optimization Strategies

### 1. Enable 1M Context (Free on Max Plan)

In your model setting, add `[1m]` suffix:
```
Model: claude-opus-4-6[1m]
```

This is client-side only — the suffix is stripped before the API call and a `context-1m-2025-08-07` beta header is injected instead. The API endpoint decides whether to honor it.

**Source**: `constants/betas.ts:6-7`, model parsing in `utils/model/model.ts`

### 2. Use Conditional Rules

Instead of loading ALL rules every turn, use `paths:` frontmatter to only load rules when relevant files are being touched:

```yaml
---
paths:
  - "src/**/*.cpp"
  - "src/**/*.h"
---
# C++ Coding Conventions
...
```

This rule only loads when C++ files are in the conversation. Saves ~1-3K tokens per irrelevant turn.

**Source**: `utils/skills/loadRules.ts` — rules with `paths:` are checked against files in the current context.

### 3. Write Good Memory Frontmatter

The memory file selector (Sonnet side-query) ONLY sees filenames and `description` frontmatter — it never reads the content to decide what to load:

```yaml
---
description: "DB schema reference — column names, table relationships, verified types for world/hotfixes/auth/characters/roleplay databases"
---
```

Make descriptions keyword-rich. If the selector can't tell what's in a file, it won't load it.

**Source**: `utils/memory/memorySelector.ts` — `description` field passed to side-query.

### 4. Keep .gitignore Clean

Git status is capped at 2K characters (`constants/common.ts`). If you have 200 untracked files, git status will be truncated and useless. A clean .gitignore means the 2K budget shows meaningful changes.

### 5. Minimize CLAUDE.md Size

CLAUDE.md is injected as a user message every turn. Every byte costs tokens. Use `.claude/rules/` for conditional content instead of putting everything in CLAUDE.md.

**Hard limit**: MEMORY.md is capped at 200 lines AND 25KB. Lines beyond 200 are silently truncated.

### 6. Manage Tool Results

When a tool result exceeds 50K characters, it's persisted to disk and the model gets a preview + file path. This is GOOD — it prevents a single `cat` of a large file from consuming your context.

But N parallel tools can each hit 50K, producing 200K aggregate in one turn. The per-message budget (`MAX_TOOL_RESULTS_PER_MESSAGE_CHARS = 200,000`) catches this — the largest results get persisted first.

**Tip**: Use `offset` and `limit` parameters on the Read tool. Reading 100 lines instead of 2000 saves ~1,900 lines of context.

### 7. Write Compaction Instructions

Add a `## Compaction Instructions` section to your CLAUDE.md. The compaction engine specifically looks for this section and preserves its instructions during summarization:

```markdown
## Compaction Instructions
When compacting, ALWAYS preserve:
1. Files modified this session
2. Current task/goal
3. Pending SQL or build actions
4. Spawned agents and their findings

Drop: exploration results, failed approaches, verbose tool output.
```

**Source**: The full LLM compaction prompt includes any `## Compaction Instructions` content from CLAUDE.md as guidance.

### 8. Use Tool Search (Deferred Tools)

Tool search reduces prompt size by not including all 40+ tool definitions upfront. Instead, tools are loaded on-demand when the model needs them. This is feature-gated via `tengu_` flags and beta headers.

If available, this saves ~5-10K tokens of tool definitions from the system prompt.

## When Compaction Happens

Compaction fires automatically in 4 tiers when tokens approach the threshold:

1. **API microcompact** — Server-side, cheapest. Clears server-side cache of old results.
2. **Client microcompact** — Clears old tool results from conversation history. No LLM call.
3. **Session memory compact** — Preserves 10K-40K tokens verbatim, summarizes the rest.
4. **Full LLM compact** — Complete conversation summarization. Most expensive.

**Key insight**: Only 5 memory files are restored after compaction, with a 50K total budget (5K per file). If you have critical context spread across 10 files, some will be lost.

## Anti-Patterns

| Anti-Pattern | Why It Wastes Context | Fix |
|-------------|----------------------|-----|
| Reading entire files | 2000 lines = ~8K tokens | Use offset/limit parameters |
| Not using .gitignore | Git status bloats to 2K cap | Maintain .gitignore |
| Everything in CLAUDE.md | Loaded every turn | Use conditional rules |
| Vague memory descriptions | Selector can't pick right files | Write keyword-rich frontmatter |
| Accumulating tool results | Never cleaned up | Let microcompact handle it, or use /compact |
