# CronCreate — RESTRICTED (P0 Stability)

## HARD RULE: NEVER create recurring cron jobs

`CronCreate` with `recurring: true` (the default!) is **BANNED**. It freezes tabs by firing prompts into the REPL when "idle", hijacking context and making the tab unresponsive. This has caused hours of lost work across multiple sessions.

### What IS allowed:
- **One-shot reminders only**: `recurring: false`, for reminders ≤60 minutes away
- Example: "remind me in 30 min to check the build" — OK
- Must always set `recurring: false` explicitly

### What is BANNED:
- `recurring: true` (the default) — NEVER
- `durable: true` with `recurring: true` — NEVER
- Any schedule meant to run repeatedly — NEVER
- "Every N minutes" / "hourly" / "daily" patterns — NEVER

### What to use instead for recurring work:
- **Session start hooks** — read files written by external scripts
- **Windows Task Scheduler** — runs Python scripts on schedule, writes results to `AI_Studio/Reports/scheduled/`
- **The `/deadlines` skill** — already handles deadline tracking without cron
- **Manual invocation** — user asks when they want it

### If a user asks for a recurring task:
Explain that recurring cron jobs freeze tabs and suggest the pull-model alternative:
> "Recurring cron jobs freeze other tabs. Instead, I can create a script that Windows Task Scheduler runs on your schedule, writing results to a file that gets surfaced at session start. Want me to set that up?"
