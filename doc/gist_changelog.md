RoleplayCore -- Session Changelog (WoW 12.x private server)



## Apr 4 2026

### Session 228 -- DB Cleanup & Housekeeping
- **Dropped staging databases**: `world_lorewalker_staging` + `hotfixes_lorewalker_staging` (session 118 leftovers). No orphan views remaining
- **LoreWalker loot error fix**: 428 rows in `creature_template_difficulty` had LootID referencing missing `creature_loot_template` entries. 208 unique creatures zeroed out. Eliminates ~1.78M DBErrors at startup
- **Gist sync**: Updated all 3 gists (DB Report, Changelog, Open Issues) to cover sessions 214-228

### Session 227 (main) -- VoxSniffer Combat Audit v1
- **CombatAudit module implemented**: CombatAudit.lua (player CLEU capture, proc watchdog, spell stats accumulators, aura timeline), ProcExpectations.lua (data-driven proc validation), audit_report.py (Python report generator)
- **Gemini audit**: PASS after 4 HIGH fixes (guard clauses, event validation, performance bounds, error handling)
- Commit: `5cd63fdd3f`

### Session 227 (tab 2) -- DB Error Cleanup + TC TDB Backfill
- **6.1M DBErrors analyzed**: Categorized all errors -- loot orphans (30%), SmartAI (22%), hotfix schema (18%), difficulty (15%), SmartAI links (11%)
- **Phase 1 cleanup**: Deleted orphan difficulty rows (26K), quest_template_addon (57K), SmartAI (195), zeroed bad loot refs (412K), hotfix cleanup (203K). Total ~698K rows
- **Phase 2a cleanup**: SmartAI broken link chains (164K), quest_poi orphans (21K), game_event orphans (3.5K)
- **TC TDB backfill**: Downloaded TDB 1200.26021, built converter, applied 598MB world (771 tables) + 334MB hotfixes (433 tables) via INSERT IGNORE on LoreWalker base. 329 removed items purged from loot
- **Plan**: `doc/db_error_cleanup_plan.md` for remaining 6.8M errors
- Commit: `eef19fe221`

### Session 226 -- Warlock + VoxSniffer Specs (Triad)
- **TRIAD-WARLOCK-FULLCLASS-V1**: 31KB spec via ChatGPT API. All 3 specs + class tree + 3 hero talent trees. 8-phase implementation plan
- **TRIAD-VOXSNIFFER-COMBAT-AUDIT-V1**: 18KB spec. Player CLEU capture, proc watchdog, spell stats, aura timeline, Python report generator
- Commit: `6e32ef1e0f`

### Session 225 -- LoreWalker Migration Apply + MySQL Tuning
- **Applied LoreWalker migration SQL**: 3 world files + 1 hotfixes file. ~3.3M rows (680K creatures, 194K gameobjects, 2M broadcast_text_locale, etc.)
- **MySQL 8.0 NVMe optimization**: 12 InnoDB settings tuned for 128GB RAM + Samsung 9100 PRO (buffer_pool 8G, io_capacity 20K, redo_log 2G, doublewrite OFF)
- **Crash fix**: Duplicate `RegisterSpellScript(spell_sha_feral_lunge)` from TC upstream sync
- **Account recreated**: auth DB was empty post-migration, created via bnetaccount create
- Server boots in ~31s, 22GB RAM, 1.78M DBErrors (cosmetic loot orphans)

### Session 224 -- Wrap-up + 13 Conditional Skills
- **13 skills made conditional**: `paths:` frontmatter (5 C++/build, 5 SQL, 2 addon, 1 packet)
- **FileChanged hook**: Script ready for schema support
- **SubagentStart + ConfigChange hooks** registered
- Commit: `8f01aa113c`

### Session 223 -- Claude Code Tier 2 Reports (1M context)
- **7 reports written** (5,550 lines): tool pipeline, swarm system, coordinator/agents, hooks, permissions, skills, MCP client
- **Key findings**: hook allow doesn't bypass deny, tool results >100K persisted to disk, fork subagents share prompt cache, MCP timeout ~27.8h
- Reports at `AI_Studio/Reports/ClaudeCodeInternals/06-12_*.md`

### Session 222 -- Claude Code Internals Deep-Dive
- **Source analysis**: 4 npm archives (162MB, 1,884 TS files). Mapped 22 internal systems across 213 directories
- **11 reports** (266KB): compaction, system prompt, context window, memory, AutoDream + 6 optimization reports
- **Optimizations applied**: 1M context enabled, 3 conditional rules, .gitignore 205->15 untracked, 54 memory frontmatter files
- Commit: `0916b667c9`

### Session 221 -- Swift Crusade Spell + Timestamp Hook Fix
- **Custom spell 1900031**: “Swift Crusade” -- +100% move speed always, +200% mounted ground+flight. Permanent, self-cast
- **Timestamp hook**: Added statusMessage for terminal visibility
- Commit: `0916b667c9`

## Apr 3 2026

### Session 220 -- Bnetserver Fix + Chrono Surge + DB Schema Repair
- **Custom spell 1900030**: “Chrono Surge” -- +250% haste, -75% CD recovery. Permanent, self-cast
- **DB schema repair**: 3 fixes for TC sync compat (crafting columns, 14 missing tables, ALTER TABLE)
- **Bnetserver port 1119 fix** + duplicate MCP process cleanup
- Commit: `54d9ef6621`

### Session 219 -- Samsung 9100 PRO SSD Install Prep
- **Hardware planning**: 34-step SSD swap checklist, disk audit (82% full), full clone strategy
- **USB recovery**: Repartitioned 57GB SanDisk from broken 1GB FAT32

### Session 218 -- Timestamp Hook + “Convos with Claude” Blog
- **Timestamp hook built**: `UserPromptSubmit` hook injecting datetime. GitHub repo: `claude-code-timestamp-hook`
- **dev.to blog post published**: “Convos with Claude: Teaching an AI to Tell Time”
- **Twitter/X cross-linked**: Tagged @AnthropicAI, @alexalbert__

### Session 217 -- TC Upstream Sync (19 Commits)
- **Full TC sync**: All 19 TrinityCore commits Mar 21 -- Apr 1 2026
- **Command system refactor**: `std::vector<ChatCommand>` -> `ChatCommandTable` (std::span). Updated 55 TC + 12 custom files
- **Transmog fixes** (3 TC commits): Full ViewedOutfit processing, TransmogHoliday.db2, artifact crash fix
- **Build**: Zero errors, 13/13 targets. Commit: `762a20e97a`

## Mar 23-31 2026

### Session 215 -- Angel VA TDIU Filing Support
- **21-8940 PDF filled**: 103 XFA fields, draft answers, migraine legal analysis (3 decisions)
- **4 buddy statements** + neurologist template + action plan + Item 26 continuation sheet

### Session 214 -- Gemini Pro Business Briefing
- **15-doc briefing package**: 10 memory + 11 desktop files synthesized. Identity correction, Google ecosystem mapping
- Commit: `c690e31568`

## Mar 21 2026

### Session 200 -- Migration Phase 7 + DB Error Cleanup Blitz
- **Build bump 66337→66527**: TACT+Wago+merge across 1,097 tables. Migration branch merged to master
- **DB error cleanup**: 10.8M→407K DBErrors/boot (96.3% reduction). 8 SQL files. SmartAI orphan cleanup (1,196 AIName fixes + 99 orphan deletions + 546 broken link chains). Creature spells, NPCText, model_info fixes
- **Spell 1247917 transmog clear fix**: Effect mismatch corrected + hotfix_blob cleanup
- Server boots clean in 36s

### Session 199 -- Spell Script Audit + Hook Test Harness
- **220 spell_script_names registered**: Comprehensive audit found 174 C++ scripts without DB entries. Resolved via TDB 1200.26021 bulk import (176 entries) + manual Wago DB2 lookups (21). Total: 5,557→5,777
- **5 SQL update files**: warlock Diabolist, migration scripts, orphan vendor cleanup, TDB bulk import, manual lookups
- **Hook test harness upgraded**: 33 tests (16 health + 17 regression scenarios)
- **GitHub community**: Defended #36851 from auto-closure, supported #36850 terminal BEL proposal
- Commit: `4fb4e96436`

