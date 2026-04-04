# TC Upstream vs VoxCore SQL Update Audit

**Date**: 2026-03-31
**TC Upstream**: `C:\Users\atayl\repos\tc-upstream\sql\updates\`
**VoxCore**: `C:\Users\atayl\VoxCore\sql\updates\`
**Method**: md5sum comparison + diff analysis for all files in `{db}/master/` subdirectories

---

## Executive Summary

| Database | TC Files | VC Files | Identical | Superset | Different | Collision | Missing in VC | VoxCore-Only |
|----------|----------|----------|-----------|----------|-----------|-----------|---------------|-------------|
| auth | 18 | 24 | 15 | 2 | 1 | 0 | 0 | 6 |
| characters | 2 | 6 | 2 | 0 | 0 | 0 | 0 | 4 |
| hotfixes | 28 | 50 | 27 | 0 | 0 | 1 | 0 | 22 |
| world | 74 | 216 | 61 | 13 | 0 | 0 | 0 | 142 |
| **TOTAL** | **122** | **296** | **105** | **15** | **1** | **1** | **0** | **174** |

**Key findings:**
- Zero TC upstream files are missing from VoxCore (full upstream coverage)
- 105 of 122 TC files (86%) are byte-identical in VoxCore
- 15 files are supersets (VoxCore contains all TC content plus VoxCore additions)
- 1 file has a minor behavioral difference (auth `WHERE` clause)
- 1 file is a naming collision (hotfixes `2026_03_21_00` -- completely different content)
- VoxCore has 174 additional custom SQL files not in TC upstream
- VoxCore also has `roleplay/master/` and `applied/`/`pending/` directories (not in TC)

### Category Definitions

| Category | Meaning |
|----------|---------|
| **IDENTICAL** | Byte-for-byte match (md5sum identical) |
| **SUPERSET** | VoxCore contains every line from TC upstream plus additional VoxCore-specific content |
| **DIFFERENT** | Same filename, same line count, but content differs (not a superset) |
| **COLLISION** | Same filename but completely different content (zero shared lines) |
| **MISSING** | TC file not present in VoxCore at all |
| **VOXCORE_ONLY** | File exists only in VoxCore (custom addition) |


---

## 1. Auth Database

**TC upstream**: 18 files | **VoxCore**: 24 files | **Identical**: 15 | **Superset**: 2 | **Different**: 1 | **VoxCore-only**: 6

### TC Upstream Files

| TC Upstream File | VoxCore Status | Notes |
|---|---|---|
| `2026_02_06_00_auth.sql` | IDENTICAL | |
| `2026_02_12_00_auth.sql` | IDENTICAL | |
| `2026_02_14_00_auth.sql` | IDENTICAL | |
| `2026_02_18_00_auth.sql` | IDENTICAL | |
| `2026_02_20_00_auth.sql` | IDENTICAL | |
| `2026_02_20_01_auth.sql` | IDENTICAL | |
| `2026_02_25_00_auth.sql` | IDENTICAL | |
| `2026_02_25_01_auth.sql` | **DIFFERENT** | TC: `WHERE gamebuild=66066` vs VC: `WHERE 1` (broader update) |
| `2026_02_27_00_auth.sql` | IDENTICAL | |
| `2026_03_02_00_auth.sql` | IDENTICAL | |
| `2026_03_03_00_auth.sql` | IDENTICAL | |
| `2026_03_04_00_auth.sql` | **SUPERSET** | TC: 23L, VC: 41L. VC prepends build_info + auth_key inserts for build 66220 before TC content |
| `2026_03_05_00_auth.sql` | **SUPERSET** | TC: 23L, VC: 39L. VC prepends build 66263 registration + key placeholders before TC content |
| `2026_03_10_00_auth.sql` | IDENTICAL | |
| `2026_03_14_00_auth.sql` | IDENTICAL | |
| `2026_03_18_00_auth.sql` | IDENTICAL | |
| `2026_03_20_00_auth.sql` | IDENTICAL | |
| `2026_03_21_00_auth.sql` | IDENTICAL | |

### VoxCore-Only Files

| VoxCore File | Lines | Purpose |
|---|---|---|
| `2026_02_26_00_auth.sql` | 5 | Create `battlenet_transmog_set_favorites` table |
| `2026_03_03_01_auth.sql` | 11 | Build 66220 build_info + auth key placeholder |
| `2026_03_12_00_auth.sql` | 4 | CreatureCodex RBAC permission (`id=3012`, `.codex` command) |
| `2026_03_25_00_auth.sql` | 23 | Build 66562 build_info + auth keys |
| `2026_03_26_00_auth.sql` | 23 | Build 66666 build_info + auth keys |
| `2026_03_28_00_auth.sql` | 23 | Build 66709 build_info + auth keys |


---

## 2. Characters Database

**TC upstream**: 2 files | **VoxCore**: 6 files | **Identical**: 2 | **VoxCore-only**: 4

### TC Upstream Files

| TC Upstream File | VoxCore Status | Notes |
|---|---|---|
| `2026_02_06_00_characters.sql` | IDENTICAL | |
| `2026_03_21_00_characters.sql` | IDENTICAL | |

### VoxCore-Only Files

| VoxCore File | Lines | Purpose |
|---|---|---|
| `2026_02_26_00_characters.sql` | 14 | Transmog outfit situation auto-switch persistence table |
| `2026_02_26_01_characters.sql` | 9 | Secondary shoulder appearance columns for asymmetric transmog |
| `2026_03_05_00_characters.sql` | 1 | Add `active` column to `character_transmog_outfits` |
| `2026_03_31_00_characters.sql` | 8 | Drop legacy flat transmog tables (replaced by normalized schema) |


---

## 3. Hotfixes Database

**TC upstream**: 28 files | **VoxCore**: 50 files | **Identical**: 27 | **Collision**: 1 | **VoxCore-only**: 22

### TC Upstream Files

| TC Upstream File | VoxCore Status | Notes |
|---|---|---|
| `2026_02_06_00_hotfixes.sql` | IDENTICAL | |
| `2026_02_12_00_hotfixes.sql` | IDENTICAL | |
| `2026_02_18_00_hotfixes_enUS.sql` | IDENTICAL | |
| `2026_02_18_01_hotfixes_deDE.sql` | IDENTICAL | |
| `2026_02_18_02_hotfixes_esES.sql` | IDENTICAL | |
| `2026_02_18_03_hotfixes_esMX.sql` | IDENTICAL | |
| `2026_02_18_04_hotfixes_frFR.sql` | IDENTICAL | |
| `2026_02_18_05_hotfixes_itIT.sql` | IDENTICAL | |
| `2026_02_18_06_hotfixes_koKR.sql` | IDENTICAL | |
| `2026_02_18_07_hotfixes_ptBR.sql` | IDENTICAL | |
| `2026_02_18_08_hotfixes_ruRU.sql` | IDENTICAL | |
| `2026_02_18_09_hotfixes_zhCN.sql` | IDENTICAL | |
| `2026_02_18_10_hotfixes_zhTW.sql` | IDENTICAL | |
| `2026_03_01_00_hotfixes.sql` | IDENTICAL | |
| `2026_03_08_00_hotfixes.sql` | IDENTICAL | |
| `2026_03_10_00_hotfixes.sql` | IDENTICAL | |
| `2026_03_16_00_hotfixes_enUS.sql` | IDENTICAL | |
| `2026_03_16_01_hotfixes_deDE.sql` | IDENTICAL | |
| `2026_03_16_02_hotfixes_esES.sql` | IDENTICAL | |
| `2026_03_16_03_hotfixes_esMX.sql` | IDENTICAL | |
| `2026_03_16_04_hotfixes_frFR.sql` | IDENTICAL | |
| `2026_03_16_05_hotfixes_itIT.sql` | IDENTICAL | |
| `2026_03_16_06_hotfixes_koKR.sql` | IDENTICAL | |
| `2026_03_16_07_hotfixes_ptBR.sql` | IDENTICAL | |
| `2026_03_16_08_hotfixes_ruRU.sql` | IDENTICAL | |
| `2026_03_16_09_hotfixes_zhCN.sql` | IDENTICAL | |
| `2026_03_16_10_hotfixes_zhTW.sql` | IDENTICAL | |
| `2026_03_21_00_hotfixes.sql` | **COLLISION** | TC: 190L (creates `transmog_outfit_entry`/`transmog_outfit_entry_locale` tables). VC: 21L (spell_effect fix + hotfix_blob cleanup). **Zero shared content.** |

### VoxCore-Only Files

| VoxCore File | Lines | Purpose |
|---|---|---|
| `2026_02_25_00_hotfixes.sql` | 53 | Remove hotfix_blob entries for DB2 stores with typed tables |
| `2026_02_25_01_hotfixes.sql` | 24 | Fix TransmogSetItem hotfix DELETE records causing Lua errors |
| `2026_02_25_02_hotfixes.sql` | 16 | Fix stale TransmogHoliday hotfix entries |
| `2026_02_26_00_hotfixes.sql` | 12 | Delete orphaned TransmogSetItem records |
| `2026_02_26_01_hotfixes.sql` | 16963 | ItemSparse + Item hotfix import (4188 sparse + 4188 item rows) |
| `2026_02_26_02_hotfixes.sql` | 1232 | Hotfix import for changed tables: 66066 -> 66102 |
| `2026_02_26_03_hotfixes.sql` | 8 | Fix hotfix_data references to unknown DB2 store hashes |
| `2026_03_03_01_hotfixes.sql` | 11 | SPELL_EFFECT_SCRIPT_EFFECT for spell 1247917 (Clear Transmog) |
| `2026_03_04_00_hotfixes.sql` | 5 | Hotfix cleanup |
| `2026_03_04_01_hotfixes.sql` | 41 | Wire up Founder's Point portal spell (1235595) |
| `2026_03_04_02_hotfixes.sql` | 39 | Wire up Silvermoon portal spell (1259194) |
| `2026_03_04_03_hotfixes.sql` | 7 | Clean up Silvermoon spell hotfix rows |
| `2026_03_04_04_hotfixes.sql` | 372 | Hotfix data batch |
| `2026_03_04_05_hotfixes.sql` | 6 | Hotfix cleanup |
| `2026_03_05_00_hotfixes.sql` | 92 | Hotfix data batch |
| `2026_03_05_01_hotfixes.sql` | 824 | Fill 332 missing broadcast_text entries from retail DB2 |
| `2026_03_07_00_hotfixes.sql` | 24 | Hotfix data batch |
| `2026_03_07_01_hotfixes.sql` | 29 | Hotfix data batch |
| `2026_03_07_02_hotfixes.sql` | 283 | Hotfix data batch |
| `2026_03_07_03_hotfixes.sql` | 15 | Hotfix data batch |
| `2026_03_07_04_hotfixes.sql` | 62 | Hotfix data batch |
| `2026_03_21_01_hotfixes.sql` | 33 | Haranir Alliance (race 91) + Earthen Horde (race 86) CharBaseInfo |


---

## 4. World Database

**TC upstream**: 74 files | **VoxCore**: 216 files | **Identical**: 61 | **Superset**: 13 | **VoxCore-only**: 142

### TC Upstream Files

| TC Upstream File | VoxCore Status | Notes |
|---|---|---|
| `2026_02_06_00_world.sql` | IDENTICAL | |
| `2026_02_06_01_world.sql` | IDENTICAL | |
| `2026_02_07_00_world.sql` | IDENTICAL | |
| `2026_02_08_00_world.sql` | IDENTICAL | |
| `2026_02_09_00_world.sql` | IDENTICAL | |
| `2026_02_13_00_world.sql` | IDENTICAL | |
| `2026_02_13_01_world.sql` | IDENTICAL | |
| `2026_02_14_00_world.sql` | IDENTICAL | |
| `2026_02_14_01_world.sql` | IDENTICAL | |
| `2026_02_14_02_world.sql` | IDENTICAL | |
| `2026_02_14_03_world.sql` | IDENTICAL | |
| `2026_02_14_04_world.sql` | IDENTICAL | |
| `2026_02_15_00_world.sql` | IDENTICAL | |
| `2026_02_15_01_world.sql` | IDENTICAL | |
| `2026_02_16_00_world.sql` | IDENTICAL | |
| `2026_02_16_01_world.sql` | IDENTICAL | |
| `2026_02_16_02_world.sql` | IDENTICAL | |
| `2026_02_16_03_world.sql` | IDENTICAL | |
| `2026_02_16_04_world.sql` | IDENTICAL | |
| `2026_02_19_00_world.sql` | IDENTICAL | |
| `2026_02_19_01_world.sql` | IDENTICAL | |
| `2026_02_19_02_world.sql` | IDENTICAL | |
| `2026_02_19_03_world.sql` | IDENTICAL | |
| `2026_02_19_04_world.sql` | IDENTICAL | |
| `2026_02_19_05_world.sql` | IDENTICAL | |
| `2026_02_19_06_world.sql` | IDENTICAL | |
| `2026_02_19_07_world.sql` | IDENTICAL | |
| `2026_02_19_08_world.sql` | IDENTICAL | |
| `2026_02_19_09_world.sql` | IDENTICAL | |
| `2026_02_23_00_world.sql` | IDENTICAL | |
| `2026_02_23_01_world.sql` | IDENTICAL | |
| `2026_02_23_02_world.sql` | IDENTICAL | |
| `2026_02_23_03_world.sql` | IDENTICAL | |
| `2026_02_26_00_world.sql` | **SUPERSET** | TC: 3L (spell_dh_void_ray), VC: 207L (TC content + orphan row cleanup from LW import) |
| `2026_02_26_01_world.sql` | **SUPERSET** | TC: 3L (spell_dh_voidglare_boon), VC: 34L (TC content + orphan loot/conditions/text cleanup) |
| `2026_02_27_00_world.sql` | **SUPERSET** | TC: 8L (spell_pri_evangelism), VC: 18L (TC content + companion creature unit_class fixes) |
| `2026_02_28_00_world.sql` | **SUPERSET** | TC: 7L (spell_warr_surge_of_adrenaline), VC: 126L (TC content + Hero's Call Board duplicate fixes) |
| `2026_02_28_01_world.sql` | IDENTICAL | |
| `2026_02_28_02_world.sql` | IDENTICAL | |
| `2026_03_01_00_world.sql` | IDENTICAL | |
| `2026_03_02_00_world.sql` | IDENTICAL | |
| `2026_03_02_01_world.sql` | IDENTICAL | |
| `2026_03_03_00_world.sql` | IDENTICAL | |
| `2026_03_04_00_world.sql` | **SUPERSET** | TC: 37L (spell_proc Whirlwind), VC: 44L (TC content + Stormwind guard quest giver flag fixes) |
| `2026_03_04_01_world.sql` | **SUPERSET** | TC: 5L (spell_warr_keep_your_feet), VC: 26L (TC content + zoneId=0 fix for Stormwind spawns) |
| `2026_03_05_00_world.sql` | **SUPERSET** | TC: 12L (spell_proc Thunder Blast), VC: 122L (TC content + 26,745 missing DifficultyID=0 creature fixes) |
| `2026_03_05_01_world.sql` | **SUPERSET** | TC: 3L (spell_proc Whirlwind), VC: 23L (TC content + SmartAI orphan cleanup) |
| `2026_03_06_00_world.sql` | **SUPERSET** | TC: 9L (spell_warr_brutal_finish), VC: 5741L (TC content + quest integrity/BtWQuests repairs) |
| `2026_03_06_01_world.sql` | **SUPERSET** | TC: 3L (spell_proc Greater Smite), VC: 457L (TC content + ATT faction gates/boss classification) |
| `2026_03_06_02_world.sql` | **SUPERSET** | TC: 9L (spell_dru_galactic_guardian), VC: 1814L (TC content + ATT quest metadata: breadcrumb/daily/weekly) |
| `2026_03_06_03_world.sql` | **SUPERSET** | TC: 3L (spell_pri_archangel), VC: 3766L (TC content + removed deprecated flags + missing game events) |
| `2026_03_06_04_world.sql` | **SUPERSET** | TC: 7L (spell_pri_searing_light), VC: 3370L (TC content + quest POI coords + GO spawns) |
| `2026_03_07_00_world.sql` | IDENTICAL | |
| `2026_03_07_01_world.sql` | IDENTICAL | |
| `2026_03_09_00_world.sql` | IDENTICAL | |
| `2026_03_10_00_world.sql` | IDENTICAL | |
| `2026_03_10_01_world.sql` | IDENTICAL | |
| `2026_03_10_02_world.sql` | IDENTICAL | |
| `2026_03_16_00_world_enUS.sql` | IDENTICAL | |
| `2026_03_16_01_world_deDE.sql` | IDENTICAL | |
| `2026_03_16_02_world_esES.sql` | IDENTICAL | |
| `2026_03_16_03_world_esMX.sql` | IDENTICAL | |
| `2026_03_16_04_world_frFR.sql` | IDENTICAL | |
| `2026_03_16_05_world_itIT.sql` | IDENTICAL | |
| `2026_03_16_06_world_koKR.sql` | IDENTICAL | |
| `2026_03_16_07_world_ptBR.sql` | IDENTICAL | |
| `2026_03_16_08_world_ruRU.sql` | IDENTICAL | |
| `2026_03_16_09_world_zhCN.sql` | IDENTICAL | |
| `2026_03_16_10_world_zhTW.sql` | IDENTICAL | |
| `2026_03_16_11_world.sql` | IDENTICAL | |
| `2026_03_17_00_world.sql` | IDENTICAL | |
| `2026_03_17_01_world.sql` | IDENTICAL | |
| `2026_03_19_00_world.sql` | IDENTICAL | |
| `2026_03_19_01_world.sql` | IDENTICAL | |

