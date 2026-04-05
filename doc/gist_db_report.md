# RoleplayCore Database Report
## Data Quality & Optimization Summary
**Prepared for CaptainCore (LoreWalkerTDB)**
**April 4, 2026 | VoxCore Project — WoW 12.x / Midnight**

> **TL;DR** — Imported ~1M rows from LoreWalkerTDB, repaired 103K hotfix entries, removed 10.6M redundant rows (97.8%), fixed 78K NPCs, added 1.6M item locale translations, and cut server startup from 3m24s → 17s. **April update**: Applied full LoreWalker migration (3.3M rows), backfilled TC TDB 1200.26021 on top, cleaned 886K orphan rows, fixed 428 orphan loot references. Client build 66709.

---

## Navigation

| # | Section | Headline |
|---|---------|----------|
| — | [Executive Summary](#executive-summary) | Full numbers table |
| 1 | [LoreWalkerTDB Integration](#part-1-lorewalkertdb-integration) | ~1M rows imported |
| 2 | [Hotfix Repair System](#part-2-hotfix-repair-system) | 103K inserts + 1.8K fixes |
| 3 | [NPC Audits & Corrections](#part-3-npc-audits-corrections) | 78,475 fixes |
| 4 | [Quest System Enrichment](#part-4-quest-system-enrichment) | 21K chain links |
| 5 | [Localization](#part-5-localization) | 1.6M locale rows |
| 6 | [Database Cleanup & Integrity](#part-6-database-cleanup-integrity) | 412K dead rows removed |
| 7 | [MySQL & Server Performance](#part-7-mysql-server-performance) | 3m24s → 17s startup |
| 8 | [Build Diff Audit (5 Builds)](#part-8-build-diff-audit-5-builds) | Zero breaking changes |
| 9 | [Placement Audits](#part-9-placement-audits) | 31K fixes generated |
| 10 | [Custom Tooling Summary](#part-10-custom-tooling-summary) | 50+ tools built |
| 11 | [Final Database State](#part-11-final-database-state) | Current row counts |
| 12 | [What It All Means for Players](#part-12-what-it-all-means-for-players) | Before / After |
| 13 | [Hotfix Redundancy Audit](#part-13-hotfix-redundancy-audit-complete) | 10.8M → 244K (97.8%) |
| 14 | [Discoveries & Lessons](#part-14-discoveries-lessons-useful-for-the-community) | 9 community findings |
| 15 | [Timeline](#part-15-timeline) | Feb 26 – Mar 7 |
| 16 | [Complete Tooling Catalog](#part-16-complete-tooling-infrastructure-catalog) | Full inventory |
| A | [Data Sources](#appendix-a-data-sources) | 6 sources |
| B | [Reproducibility](#appendix-b-reproducibility) | 6 pipelines |

---

## Executive Summary

Over February-March 2026, RoleplayCore imported, validated, and repaired data from **four major sources** (LoreWalkerTDB, Wago DB2, Raidbots, and Wowhead), performed multi-pass audits across all five databases, and built a Python tooling pipeline to make the process repeatable.

### By the Numbers

All figures are **net** — accounting for subsequent cleanup and deduplication.

| Category | Metric | Value |
|----------|--------|-------|
| **Data Imported** | LoreWalkerTDB world rows | ~1,004,000 net |
| | Hotfix rows repaired | 103,153 inserts + 1,831 column fixes |
| | Item locale translations | 1,628,651 rows across 10 languages |
| | Quest chain links | 21,758 PrevQuestID/NextQuestID updates |
| | Quest POI/objectives | 2,880 POI + 5,199 points + 633 objectives |
| **Data Corrected** | NPC fixes | 78,475 (23,904 audit + 54,571 Wowhead cross-ref) |
| **Data Cleaned** | Hotfix redundancy audit | **10.6M redundant content rows removed (97.8%)** |
| | Pre-existing orphan/dead rows | ~412,000 (initial 5-DB audit) |
| | Pre-existing duplicate loot rows | 193,542 |
| | Post-import cleanup | ~47,000 |
| **Performance** | Server startup | 3m24s → 17s (92% reduction) |
| | Hotfix content tables | 10.8M rows → ~244K rows |

---

<details>
<summary><strong>Part 1: LoreWalkerTDB Integration</strong> &mdash; <em>~1M rows from LoreWalkerTDB — hotfixes, SmartAI, world data, loot</em></summary>

### 1.1 Hotfixes Import

LoreWalkerTDB's `hotfixes.sql` (322MB) was parsed and selectively imported. Blind import wasn't possible — RoleplayCore has custom entries (e.g., `broadcast_text` entries at 999999997+, custom `chr_customization_choice` data).

**Key gains:**

| Table | Rows Added | What it fixed |
|-------|-----------|--------|
| spell_item_enchantment | +1,193 | Missing enchant effects |
| sound_kit | +3,611 | Missing ambient/NPC/spell sounds |
| item + item_sparse | +2,799 / +2,810 | Items that existed in client but had no server data |
| spell_effect | +1,335 | Missing spell effects |
| spell_visual_kit | +610 | Missing visual effects |
| creature_display_info | +123 | Missing NPC models |
| phase | +595 | Missing phase definitions |
| achievement | +849 | Missing achievement data |
| lfg_dungeons | +213 | Missing dungeon finder entries |
| trait_definition | +299 | Missing talent/trait data |
| character_loadout | +157 | Missing starting loadouts |
| **+ ~30K hotfix_data entries** | | Registry entries so the client receives corrections |

### 1.2 SmartAI Import (NPC Behavior Scripts)

Two rounds of SmartAI extraction from LW's 897MB world dump:

- **Round 1**: 22,370 rows — quest scripts (17,367), creature AI (4,965), scene triggers, timed events
- **Round 2**: 166,443 rows — 165,360 creature behaviors, 169 gameobject scripts, 702 action lists, 212 scene triggers
- **Skipped**: 525K quest boilerplate rows (all identical "cast spell 82238" phase-update scripts — not useful for RP)
- **Attempted**: ~524,000 SmartAI INSERT operations across all import rounds (including Phase 4 in Section 1.3)
- **Post-validation**: Cleanup scripts removed entries referencing non-existent spells, creatures without SmartAI AIName, deprecated event types, broken link chains, and invalid waypoints (see [Section 6.3](#63-post-import-cleanup-47478-rows))
- **Final result**: 294,425 validated scripts (up from ~268K baseline — a net gain of ~26K valid scripts, plus validation of the entire existing dataset)

### 1.3 World DB Bulk Import (March 3, 2026)

5-phase dependency-ordered import of 21 tables from LoreWalkerTDB (builds 65893/65727/65299/63906, all 12.0.x):

| Phase | Tables | Key Data |
|-------|--------|----------|
| **1. Spawns** | creature, gameobject | +29,196 creatures, +19,604 gameobjects |
| **2. Loot** | creature_loot, gameobject_loot | +184,084 creature loot, +58,066 GO loot |
| **3. Dependents** | waypoints, difficulty, addons, pools, spawn_groups, text, spells, models, formations | +30K waypoint nodes, +2K creature addons, +1.8K pool templates |
| **4. SmartAI** | smart_scripts | +336,186 NPC AI scripts (INSERT IGNORE — many later removed by validation) |
| **5. Gossip/Vendor** | gossip_menu, gossip_menu_option, npc_vendor, creature_template_addon | +58 gossip menus, +44 options, +4 vendors |
| **Total** | **21 tables** | **+665,658 net new rows** |

**Column mismatch handling**: LW uses an older TC fork, so 3 tables had different column counts. `fix_column_mismatch.py` parses SQL tuple boundaries and appends safe defaults:
- `creature`: 28 → 29 columns (appended `size=-1`)
- `gameobject`: 24 → 26 columns (appended `size=-1, visibility=256`)
- `npc_vendor`: 11 → 12 columns (appended `OverrideGoldCost=-1`)

All 15 post-import integrity checks passed with zero orphans.

### 1.4 Initial LW World Data Import (February 27, 2026)

The first LW import (predating the bulk import above) added 385,823 rows across 17 tables. The SmartAI rows (167,685) are included in the Section 1.2 totals — same data, not additional:

| Table | Rows Added |
|-------|-----------|
| smart_scripts (2 passes) | 167,685 |
| creature_loot_template | 151,509 |
| gameobject_loot_template | 59,893 |
| pickpocketing_loot_template | 1,389 |
| reference_loot_template | 662 |
| skinning_loot_template | 402 |
| quest_offer_reward | 541 |
| quest_request_items | 370 |
| pool_template | 1,176 |
| pool_members | 1,164 |
| game_event_creature | 260 |
| game_event_gameobject | 164 |
| npc_vendor | 248 |
| conversation_actors | 194 |
| areatrigger_template | 142 |
| conversation_line_template | 19 |
| conversation_template | 5 |

</details>

<details>
<summary><strong>Part 2: Hotfix Repair System</strong> &mdash; <em>103K inserts + 1.8K column fixes across 388 tables</em></summary>

### 2.1 The Problem

TrinityCore's hotfix database diverges from Blizzard's live data over time. Columns get zeroed out during schema migrations, new rows from client patches are missing, and the `hotfix_data` registry (which tells the client what corrections to apply) becomes incomplete.

### 2.2 The Solution

`repair_hotfix_tables.py` compares every hotfix DB table against authoritative Wago DB2 CSV exports and generates repair SQL:

1. Downloads 1,097 DB2 CSV tables from Wago.tools for the current build
2. Normalizes column names (28 global aliases + 23 table-specific aliases + 6 table name overrides)
3. Compares every row: identifies zeroed columns, missing rows, and custom diffs to preserve
4. Generates UPDATE statements for zeroed columns, INSERT statements for missing rows
5. Generates corresponding `hotfix_data` entries so the client receives the corrections
6. Runs in 5 batches (~80 tables each) to manage memory

### 2.3 Results (Build 66220 — March 3, 2026)

| Metric | Value |
|--------|-------|
| Tables compared | 388 |
| Rows matching | 9,790,318 |
| Zeroed columns fixed | 1,831 UPDATEs |
| Custom diffs preserved | 468,972 rows (intentional overrides) |
| Missing rows inserted | 103,153 INSERTs |
| hotfix_data entries generated | 843,894 |
| Total SQL generated | ~71 MB across 5 batch files |
| **Total hotfix_data rows (pre-trim)** | **1,084,369 across 204 tables** |

> The 1,084,369 total included entries from the repair tool, LW imports, and prior TC data. The subsequent redundancy audit ([Part 13](#part-13-hotfix-redundancy-audit-complete)) reduced hotfix content table rows to ~244K by deleting entries that matched the client's DBC baseline. The hotfix_data registry itself was reduced from 1,084,369 to 835,385 entries.

**Key table populations after repair (pre-audit — see [Part 11](#part-11-final-database-state) for current):**

| Table | Rows (pre-audit) |
|-------|------|
| spell_name | 400,000 |
| spell_effect | 513,000 |
| item_sparse | 172,000 |
| creature_display_info | 118,000 |
| content_tuning | 9,800 |
| area_table | 9,800 |

### 2.4 Scene Script Repair

`repair_scene_scripts.py` handles `scene_script_text` — Lua scripts stored as hex-encoded blobs. Fixed 36 encoding errors and inserted 224 new scene scripts.

### 2.5 What This Fixed In-Game

- Items display correct names, stats, icons, and tooltips
- Spells have correct effects, visuals, and descriptions
- NPCs show proper display models
- Achievements, currencies, and dungeon data are complete
- All corrections delivered to the client via the hotfix system on login — no client modifications needed

</details>

<details>
<summary><strong>Part 3: NPC Audits & Corrections</strong> &mdash; <em>78,475 NPC corrections from 27-check audit + Wowhead cross-ref</em></summary>

### 3.1 Automated NPC Audit Tool (27 checks)

`npc_audit.py` validates every NPC against Wago DB2 data and Wowhead scraped data:

| Category | What it checks |
|----------|---------------|
| Levels | ContentTuningID vs expected level ranges |
| Flags | Vendor/trainer/gossip flags match actual data |
| Faction | Hostile/friendly/neutral alignment |
| Classification | Normal/Elite/Rare/Boss accuracy |
| Type | Humanoid/Beast/Undead/etc |
| Duplicates | Phase-aware stacked spawn detection |
| Names | Name accuracy vs Wago/Wowhead |
| Scale | Invisible (0) or oversized (>10) creatures |
| Speed | Absurd walk/run speeds |
| Equipment | Missing weapon/armor visuals |
| Gossip | Menu existence vs flag |
| Waypoints | Path linkage integrity |
| SmartAI | Script existence vs AI assignment |
| Loot | Killable NPCs with no loot tables |
| Auras | Invalid spell references |
| Spawn times | Abnormal respawn timers |
| Movement | Wander distance vs movement type consistency |
| + 10 more | Addon orphans, quest orphans, spells, scripts, map/zone validity, etc. |

### 3.2 Three-Batch Fix Results (23,904 operations)

**Batch 1 — Core Data Corrections:**

| Fix | Count | Details |
|-----|-------|---------|
| Duplicate spawns removed | 4,867 | Phase-aware detection preserved 4,409 intentional variants |
| Faction corrections | 4,045 | 11 categories from Wago DB2 — hostile mobs made passive, alliance/horde alignment |
| SmartAI orphan cleanup | 5,550 | AIName='SmartAI' with no actual scripts — cleared |
| Waypoint orphan fixes | 1,879 | Broken pathing switched to random movement |
| Gossip flag fixes | 1,541 | NPCs with gossip menus but missing GOSSIP flag |
| Classification fixes | 1,225 | Elite/Rare/Boss from Wago DB2 |
| Creature type fixes | 574 | Humanoid/Beast/Undead corrections |
| Trainer flag fixes | 142 | Missing TRAINER flag on trainers |
| Title fixes | 82 | Missing/wrong NPC subtitles |
| Family fixes | 67 | Creature family mismatches |
| Vendor flag fixes | 16 | Missing VENDOR flag on vendors |
| Unit class fixes | 7 | Invalid unit_class=0 → 1 |
| Invalid aura fixes | 222 | Removed references to deleted spells |

**Batch 2 — QA Pass:**

| Fix | Count | Details |
|-----|-------|---------|
| Placeholder NPCs despawned | 1,838 spawns | 399 [DNT]/[DND]/[PH] entries — invisible test NPCs |
| Vendor flag cleanup (spawned) | 631 | Had VENDOR flag but no items to sell |
| Vendor flag cleanup (unspawned) | 511 | Same, on template level |
| Name corrections | 23 | Broken spaces from CSV import, typos, dev artifacts |
| Service NPC movement fixes | 114 | Wandering vendors/trainers set stationary |
| Gossip orphan fixes | 9 | GOSSIP flag with no menu |

**Batch 3 — Comprehensive QA:**

| Fix | Count | Details |
|-----|-------|---------|
| Addon orphan cleanup | 865 | Dead creature_addon rows with no matching spawn |
| Vendor spawn time normalization | 522 | 2-hour to 700-hour respawns → 5 minutes |
| Zero wander distance fixes | 313 | Random-movement NPCs stuck in place |
| Service NPC movement | 119 | More wandering vendors/trainers set stationary |
| Name corrections | 13 | Blizzard renames, Exile's Reach updates |
| Walk speed fixes | 8 | NPCs moving at 12-20x normal speed |
| Rare spawn timer fixes | 6 | 0-second respawns → 5 minutes |
| Scale fix | 1 | Invisible creature (scale 0 → 1) |
| Title placeholder | 1 | "T1" placeholder removed |

### 3.3 Wowhead Mega-Audit (54,571 operations)

Scraped **216,284 NPCs** from Wowhead's API and cross-referenced every one against the database in three tiers:

**Tier 1 — Wowhead Cross-Reference (19,024 fixes):**

| Fix | Count |
|-----|-------|
| Type & classification remapping | 6,781 |
| Level fixes (ContentTuningID corrections) | 6,548 across 3 priority tiers |
| NPC flag additions (vendor/trainer/FM/etc) | 2,265 |
| Safe type fixes (Giant, Aberration reclassification) | 2,292 |
| Subtitle/subname corrections | 516 (+243 false-positive reverts) |
| Name corrections | 379 |

**Tier 2 — Deep Validation (3,282 fixes):**

| Fix | Count |
|-----|-------|
| ContentTuningID corrections (wrong expansion tier) | 3,013 |
| Zone hierarchy fixes | 5 |
| Incorrect service flag removals | 21 |

**Tier 3 — DB2 + Internal Consistency (32,265 fixes):**

| Fix | Count |
|-----|-------|
| Orphaned waypoint paths + nodes | 31,924 |
| Invalid per-spawn model resets | 232 |
| Orphaned SmartAI scripts | 106 |
| Hostile-faction vendor fixes | 3 |

</details>

<details>
<summary><strong>Part 4: Quest System Enrichment</strong> &mdash; <em>21K quest chains, 135K POI entries, 60K objectives</em></summary>

### 4.1 Quest Chains

Using Wago's `QuestLineXQuest` DB2 CSV data, generated `PrevQuestID` and `NextQuestID` links:

| Metric | Value |
|--------|-------|
| Total quest_template_addon rows | 47,164 |
| Quests with PrevQuestID | 21,758 (46.1%) |
| Quests with NextQuestID | 17,636 (37.4%) |
| Chain starters identified | 1,862 |
| Quest lines processed | 1,605 |

Zero self-references, zero circular chains, zero dangling references. Cycle detection (DFS-based) and dangling reference cleanup built into the pipeline.

### 4.2 Quest Points of Interest (POI)

From Wago `QuestPOIBlob` and `QuestPOIPoint` CSVs:

| Table | Already in DB | Added | Final Total |
|-------|-------------|----------|-------------|
| quest_poi | 131,976 | 2,880 | 134,856 |
| quest_poi_points | 287,778 | 5,199 | 292,977 |

### 4.3 Quest Objectives

From Wago `QuestObjective` CSV: 633 new objectives across 227 quests added to the existing 59,566. Final total: 60,199.

### 4.4 Quest Starters & Enders

Reimported from LoreWalkerTDB to ensure completeness:

| Table | Rows |
|-------|------|
| creature_queststarter | 26,842 |
| creature_questender | 33,496 |
| gameobject_queststarter | 1,615 |
| gameobject_questender | 1,610 |

### 4.5 Hero's Call / Warchief's Command Board Dedup

LW import artifact: old-framework quest boards (entries 206294/206116) were stacked on top of modern boards at identical coordinates. 25 duplicate board spawns removed, quest associations migrated from old entries to the 4 modern entries that had zero quests.

</details>

<details>
<summary><strong>Part 5: Localization</strong> &mdash; <em>1.6M item locale rows across 10 languages</em></summary>

### 5.1 Item Locales (from Raidbots)

Using Raidbots' `item-names.json` (171K items, 7 locales including en_US), imported 6 non-English locales plus 4 stub locales from TC's base data:

| Table | Total Rows |
|-------|-----------|
| item_sparse_locale | 1,020,264 |
| item_search_name_locale | 608,480 |

**Full coverage** (~170K items each): German, Spanish, French, Italian, Portuguese (Brazil), Russian

**Stub coverage** (29-59 items each, from TC base data): Mexican Spanish, Korean, Chinese (Simplified/Traditional)

Players using non-English clients now see item names in their language.

</details>

<details>
<summary><strong>Part 6: Database Cleanup & Integrity</strong> &mdash; <em>412K dead rows removed, loot table PKs added, 47K post-import cleanup</em></summary>

### 6.1 Initial 5-Database Audit (Feb 27)

148-check audit across all five databases found 412K rows of dead data:

| Category | Rows Removed |
|----------|-------------|
| Orphaned loot templates | 388,000 (63% of GO loot was dead references) |
| Duplicate spawns | 17,500 |
| Broken pool chains | 2,600 |
| Duplicate hotfix_data | 1,800 |
| SmartAI/script orphans | 928 |
| Event orphans | 413 |
| **Total** | **~412,000** |

### 6.2 Loot Table Primary Key Discovery & Deduplication

**The discovery**: `creature_loot_template` and `gameobject_loot_template` ship with **no primary key** — only a non-unique index `KEY idx_primary (Entry,ItemType,Item)`. `INSERT IGNORE` silently does nothing, and any bulk import creates exact row duplicates.

During the LW import, creature_loot ballooned from ~3.1M to 6.2M rows because every INSERT doubled the existing data. Detected, deduped via CSV round-trip (`sort -u`), and recovered:

| Table | Bloated | After Recovery | Dupes Removed |
|-------|---------|---------------|-----|
| creature_loot_template | 6,207,851 | 3,276,944 | 2,930,907 |
| gameobject_loot_template | 189,843 | 124,019 | 65,824 |

Separately, found 193,542 **pre-existing** duplicate rows across 4 loot tables that were already in the DB before any import. Removed via CREATE-SELECT-SWAP.

**Prevention:** Added proper PRIMARY KEYs to all 7 loot tables.

### 6.3 Post-Import Cleanup (47,478 rows)

After the LW bulk import, the worldserver logged ~627K error lines. Systematically cleaned the import-introduced issues:

| Category | Rows Cleaned |
|----------|-------------|
| Duplicate creature spawns (< 1 yard) | 19,385 |
| Duplicate GO spawns (< 1 yard) | 18,485 |
| SmartAI errors (bad spells, unsupported types, missing refs) | 2,808 |
| Empty pool_templates | 1,806 |
| NPC vendor issues (bad items, missing flags) | 305 |
| Empty waypoint_paths | 47 |
| Orphaned dependents from spawn deletion | 4,642 |

> **Note on SmartAI validation**: Beyond the 2,808 rows in the table above, additional cleanup scripts (2026_02_25_30 through 2026_02_26_32) removed ~498K invalid SmartAI entries — scripts referencing creatures without SmartAI AIName, non-existent spells/waypoints/quests, deprecated event types, and broken link chains. This is why the final smart_scripts count (294,425) is much lower than the peak during import. The validation scripts mirror the server's own `SmartScriptMgr.cpp` checks, ensuring every remaining script is loadable without errors.

### 6.4 Other Cleanup

- **Backup tables**: 101 backup/temp tables dropped across all databases, reclaiming ~382 MB
- **MyISAM → InnoDB**: All 7 remaining MyISAM tables converted (2 required `ROW_FORMAT=DYNAMIC`) — enables row-level locking, crash recovery, buffer pool caching
- **Indexes**: 4 redundant/duplicate indexes dropped

</details>

<details>
<summary><strong>Part 7: MySQL & Server Performance</strong> &mdash; <em>Startup 3m24s → 17s, MySQL tuning, crash fix, memory leaks</em></summary>

### 7.1 Server Startup: 3m24s -> 60s -> 17s

**Phase 1: MySQL & Config Tuning (3m24s → ~60s in Debug build)**

| Optimization | Impact |
|-------------|--------|
| `tmp_table_size` was 1,024 BYTES (not MB!) | All temp tables spilled to disk — fixed to 256M |
| Disabled unused features (locales, hotswap, AH bot) | Reduced initialization overhead |
| Thread tuning (World/Hotfix DB threads 4 → 8) | Parallel loading of large tables |
| Buffer pool warm restarts | No cold cache on restart |

**Phase 2: Build Configuration (60s → 17s)**

| Optimization | Impact |
|-------------|--------|
| Switched to RelWithDebInfo (`/O2 /Ob1` vs Debug `/Od`) | Compiler optimization — biggest single improvement |

### 7.2 MySQL Configuration

| Setting | Before | After | Why |
|---------|--------|-------|-----|
| innodb_buffer_pool_size | default | 16 GB | 128 GB RAM system, keeps all data cached |
| buffer_pool_instances | 1 | 8 | Parallel access to buffer pool |
| buffer_pool_dump/load | OFF | ON | Warm restarts |
| key_buffer_size | default | 8 MB | No MyISAM tables remain |
| skip-name-resolve | OFF | ON | Faster connections |
| slow_query_log | OFF | ON (2s) | Performance monitoring |
| tmp_table_size | 1,024 bytes | 256 MB | Critical fix |

### 7.3 worldserver.conf Optimization

| Setting | Change | Impact |
|---------|--------|--------|
| Eluna.CompatibilityMode | true → false | Was forcing single-threaded map updates, nullifying MapUpdate.Threads=4 |
| MapUpdate.Threads | (now effective) | 4 parallel map processing threads |
| MaxCoreStuckTime | 0 → 600 | Freeze watchdog re-enabled |
| SocketTimeOutTimeActive | 900s → 300s | Dead connections cleaned faster |
| WorldDatabase.WorkerThreads | 4 → 8 | Faster DB operations |
| HotfixDatabase.WorkerThreads | 4 → 8 | Faster hotfix loading |
| ThreadPool | 4 → 8 | More worker threads |

### 7.4 Hotfix Pipeline Crash Fix (Critical)

With 1.08M hotfix_data rows (966K unique push IDs), the server crashed on client connect — the monolithic `SMSG_HOTFIX_CONNECT` packet exceeded the ByteBuffer 100MB assertion.

**6 bugs fixed across 3 C++ files:**

| Bug | Severity | Fix |
|-----|----------|-----|
| No chunking of HotfixConnect response | CRITICAL | Chunked at 50MB via `unique_ptr<HotfixConnect>` rotation |
| 100MB ByteBuffer assert fires before compression | CRITICAL | Assert raised to 500MB |
| Memory doubling (HotfixContent + _worldPacket copies) | HIGH | Intermediate buffers released after serialization |
| Fixed 400KB growth steps (excessive reallocation) | MEDIUM | Exponential growth (doubles capacity, capped 32MB step) |
| No cap on hotfix request count | MEDIUM | 1M request cap with warning log |

Relevant to any TC server running large hotfix datasets. Without it, 1M+ hotfix_data rows crash the server.

> **Post-audit update**: The redundancy audit ([Part 13](#part-13-hotfix-redundancy-audit-complete)) reduced hotfix content tables from ~10.8M to ~244K rows and hotfix_data from 1.08M to ~835K entries. The chunked delivery system remains as a safety net, but the payload is now well within original limits.

### 7.5 Memory Leak Fixes

Fixed memory leaks in the Visual Effects system (`EffectsHandler`):
- `RemoveEffect` now properly deletes `EffectData*` before removal
- `Reset` deletes data before clearing the container
- `GetUnitInfo` deletes on invalid unit lookup
- Dead `Clear()` method removed

### 7.6 Build System

Build parallelism increased from `-j4` to `-j20` across all build configurations, leveraging the full 24-thread CPU.

</details>

<details>
<summary><strong>Part 8: Build Diff Audit (5 Builds)</strong> &mdash; <em>5 builds diffed, zero breaking changes, oscillation detection</em></summary>

### 8.1 Scope

Diffed all Wago DB2 CSV data across 5 consecutive WoW 12.0.1 builds:
**66044 → 66102 → 66192 → 66198 → 66220**

39 priority tables compared. Cross-referenced against live MySQL databases.

### 8.2 Key Discovery: Wago Export Oscillation

Wago.tools CSV exports oscillate wildly between builds for certain tables:
- SpellEffect: 269K-608K rows
- ItemSparse: 125K-171K

This is an **export artifact**, not actual content changes. Oscillation detection built into the diff tooling.

### 8.3 Actual Content Changes (66044 -> 66220)

| Category | Changes |
|----------|---------|
| Spells | +77 new, -1 removed, ~288 attribute mods |
| Items | +17 new (mounts, titles, toys), ~308 modifications |
| Quests | +9 new |
| Achievements | +5 new (Slayer's Rise PvP) |
| Creatures | +1 new display, 5 tweaks |
| Currencies | +1 new, Honor cap 15K → 4K |

### 8.4 Scripted Spell Safety

40 spells with C++ script bindings got new SpellEffect entries. **All safe** — new effects appended at higher indices (3, 4, 5+), never replacing existing 0/1/2. Verified in source: `SpellInfo.cpp:1298` uses explicit index-based assignment.

Zero breaking changes across all 5 builds.

</details>

<details>
<summary><strong>Part 9: Placement Audits</strong> &mdash; <em>31K placement fixes generated (pending review)</em></summary>

### 9.1 Creature Placement (vs LoreWalkerTDB)

Compared 680K LW creature spawns against our 652K:

| Finding | Count |
|---------|-------|
| Missing creature spawns | 21,771 |
| Misplaced creatures | 38 |
| Property mismatches | 3,178 |
| SQL fixes generated | 24,681 |

### 9.2 GameObject Placement (vs LoreWalkerTDB)

Compared 194K LW gameobject spawns against our 174K:

| Finding | Count |
|---------|-------|
| Missing GO spawns | 5,837 |
| Misplaced GOs | 9 |
| Property mismatches | 1,625 |
| SQL fixes generated | 6,767 |

### 9.3 Status

Placement SQL fixes have been **generated but not yet applied** — requires manual review. The rotation "mismatches" (135 total) were all LW using `(0,0,0,0)` quaternion vs our correct `(0,0,0,1)` identity quaternion. Our data is correct — LW's zero quaternion is mathematically invalid.

</details>

<details>
<summary><strong>Part 10: Custom Tooling Summary</strong> &mdash; <em>50+ tools — top 10 at a glance</em></summary>

Over 50 Python scripts, MCP servers, audit tools, and SQL generators were built for this project. See **[Part 16](#part-16-complete-tooling--infrastructure-catalog)** for the complete catalog.

**Top 10 tools at a glance:**

| Tool | Purpose |
|------|---------|
| `repair_hotfix_tables.py` | 5-batch hotfix DB repair against Wago DB2 baselines |
| `hotfix_differ_r3.py` | Type-aware redundancy audit (float32, int32 sign, logical PK) |
| `npc_audit.py` | 27-check NPC validator against Wago DB2 + Wowhead |
| `import_all.py` | 5-phase dependency-ordered LoreWalkerTDB import |
| `run_all_imports.py` | 8-step Raidbots/Wago orchestrator with `--dry-run` |
| `wago_db2_server.py` | MCP server: DuckDB queries across 1,097 DB2 CSVs |
| `code_intel_server.py` | MCP server: hybrid ctags + clangd C++ intelligence (416K symbols) |
| `diff_builds.py` | Row-by-row CSV differ with oscillation detection |
| `wowhead_scraper.py` | 216K NPC data scraper for cross-reference audit |
| `db_snapshot.py` | MySQL backup/rollback with snapshot, check, prune, rollback |

</details>

<details>
<summary><strong>Part 11: Final Database State</strong> &mdash; <em>Row counts and database sizes as of April 4, 2026 (session 228)</em></summary>

### 11.1 Table Counts (April 4, 2026)

**World database (LoreWalker + TC TDB 1200.26021 merged, build 66709):**

| Table | Rows | Notes |
|-------|------|-------|
| creature_template | 225,842 | NPC definitions |
| creature | 712,400 | NPC spawn instances |
| creature_template_difficulty | 595,899 | Per-difficulty NPC stats (428 orphan loot refs cleared) |
| creature_loot_template | 3,078,121 | NPC loot tables (TC TDB backfill) |
| smart_scripts | 635,329 | NPC AI behavior scripts (+170K from TC TDB) |
| gameobject | 204,499 | World object spawn instances (+45K from TC TDB) |
| npc_vendor | 170,411 | Vendor inventory entries |
| quest_template | 48,300 | Quest definitions (+1.6K from TC TDB) |
| creature_queststarter | 29,064 | NPC quest associations |
| creature_questender | 36,051 | NPC quest turn-in associations |
| spell_script_names | 3,743 | C++ spell script bindings |

**Hotfixes database (build 66709):**

Hotfix tables contain TC TDB + LoreWalker + custom overrides. 433 tables backfilled from TC TDB 1200.26021.

> **Note**: Build bumped from 66337 to 66709 (session 200+217). LoreWalker migration applied (3.3M rows, session 225). TC TDB 1200.26021 backfilled via INSERT IGNORE (session 227). DB error cleanup removed 886K orphan rows across phases 1-2a (session 227). Orphan LootID fix cleared 428 difficulty rows (session 228).

### 11.2 Database Sizes

| Database | Size |
|----------|------|
| world | 1,370 MB |
| hotfixes | 1,291 MB |
| characters | 3 MB |
| auth | 1 MB |
| roleplay | < 1 MB |

</details>

<details>
<summary><strong>Part 12: What It All Means for Players</strong> &mdash; <em>Before/After comparison + LW collaboration notes</em></summary>

### Before
- 6,548 NPCs stuck at level 1 — endgame elites one-shottable by any player
- Tens of thousands of NPCs standing motionless with no AI scripts
- 4,867+ duplicate NPCs stacked on top of each other
- Quest chains broken — no breadcrumbs, no progression tracking
- Quest objectives showing blank or missing, no map markers
- Items showing English-only names for all non-English clients
- Loot tables with 193K duplicate entries inflating drop chances (and no primary keys to prevent more)
- 1,142 vendors with VENDOR flag but zero items to sell
- Vendors with 700-hour respawn timers, NPCs moving at 12-20x speed, an invisible creature (scale 0)
- Server startup: 3 minutes 24 seconds
- 627,000+ error lines on every startup
- Server crashing on client connect (oversized hotfix packet)

### After
- All NPCs at correct levels with proper ContentTuning scaling
- 294K validated SmartAI scripts — NPCs patrol, react, run events (26K net new scripts added, entire dataset validated)
- Clean spawns — no duplicates, no stacked/invisible NPCs
- 21,758 quest chain links, 135K POI entries, 60K quest objectives
- 1.6M+ item locale entries across 10 languages
- Correct drop rates with enforced primary keys on all loot tables
- 78,475 NPC corrections (levels, factions, flags, names, pathing)
- Service NPCs actually function (vendors sell, trainers train, gossip menus work)
- 17-second server startup (92% reduction)
- Hotfix content tables reduced from 10.8M rows to ~244K (97.8% reduction)
- Clean server logs — real errors visible instead of buried in noise

### For CaptainCore / LoreWalkerTDB

**What LW data gave us:**
- ~1M net new rows of world data
- ~524K SmartAI scripts imported (294K survived validation — the rest had broken references)
- ~242K net loot table entries
- Hotfix entries (spell enchantments, sound kits, display info, phases)
- Quest starters/enders, conversation data, spawn pools, game events

**How we built on LW data:**
1. **Import pipeline** — column mismatch handling (3 tables), dependency ordering (5 phases), 15-point validation
2. **Post-import cleanup** — 47K orphans/duplicates cleaned, 627K error lines resolved, ~498K invalid SmartAI entries removed by validation
3. **Gap filling** — 1.6M Raidbots item locales, 21K Wago quest chains, 135K quest POI, 216K Wowhead NPC cross-reference
4. **Hotfix repair + trim** — 843K hotfix_data entries generated, content tables trimmed from ~10.8M to ~244K genuine rows
5. **78K NPC corrections** — validated against Wago DB2 + Wowhead

**LW data quality findings (potential upstream fixes):**
- Column count mismatches on `creature`/`gameobject`/`npc_vendor` vs current TC schema
- Gameobject rotation quaternions stored as `(0,0,0,0)` instead of identity `(0,0,0,1)`
- Quest board entries (206294/206116) stacking on modern board coordinates
- SmartAI scripts referencing non-existent spells (1,095), unsupported types (813), missing waypoints (803)
- Duplicate spawns within 1 yard (37,870 pairs)

**Collaboration opportunities:**
- `fix_column_mismatch.py` and `validate_import.py` could help anyone consuming LW data
- The hotfix repair system is build-agnostic — works for any TC hotfix database
- The hotfix redundancy audit tools are in the RoleplayCore repo (`hotfix_audit/`)
- If LW added composite PKs to loot tables, it would prevent the INSERT IGNORE duplication trap for all consumers

</details>

<details>
<summary><strong>Part 13: Hotfix Redundancy Audit (Complete)</strong> &mdash; <em>10.8M → 244K content rows (97.8% redundant)</em></summary>

### 13.1 The Problem

After the hotfix repair ([Part 2](#part-2-hotfix-repair-system)), the hotfix database carried **~10.8M content rows** across 517 tables. The hotfix system sends only **corrections** to client data — rows that override the client's built-in DBC/DB2 files. But 97.8% of those rows were **identical** to what the client already had. This:

- Increased login time (every hotfix entry sent via SMSG_HOTFIX_CONNECT)
- Required chunked packet delivery ([Part 7.4](#74-hotfix-pipeline-crash-fix-critical)) just to avoid crashing
- Wasted server memory caching duplicate data

### 13.2 Approach: 3-Round Audit

A custom diff pipeline (`hotfix_audit/` in the repo) compares every hotfix row against DBC baselines extracted from the WoW 12.0.1.66263 client via **wow.tools.local (WTL)**. WTL extracts complete DB2 files from the client's CASC archive — more complete than Wago CSV exports, which can be partial.

Each row is classified:

| Category | Meaning | Action |
|----------|---------|--------|
| **Redundant** | Identical to DBC baseline | DELETE |
| **Override** | Differs from baseline | KEEP (genuine correction) |
| **New** | Not in DBC at all | KEEP (custom/community content) |
| **Negative Build** | VerifiedBuild < 0 | KEEP (TC deletion marker) |

### 13.3 Round 1 — Discovery

String-level comparison against DBC2CSV exports:

| Metric | Value |
|--------|-------|
| Tables audited | 388 |
| Total rows examined | ~10.8M |
| Redundant (string match) | ~9.6M (88.9%) |
| Override | ~468K |
| New (not in DBC) | ~232K |

### 13.4 Round 2 — Refined Diff

| Improvement | Details |
|-------------|---------|
| Better CSV baseline | WTL DBC2CSV instead of Wago partial exports |
| Column mapping fixes | Corrected array index off-by-one errors |
| Batch DELETE SQL | TRUNCATE for fully-redundant tables, IN-clause batches for partial |
| Orphan cleanup | 175K orphaned hotfix_data entries (referencing deleted tables) |

Removed ~204K additional redundant rows + 175K orphaned entries.

### 13.5 Round 3 — Type-Aware Cleanup

Introduced type-aware comparison to catch false negatives from string diffing:

| Fix | Details |
|-----|---------|
| **Float32 precision** | IEEE 754 bit-level comparison via `struct.pack('f')` — MySQL FLOAT truncates to ~6 sig digits during serialization |
| **Signed/unsigned int32** | Same 32-bit pattern comparison (e.g., -1 == 4294967295 as uint32) |
| **Logical primary keys** | `broadcast_text_duration` uses `(BroadcastTextID, Locale)` not `ID` |
| **Array index mapping** | DB `Foo1` → CSV `Foo[0]` (1-indexed to 0-indexed) |

Found 767,672 additional redundant rows across 109 tables.

### 13.6 Final Results

| Metric | Before | After |
|--------|--------|-------|
| **Hotfix content rows** | ~10.8M | ~244,000 |
| **Content reduction** | — | **97.8%** |
| hotfix_data entries | 1,084,369 | 835,385 |
| Hotfix DB on disk | 1,309 MB | 637 MB |

> **Note on hotfix_data**: The content table cleanup (deleting redundant rows from tables like spell_name, spell_effect, etc.) was fully applied across all 3 rounds. The hotfix_data registry was partially cleaned (orphan removal in R2), reducing it from 1.08M to ~835K. The remaining hotfix_data entries include both references to the ~244K genuine content rows and stale entries referencing deleted rows — the server skips these harmlessly at load time.

**Remaining ~244K content rows:**

| Category | Rows | Details |
|----------|------|---------|
| **Overrides** | ~8,400 | Genuine corrections (chr_customization_choice, spell_item_enchantment, item_sparse, content_tuning, creature_display_info) |
| **New content** | ~231,000 | Not in client DBC — broadcast_text (~224K from TC community data), phase (~5,700), custom items (~400) |

### 13.7 Safety

Three `db_snapshot.py` snapshots taken at each phase boundary. Each round's SQL applied only after verifying the previous round.

### 13.8 What This Fixed In-Game

- **Faster login**: SMSG_HOTFIX_CONNECT sends far less data — ~244K content rows instead of 10.8M
- **Eliminated crash risk**: Payload now well within ByteBuffer limits without chunking
- **Every remaining content row has a reason to exist** — either a genuine correction or custom content

### 13.9 Reproducibility

Tools in `hotfix_audit/` with full README. 3-step process: `build_table_info_r3.py` → `hotfix_differ_r3.py` → `gen_practical_sql_r3.py`.

</details>

<details>
<summary><strong>Part 14: Discoveries & Lessons (Useful for the Community)</strong> &mdash; <em>9 discoveries applicable to any TrinityCore 12.x project</em></summary>

These findings apply to any TrinityCore 12.x project:

### 14.1 Loot Table Primary Key Trap
`creature_loot_template` and `gameobject_loot_template` ship with **no primary key** — only `KEY idx_primary (Entry,ItemType,Item)`. `INSERT IGNORE` silently does nothing, every bulk import creates exact duplicates. We lost ~3M rows to this before discovering it. **Fix**: Add composite PKs after deduplication.

### 14.2 Wago DB2 CSV Export Oscillation
Wago CSV exports oscillate between builds. SpellEffect swings 269K-608K rows, ItemSparse 125K-171K. This is a partial-vs-full **export artifact**. Any diffing tool must detect this or it produces false positives. Quick check: `wc -l SpellEffect-enUS.csv` — >500K = full, <400K = reduced.

### 14.3 MySQL `tmp_table_size` Default Trap
`tmp_table_size` can default to 1,024 **bytes** depending on install. ALL temporary tables spill to disk, destroying GROUP BY/ORDER BY/JOIN performance. Symptom: inexplicably slow server startup. Fix: set to 256M+ explicitly.

### 14.4 Eluna CompatibilityMode Threading Kill
`Eluna.CompatibilityMode = true` (the default) forces **single-threaded map updates**, completely nullifying `MapUpdate.Threads`. A 4-thread config runs on 1 thread. Not documented anywhere obvious.

### 14.5 LoreWalkerTDB Column Mismatches
LW uses an older TC fork. At least 3 tables have fewer columns: `creature` (28 vs 29), `gameobject` (24 vs 26), `npc_vendor` (11 vs 12). Direct import fails. Need a column-count-aware parser to append defaults.

### 14.6 LoreWalkerTDB Rotation Data
LW stores `(0,0,0,0)` quaternion for gameobject rotations — mathematically invalid (identity is `(0,0,0,1)`). Don't overwrite valid rotations with LW data.

### 14.7 ByteBuffer Assert at Scale
TC's ByteBuffer asserts at 100MB. With 1M+ hotfix_data rows, SMSG_HOTFIX_CONNECT exceeds this on connect. Any server with a large hotfix dataset needs chunked delivery — or better yet, run a redundancy audit to trim the payload ([Part 13](#part-13-hotfix-redundancy-audit-complete)).

### 14.8 Stacked Quest Board Trap
LW import places old-framework quest boards (entries 206294/206116) at exact coordinates of modern boards. The old boards may be the ones actually serving quests (via `gameobject_queststarter`), while modern boards have zero associations. Deleting the "duplicate" breaks quest functionality. Always check quest associations first.

### 14.9 Hotfix Redundancy Is the Norm
97.8% of TC hotfix content rows were identical to the client's DBC baseline. Likely representative of any TC server that has run repair tools or imported LW hotfix data. The hotfix system is for corrections, not duplication. Any TC server can dramatically improve login speed by running a similar audit. Tools are in the RoleplayCore repo.

</details>

<details>
<summary><strong>Part 15: Timeline</strong> &mdash; <em>8 days, 39+ sessions, Feb 26 – Mar 5</em></summary>

| Date | Sessions | Key Milestones |
|------|----------|---------------|
| **Feb 26** | 1 | Companion AI fix, transmog wireDT fix, initial hotfix repair v1 |
| **Feb 27** | 2-7 | 5-DB audit (412K cleanup), LW import #1 (385K rows), NPC audit tool (27 checks), 3-batch NPC fixes (23,904 ops), placement audit tools |
| **Feb 28** | 8-10 | GO/quest audit tools + 2,279 DB fixes, TransmogBridge implementation, placement audits |
| **Mar 1** | 11-12 | Transmog confirmed working in-game, PR cleanup, cross-repo PR #760 |
| **Mar 3** | 13-30 | Wowhead mega-audit (54,571 ops), Raidbots/Wago pipeline (locales + quests), LW import #2 (665K rows), post-import cleanup (47K rows), hotfix repair build 66220, MySQL tuning, build diff audit (5 builds), hotfix pipeline crash fix, transmog multi-bug fixes |
| **Mar 4** | 31-38 | Hotfix redundancy audit rounds 1-3 (10.8M → 244K content rows), WTL DBC pipeline, world DB cleanup (NPC/portal fixes, SmartAI orphans), transmog client wiki, auth key update |
| **Mar 5** | 39+ | Report update, transmog diagnostics, remaining QA |

</details>

<details>
<summary><strong>Part 16: Complete Tooling & Infrastructure Catalog</strong> &mdash; <em>Full catalog: Python tools, MCP servers, agents, addons, SQL</em></summary>


<details>
<summary><strong>16.1 Python Data Pipeline Tools</strong> (14 tools)</summary>

| Tool | Location | Purpose |
|------|----------|---------|
| `repair_hotfix_tables.py` | `~/VoxCore/wago/` | 5-batch hotfix DB repair against Wago DB2 baselines |
| `repair_scene_scripts.py` | same | Scene script hex-encoded Lua repair |
| `wago_db2_downloader.py` | same | Download 1,097 DB2 CSVs from Wago.tools |
| `diff_builds.py` | same | Row-by-row CSV diffing with oscillation detection |
| `cross_ref_mysql.py` | same | Cross-reference diff results with live MySQL |
| `import_all.py` | same | 5-phase dependency-ordered LW import |
| `validate_import.py` | same | 15-check post-import integrity validator |
| `fix_column_mismatch.py` | same | Fix column count differences between TC forks |
| `run_all_imports.py` | `raidbots/` | Master 8-step Raidbots/Wago orchestrator with --dry-run and --regenerate |
| `db_snapshot.py` | `~/VoxCore/wago/` | MySQL backup/rollback (snapshot/check/list/rollback/prune) |
| `import_item_names.py` | `raidbots/` | Raidbots → 10-locale item name import |
| `quest_chain_gen.py` | `raidbots/` | Wago QuestLineXQuest → quest chain generation with DFS cycle detection |
| `gen_quest_poi_sql.py` | `raidbots/` | Wago → quest POI import |
| `quest_objectives_import.py` | `raidbots/` | Wago → quest objective import |
| `extract_lw_world.py` | `~/VoxCore/wago/` | Parse 897MB LW dump into per-table SQL |
| `validate_transmog.py` | same | Transmog data integrity check (155K appearances, 4.8K sets) |
| `transmog_lookup.py` | same | Transmog DB2 cross-reference (IMAID → item name, display) |
| `transmog_debug.py` | same | Transmog state debugger |

</details>


<details>
<summary><strong>16.2 Hotfix Audit Tools</strong> (6 tools)</summary>

| Tool | Location | Purpose |
|------|----------|---------|
| `hotfix_differ_r3.py` | `hotfix_audit/` (in repo) | Type-aware row differ — float32, int32 sign, logical PK |
| `gen_practical_sql_r3.py` | same | Cleanup SQL generator — TRUNCATE + batched DELETEs |
| `build_table_info_r3.py` | same | Column mapping builder — array index, coordinate, rename resolution |
| `merge_results.py` | same | Result aggregator and report generator |
| DBC2CSV | `~/VoxCore/ExtTools/DBC2CSV\` | Converts WTL DB2 binaries to CSV |
| wow.tools.local (WTL) | `~/VoxCore/ExtTools/WoW.tools\` | Local CASC browser — extracts DB2 baselines from game files |

</details>


<details>
<summary><strong>16.3 Audit Tools</strong> (6 tools)</summary>

| Tool | Checks | Scope |
|------|--------|-------|
| `npc_audit.py` | 27 | 662K creatures vs Wago DB2 + Wowhead |
| `go_audit.py` | 15 | 175K gameobjects vs Wago DB2 |
| `quest_audit.py` | 15 | 47K quests vs Wago DB2 |
| `creature_placement_audit.py` | 5 | Position comparison vs LW |
| `go_placement_audit.py` | 6 | Position comparison vs LW |
| `wowhead_scraper.py` | — | 216K NPC data scraper |

</details>


<details>
<summary><strong>16.4 MCP Servers</strong> (3 servers)</summary>

Model Context Protocol servers giving Claude Code direct access to project data:

| Server | Transport | Purpose |
|--------|-----------|---------|
| `wago_db2_server.py` | FastMCP/stdio | DuckDB-powered queries against 1,097 DB2 CSV tables |
| `code_intel_server.py` | FastMCP/stdio | Hybrid ctags+clangd C++ code intelligence (416K symbols) |
| MySQL MCP | .claude.json | Direct MySQL queries against all 5 databases |

</details>


<details>
<summary><strong>16.5 Claude Code Agents</strong> (5 agents)</summary>

Specialized agents defined in `.claude/agents/`:

| Agent | Expertise |
|-------|-----------|
| Packet Analyst | Hex dumps, opcode analysis, UpdateField wire format |
| DB Specialist | MySQL queries, DB2 cross-referencing, hotfix data |
| C++ Systems | Server handlers, opcode registration, transmog pipeline |
| Lua/Addon Dev | TransmogBridge/TransmogSpy, WoW Lua API |
| Python Tooling | Scripts, MCP servers, packet log parsers |

</details>


<details>
<summary><strong>16.6 WoW Addons</strong> (3 addons)</summary>

| Addon | Purpose |
|-------|---------|
| TransmogBridge | 3-layer hybrid merge transmog data capture, sends to server via addon message (workaround for broken 12.x CMSG) |
| TransmogSpy | Diagnostic logger — all transmog API calls to SavedVariables |
| SpawnDespawnTool | Category-based batch spawn/despawn for GMs |

</details>


<details>
<summary><strong>16.7 Packet Analysis & Server Tools</strong> (4 tools)</summary>

| Tool | Purpose |
|------|---------|
| `opcode_analyzer.py` | TC opcode parser, cross-ref with WPP captures |
| `start-worldserver.sh` | Session lifecycle with auto-archiving and WPP |
| `wpp-add-build.sh` / `wpp-inspect.sh` | WowPacketParser utilities |
| WowPacketParser (WPP) | `~/VoxCore/ExtTools/WowPacketParser\` — retail packet parser |

</details>


<details>
<summary><strong>16.8 SQL Fix Scripts</strong> (5 scripts)</summary>

| Script | Purpose |
|--------|---------|
| `fix_quest_chains.sql` | Dangling ref cleanup + N-hop cycle fix with recursive CTE |
| `fix_locale_and_orphans.sql` | Cross-DB cleanup: stale locales + orphaned objectives |
| `fix_orphan_quest_refs.sql` | Orphaned quest reference remediation |
| Genre 5c-8a scripts | World DB cleanup: invalid maps/phases/models, SmartAI, loot |
| Auth build consolidation | Idempotent SQL for multiple Midnight builds (65893-66220) |

</details>


<details>
<summary><strong>16.9 Infrastructure</strong> (4 components)</summary>

| Component | Details |
|-----------|---------|
| **Build** | VS2022 x64, Ninja, RelWithDebInfo, `-j20` |
| **MySQL** | UniServerZ 9.5.0 — 16GB buffer pool, 8 instances, warm restarts, 256M tmp_table_size |
| **Git** | Parallel worktrees; `db_snapshot.py` for DB rollback |
| **Hardware** | Ryzen 9 9950X3D (12C/24T), 128GB DDR5-5600, RTX 5090, 2TB NVMe |

</details>


<details>
<summary><strong>16.10 Data Sources & Pipelines</strong> (5 sources)</summary>

| Source | Pipeline | Output |
|--------|----------|--------|
| Wago.tools DB2 CSVs | `wago_db2_downloader.py` → DuckDB via MCP | DB2 data for repair/audit/diff |
| wow.tools.local | WTL → DBC2CSV → CSV | Complete DBC baselines from client CASC |
| Raidbots | `run_all_imports.py --regenerate` | Item names (171K x 7 locales), quest chains, POI |
| LoreWalkerTDB | `import_all.py` + `validate_import.py` | World spawns, loot, SmartAI, hotfixes |
| Wowhead | `wowhead_scraper.py` (216K NPCs) | NPC cross-reference data |

</details>


<details>
<summary><strong>16.11 Workflow Patterns</strong> (5 patterns)</summary>

- **Snapshot-gated phases**: `db_snapshot.py snapshot --tag <phase>` before every DB mutation; rollback on failure
- **Idempotent SQL**: INSERT IGNORE, ON DUPLICATE KEY UPDATE, DELETE-before-INSERT throughout
- **Diagnostic build cycle**: Add logging → build → test → analyze logs → iterate
- **Build diffing**: `diff_builds.py` separates real content changes from Wago export oscillation
- **Parallel execution**: Separate git worktrees, subagent delegation, background builds

---

</details>

</details>

<details>
<summary><strong>Appendix A: Data Sources</strong> &mdash; <em>6 data sources with volume estimates</em></summary>

| Source | Data Type | Volume |
|--------|----------|--------|
| **LoreWalkerTDB** | World DB, Hotfixes DB | 1.2 GB SQL dumps |
| **Wago.tools DB2 CSVs** | 1,097 client DB2 tables, 5 builds | ~5.5K CSV files |
| **wow.tools.local** | DB2 baselines from client CASC (build 66263) | ~1,097 DB2 tables |
| **Raidbots** | Item names (171K x 7 locales), talents | 168 MB JSON |
| **Wowhead** | 216K NPC tooltips, names, types, levels | 218K JSON files |
| **TrinityCore upstream** | Periodic merge + SQL updates | Git merge |

</details>

<details>
<summary><strong>Appendix B: Reproducibility</strong> &mdash; <em>6 fully reproducible pipelines</em></summary>

Every operation is fully reproducible:

1. **Hotfix repair**: `python repair_hotfix_tables.py --batch {1..5}` — idempotent
2. **LW import**: `python import_all.py` + `python validate_import.py` — 15 integrity checks
3. **Raidbots pipeline**: `python run_all_imports.py --regenerate` — 8-step with --dry-run
4. **NPC audit**: `python npc_audit.py all --report --json --sql-out` — 27 checks
5. **Build diff**: `python diff_builds.py --base 66220 --target 66263`
6. **Hotfix audit**: `build_table_info_r3.py` → `hotfix_differ_r3.py` → `gen_practical_sql_r3.py` (see `hotfix_audit/README.md`)

All scripts version-controlled in private GitHub repositories.

</details>

---

*Updated March 21, 2026 (session 199) | RoleplayCore — VoxCore84/RoleplayCore*

**Database State (March 21, 2026):** world 1,389 MB (712K creature spawns, 226K templates, 457K SmartAI scripts, 178K vendor entries), hotfixes 1,135 MB (400K spells, 236K broadcast_text, 176K items, 238K hotfix_data), characters 4 MB, auth 3 MB, roleplay < 1 MB. Total: ~2.5 GB across 5 databases. Hotfix repair completed for build 66337 (70 tables, 104 zeroed fixes, 4,291 custom diffs preserved). TrinityCore upstream merge to build 66527 in progress on branch `migration/tc-66527-merge-v3`.

**Since last DB report update (sessions 108-199):** LoreWalker TDB import v3 applied (session 118: 7 SQL files, ~502K inserts + 7.7K updates, creature delta +101,018). Hotfix repair re-run for build 66337 (session 143: 70 tables, 237,530 hotfix_data rows). TrinityCore upstream merge in progress (92 commits, build 66527). Multiple world SQL updates applied (creature spawns, SmartAI scripts, quest data, locale translations). Transmog QA fixes applied. DraconicBot v3.1 built. Auto-Parse v3+ deployed. CreatureCodex v3.0 released. VoxGM v1.0 released. VoxSniffer v1.0 released.
*Tools: VoxCore84/wago-tooling, VoxCore84/tc-packet-tools, VoxCore84/code-intel, VoxCore84/trinitycore-claude-skills*
