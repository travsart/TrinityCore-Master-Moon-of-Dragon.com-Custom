# VoxCore Session State — Multi-Tab Coordination

**Read this FIRST in any new Claude Code tab.**
This is the single source of truth for what all tabs are doing, what's done, what's blocked, and what to pick up next. Updated by whichever tab finishes work.

**Last updated**: March 22, 2026 -- Session 210: Full C++ audit of custom code. 4 parallel agents, ~40 files, 11 HIGH severity bugs fixed. Build verified 725/725 zero errors. Commit `3274d30362`. Handoff: `doc/handoff_cpp_audit.md`. Next: 14 MEDIUM fixes, in-game verification

---

## Active Tabs & Assignments

| Tab | Assignment | Status | Notes |
|-----|-----------|--------|-------|
| Antigravity (Current) | Triad Limits Tuning & Wrap-up | COMPLETE | Pushed optimized Triad execution rules and config limits for 128GB RAM. |
| Main (session 107) | Meta infrastructure, gist updates, coordination | COMPLETE | Commit `8aa10362ad`. Created session_state, bug tracker, skills, report |
| Main (session 108) | Consolidation — review all transmog docs, fix errors, update gists/memory | COMPLETE | Slot ordering fix, sniffing docs tracked |
| Main (session 109) | ImageMagick install + sniffing guide updates | COMPLETE | `8150cf3dd5` |
| Transmog Tab | Bug fixes from `memory/transmog-bugtracker.md` | COMPLETE | Session 110. 8 bugs fixed (G,H1,M6,M9,M1,M5,M2,UNICODE), 3 QA passes done. Ready for build. |
| Resource Tab (113) | Transmog resource audit — 3-pass QA of all tooling | COMPLETE | `7cef6952b0`. Bridge v3 IMPLEMENTED. lookup.py wrong DT labels. Enriched CSVs stale. Report: `doc/transmog_resource_audit.md` |
| Main (114) | LoreWalker import v3 — 3-pass QA of import prompt | COMPLETE | `80917a2739`. Fixed VB-in-PK bug, verified all 53 row counts, pre-baked SQL. Prompt: `doc/lorewalker_import_v3.md` |
| Tooling Tab (115b) | Phase 1 transmog tooling — DT maps, enriched CSVs, bridge annotation | COMPLETE | No commit (gitignored files). Created `transmog_common_maps.py`, fixed 3 tools, regenerated enriched CSVs for 66263 |
| Main (116) | Sniffing pipeline + accumulated commit | COMPLETE | `7ecad9990d`, `1419293a01`, `0808414a7e` |
| Commit Tab (117) | Commit coordination + transmog handoff | COMPLETE | No new commits. Recovered 3 reset commits from reflog. Generated transmog handoff prompt |
| Import Tab (118) | LoreWalker TDB import — write & apply 7 SQL files + fix _00_ | COMPLETE | `0997d17565`. Wrote 01-07, applied all 8 files (00-07). Fixed _00_ gameobject_template column count bug (32→35 Data zeros). ~502K inserts + 7.7K updates landed. QA clean. |
| Main (120) | NotebookLM knowledge base + tooling evaluation | COMPLETE | `b36bbb5811`. Created `doc/notebooklm/` (97 files). Evaluated Antigravity IDE. Reviewed 12 claude-code issues. |
| Main (121) | VoxPlacer polish — undo, face, favorites, minimap, ghost aura, QA | COMPLETE | `4fc562e404`. 4 features (undo stack, face-toward, favorites list, minimap button), ghost preview aura (37800), 6 QA fixes (keybinds, memory leak, false-positive state, fragile clone ref, GO clone props/orientation) |
| Main (123) | auto_parse v3 — modular log pipeline rewrite + QA + audit | COMPLETE | `98aa66149c`. 19-module package, 2,498 lines. 3 QA passes + Antigravity audit. 7 parsers, HTML dashboard, TOML config, tray icon, toast notifications |
| Main (124) | Tongue & Quill Auto-Formatter (standalone project) | COMPLETE | `C:\Users\atayl\TongueAndQuill\`. v2.1 production release, 8 AFH templates, auto-detect, PyInstaller build, audit prompt. No VoxCore commits. |
| Main (125) | DevOps pipeline overhaul — memory sync | COMPLETE | Synced memory with pipeline. Created `doc/claude_memory.md`. Updated 5 memory files |
| Main (127) | AI Studio + full sync + commit | COMPLETE | `9ee8c2bb55`. AI Studio hub (junctions for 3 projects), .agentrules, gitignore hardening (discord exports, transmog export, session logs), discord analytics script, DevOps prompts. 21 files, 855 insertions |
| Main (128) | VoxTip v1.0 + idTip rewrite + Triad handoff | COMPLETE | `97dd4ee6a2`. VoxTip debug toolkit (3 files), handoff to Antigravity, Central Brain + Triad workflow adopted. System pause acknowledged |
| Transmog (130) | Transmog bridge fail-open + MINI-BRIDGE sender | PAUSED | C++ `4f2512f29d`. Lua MINI-BRIDGE in TransmogSpy (slots 0/2/12/13, option-aware). Awaiting acceptance test |
| TQ (131) | TongueAndQuill v2.2 — page numbers, batch, 13 fixes | PAUSED | Code complete. Awaiting: AUDIT_PROMPT update, Z_Global fix, exe build, git init, Antigravity audit |
| Main (133) | Full ecosystem review + wrap-up | COMPLETE | `13ff762a9a`. Reviewed all sessions 123-132, committed Nexus Report tool + NotebookLM Enterprise docs. Memory synced |
| Main (134) | Triad guardrails + Antigravity briefing | COMPLETE | `43884ca85b`. Guardrails in MEMORY.md, coordination header in Central Brain, full capability dump for Antigravity |
| Antigravity (Auditor) | Wago CSV vs SQL Auditor pipeline | PAUSED | Python environment set up (`Setup-VoxCoreEnv.ps1`), `scripts/AI_Auditor.py` scaffolded. Command permissions overhauled in `.agentrules`. Awaiting DB connection logic. |
| Main (135) | Claude Code complaint taxonomy + support email | COMPLETE | `aa4aa29998`. Meta-issue updated, Triad/Grok reviewed. 16 issues. Support escalated. |
| Antigravity (Architect) | API Architect Producer MVP | COMPLETE | Configured run_architect.py pipeline + prompts + schemas. |
| Antigravity (Triad) | Triad Stream 1 & Stream 2 | COMPLETE | Built `build.py` orchestrator and `run_architect.py` live OpenAI pipeline. Specs saved heavily to doc/. |
| Antigravity (Bridge) | Stream 3, 4, 5 (Triad Control Plane) | COMPLETE | `2d9a9c38a2`. Built UI Command Center, Orchestrator jobs/adapters, and Claude live-bridge. DB bridge sync failed. |
| Antigravity (Loop) | Stream 6 (Triad Feedback Loop) | COMPLETE | Native `auto_retry` pipeline natively loops Headless Build -> Extract Errors -> Claude Fix -> Rebuild. |
| Antigravity (Support) | DraconicBot Novice Overhaul | COMPLETE | `d1b9cf8b08`. NLP parser (25K msgs), `diagnose.bat` auto-fixer, SME knowledge base, DM guide wizard. |
| Main (136) | DraconicBot v2.2 retool + Antigravity integration | COMPLETE | `e992e98c5e`. Lookups→Wowhead, troubleshooter retooled, 4 boot bugs fixed, bot deployed (17 cogs, 14 commands) |
| Antigravity | FAQ Phrase Banking & Regex Expansion | COMPLETE | `688bef7b1b`. 15 FAQ responses bulk expanded with 1500+ trigger phrases. |
| Antigravity (Restructure) | AI Studio Restructuring (P0) | COMPLETE | `fa550b7a81`. Moved Z_Global, schemas, templates to config/triad/. Repointed python configs. |
| Main (139) | BestiaryForge spec — creature→spell mapping pipeline | COMPLETE | `28df2070db`. 1,409-line spec, 3 QA passes + 2 adversarial rounds. Triad-approved. Phase 1 MVP next |
| Main (138b) | System optimization + AI fleet API integration | COMPLETE | `2dffaca3f2`. Power/perf tuning, OneDrive/Miniconda removed, all 3 API keys active, ChatGPT bridge operational, models upgraded, memory files overhauled |
| Main (144) | AWS Lambda deploy + Social media strategy + AI tool research | COMPLETE | `cf4a598c4f`. Lambda on AWS, 265 web searches, 20-platform social media strategy, Buffer recommended, brand identity = VoxCore (not DraconicWoW) |
| Main (145) | Audit Gap Analysis + Infrastructure Commit | COMPLETE | `263bac9675`. 33 files, Claude rules/hooks/agents, Antigravity toolkit |
| Main (146) | Lambda Tor Army v4 scraper improvements | COMPLETE | wago/ gitignored. Upgraded scraper_v4.py (adaptive WAF, multi-region, graceful shutdown), parsers.py (5 new specialized parsers, single-quote fix), generate_id_lists.py (validate sync, density stats), handler.py (15 fingerprints, coherent headers, WAF detection) |
| Main (147) | Greedy Parser v2 + Relationship Web + 66337 Hotfix Applied | COMPLETE | wago/ gitignored. parsers.py rewritten (1,245 lines, 18 extractors, relationship web — 45 edges/page, ~32M projected). generate_id_lists.py build-delta mode. scraper_v4.py 38 targets + delta mode. 66337 hotfix SQL applied (237,530 rows). |
| Main (148) | Claude Code Power Hooks + 7 Published Repos | COMPLETE | `206f2bb852`. BurntToast toasts, 3 new skills, hook test harness. Published 7 repos to VoxCore84 GitHub. |
| Main (160) | Cowork cleanup, Triad P0, Antigravity deprecation, inbox triage | COMPLETE | `b6e75874e0`. Removed prompt injections, archived transmog (3 commands, 1 agent, rules), Antigravity→API, P0 Triad directive in all core files, 39 stale specs archived, 2 personal files relocated. 20 files, -533/+180 lines |
| Main (167) | VoxGM v1.0.0 iterative review pipeline (9 rounds) | COMPLETE | `769fc01` (VoxGM GitHub), `767091feb9` (audit reports). Release gate PASS. Deployed to AddOns + Desktop zip + publishable/ |
| Main (168) | VoxSniffer v1.0.0 iterative review pipeline (7 rounds) | COMPLETE | `db077c0afc` (62 files, 8,881 lines). Dual ChatGPT review (API + Browser). Deployed to GitHub + AddOns + publishable/ + Desktop zip |
| Main (170) | Codex CLI pipeline integration | COMPLETE | Device-auth, config.toml, call_codex_review.py, review_cycle.py updated. Codex replaces ChatGPT API in rounds 1 & 4 (flat rate, repo-aware) |
| Main (171/173) | VoxGM v2.0 spec — autonomous review loop | PAUSED (R6) | 6 iterations, 30 rounds. R6 FAIL with ~15 well-scoped remaining issues. Packaged to Desktop. `e1e3ad393e`. **DO NOT implement until moved to 2_Active_Specs/** |
| Release Gate (171c) | 8 claude-code-* repo audit + v1.0.0 releases | COMPLETE | Full audit: em dashes, .gitignore, VoxCore refs, config naming, __pycache__. All 8 repos pushed + released. enforce.py bug fixed (overbroad `gh release` match). Gate status PASS |
| Main (171c) | Brand expansion + Deep Research ingestion | COMPLETE | 2 memory files created. 4 r/ClaudeAI reports ingested. Career guidance. awesome-claude-code email sent |
| Main (172) | Community engagement + Reddit outreach | COMPLETE | GitHub: 6 comments, #33465 contested, mvanhorn PR contribution. awesome-claude-code fork+branch. Reddit: 26 threads analyzed, 14 comment drafts, 5-day schedule. `606c51309d` |
| Main (173) | VoxSniffer V2 spec review pipeline (V6→V7) | COMPLETE | Autonomous fix pipeline: V6 reviews (1 CRIT + 4 HIGH + 10 MED + 7 LOW) → V7 with 25+ fixes + 3-pass self-audit. 2 CRITICAL scoping fixes in Phase 6. Spec zipped to Desktop. Remaining: initialized gate, FM.FlushAll guard, payload removal notes |
| -- | -- | -- | Add rows as tabs are opened |

**Rule**: Before starting work, check this file. If another tab owns a file or task, don't touch it. Update your row when you start and when you finish.

---

## Release Gate System (NEW — Session 165)

A pre-ship audit system is now available for all addon/tool work. Use it before shipping anything.

### Available Tools

| Tool | What | When |
|------|------|------|
| `/pre-ship <path>` | Full 5-phase audit: mechanical checks + 3 parallel adversarial agents (noob, bully, security) | Before any release, zip, or GitHub publish |
| `/release-gate-fix` | Focus only on open BLOCKING items from last audit | After running `/pre-ship`, to fix what it found |
| Enforcement hooks | `PreToolUse` blocks `git push --tags`, `gh release`, zip when gate != PASS. `PostToolUse` invalidates gate when publishable/ files are edited | Automatic — no action needed |

### Validator Agents (`.claude/agents/`)

| Agent | Role | Mode |
|-------|------|------|
| `grep-auditor` | Naming remnants, non-ASCII, secrets, dead refs | Read-only |
| `doc-auditor` | Path verification, version consistency, feature claims vs reality | Read-only |
| `app-reviewer` | Adversarial personas (noob, bully, security) | Read-only |

### Gate State File

`.claude/release-gate-status.json` — written by `/pre-ship`, read by hooks. Values: `PASS`, `FAIL`, `STALE`, `UNKNOWN`.

### Checklist Reference

Full 16-phase, ~130 item checklist: `memory/addon-building-checklist.md`. Covers Lua, C++, Python, naming, docs, packaging, security, distribution.

### Known Issue

Custom agent types (`app-reviewer`, `grep-auditor`, `doc-auditor`) require Claude Code restart to register. Until then, `/pre-ship` uses `general-purpose` agents with detailed prompts — same results, just no type restriction.

### Pre-Ship Audit Findings (Session 165)

62 findings across CreatureCodex + VoxGM. Full report was delivered in session chat. Key blockers for each project:

**CreatureCodex blockers**: Rename not finished (live source still says Bestiary), dev artifacts in distribution (CHATGPT_AUDIT_REQUEST*.md, reference/ dir), em dashes in Python/C++, RBAC SQL inconsistency between README and sql file, Linux shell scripts call Windows-only APIs

**VoxGM blockers**: ~300-400 lines dead code, Favorites/History claimed as features with zero UI, em dashes in 4 Lua files, "Max Gold (999g)" label wrong (gives ~9999g), README claims "any TrinityCore server" but ~15 commands are VoxCore-specific

---

## Current Server State

- **Build**: Current (VS build done). Includes transmog fail-open + bridge grace + BestiaryForge hooks
- **Server**: NOT RUNNING
- **Client**: 12.0.1.66263
- **DB**: world ~1,200 MB (712K creatures, +101K from LoreWalker) | hotfixes 811 MB (400K spells) | characters 4 MB
- **Logs**: Clean — zero crashes/fatals. SmartAI warnings + unhandled 12.x opcodes only.
- **LoreWalker TDB import**: APPLIED (session 118) — 7 files + _00_ Stormwind fix. Restart worldserver to load.

---

## What Needs Doing — Priority Order

### Tier 1: Server Restart & Test (requires human)

Build is done. These need a server restart and in-game testing.

- [ ] **Restart worldserver** and test:
  - Arcane Waygate (`.cast 1900028`, gossip, teleports)
  - Stormwind phase fixes (7 phase_area, Genn/Velen/Anduin visibility)
  - Valdrakken portal, embassy NPCs, Hero's Call Boards
  - Apply `_08_00` SQL before restarting
- [ ] **CreatureCodex in-game test** — C++ build clean (866/866), `.codex` command, addon deployed. GitHub v1.0.0 released
- [ ] **Enable crash dumps** — Windows crash dump generation for worldserver

> **Note**: Transmog Outfits UI work is ARCHIVED — reimplemented externally. All transmog bugs, slash commands, and agents have been removed. Historical docs preserved in `.claude/rules/archive/transmog.md` and `doc/transmog_*`.

### Tier 2: World DB Cleanup (Claude Code tab can do independently)

**Assign to**: Any available tab
**How**: Run `python tools/diff_draconic.py --zone <id> --map <map>`
**Plan**: `doc/world_db_cleanup_plan.md`

Priority order:
1. Orgrimmar (zone 1637, map 1)
2. Ironforge (zone 1537, map 0)
3. Thunder Bluff (zone 1638, map 1)
4. Darnassus (zone 1657, map 1)
5. Undercity (zone 1497, map 0)
6. Exodar (zone 3557, map 530)
7. Silvermoon (zone 3487, map 530 → newly map 0 for Midnight)
8. Dalaran (zone 4395, map 571)
9. Global phase_area audit (after all zones done)

Each zone produces a SQL file in `sql/exports/` and findings for review.

### Tier 3: Spell Implementation (Claude Code tab can do independently)

**Assign to**: Any available tab
**Context**: `memory/spell-audit.md`
- 13 RED spells need real C++ implementations (SimC-guided)
- 84 YELLOW passive DUMMY auras (low priority)
- Key spells: Avenging Wrath, Pillar of Frost, Blood Plague, Divine Hymn

### Tier 4: Data Quality (Claude Code tab can do independently)

- **66 crash-risk creature displayIDs** — query world DB, fix or remove
- **3 MySQL deadlocks** — investigate transaction contention patterns
- **Companion Squad SQL** — apply `sql/RoleplayCore/5.1 companion characters.sql`
- **Equipment gaps** — 13K NPCs missing `creature_equip_template`

### Tier 5: Website & Polish

- Arcane Codex website asset pipeline (Phase 0 ready)
- Skyriding/dragonriding outside Dragon Isles
- Orgrimmar portal room → Silvermoon (BC-era → Midnight)

---

## Key Files Quick Reference

| What | Where |
|------|-------|
| **This file** (coordination) | `doc/session_state.md` |
| Transmog bug tracker | `memory/transmog-bugtracker.md` |
| Transmog full report | `doc/transmog_implementation_report.md` |
| Transmog behavioral rules | CLAUDE.md → "Transmog UI / Midnight 12.x" section |
| World cleanup plan | `doc/world_db_cleanup_plan.md` |
| Spell audit status | `memory/spell-audit.md` |
| To-do list | `memory/todo.md` |
| Open issues (GitHub gist) | `doc/gist_open_issues.md` |
| Changelog (GitHub gist) | `doc/gist_changelog.md` |
| DB report (GitHub gist) | `doc/gist_db_report.md` |

## Skills Available

| Skill | What It Does |
|-------|-------------|
| `/build-loop` | Iterative build + fix compilation errors |
| `/check-logs` | Read server logs for errors |
| `/apply-sql` | Apply SQL file to a database |
| `/new-sql-update` | Create correctly-named SQL update file |
| `/lookup-spell` / `/lookup-item` / etc. | DB2 lookups |
| `/wrap-up` | End-of-session checklist |

---

## Rules for Multi-Tab Work

1. **Read this file first** in every new tab
2. **Claim your assignment** — update the Active Tabs table before starting
3. **One bug per commit** — don't combine fixes across domains
4. **Don't touch files another tab owns** — check the table
5. **Update this file when done** — move your task to completed, note what changed
6. **Building from Claude Code is allowed** — use `ninja -j32` via Bash (VS IDE also works)
7. **Don't duplicate research** — if a memory file or report covers it, read that instead of re-analyzing source code
8. **Update bug trackers** — after fixing a bug, change its status in the tracker

---

## Recently Completed (for context)

| Session | What | Key Output |
|---------|------|-----------|
| 185 | Legal filing review + submission package build | 5 FINAL filing packages, 47 evidence subfolders, master checklist, 12 unknown unknowns, 24-claim fact-check. No VoxCore commit (Desktop files) |
| 184 | Case file organization + folder indexing | 40 documents filed, 7 __Master_Index.md files created. `f6796a89a3` |
| 183 | Legal audit + cross-tab integration + MASTER_00 | 14 BLOCKING + 20 WARNING fixes across 6 MASTER docs. Exec summary created. Contact numbers verified. No VoxCore commit (Desktop files) |
| 173 | VoxGM v2.0 spec autonomous review loop | 6 iterations x 5 rounds (30 total). R1-R6. ~50 findings fixed. Packaged to Desktop. `e1e3ad393e` |
| 172 | Community engagement + Reddit outreach | GitHub: responded to 6 commenters, contested #33465, PR contribution for mvanhorn #32755. awesome-claude-code fork submitted. Reddit: 26 threads, 14 comment drafts, 5-day posting plan |
| 171c | 8 claude-code-* repos v1.0.0 | Full audit + fix cycle: em dashes, .gitignore, VoxCore refs, config naming, __pycache__. All 8 repos released on GitHub. enforce.py overbroad match bug fixed |
| 168 | VoxSniffer v1.0.0 | 14-module server data sniffer (62 files, 8,881 lines). 7-round dual ChatGPT review. Source-bound callbacks, nameplate reseeding, dedup-after-envelope. GitHub + AddOns + publishable/ |
| 167 | VoxGM v1.0.0 | 26-file GM control panel (2,700 lines). 9-round review. 6 tabs, minimap button, event parsers. GitHub + AddOns + publishable/ |
| 166 | CreatureCodex v1.0.0 | Creature spell/aura sniffer. 7-round review. C++ hooks + addon + Eluna. install_hooks.py fix, session.py WoW root detection. GitHub + AddOns |
| 123 | auto_parse v3 | 19-module package (2,498 lines). Plugin parser arch, session-aware watcher, alert dedup, HTML dashboard, TOML config, tray icon, crash scanner, packet pipeline. 3 QA + Antigravity audit |
| 121 | VoxPlacer Polish | 4 features (undo 10-deep stack, face-toward, favorites list, minimap button), ghost preview aura (spell 37800), 6 QA fixes. ~1140 lines C++, ~930 lines Lua |
| 120 | NotebookLM Knowledge Base | 97 files in `doc/notebooklm/` (docs, source as .txt, SQL, Lua addons). Evaluated Antigravity IDE, reviewed 12 claude-code issues |
| 119 | Anti-Theater Protocol | Completion Integrity rules in CLAUDE.md. 6 prohibitions, mandatory checklist, 5 memory files updated |
| 118 | LoreWalker TDB Import | 7 SQL files applied, 502K inserts + 7.7K updates, _00_ column bug fixed, QA clean |
| 115b | Transmog Tooling Phase 1 | Created `transmog_common_maps.py`, fixed DT maps in 3 tools (DT 12/14 added, lookup.py wrong numbering fixed), regenerated enriched CSVs for 66263, annotated bridge v3 spec |
| 113 | Transmog Resource Audit | 3-pass QA of all transmog tools/CSVs/bridge. Key: bridge v3 implemented, lookup.py wrong DT numbering, enriched CSVs stale. `doc/transmog_resource_audit.md` |
| 112 | Sniffing Guide Polish | Hub gist cleanup, generic branding, Heads Up section |
| 111 | LoreWalker TDB Analysis | 6-agent sweep, import pipeline ready in `doc/lorewalker_import_prompt.md` |
| 110 | Transmog Master Tab | 8 bugs fixed, 3 QA passes, DT/validator clean, resource audit. `doc/transmog_next_steps.md` |
| 109 | ImageMagick + sniffing docs | Installed IM, updated Midnight priorities + WPP sanitize |
| 108 | Transmog consolidation | Slot ordering fix, sniffing docs tracked |
| 107 | Meta infrastructure | This file, bug tracker, skills, gist updates |
| 106 | Wrap-up | Committed sessions 104-105b work |
| 105b | Transmog DeepDive | `doc/transmog_deepdive_wiki.md`, 4 memory files |
| 104 | Draconic diff + SW | `tools/diff_draconic.py`, 7 phase_area fixes |
| 103 | NPC tooling | `.npc copy` command |
| 102 | Collection unlocks | `.maxrep`/`.maxachieve`/`.maxtitles` |
| 101 | SpellAudit cleanup | Removed 1,842 broken stubs |

---

## GitHub Gists (synced March 21 — session 199)

- DB Report: https://gist.github.com/528e801b53f6c62ce2e5c2ffe7e63e29
- Changelog: https://gist.github.com/4c63baf8154753d2a89475d9a4f5b2cc
- Open Issues: https://gist.github.com/2b69757faa2a53172c7acb5bfa3ad3c4
