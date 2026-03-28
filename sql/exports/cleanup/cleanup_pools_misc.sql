-- World DB cleanup: pools, events, vendors, GO orphans, spawn groups
-- Generated 2026-02-27
USE `world`;

SET innodb_lock_wait_timeout=120;

-- =============================================================================
-- Step 1: Broken pool chains — pool_members type=2 referencing non-existent pool_template
-- Cascading: must iterate until stable since removing pool_templates creates new orphans
-- =============================================================================

-- Round 1
DELETE FROM pool_members WHERE type=2 AND spawnId NOT IN (SELECT entry FROM (SELECT entry FROM pool_template) AS sub);
DELETE FROM pool_template WHERE entry NOT IN (SELECT poolSpawnId FROM (SELECT DISTINCT poolSpawnId FROM pool_members) AS sub);

-- Round 2 (cascade)
DELETE FROM pool_members WHERE type=2 AND spawnId NOT IN (SELECT entry FROM (SELECT entry FROM pool_template) AS sub);
DELETE FROM pool_template WHERE entry NOT IN (SELECT poolSpawnId FROM (SELECT DISTINCT poolSpawnId FROM pool_members) AS sub);

-- Round 3 (stabilize)
DELETE FROM pool_members WHERE type=2 AND spawnId NOT IN (SELECT entry FROM (SELECT entry FROM pool_template) AS sub);
DELETE FROM pool_template WHERE entry NOT IN (SELECT poolSpawnId FROM (SELECT DISTINCT poolSpawnId FROM pool_members) AS sub);

-- =============================================================================
-- Step 3: Event orphans — game_event_creature/gameobject referencing non-existent events
-- =============================================================================

DELETE FROM game_event_creature WHERE eventEntry NOT IN (SELECT eventEntry FROM (SELECT DISTINCT eventEntry FROM game_event) AS sub);
DELETE FROM game_event_gameobject WHERE eventEntry NOT IN (SELECT eventEntry FROM (SELECT DISTINCT eventEntry FROM game_event) AS sub);

-- =============================================================================
-- Step 5: Broken conversations (report only — do not delete)
-- conversation_templates with missing FirstLineId: 11685, 12127, 12128, 14432, 14437, 17836
-- =============================================================================

-- =============================================================================
-- Step 6: Vendor cleanup — corrupted item entries
-- =============================================================================

DELETE FROM npc_vendor WHERE item = 1963229184;
DELETE FROM npc_vendor WHERE item < 0;

-- =============================================================================
-- Step 8: GO template addon orphans
-- =============================================================================

DELETE FROM gameobject_template_addon WHERE entry NOT IN (SELECT entry FROM (SELECT entry FROM gameobject_template) AS sub);

-- =============================================================================
-- Step 9: GO addon orphans (non-existent gameobject GUIDs)
-- =============================================================================

DELETE FROM gameobject_addon WHERE guid NOT IN (SELECT guid FROM (SELECT guid FROM gameobject) AS sub);

-- =============================================================================
-- Step 10: Spawn group creature orphans
-- =============================================================================

DELETE FROM spawn_group WHERE spawnType=0 AND spawnId NOT IN (SELECT guid FROM (SELECT guid FROM creature) AS sub);
