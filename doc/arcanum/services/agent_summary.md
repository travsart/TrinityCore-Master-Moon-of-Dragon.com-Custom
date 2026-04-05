---
description: "AgentSummary service — subagent result summarization, agent output condensing, context-efficient agent responses"
---

# Agent Summary Service -- Arcanum Wiki

## What Is This?

The Agent Summary service generates short progress labels for background subagents running in coordinator mode. Every ~30 seconds, it forks the subagent's conversation to produce a 3-5 word present-tense summary (e.g., "Reading runAgent.ts", "Fixing null check in validate.ts") displayed in the UI's agent progress panel.

## How It Works

`startAgentSummarization()` creates a timer-based loop that:

1. **Reads the agent's transcript** from disk via `getAgentTranscript(agentId)`
2. **Filters incomplete tool calls** to get clean messages
3. **Forks the conversation** via `runForkedAgent()` with a summary prompt
4. **Extracts the text response** and updates the agent's UI summary via `updateAgentSummary()`
5. **Schedules the next run** only after the current one completes (prevents overlapping)

The summary prompt is minimal and specific:
- "Describe your most recent action in 3-5 words using present tense (-ing)"
- "Name the file or function, not the branch"
- Tracks `previousSummary` to request something NEW each time

Like other forked agents, tools are denied via callback (not empty tool list) to preserve cache sharing. No API parameters are overridden for the same reason.

## Key Source Files

| File | Purpose |
|------|---------|
| `agentSummary.ts` | Timer loop, fork orchestration, summary extraction |

## Configuration

- `SUMMARY_INTERVAL_MS = 30,000` (30 seconds between summaries)
- Minimum 3 messages in agent transcript before summarizing
- Abort controller per summary for clean cancellation

## Interesting Findings

1. **The `forkContextMessages` is rebuilt each tick** from the live transcript, not captured at start time. The original fork messages are explicitly dropped from the closure to avoid pinning a growing message array in memory.

2. **Timer resets on completion, not initiation.** This prevents a slow summary (waiting on the API) from causing the next one to overlap.

3. **The stop function cleanly aborts** any in-progress summary and clears the timer, important for agent lifecycle management.
