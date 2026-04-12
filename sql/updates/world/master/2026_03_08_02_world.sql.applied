-- LoreWalker TDB Import — File 2: Templates
-- Source: lorewalker_world (build 66102) → Target: world (build 66263)
-- creature_template: 10 | creature_template_difficulty: 26,086
-- creature_template_model: 3,428 | creature_template_addon: 122
-- creature_template_spell: 163 | creature_equip_template: 2,023
-- gameobject_template: 2,089 | gameobject_template_addon: 57
-- Total: 33,978 rows

SET autocommit=0;

-- creature_template (PK: entry | identical schema | VB → 0)
DROP TEMPORARY TABLE IF EXISTS _imp;
CREATE TEMPORARY TABLE _imp AS
SELECT * FROM lorewalker_world.creature_template l
WHERE l.entry < 9100000
  AND NOT EXISTS (SELECT 1 FROM world.creature_template w WHERE w.entry = l.entry);
UPDATE _imp SET VerifiedBuild = 0;
INSERT IGNORE INTO world.creature_template SELECT * FROM _imp;
DROP TEMPORARY TABLE IF EXISTS _imp;

-- creature_template_difficulty (PK: Entry,DifficultyID | VB → 0 | ALL missing rows, not just new templates)
DROP TEMPORARY TABLE IF EXISTS _imp;
CREATE TEMPORARY TABLE _imp AS
SELECT * FROM lorewalker_world.creature_template_difficulty l
WHERE l.Entry < 9100000
  AND NOT EXISTS (SELECT 1 FROM world.creature_template_difficulty w
                  WHERE w.Entry = l.Entry AND w.DifficultyID = l.DifficultyID);
UPDATE _imp SET VerifiedBuild = 0;
INSERT IGNORE INTO world.creature_template_difficulty SELECT * FROM _imp;
DROP TEMPORARY TABLE IF EXISTS _imp;

-- creature_template_model (PK: CreatureID,Idx | 6 cols | VB → 0)
INSERT IGNORE INTO world.creature_template_model
  (CreatureID, Idx, CreatureDisplayID, DisplayScale, Probability, VerifiedBuild)
SELECT l.CreatureID, l.Idx, l.CreatureDisplayID, l.DisplayScale, l.Probability, 0
FROM lorewalker_world.creature_template_model l
WHERE l.CreatureID < 9100000;

-- creature_template_addon (PK: entry | no VB | identical schema)
INSERT IGNORE INTO world.creature_template_addon
SELECT * FROM lorewalker_world.creature_template_addon
WHERE entry < 9100000;

-- creature_template_spell (PK: CreatureID,`Index` | 4 cols | VB → 0)
INSERT IGNORE INTO world.creature_template_spell
  (CreatureID, `Index`, Spell, VerifiedBuild)
SELECT l.CreatureID, l.`Index`, l.Spell, 0
FROM lorewalker_world.creature_template_spell l
WHERE l.CreatureID < 9100000;

-- creature_equip_template (PK: CreatureID,ID | 12 cols | VB → 0)
INSERT IGNORE INTO world.creature_equip_template
  (CreatureID, ID, ItemID1, AppearanceModID1, ItemVisual1,
   ItemID2, AppearanceModID2, ItemVisual2,
   ItemID3, AppearanceModID3, ItemVisual3, VerifiedBuild)
SELECT l.CreatureID, l.ID, l.ItemID1, l.AppearanceModID1, l.ItemVisual1,
       l.ItemID2, l.AppearanceModID2, l.ItemVisual2,
       l.ItemID3, l.AppearanceModID3, l.ItemVisual3, 0
FROM lorewalker_world.creature_equip_template l
WHERE l.CreatureID < 9100000;

-- gameobject_template (PK: entry | identical schema | VB → 0)
DROP TEMPORARY TABLE IF EXISTS _imp;
CREATE TEMPORARY TABLE _imp AS
SELECT * FROM lorewalker_world.gameobject_template l
WHERE l.entry < 9100000
  AND NOT EXISTS (SELECT 1 FROM world.gameobject_template w WHERE w.entry = l.entry);
UPDATE _imp SET VerifiedBuild = 0;
INSERT IGNORE INTO world.gameobject_template SELECT * FROM _imp;
DROP TEMPORARY TABLE IF EXISTS _imp;

-- gameobject_template_addon (PK: entry | no VB | identical schema)
INSERT IGNORE INTO world.gameobject_template_addon
SELECT * FROM lorewalker_world.gameobject_template_addon
WHERE entry < 9100000;

COMMIT;
