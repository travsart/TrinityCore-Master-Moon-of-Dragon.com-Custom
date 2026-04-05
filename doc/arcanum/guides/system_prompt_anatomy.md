---
description: "system prompt anatomy — assembly pipeline, dynamic boundary, CLAUDE.md as user message not system, cacheable sections, token costs, what you can control"
---

# Guide: Anatomy of the System Prompt — Arcanum Wiki

> Exactly how Claude Code's system prompt is assembled, what goes where, and what you can control.

## The Big Revelation

**CLAUDE.md is NOT in the API system prompt.** It's injected as the first user message wrapped in `<system-reminder>` tags with a hedging disclaimer: "may or may not be relevant to your tasks."

This has real implications — the model treats system prompt content as higher authority than user messages. Your CLAUDE.md instructions are technically "user context" from the API's perspective, which is why Claude sometimes ignores them.

**Source**: `constants/prompts.ts` — the `getSystemPrompt()` function builds the actual `system` parameter. CLAUDE.md is injected separately in `utils/messages/`.

## System Prompt Structure

The prompt is built from sections, separated by a **dynamic boundary** marker:

```
┌──────────────────────────────────────────────┐
│ STATIC (cacheable across orgs)               │
│                                              │
│ 1. Prefix: "You are Claude Code..."         │
│ 2. Intro: software engineering helper        │
│ 3. Cyber risk instruction                    │
│ 4. System section (tools, hooks, tags)       │
│ 5. Doing tasks (code style, practices)       │
│ 6. Executing actions with care               │
│ 7. Using your tools                          │
│ 8. Tone and style                            │
│ 9. Git commit instructions                   │
│ 10. PR creation instructions                 │
│ 11. Other common operations                  │
├─ __SYSTEM_PROMPT_DYNAMIC_BOUNDARY__ ─────────┤
│ DYNAMIC (per-session, not cacheable)         │
│                                              │
│ 12. Auto memory instructions                 │
│ 13. Environment (CWD, platform, OS, model)   │
│ 14. MCP server instructions                  │
│ 15. Skill descriptions                       │
│ 16. Fast mode info                           │
│ 17. Language preference                      │
│ 18. Output style                             │
└──────────────────────────────────────────────┘
```

Everything ABOVE the dynamic boundary uses `scope: 'global'` for prompt caching — it's identical across all users and sessions, so Anthropic can cache it aggressively.

Everything BELOW is per-session and changes based on your environment, connected MCP servers, active skills, etc.

## What Goes Into Each Section

### The Prefix (Line 1)

Three possible prefixes based on context:

| Context | Prefix |
|---------|--------|
| Interactive CLI | `"You are Claude Code, Anthropic's official CLI for Claude."` |
| Agent SDK (Claude Code preset) | `"You are Claude Code, Anthropic's official CLI for Claude, running within the Claude Agent SDK."` |
| Agent SDK (custom) | `"You are a Claude agent, built on Anthropic's Claude Agent SDK."` |

**Source**: `constants/system.ts:10-18`

### Cyber Risk Instruction

A content policy instruction about security testing:

> "IMPORTANT: Assist with authorized security testing, defensive security, CTF challenges, and educational contexts. Refuse requests for destructive techniques..."

This is loaded from `constants/cyberRiskInstruction.ts` and is part of the static, cacheable section.

### Doing Tasks Section

Contains the code quality instructions — this is where the "over-engineering" warnings come from:

- "Avoid over-engineering. Only make changes that are directly requested"
- "Don't add features, refactor code, or make 'improvements' beyond what was asked"
- "Don't add error handling, fallbacks, or validation for scenarios that can't happen"
- "Don't create helpers, utilities, or abstractions for one-time operations"
- "Avoid backwards-compatibility hacks"

These are hardcoded in `constants/prompts.ts` and cannot be customized.

### Tool Usage Section

Dynamically generated based on which tools are available. Key rules baked in:
- "Do NOT use Bash to run commands when a relevant dedicated tool is provided"
- Read over cat, Edit over sed, Write over echo, Glob over find, Grep over grep
- "Use the Agent tool with specialized agents when the task matches"
- Explore agent guidance (min 3 queries threshold)
- Skill tool invocation rules

