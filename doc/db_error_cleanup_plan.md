# DB Error Cleanup Plan — Master Document

**Created**: 2026-04-04 (Session 226)
**Goal**: Reduce 6.8M DBErrors from startup to near-zero

## Current State

Two world databases overlaid:
- **LoreWalkerTDB** (Apr 3 build) — Midnight/TWW content, full creature spawns, SmartAI
- **TC TDB 1200.26021** (Feb 6) — base TrinityCore data, backfilled via INSERT IGNORE

Both data sets have internal references the other doesn't resolve = 6.8M errors.

## The Smoking Gun: DB2 Extraction Gap

Server loads 171,480 items from DB2+hotfixes. Loot tables reference ~45K items the server can't find. But **83% of those items exist in Wago's DB2 for the same build (66709)**. This means our mapextractor extraction is incomplete — Wago gets data from full CASC that our extractor misses.

Same likely applies to factions, spells, and other DB2-sourced data.

## Pipeline (Priority Order)

### Phase 1: DB2 Re-extraction (HIGHEST IMPACT)
**Why**: Fixes the root cause — server can't find items/spells/factions that ARE in the CASC data.
**How**:
1. Re-run `mapextractor.exe` from the 66709 client with full flags
2. OR use CASCExplorer/CASCToolHost to extract ALL DB2 files
3. Compare extracted file count/sizes against what Wago expects
4. Copy new DB2 files to both build dirs (RelWithDebInfo + Debug)
5. Reboot and check error reduction

**Expected impact**: Could fix 83% of item errors (~37K items), plus factions, spells, etc.

### Phase 2: Wago DB2 → Hotfix Import (MEDIUM IMPACT)
**Why**: For items that exist in Wago but still aren't in our DB2 after re-extraction.
**How**:
1. Query Wago DB2 for all "missing" item IDs (validate_ids in batches)
2. For items that exist in Wago but not in our server, generate INSERT INTO hotfixes.item + hotfixes.item_sparse
3. Generate matching hotfix_data entries
4. Apply SQL

**Tools**: `mcp__wago-db2__db2_lookup`, batch processing script
**Expected impact**: Fills remaining gaps from Phase 1

### Phase 3: Wowhead Scraping — Tor Army (LOW-MEDIUM IMPACT)
**Why**: For the ~17% of items genuinely removed from DB2 in build 66709.
**How**:
1. Generate list of item IDs that don't exist in Wago DB2 either
2. Feed to Tor Army scraper targeting Wowhead item pages
3. Parse scraped data into hotfix SQL (item name, quality, class, subclass, inventory type, etc.)
4. Apply SQL

**Tools**: `tools/wago/scraper_v4.py`, `tools/wago/parsers.py`
**Expected impact**: Recovers old-expansion items that are still referenced in loot tables

### Phase 4: Orphan Cleanup (FINAL POLISH)
**Why**: After Phases 1-3, remaining errors are genuinely orphaned data.
**How**:
1. Re-categorize errors (should be dramatically fewer)
2. DELETE orphan SmartAI scripts (GUIDs that don't exist)
3. DELETE orphan template child rows
4. Zero out loot IDs that still point to nothing
5. Fix/delete broken SmartAI link chains

**SQL files**: `sql/updates/world/master/2026_04_XX_*.sql`

### Phase 5: De-duplication (OPTIONAL)
**Why**: TC and LoreWalker may have conflicting data for the same entries.
**How**:
1. Find creature_template entries that exist in both with different values
2. Prefer LoreWalker data for Midnight/TWW content
3. Prefer TC data for older content (Classic through Dragonflight)
4. Merge SmartAI — keep both sets where they don't conflict

## What Was Already Done (Session 226)

### Applied SQL Files
| File | DB | What | Rows |
|------|-----|------|------|
| `2026_04_04_00_world.sql` | world | Phase 1: template orphans, SmartAI orphans, loot ID zeroing | ~496K |
| `2026_04_04_01_world.sql` | world | Phase 2a: SmartAI links, quest_poi, game_events | ~188K |
| `2026_04_04_00_hotfixes.sql` | hotfixes | Hotfix blob/data orphans | ~203K |
| `2026_04_04_02_world.sql` | world | NOT APPLIED (cross-DB, on hold) | — |

### TC TDB Backfill
- `TC_world_INSERT_IGNORE.sql` — 598MB, 771 tables (766 clean + 5 fixed with explicit columns)
- `TC_hotfixes_INSERT_IGNORE.sql` — 334MB, 433 tables (clean)
- `TC_world_mismatched_fix.sql` — 54MB, 5 tables with column name mapping

### Tools Created
- `tools/tdb_to_insert_ignore.py` — converts mysqldump to INSERT IGNORE format
- `tools/extract_mismatched_tables.py` — handles column count mismatches

## Error Breakdown (Post-Backfill, 6.8M total)

| Count | Category | Fix Phase |
|------:|----------|-----------|
| 1,582K | Loot: creature entry missing | Phase 1 (re-extract DB2) |
| 1,423K | SmartAI: GUID/entry not exist | Phase 4 (orphan cleanup) |
| 744K | Hotfix blob/data | Already partially fixed |
| 671K | Difficulty: unsupported | Phase 4 (delete bad spawns) |
| 654K | SmartAI: Link Event | Phase 4 (delete broken chains) |
| 308K | SmartAI: other | Phase 4 |
| 304K | Flags: disallowed (harmless) | Cosmetic — server auto-fixes |
| 233K | Loot: entry missing (other) | Phase 1-2 |
| 227K | Quest: addon orphan | Phase 4 |
| 174K | Other | Mixed |
| 124K | Template: orphan child | Phase 4 |
| 103K | Faction: missing | Phase 1-2 |

## Key Files
- TC TDB: `ExtTools/TC_TDB/TDB_full_world_1200.26021_2026_02_06.sql`
- LoreWalker TDB: `ExtTools/LoreWalkerTDB/world.sql`
- Old error log: `out/build/.../DBErrors.log.pre_tc_backfill`
- This plan: `doc/db_error_cleanup_plan.md`
