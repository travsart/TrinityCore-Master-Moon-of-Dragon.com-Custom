UPDATE `creature_template_addon` SET `auras`=0 WHERE `entry`=42405; -- Two-Shoed Lou
UPDATE `creature_template` SET `unit_flags`=512 WHERE `entry`=42387; -- Thug
UPDATE `creature_template` SET `unit_flags`=512 WHERE `entry`=42575; -- Hope Saldean
-- UPDATE `creature_template` SET `faction`=35 WHERE `entry`=42750; -- Marshal Gryan Stoutmantle
UPDATE `creature_template` SET `faction`=35 WHERE `entry`=42744; -- Lieutenant Horatio Laine
UPDATE `creature_template` SET `unit_flags`=512, `unit_flags2`=2048 WHERE `entry`=42635; -- Ripsnarl
UPDATE `creature_template` SET `unit_flags`=0 WHERE `entry`=42391; -- West Plains Drifter
UPDATE `creature_template` SET `unit_flags`=0 WHERE `entry`=42386; -- Homeless Stormwind Citizen
UPDATE `creature_template` SET `unit_flags`=0 WHERE `entry`=42384; -- Homeless Stormwind Citizen
UPDATE `creature_template` SET `unit_flags`=0 WHERE `entry`=42383; -- Transient
UPDATE `creature_template` SET `flags_extra`=2 WHERE `entry`=878; -- Scout Galiaan
UPDATE `creature_template` SET `flags_extra`=2 WHERE `entry`=821; -- The Westfall Brigade


UPDATE `creature_template` SET `ScriptName`='npc_horatio' WHERE `entry`=42308; -- Lieutenant Horatio Laine
UPDATE `creature_template` SET `ScriptName`='npc_westplains_drifter' WHERE `entry`=42383; -- Transient
UPDATE `creature_template` SET `ScriptName`='npc_westplains_drifter' WHERE `entry`=42384; -- Homeless Stormwind Citizen
UPDATE `creature_template` SET `ScriptName`='npc_westplains_drifter' WHERE `entry`=42386; -- Homeless Stormwind Citizen
UPDATE `creature_template` SET `ScriptName`='npc_westplains_drifter' WHERE `entry`=42391; -- West Plains Drifter
UPDATE `creature_template` SET `ScriptName`='npc_crate_mine' WHERE `entry`=42500; -- Two-Shoed Lou\'s Old House
UPDATE `creature_template` SET `ScriptName`='npc_thug' WHERE `entry`=42387; -- Thug
UPDATE `creature_template` SET `ScriptName`='npc_shadowy_tower' WHERE `entry`=17234; -- [Unused] Tunneler Visual
UPDATE `creature_template` SET `ScriptName`='npc_shadowy_trigger' WHERE `entry`=43515; -- Moonbrook Player Trigger
UPDATE `creature_template` SET `ScriptName`='npc_rise_br' WHERE `entry`=234; -- Marshal Gryan Stoutmantle
UPDATE `creature_template` SET `ScriptName`='npc_ripsnarl' WHERE `entry`=42635; -- Ripsnarl

 -- [Unused] Tunneler Visual -- Spawn
INSERT IGNORE INTO `creature`(`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`modelid`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`currentwaypoint`,`curHealthPct`,`MovementType`,`npcflag`,`unit_flags`,`unit_flags2`,`unit_flags3`,`ScriptName`,`StringId`,`VerifiedBuild`) VALUES 
(31915,17234,0,40,5289,'0',0,0,0,-1,0,0,-11135.7,545.992,70.3372,0.24347,300,0,0,100,0,0,0,0,0,'',NULL,0), -- [Unused] Tunneler Visual
(31916,43515,0,40,20,'0',0,0,0,-1,0,0,-10954.5,1509,54,0,300,3,0,100,1,0,0,0,0,'',NULL,0); -- Moonbrook Player Trigger

-- Kirk Maxwell -- Companion Vendor
INSERT IGNORE INTO `npc_vendor`(`entry`,`slot`,`item`,`maxcount`,`incrtime`,`ExtendedCost`,`type`,`BonusListIDs`,`PlayerConditionID`,`IgnoreFiltering`,`VerifiedBuild`) VALUES
(10045,0,31760,0,0,0,1,NULL,0,0,0),
(10045,0,44998,0,0,0,1,NULL,0,0,0),
(10045,0,68833,0,0,0,1,NULL,0,0,0),
(10045,0,71076,0,0,0,1,NULL,0,0,0),
(10045,0,72042,0,0,0,1,NULL,0,0,0);