---
description: "memory system architecture — 4 subsystems, memory directory structure, file types, MEMORY.md always-loaded, topic file selection, persistence"
---

# Memory System Architecture -- Arcanum Wiki

## Overview

Claude Code has four distinct memory subsystems forming a layered pipeline: MEMORY.md (the always-loaded index entrypoint), topic files (loaded on-demand by a Sonnet side-query), session memory (per-conversation structured notes for compaction), and background extraction (a forked agent that writes new memories after each query loop). A fifth subsystem, team memory sync, provides server-synced shared memory across GitHub org members.

The critical design choice is that MEMORY.md is an index, not a dump. It points to topic files that contain the actual memory content. The Sonnet selector only sees filenames and frontmatter descriptions -- it never reads file content during selection.

## How It Works

### End-to-End Flow

1. **Session Start**: MEMORY.md is loaded into the system prompt (truncated at 200 lines / 25KB). Topic files are NOT preloaded.
2. **Per-Turn Recall**: On each user message, a Sonnet side-query scans topic file headers (frontmatter) and selects up to 5 relevant memories to inject.
3. **Per-Turn Extraction**: After each query loop completion (model produces final response with no tool calls), a background forked agent analyzes the conversation and writes new memories.
4. **Session Memory**: A separate periodic background agent maintains a structured session summary file (used for compaction).
5. **Team Sync**: A file watcher monitors the team memory subdirectory and syncs changes to/from an Anthropic server API, scoped per GitHub repo.

### Memory Directory Structure

```
~/.claude/projects/<sanitized-project-root>/memory/
    MEMORY.md                    # Index entrypoint (always loaded)
    user_role.md                 # Topic files with frontmatter
    feedback_testing.md
    project_deadlines.md
    team/                        # Team memory subdirectory
        MEMORY.md                # Team index
        patterns.md
    logs/                        # KAIROS daily logs (assistant mode only)
```

All git worktrees of the same repo share one memory directory. The path is derived from `findCanonicalGitRoot()`, not the working directory.

### Four Memory Types

Memories are constrained to exactly four types (from `src/memdir/memoryTypes.ts`):

| Type | Description | When to Save |
|------|-------------|--------------|
| `user` | Role, goals, preferences, knowledge | Learning about the user |
| `feedback` | Corrections AND confirmations on approach | User corrects/confirms behavior |
| `project` | Ongoing work, goals, bugs, incidents | Who/what/why/when context |
| `reference` | Pointers to external systems | External resource locations |

Explicitly excluded (even if user asks): code patterns, architecture, file paths, git history, debugging solutions, and anything already in CLAUDE.md files.

### Topic File Frontmatter

```yaml
---
name: {{memory name}}
description: {{one-line description -- used for relevance decisions}}
type: {{user, feedback, project, reference}}
---
```

The `description` field is the ONLY thing driving relevance during selection. Poorly-described topic files will never be recalled.

## Key Source Files

| File | Purpose |
|------|---------|
| `src/memdir/memdir.ts` | MEMORY.md loading, truncation, prompt building |
| `src/memdir/memoryTypes.ts` | Four-type taxonomy, frontmatter format |
| `src/memdir/paths.ts` | Path resolution, security validation |
| `src/memdir/memoryScan.ts` | Directory scanning, 200-file cap |
| `src/memdir/findRelevantMemories.ts` | Sonnet side-query for topic selection |
| `src/services/extractMemories/extractMemories.ts` | Background extraction agent |
| `src/services/SessionMemory/sessionMemory.ts` | Session memory maintenance |
| `src/services/teamMemorySync/` | Team memory sync infrastructure |

## Configuration

| Variable | Effect |
|----------|--------|
| `CLAUDE_CODE_DISABLE_AUTO_MEMORY=1` | Disables all memory features |
| `CLAUDE_COWORK_MEMORY_PATH_OVERRIDE` | Full path override for memory directory |
| `CLAUDE_COWORK_MEMORY_EXTRA_GUIDELINES` | Injects extra text into memory prompt |
| `autoMemoryEnabled` in settings | Enable/disable auto-memory |

## Cross-References

- [Memory Selector](memory_selector.md) -- How Sonnet selects topic files
- [Memory Limits](memory_limits.md) -- All hard limits and caps
- [AutoDream](autodream.md) -- Background memory consolidation

## Interesting Findings

**The extraction agent has a 5-turn budget.** The efficient strategy is: turn 1 = parallel reads of all files to update; turn 2 = parallel writes/edits. No room for verification or exploration.

**Write carve-out security model.** The auto-memory directory has a special exemption from `DANGEROUS_DIRECTORIES` write checks. However, the `autoMemoryDirectory` setting in project-scoped `.claude/settings.json` is intentionally excluded from settings sources to prevent a malicious repo from setting `autoMemoryDirectory: "~/.ssh"` and gaining silent write access.

**KAIROS daily log mode.** When `feature('KAIROS')` is active, memory writing changes completely: instead of maintaining MEMORY.md as a live index, the agent appends to daily log files, and a nightly `/dream` skill distills logs into topic files.
