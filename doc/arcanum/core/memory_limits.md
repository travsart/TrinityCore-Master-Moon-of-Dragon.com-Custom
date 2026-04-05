---
description: "memory hard limits — 200 lines MEMORY.md, 25KB cap, 200 file mtime cap, 5 files per turn, 50K post-compact restore budget, 5K per file restore cap"
---

# Memory Limits -- Arcanum Wiki

## Overview

The memory system enforces multiple hard limits to prevent context bloat and maintain performance. These limits are scattered across several source files and operate at different levels -- from the MEMORY.md entrypoint to individual topic file selection to the overall memory directory capacity.

## How It Works

### MEMORY.md Entrypoint Limits

From `src/memdir/memdir.ts`:

```typescript
export const ENTRYPOINT_NAME = 'MEMORY.md'
export const MAX_ENTRYPOINT_LINES = 200
export const MAX_ENTRYPOINT_BYTES = 25_000  // ~125 chars/line at 200 lines
```

**Truncation logic** (`truncateEntrypointContent()`):
1. Line truncation first (natural boundary): if >200 lines, slice to first 200
2. Byte truncation second: if resulting content >25KB, cut at last newline before 25KB
3. Warning appended if either cap fires:

```
> WARNING: MEMORY.md is {reason}. Only part of it was loaded. Keep index entries
> to one line under ~200 chars; move detail into topic files.
```

The `reason` varies: lines only, bytes only, or both.

### Topic File Limits

**200 files maximum.** `scanMemoryFiles()` in `src/memdir/memoryScan.ts` caps the scan at 200 files, sorted by mtime (newest first). Files beyond this threshold become invisible to the selector.

**5 files per turn.** `findRelevantMemories()` returns at most 5 topic file paths per user message. Files already surfaced in prior turns are filtered out before selection, so the 5-slot budget is always spent on fresh candidates.

**30-line frontmatter extraction.** Only the first 30 lines of each topic file are read during scanning (for frontmatter extraction). The file content is loaded in full only when selected by the Sonnet query.

### Session Memory Limits

From `src/services/SessionMemory/prompts.ts`:

```
MAX_SECTION_LENGTH = 2000 tokens per section
MAX_TOTAL_SESSION_MEMORY_TOKENS = 12000 total
```

When session memory exceeds the budget, the update prompt includes aggressive condensation instructions.

The session memory template has 10 sections (Session Title, Current State, Task Specification, Files and Functions, Workflow, Errors & Corrections, Codebase Documentation, Learnings, Key Results, Worklog). At 2000 tokens per section, the theoretical maximum is 20K tokens, but the 12K total cap forces prioritization.

### Session Memory Compact Thresholds

From `src/services/compact/sessionMemoryCompact.ts`:

```typescript
export const DEFAULT_SM_COMPACT_CONFIG: SessionMemoryCompactConfig = {
  minTokens: 10_000,          // At least 10K tokens of recent messages kept
  minTextBlockMessages: 5,     // At least 5 messages with text kept
  maxTokens: 40_000,          // Hard cap: keep at most 40K tokens
}
```

### Extraction Agent Limits

- **Max turns: 5** -- The background extraction agent is capped at 5 tool-call rounds
- **Efficient strategy**: Turn 1 = parallel reads, Turn 2 = parallel writes. No room for exploration.
- **Throttle**: Fires every N eligible turns (configurable via `tengu_bramble_lintel`, default 1)

### CLAUDE.md Character Warning

From `src/utils/claudemd.ts`:

```typescript
export const MAX_MEMORY_CHARACTER_COUNT = 40000
```

Files exceeding 40,000 characters are flagged but NOT truncated. This is a soft warning only.

### Team Memory Limits

From `src/services/teamMemorySync/`:

```typescript
const MAX_FILE_SIZE_BYTES = 250_000    // per entry (250KB)
const MAX_PUT_BODY_BYTES = 200_000     // per PUT request
```

Entries exceeding the PUT body limit are split into sequential uploads.

### Post-Compact File Restoration

From `src/services/compact/compact.ts`:

```typescript
export const POST_COMPACT_MAX_FILES_TO_RESTORE = 5
export const POST_COMPACT_TOKEN_BUDGET = 50_000
export const POST_COMPACT_MAX_TOKENS_PER_FILE = 5_000
export const POST_COMPACT_MAX_TOKENS_PER_SKILL = 5_000
export const POST_COMPACT_SKILLS_TOKEN_BUDGET = 25_000
```

## Summary Table

| Limit | Value | Enforced By |
|-------|-------|-------------|
| MEMORY.md max lines | 200 | `memdir.ts` (hard truncation) |
| MEMORY.md max bytes | 25,000 | `memdir.ts` (hard truncation) |
| Topic files max | 200 | `memoryScan.ts` (scan cap) |
| Topics per turn | 5 | `findRelevantMemories.ts` (selection cap) |
| Frontmatter scan lines | 30 | `memoryScan.ts` |
| Session memory per section | 2,000 tokens | `SessionMemory/prompts.ts` |
| Session memory total | 12,000 tokens | `SessionMemory/prompts.ts` |
| SM compact min preserved | 10,000 tokens | `sessionMemoryCompact.ts` |
| SM compact max preserved | 40,000 tokens | `sessionMemoryCompact.ts` |
| SM compact min messages | 5 text messages | `sessionMemoryCompact.ts` |
| Extraction agent turns | 5 | `extractMemories.ts` |
| CLAUDE.md char warning | 40,000 | `claudemd.ts` (soft warning only) |
| Team memory per entry | 250,000 bytes | `teamMemorySync/` |
| Post-compact files | 5 files, 50K tokens | `compact.ts` |
| Post-compact per file | 5,000 tokens | `compact.ts` |
| Post-compact skills | 25,000 tokens total | `compact.ts` |

## Key Source Files

| File | Purpose |
|------|---------|
| `src/memdir/memdir.ts` | MEMORY.md truncation constants |
| `src/memdir/memoryScan.ts` | 200-file cap, 30-line scan |
| `src/memdir/findRelevantMemories.ts` | 5-per-turn selection limit |
| `src/services/SessionMemory/prompts.ts` | Section and total token limits |
| `src/services/compact/sessionMemoryCompact.ts` | SM compact thresholds |

## Cross-References

- [Memory Overview](memory_overview.md) -- Full architecture
- [Memory Selector](memory_selector.md) -- How selection works within limits
- [Compaction Tiers](compaction_tiers.md) -- How session memory compact uses these thresholds

## Interesting Findings

**The 200-file cap sorts by mtime, not relevance.** Older files are dropped regardless of their importance. This means a critical but rarely-updated reference file could become invisible if 200+ newer files exist. Consolidating related memories into fewer files is the recommended mitigation.

**MEMORY.md truncation is silent in context.** The warning is appended to the truncated content, which means the model sees it. But the user does not see a separate notification -- the only indication is the model's behavior when it encounters the warning in its context.

**Session memory customization is possible.** The template and prompt can be overridden by placing files at `~/.claude/session-memory/config/template.md` and `~/.claude/session-memory/config/prompt.md`. The prompt supports `{{variableName}}` substitution.
