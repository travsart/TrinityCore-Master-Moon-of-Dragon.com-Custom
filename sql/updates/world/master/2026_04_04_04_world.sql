-- Phase 4: Orphan cleanup — remaining orphan rows after Phase 1-2a cleanup
-- Session 228 Tab A: RoleplayCore re-apply + DB error cleanup
-- Estimated: ~32,644 orphan rows across 10 tables

-- ==========================================================================
-- 1. quest_visual_effect orphans (~30,590 rows)
--    ID references quests that don't exist in quest_template
-- ==========================================================================
DELETE FROM `quest_visual_effect`
WHERE `ID` NOT IN (SELECT `ID` FROM `quest_template`);

-- ==========================================================================
-- 2. creature_addon GUID orphans (~1,284 rows)
--    guid references creatures that don't exist in creature spawn table
-- ==========================================================================
DELETE FROM `creature_addon`
WHERE `guid` NOT IN (SELECT `guid` FROM `creature`);

-- ==========================================================================
-- 3. creature_template_movement orphans (~237 rows)
--    CreatureID references entries that don't exist in creature_template
-- ==========================================================================
DELETE FROM `creature_template_movement`
WHERE `CreatureID` NOT IN (SELECT `entry` FROM `creature_template`);

-- ==========================================================================
-- 4. npc_vendor orphans (~200 rows)
--    entry references entries that don't exist in creature_template
-- ==========================================================================
DELETE FROM `npc_vendor`
WHERE `entry` NOT IN (SELECT `entry` FROM `creature_template`);

-- ==========================================================================
-- 5. creature_queststarter orphans (~114 rows)
--    id references entries that don't exist in creature_template
-- ==========================================================================
DELETE FROM `creature_queststarter`
WHERE `id` NOT IN (SELECT `entry` FROM `creature_template`);

-- ==========================================================================
-- 6. smart_scripts gameobject entry orphans (~69 rows)
--    source_type=1 (GO), positive entryorguid not in gameobject_template
-- ==========================================================================
DELETE FROM `smart_scripts`
WHERE `source_type` = 1
  AND `entryorguid` > 0
  AND `entryorguid` NOT IN (SELECT `entry` FROM `gameobject_template`);

-- ==========================================================================
-- 7. creature_template_gossip orphans (~55 rows)
--    CreatureID references entries that don't exist in creature_template
-- ==========================================================================
DELETE FROM `creature_template_gossip`
WHERE `CreatureID` NOT IN (SELECT `entry` FROM `creature_template`);

-- ==========================================================================
-- 8. creature_formations orphans (~40 rows)
--    leaderGUID references GUIDs that don't exist in creature
-- ==========================================================================
DELETE FROM `creature_formations`
WHERE `leaderGUID` NOT IN (SELECT `guid` FROM `creature`);

-- ==========================================================================
-- 9. creature_questender orphans (~13 rows)
--    id references entries that don't exist in creature_template
-- ==========================================================================
DELETE FROM `creature_questender`
WHERE `id` NOT IN (SELECT `entry` FROM `creature_template`);

-- ==========================================================================
-- 10. creature_template_difficulty orphans (~2 rows)
--     Entry references entries that don't exist in creature_template
-- ==========================================================================
DELETE FROM `creature_template_difficulty`
WHERE `Entry` NOT IN (SELECT `entry` FROM `creature_template`);
