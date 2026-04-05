---
description: "root source files — Tool.ts buildTool factory, query.ts 1729-line main loop, QueryEngine.ts, commands.ts registry, context.ts assembly, setup.ts bootstrap"
---

# Root Source Files
> Source: `src/*.ts` (top-level files)
> Status: STUB — needs research

## The Big 10
| File | Lines | Role |
|------|-------|------|
| query.ts | 1,729 | Main conversation loop |
| QueryEngine.ts | 1,295 | SDK/headless engine |
| Tool.ts | 792 | Tool interface + buildTool factory |
| commands.ts | 754 | 86+ slash commands, skill loading |
| tools.ts | 389 | Tool registry + pool assembly |
| setup.ts | 477 | Session init |
| context.ts | 189 | System/user context assembly |
| history.ts | 464 | JSONL prompt history |
| Task.ts | 125 | Task type definitions |
| cost-tracker.ts | 323 | Token/cost tracking |

## Key Questions
- Dependency graph between these files
- Which ones are hot paths (called every turn)?
