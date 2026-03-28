-- Auth DB cleanup: RBAC default permission assignments for custom commands
-- Generated 2026-02-27
USE `auth`;
SET innodb_lock_wait_timeout=120;

-- =============================================================================
-- Step 4: Assign custom permissions to appropriate security levels
-- Player RP commands (secId=0): .barber, .castgroup, .typing, .disp *, .comp
-- GM commands (secId=2): .customnpc *, .gobject set scale, .npc set scale, .castscene, .gob visible
-- =============================================================================

INSERT IGNORE INTO rbac_default_permissions (secId, permissionId, realmId) VALUES
-- Player commands (secId=0)
(0, 1002, -1),  -- .barber
(0, 1003, -1),  -- .castgroup
(0, 1005, -1),  -- .typing on
(0, 1006, -1),  -- .typing off
(0, 1008, -1),  -- .disp
(0, 1009, -1),  -- .disp head
(0, 1010, -1),  -- .disp shoulders
(0, 1011, -1),  -- .disp shirt
(0, 1012, -1),  -- .disp chest
(0, 1013, -1),  -- .disp waist
(0, 1014, -1),  -- .disp legs
(0, 1015, -1),  -- .disp feet
(0, 1016, -1),  -- .disp wrists
(0, 1017, -1),  -- .disp hands
(0, 1018, -1),  -- .disp back
(0, 1019, -1),  -- .disp tabard
(0, 1020, -1),  -- .disp mainhand
(0, 1021, -1),  -- .disp offhand
(0, 3008, -1),  -- .comp
-- GM commands (secId=2)
(2, 1004, -1),  -- .castscene
(2, 1360, -1),  -- .customnpc set displayid
(2, 1361, -1),  -- .customnpc set guild
(2, 1362, -1),  -- .customnpc set rank
(2, 1363, -1),  -- .customnpc set scale
(2, 1364, -1),  -- .customnpc set tameable
(2, 1365, -1),  -- .customnpc remove variation
(2, 1398, -1),  -- .gobject set scale
(2, 1589, -1),  -- .npc set scale
(2, 2101, -1),  -- .customnpc create
(2, 2102, -1),  -- .customnpc spawn
(2, 2103, -1),  -- .customnpc set displayname
(2, 2104, -1),  -- .customnpc set face
(2, 2105, -1),  -- .customnpc set gender
(2, 2106, -1),  -- .customnpc set race
(2, 2107, -1),  -- .customnpc set subname
(2, 2108, -1),  -- .customnpc equip armor
(2, 2109, -1),  -- .customnpc equip left
(2, 2110, -1),  -- .customnpc equip ranged
(2, 2111, -1),  -- .customnpc equip right
(2, 2112, -1),  -- .customnpc delete
(2, 3004, -1);  -- .gob visible
