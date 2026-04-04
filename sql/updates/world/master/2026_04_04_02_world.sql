-- DB Error Cleanup Phase 2b: Cross-database orphan cleanup (loot items + factions)
-- REQUIRES: mysql CLI execution (cross-DB joins: world + hotfixes)
-- Cannot be applied via worldserver's built-in SQL updater

-- ============================================================================
-- 1. creature_loot_template — items that don't exist in hotfixes.item (2,969,502 rows)
--    Error: "Table 'creature_loot_template' Entry X ItemType 0 Item Y: item does not exist - skipped"
--    99.3% of distinct items in this table don't exist in 12.x item DB.
--    These are legacy TDB loot entries — items can't drop.
-- ============================================================================
DELETE clt FROM world.creature_loot_template clt
LEFT JOIN hotfixes.item i ON i.ID = clt.Item
WHERE clt.ItemType = 0
  AND clt.Item > 0
  AND i.ID IS NULL;

-- ============================================================================
-- 2. gameobject_loot_template — items that don't exist (119,709 rows)
-- ============================================================================
DELETE glt FROM world.gameobject_loot_template glt
LEFT JOIN hotfixes.item i ON i.ID = glt.Item
WHERE glt.ItemType = 0
  AND glt.Item > 0
  AND i.ID IS NULL;

-- ============================================================================
-- 3. disenchant_loot_template — items that don't exist (180 rows)
-- ============================================================================
DELETE dlt FROM world.disenchant_loot_template dlt
LEFT JOIN hotfixes.item i ON i.ID = dlt.Item
WHERE dlt.ItemType = 0
  AND dlt.Item > 0
  AND i.ID IS NULL;

-- ============================================================================
-- 4. spell_loot_template — items that don't exist (503 rows)
-- ============================================================================
DELETE slt FROM world.spell_loot_template slt
LEFT JOIN hotfixes.item i ON i.ID = slt.Item
WHERE slt.ItemType = 0
  AND slt.Item > 0
  AND i.ID IS NULL;

-- ============================================================================
-- 5. creature_template — non-existing faction templates (205,350 rows)
--    Error: "Creature (Entry: X) has non-existing faction template (Y). This can lead to crashes, set to faction 35."
--    Server auto-fixes to faction 35 at runtime. Make DB match.
-- ============================================================================
UPDATE world.creature_template ct
LEFT JOIN hotfixes.faction_template ft ON ft.ID = ct.faction
SET ct.faction = 35
WHERE ct.faction > 0
  AND ft.ID IS NULL;
