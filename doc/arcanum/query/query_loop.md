---
description: "query.ts main loop — 1729 lines, API call stream tool dispatch compact check, conversation iteration, the brain of Claude Code"
---

# The Query Loop — query.ts
> Source: `src/query.ts` (1,729 lines) — THE most important file in CC
> Status: STUB — needs research

## What This Is
The main conversation loop. Every interaction goes through this: user message → API call → stream response → tool dispatch → check compact → loop.

## Key Questions
- Full flow diagram of one query iteration
- How tool calls are dispatched and results collected
- Streaming behavior — when does output appear vs buffer?
- Compact check — how does it decide to compact mid-query?
- Error handling — API errors, tool failures, rate limits
- How conversation history accumulates
