---
description: "task tools — TaskCreate TaskGet TaskList TaskUpdate TaskOutput TaskStop, todo list, task status pending in_progress completed, blocking dependencies, owner assignment"
---

# Task Tools -- Arcanum Wiki

Covers `TaskCreateTool`, `TaskGetTool`, `TaskListTool`, `TaskUpdateTool`, `TaskOutputTool`, and `TaskStopTool`.

## Overview

The Task tools provide a structured task management system (todo list) that the model can use to track work items. Tasks have IDs, subjects, descriptions, statuses, owners, and blocking relationships. The system is enabled via `isTodoV2Enabled()`.

---

## TaskCreateTool

### Purpose
Creates a new task in the task list.

### Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `subject` | string | Yes | Brief task title |
| `description` | string | Yes | What needs to be done |
| `activeForm` | string | No | Present continuous form for spinner (e.g., "Running tests") |
| `metadata` | Record | No | Arbitrary metadata to attach |

### Key Details
- Runs `TaskCreated` hooks after creation; if any hook returns a blocking error, the task is deleted
- Auto-expands the task list UI panel after creation
- `shouldDefer: true`
- Concurrency safe
- Returns task ID and subject

---

## TaskGetTool

### Purpose
Retrieves details of a specific task by ID.

### Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `id` | string | Yes | Task ID to look up |

### Key Details
- `shouldDefer: true`
- Concurrency safe, read only
- Returns full task details including status, description, owner, blocks/blockedBy

---

## TaskListTool

### Purpose
Lists all tasks in the current task list, optionally filtered by status.

### Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `status` | enum | No | Filter: `pending`, `in_progress`, `completed` |

### Key Details
- `shouldDefer: true`
- Concurrency safe, read only
- Returns array of task summaries

---

## TaskUpdateTool

### Purpose
Updates an existing task's status, description, or metadata.

### Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `id` | string | Yes | Task ID to update |
| `status` | enum | No | New status: `pending`, `in_progress`, `completed` |
| `description` | string | No | Updated description |
| `metadata` | Record | No | Updated metadata |

### Key Details
- `shouldDefer: true`
- Concurrency safe
- Runs `TaskUpdated` hooks on status changes

---

## TaskOutputTool

### Purpose
Reads output from a background task (bash command or agent running in background). This is how the model checks on work that was launched with `run_in_background: true`.

### Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `task_id` | string | Yes | Background task ID |

### Key Details
- Reads from the task's output file on disk
- Streams progress updates showing output accumulation
- Returns the task's stdout/stderr, completion status, and exit code
- `shouldDefer: true`

---

## TaskStopTool

### Purpose
Stops (kills) a running background task.

### Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `task_id` | string | Yes | Background task ID to stop |

### Key Details
- Sends abort signal to the task's abort controller
- `shouldDefer: true`

## Interesting Findings

1. TaskCreateTool auto-expands the task list UI panel (`expandedView: 'tasks'`) whenever a task is created, ensuring the user sees the task list.

2. The task system supports blocking relationships (`blocks`, `blockedBy`) but these are set via metadata, not dedicated parameters -- suggesting the model manages dependencies through its own reasoning.

3. TaskOutputTool is the critical bridge between `run_in_background` bash commands and the model's ability to check their results. Without it, background tasks would be fire-and-forget.

4. All Task tools use `shouldDefer: true` and are gated by `isTodoV2Enabled()`, meaning they're only available when the task management feature is active.
