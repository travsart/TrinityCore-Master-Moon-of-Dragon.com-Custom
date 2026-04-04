-- ============================================================================
-- Midnight-era portal room portals + additional board spawns
-- Source: Draconic-WOW database (build 66263), cross-referenced with LoreWalker
-- 2026-03-07
-- ============================================================================
-- Draconic uses 620xxx portal entries (Midnight-era) instead of 323840 (BfA).
-- The 620xxx entries have correct 12.x displayIds, teleport spells, and
-- ContentTuningIds. This script:
--   1. Removes the BfA placeholder portals we inserted in _17_world.sql
--   2. Creates the proper 620xxx gameobject_templates from Draconic
--   3. Spawns 12 Midnight-era portals at Draconic-verified positions
--   4. Adds the decorative portal visual (311875)
--   5. Adds Hero's Call Board entry 206294 spawns (2 locations)
-- ============================================================================

-- ============================================================================
-- SECTION 1: Remove BfA placeholder portal spawns (from _17_world.sql)
-- These are superseded by the proper Midnight 620xxx entries below.
-- ============================================================================
DELETE FROM `gameobject` WHERE `guid` IN (500392, 500393, 500394, 500395, 500396, 500397, 500398);


-- ============================================================================
-- SECTION 2: Create missing 620xxx gameobject_templates
-- All data from Draconic build 66263 (MySQL 9.5.0).
-- Type 22 = GAMEOBJECT_TYPE_SPELL_FOCUS
-- Data0 = focusId (linked spell focus), Data5 = linkedTrapId (teleport spell)
-- We already have: 620463 (Dornogal). Creating the other 11.
-- ============================================================================

INSERT IGNORE INTO `gameobject_template` (`entry`,`type`,`displayId`,`name`,`IconName`,`castBarCaption`,`unk1`,`size`,`Data0`,`Data1`,`Data2`,`Data3`,`Data4`,`Data5`,`Data6`,`Data7`,`Data8`,`Data9`,`Data10`,`Data11`,`Data12`,`Data13`,`Data14`,`Data15`,`Data16`,`Data17`,`Data18`,`Data19`,`Data20`,`Data21`,`Data22`,`Data23`,`Data24`,`Data25`,`Data26`,`Data27`,`Data28`,`Data29`,`Data30`,`Data31`,`Data32`,`Data33`,`Data34`,`ContentTuningId`,`RequiredLevel`,`AIName`,`ScriptName`,`VerifiedBuild`) VALUES
-- Portal to Caverns of Time
(620455, 22, 57430, 'Portal to Caverns of Time', '', '', '', 1, 466601, -1, 0,0,0, 131923, 1, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0, 864, 1, '', '', 64978),
-- Portal to Valdrakken
(620458, 22, 77931, 'Portal to Valdrakken', '', '', '', 1, 393590, -1, 0,0,0, 103378, 1, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0, 864, 1, '', '', 64978),
-- Portal to Oribos
(620464, 22, 65524, 'Portal to Oribos', '', '', '', 1, 329132, -1, 0,0,0, 85101, 1, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0, 2651, 10, '', '', 64978),
-- Portal to Boralus
(620465, 22, 55652, 'Portal to Boralus', '', '', '', 1, 280222, -1, 0,0,0, 67398, 1, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0, 2651, 10, '', '', 64978),
-- Portal to Jade Forest
(620467, 22, 55651, 'Portal to Jade Forest', '', '', '', 1, 130702, -1, 0,0,0, 67395, 1, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0, 57, 10, '', '', 64978),
-- Portal to Shattrath
(620472, 22, 55653, 'Portal to Shattrath', '', '', '', 1, 33690, -1, 0,0,0, 6289, 1, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0, 57, 10, '', '', 64978),
-- Portal to the Exodar
(620473, 22, 55650, 'Portal to the Exodar', '', '', '', 1, 121850, -1, 0,0,0, 127988, 1, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0, 57, 10, '', '', 64978),
-- Portal to Dalaran, Crystalsong Forest
(620475, 22, 55649, 'Portal to Dalaran, Crystalsong Forest', '', '', '', 1, 53140, -1, 0,0,0, 76876, 1, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0, 826, 10, '', '', 64978),
-- Portal to Bel'ameth
(620476, 22, 87382, 'Portal to Bel''ameth', '', '', '', 1, 433911, -1, 0,0,0, 132203, 1, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0, 0, 0, '', '', 64978),
-- Portal to Azsuna
(620477, 22, 55648, 'Portal to Azsuna', '', '', '', 1, 296901, -1, 0,0,0, 67396, 1, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0, 331, 10, '', '', 64978),
-- Portal to Stormshield, Ashran
(620479, 22, 55647, 'Portal to Stormshield, Ashran', '', '', '', 1, 225748, -1, 0,0,0, 67397, 1, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0, 864, 1, '', '', 64978);


-- ============================================================================
-- SECTION 3: Spawn Midnight portal room portals (Draconic-verified positions)
-- All in map=0, zone=1519, area=10523 (Stormwind Portal Room).
-- Positions, orientations, and rotations exactly from Draconic build 66263.
-- Portals we already have (620463 Dornogal, 543407 Founder's Point) are KEPT.
-- ============================================================================

-- Portal to Valdrakken (Dragonflight)
INSERT IGNORE INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`StringId`,`VerifiedBuild`,`size`,`visibility`)
VALUES (10001980, 620458, 0, 1519, 10523, '0', 0, 0, 0, -1, -9078.02, 873.425, 68.1833, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 0, -1, 256);

