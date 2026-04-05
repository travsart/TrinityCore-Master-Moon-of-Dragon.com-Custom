---
description: "hook execution pipeline — evaluation order, timeout handling, JSON stdin stdout protocol, error recovery, parallel vs sequential hooks"
---

# Hook Execution Pipeline
> Source: `src/hooks/`, `utils/hooks/`
> Status: STUB — needs research

## Key Questions
- Execution order: shell > prompt > agent > http?
- Timeout behavior per hook type
- How hook results (allow/deny/message) are processed
- The 4-way permission race between hooks and permission rules
- Error handling when hooks fail/timeout
