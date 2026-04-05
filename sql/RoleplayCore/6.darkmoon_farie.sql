USE `world`;
-- ----------------------------
-- Misc fixes
-- ----------------------------

REPLACE INTO `spell_script_names` VALUES (73814, 'spell_darkmoon_citizen_costume');
REPLACE INTO `spell_script_names` VALUES (73810, 'spell_darkmoon_citizen_costume');
REPLACE INTO `spell_script_names` VALUES (73104, 'spell_darkmoon_citizen_costume');
REPLACE INTO `spell_script_names` VALUES (73815, 'spell_darkmoon_citizen_costume');
REPLACE INTO `spell_script_names` VALUES (70764, 'spell_darkmoon_citizen_costume');
REPLACE INTO `spell_script_names` VALUES (71084, 'spell_darkmoon_citizen_costume');
REPLACE INTO `spell_script_names` VALUES (102053, 'spell_cook_crunchy_frog');

UPDATE `creature_template` SET `ScriptName` = 'npc_fire_eater_darkmoon' WHERE `entry` IN (55229, 55230, 55231);
UPDATE `creature_template` SET `ScriptName` = 'npc_fire_juggler_darkmoon' WHERE `entry` IN (55220, 55221, 55222, 55223, 55225, 55226, 55341, 55342);

-- ----------------------------
-- Canon fixes
-- ----------------------------
REPLACE INTO `gossip_menu` VALUES (6575, 7789, 29704);
REPLACE INTO `gossip_menu_option` VALUES (6575, -1683200, 0, 0, 'How do I use the cannon?', 10769, 0, 0, 6574, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 29704);
REPLACE INTO `gossip_menu_option` VALUES (6575, -1683201, 1, 0, 'Launch me! |cFF0008E8(Darkmoon Game Token)|r', 53038, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 29704);
REPLACE INTO `gossip_menu_option` VALUES (6575, -1683202, 2, 0, 'Launch me! |cFF0008E8(Darkmoon Game Token)|r', 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 0);
REPLACE INTO `gossip_menu_option` VALUES (16972, 43062, 0, 0, 'I understand.', 53318, 0, 0, 16970, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 56819);
REPLACE INTO `gossip_menu_option_locale` VALUES (16972, 0, 'ruRU', 'Понятно.', NULL);
REPLACE INTO `npc_text` VALUES (7790, 1, 0, 0, 0, 0, 0, 0, 0, 10770, 0, 0, 0, 0, 0, 0, 0, 17658);
REPLACE INTO `creature_template_gossip` VALUES (15303, 8590, 48676);


UPDATE `creature_template` SET `ScriptName` = 'npc_canon_maxima' WHERE `entry` = 15303;
UPDATE `creature_template` SET `ScriptName` = 'npc_darkmoon_canon_target' WHERE `entry` = 33068;
UPDATE `creature_template` SET `ScriptName` = 'npc_canon_fozlebub' WHERE `entry` = 57850;

REPLACE INTO `spell_script_names` VALUES (102112, 'spell_darkmoon_canon_preparation');

-- ----------------------------
-- DanceBattle fixes
-- ----------------------------
REPLACE INTO `gossip_menu` VALUES (26818, 42769, 50000);
REPLACE INTO `gossip_menu_option` VALUES (26818, 52650, 0, 0, 'Sounds fun. What are the rules?', 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 50000);
REPLACE INTO `gossip_menu_option` VALUES (26818, 52651, 1, 0, 'Ready to dance! |cFF0008E8(Darkmoon Game Token)|r', 0, 0, 0, 26888, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 50000);
REPLACE INTO `gossip_menu_option` VALUES (16972, 43062, 0, 0, 'I understand.', 53318, 0, 0, 16970, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 56819);
REPLACE INTO `gossip_menu_option` VALUES (26888, 53515, 0, 0, 'Just something fun and easygoing!', 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 0);
REPLACE INTO `gossip_menu_option` VALUES (26888, 53516, 1, 0, 'I\'d like a little challenge!', 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 0);
REPLACE INTO `gossip_menu_option` VALUES (26888, 53517, 2, 0, 'Show me your moves, don\'t hold back!', 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 0);
REPLACE INTO `gossip_menu_option` VALUES (26888, 53518, 3, 0, 'I want to ask something else.', 0, 0, 0, 26818, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 0);
REPLACE INTO `gossip_menu_option_locale` VALUES (26818, 0, 'ruRU', 'Звучит заманчиво. А какие правила?', NULL);
REPLACE INTO `gossip_menu_option_locale` VALUES (26818, 1, 'ruRU', 'Потанцуем! |cFF0008E8(Игровой жетон Новолуния)|r', NULL);
REPLACE INTO `gossip_menu_option_locale` VALUES (16972, 0, 'ruRU', 'Понятно.', NULL);
REPLACE INTO `gossip_menu_option_locale` VALUES (26888, 0, 'ruRU', 'Что-то попроще. Я просто хочу повеселиться!', NULL);
REPLACE INTO `gossip_menu_option_locale` VALUES (26888, 1, 'ruRU', 'Сложный, но не слишком!', NULL);
REPLACE INTO `gossip_menu_option_locale` VALUES (26888, 2, 'ruRU', 'Пусть не сдерживается и покажет все свои движения!', NULL);
REPLACE INTO `gossip_menu_option_locale` VALUES (26888, 3, 'ruRU', 'Я хочу спросить кое-что еще.', NULL);
REPLACE INTO `creature_template_gossip` VALUES (181097, 26818, 50000);
REPLACE INTO `npc_text` VALUES (42770, 1, 0, 0, 0, 0, 0, 0, 0, 207528, 0, 0, 0, 0, 0, 0, 0, 42979);
REPLACE INTO `npc_text` VALUES (42798, 1, 0, 0, 0, 0, 0, 0, 0, 208986, 0, 0, 0, 0, 0, 0, 0, 42979);

