-- LoreWalker TDB Import — File 6: Loot Tables
-- Source: lorewalker_world (build 66102) → Target: world (build 66263)
-- reference_loot_template: 51 | gameobject_loot_template: 60,244
-- pickpocketing_loot_template: 1,389 | skinning_loot_template: 402
-- item_loot_template: 110 | spell_loot_template: 64
-- Total: 62,260 rows
-- NOTE: creature_loot_template intentionally excluded (we lead with raidbots data)

SET autocommit=0;

-- reference_loot_template (MUST come first — referenced by other loot tables)
INSERT IGNORE INTO world.reference_loot_template
SELECT l.* FROM lorewalker_world.reference_loot_template l
WHERE NOT EXISTS (
  SELECT 1 FROM world.reference_loot_template w
  WHERE w.Entry = l.Entry AND w.ItemType = l.ItemType AND w.Item = l.Item
);

-- gameobject_loot_template
INSERT IGNORE INTO world.gameobject_loot_template
SELECT l.* FROM lorewalker_world.gameobject_loot_template l
WHERE NOT EXISTS (
  SELECT 1 FROM world.gameobject_loot_template w
  WHERE w.Entry = l.Entry AND w.ItemType = l.ItemType AND w.Item = l.Item
);

-- pickpocketing_loot_template
INSERT IGNORE INTO world.pickpocketing_loot_template
SELECT l.* FROM lorewalker_world.pickpocketing_loot_template l
WHERE NOT EXISTS (
  SELECT 1 FROM world.pickpocketing_loot_template w
  WHERE w.Entry = l.Entry AND w.ItemType = l.ItemType AND w.Item = l.Item
);

-- skinning_loot_template
INSERT IGNORE INTO world.skinning_loot_template
SELECT l.* FROM lorewalker_world.skinning_loot_template l
WHERE NOT EXISTS (
  SELECT 1 FROM world.skinning_loot_template w
  WHERE w.Entry = l.Entry AND w.ItemType = l.ItemType AND w.Item = l.Item
);

-- item_loot_template
INSERT IGNORE INTO world.item_loot_template
SELECT l.* FROM lorewalker_world.item_loot_template l
WHERE NOT EXISTS (
  SELECT 1 FROM world.item_loot_template w
  WHERE w.Entry = l.Entry AND w.ItemType = l.ItemType AND w.Item = l.Item
);

-- spell_loot_template
INSERT IGNORE INTO world.spell_loot_template
SELECT l.* FROM lorewalker_world.spell_loot_template l
WHERE NOT EXISTS (
  SELECT 1 FROM world.spell_loot_template w
  WHERE w.Entry = l.Entry AND w.ItemType = l.ItemType AND w.Item = l.Item
);

COMMIT;
