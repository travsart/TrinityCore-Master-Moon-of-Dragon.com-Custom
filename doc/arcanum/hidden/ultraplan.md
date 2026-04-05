---
description: "UltraPlan — advanced planning system, ccrSession.ts, keyword detection triggers, enhanced plan mode, multi-step task decomposition"
---

# UltraPlan -- Arcanum Wiki

## What Is This?

UltraPlan is a feature that teleports planning sessions to a remote Claude Code Runner (CCR) container in the cloud. When a user types the word "ultraplan" in their prompt, Claude Code intercepts it, launches a remote session, and polls for the plan to be approved through a browser-based UI. The remote container runs in "plan mode" where the model creates a structured plan that the user can review, edit, reject, or approve -- then optionally teleport back to the local terminal for execution.

There is also an "ultrareview" variant detected by the same keyword system.

## How It Works

### Keyword Detection (keyword.ts)

The system watches for the word "ultraplan" (case-insensitive) in user input. It uses sophisticated filtering to avoid false positives:

```typescript
export function findUltraplanTriggerPositions(text: string): TriggerPosition[] {
  return findKeywordTriggerPositions(text, 'ultraplan')
}
```

**Exclusions** (the keyword is NOT triggered when):
- Inside paired delimiters: backticks, double quotes, angle brackets, curly braces, square brackets, parentheses
- In path/identifier context: preceded/followed by `/`, `\`, or `-`, or followed by `.` + word char (e.g., `src/ultraplan/foo.ts`)
- Followed by `?` (questions about the feature should not invoke it)
- Input starts with `/` (slash commands bypass keyword detection)
- Smart single-quote handling: apostrophes in "let's ultraplan it's" still trigger, but `'ultraplan'` in quotes does not

When triggered, the keyword is replaced: "please ultraplan this" becomes "please plan this" -- preserving the user's casing of "plan".

### CCR Session Polling (ccrSession.ts)

Once the remote session is launched, `pollForApprovedExitPlanMode()` polls the CCR event stream every 3 seconds for up to the configured timeout. The system uses a stateful `ExitPlanModeScanner` class that ingests SDKMessage batches and tracks:

- `exitPlanCalls[]` -- ExitPlanMode tool_use IDs emitted by the remote model
- `results` -- corresponding tool_results (approved or rejected)
- `rejectedIds` -- plans the user rejected (user can iterate in the browser)
- Terminated state -- session crashed/errored

**Phase tracking** for UI:
```typescript
export type UltraplanPhase = 'running' | 'needs_input' | 'plan_ready'
```

**Two approval paths**:
1. **Approved in browser** (`kind: 'approved'`): Plan text extracted from `## Approved Plan:` marker in the tool_result. Execution stays remote.
2. **Teleported back** (`kind: 'teleport'`): User clicked "teleport back to terminal" in the browser PlanModal. The browser sends a rejection with the plan text embedded after the `__ULTRAPLAN_TELEPORT_LOCAL__` sentinel. Execution moves to the local terminal.

The poll has robust error handling: 5 consecutive transient failures before giving up, and distinct error reasons (`terminated`, `timeout_pending`, `timeout_no_plan`, `extract_marker_missing`, `network_or_unknown`, `stopped`).

### Precedence Logic

The scanner uses strict precedence: `approved > terminated > rejected > pending > unchanged`. A batch can contain both an approved tool_result AND a subsequent crash -- the approved plan is real and preserved.

## Feature Gating

- Keyword detection is always active (no feature flag visible in the keyword module)
- Remote session creation requires OAuth authentication
- Plan mode is set via `set_permission_mode` control_request in the CreateSession events
- The `ExitPlanModeV2Tool` must be available on the remote

## User-Facing Behavior

1. User types "ultraplan" anywhere in their prompt (not in quotes, paths, or slash commands)
2. PromptInput rainbow-highlights the word and shows a notification
3. On submit, the keyword is replaced with "plan" and a remote CCR session is created
4. A pill/detail-view appears showing the phase: running, needs_input, plan_ready
5. The remote model creates a plan and calls ExitPlanMode
6. User reviews the plan in the browser, can reject (iterate) or approve
7. On approve: plan is executed remotely
8. On teleport: plan is sent back to the local terminal for execution

## Key Source Files

| File | Purpose |
|------|---------|
| `src/utils/ultraplan/keyword.ts` | Keyword detection with sophisticated false-positive filtering |
| `src/utils/ultraplan/ccrSession.ts` | Remote session polling, ExitPlanModeScanner state machine |

## Configuration

- `POLL_INTERVAL_MS = 3000` -- polling frequency
- `MAX_CONSECUTIVE_FAILURES = 5` -- transient error tolerance
- `ULTRAPLAN_TELEPORT_SENTINEL = '__ULTRAPLAN_TELEPORT_LOCAL__'` -- marker for teleport-back

## Interesting Findings

1. **The keyword system is extremely careful about false positives.** It tracks paired delimiters with nesting, handles apostrophes vs single quotes, recognizes file paths, and even skips `?` suffixes (questions about ultraplan). This level of sophistication suggests users were hitting false positives during development.

2. **The rejection cycle is first-class.** Users can reject a plan, iterate with the model in the browser, and submit again. The scanner tracks all rejections and only looks at the newest non-rejected ExitPlanMode call. This suggests plan iteration is a core workflow, not an edge case.

3. **The "teleport back" flow** is clever: it abuses the rejection mechanism by embedding the plan text in an error tool_result with a sentinel string. This avoids needing a separate API endpoint for the teleport action.

4. **`quietIdle` detection** accounts for CCR status lag: only trust "idle" when no new events arrived in the same poll. Events flowing means the session is working regardless of what the status snapshot says.

5. **The `needs_input` phase** handles the case where the remote model asks a clarifying question before creating the plan. The user can reply in the browser. This means ultraplan supports multi-turn planning conversations, not just single-shot plan generation.