### Session 198 -- Continuous Improvement Pipeline + Angel TDIU Prep
- **5 new skills**: `/retro`, `/checkpoint`, `/draft-email`, `/read-any`, wrap-up Step 8 retro
- **scenario_calc.py**: Financial scenario comparison table generator
- **agent-practices.md**: 12 rules for agent quality (pre-read memory, output caps, incremental writes)
- **Angel TDIU questionnaire**: Pre-filled 60% from case archive. Dossier cached
- Commit: `4d99d3227b`

### Session 197 -- Hook Fixes + Agent Tools + GitHub Engagement
- **3 hook false positives fixed**: release-gate-enforce, release-gate-revalidate, sql-safety
- **3 agent tools built**: `write_file.py`, `folder_index.py`, `bulk_extract.py`
- **6 GitHub comments**: Substantive engagement on anthropics/claude-code issues
- Commit: `9411a6b6c7`

## Mar 20 2026

### Sessions 193-196 -- Financial Planning + Case Work
- **Full 24-month AMEX audit**: 4,034 transactions, $209K total, $8,370/mo average. Key leaks: $764/mo DoorDash, $1,033/mo Amazon, $650/mo AI subs
- **Credit report analysis**: Clean — 18 accounts, 0 collections, 0 late payments
- **27-source money/aid document**: Military family financial assistance guide. Angel TDIU worth +$1,738/mo. NM property tax exemption saves ~$526/mo
- **Family Action Plan**: LES-verified income/expense gap, 6 post-separation scenarios, CRDP research (+$1,000-1,500/mo underestimated)
- **OWF reactivated**: Found Erasmo Valles approval from Feb 13, resume never confirmed sent. Email sent with resume
- **Finances folder organized**: 65+ files into 6 subfolders with INDEX.md
- **Brutal Honesty Mandate**: Permanent memory rule for realistic business/financial advice

## Mar 16 2026

### Sessions 179-185 -- Legal Filing Sprint
- **NPDB Subject Statement filed**: Fact-checked against archive, corrected dates, submitted to NPDB
- **5-phase evidence audit**: 661 files inventoried, 561 images cataloged, 96/107 mapped to discrepancies
- **5 filing packages built**: AFBCMR DD-149, NPDB dispute, HIPAA/OCR, IG supplement, congressional. All FINAL in `Submission_Packages/`
- **47 evidence subfolders**: Master submission checklist + pre-filing action items

## Mar 15 2026

### Sessions 170-178 -- Review Pipeline + Published Tools
- **Codex CLI integrated**: Device-auth, `review_cycle.py` parallel pipeline (Phase 1: Codex+Gemini+Claude → Phase 2: verify → Phase 3: seal)
- **VoxGM v2.0 spec**: 6 iterations, 30 review rounds. Paused at R6 with ~15 remaining issues
- **8 claude-code-* repos audited + released**: v1.0.0 for guardrails, edit-verifier, sql-safety, windows-toasts, hook-tester, compaction-keeper, scroll-fix, workflow-guard
- **Brand expansion**: awesome-claude-code submission, Reddit outreach (26 threads, 14 comment drafts)
- **System optimization**: Audio stack cleanup, PDF readers, case file OCR pipeline

## Mar 13-14 2026

### Sessions 164-168 -- Addon Releases
- **VoxGM v1.0.0**: 26 files, ~2,700 lines Lua. 6 tabs, minimap button, SavedVariables. 9-round iterative review. Release gate PASS
- **VoxSniffer v1.0.0**: 62 files, 8,881 lines. 14 capture modules, ring buffer, session management. 7-round dual ChatGPT review
- **Release Gate system deployed**: 4-layer pre-ship audit (`/pre-ship`), 2 enforcement hooks, 3 adversarial agents, gate state file
- Commits: `769fc01` (VoxGM), `db077c0afc` (VoxSniffer)

### Sessions 157-163 -- CreatureCodex + DraconicBot
- **CreatureCodex v3.0.0**: Dual-layer creature spell/aura sniffer (server C++ hooks + client addon). `.codex` GM command. Tabbed export. 3 README stress tests (EN/RU/DE)
- **DraconicBot v3.1.0**: AI stress test 5%→98% (126 cases). KB splitting (9→89 sections). Oracle Cloud VM provisioned. Full AI KB rewrite (8 files, 20 FAQs, 51 GM commands)

### Sessions 153-160 -- Infrastructure Overhaul
- **VoxCore Daemon Phase 1**: Persistent Python background process for autonomous DevOps. APScheduler, state manager, Claude API with circuit breaker, Discord notifications. 15 files
- **Transmog ARCHIVED** (session 159): Entire transmog system reimplemented externally. Slash commands removed, agent deleted, rules archived
- **Antigravity DEPRECATED**: Gemini now accessed via API. Historical config archived
- **Triad P0 directive**: Mandatory in all core files. 39 stale specs archived
- Commits: various

## Mar 10-12 2026

### Sessions 135-152 -- Scraper + Lambda + Hotfix 66337
- **Tor Army v3.2**: HTTP/2 multiplexed scraper. 230K/hr peak, 193K sustained at 400x5 workers. Standalone repo
- **Lambda Swarm**: AWS Lambda scraper (blocked by Cloudflare for Wowhead — Tor Army is primary)
- **Hotfix repair 66337**: 70 tables, 104 zeroed fixes, 4,291 custom diffs preserved, 237,530 hotfix_data rows
- **Social media strategy**: 265 web searches, 20-platform strategy, Buffer recommended
- **8 published GitHub repos**: claude-code-* tools released as v1.0.0
- Various commits across sessions

## Mar 9 2026

