---
description: "CLAUDE.md injection — 6 source discovery, @include directives, priority order managed user project local, injected as user message not system prompt"
---

# CLAUDE.md Injection -- Arcanum Wiki

## Overview

CLAUDE.md files are Claude Code's primary mechanism for project-specific instructions. Despite their name suggesting a connection to the system prompt, they are injected as the first user message wrapped in `<system-reminder>` tags, not as part of the `system` API parameter. This means CLAUDE.md content does not benefit from system-level prompt caching and competes with conversation messages for context space.

The loading system discovers CLAUDE.md files from six sources in a specific priority order, supports recursive `@include` directives, conditional rules via frontmatter, and HTML comment stripping for author-only notes.

## How It Works

### Load Order (Later = Higher Priority)

The discovery process is implemented in `getMemoryFiles()` at `src/utils/claudemd.ts:790`. Files loaded later have higher priority because the model pays more attention to recency:

```
1. Managed     /etc/claude-code/CLAUDE.md              -- Lowest priority (policy)
2. User        ~/.claude/CLAUDE.md                     -- Private global instructions
3. Project     CLAUDE.md, .claude/CLAUDE.md,           -- Checked into codebase
               .claude/rules/*.md (per parent dir)
4. Local       CLAUDE.local.md (per parent dir)        -- Private, not checked in
5. AutoMem     ~/.claude/projects/<hash>/MEMORY.md     -- Auto-memory entrypoint
6. TeamMem     (feature-gated team memory)             -- Shared team memory
```

### Discovery Process

The function walks the directory tree from root to CWD:

1. **Managed**: Reads `/etc/claude-code/CLAUDE.md` and `/etc/claude-code/.claude/rules/*.md`
2. **User** (if enabled): Reads `~/.claude/CLAUDE.md` and `~/.claude/rules/*.md`
3. **Project + Local** (directory walk):
   - Starts at CWD, walks UP to filesystem root, collecting directories
   - Reverses the list (now root-to-CWD order)
   - For each directory: reads `CLAUDE.md`, `.claude/CLAUDE.md`, `.claude/rules/*.md`, and `CLAUDE.local.md`
   - Walk stops at git root to prevent parent directory leakage
4. **Additional directories** (from `--add-dir` flag)
5. **AutoMem entrypoint**: `~/.claude/projects/<hash>/MEMORY.md`
6. **TeamMem entrypoint** (feature-gated)

### Formatting and Injection

Each file is formatted with a header describing its source (`getClaudeMds()` at line 1153):

```
Contents of /path/to/CLAUDE.md (project instructions, checked into the codebase):

<file content>
```

Type descriptions vary:
- Project: `"(project instructions, checked into the codebase)"`
- Local: `"(user's private project instructions, not checked in)"`
- User: `"(user's private global instructions for all projects)"`
- AutoMem: `"(user's auto-memory, persists across conversations)"`
- TeamMem: `"(shared team memory, synced across the organization)"`

All files are concatenated with the preamble:

```
Codebase and user instructions are shown below. Be sure to adhere to these
instructions. IMPORTANT: These instructions OVERRIDE any default behavior
and you MUST follow them exactly as written.
```

The final concatenated content is wrapped in `<system-reminder>` tags and prepended as the first user message:

```xml
<system-reminder>
As you answer the user's questions, you can use the following context:
# claudeMd
<all CLAUDE.md content>

# currentDate
Today's date is 2026-04-04.

      IMPORTANT: this context may or may not be relevant to your tasks.
      You should not respond to this context unless it is highly relevant
      to your task.
</system-reminder>
```

### @include Directive

CLAUDE.md files support an include mechanism for referencing external files:

- Syntax: `@path`, `@./relative/path`, `@~/home/path`, `@/absolute/path`
- Max recursion depth: 5 (`MAX_INCLUDE_DEPTH` at line 537)
- Only works in leaf text nodes (not inside code blocks)
- Circular references prevented by tracking processed paths
- Non-existent files silently ignored
- Only text file extensions allowed (`.md`, `.ts`, `.py`, `.sql`, etc. -- 80+ extensions)

### HTML Comment Stripping

Block-level HTML comments (`<!-- ... -->`) are stripped from content before injection via `stripHtmlComments()` at line 292. This allows CLAUDE.md authors to include notes that the model never sees -- useful for temporarily disabling sections or leaving editor-only documentation.

### Size Limits

- **40K character soft limit**: Files exceeding 40,000 characters are flagged by `getLargeMemoryFiles()` but NOT truncated. The warning is informational only.
- **MEMORY.md hard limits**: 200 lines / 25KB max (truncated with warning appended)
- **No system prompt budget**: There is no explicit token budget for CLAUDE.md. It consumes whatever tokens it needs from the context window. The only constraint is that total context (system prompt + CLAUDE.md + conversation) must stay within the auto-compact threshold.

## Key Source Files

| File | Purpose |
|------|---------|
| `src/utils/claudemd.ts` | Core loading, parsing, formatting (~1,400 lines) |
| `src/context.ts` | `getUserContext()` that wraps CLAUDE.md for injection |
| `src/utils/api.ts` | `prependUserContext()` that creates the first user message |
| `src/memdir/memdir.ts` | MEMORY.md loading and truncation |

## Configuration

| Setting | Effect |
|---------|--------|
| `userSettings` source toggle | Enables/disables `~/.claude/CLAUDE.md` loading |
| `--add-dir` CLI flag | Adds additional directories for CLAUDE.md discovery |
| `MAX_MEMORY_CHARACTER_COUNT = 40000` | Soft warning threshold for file size |

## Cross-References

- [System Prompt](system_prompt.md) -- Where CLAUDE.md fits in the prompt assembly
- [Rules System](rules_system.md) -- How .claude/rules/ files work
- [Memory Overview](memory_overview.md) -- MEMORY.md and the auto-memory system
- [Compaction Instructions](compaction_instructions.md) -- How CLAUDE.md survives compaction

## Interesting Findings

**Priority order is counterintuitive.** Files loaded LATER have HIGHER priority. This means `.claude/rules/` files at the CWD level are loaded latest among project files and get the most model attention. Universal defaults should go in `~/.claude/CLAUDE.md` (loaded early, lowest priority).

**Rules files within a directory have NO guaranteed sort order.** They come in `readdir()` order, which is filesystem-dependent. On most systems this is roughly alphabetical, but it is NOT guaranteed. Each rules file should be self-contained rather than depending on execution order.

**The `tengu_moth_copse` feature flag changes memory injection.** When active, MEMORY.md index is NOT injected into the user context. Instead, memory files are surfaced via attachments through a `findRelevantMemories` prefetch. The `filterInjectedMemoryFiles()` function strips AutoMem and TeamMem types from the injection path.
