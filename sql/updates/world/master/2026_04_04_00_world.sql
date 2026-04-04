-- DB Error Cleanup Phase 1: Orphan references in world DB
-- Eliminates ~5.5M+ startup errors from DBErrors.log
-- Categories: template orphans, SmartAI orphans, loot reference orphans, quest orphans

-- ============================================================================
-- 1. creature_template_difficulty — orphan entries (26,001 rows)
--    Error: "Creature template (Entry: X) does not exist but has a record in `creature_template_difficulty`"
-- ============================================================================
DELETE ctd FROM creature_template_difficulty ctd
LEFT JOIN creature_template ct ON ctd.Entry = ct.entry
WHERE ct.entry IS NULL;

-- ============================================================================
-- 2. creature_template_model — orphan entries (67 rows)
--    Error: "Creature template (Entry: X) does not exist but has a record in `creature_template_model`"
-- ============================================================================
DELETE ctm FROM creature_template_model ctm
LEFT JOIN creature_template ct ON ctm.CreatureID = ct.entry
WHERE ct.entry IS NULL;

-- ============================================================================
-- 3. creature_template_addon — orphan entries (9 rows)
--    Error: "Creature template (Entry: X) does not exist but has a record in `creature_template_addon`"
-- ============================================================================
DELETE cta FROM creature_template_addon cta
LEFT JOIN creature_template ct ON cta.entry = ct.entry
WHERE ct.entry IS NULL;

-- ============================================================================
-- 4. gameobject_template_addon — orphan entries (18 rows)
--    Error: "GameObject template (Entry: X) does not exist but has a record in `gameobject_template_addon`"
-- ============================================================================
DELETE gta FROM gameobject_template_addon gta
LEFT JOIN gameobject_template gt ON gta.entry = gt.entry
WHERE gt.entry IS NULL;

-- ============================================================================
-- 5. quest_template_addon — orphan entries (56,868 rows)
--    Error: "Table `quest_template_addon` has data for quest X but such quest does not exist"
-- ============================================================================
DELETE qta FROM quest_template_addon qta
LEFT JOIN quest_template qt ON qta.ID = qt.ID
WHERE qt.ID IS NULL;

-- ============================================================================
-- 6. SmartAI — GUID-based orphans (91 rows)
--    Error: "SmartAIMgr::LoadSmartAIFromDB: Creature guid (X) does not exist, skipped loading."
--    In smart_scripts, negative entryorguid with source_type=0 = creature GUID
-- ============================================================================
DELETE ss FROM smart_scripts ss
LEFT JOIN creature c ON c.guid = ABS(ss.entryorguid)
WHERE ss.source_type = 0
  AND ss.entryorguid < 0
  AND c.guid IS NULL;

-- ============================================================================
-- 7. SmartAI — entry-based orphans (104 rows)
--    Creature entries referenced in SmartAI that don't exist in creature_template
-- ============================================================================
DELETE ss FROM smart_scripts ss
LEFT JOIN creature_template ct ON ct.entry = ss.entryorguid
WHERE ss.source_type = 0
  AND ss.entryorguid > 0
  AND ct.entry IS NULL;

-- ============================================================================
-- 8. Loot ID references — creature_template_difficulty.LootID pointing to
--    nonexistent creature_loot_template entries (420,035 rows)
--    Error: "Table 'creature_loot_template' Entry X does not exist but it is used by Creature Y"
-- ============================================================================
UPDATE creature_template_difficulty ctd
LEFT JOIN (SELECT DISTINCT Entry FROM creature_loot_template) clt ON clt.Entry = ctd.LootID
SET ctd.LootID = 0
WHERE ctd.LootID > 0
  AND clt.Entry IS NULL;

-- ============================================================================
-- 9. PickPocketLootID references — pointing to nonexistent pickpocketing_loot_template (7,516 rows)
--    Error: "Table 'pickpocketing_loot_template' Entry X does not exist but it is used by Creature Y"
-- ============================================================================
UPDATE creature_template_difficulty ctd
LEFT JOIN (SELECT DISTINCT Entry FROM pickpocketing_loot_template) plt ON plt.Entry = ctd.PickPocketLootID
SET ctd.PickPocketLootID = 0
WHERE ctd.PickPocketLootID > 0
  AND plt.Entry IS NULL;

-- ============================================================================
-- 10. SkinLootID references — pointing to nonexistent skinning_loot_template (9,957 rows)
--     Error: "Table 'skinning_loot_template' Entry X does not exist but it is used by Creature Y"
-- ============================================================================
UPDATE creature_template_difficulty ctd
LEFT JOIN (SELECT DISTINCT Entry FROM skinning_loot_template) slt ON slt.Entry = ctd.SkinLootID
SET ctd.SkinLootID = 0
WHERE ctd.SkinLootID > 0
  AND slt.Entry IS NULL;