### Session 134 -- Triad Guardrails + Aegis Phase 2 Closeout
- **Triad AI workflow codified**: Claude Code = Implementer, ChatGPT = Architect, Antigravity (Gemini) = Systems Architect / QA-QC. Mandatory preflight checks, refusal protocol for unspec'd features, Central Brain coordination file
- **Aegis Config Phase 2 complete**: Hardcoded `C:\Users\` paths removed from 8 runtime scripts (batch + Python). New infrastructure: `resolve_roots.py` bootstrap, `Aegis_Path_Contract.md` (frozen alias rules), `paths.json` canonical registry, audit scanner + classifier (692 entries, 400+ false positives filtered), smoke test checklist
- 25 files changed, 2,293 insertions across Phase 2. Commits: `43884ca85b`, `c0de05d950`

### Session 133 -- Full Ecosystem Review
- Read all operational docs, verified Triad roles, committed Nexus Report generator (`tools/log_tools/generate_nexus_report.py` -- Vertex AI/Gemini daily devlog), NotebookLM Enterprise planning docs
- Commit: `13ff762a9a`

### Session 132 -- Cowork VM Fix
- Diagnosed missing Cowork in Claude Desktop -- session 84 gaming perf tuning had disabled Hyper-V/virtualization. Re-enabled `HypervisorPlatform`. Reboot pending

### Session 131 -- TongueAndQuill v2.2
- AFH 33-337 document formatter upgrade: page numbering (page 2+ flush right), batch mode (GUI + CLI), template cache, 13 bug fixes (temp file leaks, DPI awareness, double-click guard, dead code removal, keyboard shortcuts)
- Standalone project at `C:\Users\atayl\TongueAndQuill\` (~1,530 lines). Awaiting Antigravity audit

### Session 130 -- Transmog Bridge: Fail-Open + MINI-BRIDGE
- **C++ server-side**: `hasUsableBridgeData` guard prevents savedOutfit baseline from wiping parsed CMSG data. `_transmogBridgeWaitOneUpdate` one-update grace period prevents safety-net from firing before bridge arrives
- **TransmogSpy MINI-BRIDGE sender**: Moved bridge send from TransmogBridge into TransmogSpy's PreClick hook. Limited to slots 0/2/12/13, option-aware. TransmogBridge set to passive mode
- Status: PAUSED in acceptance-test mode. Commit: `4f2512f29d`

### Sessions 126-129 -- DraconicBot v2.1
- Full Discord support bot for DraconicWoW (`tools/discord_bot/`). 14 cogs, 16 slash commands, ~2,700 lines across 19 Python modules
- FAQ auto-responder (11 patterns from 30K message analysis), slash lookups (/spell /item /creature /area /faction), bug triage + auto-threading, SOAP server status, TrinityCore GitHub build watchdog, Wowhead resolver, onboarding, /help dropdown, /troubleshoot decision tree, changelog feed, automod, welcome roles
- 68 custom WoW icon emojis. Awaiting server owner authorization to deploy
- Commits: `593cb53ae6`, `64a786a312`, `375c1a0674`

### Sessions 123-125 -- Auto-Parse v3 + DevOps Pipeline
- **Auto-Parse v3**: `tools/auto_parse/` modular package (19 modules, 2,498 lines). 7 parsers, TOML config, HTML dashboard, tray icon, toast notifications, alert dedup, graceful shutdown, client data capture, session brief synthesis. 3 QA passes + Antigravity audit
- **DevOps pipeline**: `start_all.bat` (6-step boot), `stop_all.bat` (graceful shutdown + handover), `apply_pending_sql.bat` (boot-time SQL deploy), Claude Code handover protocol
- Commits: `98aa66149c`, `b36bbb5811`

### Session 121 -- VoxPlacer Polish
- 4 features: undo stack, face-toward, favorites list, minimap button. Ghost preview aura (37800). 6 QA fixes
- 13 commands total, ~1,140 C++ / ~930 Lua. Commit: `4fc562e404`

### Session 120 -- NotebookLM + Tooling Evaluation
- Created `doc/notebooklm/` knowledge base (97 files) for AI-assisted analysis
- Evaluated Antigravity IDE capabilities. Reviewed 12 claude-code GitHub issues
- Commit: `b36bbb5811`

### Sessions 116-118 -- LoreWalker TDB Import + Transmog QA
- **LoreWalker import**: 7 SQL files applied (~502K inserts + 7.7K updates). Creature delta +101,018 exact, zero orphans
- **Transmog QA audit fixes** (session 116): H1 ClearDynamicUpdateFieldValues, M1 zero invalid enchants, M2 decouple enchant restore, M5 parse weapon options, M6 hidden pants, M9 gate illusion bootstrap
- **Sniffing pipeline**: parse_sniff gap auditor, parse_addon_data cross-ref tool
- Commits: `7ecad9990d`, `1419293a01`, `0808414a7e`, `0997d17565`

### Sessions 113-115 -- Transmog Resource Audit + Tooling
- 3-pass QA of all transmog tooling, Bridge v3 implemented, enriched CSVs regenerated for build 66263
- DT label bugs found and fixed in lookup.py. Report: `doc/transmog_resource_audit.md`
- Commit: `7cef6952b0`

### Sessions 108-110 -- Transmog Bug Fixes + Consolidation
- 8 bugs fixed (G, H1, M6, M9, M1, M5, M2, UNICODE), 3 QA passes
- Slot ordering fix, sniffing docs tracked, cheatsheet DisplayType corrections

## Mar 8 2026

### Session 106 -- Meta Infrastructure + Gist Updates
- **Transmog bug tracker**: Created `transmog-bugtracker.md` — single source of truth for all transmog bugs (3 CRITICAL, 6 HIGH, 4 MEDIUM, 4 LOW)
- **New skills**: `/transmog-implement` (pick & fix next bug), `/transmog-status` (quick bug overview)
- **Comprehensive transmog report**: `doc/transmog_implementation_report.md` — standalone reference for implementation sessions
- **Gist updates**: All 3 gists synced to current state (DB sizes, session history)
- **Server logs audit**: Clean — zero crashes/fatals, only unhandled opcodes + SmartAI warnings

### Session 105b -- Transmog DeepDive Wiki
- **1,500-line authoritative wiki** from 54 source Lua/XML files (`doc/transmog_deepdive_wiki.md`)
- **4 new memory files**: transmog-enums, transmog-api-reference, transmog-ui-architecture, transmog-updatefields
- **Critical correction**: DisplayType values in cheatsheet were wrong — fixed via DeepDive source analysis
- **UpdateField structures**: Full WPP-derived struct layouts for TransmogOutfitSlotData

### Session 105 -- Transmog Cheatsheet Update
- Updated `doc/transmog_cheatsheet.md` with corrected DisplayType values and opcodes

### Session 104 -- Draconic Diff Tool + Stormwind Cleanup
- **`tools/diff_draconic.py`**: Zone-by-zone world DB diff vs Draconic-WOW (build 66263, named pipe)
- **Stormwind findings**: 7 missing `phase_area` entries (root cause of invisible NPCs), portal fixes, board dedup
- **`.npc copy` command**: Code complete in `npc_copy_command.cpp`
- **Comprehensive cleanup plan**: `doc/world_db_cleanup_plan.md` — 9 zones remaining
- Commit `e90f4da5bc`

### Session 103 -- NPC Tooling
- `.npc copy` command implementation
- Crash investigation (MoveSplineInit)

### Session 102 -- Collection Unlock Commands
- **New commands**: `.maxrep`, `.maxachieve`, `.maxtitles` — unlock all reputations/achievements/titles
- **Arcane Waygate**: Custom spell 1900028, gossip menu, teleport destinations
- **RBAC QA**: Permission audit and cleanup
- Commit `c1d17d983b`

### Session 101 -- SpellAudit Stub Cleanup
- Removed 1,842 broken C++ spell script stubs (empty handlers causing load failures)
- Registered Sweeping Strikes + Beacon of Light (real implementations)
- Commit `5eb5883fb4`

### Session 100 -- CC Lighthouse + PWA
- Command Center Lighthouse score 93/100
- PWA support, WebP icons, auto-start in `start_all.bat`
- Commit `da14bd911e`

## Mar 7 2026

### Session 99 -- Shortcut & Command Center QA Mega-Pass
- **5-agent QA sweep**: 48 desktop shortcuts, 48 CC tiles fully audited and synced
- **Bnetserver/worldserver shortcuts**: Fixed to use `cmd.exe /k` wrappers
- **start-worldserver.sh**: Fixed `$SCRIPT_DIR` vs `$RUNTIME` path bug
- **opcode_analyzer.py**: Fixed `DEFAULT_PROJECT_ROOT` pointing to wrong directory
- **CC fixes**: URL guard, unused imports, cmd.exe wrapping, larger logo (180px)
- **Bat files**: Deleted obsolete `start_mysql.bat`, cleaned vestigial MySQL80 refs
- Commits `48c9d4f9b7`, `01aaf028e9`, pushed

### Session 98 -- MySQL QA + DB Health Audit + Spell Audit SQL
- **UniServerZ MySQL**: Fixed skip-name-resolve TCP blocking, procs_priv charset corruption, IP grants
- **Spell audit SQL applied**: 114 serverside_spell stubs, 18 spell_proc entries, 1,888 spell_script_names
- **MySQL QA (5 agents)**: my.ini optimized (8GB buffer pool, 20K IO), batch files updated
- **DB event cleanup**: Disabled broken Panda event, deleted collecting_battle_pets
- **Discovered ~70 unapplied SQL updates** from consolidation -- handed off for bulk apply
- Commit `039df2a6af`, pushed

### Session 97b -- Hotfix Repair + Bulk SQL Restore
- **Hotfix repair build 66263**: 2,727,115 missing rows inserted, 496 zeroed columns fixed across 28 tables (339 MB SQL)
- **Bulk SQL restore**: Applied ~200 SQL files across all databases after fresh DB rebuild
- DB restored to: creature_template_spell 172K, creature 611K, npc_vendor 174K, spell_name 400K

### Session 97 -- WT Font Config + Shortcut Regen
- **Windows Terminal**: JetBrains Mono 14pt font for Claude profile
- **Desktop shortcuts regenerated**: 47 shortcuts across 8 folders
- Commit `0c02d75803`, pushed

### Session 96 -- Command Center Integrations + Path Fix
- **Systemic path fix**: 15 subprocess.Popen commands -- removed absolute path doubling
- **New integrations**: GitHub Repo, Discord, Wago.tools, GitHub Pages (URL cards)
- **2 new pipelines**: Wago DB2 Download, DB Snapshot List-Rollback
- **Desktop sync tool**: `sync_from_desktop.py` diffs VC desktop folders vs CC cards
- Commit `bf877e0e0f`, pushed

### Session 95 -- Spell Creator + SpellAudit Fix
- **New tool: `tools/spell_creator.py`**: Python CLI replacing old .NET SpellCreator
  - 11 templates, full clone from wago CSV, hotfix SQL generation, SOAP reload
  - Icon search (32K+), enum reference, clipboard/SQL/DB/SOAP output modes
- **SpellAudit generator fix**: Regenerated 13 class files (1,842 scripts) -- zero warnings
- **SOAP enabled**: worldserver.conf `SOAP.Enabled = 1`
- Commit `68dcea8161`, pushed

### Session 94 -- Comprehensive Code Quality Pass
- **Full source audit**: 11 agents, **39 fixes across 19 files**
- 7 critical crash fixes (bounds checks, nullptr guards, assert-->if)
- 8 high (EffectsHandler memory leaks, CompanionAI O(n)-->O(1), GM logging)
- 12 medium (strtok/atoi-->StringTo, scale bounds, SpellVisualKit validation)
- 12 low (typos, help text, dead methods, #pragma once, NULL-->nullptr)
- Config optimizations: worldserver.conf + MySQL my.ini tuned
- Commit `f3f5e015a1`, pushed

### Session 93 -- Command Center QA Fixes
- 6 bug fixes for VoxCore Command Center (icon crop, missing pipeline cards, index validation, null guard, Jinja2 stepper, image try-except)
- Commit `f0ee73d7f1`, pushed

### Session 92 -- Archive Cleanup + Shortcut Audit
- Deleted `archive/` (287 MB orphaned files), audited all 44 desktop shortcuts -- 44/44 OK

### Session 91 -- Doc Audit
- `doc/` directory: 20 --> 13 files, deleted 7 obsolete, renamed gist_current --> gist_db_report
- Commit `1a49d90996`

### Session 90 -- SQL Directory Audit
- **12 issues fixed**: 5 idempotency (INSERT IGNORE, CREATE IF NOT EXISTS, etc.), roleplay DB in create_mysql, sql/old pruned
- **Pruned 19,416 old TDB files** from git tracking (3.8 GB saved)
- Tracked files: 19,820 --> 399
- Commit `e15eb622c6`

### Session 89 -- Archive Cleanup
- Deleted `archive/` directory (287 MB) -- orphaned pre-consolidation files

### Session 88 -- Spell Audit Pipeline
- **QA'd class_spell_audit.py**: Fixed 3 classification bugs
- **Generated 1,842 C++ spell script stubs** across 13 per-class files
- **Generated SQL**: 114 serverside_spell stubs, 18 spell_proc entries, 1,888 spell_script_names
- All 13 C++ files compile cleanly, 38,695 lines added
- Commit `27c2d7d04e`, pushed

### Session 87 -- Scenic Art Upscaling Pass 2
- Upscaled 154 images across 6 categories using Real-ESRGAN 4x (622 MB output)

## Mar 6 2026

### Session 86 -- Grand Consolidation QA
- 200+ old path refs fixed, memory files consolidated (28-->23), .gitignore (39-->73 lines)
- Windows Defender hardened, GitHub repo/gists cleaned, README rewritten
- Commit `37ce2554f8`

### Session 85 -- Grand Consolidation
- Moved everything to `~/VoxCore/`, organized personal docs, rebuilt entire project
- Commit `95eb139ee2`

### Session 84 -- Windows System Performance Tuning
- **30+ registry tweaks**: Hyper-V/VBS/HVCI off, Spectre mitigations off, NVIDIA MSI mode, GameDVR off
- **Network**: Nagle's disabled, TCP window scaling, 65K ports, 30s TIME_WAIT
- **Boot**: useplatformtick + disabledynamictick (0.5ms timer)

### Session 83 -- Grand Consolidation Plan
- Full C: drive audit, 757-line consolidation plan across 8 phases

### Session 82 -- Stormwind Cleanup: Event NPCs, Boards, Portals
- 6 SQL fixes: Wickerman Revelers, Argent Crusade stationary, broken Hero's Call Boards, stale Dalaran portal, invisible Silvermoon portal (displayId fix)
- Commit `b0c1dd07ce`

### Session 81 -- DB Optimization + Claude Code Tuning
- 69 fragmented tables optimized (~150 MB reclaimed), MySQL buffer pool 4-->8 GB
- Claude Code permissions pre-allowed, MEMORY.md diet (175-->76 lines, -70%)
- Commit `b0c1dd07ce`

### Session 80 -- Community Listfile + Scenic Art + Transmog DeepDive
- 2.1M-entry community listfile downloaded, 6 new scenic art categories (217 files)
- Transmog DeepDive: 35 source Lua/XML, 3 new 12.x DB2 CSVs, Blizzard_DebugTools addon

### Session 79 -- Transmog QA Audit: 1 HIGH, 10 MEDIUM, 10 LOW
- Full two-pass QA audit (10 agents), H1: stored outfit Slots array accumulation bug identified

### Session 78 -- Cowork Symlink Network
- 19 symlinks created, confirmed symlinks don't work in Cowork (Linux sandbox limitation)

### Session 77 -- Mega Data Mining: 316K SQL statements across 7 files
- Quest integrity compiler, BtWQuests untapped, ATT mega extract, quest metadata
- Wowhead NPC mining: 161K creature spells, 105K loot, 4K trainers
- Safe spawns: 28,665 creature spawns generated
- DB impact: creature 665K-->694K, creature_template_spell 10K-->171K
- Commit `194a596ca0`

### Session 76 -- Claude Customization + Wrap-Up QA
- Claude.ai profile updated, `/wrap-up` QA (6 fixes), VS 2022-->2026 refs updated

### Session 75 -- WebTerm + VoxCore Rebrand + Transmog IDT Fix
- **WebTerm**: Python web terminal (Flask+SocketIO+xterm.js) replacing ttyd
- **VoxCore rebrand**: RoleplayCore --> VoxCore across skills + CLAUDE.md
- **Transmog IDT fix**: Armor slots use IDT=0 for assigned appearances
- Commit `d1a3060e6e`, pushed

### Session 74 — Wowhead 310K QA Pipeline + Coord Converter + SQLite DB (Mar 6 2026)
- **310K NPC reparse**: Fixed models/displayID extractor (3 regex patterns: ModelViewer.showLightbox, dataset.displayId, data-mv-display-id). 204,136 model entries extracted (up from 0%). 3,345s, 0 errors
- **Coordinate converter** (`coord_convert.py`): Wowhead UI-map percentages → TrinityCore world coordinates via UiMapAssignment DB2 (1,909 maps, 22,494 assignments). Key discovery: axes SWAPPED + INVERTED. Validated <10 unit accuracy across 4 zones
- **SQLite database** (`wowhead_npcs.db`): 338 MB, 16 tables, fully indexed. 309,996 NPCs, 2,092,045 coords, 538,169 drops, 204,136 models, 1,538,668 sounds. Import: 99s, 0 errors
- **QA Pass 1** (field completeness): 99.1% names, 70.8% creature type, 32.2% with coords
- **QA Pass 2** (coord quality): 0 out-of-range, 162 continent uiMaps (82K NPCs safe), 56 instance maps (26K must NOT spawn)
- **QA Pass 3** (DB cross-reference): 38,119 gap NPCs — 478 CRITICAL (quest), 145 HIGH (vendor), 12,017 MEDIUM (SmartAI), 25,479 NORMAL. 99.3% type match, 100% classification match
- **Oddities analysis**: 8 oddities identified, 5 safe vs 5 risky actions classified
- **Integration architecture**: Designed 6 Flask routes, Leaflet.js maps, spawn SQL pipeline
- **VoxCore Data Intelligence Report**: 1,434-line/77KB comprehensive report covering all 11 data sources, Vox Army specs, optimization roadmap, buildable products
- Wago commit: `2351879`

### Session 73 — Scraper V2 + Full Wowhead NPC Scrape (Mar 5 2026)
- **Scraper V2**: Complete overhaul of `scrape_all_gaps_tor.py` (1,262 lines). 5 entity types, enriched NPC parser, HTML caching (gzip), reparse mode, adaptive Tor timing
- **Full 310K NPC scrape**: 240 Tor workers, IDs 1-310000, ~315K/hr. Zero errors
- **Critical finding**: v1 parser only captured name for 97.6% of pages — g_mapperData and g_npcs completely missed
- Wago commit: `c6d7734`

### Session 73 — Transmog Corrective Pass (Mar 5 2026)
- **6 surgical fixes** to `fillOutfitData` in Player.cpp — retail behavioral model alignment
  - Added `isStored` parameter to distinguish stored TransmogOutfits from live ViewedOutfit
  - Assigned rows always ADT=1 (both contexts); viewed empty = ADT=2/IDT=2; stored empty = ADT=0/IDT=0
  - Bootstrap from equipped items only for viewed outfits (stored keep empty slots as 0/0)
  - SlotOption uses wire option index (mapping.option) not visual classification (0/1/3)
  - Stamped MH/OH options use real MainHandOption/OffHandOption enums, not booleans
  - Paired weapon placeholder threshold corrected to options >= 5 (was >= 6)
- **CLAUDE.md**: Updated transmog authoritative rules — all confidence levels now HIGH
- **`/transmog-correct` command**: Added to `.claude/commands/` for future corrective passes
- Commit: `7bb510359b`

### Session 72 — Universal Scraper + Midnight Data Harvest (Mar 5 2026)
- **Scraper v2**: Upgraded to 5 entity types (quest, npc, trainer, vendor, object), 120 Tor workers, per-batch logging
  - Added NPC page parser (vendor items, teaches, quests, gossip) and object page parser (quest starts/ends, loot)
  - Smoke test + full batch: 15,044 pages in 797s (68K/hr), zero failures
- **Midnight NPC scrape**: 13,491 NPC pages (entry range 235K-300K) — complete Midnight expansion NPC data
- **Midnight objects**: 555 gameobject pages with quest links and loot tables
- **Trainer gap fill**: 1,022 trainer NPC pages for server-wide broken trainer fix
- **Gap quests**: 27 remaining quest pages scraped (merged gap + midnight quest IDs)
- Wago commit: `ca37060`

### Session 71 — Claude Desktop Cowork Integration (Mar 5 2026)
- **Cowork bridge**: sync_bridge.py snapshots 389 source files into ~/cowork/bridge/ for Claude Desktop
- **Project bible**: Comprehensive project reference for Cowork (bridge paths, repos, gists, tools, schema)
- **ttyd web terminal**: Full system access for Cowork via localhost:7681
- **CLAUDE.md**: Added transmog UI/Midnight 12.x authoritative rules section
- Commit: `3cc2d58c70`

### Session 70 — Transmog Audit Pass 2 + Hidden DT Fix (Mar 5 2026)
- **Transmog audit pass 2**: 585-line QA report (client events, TRANSMOG_COLLECTION_UPDATED over-firing, DisplayType)
- **Hidden appearance detection**: ItemID-based detection for 10 hidden items, DT=3 for hidden slots
- **Paired weapon DT=4**: Options 6-8 on MH/OH emit ADT=4+IDT=4
- Commits: `8d36580ac4`, `053e01fed2`

### Session 69 — Stormwind Retail Accuracy (Mar 5 2026)
- **Ambient NPC scripts**: 37 SmartAI entries for ~426 spawns (guards, citizens, workers, refugees, dracthyr)
- **Phase cleanup**: 139 Day of the Dead ghosts event-gated, 2 broken Darkmoon spawns removed
- **Combat NPC equipment**: 19 entries (sentinels, captains, officers, marshals)
- **Hero's Call Board dedup**: Removed old GO 206111 overlapping newer 281339
- Commits: `152178ffcc`, `295d04f890`, `e710f5a709`, `9e728c9139`, `9e53777d55`

### Session 67 — Stormwind Retail Sniff + Hero's Call Board Dedup (Mar 5 2026)
- **Retail sniff import**: 152 creature spawns, 21 GO spawns, 9 equipment templates from Stormwind retail ground truth
- **Sniff enrichment**: 161 updates — creature type/family/Classification/unit_class, HP/Mana modifiers, portal deduplication
- **Stormwind Wowhead scrape**: 28 Hero's Call Board quest starters applied, then reverted (duplicate of newer GO 281339)
- **Hero's Call Board dedup**: Removed old GO 206111 overlapping newer GO 281339 + 5 SmartAI orphan cleanups
- Commits: `9962076dbf`, `c5cb54ec54`, `c7bada7e1d`, `9e53777d55`

### Session 67 — Transmog Hidden Appearance + PacketScope (Mar 5 2026)
- **Hidden appearance detection**: Proper DT=3 handling for hidden visual IMA IDs
- **Paired weapon DT=4**: Display type for paired weapon transmog
- **TransmogSpy label fix**: Corrected display type labels in diagnostic output
- **PacketScope improvements**: Better transmog packet inspection tooling
- Commit: `8d36580ac4`

### Session 66 — Midnight Expansion Scrape + BtWQuests Enrichment (Mar 5 2026)
- **Midnight expansion scrape R2**: 226 queststarters, 181 questenders, 174 vendor items, 11 GO quest links from Wowhead
- **BtWQuests CT + ATT enrichment**: 228 ContentTuningID fills, 252 vendor items, 426 exclusive groups
- **Transmog addon QA**: Name-Realm whisper fix, button hook reliability, failed event logging, nil guard on payload send, pcall on Layer 1
- Commits: `c76813221d`, `6be6f4682b`, `e0bd9ef78b`, `0014c37771`

### Session 65 — NPC Mega-Scrape + ATT Cross-Reference + Quest Chains (Mar 5 2026)
- **NPC mega-scrape**: 80,943 Wowhead pages scraped with 120 Tor workers (~250K/hr)
  - 1,727 creature queststarters, 2,979 creature questenders, 2,535 vendor items
- **ATT cross-reference import**: 170 creature QS, 124 GO QS, 176 quest chain links
- **Quest chain application**: 572 PrevQuestID + 2,008 NextQuestID from BtWQuests
- **Scraper v2 built**: shared priority queue, adaptive delay, 100+ workers, auto-parse
- **Running totals**: creature_queststarter 34,647 | creature_questender 37,026 | gameobject_queststarter 2,066 | npc_vendor 176,853
- **Remaining gaps**: 16,552 quests without starter, 13,165 without ender

### Session 64 — Build 66263 Data Pipeline Bump (Mar 5 2026)
- **TACT extraction**: 1,094 DB2 tables from local CASC (build 66263, cleanup build: -813 rows in key tables)
- **Wago CSVs downloaded**: 66263 CSVs merged with TACT data
- **12 Python scripts updated**: wago_common, tact_extract, merge_csv_sources, diff_builds, cross_ref_mysql, att_parse_addon, att_parse_hierarchy, att_constants, att_enrich_sqlite, audit_talent_spells, parse_vendor_scrape, phase_resolver
- **Memory + tooling inventory refreshed** for 66263 references
- **Key changes in 66263**: -106 ItemSparse, -107 IMA, -266 ItemAppearance, -82 SpellName, +14 SpellEffect, +8 QuestV2
- **WPP**: Updated to nightly build (66263 support). Old 66220 backed up to `WowPacketParser_66220_backup/`
- **Ymir**: Updated to 66263
- **Auth keys**: TC published 66263 keys. Applied + bypass reverted in WorldSocket.cpp
- **Hotfix repair**: Needs re-run against 66263 baseline

### Session 64 — BtWQuests + Vendor Scrape + Midnight Data (Mar 5 2026)
- **BtWQuests addon parse**: Extracted quest starter/ender data from BtWQuests addon Lua files
  - 1,062 new `creature_queststarter` entries, 57 new `gameobject_queststarter` entries
  - All cross-referenced against existing DB to avoid duplicates
- **Wowhead vendor scrape R2**: 772 pages scraped, 92 had vendor data
  - 1,435 new `npc_vendor` entries across 82 NPCs
- **Midnight data import** (from session 61 scrape): 58 quest starters, 60 quest enders, 819 loot entries, 526 creature spells
- **Running totals**: creature_queststarter 32,458 | gameobject_queststarter 1,933 | npc_vendor 173,855
- Commit: `9340906e9d`

### Session 64 — Build 66263 Auth Fix (Mar 5 2026)
- **New WoW build** 12.0.1.66263 detected — client auto-updated from 66220
- **Auth SQL** `2026_03_05_00_auth.sql`: registers build 66263 in `build_info`, updates realmlist gamebuild, deletes stale `build_auth_key` rows, has commented-out key INSERT template
- **WorldSocket.cpp bypass**: Temporary — logs warning instead of rejecting when auth key not found. Will be reverted when TrinityCore publishes official 66263 keys
- Commit: `e3fc8cd9d6`

### Session 63 — High-Tier Service NPC Spawns (Mar 5 2026)
- 1,492 vendor/trainer/flightmaster spawns from Wowhead coordinate transforms
- 1,483 ContentTuningID assignments via zone lookup
- GUID range 3000217122–3000218613
- Commit: `25031c1eda`

### Session 63 — Transmog Audit Full Implementation (Mar 5 2026)
- **All 4 phases implemented** from 26-item 5-agent audit action plan
- **Phase 1+2** (commit `20c9a0ea23`): 4 server bugs fixed (per-spec appearance bootstrap, HandleTransmogOutfitNew active ID, Finalize flush, clear spell active ID reset) + 4 Bridge cleanup items (multi-part split bail-out, dead Layer 2 code, diagnostic probe removal, deterministic slot ordering)
- **Phase 3** (commit `1dfc2eb207`): TransmogSpy v2 rewrite — 944→1,317 lines, 17 commands, 12 new events, displayType capture, IMA-to-name resolution, 6 new hooks, illusion tracking, `/tspy status/bridge/resolve/items`
- **Phase 4** (commit `c8df50eddd`): Hardening — IgnoreMask baseline restore, stale partial payload cleanup, spec-switch ViewedOutfit resync, per-slot invalid/uncollected appearance zeroing
- **Final fix** (commit `ab43e4823d`): `EffectEquipTransmogOutfit` was the only outfit-apply path missing `SetEquipmentSet()` → ViewedOutfit sync. Situations parser consistency (hardcoded 256→MaxTransmogOutfitSlotCount, proper error paths)

### Session 62 — Transmog 5-Agent Comprehensive Audit (Mar 5 2026)
- 5 parallel agents audited: TransmogBridge, TransmogSpy, server handlers, Player.cpp, retail sniffer
- Found: 9 HIGH + 19 MEDIUM + 23 LOW findings across all subsystems
- Key server bugs: per-spec appearance bootstrap missing, HandleTransmogOutfitNew missing SetActiveTransmogOutfitID, FinalizeTransmogBridgePendingOutfit missing UpdateField flush
- TransmogSpy: missing 12 events, no displayType capture, no IMA name resolution
- 26-item action plan in 5 phases (server fixes → Bridge cleanup → Spy v2 → hardening → retail capture)

### Session 60c — Transmog Stale Detection Fix (Mar 5 2026)
- **Server-side stale rejection** replaces client-side preSnapshot/comparison detection
- Addon now tags overrides: `option=1` (SetPendingTransmog hook = trusted), `option=0` (snapshot/fallback)
- Server parses into `FromHook` flag; rejects snapshot data for slots the saved outfit ignores
- Eliminates false positive where bootstrapped appearance echoed back as outfit data (required double-apply)
- Removed ~44 lines client-side detection, added ~37 lines server-side rejection
- Commit: `0cde8db70c`

### Session 60b — Transmog 11-Fix QA Sweep (Mar 5 2026)
- 5 MEDIUM + 6 LOW fixes from comprehensive 4-agent audit
- M1: `_activeTransmogOutfitID` persisted to DB (new `active` column + SQL migration)
- M2: `EffectEquipTransmogOutfit` sets active outfit ID before applying
- M3: Situations packet parser capped at 256 entries (OOM prevention)
- M4: Cross-contamination fix in CurrentSpecOnly reset paths
- L1-L6: Double flush removal, per-spec illusion bootstrap, clear spell IgnoreMask, DeleteEquipmentSet cleanup, signed shift fix, dead code warning
- Commit: `27b5496f4f`

### Session 61 — Midnight Expansion Data Scrape + Import
- Scraped 38 Wowhead Midnight guide pages + 586 entity pages (44 MB raw HTML, 0 WAF blocks)
- Extracted: 712 items, 282 NPCs, 247 spells, 154 quests, 85 achievements, 1,314 loot drops
- Cross-referenced all data against world DB: 819 new loot entries, 526 boss abilities, 118 quest links
- Applied `2026_03_05_15_world.sql` (1,463 rows). Commits: wago `966e0eb`, RC `d81962a4d6`

## 2026-03-05 — ATT Mega-Parser (Session 54)

### AllTheThings Complete Data Extraction
- **`att_to_sqlite.py`** â€” comprehensive SQLite extractor for ALL AllTheThings data
- **60 normalized tables** from 30 data loaders, 52.6 MB database, 27s full rebuild
- Phase 1: Lua AST parse of 1,635 files -> quests (47K), NPCs (6.3K), items (175K), encounters (946), coords (54K), timelines (32K), costs (19K)
- Phase 2: 30 supplementary loaders covering:
  - Core DBs: Objects (22K), Mounts (1.5K), Pets (1K), Flight Paths (1.3K), Reagents (31K), Achievements (13K)
  - Item enrichment: itemDB.json (157K), .dynamic per-expansion metadata (310K), auto-sources (99K)
  - Transmog: Sets (3.6K), Set Items (57K), Illusions (86), Missing Transmog audit (76K)
  - Professions: 15 profession files (5.6K recipes), Glyphs (690)
  - Expansion systems: Runeforge powers (261), Conduits (286), Blueprints (68)
  - Filters: FilterDB (58), Item Filters RWP (136K), ClassInfo specs (57), Instances (207)
  - Audit: Missing Items (29K), Missing Quests (3.1K)
  - Collectibles: Mount Mods, Music Rolls, Pepe, Pocopoc (490 total)
- Commit `b1f0bd0` (wago-tooling repo)


Chronological log of all database, code, and infrastructure changes. Each entry includes the session number, what changed, and the commit hash where applicable.

---

## Mar 5, 2026

### Session 60 — Transmog Phase 2 Fixes (Mar 5 2026)
- **H1 fix** (clear spell desync): `spell_clear_transmog.cpp` now syncs cleared state to active outfit — zeros Appearances[], Enchants[], SecondaryShoulderApparanceID, calls SetEquipmentSet() for DB persist + ViewedOutfit rebuild
- **M4 fix** (illusion bootstrap): `fillOutfitData` bootstraps weapon enchant illusions from equipped items when outfit doesn't define them — illusions no longer vanish on relog
- **Diagnostic probe**: TransmogBridge.lua logs all 4 API sources per slot at CommitAndApplyAllPending timing — data needed for v2 addon rewrite
- Commits: `5d38823153` (Phase 2 fixes), `69a725cc59` (probe + docs)

### Session 59 — Transmog QA Audit + Phase 1 Fixes (Mar 5 2026)
- **Full QA audit**: 3 parallel agents audited TransmogBridge addon, server handlers, packet parsing, UpdateField sync. Found 2 critical + 4 medium + 5 low issues
- **Bug E fix** (single-item transmog → full rebuild): `HandleTransmogrifyItems` now calls `SetEquipmentSet()` after syncing changes — persists to DB, refreshes ViewedOutfit
- **Bug B fix** (old head/shoulder persist): Added `_activeTransmogOutfitID` tracking. ViewedOutfit now renders the actually-applied outfit instead of always the lowest SetID
- **IgnoreMask fix**: `HandleTransmogOutfitUpdateInfo` preserves existing IgnoreMask instead of clobbering with uninitialized value
- **Packet hardening**: Sanity cap (256), accumulation handling (last-30), ordinal comment fix
- Commits: `289677be44` (Phase 1), `12bc18f374` (packets), `3b8d14c7c6` (gitignore)

### Session 58 — Wowhead Gap Scraper (Mar 5 2026)
- Built 3-script pipeline: generate_gap_targets.py → scrape_gaps_tor.py → import_scraped_gaps.py
- Scraped 5,653 Wowhead pages via 30 Tor workers at 45K/hr (0 WAF blocks)
- Fixed parsers: quest pages use WH.markup.printHtml custom markup, vendor data needs balanced-bracket JSON extraction
- Applied: 592 creature quest starters, 683 creature quest enders, 202 GO starters, 208 GO enders, 8,799 vendor items
- Reverted gossip import (56 NPCs) — scraper picked up user comments instead of NPC dialogue
- Cleaned 73 orphaned GO quest entries (TWW Candy Buckets without templates)
- Also committed: audit_talent_spells.py (183 critical + 242 high priority broken talent spells identified)


### 2026-03-05 â€” Website QA Round 2
- Cross-page sidebar, back-to-top, accuracy audit (30/30 stats verified, 7 stale values fixed)
- Architecture diagram, Konami easter egg
- Commit: `068c81c`

### Session 55 â€” VoxCore Website QA Round 2
- Site architecture overhaul: cross-page sidebar, external CSS/JS, accuracy audit (30/30 stats verified)
- `refresh_content.py` + `update_site.bat` build pipeline
- Commit: `068c81c` (roleplaycore-report)

### Session 53 â€” TACT/Wago CSV Merge Pipeline
- **TACT vs Wago audit**: 251K missing SpellEffect rows in Wago, 7,119 stale hotfix overrides found
- **`merge_csv_sources.py` created** â€” TACT base + Wago CDN extras (998 TACT-only + 99 merged tables)
- `wago_common.py` WAGO_CSV_DIR auto-points to merged output â€” zero downstream changes needed
- `tact_extract.py` QA: fixed default output (data-loss risk), 3 file handle leaks
- Commits: `1c3534d`+`bcbc07f` (wago-tooling)

### Session 51 â€” Missing Spawns + Phase Resolution + ATT Import
- **1,755 quest NPC spawns** deployed (84% reduction in missing quest NPCs)
- **Phase-duplicate resolution**: 214 entries analyzed, 207 re-inserted, 7 REMOVED skipped
- **ATT data applied**: 4,630 quest starters, 3,081 quest chains, 1,510 vendor items
- **QA fixes**: 11 HealthModifier=0, 30,130 orphaned waypoint nodes, 51 orphaned loot refs
- Commits: `68e154e68c`, `fcf1cf2738`, `1d53f2a1d3`, `04c0d4652c`

### 2026-03-05 â€” Transmog: Retail Sniffer-Informed Fix Pass
- Decoded Ymir retail packet capture (build 66220, 2.77M lines) â€” ground truth for transmog packet analysis
- **Retail discovery**: 30 entries per outfit (12 armor + 18 weapon options), DT=3 = hidden visual IMA (not "remove transmog")
- Reverted wrong DT=3 assignment (DT=3+IMA=0 doesn't exist on retail) â†’ simple (imaID > 0) ? 1 : 0
- Reverted last-group-only merge (growing packets were wrong diagnosis) â†’ restored first-non-zero-wins
- Added IgnoreMask repair pass in fillOutfitData
- Found 11 hidden visual IMA IDs (77343-198608 range)
- Commit: `fae00afb86`

### 2026-03-05 â€” DBCD Audit + Broadcast Text Fill
- Built DB2Query CLI tool using DBCD 2.2.0 for retail DB2 binary cross-reference
- Removed 363 redundant hotfix rows across 13 tables (verified identical to retail DB2)
- Filled 393 missing broadcast_text entries from Wago DB2
- creature_text broadcast_text coverage: 335 missing â†’ 0 (100% complete)
- Commit: `faec6435de`

### Session 50 â€” AllTheThings Database Parser
- **ATT Database parser built** (`att_parser.py`): Full Lua tokenizer + parser for the AllTheThings Database repo (1,576 files, 47K quests extracted)
- **Validated SQL generator** (`att_generate_sql.py`): Cross-references ATT data against TC MySQL, filters deprecated/DNT quests, validates all IDs
- **8,950 new rows ready to apply**:
  - 4,359 `creature_queststarter` â€” quest-giver NPC assignments
  - 3,081 `quest_template_addon` PrevQuestID â€” quest chain prerequisite links
  - 1,510 `npc_vendor` â€” vendor inventory items
- Tools committed to `VoxCore84/wago-tooling`: `81cf71a`

### Session 49 â€” TDB Delta + Scraper Hardening
- **TDB 1200.26021 delta applied**: quest_offer_reward 18,054 â†’ 20,022 (+1,967), quest_request_items +69
- **Wowhead 403 resolved** â€” expired on its own, scraper upgraded with curl_cffi Chrome131 TLS
- **27,328 quests** ready for reward text scrape (~2 hours via two-phase approach)
- Commits: `e6b44edab3` (RoleplayCore), `f594f1b`+`7a9667b`+`80d42e8` (wago-tooling)

### Session 47 â€” Gist Accuracy Audit + hotfix_data R3 Cleanup
- **hotfix_data orphan cleanup**: `cleanup_hotfix_data_orphans.py` removed 608,401 orphaned entries. 226,984 remaining (was 835,385). Hotfixes DB 637â†’535 MB
- **Gist accuracy audit**: Verified all report numbers against live DB. Fixed 6 critical errors:
  - smart_scripts: 792Kâ†’294,425 (imports cleaned by validation scripts)
  - Part 11 hotfix tables: pre-audit numbers replaced with post-audit actuals
  - hotfix_data: 835Kâ†’227K (R3 registry cleanup applied)
  - DB sizes: world 1,267 MB, hotfixes 535 MB (previously stale)
- **OPTIMIZE/ANALYZE** on hotfix_data + 8 tables with stale InnoDB statistics
- Commit: `21fa23b0d1`

### Session 46 â€” WPP Script Hardening
- **20-bug QA** across 4 files: `start-worldserver.sh`, `extract_transmog_packets.py`, `wpp-inspect.sh`, `opcode_analyzer.py`
- Root cause: runtime `start-worldserver.sh` had stale WPP path (`out/` subdir)
- EXIT trap for bnetserver, `$WPP` full path, `cd` error guards, `set -o pipefail`, streaming packet extraction
- Commit: `8584c3c2e0` + tc-packet-tools `821e74f`

### Session 45 â€” DB Report Update
- **Hotfix audit tools committed**: `hotfix_differ_r3.py`, `gen_practical_sql_r3.py`, `build_table_info_r3.py`, `merge_results.py` + README.md + .gitignore
- **Gist created**: `528e801b53f6c62ce2e5c2ffe7e63e29` â€” comprehensive database report (Parts 1-16)
- Commit: `9ae9d40788`

### Session 44 â€” Tools Consolidation
- Moved `C:\Users\atayl\OneDrive\Desktop\Excluded\` â†’ `~/VoxCore/ExtTools/`
- Fixed WPP path in 13 files across 4 repos
- Added 7 missing tools to inventory
- Pushed: wago-tooling `b56bfb0`, tc-packet-tools `d956a5a`, trinitycore-claude-skills `25967f7`

### Session 43 â€” CTD Fix + SmartAI Cleanup
- **Missing CTD rows**: 26,745 creatures missing DifficultyID=0 â†’ 0 remaining
  - Step 1a: 24,070 Diff2â†’Diff0 copies
  - Step 1b: 68 from other difficulties
  - Step 2: 2,607 default Diff0 rows
- **SmartAI orphans**: 5,894 creatures with AIName='SmartAI' but no scripts â†’ cleared
  - +181 GUID-based script creatures restored (missed by entry-only check)
- **AIName fixes**: 3 data errors ('0', 'CombaAI' typo)
- **ContentTuning enrichment**: 4,820 spawned CT=0 creatures â†’ zone/neighbor lookup
- Commit: `f0782d5030`, `9536a248b6`

## Mar 4, 2026

### Sessions 37-38 â€” Phased Cleanup
- 3 SQL fixes, 11 Stormwind CTD rows
- Hotfix R2 cleanup: 204K redundant rows removed
- Companion hotfix_data cleanup: 174,799 orphaned entries
- `.gitignore` updates
- Commit: `22d3f83d57`

### Sessions 35-36 â€” Hotfix R3 Audit + Transmog Diagnostics
- **R3 type-aware audit**: 109 tables, 768K redundant rows removed
  - Float32 IEEE 754 bit-level comparison
  - Signed/unsigned int32 pattern matching
  - Logical PK overrides (broadcast_text_duration)
- **Transmog diagnostic build**: secondary shoulder fix, deleted set skip, caller tracing
- Commit: `c1e9a53c84`

### Session 34 â€” Auth Key Update
- Reverted auth bypass at WorldSocket.cpp
- Applied TC build 66220 auth keys (7 keys)
- Commit: `8bbd610fc7`

### Session 33 â€” Creature DB2 Orphans
- 137 orphaned hotfix_data entries for Creature DB2 (hash 0xC9D6B6B3) removed
- Commit: `319c2781cb`

### Session 32 â€” Repo Cleanup
- docs â†’ `doc/`, tools â†’ `tools/`, batch scripts â†’ `tools/build/`
- Deleted 1.4 GB hotfix_audit output + junk from repo root
- Commit: `a7cf01b4ba`

### Session 31 â€” Transmog Client Wiki
- 3,487-line reference wiki + 119-line cheatsheet from 15 Blizzard Lua/XML source files
- Commit: `ad8f9eaa9f`

## Mar 3, 2026

### Sessions 13-30 â€” Major Data Pipeline Day
- **Wowhead mega-audit**: 216,284 NPCs scraped, 54,571 operations across 3 tiers
- **Raidbots/Wago pipeline**: 1.6M item locale entries, 21K quest chains, 135K quest POI
- **LW import #2**: 665,658 net new rows across 21 tables (5-phase dependency ordering)
- **Post-import cleanup**: 47,478 rows cleaned, 627K error lines resolved
- **Hotfix repair build 66220**: 388 tables, 103K inserts, 1.8K column fixes
- **MySQL tuning**: tmp_table_size 1KBâ†’256MB, buffer pool 16GB, warm restarts
- **Build diff audit**: 5 builds diffed, Wago oscillation detected, zero breaking changes
- **Hotfix pipeline crash fix**: 6 bugs across 3 C++ files (chunked delivery, ByteBuffer assert)
- **Transmog 4-bug fix**: Commit `272c373105`

## Mar 1, 2026

### Sessions 11-12 â€” Transmog Confirmed Working
- 14/14 manual clicks, 13/14 outfit loading (secondary shoulder gap)
- PR cleanup and cross-repo PR #760 on KamiliaBlow/RoleplayCore

## Feb 28, 2026

### Sessions 8-10 â€” Quest/GO Audits + TransmogBridge
- GO/quest audit tools + 2,279 DB fixes
- TransmogBridge addon implementation (3-layer hybrid merge)
- Creature/GO placement audits vs LoreWalkerTDB

## Feb 27, 2026

### Sessions 2-7 â€” Foundation
- 5-database audit: 148 checks, 412K dead rows removed
- LW import #1: 385,823 rows across 17 tables
- NPC audit tool (27 checks), 3-batch NPC fixes (23,904 operations)
- Placement audit tools built
- Loot table PK discovery + deduplication (193K pre-existing dupes + 3M import dupes)
- Backup table cleanup: 101 tables dropped, 382 MB reclaimed
- MyISAMâ†’InnoDB migration (7 tables)

## Feb 26, 2026

### Session 1 â€” Initial Setup
- Companion AI fix
- Transmog wireDT fix
- Initial hotfix repair v1

---

## Database State (March 21, 2026)

| Database | Size | Key Metric |
|----------|------|------------|
| world | 1,389 MB | 712K creatures, 226K templates, 457K SmartAI, 5.8K spell scripts |
| hotfixes | 1,135 MB | 400K spell_name, 234K broadcast_text, 176K item_sparse (full 66337 restore) |
| characters | 4 MB | 5 characters, transmog outfits table ready |
| auth | 3 MB | 2 accounts, build 66527 registered |
| roleplay | 0.1 MB | 4 tables (creature_extra, template_extra, custom_npcs, server_settings) |

## Repositories

| Repo | Latest Commit | Purpose |
|------|--------------|---------|
| VoxCore84/RoleplayCore | `98ba6d1fc2` | Main server (TC fork, 12.x/Midnight) |
| VoxCore84/wago-tooling | `2351879` | Wago/LW/hotfix tools |
| VoxCore84/tc-packet-tools | `821e74f` | WPP + packet analysis |
| VoxCore84/code-intel | -- | C++ MCP server (416K symbols) |
| VoxCore84/CreatureCodex | v3.0.0 | Creature spell/aura sniffer addon + server hooks |
| VoxCore84/VoxGM | v1.0.0 | Unified GM control panel addon |
| VoxCore84/draconic-bot | -- | Discord support bot for DraconicWoW |
| VoxCore84/claude-code-scroll-fix | v1.0.0 | Terminal scroll jump fix |
| VoxCore84/claude-code-* (7 repos) | v1.0.0 | guardrails, edit-verifier, sql-safety, windows-toasts, hook-tester, compaction-keeper, workflow-guard |

## AI Infrastructure (March 21, 2026)

| Component | Status | Notes |
|-----------|--------|-------|
| Triad Workflow | ACTIVE | ChatGPT API=Architect, Claude Code=Implementer, Gemini API=Auditor |
| Central Brain | `AI_Studio/0_Central_Brain.md` | Cross-agent coordination blackboard |
| 5-Round Review Cycle | OPERATIONAL | Parallel Phase 1 (3 reviewers) → verify → seal. `review_cycle.py` |
| VoxCore Daemon | Phase 1 COMPLETE | APScheduler, state mgr, Claude API, Discord notify. Phase 2 next |
| DraconicBot | v3.1.0 READY | AI stress test 98%. Oracle Cloud VM provisioned, not yet deployed |
| Release Gate | DEPLOYED | `/pre-ship` audit + 2 enforcement hooks + 3 adversarial agents |
| Auto-Parse | v3+ DEPLOYED | 19-module pipeline, headless via `start_all.bat` |
| DevOps Pipeline | DEPLOYED | 6-step boot, graceful shutdown, SQL pending queue, handover agent |
| Antigravity IDE | DEPRECATED | Gemini accessed via API now |

*Updated March 21, 2026 (session 200)*

