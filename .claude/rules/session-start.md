# Session Start — MANDATORY

Runs automatically at the start of every new conversation. No slash command needed.

## P0 — USE THE TRIAD
Before implementing anything non-trivial, check: should ChatGPT generate a spec first? After implementing, should Gemini audit it? See CLAUDE.md § P0 for trigger table and exact commands. **Do not brute-force everything yourself.**

## Required Actions (BEFORE responding to user's first message)

1. **Silently read `AI_Studio/0_Central_Brain.md`** — acquire Triad context
2. **Read `doc/session_state.md` (MANDATORY — not "if exists")** — scan the `## Active Tabs & Assignments` table. **Before your first tool call**, note: (a) which tabs are ACTIVE, (b) which files each owns, (c) whether any of those files overlap with what the user is likely to ask about. If you end up doing work that touches a file owned by another ACTIVE tab, STOP and coordinate via the patch-file handoff pattern (write to `AI_Studio/Reports/` with merge instructions) instead of editing the file directly. **Escalated rule as of session 242** — sessions 239, 241, 242 all failed this and discovered overlap at wrap-up instead of at start.
3. **Read `## Next Session` section of `todo.md`** from memory — pre-loaded task list
4. **Run `python .claude/hooks/deadline-alert.py`** — surface any critical deadlines (HARD <30d, any <14d, past-due). Print to user if output is non-empty. Silent on clean days. Source: `.claude/deadlines.json`.

## Claude Code is Primary
You are the Primary Terminal and Coordinator for VoxCore. All other AIs (ChatGPT, Gemini, Cowork) are API endpoints or scheduled task runners called from here. Execute pipeline actions immediately. Antigravity (Windsurf IDE) is deprecated — Gemini is accessed via API now.

## EXTRACT and TRACK Actionable Items
**Reading is not enough.** Session 114 bug: Claude read session_state.md which said "Apply _08_00 SQL before restarting" then completely ignored it.

- After reading coordination docs, **list every actionable instruction** (one line each)
- **Show to user**: "I found these pending items: [list]. Which should I handle?"
- If skipping a listed item, acknowledge explicitly with reason
- **Do not silently drop items.** If you read it, you own it until you hand it back

## Tab Assignments
If `session_state.md` has active tab assignments, announce what this tab should focus on. If user's request conflicts with tab assignments, ask before proceeding.

If neither file exists or both are stale, proceed normally with user's request.
