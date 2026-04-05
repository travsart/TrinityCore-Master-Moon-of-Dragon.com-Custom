# Session 228 — 5 Tab Handoff Prompts

**Created**: 2026-04-04 (end of session 227)
**Context**: Overnight sprint sessions 222-227 did CC internals research, LoreWalker migration, TC TDB backfill, DB error cleanup, Warlock specs, VoxSniffer Combat Audit. Server is running with merged TC+LoreWalker data (6.8M DBErrors remaining). Multiple workstreams ready for parallel execution.

**Already in progress** (from session_state.md):
- Tab 1 (228): Warlock extraction pipeline
- Tab 2 (228): DB Cleanup & Housekeeping

---

## Tab A: RoleplayCore Re-Apply → Then DB Error Cleanup Phase 4

```
Read doc/session_state.md and doc/db_error_cleanup_plan.md first.

This tab owns: ALL database work (world, hotfixes, roleplay). Do NOT touch C++ or addons.

Context: Session 227 applied TC TDB 1200.26021 via INSERT IGNORE on top of LoreWalkerTDB. Both data sets coexist. 6.8M DBErrors remain. Our custom RoleplayCore SQL files (sql/RoleplayCore/) were deleted from git but need re-applying to the DB. The files exist in git history.

## STEP 1: Re-apply RoleplayCore custom SQL (do this FIRST)

1. Recover RoleplayCore SQL files from git: `git show HEAD~5:sql/RoleplayCore/` (they were deleted recently but exist in history, commit 6f6938f29d or earlier). Write each file to a temp dir so you can apply them.

2. Re-apply these in order (they're numbered):
   - 1. auth db.sql — SKIP (user said leave auth alone)
   - 2. hotfixes db.sql
   - 2.1 hotfixes db spell changes.sql
   - 3. roleplay db.sql
   - 4. world db.sql
   - 4.1 world db (misc column).sql
   - 5-5.3 companion system SQL
   - 6. player_morph.sql
   - 7-12. unlock SQL (appearances, warband_scenes, mounts, heirlooms, toys, reputations)
   - custom_tables.sql
   - DarkmoonFaire_patch.sql

3. For each file, check if it uses INSERT/REPLACE/UPDATE — if INSERT, use INSERT IGNORE to avoid duplicates with TC data.

4. Check for TC incremental updates that may need re-running:
   - List: sql/updates/world/master/2026_02_06_* through 2026_04_03_*
   - These were tracked by worldserver auto-updater but may have partially failed when TC base data was missing
   - Check the `updates` table in world DB: SELECT * FROM updates ORDER BY timestamp DESC LIMIT 20

5. Document what you applied in doc/session_state.md

## STEP 2: DB Error Cleanup Phase 4 (do this AFTER Step 1)

Now that custom SQL is re-applied, clean up the remaining orphan errors:

1. Analyze the CURRENT DBErrors.log at out/build/x64-RelWithDebInfo/bin/RelWithDebInfo/DBErrors.log (6.8M lines, 643MB). Categorize errors with Python streaming (don't load into memory).

2. Fix these categories in order:
   a. SmartAI GUID/entry orphans (1.4M errors) — DELETE FROM smart_scripts WHERE source_type=0 AND entryorguid < 0 AND ABS(entryorguid) NOT IN (SELECT guid FROM creature). Then positive entryorguid NOT IN creature_template.
   b. SmartAI broken link chains (654K errors) — DELETE events where link > 0 but target event doesn't have event_type=61 (SMART_EVENT_LINK)
   c. Unsupported difficulty spawns (671K creature + 205K areatrigger + 70K gameobject) — DELETE spawn rows with spawnDifficulties that the map doesn't support
   d. quest_template_addon orphans (227K) — DELETE WHERE ID NOT IN (SELECT ID FROM quest_template)
   e. creature_template orphan children — DELETE from creature_template_difficulty, creature_template_model, creature_template_addon WHERE entry NOT IN (SELECT entry FROM creature_template)

3. DESCRIBE every table before writing SQL. Verify column names.

4. Write SQL to sql/updates/world/master/2026_04_04_03_world.sql (use /new-sql-update if needed).

5. Apply via /apply-sql or MCP safe_apply. Check affected row counts.

6. Do NOT restart the server — just apply to DB. We'll restart once all tabs finish.

Key tools: mcp__voxcore-db__describe, mcp__voxcore-db__query, mcp__voxcore-db__safe_apply
MySQL CLI: "C:/Program Files/MySQL/MySQL Server 8.0/bin/mysql.exe" -u root -padmin
```

---

## Tab C: Claude Code Hidden Commands + Feature Flags Survey

