# Agent Best Practices — MANDATORY

Rules for launching and managing agents (subagents, background agents, file-sorter, etc.)

## Before Launching

1. **Pre-read existing memory** — Check `memory/` topic files for cached data before sending agents to re-discover it. If `memory/dossier_angel.md` exists, read it instead of searching 40 files.
2. **Use /index-folder first** — For large directories (Case_Reference, Finances), generate a manifest with `python tools/folder_index.py <dir>` and pass the manifest path to agents. They read one JSON file instead of doing hundreds of ls/find calls.
3. **Set hard output limits** — Tell agents: "Your output should be under 200 lines. If you need more, split into sections and write each to a file immediately." Prevents accumulate-then-choke failures.
4. **Give agents file paths, not descriptions** — "Read `C:/Users/atayl/Desktop/IMPORTANT DOCS/Finances/02_VA_Benefits_Income/Angel_Full_Checklist.md`" beats "find Angel's checklist."

## During Execution

5. **Write incrementally, not at the end** — Agents should write findings to `AI_Studio/Reports/` as they go, not accumulate everything and write once at completion. If an agent crashes or context overflows, partial results are preserved.
6. **Verify content, not just filenames** — When an agent reports "found 15 relevant files," it must also report what was IN those files. File existence is not evidence of content.

## After Completion

7. **Persist results immediately** — When a background agent completes, write its key findings to a persistent file BEFORE doing anything else. Context compresses; disk doesn't.
8. **Cross-check for contradictions** — If an agent found data that conflicts with existing memory files or previous agent results, flag it explicitly. Don't silently overwrite.
9. **Merge, don't duplicate** — Before writing a new report, check if an existing report covers the same topic. Update the existing file instead of creating a new one.

## Quality Checks (apply to agent output)

10. **Internal contradictions** — Does the agent's output contradict itself? (e.g., "no jobs found" in one section, "admin assistant" in another)
11. **Factual accuracy** — Are dates, names, amounts, and citations verifiable against source files?
12. **Completeness gaps** — Did the agent search all specified locations? Did it skip any file types?
