---
description: "team tools — TeamCreateTool TeamDeleteTool SendMessageTool, multi-agent swarm, team_name, broadcast, shutdown_request, plan_approval_response, teammate coordination"
---

# Team Tools -- Arcanum Wiki

Covers `TeamCreateTool`, `TeamDeleteTool`, and `SendMessageTool`.

## TeamCreateTool

### Purpose
Creates a multi-agent swarm team, establishing the team lead identity and team file for coordination. This is the prerequisite for spawning teammates via AgentTool.

### Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `team_name` | string | Yes | Name for the new team |
| `description` | string | No | Team description/purpose |
| `agent_type` | string | No | Type/role of the team lead |

### Key Details
- Only enabled when `isAgentSwarmsEnabled()` returns true
- Generates unique team name if provided name already exists (via `generateWordSlug()`)
- Creates a team file at `getTeamFilePath()` with leader identity
- Assigns a unique color to the team lead via `assignTeammateColor()`
- Registers team for session cleanup
- Resets and configures the task list for the team
- `shouldDefer: true`

### Team File Structure
The team file (JSON) contains:
- `name`: Team name
- `description`: Team purpose
- `createdAt`: Timestamp
- `leader`: Agent ID, name, model, color
- `members`: Array of member records (added when teammates spawn)

---

## TeamDeleteTool

### Purpose
Deletes a team and all its associated resources (teammates, panes, team file).

### Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `team_name` | string | Yes | Name of the team to delete |

### Key Details
- Only enabled when `isAgentSwarmsEnabled()` returns true
- Kills all teammate panes/processes
- Removes the team file
- Cleans up team context from AppState

---

## SendMessageTool

### Purpose
Sends a message to a specific teammate by name via the mailbox system. Used for inter-agent communication within a team.

### Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `to` | string | Yes | Name of the teammate to message |
| `text` | string | Yes | Message content |

### Key Details
- Writes to the recipient's mailbox via `writeToMailbox()`
- The recipient's inbox poller picks up the message on its next check
- Messages include sender name and timestamp
- `shouldDefer: true`

## Interesting Findings

1. TeamCreateTool uses `generateWordSlug()` for unique names when there's a collision, suggesting a dictionary-based slug generator (e.g., "bright-falcon", "swift-river").

2. The team system supports three backend types for teammates: tmux panes, iTerm2 native panes, and in-process (AsyncLocalStorage). The backend is detected automatically at team creation time.

3. The mailbox system is the sole communication mechanism between teammates -- there's no shared memory or direct function calls between agents. This design ensures clean isolation.

4. Team lead identity uses the constant `TEAM_LEAD_NAME` from `src/utils/swarm/constants.ts`, establishing a well-known name that teammates can always address messages to.
