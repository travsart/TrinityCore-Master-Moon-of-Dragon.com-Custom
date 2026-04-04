---
allowed-tools: Agent, Read, Grep, Glob, Bash(powershell*), Edit, Write
description: Audit C++ code for bugs, performance issues, and correctness problems using parallel agents
paths: src/**/*.cpp, src/**/*.h, src/**/*.hpp
---

# Code Audit

Fan out parallel researcher agents to audit C++ source code for bugs, performance issues, and correctness problems. Produces a severity-ranked report.

## Arguments

$ARGUMENTS — Directory or file pattern to audit (e.g., `src/server/scripts/Custom/`, `Companion`, `all`)

## Process

### Step 1: Discover scope

If the user provides a specific directory, use it. If they say `all` or nothing, default to all custom code areas:
- `src/server/game/RolePlay/`
- `src/server/game/Companion/`
- `src/server/game/Hoff/`
- `src/server/game/Craft/`
- `src/server/game/Entities/Creature/CreatureOutfit.*`
- `src/server/scripts/Custom/`
- `src/server/scripts/Commands/cs_customnpc.cpp`

Use `Glob` to discover all `.h` and `.cpp` files in the target scope. Count them to determine how many agents to spawn.

### Step 2: Partition and launch agents

Split the files into 2-5 groups of roughly equal size. For each group, launch a **researcher** agent in the background with this prompt template:

```
Audit ALL .h and .cpp files in these locations for bugs, performance issues,
correctness problems, memory leaks, unused code, and optimization opportunities:

[LIST OF FILES/DIRECTORIES]

For each issue found, report:
- File path and line number
- Issue type (bug, performance, correctness, unused code)
- Severity (HIGH/MEDIUM/LOW)
- What the problem is
- Suggested fix

HIGH = crashes, UB, data loss, security issues
MEDIUM = gameplay bugs, significant performance, correctness issues with workarounds
LOW = style, minor perf, dead code, naming

Focus on REAL issues, not style nits. Look for:
- Potential null pointer dereferences
- Missing error handling
- Inefficient queries or loops (especially O(N) where O(1) exists)
- Thread safety issues
- Memory leaks or dangling pointers
- Unused variables/functions/parameters
- Logic errors (always-true conditions, unreachable code)
- Missing const references on heavy types (string, vector, map)
- Redundant container copies
- SQL injection risks
- Incorrect use of TC APIs (wrong overload, missing null checks)
- Use-after-move, self-assignment UB
- Console::Yes on commands that call GetPlayer()

Read every file completely. Do NOT skip files.
End with a summary table: | Severity | Count | Issue #s |
```

### Step 3: Consolidate

After all agents complete:

1. **Collect** all findings into a single list
2. **Deduplicate** — multiple agents may find the same issue (especially in shared headers)
3. **Verify false positives** — for any finding that seems suspicious, Read the actual code and check. Drop confirmed false positives with a note
4. **Sort by severity** — HIGH first, then MEDIUM, then LOW
5. **Count** totals per severity

### Step 4: Report

Write the consolidated report to `doc/code_audit_[date].md` with:

```markdown
# Code Audit Report — [date]

**Scope**: [directories audited]
**Files**: [count] .h/.cpp files
**Agents**: [count] parallel researchers

## Summary
| Severity | Count |
|----------|-------|
| HIGH     | N     |
| MEDIUM   | N     |
| LOW      | N     |

## HIGH Severity
| # | File:Line | Issue | Fix |
...

## MEDIUM Severity
...

## LOW Severity
...

## False Positives Dropped
- [issue] — [why it's not real]
```

### Step 5: Offer to fix

After presenting the report, ask: "Want me to fix the HIGH severity issues? (N items, all surgical fixes)"

If the user says yes, apply fixes following these rules:
- One Edit per fix, minimal changes
- Build after all fixes to verify: `powershell.exe -ExecutionPolicy Bypass -File "_build_ps.ps1" debug 2>&1`
- If build fails, fix the build error and rebuild

## Rules
- Never modify code during the audit phase — audit is read-only
- Each agent must read every file assigned to it completely — no sampling
- Verify at least the top 3 HIGH findings by reading the source before reporting
- Drop false positives explicitly with reasoning — don't silently remove
- If scope has >60 files, use 5 agents. If <15 files, use 2 agents