### Git Instructions

The full git commit and PR creation templates. These are LONG (~2K tokens) and include:
- Commit message format with Co-Authored-By
- PR creation workflow with gh CLI
- Safety protocol (never force push, never skip hooks)

### Environment Section

Dynamically generated per-session:

```
# Environment
- Primary working directory: /path/to/project
  - Is a git repository: true
- Platform: win32
- Shell: bash
- OS Version: Windows 11 Pro 10.0.26200
- You are powered by [model name]. The exact model ID is [id].
```

**Model info**: The prompt includes the marketing name AND the canonical model ID. It also includes the knowledge cutoff date and latest model family information.

### Auto Memory Section

Only included if a memory directory exists:

```
# auto memory
You have a persistent auto memory directory at `~/.claude/projects/.../memory/`.
...
## How to save memories
## What to save
## What NOT to save
## Explicit user requests
```

This section teaches Claude how to use its own memory system.

### MCP Server Instructions

For each connected MCP server that provides instructions:

```
# MCP Server Instructions

## server-name
[Instructions provided by the server]
```

### Skill Descriptions

Active skills are listed with their descriptions:

```
The following skills are available for use with the Skill tool:
- /build-loop: Iteratively build and fix errors
- /check-logs: Read server logs for errors
...
```

## What Happens AFTER the System Prompt

After the system prompt is set, additional context is injected as user messages:

1. **CLAUDE.md content** — wrapped in `<system-reminder>` tags with "may or may not be relevant" hedging
2. **Rules files** — from `.claude/rules/` (conditional ones only if paths match)
3. **Memory files** — MEMORY.md (always) + up to 5 selected topic files
4. **Git status** — branch, remote, status (capped at 2K chars)

These are ALL injected as user messages, not system messages. This is architecturally significant — system messages have higher authority in the model's attention.

## Controlling What You Can

### You CAN control:
- CLAUDE.md content (injected as user message)
- `.claude/rules/` files (conditional activation)
- Memory files and their frontmatter
- MCP server instructions (via your servers)
- Skill descriptions and activation
- Environment (CWD, git state)
- Git status cleanliness (.gitignore)

### You CANNOT control:
- The system prompt text (hardcoded)
- Tool descriptions (hardcoded)
- The "may or may not be relevant" hedging on CLAUDE.md
- Section ordering
- The dynamic boundary position
- Code style instructions ("don't over-engineer")

### You CAN work around:
- The hedging on CLAUDE.md: Use strong, imperative language. "MUST", "ALWAYS", "NEVER", "CRITICAL". The hedging weakens authority, but emphatic language partially compensates.
- Over-engineering warnings: Be explicit in your CLAUDE.md about when you DO want comprehensive solutions.
- Tool description limits: MCP servers can add instructions that augment tool descriptions.

## Token Costs

Approximate token costs for each section:

| Section | ~Tokens | Cacheable |
|---------|---------|-----------|
| Static prefix + instructions | 8,000-12,000 | Yes (global) |
| Tool descriptions (40+ tools) | 3,000-5,000 | Yes (global) |
| Git instructions | ~2,000 | Yes (global) |
| Environment | ~500 | No (per-session) |
| Memory instructions | ~800 | No (per-session) |
| MCP instructions | Variable | No (per-session) |
| Skill descriptions | Variable | No (per-session) |
| CLAUDE.md (user message) | YOUR content | No |
| Rules files | YOUR content | No |
| Memory files | YOUR content | No |
| Git status | Up to 2K chars | No |

**Total baseline**: ~15-20K tokens before your content. This is the "tax" every turn.

## Cross-References

- [Architecture Overview](../core/architecture.md) — full system architecture
- [CLAUDE.md Injection](../core/claude_md_injection.md) — how CLAUDE.md is found and merged
- [Context Window](optimizing_context.md) — optimizing your context budget
- [Rules System](../core/rules_system.md) — conditional rules deep dive
