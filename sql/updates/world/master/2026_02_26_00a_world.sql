-- 2026_02_26_00a_world.sql
-- cleanup orphaned rows from LW import
-- Extracted from 2026_02_26_00_world.sql during SQL contamination nuke

SET innodb_lock_wait_timeout=120;

-- ============================================================================
-- 1. QUEST-RELATED ORPHANS (346,724 rows)
--    smart_scripts, quest_template_addon, quest_objectives referencing
--    quests that no longer exist in quest_template (removed from game)
-- ============================================================================

-- smart_scripts (type=5, quest) with no matching quest_template (~288,819 rows)
DELETE ss FROM smart_scripts ss
LEFT JOIN quest_template qt ON ss.entryorguid = qt.ID
WHERE ss.source_type = 5 AND qt.ID IS NULL;

-- quest_template_addon with no matching quest_template (~57,788 rows)
DELETE qta FROM quest_template_addon qta
LEFT JOIN quest_template qt ON qta.ID = qt.ID
WHERE qt.ID IS NULL;

-- quest_objectives with no matching quest_template (~117 rows)
DELETE qo FROM quest_objectives qo
LEFT JOIN quest_template qt ON qo.QuestID = qt.ID
WHERE qt.ID IS NULL;

-- creature_queststarter referencing non-existent quest (~354 rows)
DELETE cqs FROM creature_queststarter cqs
LEFT JOIN quest_template qt ON cqs.quest = qt.ID
WHERE qt.ID IS NULL;

-- creature_questender referencing non-existent quest (~362 rows)
DELETE cqe FROM creature_questender cqe
LEFT JOIN quest_template qt ON cqe.quest = qt.ID
WHERE qt.ID IS NULL;

-- ============================================================================
-- 2. CREATURE_TEMPLATE_DIFFICULTY ORPHANS (~26,071 rows)
--    Difficulty entries for ~7,088 creatures that no longer exist
-- ============================================================================

DELETE ctd FROM creature_template_difficulty ctd
LEFT JOIN creature_template ct ON ctd.Entry = ct.entry
WHERE ct.entry IS NULL;

-- ============================================================================
-- 3. ORPHANED SPAWNS (950 rows)
--    Creature/gameobject spawns referencing non-existent templates
-- ============================================================================

-- Creature spawns with no creature_template (~258 rows)
DELETE c FROM creature c
LEFT JOIN creature_template ct ON c.id = ct.entry
WHERE ct.entry IS NULL;

-- Gameobject spawns with no gameobject_template (~692 rows)
DELETE g FROM gameobject g
LEFT JOIN gameobject_template gt ON g.id = gt.entry
WHERE gt.entry IS NULL;

-- ============================================================================
-- 4. ORPHANED ADDON DATA (~2,424 rows)
--    Addon entries for spawns that no longer exist
-- ============================================================================

-- creature_addon with no matching creature spawn (~3 rows)
DELETE ca FROM creature_addon ca
LEFT JOIN creature c ON ca.guid = c.guid
WHERE c.guid IS NULL;

-- gameobject_addon with no matching gameobject spawn (~2,412 rows)
DELETE ga FROM gameobject_addon ga
LEFT JOIN gameobject g ON ga.guid = g.guid
WHERE g.guid IS NULL;

-- creature_template_addon with no matching creature_template (~9 rows)
DELETE cta FROM creature_template_addon cta
LEFT JOIN creature_template ct ON cta.entry = ct.entry
WHERE ct.entry IS NULL;

-- ============================================================================
-- 5. ORPHANED SPAWN GROUPS & POOL MEMBERS (~3,551 rows)
-- ============================================================================

-- spawn_group referencing non-existent creature (~3,467 rows)
DELETE sg FROM spawn_group sg
LEFT JOIN creature c ON sg.spawnId = c.guid
WHERE sg.spawnType = 0 AND c.guid IS NULL;

-- spawn_group referencing non-existent gameobject (~57 rows)
DELETE sg FROM spawn_group sg
LEFT JOIN gameobject g ON sg.spawnId = g.guid
WHERE sg.spawnType = 1 AND g.guid IS NULL;

-- pool_members (creature) with no matching creature spawn (~18 rows)
DELETE pm FROM pool_members pm
LEFT JOIN creature c ON pm.spawnId = c.guid
WHERE pm.type = 0 AND c.guid IS NULL;

-- pool_members (gameobject) with no matching gameobject spawn (~9 rows)
DELETE pm FROM pool_members pm
LEFT JOIN gameobject g ON pm.spawnId = g.guid
WHERE pm.type = 1 AND g.guid IS NULL;

