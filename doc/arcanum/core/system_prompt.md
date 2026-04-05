---
description: "system prompt assembly — 3-stream pipeline, 21 dynamic sections, override priority, SYSTEM_PROMPT_DYNAMIC_BOUNDARY, global cache scope, cyber risk instruction"
---

# System Prompt Assembly Pipeline -- Arcanum Wiki

## Overview

Claude Code's system prompt is assembled from three parallel data streams that converge at query time: the static/dynamic system prompt sections, user context (CLAUDE.md + memory files), and system context (git status + date). These three streams are injected into different locations of the API request -- the system prompt sections go into the `system` parameter, CLAUDE.md goes into the first user message wrapped in `<system-reminder>` tags, and git status is appended to the end of the system prompt.

This architecture has a critical implication: CLAUDE.md content is NOT in the system prompt. It is presented as user-role context, which means it does not benefit from system-level prompt caching and competes with conversation messages for context space.

## How It Works

### The Three Injection Points

**1. `system` parameter** -- Contains the core prompt instructions, dynamic per-session sections, and system context (git status). Assembled by `getSystemPrompt()` in `src/constants/prompts.ts`.

**2. First user message** -- Contains CLAUDE.md content and the current date, wrapped in `<system-reminder>` tags. Prepended by `prependUserContext()` in `src/utils/api.ts`.

**3. `appendSystemPrompt`** -- An optional SDK-provided suffix added to the system prompt array. Used by integrations that need to inject additional instructions.

The orchestration function is `fetchSystemPromptParts()` in `src/utils/queryContext.ts`, which fetches all three components in parallel.

### Static Sections (Globally Cacheable)

These seven sections are hard-coded strings that are identical across all users and sessions, making them eligible for global prompt caching (scope: `global`):

| Order | Section | Content |
|-------|---------|---------|
| 1 | `getSimpleIntroSection()` | Identity: "You are an interactive agent..." + URL warning |
| 2 | `getSimpleSystemSection()` | System rules: output formatting, tool permissions, hooks |
| 3 | `getSimpleDoingTasksSection()` | Task execution guidelines, code style, security |
| 4 | `getActionsSection()` | Reversibility, blast radius, confirmation rules |
| 5 | `getUsingYourToolsSection()` | Tool usage rules, parallel calls, task management |
| 6 | `getSimpleToneAndStyleSection()` | No emojis, concise, file:line references |
| 7 | `getOutputEfficiencySection()` | Output conciseness (different for ant vs external) |

### Dynamic Boundary

After the static sections, a `__SYSTEM_PROMPT_DYNAMIC_BOUNDARY__` marker separates globally-cacheable content from per-session content. This is defined at `src/constants/prompts.ts:114`.

### Dynamic Sections (Per-Session)

Dynamic sections are managed via `systemPromptSection()` and cached once per session (unless explicitly uncached):

| Order | Section Name | Content |
|-------|-------------|---------|
| 9 | `session_guidance` | Agent tool usage, skill commands, verification |
| 10 | `memory` | Auto-memory (MEMORY.md) mechanics instructions |
| 11 | `ant_model_override` | Internal model suffix (ant-only) |
| 12 | `env_info_simple` | CWD, platform, shell, OS, model ID, knowledge cutoff |
| 13 | `language` | Language preference if set |
| 14 | `output_style` | Custom output style if configured |
| 15 | `mcp_instructions` | MCP server instructions (**UNCACHED** -- recomputes every turn) |
| 16 | `scratchpad` | Scratchpad directory instructions |
| 17 | `frc` | Function result clearing notice |
| 18 | `summarize_tool_results` | Note about important tool results |
| 19 | `numeric_length_anchors` | Length limits (ant-only) |
| 20 | `token_budget` | Token budget instructions (feature-gated) |
| 21 | `brief` | Brief section (Kairos feature) |

**The MCP instructions section** is the ONLY `DANGEROUS_uncachedSystemPromptSection` in the standard prompt. It recomputes every turn because MCP servers can connect/disconnect between turns. When the value changes, the entire prompt cache is invalidated.

### System Prompt Override Priority

From `buildEffectiveSystemPrompt()` in `src/utils/systemPrompt.ts`:

```
Priority (highest wins):
0. overrideSystemPrompt (loop mode) -- REPLACES everything
1. Coordinator system prompt (coordinator mode)
2. Agent system prompt (if mainThreadAgentDefinition set)
   - Proactive mode: agent prompt APPENDED to default
   - Normal mode: agent prompt REPLACES default
3. customSystemPrompt (--system-prompt flag)
4. defaultSystemPrompt (the standard Claude Code prompt)

appendSystemPrompt is always added at the end (except when override is set).
```

### Cache Control

The `buildSystemPromptBlocks()` function in `src/services/api/claude.ts` splits the system prompt at the dynamic boundary into two blocks for the API's `system` parameter:
- Block 1 (static): `{ type: 'text', text: staticContent, cache_control: { type: 'ephemeral' } }`
- Block 2 (dynamic): `{ type: 'text', text: dynamicContent }`

This enables the server to cache the static portion across requests.

## Key Source Files

| File | Purpose |
|------|---------|
| `src/constants/prompts.ts` | Main system prompt assembly (`getSystemPrompt()`) |
| `src/constants/systemPromptSections.ts` | Section caching infrastructure |
| `src/utils/queryContext.ts` | `fetchSystemPromptParts()` orchestration |
| `src/utils/systemPrompt.ts` | Override priority resolution |
| `src/utils/api.ts` | `prependUserContext()`, `appendSystemContext()` |
| `src/services/api/claude.ts` | `buildSystemPromptBlocks()` for API formatting |
| `src/context.ts` | `getUserContext()` and `getSystemContext()` |

## Configuration

The system prompt itself is not directly configurable. However:
- CLAUDE.md content is injected via the user context path (see [CLAUDE.md Injection](claude_md_injection.md))
- MCP server instructions come from connected servers
- The `--system-prompt` CLI flag overrides the default prompt
- The `appendSystemPrompt` SDK option adds content to the end

## Cross-References

- [CLAUDE.md Injection](claude_md_injection.md) -- How CLAUDE.md files are loaded and injected
- [Rules System](rules_system.md) -- How .claude/rules/ files work
- [Context Window](context_window.md) -- Token budget implications

## Interesting Findings

**CLAUDE.md is NOT system-level.** Despite the `MEMORY_INSTRUCTION_PROMPT` preamble saying these instructions "OVERRIDE any default behavior," CLAUDE.md content is injected as a user message with hedging language: "this context may or may not be relevant to your tasks." This creates a tension -- the preamble demands compliance while the wrapper suggests optionality.

**Auto-compact regenerates the prompt fresh.** The system prompt is never truncated by compaction. It is regenerated from scratch every turn. This means verbose rules have a persistent cost on every API call -- every character pays rent on every request.

**Git status is truncated at 2K characters.** The `MAX_STATUS_CHARS = 2000` constant in `src/context.ts` means repositories with many untracked files will have truncated status. The truncation message tells the model to use Bash for more detail.

**Section caching uses module-level state.** Sections created with `systemPromptSection()` are cached in `getSystemPromptSectionCache()` until `/clear` or `/compact` calls `clearSystemPromptSections()`. This means dynamic sections like environment info are computed once per session, not once per turn.
