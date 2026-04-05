---
description: "YOLO classifier — permission auto-accept, dangerous pattern detection, consecutive denial tracking, auto mode fallback, bashClassifier"
---

# YOLO Classifier (Auto Mode) -- Arcanum Wiki

## Overview

Auto mode replaces user permission prompts with an AI classifier that decides whether each tool use is safe. Gated behind the `TRANSCRIPT_CLASSIFIER` feature flag, it implements a cascade of fast paths (acceptEdits simulation, safe-tool allowlist) before falling back to actual classifier inference. The system includes denial tracking to prevent infinite loops and a "fail closed" default where unavailable classifiers result in denial.

## How It Works

### Auto Mode Pipeline

When `hasPermissionsToUseTool()` receives an `ask` result in auto mode (`src/utils/permissions/permissions.ts:518-927`):

```
1. Non-classifier-approvable safety check? -> keep ASK (or DENY if headless)
2. Tool requires user interaction?         -> keep ASK
3. PowerShell without POWERSHELL_AUTO_MODE? -> keep ASK
4. Would acceptEdits mode allow this?      -> ALLOW (skip classifier)
5. Tool on safe allowlist?                 -> ALLOW (skip classifier)
6. Run YOLO classifier                     -> ALLOW or DENY
7. Classifier unavailable?                 -> DENY (fail closed) or ASK (fail open)
8. Denial limit exceeded?                  -> fall back to ASK for user review
```

### Fast Paths (Skip Classifier)

**acceptEdits simulation** (step 4): Simulates `tool.checkPermissions()` with an acceptEdits mode context. If the tool would be auto-allowed in acceptEdits mode, the classifier is skipped. Agent and REPL tools are excluded from this fast path to prevent silently bypassing classification for dangerous operations.

**Safe-tool allowlist** (step 5): `classifierDecisionModule.isAutoModeAllowlistedTool()` checks a built-in list of inherently safe tools. These skip classification entirely.

### Classifier Result

```typescript
type YoloClassifierResult = {
  shouldBlock: boolean
  reason: string
  unavailable?: boolean
  transcriptTooLong?: boolean
  stage?: 'fast' | 'thinking'  // 2-stage XML classifier
  model: string
  usage?: ClassifierUsage
  durationMs?: number
}
```

- `shouldBlock: true` -> deny with classifier reason
- `shouldBlock: false` -> allow
- `unavailable: true` -> check `tengu_iron_gate_closed` gate: true = deny (fail closed), false = fall back to prompting (fail open)
- `transcriptTooLong: true` -> fall back to prompting (deterministic error)

### Denial Tracking

From `src/utils/permissions/denialTracking.ts`:

```typescript
export const DENIAL_LIMITS = {
  maxConsecutive: 3,    // 3 blocked in a row -> fall back to prompt
  maxTotal: 20,         // 20 total blocks this session -> fall back to prompt
}
```

State transitions:
- On classifier DENY: `consecutiveDenials++`, `totalDenials++`
- On any ALLOW (including rule-based): `consecutiveDenials = 0`
- Limits exceeded: convert deny to ask so user can review the transcript
- Headless mode with limits exceeded: `throw AbortError` to abort the agent

Async subagents use `context.localDenialTracking` (mutable in-place) instead of `appState.denialTracking`, preventing denial state from leaking between agents.

### Dangerous Permission Stripping

When entering auto mode, `stripDangerousPermissionsForAutoMode()` removes allow rules that would bypass the classifier. Patterns include:

**Cross-platform code execution**: python, node, deno, tsx, ruby, perl, php, lua, npx, bunx, npm/yarn/pnpm/bun run, bash, sh, ssh

**Bash-specific**: zsh, fish, eval, exec, env, xargs, sudo, plus ant-only (gh, curl, wget, git, kubectl, aws, gcloud)

**PowerShell-specific**: pwsh, cmd, wsl, iex, invoke-expression, start-process, new-pssession, add-type, new-object, plus `.exe` variants

**Agent rules**: ANY Agent allow rule is always dangerous -- it would auto-approve sub-agent spawns before the classifier can evaluate the sub-agent's prompt.

Five match shapes are tested per pattern: exact (`"python"`), prefix (`"python:*"`), wildcard (`"python*"`), space-wildcard (`"python *"`), flag (`"python -*"`).

Stripped rules are stashed in `strippedDangerousRules` and restored when leaving auto mode. Rules are modified in-memory only -- never deleted from disk.

## Key Source Files

| File | Purpose |
|------|---------|
| `src/utils/permissions/permissions.ts` | Auto mode pipeline (lines 518-927) |
| `src/utils/permissions/denialTracking.ts` | `DENIAL_LIMITS`, `recordDenial()`, `shouldFallbackToPrompting()` |
| `src/utils/permissions/dangerousPatterns.ts` | `CROSS_PLATFORM_CODE_EXEC`, `DANGEROUS_BASH_PATTERNS` |
| `src/utils/permissions/permissionSetup.ts` | `stripDangerousPermissionsForAutoMode()`, `restoreDangerousPermissions()` |

## Configuration

Auto mode is activated via the `TRANSCRIPT_CLASSIFIER` feature flag (build-gated). The `tengu_iron_gate_closed` GrowthBook gate controls fail-closed vs fail-open behavior.

## Cross-References

- [Permissions Overview](overview.md) -- Full evaluation pipeline
- [Permission Modes](modes.md) -- Mode transitions and auto mode entry/exit
- [Glob Patterns](glob_patterns.md) -- Rule matching syntax

## Interesting Findings

**Fail closed is the security-first default.** When the classifier is unavailable and `tengu_iron_gate_closed` is true, the system denies. This prevents a classifier outage from silently opening the security gate.

**The 2-stage classifier.** The `stage` field indicates whether the result came from a fast first pass or a more expensive thinking pass, enabling tiered decisions where obvious cases are handled quickly.

**The acceptEdits fast path is the biggest cost saver.** Most file edits within the working directory pass through this fast path without ever hitting the classifier, avoiding the API call entirely.
