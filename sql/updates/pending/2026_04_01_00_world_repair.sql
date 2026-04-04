-- DB Contamination Repair — April 2026
-- Fixes damage from stale VoxCore SQL that used destructive REPLACE INTO statements
-- with data from old builds (53040/56311/62438/64743/65560).
--
-- Source files refactored (no longer destructive):
--   sql/RoleplayCore/4. world db.sql        — creature_model_info + creature_template
--   sql/RoleplayCore/DarkmoonFaire_patch.sql — creature_template
-- Source files archived (removed from update path):
--   sql/archive/2026_03_17_00_world.sql      — 4MB, 4582 stale refs
--   sql/archive/2026_03_05_34_world.sql      — 515KB, Wowhead HTML junk

-- ============================================================================
-- 1. Fix zeroed creature_model_info entries
-- These 16 DisplayIDs were overwritten with all-zero BoundingRadius/CombatReach
-- by the old 4. world db.sql. Deleting them allows TDB values to be re-imported,
-- or the server will auto-populate them from CreatureDisplayInfo DB2 data at runtime.
-- ============================================================================

DELETE FROM `creature_model_info` WHERE `DisplayID` IN (
    105540, 105328, 105213, 112642, 113958, 114211,
    106003, 107058, 107056, 105268, 115279, 115281,
    116539, 116687, 126177, 113609
) AND `BoundingRadius` = 0 AND `CombatReach` = 0;

-- ============================================================================
-- 2. Clean Wowhead HTML junk from quest_request_items
-- The archived 2026_03_05_34_world.sql inserted ~740 quest rows scraped from
-- Wowhead that contained raw HTML/JavaScript in CompletionText fields.
-- These rows have VerifiedBuild = 0 (not from a real client build).
-- ============================================================================

DELETE FROM `quest_request_items`
WHERE `VerifiedBuild` = 0
  AND (`CompletionText` LIKE '%<div%' OR `CompletionText` LIKE '%<script%'
    OR `CompletionText` LIKE '%<span%' OR `CompletionText` LIKE '%class="icon%'
    OR `CompletionText` LIKE '%wowhead.com%');

-- ============================================================================
-- 3. Restore creature_template ScriptNames
-- The old REPLACE INTO statements overwrote entire creature_template rows.
-- The refactored SQL now uses UPDATE SET ScriptName only.
-- Re-apply ScriptNames here to ensure they're set correctly on the live DB,
-- even if TDB has been re-imported since the contamination.
-- ============================================================================

UPDATE `creature_template` SET `ScriptName` = 'npc_pet_warlock_darkglare' WHERE `entry` = 103673 AND `ScriptName` = '';
UPDATE `creature_template` SET `ScriptName` = 'npc_warl_demonic_gateway' WHERE `entry` IN (59262, 59271) AND `ScriptName` = '';
UPDATE `creature_template` SET `ScriptName` = 'npc_warlock_dreadstalker' WHERE `entry` = 98035 AND `ScriptName` = '';
UPDATE `creature_template` SET `ScriptName` = 'npc_pet_warlock_wild_imp' WHERE `entry` = 55659 AND `ScriptName` = '';
UPDATE `creature_template` SET `ScriptName` = 'npc_warr_ravager' WHERE `entry` = 76168 AND `ScriptName` = '';
UPDATE `creature_template` SET `ScriptName` = 'npc_dk_defile' WHERE `entry` = 82521 AND `ScriptName` = '';
UPDATE `creature_template` SET `ScriptName` = 'npc_monk_jade_serpent_statue' WHERE `entry` = 60849 AND `ScriptName` = '';
UPDATE `creature_template` SET `ScriptName` = 'npc_monk_xuen' WHERE `entry` = 63508 AND `ScriptName` = '';
UPDATE `creature_template` SET `ScriptName` = 'npc_feral_spirit' WHERE `entry` = 29264 AND `ScriptName` = '';

-- Darkmoon Faire ScriptNames
UPDATE `creature_template` SET `ScriptName` = 'npc_canon_maxima' WHERE `entry` = 15303 AND `ScriptName` = '';
UPDATE `creature_template` SET `ScriptName` = 'npc_darkmoon_canon_target' WHERE `entry` = 33068 AND `ScriptName` = '';
UPDATE `creature_template` SET `ScriptName` = 'npc_canon_fozlebub' WHERE `entry` = 57850 AND `ScriptName` = '';
UPDATE `creature_template` SET `ScriptName` = 'npc_dance_battle_simon_sezdans' WHERE `entry` = 181097 AND `ScriptName` = '';
UPDATE `creature_template` SET `ScriptName` = 'npc_ziggie_sparks' WHERE `entry` = 85546 AND `ScriptName` = '';

-- ============================================================================
-- NOTE: If creature_template entries 103673, 59262, 59271, 98035, 55659, 76168,
-- 82521, 60849, 63508, 29264, 102199, 73967, 15303, 33068, 57850, 181097, 85546
-- have stale non-ScriptName column values (from old VerifiedBuild 53040/62438/64743),
-- a TDB re-import will restore them. The refactored setup SQL no longer overwrites
-- these columns.
-- ============================================================================
