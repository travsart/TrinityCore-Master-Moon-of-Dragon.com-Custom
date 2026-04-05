---
description: "permission modes — default auto plan acceptEdits bypassPermissions dontAsk, mode selection logic, mode capabilities, YOLO auto mode"
---

# Permission Modes -- Arcanum Wiki

## Overview

Claude Code defines 7 permission modes that control how tool invocations are handled. Five are user-visible ("external"), and two are internal (build-gated). Modes can be cycled during a session, and transitions are managed centrally by `transitionPermissionMode()` to handle dangerous rule stripping, state preservation, and mode-specific setup.

## How It Works

### External Modes (User-Visible)

| Mode | Symbol | Behavior |
|------|--------|----------|
| `default` | _(none)_ | Normal flow -- ask for each tool use that is not pre-approved |
| `plan` | pause icon | Read-only mode -- tools that write are blocked |
| `acceptEdits` | `>>` | Auto-allow file edits in the working directory |
| `bypassPermissions` | `>>` | Allow everything except deny rules, ask rules, and safety checks |
| `dontAsk` | `>>` | Convert all `ask` results to `deny` -- never prompt the user |

### Internal Modes

| Mode | Gate | Behavior |
|------|------|----------|
| `auto` | `TRANSCRIPT_CLASSIFIER` feature flag | AI classifier decides instead of user. Strips dangerous allow rules. Falls back to prompting after denial limits. |
| `bubble` | Internal | Not user-addressable. Used for fork subagents where permission prompts surface to the parent terminal. |

### Mode Transitions

`transitionPermissionMode()` handles all state transitions:

**Entering auto mode**: Activates `setAutoModeActive(true)`, then `stripDangerousPermissionsForAutoMode()` removes allow rules that would bypass the classifier (Bash(\*), python:\*, Agent, etc.). Stripped rules are stashed for restoration.

**Leaving auto mode**: Calls `restoreDangerousPermissions()` to re-add the stashed rules. Sets `setNeedsAutoModeExitAttachment(true)` so the model is notified.

**Entering plan mode from auto**: Preserves auto mode state via `prePlanMode` field and keeps dangerous rules stripped.

**Plan mode exit**: Clears `prePlanMode`, runs `setHasExitedPlanMode(true)`.

### bypassPermissions Availability

Can be disabled by:
- GrowthBook gate `tengu_disable_bypass_permissions_mode`
- Settings: `permissions.disableBypassPermissionsMode === 'disable'`
- Claude Code Remote (CCR): only `acceptEdits` and `plan` modes supported

When disabled, `--dangerously-skip-permissions` is silently downgraded to `default`.

### acceptEdits Behavior

In `acceptEdits` mode, file edits within the working directory (and additional directories) are auto-approved. File edits outside the working directory still prompt. Dangerous paths (`.git/`, `.claude/`, shell configs) still prompt regardless.

## Key Source Files

| File | Purpose |
|------|---------|
| `src/utils/permissions/PermissionMode.ts` | Mode configs, display, cycling |
| `src/utils/permissions/permissionSetup.ts` | `transitionPermissionMode()`, dangerous rule stripping |
| `src/types/permissions.ts` | Mode type definitions |

## Cross-References

- [Permissions Overview](overview.md) -- Full pipeline
- [YOLO Classifier](yolo_classifier.md) -- Auto mode details
- [Glob Patterns](glob_patterns.md) -- Rule syntax

## Interesting Findings

**Auto mode strips dangerous rules in-memory only.** Rules are never deleted from disk -- only the in-memory `alwaysAllowRules` context is modified. Leaving auto mode restores the original rules from the stash.

**Plan mode during auto preserves the auto state.** The `prePlanMode` field remembers the pre-plan mode so that exiting plan correctly returns to auto (with its dangerous rule stripping intact) rather than default.
