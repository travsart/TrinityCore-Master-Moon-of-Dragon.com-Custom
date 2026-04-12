---
description: "worktree tools — EnterWorktreeTool ExitWorktreeTool, git worktree isolation, agent file isolation, keep remove discard_changes, CWD switch, tmux session cleanup"
title: "Worktree Tools -- Arcanum Wiki"
tags: [tools, git-worktree, agent-file, cwd-switch, tmux-session]
---

# Worktree Tools -- Arcanum Wiki

Covers `EnterWorktreeTool` and `ExitWorktreeTool`.

## EnterWorktreeTool

### Purpose
Creates an isolated git worktree and switches the session's working directory into it. Used for agent isolation where file changes shouldn't affect the main working tree.

### Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `name` | string | No | Worktree name (validated: letters, digits, dots, underscores, dashes, max 64 chars). Random name generated if omitted. |

### Execution Flow
1. Validates not already in a worktree session.
2. Resolves to the main repo root (handles being inside an existing worktree).
3. Creates the worktree via `createWorktreeForSession()` using the session ID and slug.
4. Changes `process.cwd()` and internal CWD tracking to the worktree path.
5. Clears cached system prompt sections, memory file caches, and plans directory cache (all CWD-dependent).
6. Logs analytics event `tengu_worktree_created`.

### Output
Returns `worktreePath`, `worktreeBranch`, and a message explaining the worktree status.

### Constraints
- `shouldDefer: true`
- Only one worktree session at a time
- Name validation via `validateWorktreeSlug()` (regex enforcement)

---

## ExitWorktreeTool

### Purpose
Exits a worktree session created by `EnterWorktreeTool`, either keeping the worktree on disk or removing it.

### Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `action` | enum | Yes | `keep` (leave on disk) or `remove` (delete worktree and branch) |
| `discard_changes` | boolean | No | Required true when removing with uncommitted/unmerged changes |

### Execution Flow
1. Validates a worktree session exists (no-op if not).
2. Restores the original CWD.
3. If `action: 'remove'`: checks for uncommitted files or unmerged commits. Refuses unless `discard_changes: true`.
4. Clears CWD-dependent caches (system prompt sections, memory files, plans directory).
5. If a tmux session was attached: killed on remove, left running on keep.

### Constraints
- Only operates on worktrees created by EnterWorktreeTool in this session
- Will NOT touch manually created worktrees or worktrees from previous sessions
- `shouldDefer: true`

### Interesting Findings

1. The worktree is created under `.claude/worktrees/` inside the git repository, keeping it separate from the main working tree.

2. When entering a worktree from within an existing worktree, the tool first navigates to the main repo root via `findCanonicalGitRoot()`, ensuring worktree creation always works correctly.

3. The slug falls back to `getPlanSlug()` when no name is provided, linking the worktree name to the current plan context.
