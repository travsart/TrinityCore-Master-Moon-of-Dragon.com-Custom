---
description: "tool dispatch — concurrent read-only batching max 10, tool result >100K disk persistence, ToolSearch deferred loading, execution pipeline"
---

# Tool Dispatch
> Source: `src/query.ts`, `src/tools.ts`
> Status: STUB — needs research

## Known
- Concurrent read-only batching (max 10)
- Results >100K chars persisted to disk
- ToolSearch defers tools to save prompt tokens

## Key Questions
- How are concurrent tool calls batched and executed?
- Read-only detection — which tools are read-only?
- How does ToolSearch decide to defer/include tools?
- Tool result size handling and disk persistence
