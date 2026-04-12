# VoxCore Session State — Multi-Tab Coordination

**Read this FIRST in any new Claude Code tab.**
This is the single source of truth for what all tabs are doing, what's done, what's blocked, and what to pick up next. Updated by whichever tab finishes work.

**Last updated**: April 10, 2026 — CalmCore-244 tab (session 244) completed Codebase Intelligence DB + FastMCP server. CalmCore-244 COMPLETE. VoxCore `src/` domain-split commit still pending. (Dean Sides email, Scott Tranchant letter, both OPBs, Tolin rebuttal, NJP forgery analysis). User confirmed 23-24 referral OPB still in official record and held-past-SCOD delay was purposeful to enable forgery-based NJP finalization. `AI_Studio/Reports/memory_patch_23-24_OPB_and_forgery.md` handed off to Case-SME tab for merge. Case-SME is concurrently in error-correction phase of 9-pass SME sweep + editing `memory/case-contacts.md` — my case-contacts edits are in the patch file, NOT applied, to avoid collision. Mbox-Fast tab finished massive parallel work (45 audio transcribed, unredact toolchain, 13 FOIA redaction identifications, Lawrence QAI audio smoking gun, FOIA draft ready) — commit `737f50cc9d`. Zero physical ops in `Case_Reference/` until Case-SME finishes error-correction phase.

---

## Active Tabs & Assignments

