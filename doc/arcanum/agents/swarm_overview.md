---
description: "multi-agent swarm architecture — team spawning, leader permission bridge, 50-message UI cap, teammate model, reconnection, parallel agent execution"
---

# Multi-Agent Swarm Architecture -- Arcanum Wiki

## Overview

Claude Code's swarm system allows a single "leader" session to spawn and coordinate multiple "teammate" agents, each running as an independent agent loop. The system supports three execution backends (tmux, iTerm2, in-process) behind a unified `TeammateExecutor` interface, with communication handled through file-based mailboxes. The architecture is leader-centric: one leader creates the team, manages permissions, coordinates shutdown, and teammates cannot create teams.

## How It Works

### Team Configuration

Every team is represented by a JSON config file at `~/.claude/teams/{team-name}/config.json`. The `TeamFile` structure includes team metadata, lead agent ID, and an array of members with their agent IDs, prompts, models, colors, worktree paths, subscriptions, and backend types.

### Team Creation Lifecycle

When the leader invokes `TeamCreateTool`:
1. Validate no existing team (one team per leader)
2. Generate unique team name if collision exists
3. Create lead agent ID as `team-lead@{teamName}`
4. Write team config file to `~/.claude/teams/{team-name}/config.json`
5. Register for session cleanup (destroyed on leader exit)
6. Create task list directory at `~/.claude/tasks/{team-name}/`
7. Update AppState with team context

### Agent Lifecycle State Machine

```
[SPAWN]
  |
  v
RUNNING  <-- (new message / task claimed)
  |
  | runAgent() completes
  v
IDLE  -- sends idle notification to leader
  |
  +--> [mailbox message]   --> RUNNING
  +--> [unclaimed task]    --> RUNNING
  +--> [pending user msg]  --> RUNNING
  +--> [shutdown request]  --> RUNNING (model decides)
  |       |
  |       +--> approve --> [EXIT]
  |       +--> reject  --> IDLE
  +--> [abort signal]      --> [EXIT]
  |
[KILLED]  -- leader force-kills via AbortController
```

### Teammate System Prompt

Every teammate receives the standard system prompt plus an addendum:

```
IMPORTANT: You are running as an agent in a team. To communicate with anyone:
- Use the SendMessage tool with `to: "<name>"` for specific teammates
- Use the SendMessage tool with `to: "*"` sparingly for broadcasts
Just writing a response in text is not visible to others -- you MUST use SendMessage.
```

Team-essential tools (SendMessage, TeamCreate, TeamDelete, TaskCreate/Get/List/Update) are always injected even when a custom agent definition specifies an explicit tool list.

### Cleanup

Session-end cleanup (`cleanupSessionTeams()`) handles teams that were created but never explicitly deleted: kill orphaned panes, destroy worktrees, remove team and task directories.

## Key Source Files

| File | Purpose |
|------|---------|
| `src/utils/swarm/teamHelpers.ts` | Team config, creation, deletion, cleanup |
| `src/utils/swarm/constants.ts` | Session names, env vars |
| `src/tools/TeamCreateTool/TeamCreateTool.ts` | Team creation logic |
| `src/tools/TeamDeleteTool/TeamDeleteTool.ts` | Team deletion with safety checks |
| `src/tools/SendMessageTool/SendMessageTool.ts` | Inter-agent messaging |

## Cross-References

- [Swarm Backends](swarm_backends.md) -- Tmux, iTerm2, InProcess backends
- [Swarm Messaging](swarm_messaging.md) -- Mailbox system and message routing
- [Teams and Tasks](teams_and_tasks.md) -- Task coordination

## Interesting Findings

**One whale session launched 292 agents in 2 minutes and hit 36.8GB RSS.** This is why in-process teammate messages are capped at 50 in the UI mirror (`TEAMMATE_MESSAGES_UI_CAP = 50`).

**Teammate mode is captured once at startup.** `captureTeammateModeSnapshot()` freezes the mode (`auto`, `tmux`, or `in-process`) and it never changes during the session, even if the user modifies global config.

**TeamDeleteTool refuses to delete teams with active members.** The safety check ensures all non-lead members have `isActive === false` before allowing deletion.
