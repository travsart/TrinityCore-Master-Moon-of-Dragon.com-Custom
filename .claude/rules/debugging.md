---
paths: "src/**/*.cpp, src/**/*.h, src/**/*.hpp, sql/**/*.sql, out/**/Server.log, out/**/DBErrors.log, out/**/Debug.log"
---

# Debugging Methodology — MANDATORY PIPELINE

This is a BLOCKING pipeline. Skipping a gate is a hard error.

## 4-Gate Pipeline

1. **GATE 1: Collect Data** — Fan out parallel agents to read ALL relevant logs (`Server.log`, `DBErrors.log`, `Debug.log`), query DB state, trace code paths with codeintel. **No hypothesis until data is collected.**

2. **GATE 2: Analyze** — State hypothesis with explicit data citations. Every claim needs a log line, packet byte, DB row, or code path. No citation = no claim.

3. **GATE 3: Propose Fix** — One change at a time. Root cause only. Trace downstream callers with codeintel before modifying any function.

4. **GATE 4: Verify** — Build, re-collect all data, confirm hypothesis matches. If not, back to Gate 1.

## Key Rules
- Never combine fixes
- Don't patch readers to fix writers
- DESCRIBE tables before SQL
- Don't summarize before reading data
- Don't propose fixes in the same message as the bug report

Full recipes, data source tables, and anti-patterns: auto-memory `debugging-methodology.md`