| Tab | Assignment | Status | Notes |
|-----|-----------|--------|-------|
| Main (229) | Warlock Phase 4 modernization + Tier B triage | COMPLETE | 15 old-style handlers → RegisterSpellScript. Triage: 147 TC-native / 22 real TODO. Handoffs below. |
| **Warlock-A1** | Tier A: Summon Demonic Tyrant (265187) | COMPLETE | `spell_warl_summon_demonic_tyrant`: demon duration +15s, Reign of Tyranny buff. SQL: `_06_world.sql`. Commit: `0d4717c013` |
| **Warlock-A2** | Tier A: Mayhem (387506) | READY | Destruction. Spell duplication to secondary target. See handoff below. |
| **Warlock-A3** | Tier A: Demonic Soul (449614) | COMPLETE | Aura 396 = native TRIGGER_SPELL_ON_POWER_AMOUNT (not PROC_TRIGGER_SPELL_COPY). Handler on 450510 chains to AoE damage burst 449801 (3.53 SP coeff). SQL: `_07_world.sql`. Commit: `0d4717c013` |
| **Warlock-B** | Tier B: 6 Summon spells | COMPLETE | 5 handlers + 1 TC_NATIVE. C++ compiles clean (LNK1104 = server running). SQL: `2026_04_04_08_world.sql`. |
| **Warlock-C** | Tier C: 5 Class utilities | COMPLETE | Demon Skin + Soul Link handlers. Mortal Coil/Soulburn/Ichor TC_NATIVE. Deep audit: GetPet() fix. SQL: `_09_world.sql`. Commits: `0d4717c013`, `51d8381bd1`. |
| **Warlock-D** | Tier D: 8 MAYBE spells + deep audit | COMPLETE | All 8 resolved. Deep audit: fixed 5 orphan DB entries (2 name mismatches + 3 stale), Soul Link pet check. SQL: `_11`. Commits: `0d4717c013`, `51d8381bd1`. |
| **CC-297-Refresh** | Re-extract claude-code 2.1.97 sources, diff vs 2.1.88, patch all 22 internals reports + memory file | COMPLETE | Commit: `1e8db7592d`. 8 tracked reports + README committed (188 insertions, 18 deletions). 14 more reports updated on disk (gitignored). memory/claude-code-internals.md updated (outside git, ~/.claude/projects/). 15 findings files in `_2.1.97_refresh/`. Sourcemap extraction impossible (Anthropic stripped cli.js.map post-2.1.88 leak); used hybrid changelog + cli.js grep methodology instead. |
| **Hook-Daemon** | Persistent asyncio HTTP daemon replacing per-hook Python subprocess spawns. 18/20 hooks converted from `type:"command"` → `type:"http"` pointing at `localhost:19484/hook/<name>`. | COMPLETE (2026-04-09) | `hook_daemon.py` (1,244 lines, stdlib only), `daemon_shim.py` (72 lines, SessionStart auto-starter), `release-gate-enforce.py` refactored exit(2)→JSON block, `test-hooks.py` added Phase 0 + Phase 3 HTTP integration tests, `start_all.bat` step 7 added, handoff rewritten. Latency: 119ms → 0.94ms avg (~127× faster). Tests: 49/49 passing. Live-verified during implementation session. Daemon auto-starts via SessionStart shim on every CC boot. Rollback: one `git checkout` + `taskkill`. The original handoff proposed a TCP+dispatch.py architecture that would've still spawned Python per hook — 2.1.97 source refresh revealed `type:"http"` hooks as the correct primitive. |
| **Case-SME** | SME sweep of IMPORTANT DOCS (9 passes complete) → infrastructure build (extract_cache, audio_transcribe_v2 with GPU, ocr_images, rag_build/query, /sme-sweep, /rag-search) → Pass 7-9 + RAG ingest → user corrections (Chad/CWI/Tolin) | ACTIVE | Started 2026-04-09. Output: `AI_Studio/Reports/sme_importantdocs/` (12 pass reports + 5 dossiers + master_timeline.md + pii_redaction_report.md + sprint reports). New tools in `tools/` (extract_cache.py, audio_transcribe_v2.py, ocr_images.py, rag_build.py, rag_query.py). New skills `/sme-sweep`, `/rag-search`. New memory files: `sme-sweep-infrastructure.md`, `case-audio-recordings.md`. RAG live: 9082 chunks at `.cache/rag/chroma/`. Currently in error-correction phase (CWI/QAI conflation, Tolin payment, Chad→Robert L. Johnston). Owns: `AI_Studio/Reports/sme_importantdocs/`, `tools/{extract_cache,audio_transcribe_v2,ocr_images,rag_build,rag_query}.py`, `.claude/commands/{sme-sweep,rag-search}.md`, `memory/{case-audio-recordings,sme-sweep-infrastructure}.md`, `memory/case-contacts.md` (edits per Pass 5/7), `.cache/{extracted,ocr,transcribed,rag}/`. Does NOT touch: `Case_Reference/` actual files (read-only) — JD-Planner restructure must wait until I finish all corrections. |
| **JD-Planner** | Design Johnny Decimal `XXX_YYY` restructure for `Case_Reference/` (proposal only, zero file ops) | COMPLETE (2026-04-09) | Deliverables: (1) `AI_Studio/Reports/case_archive_jd_proposal.md` (~550 lines) — 29-category JD map + 21-rename plan + `_INDEX.json` schema + 7-phase runbook + 3 approval gates. (2) `tools/migrate_case_archive_jd.py` (~920 lines, stdlib only) — staging+swap pattern, 5 subcommands (index/plan/migrate/verify/delete), folder renames only (filenames preserved for chain-of-custody), pre/post sha256 verification. Survey: 1,895 files, 27 top-level entries, 3 number collisions, 4 duplicate folders, 6 loose entries, 1 audio misfile. Script committed as part of `3551a0bb39` (Split-Audit tab's harvest). **Zero physical ops in `Case_Reference/`.** Execution blocked on Case-SME tab completion — resumption playbook in `memory/todo.md` Next Session. |
| **Mbox-Fast + Recordings + Unredact** | Session 240. (1) `tools/mbox/parallel.py` bug fixes (contentless FTS5 DELETE, ATTACH cross-DB persistence, journal_mode=MEMORY, attachment dedup FK, argparse %, cp1252 stdout). Live DB 17,050 msgs / 3,248 atts. (2) Transcribed **45 audio files** across `Recordings/` (24) + `Recordings Pt 2/` (21, 419 MB) via `audio_transcribe_v2.py --model medium --device cuda`. Caches: `Recordings_816ff3c0e3` + `Recordings_Pt_2_d392a2a3c7`. **Smoking gun**: 5-hr self-identification recording with explicit IHPP coercion language; Capt Lawrence's QAI interview opening captured in `Cannon Air Force Base 3.m4a`. RAG rebuilt: 9,082 chunks. (3) Built **`tools/unredact/`** (10 modules, 3,645 lines) + applied to QAI binder 2025-01787-F: **60 Category 1 pages recovered** + **13 high-confidence ChatGPT gpt-5.4 identifications** (Cermak email ×6, Elliot Ko ×2, McMaster, Taylor ×5) + **forensic chain of custody**: PDF author metadata `1565647551A` matches digital signature ID of **USAF FOIA Manager Victor Delgado-Cusibichan** on the 2025-01787-F Closure Letter. (4) FOIA draft at `Desktop/Excluded/unredact/foia_drafts/FOIA_REQUEST_2026-04_GAP_PERIOD.md` — gap period 12/19/2024–present, 5 parallel component filings, Section 8 over-redaction challenge citing *Stern v. FBI* and *Lesar v. DOJ*. Ready for Tolin review. | COMPLETE (2026-04-09) | Commit: `737f50cc9d` (11 files, +3,645 / -2). Owned: `tools/mbox/*`, `tools/unredact/*`, `Desktop/Excluded/mbox/mbox_index.db`, `Desktop/Excluded/unredact/*`, `.cache/transcribed/Recordings_*`. Did NOT commit: `src/` deletions (Split-Audit in-progress), `AI_Studio/0_Central_Brain.md` (ghost M flag — no actual staged diff). Used read-only: `tools/rag_*.py`, `tools/audio_transcribe*.py`. |
| **Split-Audit** | VoxCore/CalmCore split audit under new rule (all WoW → CalmCore, all non-WoW → VoxCore). Produced `AI_Studio/Reports/voxcore_calmcore_split.md` (34-delete-from-CalmCore, 29-delete-from-VoxCore inventory). Committed Tier 1 isolation in CalmCore (`ce2cdc4f86`, `e5a4b8880b` — local only, no remote). Harvested session 238 non-WoW tooling into VoxCore commit `3551a0bb39`. | COMPLETE (2026-04-09) | Touched: VoxCore `.claude/agents/redaction-scanner/`, `.claude/agents/evidence-cataloger/`, `AI_Studio/Reports/voxcore_calmcore_split.md`, `doc/session_state.md`, `memory/{recent-work,todo,improvements}.md`. Touched in CalmCore: `.claude/settings.json`, `.claude/commands/{build-loop,handoff,person-dossier}.md`, `.claude/agents/file-sorter/CLAUDE.md`, `.claude/hooks/{docx-auto-extract,stop-verify}.py`, `tools/shortcuts/create_shortcuts.py`. Did NOT touch: `tools/mbox/*` (Mbox-Fast owns), `Case_Reference/*` or `memory/case-*.md` (Case-SME owns), `src/` (user manually wiped + recovered mid-session). |
| **Case-DCSA-Review** | Session 240: Non-invasive legal document review + DCSA SIR package pre-review + 23-24 OPB / NJP forgery analysis. User-driven thread, no code written, all output is analysis/advice + memory patch. (1) Drafted Dean Sides SAPRO update email (re: SA Grice AFOSI §1044e victim access denial) with a per-se conflict-of-interest paragraph that translates user's "I don't trust anyone at Cannon" into actionable structural language Dean can forward to HAF/A1Z on Mon Apr 13. (2) Reviewed Scott Tranchant DCSA supervisor letter draft — top-tier Guideline I rebuttal with three small fixes flagged (letterhead 27 SOSS→27 SOW/MAFR, Para 7 typo "responsibility"→"responsibly", CUI marking inconsistency) and Tolin P7 signoff gate preserved. (3) Reviewed 24-25 OPB — rater Lt Col Etienne (memory-hostile insider threat actor, now 27 SOMRS/CC) + HLR Col Earles (memory-hostile DHA privilege revoker, active AFBCMR target) + **66 days supervised** (possibly below DAFI 36-2406 120-day minimum) + duty title/bullet mismatch documenting Aug 2024 removal from clinical duties. (4) Reviewed 23-24 referral OPB + Tolin 2 Feb 2025 rebuttal — **user confirmed referral still in official record** (Tolin rebuttal unsuccessful), NJP was still pending at referral (Tolin Para 2 procedurally fatal if true), and **121-day held-past-SCOD delay was purposeful** to allow NJP forgery documentation to be finalized ("they kept my OPB open past the SCOD of 31 Aug just so they could finally get the forged signature and forged initials and other B.S. taken care of and still give me the NJP and referral OPB"). (5) Identified Wareham 21 Oct 2024 supplemental appeal as **earliest formal §1034 reprisal claim in case record** — predates every later IG/congressional/AFBCMR element. (6) Flagged Labor Day 2024 suicide attempt as documented in Adam's own Sep 10 2024 NJP response ("drove me to attempt suicide") — same event as Sep 3 2024 DISS IR currently subject of DCSA SIR. (7) Plaud audio export walkthrough for user; recordings to be handed off to Mbox-Fast tab's `Desktop/Excluded/Recordings/` pipeline. (8) SVC vs VLC terminology explainer. **Cross-tab note**: Case-SME tab is actively editing `memory/case-contacts.md` per its own Pass 5/7 (Chad→Johnston, CWI/QAI conflation, Tolin payment) — my case-contacts.md edits are in patch file, not applied, to avoid collision. **Cross-tab overlap**: Mbox-Fast tab's `Cannon Air Force Base 3.m4a` (Lawrence QAI interview opening) may corroborate the 23-24 referral OPB / NJP forgery timeline documented in this session — Case-SME should cross-reference when merging the patch. | COMPLETE (2026-04-09) | Touched: `doc/session_state.md` (this row), `AI_Studio/Reports/memory_patch_23-24_OPB_and_forgery.md` (new, ~430 lines — handoff to Case-SME), `AI_Studio/0_Central_Brain.md` (timestamp + focus refresh), `memory/recent-work.md` (session 240 entry), `memory/todo.md` (Next Session refresh), `memory/improvements.md` (session 240 retro). Did NOT touch: `memory/case-*.md` (Case-SME owns — patch file handed off instead), `Case_Reference/*` (Case-SME + JD-Planner ownership chain), `tools/mbox/*` or `.cache/transcribed/*` (Mbox-Fast owns), `tools/unredact/*` (Mbox-Fast owns), `src/` (pending split execution). **No code written.** All output is legal analysis, email drafts for user review, coordination docs, and the case memory patch file for Case-SME handoff. |
| **CalmCore-244** | Session 244: Built CalmCore Codebase Intelligence DB + FastMCP MCP server. `tools/build_code_index.py` (9-layer builder: 2222 files, 197K functions, 25K classes, 1001 opcodes, 494 aura handlers, 648 config keys, 823 AddSC_ registrations, 275 digest sections). `tools/codebase_db_server.py` (9 FastMCP tools: codebase_query, codebase_search, codebase_stats, codebase_digest, etc.). `.claude.json` MCP registration. DB at `.cache/codebase.db` (42MB, rebuilds in 17s). CalmCore push still failing HTTP 500 (1.2GB repo). | COMPLETE (2026-04-10) | Commit: CalmCore `7a020c45b3`. Owns: `CalmCore/tools/build_code_index.py`, `CalmCore/tools/codebase_db_server.py`, `CalmCore/.claude.json`. Did NOT touch: `Case_Reference/`, `memory/case-*.md`, `tools/mbox/*`, `tools/unredact/*`, VoxCore `src/` (domain-split pending). |
| **CalmCore-245** | Session 245: DB findings triage + tiered digest + NPCHandler crash fix. (1) Script loader triage: 2 real bugs fixed (`AddSC_ironforge` + `AddSC_darkmoon_mount_race`), 4 false positives in DB builder fixed (L6f now matches `scripts_loader` + ScriptMgr.cpp → 0 unregistered). (2) Config mismatch triage: 3 real bugs fixed (`fatigue.enabled`, `fishing_fix.enabled`, `SoloLFG.Enable` code defaults), 18 false positives (bool/int repr). (3) Tiered digest system: `digest_source.py` extended with `--tier auto|xlarge|large|medium|small|tiny`, batch calling for small/tiny, `--gen-list` from DB. 5 tier target lists generated (2,185 files, ~$36 total). (4) NPCHandler null-crash: removed 2 dead `else` branches calling `go->AI()` on null. (5) GitHub push: HTTP 500 blocked by early commits with 38-44MB SQL blobs — `tools/push_incremental.py` written. Root cause: need SSH or `git filter-repo --strip-blobs-bigger-than 20M`. All builds clean. | COMPLETE (2026-04-10) | Commits: CalmCore `6a5ddfe390`, `424db8e670`, `3f4d74700c`, `99b51efef4`. Owned: `CalmCore/src/server/scripts/DarkmoonIsland/darkmoon_island_script_loader.cpp`, `CalmCore/src/server/scripts/EasternKingdoms/eastern_kingdoms_script_loader.cpp`, `CalmCore/src/server/game/DungeonFinding/LFGScripts.cpp`, `CalmCore/src/server/game/Entities/Player/Player.cpp`, `CalmCore/src/server/game/Handlers/NPCHandler.cpp`, `CalmCore/tools/build_code_index.py`, `CalmCore/tools/digest_source.py`, `CalmCore/tools/push_incremental.py`, `CalmCore/tools/digest_targets_*.txt`. Did NOT touch: `Case_Reference/`, `memory/case-*.md`, VoxCore `src/` (domain-split pending). |
| **docs-rag-247** | Session 247: Built docs-rag MCP server (`tools-dev/docs-rag/`, 2 files, 6 FastMCP tools). Semantic vector search over IMPORTANT DOCS via ChromaDB + Ollama nomic-embed-text. Tools: `docs_rag_search`, `docs_rag_read`, `docs_rag_list`, `docs_rag_status`, `docs_rag_rebuild`, `docs_rag_reload`. Registered in `.mcp.json` + `settings.local.json`. All tools verified working. Index: 9,082 chunks (Angel_VA full, others partial from legacy sweep). Next: `docs_rag_rebuild()` to extract remaining 5 folders. | COMPLETE (2026-04-11) | Commit: `e880aae2d4`. Owns: `tools-dev/docs-rag/*`, `.mcp.json` docs-rag entry, `.claude/settings.local.json` enabledMcpjsonServers. Did NOT touch: `Case_Reference/`, `memory/case-*.md`, `src/`. |
| **SME-sweep-248** | Session 248: Full SME sweep of IMPORTANT DOCS + SAPR-standard ___MASTER.md architecture. (1) 11-phase SME sweep: 2,265 files, 826 extracted, 739 OCR'd, 7 audio transcribed. 7 parallel content mapping agents. 17 contradictions, 10 memory edits. (2) 4-pass dedup/archive/master: 30 ___MASTER files written to SAPR standard, 29 ___INDEX.json, ___ALL_MASTERS.md manifest. (3) Hub documents renamed 01-14 by importance. VA Benefits content restored. (4) Verification passes: automated filename/path/count/duration checks. (5) Quick fixes: Wareham email, "400+" sessions, OPB OCR, misplaced files resolved. (6) docs-rag rebuilt to 25,820 chunks. (7) Monday HAF call prep prompt written to Desktop. | COMPLETE (2026-04-11) | Commit: `8129fce161`. Touched: `AI_Studio/Reports/sme_important_docs/` (20 files), `doc/session_state.md`, `AI_Studio/0_Central_Brain.md`, 10 memory files. Created on IMPORTANT DOCS Desktop: 30 ___MASTER files, 29 ___INDEX.json, ___ALL_MASTERS.md, 14 subfolder _Archive/ entries, OPB OCR. Did NOT touch: `src/`, CalmCore, Case_Reference source files (only added ___MASTER/___INDEX + copied SAPR packet). |
| **Session-251** | Cron fix + Clinical summary + HAF call prep audit | COMPLETE (2026-04-11) | Banned recurring CronCreate (3-layer defense, both repos). Built 399-line clinical summary from 66 MH notes (5 parallel agents). Full packet audit + "how bad is this" assessment + forms/filings checklist. Commit: `1deb72a8db`. Did NOT touch: `src/`, CalmCore code, Case_Reference source files. Created: `Monday_HAF_Call_13Apr2026/11_CLINICAL_SUMMARY_FOR_TOLIN.md`, `Case_Reference/06_CLINICAL_RECORDS/MHS_Genesis_Clinical_Summary_Oct2024_Mar2026.md`. |
| -- | -- | -- | Add rows as tabs are opened |

**Rule**: Before starting work, check this file. If another tab owns a file or task, don't touch it. Update your row when you start and when you finish.

---


> Historical tab rows (sessions 107-228) and Warlock session 229 handoff prompts archived to [session_state_archive.md](session_state_archive.md).

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
- **Server**: RUNNING (PID 33360, 22GB RAM)
- **Client**: 12.0.1.66709
- **DB**: world ~1,400 MB (TC TDB + LoreWalker merged) | hotfixes ~900 MB (TC + LW) | characters 4 MB
- **Logs**: 6.8M DBErrors from LAST boot (pre-cleanup). After Phase 1-2a (886K rows) + Phase 4 (32K world + 20K hotfix), expect ~2-3M remaining (loot items, flags, serverside spells — not fixable with SQL). Zero crashes.
- **LoreWalker TDB**: APPLIED. **TC TDB 1200.26021**: BACKFILLED via INSERT IGNORE (session 227). Both data sets coexist. **RoleplayCore SQL**: Re-applied (session 228 Tab A).

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

> **Note**: Transmog Outfits UI work is ARCHIVED — reimplemented externally. All transmog bugs, slash commands, and agents have been removed. Historical docs preserved in `doc/archive/transmog.md` and `doc/transmog_*`.

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
| 227 (main) | VoxSniffer Combat Audit v1 | CombatAudit.lua + ProcExpectations.lua + audit_report.py. Gemini audit PASS after 4 HIGH fixes. `5cd63fdd3f` |
| 227 (tab 2) | DB error cleanup + TC TDB backfill | Phase 1-2a cleanup (886K rows), TC TDB INSERT IGNORE (771+433 tables), 5 mismatch fixes, 329 removed items purged, plan doc. `eef19fe221` |
| 224 | Session 222/223 wrap-up + optimization application | 13 skills conditional, FileChanged hook, SubagentStart/ConfigChange hooks, SME handoff prompt. `8f01aa113c` |
| 223 | Claude Code Tier 2 reports (1M tab) | 7 reports (5,550 lines): tool pipeline, swarm, coordinator, hooks, permissions, skills, MCP. 4 audit agents: concurrency, hooks, skills paths, fork mode. 13 skills made conditional |
| 222 | Claude Code internals research + config optimizations | 11 reports (266KB), 1M context enabled, 3 conditional rules, .gitignore optimized (205→15 untracked), 54 memory frontmatter files. Source: `C:/Users/atayl/Desktop/claude-code-source/`. `0916b667c9` |
| 221 | Swift Crusade spell + timestamp hook fix | Custom spell 1900031 (+100% move speed, +200% mounted ground+flight). Timestamp hook statusMessage added for terminal visibility. `0916b667c9` |
| 220 | Bnetserver fix + Chrono Surge spell + DB schema repair | Port 1119 fix, custom spell 1900030 (+250% haste/-75% CD), 3 DB schema fixes (crafting columns/tables for TC sync), duplicate process cleanup. `54d9ef6621` |
| 215 | Angel VA TDIU (21-8940) filing support | Filled PDF (103 XFA fields), draft answers doc, migraine legal analysis (3 decisions), 4 buddy statements (Adam v2 + 3 templates), neurologist letter template, complete action plan, print-ready Item 26 continuation sheet. All in Desktop/Excluded/Angel_VA/. No VoxCore commit |
| 214 | Gemini Pro VoxCore business briefing | 15-doc package (1,440 lines combined). 10 memory + 11 desktop files synthesized. Identity correction for wrong VoxCore. Google ecosystem + Triad-to-Vertex migration mapped. `c690e31568` |
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

## GitHub Gists (synced April 4 — session 228)

- DB Report: https://gist.github.com/528e801b53f6c62ce2e5c2ffe7e63e29
- Changelog: https://gist.github.com/4c63baf8154753d2a89475d9a4f5b2cc
- Open Issues: https://gist.github.com/2b69757faa2a53172c7acb5bfa3ad3c4
