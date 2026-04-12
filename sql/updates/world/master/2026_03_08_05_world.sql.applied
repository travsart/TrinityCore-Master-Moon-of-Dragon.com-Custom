-- LoreWalker TDB Import — File 5: Behavioral
-- Source: lorewalker_world (build 66102) → Target: world (build 66263)
-- smart_scripts: 171,162 | waypoint_path: 61 | waypoint_path_node: 87
-- npc_vendor: 3,910 | gossip_menu: 399 | gossip_menu_option: 279
-- npc_text: 343 | creature_text: 232 | scene_template: 195 | conditions: 1,044
-- Total: 177,712 rows

SET autocommit=0;

-- smart_scripts (PK: entryorguid,source_type,id,link | no VB | identical schema)
-- source_type filter: 0=creature, 1=gameobject, 2=areatrigger, 9=timed actionlist, 12=spell
-- Excludes: type 5 (LW custom scene extension, 526K rows)
INSERT IGNORE INTO world.smart_scripts
SELECT * FROM lorewalker_world.smart_scripts l
WHERE l.source_type IN (0, 1, 2, 9, 12)
  AND l.entryorguid < 9100000
  AND l.entryorguid > -9100000
  AND NOT EXISTS (SELECT 1 FROM world.smart_scripts w
    WHERE w.entryorguid = l.entryorguid AND w.source_type = l.source_type
    AND w.id = l.id AND w.link = l.link);

-- waypoint_path_node (PK: PathId,NodeId | no VB | identical schema)
-- MUST run BEFORE waypoint_path INSERT so NOT IN correctly identifies new paths
INSERT IGNORE INTO world.waypoint_path_node
SELECT * FROM lorewalker_world.waypoint_path_node l
WHERE l.PathId NOT IN (SELECT PathId FROM world.waypoint_path);

-- waypoint_path (PK: PathId | no VB | identical schema — includes Velocity column)
INSERT IGNORE INTO world.waypoint_path
SELECT * FROM lorewalker_world.waypoint_path;

-- npc_vendor (PK: entry,item,ExtendedCost,type | 11 cols | VB → 0)
-- SCHEMA DIFF: world has extra 'OverrideGoldCost' column (default -1) — excluded
INSERT IGNORE INTO world.npc_vendor
  (entry, slot, item, maxcount, incrtime, ExtendedCost, type,
   BonusListIDs, PlayerConditionID, IgnoreFiltering, VerifiedBuild)
SELECT l.entry, l.slot, l.item, l.maxcount, l.incrtime, l.ExtendedCost, l.type,
       l.BonusListIDs, l.PlayerConditionID, l.IgnoreFiltering, 0
FROM lorewalker_world.npc_vendor l
WHERE l.entry < 9100000;

-- gossip_menu (PK: MenuID,TextID | 3 cols | VB → 0)
INSERT IGNORE INTO world.gossip_menu (MenuID, TextID, VerifiedBuild)
SELECT l.MenuID, l.TextID, 0
FROM lorewalker_world.gossip_menu l;

-- gossip_menu_option (PK: MenuID,OptionID | 18 cols | VB → 0)
DROP TEMPORARY TABLE IF EXISTS _imp;
CREATE TEMPORARY TABLE _imp AS
SELECT * FROM lorewalker_world.gossip_menu_option l
WHERE NOT EXISTS (SELECT 1 FROM world.gossip_menu_option w
                  WHERE w.MenuID = l.MenuID AND w.OptionID = l.OptionID);
UPDATE _imp SET VerifiedBuild = 0;
INSERT IGNORE INTO world.gossip_menu_option SELECT * FROM _imp;
DROP TEMPORARY TABLE IF EXISTS _imp;

-- npc_text (PK: ID | 18 cols | VB → 0)
DROP TEMPORARY TABLE IF EXISTS _imp;
CREATE TEMPORARY TABLE _imp AS
SELECT * FROM lorewalker_world.npc_text l
WHERE NOT EXISTS (SELECT 1 FROM world.npc_text w WHERE w.ID = l.ID);
UPDATE _imp SET VerifiedBuild = 0;
INSERT IGNORE INTO world.npc_text SELECT * FROM _imp;
DROP TEMPORARY TABLE IF EXISTS _imp;

-- creature_text (PK: CreatureID,GroupID,ID | no VB | identical schema)
INSERT IGNORE INTO world.creature_text
SELECT * FROM lorewalker_world.creature_text
WHERE CreatureID < 9100000;

-- scene_template (PK: SceneId | no VB)
-- SCHEMA DIFF: LW has extra 'RTComment' column — explicit column list excludes it
INSERT IGNORE INTO world.scene_template (SceneId, Flags, ScriptPackageID, Encrypted, ScriptName)
SELECT l.SceneId, l.Flags, l.ScriptPackageID, l.Encrypted, l.ScriptName
FROM lorewalker_world.scene_template l;

-- conditions (PK: 11-column composite | no VB | identical schema)
INSERT IGNORE INTO world.conditions
SELECT l.* FROM lorewalker_world.conditions l
WHERE NOT EXISTS (SELECT 1 FROM world.conditions w
  WHERE w.SourceTypeOrReferenceId = l.SourceTypeOrReferenceId
    AND w.SourceGroup = l.SourceGroup AND w.SourceEntry = l.SourceEntry
    AND w.SourceId = l.SourceId AND w.ElseGroup = l.ElseGroup
    AND w.ConditionTypeOrReference = l.ConditionTypeOrReference
    AND w.ConditionTarget = l.ConditionTarget
    AND w.ConditionValue1 = l.ConditionValue1
    AND w.ConditionValue2 = l.ConditionValue2
    AND w.ConditionValue3 = l.ConditionValue3);

COMMIT;
