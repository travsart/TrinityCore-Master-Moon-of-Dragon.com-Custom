---
description: "plan mode tools — EnterPlanModeTool ExitPlanModeV2Tool, read-only exploration, plan approval, allowedPrompts, agent plan_approval_request, no subagent restriction"
---

# Plan Mode Tools -- Arcanum Wiki

Covers `EnterPlanModeTool` and `ExitPlanModeV2Tool`.

## EnterPlanModeTool

### Purpose
Switches the session into plan mode -- a read-only exploration phase where the model investigates the codebase and designs an approach before writing code. No parameters are needed.

### Parameters
None (empty strict object schema).

### Execution Flow
1. Rejects if called from a subagent context.
2. Calls `handlePlanModeTransition()` to record the transition.
3. Updates `toolPermissionContext.mode` to `plan` via `prepareContextForPlanMode()`, which handles classifier activation when the user's default mode is `auto`.

### Tool Result
Returns instructions telling the model to:
- Explore the codebase
- Identify similar features and patterns
- Consider multiple approaches
- Use AskUserQuestion for clarification
- Call ExitPlanMode when ready

When the plan-mode interview phase is enabled (`isPlanModeInterviewPhaseEnabled()`), a simpler message is returned: "DO NOT write or edit any files except the plan file."

### Constraints
- Disabled when `--channels` is active (plan approval dialog needs the terminal)
- `shouldDefer: true` -- requires ToolSearch
- Concurrency safe, read only

---

## ExitPlanModeV2Tool

### Purpose
Presents the model's plan for user approval and exits plan mode, restoring the previous permission mode. Handles teammate plan approval via mailbox, auto-mode restoration with circuit breaker safety, and plan file persistence.

### Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `allowedPrompts` | array | No | Prompt-based permissions needed for implementation (e.g., "run tests") |

The SDK-facing input schema additionally includes `plan` (injected by `normalizeToolInput` from disk) and `planFilePath`.

### Execution Flow

#### For Teammates with `planModeRequired`:
1. Reads plan from disk via `getPlan()`
2. Sends `plan_approval_request` to team lead via mailbox
3. Updates task state to show "awaiting approval"
4. Returns with `awaitingLeaderApproval: true`

#### For Regular Sessions:
1. Reads plan from input (CCR web UI edit) or disk
2. If the user edited the plan via CCR/Ctrl+G, re-writes it to disk
3. Computes gate-off fallback: if `prePlanMode` was `auto` but the gate is now off (circuit breaker), falls back to `default` with a notification
4. Restores the pre-plan permission mode via `setAppState`
5. Handles auto-mode activation/deactivation and dangerous permission stripping/restoration
6. Sets flags for plan-mode-exit and auto-mode-exit attachments

### Key Implementation Details

#### Circuit Breaker Safety
When restoring to auto mode after plan mode, the tool checks if the auto-mode gate is still enabled. If the circuit breaker tripped during plan mode, the tool falls back to `default` mode instead of restoring `auto`, preventing bypass of safety mechanisms.

#### Plan Persistence
Plans are saved to disk at `getPlanFilePath()`. The CCR web UI can send edited plans via `permissionResult.updatedInput`. After receiving an edit, the tool writes it to disk and re-snapshots for remote environments.

#### Permission Restoration
When exiting plan mode:
- If restoring to `auto`: strips dangerous permissions
- If restoring to non-auto and permissions were stripped: restores them
- `prePlanMode` is cleared from context

### Constraints
- Validation rejects if not in plan mode (unless teammate)
- Disabled when `--channels` is active
- `shouldDefer: true`
- `requiresUserInteraction()`: true for non-teammates, false for all teammates

### Interesting Findings

1. The `hasExitedPlanModeInSession()` check is used in validation analytics to track how often the model calls ExitPlanMode when not in plan mode -- this happens after plan approval when the tool is still in the deferred tool list.

2. The plan result message includes a hint about `TeamCreateTool` if agent swarms are available: "consider using the TeamCreate tool to create a team and parallelize the work."

3. Plan labels distinguish user-edited plans ("Approved Plan (edited by user)") from model-generated ones, which feeds into the CCR Ultraplan flow's `extractApprovedPlan()` function.
