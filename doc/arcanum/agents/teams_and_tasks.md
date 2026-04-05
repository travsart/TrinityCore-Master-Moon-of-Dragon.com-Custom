---
description: "teams and tasks — TeamCreate TeamDelete, task list coordination, task ownership, blocking dependencies, team config files, teammate discovery"
---

# Teams and Tasks -- Arcanum Wiki

## Overview

The team system provides coordination infrastructure for multi-agent work: team-wide task lists, per-teammate configuration, allowed path management, and lifecycle hooks. Tasks are stored on disk and claimed by teammates via atomic file operations, enabling parallel work distribution without conflicts.

## How It Works

### Team File Structure

The team config at `~/.claude/teams/{team-name}/config.json` tracks:

```typescript
type TeamFile = {
  name: string
  description?: string
  createdAt: number
  leadAgentId: string          // "team-lead@team-name"
  leadSessionId?: string       // Leader's session UUID
  teamAllowedPaths?: TeamAllowedPath[]  // Paths all teammates can edit
  members: Array<{
    agentId: string            // "researcher@team-name"
    name: string               // "researcher"
    prompt?: string            // Task instructions
    color?: string
    planModeRequired?: boolean
    joinedAt: number
    cwd: string
    worktreePath?: string
    subscriptions: string[]
    backendType?: BackendType
    isActive?: boolean         // false when idle
    mode?: PermissionMode
  }>
}
```

### Task List System

Each team has a task directory at `~/.claude/tasks/{team-name}/`. Task files contain structured JSON with task descriptions, status, assigned agent, and completion metadata.

The task claim flow:
1. Teammate calls `tryClaimNextTask()` during its idle poll loop
2. Atomic file read + write prevents double-claiming
3. Claimed tasks are locked to the claiming agent's ID
4. On completion, the task status is updated to `resolved`, `blocked`, or `failed`

### Team-Wide Allowed Paths

`teamAllowedPaths` in the team config grants edit permissions across all teammates without individual permission prompts. Applied during teammate initialization via `addSessionRules()`.

### Teammate Initialization

At spawn, each teammate:
1. Applies team-wide allowed paths as permission rules
2. Registers a Stop hook that sends an idle notification to the leader and marks itself as inactive in the team config

```typescript
addFunctionHook(setAppState, sessionId, 'Stop', '',
    async (messages, _signal) => {
        void setMemberActive(teamName, agentName, false)
        const notification = createIdleNotification(agentName, {
            idleReason: 'available',
            summary: getLastPeerDmSummary(messages),
        })
        await writeToMailbox(leadAgentName, { from: agentName, text: jsonStringify(notification) })
        return true
    },
    'Failed to send idle notification',
    { timeout: 10000 }
)
```

### In-Process Task State

In-process teammates register as tasks in `AppState.tasks`:

```typescript
type InProcessTeammateTaskState = {
  type: 'in_process_teammate'
  identity: TeammateIdentity     // agentId, name, team, color
  prompt: string
  abortController?: AbortController
  currentWorkAbortController?: AbortController
  awaitingPlanApproval: boolean
  permissionMode: PermissionMode
  isIdle: boolean
  shutdownRequested: boolean
  messages?: Message[]           // UI mirror, capped at 50
  pendingUserMessages: string[]
  onIdleCallbacks?: Array<() => void>
}
```

### Reconnection

Two scenarios are handled by `reconnection.ts`:
- **Fresh spawn**: `computeInitialTeamContext()` reads CLI args and produces initial team context
- **Resumed session**: `initializeTeammateContextFromSession()` rebuilds team context from the stored transcript and team file on disk

## Key Source Files

| File | Purpose |
|------|---------|
| `src/utils/swarm/teamHelpers.ts` | Team config CRUD, cleanup, allowed paths |
| `src/tasks/InProcessTeammateTask/types.ts` | In-process task state types |
| `src/tasks/InProcessTeammateTask/InProcessTeammateTask.tsx` | Task UI component |
| `src/utils/swarm/reconnection.ts` | Session resume and fresh spawn |
| `src/utils/swarm/teammateInit.ts` | Initialization hooks |

## Cross-References

- [Swarm Overview](swarm_overview.md) -- Architecture context
- [Swarm Messaging](swarm_messaging.md) -- Communication between teammates

## Interesting Findings

**Kill cleanup is thorough.** `killInProcessTeammate()` performs: abort controller, unregister cleanup, invoke idle callbacks, remove from team context, set status to 'killed', clear messages to last-only, remove from team file, evict task output, and schedule task eviction after display timeout.

**Worktree cleanup is part of team deletion.** `cleanupTeamDirectories()` reads the team file, finds all worktree paths, destroys them via `git worktree remove --force`, then removes the team and task directories.
