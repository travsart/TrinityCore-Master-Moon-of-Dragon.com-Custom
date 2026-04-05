---
description: "memory selector — Sonnet side-query, 2-stage scan+select, reads only filename + description frontmatter, staleness handling, 5 file max per turn"
---

# Memory Selector -- Arcanum Wiki

## Overview

Claude Code's memory recall system uses a two-stage process to load relevant topic files on demand: a fast directory scan that builds a manifest of available memories with their frontmatter descriptions, followed by a Sonnet side-query that selects up to 5 relevant files per turn based on the user's query. The selector never reads file content -- it operates entirely on filenames and one-line descriptions from YAML frontmatter.

## How It Works

### Stage 1: Scan (`memoryScan.ts`)

`scanMemoryFiles()` reads the memory directory recursively:

1. Lists all `.md` files (excluding `MEMORY.md` itself)
2. Reads the first 30 lines of each file (frontmatter extraction only)
3. Parses frontmatter for `description` and `type` fields
4. Sorts by mtime (newest first)
5. Caps at **200 files maximum**

The scan produces a manifest in this format:

```
- [user] user_role.md (2026-04-03T12:00:00.000Z): Senior engineer focused on React
- [feedback] feedback_testing.md (2026-04-02T10:00:00.000Z): Don't mock databases
- [project] migration.md (2026-04-01T08:00:00.000Z): TrinityCore migration to 66709
```

### Stage 2: Select (`findRelevantMemories.ts`)

`findRelevantMemories()` uses a Sonnet side-query (small, fast model) to select relevant memories. The system prompt:

```
You are selecting memories that will be useful to Claude Code as it processes
a user's query. You will be given the user's query and a list of available
memory files with their filenames and descriptions.

Return a list of filenames for the memories that will clearly be useful to
Claude Code as it processes the user's query (up to 5). Only include memories
that you are certain will be helpful based on their name and description.
- If you are unsure if a memory will be useful in processing the user's query,
  then do not include it in your list. Be selective and discerning.
- If there are no memories in the list that would clearly be useful, feel free
  to return an empty list.
- If a list of recently-used tools is provided, do not select memories that are
  usage reference or API documentation for those tools (Claude Code is already
  exercising them). DO still select memories containing warnings, gotchas, or
  known issues about those tools -- active use is exactly when those matter.
```

The side-query receives the user's message plus the full manifest of available memories. It returns up to 5 file paths with mtime values.

### Deduplication of Already-Surfaced Files

Files that were already surfaced in prior turns are filtered out BEFORE the Sonnet call. This means the 5-slot budget is always spent on fresh candidates. The `alreadySurfaced` parameter tracks which files were shown in previous turns.

This has an important implication: if a memory is relevant across multiple turns, it will only be loaded once. The model must retain the information from the initial injection.

### Staleness Handling

When topic files are loaded, a staleness caveat is attached for memories older than 1 day (from `src/memdir/memoryAge.ts`):

```
This memory is {N} days old. Memories are point-in-time observations, not live
state -- claims about code behavior or file:line citations may be outdated.
Verify against current code before asserting as fact.
```

The age is computed as `Math.floor((Date.now() - mtimeMs) / 86_400_000)` and presented as human-readable strings ("today", "yesterday", "47 days ago") because "models are poor at date arithmetic."

### Extraction Agent Prompt

The extraction agent (which writes new memories) receives the manifest pre-injected so it does not spend a turn on `ls`:

```
## Existing memory files

- [user] user_role.md (2026-04-03T...): Senior engineer focused on React
- [feedback] feedback_testing.md (2026-04-02T...): Don't mock databases

Check this list before writing -- update an existing file rather than creating a duplicate.
```

### Tool Permissions for Extraction Agent

The `createAutoMemCanUseTool()` function enforces strict limits:

| Tool | Permission |
|------|-----------|
| Read, Grep, Glob | Unrestricted |
| Bash | Read-only commands only (ls, find, grep, cat, stat, wc, head, tail) |
| Edit, Write | ONLY for paths within the auto-memory directory |
| All others | Denied |
| Max turns | 5 |

## Key Source Files

| File | Purpose |
|------|---------|
| `src/memdir/findRelevantMemories.ts` | Sonnet side-query selection logic |
| `src/memdir/memoryScan.ts` | Directory scanning, frontmatter parsing, 200-file cap |
| `src/memdir/memoryAge.ts` | Staleness calculation, human-readable age strings |
| `src/services/extractMemories/extractMemories.ts` | Background extraction agent |
| `src/services/extractMemories/prompts.ts` | Extraction prompt templates |

## Configuration

The selector's behavior is not directly configurable by users. The 5-file limit and 200-file cap are hard-coded. The selector model defaults to Sonnet (small, fast).

## Cross-References

- [Memory Overview](memory_overview.md) -- Full memory system architecture
- [Memory Limits](memory_limits.md) -- All hard limits
- [AutoDream](autodream.md) -- Cross-session memory consolidation

## Interesting Findings

**The description field is the entire ranking signal.** The Sonnet selector never reads file content -- it operates exclusively on the one-line `description` field in frontmatter. A memory with a vague description like "project notes" will almost never be selected over one with "TrinityCore creature_template column names: faction not FactionID."

**Mutual exclusion with the main agent.** When the main agent writes memories itself (detected by scanning assistant messages for Write/Edit tool_use blocks targeting auto-memory paths via `hasMemoryWritesSince()`), the background extraction agent skips entirely. The cursor advances past the range so the next extraction only sees new messages.

**Recently-used tools suppress API docs but not warnings.** The selector prompt explicitly distinguishes between usage documentation (suppressed when the tool is actively being used) and warnings/gotchas (always shown because "active use is exactly when those matter").
