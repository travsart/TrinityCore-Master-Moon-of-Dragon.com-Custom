---
description: "entry points — REPL SDK bridge modes, cli argument parsing, headless QueryEngine, IDE integration entry, mode detection logic"
---

# Entry Points
> Source: `src/entrypoints/`, `src/entrypoints/sdk/`
> Status: STUB — needs research

## Three Entry Modes
1. **REPL** — interactive terminal (what we use)
2. **SDK** — headless programmatic API (Agent SDK)
3. **Bridge** — IDE integration (VS Code, Desktop)

## Key Questions
- How does the CLI detect which mode to use?
- What capabilities differ between modes?
- The SDK QueryEngine — how does headless mode work?
