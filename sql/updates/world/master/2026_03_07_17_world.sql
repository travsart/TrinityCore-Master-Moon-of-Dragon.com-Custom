-- ============================================================================
-- Spawn missing BfA portal room portals + fix Hero's Call Board Z/rotation
-- Source: LoreWalkerTDB world.sql (build 65893) cross-referenced with Wowhead
-- 2026-03-07
-- ============================================================================

-- ============================================================================
-- SECTION 1: BfA Portal Room portals (entries 323840-323846)
-- These 7 portals have templates in gameobject_template but ZERO spawns.
-- LoreWalker has exact spawn data (guids 500392-500398) which was never
-- imported during the LW bulk import (which focused on SmartAI/loot/vendors).
-- All portals are in Stormwind (map 0), areaId 10523 (Stormwind Portal Room).
-- Z values split: ~68 for main floor portals, ~74 for upper platform portals.
-- ============================================================================

-- Portal to Stormshield, Ashran (WoD) — upper platform
INSERT IGNORE INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`StringId`,`VerifiedBuild`,`size`,`visibility`)
VALUES (500392, 323840, 0, 1519, 10523, '0', 0, 0, 0, -1, -9035.82, 1004.43, 73.9381, 2.20369, 0, 0, 0.896873, 0.442289, 120, 255, 1, '', NULL, 0, -1, 256);

-- Portal to Azsuna (Legion)
INSERT IGNORE INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`StringId`,`VerifiedBuild`,`size`,`visibility`)
VALUES (500393, 323841, 0, 1519, 10523, '0', 0, 0, 0, -1, -9053.42, 991.359, 73.9737, 5.36689, 0, 0, -0.442288, 0.896873, 120, 255, 1, '', NULL, 0, -1, 256);

-- Portal to Dalaran, Crystalsong Forest (WotLK) — main floor
INSERT IGNORE INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`StringId`,`VerifiedBuild`,`size`,`visibility`)
VALUES (500394, 323842, 0, 1519, 10523, '0', 0, 0, 0, -1, -9023.58, 952.492, 68.3227, 5.36689, 0, 0, -0.442288, 0.896873, 120, 255, 1, '', NULL, 0, -1, 256);

-- Portal to the Exodar (BC) — main floor
INSERT IGNORE INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`StringId`,`VerifiedBuild`,`size`,`visibility`)
VALUES (500395, 323843, 0, 1519, 10523, '0', 0, 0, 0, -1, -9006.19, 965.365, 68.2277, 5.36689, 0, 0, -0.442288, 0.896873, 120, 255, 1, '', NULL, 0, -1, 256);

-- Portal to Jade Forest (MoP) — main floor
INSERT IGNORE INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`StringId`,`VerifiedBuild`,`size`,`visibility`)
VALUES (500396, 323844, 0, 1519, 10523, '0', 0, 0, 0, -1, -9005.13, 928.612, 68.1791, 2.22529, 0, 0, 0.896873, 0.442289, 120, 255, 1, '', NULL, 0, -1, 256);

-- Portal to Boralus (BfA) — upper platform
INSERT IGNORE INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`StringId`,`VerifiedBuild`,`size`,`visibility`)
VALUES (500397, 323845, 0, 1519, 10523, '0', 0, 0, 0, -1, -9070.82, 1014.08, 73.5917, 0.654497, 0, 0, 0.321439, 0.94693, 120, 255, 1, '', NULL, 0, -1, 256);

-- Portal to Shattrath (BC) — main floor
INSERT IGNORE INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`StringId`,`VerifiedBuild`,`size`,`visibility`)
VALUES (500398, 323846, 0, 1519, 10523, '0', 0, 0, 0, -1, -8988.78, 942.689, 68.3227, 2.22529, 0, 0, 0.896873, 0.442289, 120, 255, 1, '', NULL, 0, -1, 256);


