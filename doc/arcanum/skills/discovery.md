---
description: "skill discovery — dynamic file edit walk-up search, .claude/skills/ directories, skill hot-reload, file change triggers, cache invalidation"
---

# Skill Dynamic Discovery -- Arcanum Wiki

## Overview

When Claude Code reads, writes, or edits files, it can discover `.claude/skills/` directories nested within the project tree. This dynamic discovery mechanism activates skills that were not visible at session start, enabling monorepo-style projects where different subdirectories have their own skill sets.

## How It Works

### Discovery Process

`discoverSkillDirsForPaths()` is called when file tools interact with paths:

1. For each file path touched, walk up from the file's parent directory toward CWD (exclusive of CWD itself, since CWD-level skills load at startup)
2. At each level, check for `.claude/skills/` directory existence
3. Skip gitignored directories via `isPathGitignored()` to prevent untrusted dependency skills
4. Track checked paths in `dynamicSkillDirs` Set to avoid repeated `stat()` calls
5. Sort results deepest-first so skills closer to the file take precedence

### Dynamic vs Base Skills

Dynamic skills are stored in a separate `dynamicSkills` Map and merged into `getCommands()` results at query time. They are inserted before built-in commands but after all other sources. Name-based deduplication ensures base commands win on collision.

### Change Notification

When new dynamic skills are loaded, `skillsLoaded.emit()` fires a signal. The `skillChangeDetector` subscribes to clear memoization caches, ensuring new skills appear immediately in the model's listing.

### Live Reload via Chokidar

The `skillChangeDetector` watches skill directories for changes using Chokidar (with polling on Bun to avoid a known deadlock). Changes are debounced at 300ms to prevent cascading reloads during bulk operations. On change:
1. Fire ConfigChange hook for the batch
2. Clear all skill and command caches
3. Reset sent-skill-names tracker
4. Emit `skillsChanged` signal

## Key Source Files

| File | Purpose |
|------|---------|
| `src/skills/loadSkillsDir.ts` | `discoverSkillDirsForPaths()`, `addSkillDirectories()` |
| `src/utils/skills/skillChangeDetector.ts` | Chokidar watcher, debounced reload |

## Cross-References

- [Skills Overview](overview.md) -- Full architecture
- [Conditional Activation](conditional_activation.md) -- The `paths:` mechanism

## Interesting Findings

**Gitignored directories are skipped for security.** Dynamic discovery explicitly skips gitignored paths like `node_modules/pkg/.claude/skills/` to prevent untrusted dependency packages from injecting skills silently.