-- Portal to Oribos (Shadowlands)
INSERT IGNORE INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`StringId`,`VerifiedBuild`,`size`,`visibility`)
VALUES (10001988, 620464, 0, 1519, 10523, '0', 0, 0, 0, -1, -9095.66, 896.311, 68.6197, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 0, -1, 256);

-- Portal to Jade Forest (MoP)
INSERT IGNORE INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`StringId`,`VerifiedBuild`,`size`,`visibility`)
VALUES (10001978, 620467, 0, 1519, 10523, '0', 0, 0, 0, -1, -9005.21, 928.899, 68.0438, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 0, -1, 256);

-- Portal to Shattrath (BC)
INSERT IGNORE INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`StringId`,`VerifiedBuild`,`size`,`visibility`)
VALUES (10001989, 620472, 0, 1519, 10523, '0', 0, 0, 0, -1, -8988.79, 942.814, 68.0437, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 0, -1, 256);

-- Portal to Dalaran, Crystalsong Forest (WotLK)
INSERT IGNORE INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`StringId`,`VerifiedBuild`,`size`,`visibility`)
VALUES (10001982, 620475, 0, 1519, 10523, '0', 0, 0, 0, -1, -9023.37, 952.547, 68.336, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 0, -1, 256);

-- Portal to Caverns of Time (Classic/BC)
INSERT IGNORE INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`StringId`,`VerifiedBuild`,`size`,`visibility`)
VALUES (10001983, 620455, 0, 1519, 10523, '0', 0, 0, 0, -1, -8984.52, 963.316, 68.6964, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 0, -1, 256);

-- Portal to the Exodar (BC)
INSERT IGNORE INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`StringId`,`VerifiedBuild`,`size`,`visibility`)
VALUES (10001981, 620473, 0, 1519, 10523, '0', 0, 0, 0, -1, -9006.16, 965.464, 68.2215, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 0, -1, 256);

-- Portal to Bel'ameth (Dragonflight 10.2.5+)
INSERT IGNORE INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`StringId`,`VerifiedBuild`,`size`,`visibility`)
VALUES (10001990, 620476, 0, 1519, 10523, '0', 0, 0, 0, -1, -9126.46, 971.29, 73.6619, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 0, -1, 256);

-- Portal to Azsuna (Legion)
INSERT IGNORE INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`StringId`,`VerifiedBuild`,`size`,`visibility`)
VALUES (10001979, 620477, 0, 1519, 10523, '0', 0, 0, 0, -1, -9054.06, 992.01, 73.5924, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 0, -1, 256);

-- Portal to Stormshield, Ashran (WoD)
INSERT IGNORE INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`StringId`,`VerifiedBuild`,`size`,`visibility`)
VALUES (10001987, 620479, 0, 1519, 10523, '0', 0, 0, 0, -1, -9036.3, 1004.39, 73.9377, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 0, -1, 256);

-- Portal to Boralus (BfA)
INSERT IGNORE INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`StringId`,`VerifiedBuild`,`size`,`visibility`)
VALUES (10001985, 620465, 0, 1519, 10523, '0', 0, 0, 0, -1, -9070.33, 1012.99, 73.5922, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 0, -1, 256);


-- ============================================================================
-- SECTION 4: Decorative portal visual (311875)
-- We have the template already. Draconic has 1 spawn at the portal room entrance.
-- ============================================================================

INSERT IGNORE INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`StringId`,`VerifiedBuild`,`size`,`visibility`)
VALUES (500399, 311875, 0, 1519, 10523, '0', 0, 0, 0, -1, -8999.15, 863.503, 65.7741, 5.36689, 0, 0, -0.442288, 0.896873, 120, 255, 1, '', NULL, 0, -1, 256);


-- ============================================================================
-- SECTION 5: Hero's Call Board entry 206294 — 2 additional Stormwind spawns
-- We already have entry 206294 in gameobject_template. Draconic has 2 spawns
-- at Old Town and Mage Quarter (slightly different positions from the 206111
-- and 281339 boards already in our DB — these co-exist as separate boards).
-- ============================================================================

-- 206294 Board: Old Town (areaId ~5148)
INSERT IGNORE INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`StringId`,`VerifiedBuild`,`size`,`visibility`)
VALUES (301900, 206294, 0, 1519, 5148, '0', 0, 0, 0, -1, -8823.33, 636.332, 94.5673, 2.15758, 0, 0, -0.881386, -0.472397, 120, 0, 1, '', NULL, 0, -1, 256);

-- 206294 Board: Mage Quarter (areaId ~5150)
INSERT IGNORE INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`StringId`,`VerifiedBuild`,`size`,`visibility`)
VALUES (301901, 206294, 0, 1519, 5150, '0', 0, 0, 0, -1, -8340.34, 643, 96.1393, 0.734935, 0, 0, -0.359253, -0.93324, 120, 0, 1, '', NULL, 0, -1, 256);


-- ============================================================================
-- Summary:
--   7 BfA placeholder portals DELETED (guids 500392-500398, from _17_world.sql)
--  11 Midnight portal templates CREATED (620455-620479)
--  11 Midnight portal spawns CREATED (guids 10001978-10001990, excl 10001984/86)
--   1 decorative portal visual CREATED (guid 500399, entry 311875)
--   2 Hero's Call Board 206294 spawns CREATED (guids 301900-301901)
--
-- Stormwind Portal Room now has 14 functional portals:
--   Hellfire (195141), Dornogal (620463), Founder's Point (543407),
--   Valdrakken (620458), Oribos (620464), Boralus (620465),
--   Jade Forest (620467), Shattrath (620472), Exodar (620473),
--   Dalaran CS (620475), Bel'ameth (620476), Azsuna (620477),
--   Stormshield (620479), Caverns of Time (620455)
-- ============================================================================
