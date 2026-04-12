---
description: "navigation commands — /add-dir add working directory, /files list tracked files, /teleport remote session jump"
title: "Navigation Commands -- Arcanum Wiki"
tags: [commands, files-list]
---

# Navigation Commands -- Arcanum Wiki

## Overview

These commands control Claude Code's working context -- which directories it operates in and which files are loaded. They let users manage the scope of what Claude can see and modify.

## Commands

### /add-dir
- **Arguments**: `<path>`
- **What it does**: Adds a new working directory to the current session. Claude Code can operate in multiple directories simultaneously. This command adds a directory to the list, making its files accessible for reading, editing, and tool operations. Useful when working across multiple projects or when a relevant directory is outside the initial working directory.
- **Feature gating**: None -- always available.
- **Key code**:
```typescript
const addDir = {
  type: 'local-jsx',
  name: 'add-dir',
  description: 'Add a new working directory',
  argumentHint: '<path>',
}
```

---

### /files
- **Arguments**: None
- **What it does**: Lists all files currently loaded in the conversation context. Shows which files Claude has read or been given access to during the session. Useful for understanding what the model "knows about" at any given point.
- **Feature gating**: Only enabled for Anthropic employees (`USER_TYPE === 'ant'`). Available in non-interactive mode.
- **Key code**:
```typescript
const files = {
  type: 'local',
  name: 'files',
  description: 'List all files currently in context',
  isEnabled: () => process.env.USER_TYPE === 'ant',
  supportsNonInteractive: true,
}
```
- **Notes**: This is an ant-only debugging command. Regular users cannot see what files are in context (they can use `/context` for a higher-level view).

---

### /teleport
- **Arguments**: Unknown (stubbed)
- **What it does**: STUBBED OUT. Was the command for "teleporting" a local session to a remote cloud environment. The teleport functionality still exists in the codebase (used by `/review` ultrareview and `/plan` ultraplan) but the standalone slash command is disabled.
- **Feature gating**: `isEnabled: () => false`, `isHidden: true`
- **Notes**: Teleport is referenced extensively in other commands (ultraplan, ultrareview) via `teleportToRemote()` utility function, but the standalone `/teleport` command is not available.

## Hidden/Undocumented Commands

- **/files** -- Ant-only, never shown to regular users.
- **/teleport** -- Stubbed out. The teleport infrastructure exists but the command interface is disabled.
