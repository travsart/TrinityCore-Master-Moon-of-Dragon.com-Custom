---
description: "swarm messaging — mailbox system, inter-agent messages, SendMessage broadcast DM, shutdown_request, plan_approval, message queuing and delivery"
---

# Swarm Messaging -- Arcanum Wiki

## Overview

All inter-agent communication in the swarm system flows through file-based mailboxes, regardless of backend type. The `SendMessageTool` exposes messaging to agents, supporting direct messages, broadcasts, shutdown protocols, and plan approval flows. A parallel permission bridge system allows in-process teammates to use the leader's permission UI.

## How It Works

### Message Types

The SendMessageTool accepts three structured message types plus plain text:

| Type | Fields | Purpose |
|------|--------|---------|
| `shutdown_request` | reason? | Leader requests teammate to stop |
| `shutdown_response` | request_id, approve, reason? | Teammate approves/rejects shutdown |
| `plan_approval_response` | request_id, approve, feedback? | Leader approves/rejects plan |
| Plain text | summary (required) | Regular message with 5-10 word preview |

### Routing

| `to` value | Behavior |
|-----------|----------|
| `"researcher"` | DM to teammate by name |
| `"*"` | Broadcast to all members except sender |
| `"uds:/path.sock"` | Cross-session local peer (feature-gated) |
| `"bridge:session_..."` | Remote Control cross-machine peer |

### Message Flow

**DM**: The sender writes to the recipient's mailbox file at `~/.claude/teams/{team}/mailboxes/{recipient}.json`. Each message includes `from`, `text`, `summary`, `timestamp`, and optional `color`.

**Broadcast**: Reads the team file, iterates all members except sender, writes to each member's mailbox individually.

### Shutdown Protocol (Two-Phase Handshake)

1. Leader sends `shutdown_request` with reason to teammate's mailbox
2. Teammate's model processes the request and decides
3. If approved: writes `shutdown_approved` to leader mailbox, then aborts (in-process) or gracefully shuts down (pane-based)
4. If rejected: writes `shutdown_rejected` with reason, continues working

### Plan Mode Approval

When `planModeRequired` is set, teammates must submit a plan before implementing:
1. Teammate submits plan text to leader
2. Leader reviews and sends `plan_approval_response`
3. On approval, the leader's current permission mode is inherited (but if leader is in `plan` mode, the teammate gets `default` instead)

### Permission Bridge (In-Process)

For in-process teammates, a module-level bridge lets the REPL register its permission UI functions so teammates can share the same permission dialog as the leader:

```typescript
// leaderPermissionBridge.ts
let registeredSetter: SetToolUseConfirmQueueFn | null = null
let registeredPermissionContextSetter: SetToolPermissionContextFn | null = null
```

The teammate's permission prompt appears in the leader's UI with a colored worker badge. Permission updates flow back with `preserveMode: true` to prevent workers' transformed modes from leaking to the coordinator.

### Permission Sync (Cross-Process)

For tmux/iTerm2 teammates, permissions use a file-based system:
1. Worker writes request to `~/.claude/teams/{team}/permissions/pending/{id}.json`
2. Leader reads pending directory
3. Leader resolves: moves from `pending/` to `resolved/`
4. Worker polls `resolved/` at 500ms intervals

File locking via `lockfile.lock()` prevents race conditions.

### Idle Polling Priority

`waitForNextPromptOrShutdown()` polls every 500ms with strict priority:

1. In-memory pending messages (from transcript viewing)
2. Shutdown requests (scanned across ALL unread messages)
3. Team-lead messages (not starved by peer chatter)
4. Peer messages (FIFO)
5. Unclaimed tasks (from team task list)

## Key Source Files

| File | Purpose |
|------|---------|
| `src/tools/SendMessageTool/SendMessageTool.ts` | Message tool with routing and protocols |
| `src/utils/swarm/permissionSync.ts` | File-based permission relay |
| `src/utils/swarm/leaderPermissionBridge.ts` | In-process permission bridge |
| `src/utils/swarm/teammateInit.ts` | Teammate initialization and idle hooks |

## Cross-References

- [Swarm Overview](swarm_overview.md) -- Architecture context
- [Swarm Backends](swarm_backends.md) -- Backend-specific details
- [Teams and Tasks](teams_and_tasks.md) -- Task coordination

## Interesting Findings

**Sandbox permission relay uses structured file exchange.** Network access requests from sandboxed environments follow the same mailbox pattern with dedicated `sandbox-{timestamp}-{random}` request IDs and host-level granularity.

**Leader messages are never starved.** The polling priority explicitly puts team-lead messages above peer messages, ensuring the leader's intent is always processed first even when teammates are chattering.
