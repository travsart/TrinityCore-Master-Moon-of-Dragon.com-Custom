---
description: "CronTools — CronCreate CronList CronDelete, scheduled prompts, recurring tasks, one-shot reminders, 5-field cron expressions, session-scoped, jitter, 3-day auto-expire"
title: "Cron Tools -- Arcanum Wiki"
tags: [tools, scheduled-prompts, recurring-tasks, one-shot-reminders, 5-field-cron, session-scoped, jitter, 3-day-auto-expire]
---

# Cron Tools -- Arcanum Wiki

Covers `CronCreateTool`, `CronListTool`, and `CronDeleteTool`.

## CronCreateTool

### Purpose
Schedules a recurring or one-shot prompt using standard 5-field cron expressions. Supports both in-memory (session-scoped) and durable (survives restarts) modes.

### Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `cron` | string | Yes | Standard 5-field cron expression: "M H DoM Mon DoW" |
| `prompt` | string | Yes | The prompt to enqueue at each fire time |
| `recurring` | boolean | No | true (default) = fire on every match; false = fire once then auto-delete |
| `durable` | boolean | No | true = persist to `.claude/scheduled_tasks.json`; false (default) = in-memory only |

### Key Details
- Maximum 50 jobs (`MAX_JOBS = 50`)
- Validates cron expression via `parseCronExpression()`
- Checks that the expression matches at least one date in the next year
- Durable jobs are gated by `isDurableCronEnabled()` feature flag
- Recurring jobs auto-expire after `DEFAULT_MAX_AGE_DAYS`
- `shouldDefer: true`
- Enabled only when `isKairosCronEnabled()` returns true

### Cron Expression Examples
- `*/5 * * * *` = every 5 minutes
- `30 14 28 2 *` = Feb 28 at 2:30pm local time (once)
- `0 9 * * 1-5` = weekdays at 9am

---

## CronListTool

### Purpose
Lists all scheduled cron tasks (both in-memory and durable).

### Key Details
- Returns task ID, cron expression, human-readable schedule, recurring flag, and prompt
- `shouldDefer: true`

---

## CronDeleteTool

### Purpose
Deletes a scheduled cron task by ID.

### Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `id` | string | Yes | Task ID to delete |

### Key Details
- Removes from both in-memory and durable storage
- `shouldDefer: true`

## Interesting Findings

1. The durable mode persists to `.claude/scheduled_tasks.json`, meaning scheduled tasks can survive Claude Code restarts. This enables "remind me at X" use cases across sessions.

2. Non-recurring tasks auto-delete after firing once, implementing a clean "one-shot reminder" pattern. The cron expression for these is typically a pinned minute/hour/day.

3. The `DEFAULT_MAX_AGE_DAYS` auto-expiry prevents forgotten recurring tasks from running indefinitely. Users must actively manage long-running schedules.
