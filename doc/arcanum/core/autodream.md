---
description: "AutoDream background consolidation — 4 phases Orient Gather Consolidate Prune, 24h + 5 session gate, tengu_onyx_plover flag, lock file mtime, memory updates"
---

# AutoDream -- Arcanum Wiki

## Overview

AutoDream is Claude Code's background memory consolidation engine -- a system that periodically reviews recent session transcripts and consolidates what it learned into durable, well-organized memory files. The metaphor is deliberate: the system "dreams" about past conversations to distill important information. It runs as a forked subagent that shares the parent's prompt cache, operates in the background with a "dreaming" pill visible in the footer, and can be killed by the user via the background tasks dialog.

AutoDream is distinct from per-turn memory extraction (`extractMemories`), which runs after every query loop completion. AutoDream runs across sessions -- it fires when 5+ sessions have accumulated and 24+ hours have passed since the last consolidation.

## How It Works

### The Four-Phase Consolidation Process

The consolidation prompt (`src/services/autoDream/consolidationPrompt.ts`) defines a structured workflow:

**Phase 1 -- Orient**: Read the memory directory, review MEMORY.md, skim existing topic files to understand current state and avoid creating duplicates.

**Phase 2 -- Gather Recent Signal**: Look for new information in daily logs (if KAIROS mode), existing memories that have drifted from reality, and session transcripts (grep narrowly for specific terms, never exhaustively read).

**Phase 3 -- Consolidate**: Write or update memory files following the standard memory type conventions. Focus on merging into existing files, converting relative dates to absolute dates, and deleting contradicted facts.

**Phase 4 -- Prune and Index**: Update MEMORY.md to stay under 200 lines and 25KB. Each entry should be one line under 150 characters. Remove stale pointers, shorten verbose entries, add new pointers, resolve contradictions.

### Gating Conditions (Cheapest-First Order)

For AutoDream to fire, ALL of these must be true:

**Gate 0 -- Feature**: Not KAIROS mode, not remote mode, auto-memory enabled, auto-dream enabled (user setting or GrowthBook `tengu_onyx_plover.enabled`).

**Gate 1 -- Time** (one `stat` call): Hours since last consolidation >= `minHours` (default 24). The timestamp is the lock file's mtime. If no lock file exists, `lastConsolidatedAt = 0` and the gate always passes.

**Gate 2 -- Scan Throttle**: Time since last session scan >= 10 minutes (`SESSION_SCAN_INTERVAL_MS`). Prevents redundant directory scans when the time gate passes but the session gate does not.

**Gate 3 -- Session**: Number of session transcripts modified since last consolidation >= `minSessions` (default 5). Current session is excluded.

**Gate 4 -- Lock** (PID file): No other live process currently holds the consolidation lock. Stale locks (PID alive but lock > 1 hour old) are reclaimed.

### Lock File Mechanics

The lock file at `{autoMemPath}/.consolidate-lock` serves dual duty: mutual exclusion AND timestamp storage. Its mtime IS `lastConsolidatedAt`, and its body contains the PID of the current holder.

**State machine**:
- NO FILE: lastConsolidatedAt = 0, never dreamed
- FILE (no live PID): reclaimable, dream can proceed
- FILE (live PID, < 1h old): blocked, another process is dreaming
- FILE (live PID, > 1h old): stale, reclaim

**Rollback on failure**: If the forked agent fails, `rollbackConsolidationLock(priorMtime)` rewinds the mtime so the time gate passes again on the next attempt. The scan throttle provides natural backoff.

### DreamTask UI Integration

The dream agent registers as a `DreamTask` in AppState, visible in the footer as a "dreaming" pill. The detail dialog shows:
- Status (running/completed/failed/killed)
- Elapsed time
- Number of sessions being reviewed
- Number of files touched
- Last 6 text turns from the agent (earlier turns collapse to a count)

When the user kills a dream (press 'x' in the dialog):
1. AbortController.abort() cancels the forked agent
2. Lock mtime is rolled back so next session can retry
3. Status set to 'killed'

### Tool Permissions

The dream agent uses the same `createAutoMemCanUseTool()` function as the extraction agent:

| Tool | Permission |
|------|-----------|
| Read, Grep, Glob | Unrestricted |
| Bash | Read-only only (ls, find, grep, cat, stat, wc, head, tail) |
| Edit, Write | ONLY within the auto-memory directory |
| All others | Denied |

### Completion Notification

When dreaming completes and files were touched:
```typescript
if (dreamState.filesTouched.length > 0) {
  appendSystemMessage({
    ...createMemorySavedMessage(dreamState.filesTouched),
    verb: 'Improved',
  })
}
```

This shows "Improved 3 memories" in the main conversation transcript.

## Key Source Files

| File | Purpose |
|------|---------|
| `src/services/autoDream/autoDream.ts` | Main orchestrator, gate evaluation, forked agent launch |
| `src/services/autoDream/config.ts` | Feature flag check, `isAutoDreamEnabled()` |
| `src/services/autoDream/consolidationLock.ts` | Lock + session scan, rollback |
| `src/services/autoDream/consolidationPrompt.ts` | The 4-phase consolidation prompt |
| `src/tasks/DreamTask/DreamTask.ts` | Task state, UI lifecycle, kill handler |
| `src/components/tasks/DreamDetailDialog.tsx` | Detail dialog UI |
| `src/utils/backgroundHousekeeping.ts` | Startup initialization |

## Configuration

| Setting | Default | Effect |
|---------|---------|--------|
| `autoDreamEnabled` in settings | Server-controlled | Enable/disable auto-dream |
| `tengu_onyx_plover.enabled` (GrowthBook) | `false` | Server-side default |
| `tengu_onyx_plover.minHours` | 24 | Minimum hours between dreams |
| `tengu_onyx_plover.minSessions` | 5 | Minimum sessions since last dream |

To enable: add `"autoDreamEnabled": true` to `~/.claude/settings.json`.

There is no way to change `minHours` or `minSessions` locally -- these are controlled by the GrowthBook flag.

### Manual Trigger

The `/dream` skill (registered when `KAIROS` or `KAIROS_DREAM` features are enabled) runs the same consolidation prompt as a foreground skill rather than a background fork. Useful for forcing consolidation before the 24-hour timer.

## Cross-References

- [Memory Overview](memory_overview.md) -- Full memory system architecture
- [Memory Selector](memory_selector.md) -- How consolidated memories are recalled
- [Memory Limits](memory_limits.md) -- MEMORY.md caps that constrain Phase 4

## Interesting Findings

**Prompt cache efficiency.** The dream agent shares the parent's prompt cache via `cacheSafeParams`. Most of the cost is output tokens for memory writes, not input. The analytics events track `cache_read_input_tokens` to verify high cache hit ratios.

**Does NOT run on a timer.** AutoDream only fires at the end of a user turn -- it requires active use. It will never run on an idle machine. The `isForced()` function is hardcoded to `return false` (test-only override).

**Does NOT run in remote/cloud mode.** The `isGateOpen()` check explicitly excludes remote mode and KAIROS (assistant) mode, which has its own dream mechanism.

**The dream prompt instructs: "Look only for things you already suspect matter."** This means the quality of existing MEMORY.md content directly influences what the dream agent discovers. Well-organized existing memories guide the dream toward updating relevant topics rather than creating random new ones.