-- ============================================================================
-- 6. ORPHANED NPC/CREATURE REFERENCES (~616 rows)
--    Various tables referencing non-existent creature_template entries
-- ============================================================================

-- npc_vendor with no matching creature_template (~210 rows)
DELETE nv FROM npc_vendor nv
LEFT JOIN creature_template ct ON nv.entry = ct.entry
WHERE ct.entry IS NULL;

-- creature_queststarter with no matching creature_template (~112 rows)
DELETE cqs FROM creature_queststarter cqs
LEFT JOIN creature_template ct ON cqs.id = ct.entry
WHERE ct.entry IS NULL;

-- creature_questender with no matching creature_template (~14 rows)
DELETE cqe FROM creature_questender cqe
LEFT JOIN creature_template ct ON cqe.id = ct.entry
WHERE ct.entry IS NULL;

-- creature_text with no matching creature_template (~155 rows)
DELETE ct2 FROM creature_text ct2
LEFT JOIN creature_template ct ON ct2.CreatureID = ct.entry
WHERE ct.entry IS NULL AND ct2.CreatureID > 0;

-- creature_template_model with no matching creature_template (~69 rows)
DELETE ctm FROM creature_template_model ctm
LEFT JOIN creature_template ct ON ctm.CreatureID = ct.entry
WHERE ct.entry IS NULL;

-- creature_template_gossip with no matching creature_template (~57 rows)
DELETE ctg FROM creature_template_gossip ctg
LEFT JOIN creature_template ct ON ctg.CreatureID = ct.entry
WHERE ct.entry IS NULL;

-- ============================================================================
-- 7. ORPHANED SMART_SCRIPTS — creature/gameobject (~264 rows)
-- ============================================================================

-- smart_scripts (creature, type=0) with no matching creature_template (~104 rows)
DELETE ss FROM smart_scripts ss
LEFT JOIN creature_template ct ON ss.entryorguid = ct.entry
WHERE ss.source_type = 0 AND ss.entryorguid > 0 AND ct.entry IS NULL;

-- smart_scripts (creature, type=0) with no matching creature spawn, negative guid (~91 rows)
DELETE ss FROM smart_scripts ss
LEFT JOIN creature c ON ss.entryorguid = -CAST(c.guid AS SIGNED)
WHERE ss.source_type = 0 AND ss.entryorguid < 0 AND c.guid IS NULL;

-- smart_scripts (gameobject, type=1) with no matching gameobject_template (~69 rows)
DELETE ss FROM smart_scripts ss
LEFT JOIN gameobject_template gt ON ss.entryorguid = gt.entry
WHERE ss.source_type = 1 AND ss.entryorguid > 0 AND gt.entry IS NULL;

-- ============================================================================
-- 8. ORPHANED GAMEOBJECT QUEST RELATIONS (~121 rows)
-- ============================================================================

-- gameobject_queststarter with no matching gameobject_template (~55 rows)
DELETE gqs FROM gameobject_queststarter gqs
LEFT JOIN gameobject_template gt ON gqs.id = gt.entry
WHERE gt.entry IS NULL;

-- gameobject_questender with no matching gameobject_template (~66 rows)
DELETE gqe FROM gameobject_questender gqe
LEFT JOIN gameobject_template gt ON gqe.id = gt.entry
WHERE gt.entry IS NULL;

-- ============================================================================
-- 9. DATA CORRUPTION FIXES
-- ============================================================================

-- creature_loot_template with negative Chance values (~104 rows)
UPDATE creature_loot_template SET Chance = ABS(Chance) WHERE Chance < 0;

-- Creatures with zero positions — likely bad spawn data (~146 rows)
DELETE FROM creature WHERE position_x = 0 AND position_y = 0 AND position_z = 0;

-- Gameobjects with zero positions (~80 rows)
DELETE FROM gameobject WHERE position_x = 0 AND position_y = 0 AND position_z = 0;

-- ============================================================================
-- 10. CASCADING CLEANUP
--     Addon orphans exposed by zero-position spawn deletes above
-- ============================================================================

-- creature_addon orphaned by zero-position creature deletes (~27 rows)
DELETE ca FROM creature_addon ca
LEFT JOIN creature c ON ca.guid = c.guid
WHERE c.guid IS NULL;

-- gameobject_addon orphaned by zero-position gameobject deletes (~2 rows)
DELETE ga FROM gameobject_addon ga
LEFT JOIN gameobject g ON ga.guid = g.guid
WHERE g.guid IS NULL;