UPDATE `creature_template` SET `ScriptName` = 'npc_dance_battle_simon_sezdans' WHERE `entry` = 181097;

REPLACE INTO `scene_template` (`SceneId`, `Flags`, `ScriptPackageID`, `Encrypted`, `ScriptName`) VALUES (2709, 17, 3193, 0, 'scene_darkmoon_dance_battle');

-- ----------------------------
-- Firebird Challenge fixes
-- ----------------------------

REPLACE INTO `areatrigger_create_properties` (`Id`,`IsCustom`,`AreaTriggerId`,`IsAreatriggerCustom`,`Flags`,`MoveCurveId`,`ScaleCurveId`,`MorphCurveId`,`FacingCurveId`,`AnimId`,`AnimKitId`,`DecalPropertiesId`,`SpellForVisuals`,`TimeToTargetScale`,`Speed`,`SpeedIsTime`,`Shape`,`ShapeData0`,`ShapeData1`,`ShapeData2`,`ShapeData3`,`ShapeData4`,`ShapeData5`,`ShapeData6`,`ShapeData7`,`ScriptName`,`VerifiedBuild`) VALUES (3069, 0, 7712, 0, 0, 0, 0, 0, 0, -1, 0, 0, NULL, 0, 1, 0, 0, 5, 5, 0, 0, 0, 0, 0, 0, 'at_darkmoon_firebird_ring', 44061);
REPLACE INTO `areatrigger_template` VALUES (7712, 0, 0, 0, 0, 56819);

REPLACE INTO `spell_script_names` VALUES (170819, 'spell_darkmoon_firebird_challenge');
REPLACE INTO `spell_script_names` VALUES (170820, 'spell_darkmoon_firebird_challenge_check_trigger');

REPLACE INTO `creature_template_gossip` VALUES (85546, 16970, 50000);
REPLACE INTO `gossip_menu` VALUES (16970, 24702, 50000);
REPLACE INTO `gossip_menu_option` VALUES (16970, 43059, 0, 0, 'How do I play Firebird\'s Challenge?', 87053, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 56819);
REPLACE INTO `gossip_menu_option` VALUES (16970, 43060, 1, 0, 'Ready to fly! |cFF0008E8(Darkmoon Game Token)|r', 87062, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 56819);
REPLACE INTO `gossip_menu_option` VALUES (16972, 43062, 0, 0, 'I understand.', 53318, 0, 0, 16970, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 56819);
REPLACE INTO `gossip_menu_option_locale` VALUES (16972, 0, 'ruRU', 'Понятно.', NULL);
REPLACE INTO `npc_text` VALUES (24704, 1, 0, 0, 0, 0, 0, 0, 0, 87078, 0, 0, 0, 0, 0, 0, 0, 19865);

UPDATE `creature_template` SET `ScriptName` = 'npc_ziggie_sparks' WHERE `entry` = 85546;

-- ----------------------------
-- Ring Toss fixes
-- ----------------------------

-- ----------------------------
-- Shooting Gallety fixes
-- ----------------------------


-- ----------------------------
-- Tonk Battle fixes
-- ----------------------------

REPLACE INTO `spell_script_names` VALUES (101838, 'spell_gen_repair_damaged_tonk');

-- ----------------------------
-- Wrack Gnoll fixes
-- ----------------------------