-- ============================================================================
-- SECTION 2: Hero's Call Board (entry 206111) — fix Z, orientation, rotations
-- All 5 spawns have position_z=0, orientation=0, all rotations zeroed.
-- Board 3 Z confirmed from LoreWalker (entry 206111 at same location = 94.36).
-- Board 4 Z confirmed from LoreWalker (entry 281339 at same location = 95.86).
-- Boards 1,2,5 Z estimated from nearest spawned creatures within 20 yards.
-- Orientations: LW-sourced where available, estimated from nearby NPCs otherwise.
-- ============================================================================

-- Board 1: Harbor/Cathedral area — Z from nearby Sea Gull (25.20)
UPDATE `gameobject` SET
    `position_z` = 25.2043,
    `orientation` = 3.78737,
    `rotation2` = -0.948323,
    `rotation3` = 0.317306,
    `zoneId` = 1519
WHERE `guid` = 60458910 AND `id` = 206111 AND `position_z` = 0;

-- Board 2: Dwarven District — Z from nearby Caretaker Folsom (103.32)
UPDATE `gameobject` SET
    `position_z` = 103.321,
    `orientation` = 4.86191,
    `rotation2` = -0.652318,
    `rotation3` = 0.757946,
    `zoneId` = 1519
WHERE `guid` = 60458911 AND `id` = 206111 AND `position_z` = 0;

-- Board 3: Old Town — Z from LoreWalker exact match (94.36) [CONFIRMED]
UPDATE `gameobject` SET
    `position_z` = 94.3614,
    `orientation` = 4.86191,
    `rotation2` = -0.652318,
    `rotation3` = 0.757946,
    `zoneId` = 1519
WHERE `guid` = 60458912 AND `id` = 206111 AND `position_z` = 0;

-- Board 4: Mage Quarter — Z from LoreWalker entry 281339 at same location (95.86) [CONFIRMED]
UPDATE `gameobject` SET
    `position_z` = 95.8576,
    `orientation` = 3.78737,
    `rotation2` = -0.948323,
    `rotation3` = 0.317306,
    `zoneId` = 1519
WHERE `guid` = 60458913 AND `id` = 206111 AND `position_z` = 0;

-- Board 5: Dwarven District (2nd) — Z from nearby Rat NPC (97.87)
UPDATE `gameobject` SET
    `position_z` = 97.8661,
    `orientation` = 4.86191,
    `rotation2` = -0.652318,
    `rotation3` = 0.757946,
    `zoneId` = 1519
WHERE `guid` = 60458914 AND `id` = 206111 AND `position_z` = 0;


-- ============================================================================
-- SECTION 3: BfA Hero's Call Board (entry 281339) — 2 spawns from LoreWalker
-- Modern replacement board with zero spawns in our DB.
-- Both positions confirmed by LoreWalker (guids 220456, 301205).
-- ============================================================================

-- BfA Board: Mage Quarter (areaId 5150)
INSERT IGNORE INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`StringId`,`VerifiedBuild`,`size`,`visibility`)
VALUES (220456, 281339, 0, 1519, 5150, '0', 0, 0, 0, -1, -8341.77, 641.757, 95.8576, 3.78737, 0, 0, -0.948323, 0.317306, 120, 0, 1, '', NULL, 0, -1, 256);

-- BfA Board: Old Town (areaId 5148)
INSERT IGNORE INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`StringId`,`VerifiedBuild`,`size`,`visibility`)
VALUES (301205, 281339, 0, 1519, 5148, '0', 0, 0, 0, -1, -8823.85, 630.573, 94.3177, 3.83973, 0, 0, -0.939692, 0.34202, 120, 0, 1, '', NULL, 0, -1, 256);


-- ============================================================================
-- Summary:
--   7 BfA portal room portals INSERTED (guids 500392-500398)
--   5 Hero's Call Boards UPDATED (Z, orientation, rotation, zoneId)
--   2 BfA Hero's Call Boards INSERTED (guids 220456, 301205)
-- ============================================================================
