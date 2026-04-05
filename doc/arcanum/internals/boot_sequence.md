---
description: "boot sequence — cli.js entrypoints bootstrap state.ts setup.ts main.tsx REPL.tsx query.ts, initialization order, session ID creation"
---

# Boot Sequence
> Source: `src/bootstrap/`, `src/entrypoints/`, `src/setup.ts`
> Status: STUB — needs research

## Known Sequence
cli.js → entrypoints (REPL/SDK/bridge) → bootstrap/state.ts → setup.ts → main.tsx → REPL.tsx → query.ts

## Key Questions
- What happens in each boot phase? Timing?
- What can fail and how is it recovered?
- Difference between REPL, SDK, and bridge entry points
- Global mutable state initialization (session ID, cost tracker, tokens)