```
This tab owns: Claude Code internals research. Read-only exploration. No VoxCore code changes.

Context: Sessions 222-223 mapped 22 major systems in CC source (v2.1.88, v0.2.57). 12 reports written (Tier 1 + Tier 2 COMPLETE). Source at C:/Users/atayl/Desktop/claude-code-source/claude-code-source/. Reports at AI_Studio/Reports/ClaudeCodeInternals/. SME prompt at C:/Users/atayl/Desktop/PROMPT_Claude_Code_Internals_SME.md.

Your job — 3 deliverables:

1. HIDDEN COMMANDS CATALOG (write to AI_Studio/Reports/ClaudeCodeInternals/18_commands_catalog.md):
   - Read src/commands/ directory (189 files, 80+ commands)
   - For each command: name, description, what it does, whether it's usable in current version
   - Flag especially interesting ones: /ctx_viz, /thinkback, /thinkback-play, /bughunter, /good-claude, /stickers, /ultraplan, /security-review, /autofix-pr
   - Test which ones actually work by trying them (they may be gated)

2. FEATURE FLAGS SURVEY (write to AI_Studio/Reports/ClaudeCodeInternals/21_feature_flags.md):
   - Grep the source for all "tengu_*" flags and any other feature gates
   - Document what each flag controls
   - Check which are active in our environment (some may be server-side A/B tests)
   - Known gates: tengu_onyx_plover (AutoDream), tengu_session_memory

3. ULTRAPLAN REPORT (write to AI_Studio/Reports/ClaudeCodeInternals/16_ultraplan.md):
   - Read utils/ultraplan/ (2 files)
   - Document how it works, what triggers it, how to use it
   - Compare with regular /plan mode

Follow existing report format from the README.md. Keep reports thorough but under 500 lines each.
```

---

## Tab D: Housekeeping — Unicode Fix, Staging DBs, Gists, Bridge

```
Read doc/session_state.md first.

This tab owns: Housekeeping tasks. Small focused fixes.

Your job — 5 items:

1. FIX UNICODE FILENAME:
   - sql/updates/world/master/2026_04_03_02_world.sql has a hidden U+200E (left-to-right mark) character in the filename
   - This breaks the cowork bridge sync (UnicodeEncodeError)
   - Rename it: git mv the file to the same name without the control character
   - Verify: python -c "import os; print([f for f in os.listdir('sql/updates/world/master') if '04_03_02' in f])"

2. DROP STAGING DATABASES:
   - DROP DATABASE IF EXISTS world_lorewalker_staging;
   - DROP DATABASE IF EXISTS hotfixes_lorewalker_staging;
   - Check for: lorewalker_world (may be a view or DB)
   - Use mysql CLI: "C:/Program Files/MySQL/MySQL Server 8.0/bin/mysql.exe" -u root -padmin

3. UPDATE GISTS:
   - doc/gist_changelog.md — add sessions 217-227
   - doc/gist_db_report.md — update with current table counts and TC+LW merged state
   - Then run: /publish-gists

4. FIX BRIDGE SYNC:
   - After fixing the Unicode filename, re-run: python C:/Users/atayl/cowork/sync_bridge.py --full
   - If it still fails, check for other Unicode issues in filenames

5. Commit all changes with message: "chore: housekeeping — Unicode fix, drop staging DBs, gist updates"
```

---

## Tab E: CC Internals Tier 3 — Computer Use + Bridge + Buddy Reports

```
This tab owns: Claude Code internals research (Tier 3). Read-only exploration. No VoxCore code changes.

Context: Same as Tab C. Source at C:/Users/atayl/Desktop/claude-code-source/claude-code-source/. Reports at AI_Studio/Reports/ClaudeCodeInternals/.

Your job — 3 Tier 3 reports:

1. COMPUTER USE REPORT (write to AI_Studio/Reports/ClaudeCodeInternals/13_computer_use.md):
   - Read utils/computerUse/ (13 files) — "Chicago" codename
   - Document: screenshots, mouse/keyboard input, MCP server integration
   - How is it feature-gated? What triggers it?
   - Can we enable it? What would it take?

2. BRIDGE REPORT (write to AI_Studio/Reports/ClaudeCodeInternals/17_bridge.md):
   - Read src/bridge/ (31 files)
   - Document: IDE integration layer, session management, permission callbacks
   - How VS Code extension communicates with CLI
   - WebSocket protocol details

3. BUDDY SYSTEM REPORT (write to AI_Studio/Reports/ClaudeCodeInternals/15_buddy_system.md):
   - Read src/buddy/ (6 files)
   - Document: Tamagotchi pet feature, gacha, shiny variants, soul descriptions
   - How to enable, what it looks like, Easter eggs

Follow existing report format from README.md. Each report should be 200-400 lines with code snippets, architecture diagrams (ASCII), and actionable findings.

After writing all 3, update the README.md Tier 3 section to mark them COMPLETE and add key findings.
```

---

## Coordination Notes

- **Tab A** handles ALL database work in the correct order (re-apply custom SQL first, then cleanup). No other tab touches world/hotfixes/roleplay DBs.
- **Tab C** and **Tab E** are both CC research — fully independent, no file conflicts.
- **Tab D** is housekeeping — independent of everything except it drops staging DBs (harmless, those are orphans).
- All tabs should update doc/session_state.md when they start and finish.
- Server is RUNNING (PID 33360). Do NOT restart until Tab A finishes.
