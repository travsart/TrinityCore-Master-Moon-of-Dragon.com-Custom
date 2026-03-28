-- ============================================================================
-- Cleanup: SmartAI, scripting, pathing, and formation orphans
-- Generated: 2026-02-27
-- ============================================================================
USE `world`;
SET innodb_lock_wait_timeout = 120;

-- ---------------------------------------------------------------------------
-- 1. Invalid SmartAI action_types (action_type > 159, beyond SMART_ACTION_END)
--    Incompatible LW import: action_types 204, 205, 206, 209, 213, 218
-- ---------------------------------------------------------------------------
SELECT 'Task 1 BEFORE' AS status, COUNT(*) AS cnt
  FROM smart_scripts WHERE action_type > 159;

DELETE FROM smart_scripts
 WHERE action_type > 159;

SELECT 'Task 1 AFTER' AS status, COUNT(*) AS cnt
  FROM smart_scripts WHERE action_type > 159;

-- ---------------------------------------------------------------------------
-- 2. Orphan creature_formations (memberGUID or leaderGUID not in creature)
-- ---------------------------------------------------------------------------
SELECT 'Task 2 BEFORE' AS status, COUNT(*) AS cnt
  FROM creature_formations
 WHERE memberGUID NOT IN (SELECT guid FROM creature)
    OR leaderGUID NOT IN (SELECT guid FROM creature);

DELETE FROM creature_formations
 WHERE memberGUID NOT IN (SELECT g.guid FROM (SELECT guid FROM creature) AS g)
    OR leaderGUID NOT IN (SELECT g.guid FROM (SELECT guid FROM creature) AS g);

SELECT 'Task 2 AFTER' AS status, COUNT(*) AS cnt
  FROM creature_formations
 WHERE memberGUID NOT IN (SELECT guid FROM creature)
    OR leaderGUID NOT IN (SELECT guid FROM creature);

-- ---------------------------------------------------------------------------
-- 3. Orphan creature_template_movement (CreatureId not in creature_template)
-- ---------------------------------------------------------------------------
SELECT 'Task 3 BEFORE' AS status, COUNT(*) AS cnt
  FROM creature_template_movement
 WHERE CreatureId NOT IN (SELECT entry FROM creature_template);

DELETE FROM creature_template_movement
 WHERE CreatureId NOT IN (SELECT e.entry FROM (SELECT entry FROM creature_template) AS e);

SELECT 'Task 3 AFTER' AS status, COUNT(*) AS cnt
  FROM creature_template_movement
 WHERE CreatureId NOT IN (SELECT entry FROM creature_template);

-- ---------------------------------------------------------------------------
-- 4. SmartAI creature orphans (source_type=0, positive entryorguid,
--    referencing non-existent creature_template entries)
-- ---------------------------------------------------------------------------
SELECT 'Task 4 BEFORE' AS status, COUNT(*) AS cnt
  FROM smart_scripts
 WHERE source_type = 0
   AND entryorguid > 0
   AND entryorguid NOT IN (SELECT entry FROM creature_template);

DELETE FROM smart_scripts
 WHERE source_type = 0
   AND entryorguid > 0
   AND entryorguid NOT IN (SELECT e.entry FROM (SELECT entry FROM creature_template) AS e);

SELECT 'Task 4 AFTER' AS status, COUNT(*) AS cnt
  FROM smart_scripts
 WHERE source_type = 0
   AND entryorguid > 0
   AND entryorguid NOT IN (SELECT entry FROM creature_template);

-- ---------------------------------------------------------------------------
-- 5. SmartAI GO orphans (source_type=1, positive entryorguid,
--    referencing non-existent gameobject_template entries)
-- ---------------------------------------------------------------------------
SELECT 'Task 5 BEFORE' AS status, COUNT(*) AS cnt
  FROM smart_scripts
 WHERE source_type = 1
   AND entryorguid > 0
   AND entryorguid NOT IN (SELECT entry FROM gameobject_template);

DELETE FROM smart_scripts
 WHERE source_type = 1
   AND entryorguid > 0
   AND entryorguid NOT IN (SELECT e.entry FROM (SELECT entry FROM gameobject_template) AS e);

SELECT 'Task 5 AFTER' AS status, COUNT(*) AS cnt
  FROM smart_scripts
 WHERE source_type = 1
   AND entryorguid > 0
   AND entryorguid NOT IN (SELECT entry FROM gameobject_template);

-- ---------------------------------------------------------------------------
-- 6. SmartAI invalid target_type=80 (entryorguid=219950)
-- ---------------------------------------------------------------------------
SELECT 'Task 6 BEFORE' AS status, COUNT(*) AS cnt
  FROM smart_scripts WHERE target_type = 80;

DELETE FROM smart_scripts
 WHERE target_type = 80;

SELECT 'Task 6 AFTER' AS status, COUNT(*) AS cnt
  FROM smart_scripts WHERE target_type = 80;

-- ---------------------------------------------------------------------------
-- 7. Orphan gossip_menu_option (MenuID not in gossip_menu)
-- ---------------------------------------------------------------------------
SELECT 'Task 7 BEFORE' AS status, COUNT(*) AS cnt
  FROM gossip_menu_option
 WHERE MenuID NOT IN (SELECT MenuID FROM gossip_menu);

DELETE FROM gossip_menu_option
 WHERE MenuID NOT IN (SELECT m.MenuID FROM (SELECT MenuID FROM gossip_menu) AS m);

SELECT 'Task 7 AFTER' AS status, COUNT(*) AS cnt
  FROM gossip_menu_option
 WHERE MenuID NOT IN (SELECT MenuID FROM gossip_menu);

-- ---------------------------------------------------------------------------
-- 8. Orphan gossip conditions (SourceType=15, referencing non-existent
--    gossip_menu_option rows)
-- ---------------------------------------------------------------------------
SELECT 'Task 8 BEFORE' AS status, COUNT(*) AS cnt
  FROM conditions
 WHERE SourceTypeOrReferenceId = 15
   AND NOT EXISTS (
       SELECT 1 FROM gossip_menu_option gmo
        WHERE gmo.MenuID = conditions.SourceGroup
          AND gmo.OptionID = conditions.SourceEntry
   );

DELETE FROM conditions
 WHERE SourceTypeOrReferenceId = 15
   AND NOT EXISTS (
       SELECT 1 FROM gossip_menu_option gmo
        WHERE gmo.MenuID = conditions.SourceGroup
          AND gmo.OptionID = conditions.SourceEntry
   );

SELECT 'Task 8 AFTER' AS status, COUNT(*) AS cnt
  FROM conditions
 WHERE SourceTypeOrReferenceId = 15
   AND NOT EXISTS (
       SELECT 1 FROM gossip_menu_option gmo
        WHERE gmo.MenuID = conditions.SourceGroup
          AND gmo.OptionID = conditions.SourceEntry
   );
