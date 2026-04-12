-- LoreWalker TDB Import — File 4: Spawns
-- Source: lorewalker_world (build 66102) → Target: world (build 66263)
-- creature: 101,018 | gameobject: 1,290 | creature_addon: ~16,609 (post-import)
-- gameobject_addon: ~3 | spawn_group: 10,145 | pool_template: 1,886
-- pool_members: 240 | creature_formations: ~1,243 (post-import)
-- game_event_creature: ~257 (post-import) | game_event_gameobject: ~17 (post-import)
-- Total: ~119,472 rows (post-import estimates)

SET autocommit=0;

-- creature (PK: guid)
-- SCHEMA DIFF: world.creature has extra column 'size' (default -1) — excluded
INSERT IGNORE INTO world.creature
  (guid, id, map, zoneId, areaId, spawnDifficulties, phaseUseFlags, PhaseId, PhaseGroup,
   terrainSwapMap, modelid, equipment_id, position_x, position_y, position_z, orientation,
   spawntimesecs, wander_distance, currentwaypoint, curHealthPct, MovementType, npcflag,
   unit_flags, unit_flags2, unit_flags3, ScriptName, StringId, VerifiedBuild)
SELECT
  l.guid, l.id, l.map, l.zoneId, l.areaId, l.spawnDifficulties, l.phaseUseFlags, l.PhaseId,
  l.PhaseGroup, l.terrainSwapMap, l.modelid, l.equipment_id, l.position_x, l.position_y,
  l.position_z, l.orientation, l.spawntimesecs, l.wander_distance, l.currentwaypoint,
  l.curHealthPct, l.MovementType, l.npcflag, l.unit_flags, l.unit_flags2, l.unit_flags3,
  l.ScriptName, l.StringId, 0
FROM lorewalker_world.creature l
WHERE l.id < 9100000
  AND NOT (l.position_x = 0 AND l.position_y = 0 AND l.position_z = 0)
  AND EXISTS (SELECT 1 FROM world.creature_template ct WHERE ct.entry = l.id)
  AND NOT EXISTS (SELECT 1 FROM world.creature w WHERE w.guid = l.guid);

-- gameobject (PK: guid)
-- SCHEMA DIFF: world.gameobject has extra columns 'size' (default -1), 'visibility' (default 256)
INSERT IGNORE INTO world.gameobject
  (guid, id, map, zoneId, areaId, spawnDifficulties, phaseUseFlags, PhaseId, PhaseGroup,
   terrainSwapMap, position_x, position_y, position_z, orientation, rotation0, rotation1,
   rotation2, rotation3, spawntimesecs, animprogress, state, ScriptName, StringId, VerifiedBuild)
SELECT
  l.guid, l.id, l.map, l.zoneId, l.areaId, l.spawnDifficulties, l.phaseUseFlags, l.PhaseId,
  l.PhaseGroup, l.terrainSwapMap, l.position_x, l.position_y, l.position_z, l.orientation,
  l.rotation0, l.rotation1, l.rotation2, l.rotation3, l.spawntimesecs, l.animprogress,
  l.state, l.ScriptName, l.StringId, 0
FROM lorewalker_world.gameobject l
WHERE l.id < 9100000
  AND l.guid < 1913720832000
  AND NOT (l.position_x = 0 AND l.position_y = 0 AND l.position_z = 0)
  AND EXISTS (SELECT 1 FROM world.gameobject_template gt WHERE gt.entry = l.id)
  AND NOT EXISTS (SELECT 1 FROM world.gameobject w WHERE w.guid = l.guid);

-- creature_addon (PK: guid | no VB | identical schema)
-- Runs AFTER creature INSERT — EXISTS picks up newly imported creatures
INSERT IGNORE INTO world.creature_addon
SELECT l.* FROM lorewalker_world.creature_addon l
WHERE EXISTS (SELECT 1 FROM world.creature w WHERE w.guid = l.guid);

-- gameobject_addon (PK: guid | no VB | identical schema)
INSERT IGNORE INTO world.gameobject_addon
SELECT l.* FROM lorewalker_world.gameobject_addon l
WHERE EXISTS (SELECT 1 FROM world.gameobject w WHERE w.guid = l.guid);

-- spawn_group (PK: groupId,spawnType,spawnId | no VB | identical schema)
INSERT IGNORE INTO world.spawn_group
SELECT * FROM lorewalker_world.spawn_group;

-- pool_template (PK: entry | no VB | identical schema)
INSERT IGNORE INTO world.pool_template
SELECT * FROM lorewalker_world.pool_template;

-- pool_members (PK: type,spawnId | no VB | identical schema)
INSERT IGNORE INTO world.pool_members
SELECT * FROM lorewalker_world.pool_members;

-- creature_formations (PK: memberGUID | no VB | identical schema)
-- Only import where the member creature exists (including newly imported)
INSERT IGNORE INTO world.creature_formations
SELECT l.* FROM lorewalker_world.creature_formations l
WHERE EXISTS (SELECT 1 FROM world.creature w WHERE w.guid = l.memberGUID);

-- game_event_creature (PK: guid,eventEntry | no VB | identical schema)
INSERT IGNORE INTO world.game_event_creature
SELECT l.* FROM lorewalker_world.game_event_creature l
WHERE EXISTS (SELECT 1 FROM world.creature w WHERE w.guid = l.guid);

-- game_event_gameobject (PK: guid,eventEntry | no VB | identical schema)
INSERT IGNORE INTO world.game_event_gameobject
SELECT l.* FROM lorewalker_world.game_event_gameobject l
WHERE l.guid < 1913720832000
  AND EXISTS (SELECT 1 FROM world.gameobject w WHERE w.guid = l.guid);

COMMIT;
