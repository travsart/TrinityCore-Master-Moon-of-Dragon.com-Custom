-- =====================================================
-- Combined SQL generated on 2026-04-05 20:56:53Z
-- Sources:
--  0.custom_tables.sql
--  1.auth.sql
--  2.hotfixes.sql
--  3.character_db.sql
--  4.characters.sql
--  5.chromie_time.sql
--  6.darkmoon_farie.sql
-- =====================================================\n
-- ----- Begin file: 0.custom_tables.sql -----
-- ============================================================================
-- VoxCore Custom Tables — Consolidated DDL
-- ============================================================================
-- Run this after any fresh TDB import to recreate all VoxCore-custom tables.
-- Order: auth → hotfixes → world → characters → roleplay
--
-- Usage:
--   mysql -u root -padmin < sql/RoleplayCore/custom_tables.sql
--
-- NOTE: This file only creates TABLE STRUCTURE. For seed data, RBAC perms,
-- and hotfix table schemas, run the full numbered setup files in order.
-- ============================================================================

-- ============================================================================
-- AUTH DATABASE
-- ============================================================================
USE auth;

CREATE TABLE IF NOT EXISTS `account_warband_groups` (
  `id` bigint(20) unsigned NOT NULL,
  `accountId` int(10) unsigned NOT NULL,
  `realmId` int(10) unsigned NOT NULL DEFAULT '1',
  `orderIndex` tinyint(3) unsigned NOT NULL,
  `name` varchar(257) NOT NULL,
  `warbandSceneId` int(10) unsigned NOT NULL DEFAULT '0',
  `flags` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `idx_account_realm` (`accountId`, `realmId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `account_warband_group_members` (
  `groupId` bigint(20) unsigned NOT NULL,
  `characterGuid` bigint(20) unsigned NOT NULL,
  `placementId` int(10) unsigned NOT NULL,
  `type` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`groupId`, `characterGuid`),
  CONSTRAINT `fk_warband_group` FOREIGN KEY (`groupId`) REFERENCES `account_warband_groups` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `battlenet_transmog_set_favorites` (
  `battlenetAccountId` int unsigned NOT NULL,
  `transmogSetId` int unsigned NOT NULL,
  PRIMARY KEY (`battlenetAccountId`, `transmogSetId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ============================================================================
-- HOTFIXES DATABASE — Schema fixes for columns missing from TC TDB
-- ============================================================================
USE hotfixes;

-- crafting_quality: C++ expects CraftingQualityAtlasSetID but TC TDB omits it
DROP PROCEDURE IF EXISTS fix_crafting_quality;
DELIMITER //
CREATE PROCEDURE fix_crafting_quality()
BEGIN
    IF NOT EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'hotfixes' AND TABLE_NAME = 'crafting_quality' AND COLUMN_NAME = 'CraftingQualityAtlasSetID') THEN
        ALTER TABLE `crafting_quality` ADD COLUMN `CraftingQualityAtlasSetID` int NOT NULL DEFAULT 0 AFTER `QualityTier`;
    END IF;
END //
DELIMITER ;
CALL fix_crafting_quality();
DROP PROCEDURE IF EXISTS fix_crafting_quality;

-- ============================================================================
-- WORLD DATABASE
-- ============================================================================
USE world;

CREATE TABLE IF NOT EXISTS `creature_template_outfits` (
  `entry` int UNSIGNED NOT NULL,
  `npcsoundsid` INT(10) UNSIGNED NOT NULL DEFAULT '0' COMMENT 'entry from NPCSounds.dbc/db2',
  `race` tinyint UNSIGNED NOT NULL DEFAULT 1,
  `class` tinyint UNSIGNED NOT NULL DEFAULT 1,
  `gender` tinyint UNSIGNED NOT NULL DEFAULT 0 COMMENT '0 for male, 1 for female',
  `spellvisualkitid` int NOT NULL DEFAULT 0,
  `customizations` text CHARACTER SET utf8 COLLATE utf8_general_ci NULL,
  `head` bigint NOT NULL DEFAULT 0,
  `head_appearance` int UNSIGNED NOT NULL DEFAULT 0,
  `shoulders` bigint NOT NULL DEFAULT 0,
  `shoulders_appearance` int UNSIGNED NOT NULL DEFAULT 0,
  `body` bigint NOT NULL DEFAULT 0,
  `body_appearance` int UNSIGNED NOT NULL DEFAULT 0,
  `chest` bigint NOT NULL DEFAULT 0,
  `chest_appearance` int UNSIGNED NOT NULL DEFAULT 0,
  `waist` bigint NOT NULL DEFAULT 0,
  `waist_appearance` int UNSIGNED NOT NULL DEFAULT 0,
  `legs` bigint NOT NULL DEFAULT 0,
  `legs_appearance` int UNSIGNED NOT NULL DEFAULT 0,
  `feet` bigint NOT NULL DEFAULT 0,
  `feet_appearance` int UNSIGNED NOT NULL DEFAULT 0,
  `wrists` bigint NOT NULL DEFAULT 0,
  `wrists_appearance` int UNSIGNED NOT NULL DEFAULT 0,
  `hands` bigint NOT NULL DEFAULT 0,
  `hands_appearance` int UNSIGNED NOT NULL DEFAULT 0,
  `back` bigint NOT NULL DEFAULT 0,
  `back_appearance` int UNSIGNED NOT NULL DEFAULT 0,
  `tabard` bigint NOT NULL DEFAULT 0,
  `tabard_appearance` int UNSIGNED NOT NULL DEFAULT 0,
  `guildid` bigint UNSIGNED NOT NULL DEFAULT 0,
  `description` text CHARACTER SET utf8 COLLATE utf8_general_ci NULL,
  PRIMARY KEY (`entry`) USING BTREE
) ENGINE=InnoDB CHARACTER SET=utf8 COLLATE=utf8_general_ci ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `creature_template_outfits_customizations` (
  `chrCustomizationOptionID` int UNSIGNED NOT NULL,
  `chrCustomizationChoiceID` int UNSIGNED NOT NULL DEFAULT 0,
  `outfitID` bigint NOT NULL
) ENGINE=InnoDB CHARACTER SET=utf8 COLLATE=utf8_general_ci ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `scrapping_loot_template` (
  `Entry` int UNSIGNED NOT NULL DEFAULT 0,
  `ItemType` tinyint NOT NULL DEFAULT 0,
  `Item` int UNSIGNED NOT NULL DEFAULT 0,
  `Chance` float NOT NULL DEFAULT 100,
  `QuestRequired` tinyint(1) NOT NULL DEFAULT 0,
  `LootMode` smallint UNSIGNED NOT NULL DEFAULT 1,
  `GroupId` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `MinCount` tinyint UNSIGNED NOT NULL DEFAULT 1,
  `MaxCount` tinyint UNSIGNED NOT NULL DEFAULT 1,
  `Comment` varchar(255) DEFAULT NULL,
  KEY `idx_entry` (`Entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb3;

CREATE TABLE IF NOT EXISTS `companion_roster` (
  `entry`      INT UNSIGNED NOT NULL COMMENT 'creature_template entry',
  `name`       VARCHAR(64) NOT NULL,
  `role`       TINYINT UNSIGNED NOT NULL COMMENT '0=Tank,1=Melee,2=Ranged,3=Caster,4=Healer',
  `spell1`     INT UNSIGNED NOT NULL DEFAULT 0,
  `spell2`     INT UNSIGNED NOT NULL DEFAULT 0,
  `spell3`     INT UNSIGNED NOT NULL DEFAULT 0,
  `cooldown1`  INT UNSIGNED NOT NULL DEFAULT 8000,
  `cooldown2`  INT UNSIGNED NOT NULL DEFAULT 12000,
  `cooldown3`  INT UNSIGNED NOT NULL DEFAULT 15000,
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Companion squad roster definitions';

-- ============================================================================
-- CHARACTERS DATABASE
-- ============================================================================
USE characters;

CREATE TABLE IF NOT EXISTS `character_companion_squad` (
  `guid`          BIGINT UNSIGNED NOT NULL,
  `slot`          TINYINT UNSIGNED NOT NULL COMMENT '0-4',
  `roster_entry`  INT UNSIGNED NOT NULL,
  PRIMARY KEY (`guid`, `slot`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Per-character companion squad slots';

CREATE TABLE IF NOT EXISTS `character_companion_control` (
  `guid`       BIGINT UNSIGNED NOT NULL,
  `mode`       TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '0=Passive,1=Defend,2=Assist',
  `following`  TINYINT UNSIGNED NOT NULL DEFAULT 1,
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Per-character companion control state';

CREATE TABLE IF NOT EXISTS `character_transmog_outfits` (
  `guid` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Character GUID',
  `setguid` bigint unsigned NOT NULL AUTO_INCREMENT COMMENT 'Unique outfit set GUID',
  `setindex` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Outfit index (slot)',
  `name` varchar(128) NOT NULL COMMENT 'Outfit display name',
  `iconname` varchar(256) NOT NULL COMMENT 'Outfit icon identifier',
  `ignore_mask` int NOT NULL DEFAULT '0' COMMENT 'Bitmask of ignored equipment slots',
  `appearance0` int unsigned NOT NULL DEFAULT '0',
  `appearance1` int unsigned NOT NULL DEFAULT '0',
  `appearance2` int unsigned NOT NULL DEFAULT '0',
  `appearance3` int unsigned NOT NULL DEFAULT '0',
  `appearance4` int unsigned NOT NULL DEFAULT '0',
  `appearance5` int unsigned NOT NULL DEFAULT '0',
  `appearance6` int unsigned NOT NULL DEFAULT '0',
  `appearance7` int unsigned NOT NULL DEFAULT '0',
  `appearance8` int unsigned NOT NULL DEFAULT '0',
  `appearance9` int unsigned NOT NULL DEFAULT '0',
  `appearance10` int unsigned NOT NULL DEFAULT '0',
  `appearance11` int unsigned NOT NULL DEFAULT '0',
  `appearance12` int unsigned NOT NULL DEFAULT '0',
  `appearance13` int unsigned NOT NULL DEFAULT '0',
  `appearance14` int unsigned NOT NULL DEFAULT '0',
  `appearance15` int unsigned NOT NULL DEFAULT '0',
  `appearance16` int unsigned NOT NULL DEFAULT '0',
  `appearance17` int unsigned NOT NULL DEFAULT '0',
  `appearance18` int unsigned NOT NULL DEFAULT '0',
  `mainHandEnchant` int unsigned NOT NULL DEFAULT '0',
  `offHandEnchant` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`setguid`),
  UNIQUE KEY `idx_set` (`guid`,`setguid`,`setindex`),
  KEY `Idx_setindex` (`guid`,`setindex`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Player Transmog Outfits';

CREATE TABLE IF NOT EXISTS `character_transmog_outfit_situations` (
  `guid` bigint unsigned NOT NULL COMMENT 'Character GUID',
  `setguid` bigint unsigned NOT NULL COMMENT 'Equipment set GUID',
  `situationID` int unsigned NOT NULL DEFAULT '0',
  `specID` int unsigned NOT NULL DEFAULT '0',
  `loadoutID` int unsigned NOT NULL DEFAULT '0',
  `equipmentSetID` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`,`setguid`,`situationID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Transmog outfit auto-switch situations';

CREATE TABLE IF NOT EXISTS `player_morph` (
  `playerGuid` bigint unsigned NOT NULL,
  `morphDisplayId` int unsigned NOT NULL DEFAULT 0,
  `scale` float NOT NULL DEFAULT 1,
  PRIMARY KEY (`playerGuid`)
) ENGINE=InnoDB;

-- ============================================================================
-- ROLEPLAY DATABASE
-- ============================================================================
CREATE DATABASE IF NOT EXISTS roleplay CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
USE roleplay;

CREATE TABLE IF NOT EXISTS `creature_extra` (
  `guid` bigint UNSIGNED NOT NULL,
  `scale` float NOT NULL DEFAULT -1,
  `id_creator_bnet` int UNSIGNED NOT NULL DEFAULT 0,
  `id_creator_player` bigint UNSIGNED NOT NULL DEFAULT 0,
  `id_modifier_bnet` int UNSIGNED NOT NULL DEFAULT 0,
  `id_modifier_player` bigint UNSIGNED NOT NULL DEFAULT 0,
  `created` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `modified` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `phaseMask` int UNSIGNED NOT NULL DEFAULT 1,
  `displayLock` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `displayId` int UNSIGNED NOT NULL DEFAULT 0,
  `nativeDisplayId` int UNSIGNED NOT NULL DEFAULT 0,
  `genderLock` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `gender` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `swim` tinyint UNSIGNED NOT NULL DEFAULT 1,
  `gravity` tinyint UNSIGNED NOT NULL DEFAULT 1,
  `fly` tinyint UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`guid`) USING BTREE
) ENGINE=InnoDB CHARACTER SET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=Dynamic;

CREATE TABLE IF NOT EXISTS `creature_template_extra` (
  `id_entry` int UNSIGNED NOT NULL,
  `disabled` tinyint NOT NULL DEFAULT 0,
  PRIMARY KEY (`id_entry`) USING BTREE
) ENGINE=InnoDB CHARACTER SET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=Dynamic;

CREATE TABLE IF NOT EXISTS `custom_npcs` (
  `Key` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL,
  `Entry` int UNSIGNED NOT NULL,
  PRIMARY KEY (`Key`) USING BTREE
) ENGINE=InnoDB CHARACTER SET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=Dynamic;

CREATE TABLE IF NOT EXISTS `server_settings` (
  `setting_name` VARCHAR(50) NOT NULL,
  `setting_value` VARCHAR(255) NOT NULL,
  PRIMARY KEY (`setting_name`)
) ENGINE=InnoDB CHARACTER SET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=Dynamic;

CREATE TABLE IF NOT EXISTS `codex_aggregated` (
  `creature_entry` INT UNSIGNED NOT NULL,
  `spell_id` INT UNSIGNED NOT NULL,
  `cast_count` INT UNSIGNED NOT NULL DEFAULT 0,
  `last_reporter` VARCHAR(64) NOT NULL DEFAULT '',
  `last_seen` INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`creature_entry`, `spell_id`),
  KEY `idx_creature` (`creature_entry`),
  KEY `idx_spell` (`spell_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='CreatureCodex multi-player aggregated spell discoveries';


USE world;

-- Idempotent custom column additions (MySQL 8.0 compatible)
USE `world`;
DROP PROCEDURE IF EXISTS add_custom_columns;
DELIMITER //
CREATE PROCEDURE add_custom_columns()
BEGIN
    -- gameobject.size
    IF NOT EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'gameobject' AND COLUMN_NAME = 'size') THEN
        ALTER TABLE `gameobject` ADD COLUMN `size` FLOAT NOT NULL DEFAULT '-1';
    END IF;

    -- gameobject.visibility
    IF NOT EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'gameobject' AND COLUMN_NAME = 'visibility') THEN
        ALTER TABLE `gameobject` ADD COLUMN `visibility` FLOAT NOT NULL DEFAULT '256';
    END IF;

    -- creature.size
    IF NOT EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'creature' AND COLUMN_NAME = 'size') THEN
        ALTER TABLE `creature` ADD COLUMN `size` FLOAT NOT NULL DEFAULT '-1' AFTER `StringId`;
    END IF;

    -- npc_vendor.OverrideGoldCost
    IF NOT EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'npc_vendor' AND COLUMN_NAME = 'OverrideGoldCost') THEN
        ALTER TABLE `npc_vendor` ADD COLUMN `OverrideGoldCost` INT NOT NULL DEFAULT '-1';
    END IF;

    -- scrapping_loot_template.ItemType
    IF NOT EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'scrapping_loot_template' AND COLUMN_NAME = 'ItemType') THEN
        ALTER TABLE `scrapping_loot_template` ADD COLUMN `ItemType` tinyint NOT NULL DEFAULT 0 AFTER `Entry`;
    END IF;

    -- creature_loot_template.Reference
    IF NOT EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'creature_loot_template' AND COLUMN_NAME = 'Reference') THEN
        ALTER TABLE `creature_loot_template` ADD COLUMN `Reference` VARCHAR(255) DEFAULT NULL AFTER `Comment`;
    END IF;
END //
DELIMITER ;
CALL add_custom_columns();
DROP PROCEDURE IF EXISTS add_custom_columns;-- ----- End file: 0.custom_tables.sql -----
-- ----- Begin file: 1.auth.sql -----
USE `auth`;
INSERT IGNORE INTO `rbac_permissions` VALUES (1002, 'Command: .barber');
INSERT IGNORE INTO `rbac_permissions` VALUES (1003, 'Command: .castgroup');
INSERT IGNORE INTO `rbac_permissions` VALUES (1004, 'Command: .castscene');
INSERT IGNORE INTO `rbac_permissions` VALUES (1360, 'Command: .customnpc set displayid');
INSERT IGNORE INTO `rbac_permissions` VALUES (1361, 'Command: .customnpc set guild');
INSERT IGNORE INTO `rbac_permissions` VALUES (1362, 'Command: .customnpc set rank');
INSERT IGNORE INTO `rbac_permissions` VALUES (1363, 'Command: .customnpc set scale');
INSERT IGNORE INTO `rbac_permissions` VALUES (1364, 'Command: .customnpc set tameable');
INSERT IGNORE INTO `rbac_permissions` VALUES (1365, 'Command: .customnpc remove variation');
INSERT IGNORE INTO `rbac_permissions` VALUES (1398, 'Command: .gobject set scale');
INSERT IGNORE INTO `rbac_permissions` VALUES (1589, 'Command: .npc set scale');
INSERT IGNORE INTO `rbac_permissions` VALUES (2101, 'Command: .customnpc create');
INSERT IGNORE INTO `rbac_permissions` VALUES (2102, 'Command: .customnpc spawn');
INSERT IGNORE INTO `rbac_permissions` VALUES (2103, 'Command: .customnpc set displayname');
INSERT IGNORE INTO `rbac_permissions` VALUES (2104, 'Command: .customnpc set face');
INSERT IGNORE INTO `rbac_permissions` VALUES (2105, 'Command: .customnpc set gender');
INSERT IGNORE INTO `rbac_permissions` VALUES (2106, 'Command: .customnpc set race');
INSERT IGNORE INTO `rbac_permissions` VALUES (2107, 'Command: .customnpc set subname');
INSERT IGNORE INTO `rbac_permissions` VALUES (2108, 'Command: .customnpc equip armor');
INSERT IGNORE INTO `rbac_permissions` VALUES (2109, 'Command: .customnpc equip left');
INSERT IGNORE INTO `rbac_permissions` VALUES (2110, 'Command: .customnpc equip ranged');
INSERT IGNORE INTO `rbac_permissions` VALUES (2111, 'Command: .customnpc equip right');
INSERT IGNORE INTO `rbac_permissions` VALUES (2112, 'Command: .customnpc delete');
INSERT IGNORE INTO `rbac_permissions` VALUES (3000, 'Command: .wmorph');
INSERT IGNORE INTO `rbac_permissions` VALUES (3001, 'Command: .wscale');
INSERT IGNORE INTO `rbac_permissions` VALUES (3002, 'Command: .remorph');
INSERT IGNORE INTO `rbac_permissions` VALUES (3004, 'Command: .gob visible');
INSERT IGNORE INTO `rbac_permissions` VALUES (1005, 'Command: .typing on');
INSERT IGNORE INTO `rbac_permissions` VALUES (1006, 'Command: .typing off');
INSERT IGNORE INTO `rbac_permissions` VALUES (1022, 'Command: .settime');
INSERT IGNORE INTO `rbac_permissions` VALUES (1008, 'Command: .disp');
INSERT IGNORE INTO `rbac_permissions` VALUES (1009, 'Command: .disp head');
INSERT IGNORE INTO `rbac_permissions` VALUES (1010, 'Command: .disp shoulders');
INSERT IGNORE INTO `rbac_permissions` VALUES (1011, 'Command: .disp shirt');
INSERT IGNORE INTO `rbac_permissions` VALUES (1012, 'Command: .disp chest');
INSERT IGNORE INTO `rbac_permissions` VALUES (1013, 'Command: .disp waist'); 
INSERT IGNORE INTO `rbac_permissions` VALUES (1014, 'Command: .disp legs'); 
INSERT IGNORE INTO `rbac_permissions` VALUES (1015, 'Command: .disp feet'); 
INSERT IGNORE INTO `rbac_permissions` VALUES (1016, 'Command: .disp wrists'); 
INSERT IGNORE INTO `rbac_permissions` VALUES (1017, 'Command: .disp hands'); 
INSERT IGNORE INTO `rbac_permissions` VALUES (1018, 'Command: .disp back'); 
INSERT IGNORE INTO `rbac_permissions` VALUES (1019, 'Command: .disp tabard'); 
INSERT IGNORE INTO `rbac_permissions` VALUES (1020, 'Command: .disp mainhand'); 
INSERT IGNORE INTO `rbac_permissions` VALUES (1021, 'Command: .disp offhand');

INSERT IGNORE INTO `rbac_linked_permissions` VALUES (199, 1002);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 1003);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 1004);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 1360);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 1361);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 1362);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 1363);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 1364);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 1365);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 1398);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 1589);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 2101);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 2102);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 2103);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 2104);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 2105);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 2106);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 2107);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 2108);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 2109);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 2110);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 2111);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 2112);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 3004);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (199, 3000);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (199, 3001);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (199, 3002);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 1022);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (199, 1005);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (199, 1006);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (199, 1008);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (199, 1009);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (199, 1010);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (199, 1011);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (199, 1012);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (199, 1013);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (199, 1014);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (199, 1015);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (199, 1016);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (199, 1017);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (199, 1018);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (199, 1019);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (199, 1020);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (199, 1021);

INSERT IGNORE INTO `rbac_permissions` VALUES (1007, 'Command: .typing');
INSERT IGNORE INTO `rbac_permissions` VALUES (2004, 'Command: .blackmarket');
INSERT IGNORE INTO `rbac_permissions` VALUES (2005, 'Command: .blackmarket set duration');
INSERT IGNORE INTO `rbac_permissions` VALUES (2113, 'Command: .customnpc unequip armor');
INSERT IGNORE INTO `rbac_permissions` VALUES (2114, 'Command: .customnpc unequip left');
INSERT IGNORE INTO `rbac_permissions` VALUES (2115, 'Command: .customnpc unequip ranged');
INSERT IGNORE INTO `rbac_permissions` VALUES (2116, 'Command: .customnpc unequip right');
INSERT IGNORE INTO `rbac_permissions` VALUES (3005, 'Command: .scenario all');
INSERT IGNORE INTO `rbac_permissions` VALUES (3006, 'Command: .npc set aura');
INSERT IGNORE INTO `rbac_permissions` VALUES (3007, 'Command: .outfit');
INSERT IGNORE INTO `rbac_permissions` VALUES (3008, 'Command: .comp');
INSERT IGNORE INTO `rbac_permissions` VALUES (3009, 'Command: .maxrep');
INSERT IGNORE INTO `rbac_permissions` VALUES (3010, 'Command: .maxtitles');
INSERT IGNORE INTO `rbac_permissions` VALUES (3011, 'Command: .maxachieve');
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (199, 1007);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 2004);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 2005);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 2113);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 2114);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 2115);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 2116);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 3005);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 3006);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 3007);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 3008);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 3009);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 3010);
INSERT IGNORE INTO `rbac_linked_permissions` VALUES (193, 3011);

INSERT IGNORE INTO `rbac_default_permissions` (`secId`, `permissionId`) VALUES
(0, 3008);-- ----- End file: 1.auth.sql -----
-- ----- Begin file: 2.hotfixes.sql -----
USE `hotfixes`;
-- ----------------------------
-- Table structure for chr_customization_material
-- ----------------------------
DROP TABLE IF EXISTS `chr_customization_material`;
CREATE TABLE `chr_customization_material`  (
  `ID` int UNSIGNED NOT NULL DEFAULT 0,
  `ChrModelTextureTargetID` int NOT NULL DEFAULT 0,
  `MaterialResourcesID` int NOT NULL DEFAULT 0,
  `VerifiedBuild` int NOT NULL DEFAULT 0,
  PRIMARY KEY (`ID`, `VerifiedBuild`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_unicode_ci ROW_FORMAT = DYNAMIC;

-- ----------------------------
-- Table structure for creature_display_info_option
-- ----------------------------
DROP TABLE IF EXISTS `creature_display_info_option`;
CREATE TABLE `creature_display_info_option`  (
  `ID` int UNSIGNED NOT NULL DEFAULT 0,
  `ChrCustomizationOptionID` int NOT NULL DEFAULT 0,
  `ChrCustomizationChoiceID` int NOT NULL DEFAULT 0,
  `CreatureDisplayInfoExtraID` int NOT NULL DEFAULT 0,
  `VerifiedBuild` int NOT NULL DEFAULT 0,
  PRIMARY KEY (`ID`, `VerifiedBuild`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_unicode_ci ROW_FORMAT = DYNAMIC;

-- ----------------------------
-- Table structure for game_tips
-- ----------------------------
DROP TABLE IF EXISTS `game_tips`;
CREATE TABLE `game_tips`  (
  `ID` int UNSIGNED NOT NULL DEFAULT 0,
  `Text` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NULL,
  `SortIndex` int UNSIGNED NOT NULL DEFAULT 0,
  `MinLevel` int NOT NULL DEFAULT 0,
  `MaxLevel` int NOT NULL DEFAULT 0,
  `ContentTuningID` int NOT NULL DEFAULT 0,
  `VerifiedBuild` int NOT NULL DEFAULT 0,
  PRIMARY KEY (`ID`, `VerifiedBuild`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_unicode_ci ROW_FORMAT = DYNAMIC;

-- ----------------------------
-- Table structure for game_tips_locale
-- ----------------------------
DROP TABLE IF EXISTS `game_tips_locale`;
CREATE TABLE `game_tips_locale`  (
  `ID` int UNSIGNED NOT NULL DEFAULT 0,
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Text_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NULL,
  `VerifiedBuild` int NOT NULL DEFAULT 0,
  PRIMARY KEY (`ID`, `locale`, `VerifiedBuild`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_unicode_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for light_params
-- ----------------------------
DROP TABLE IF EXISTS `light_params`;
CREATE TABLE `light_params`  (
  `ID` int UNSIGNED NOT NULL DEFAULT 0,
  `OverrideCelestialSphere1` float NOT NULL DEFAULT 0,
  `OverrideCelestialSphere2` float NOT NULL DEFAULT 0,
  `OverrideCelestialSphere3` float NOT NULL DEFAULT 0,
  `OverrideSunPosition1` float NOT NULL DEFAULT 0,
  `OverrideSunPosition2` float NOT NULL DEFAULT 0,
  `OverrideSunPosition3` float NOT NULL DEFAULT 0,
  `HighlightSky` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `LightSkyboxID` smallint UNSIGNED NOT NULL DEFAULT 0,
  `CloudTypeID` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `Glow` float NOT NULL DEFAULT 0,
  `WaterShallowAlpha` float NOT NULL DEFAULT 0,
  `WaterDeepAlpha` float NOT NULL DEFAULT 0,
  `OceanShallowAlpha` float NOT NULL DEFAULT 0,
  `OceanDeepAlpha` float NOT NULL DEFAULT 0,
  `Flags` int NOT NULL DEFAULT 0,
  `SsaoSettingsID` int NOT NULL DEFAULT 0,
  `SunPolar` float NOT NULL DEFAULT 0,
  `SunAzimuth` float NOT NULL DEFAULT 0,
  `SunAttenuationStart` float NOT NULL DEFAULT 0,
  `SunAttenuationEnd` float NOT NULL DEFAULT 0,
  `Field_12_0_1_65617_016` float NOT NULL DEFAULT 0,
  `Field_12_0_1_65617_017` float NOT NULL DEFAULT 0,
  `Field_12_0_1_65617_018` float NOT NULL DEFAULT 0,
  `Field_12_0_1_65617_019` float NOT NULL DEFAULT 0,
  `Field_12_0_1_65617_020` float NOT NULL DEFAULT 0,
  `Field_12_0_1_65617_021` float NOT NULL DEFAULT 0,
  `Field_12_0_1_65617_022` float NOT NULL DEFAULT 0,
  `Field_12_0_1_65617_023` float NOT NULL DEFAULT 0,
  `Field_12_0_1_65617_024` float NOT NULL DEFAULT 0,
  `Field_12_0_1_65617_025` float NOT NULL DEFAULT 0,
  `Field_12_0_1_65617_026` float NOT NULL DEFAULT 0,
  `Field_12_0_1_65617_027` int NOT NULL DEFAULT 0,
  `Field_12_0_1_65617_028` float NOT NULL DEFAULT 0,
  `Field_12_0_1_65617_029` float NOT NULL DEFAULT 0,
  `Field_12_0_1_65617_030` float NOT NULL DEFAULT 0,
  `Field_12_0_1_65617_031` float NOT NULL DEFAULT 0,
  `VerifiedBuild` int NOT NULL DEFAULT 0,
  PRIMARY KEY (`ID`, `VerifiedBuild`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_unicode_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for light_skybox
-- ----------------------------
DROP TABLE IF EXISTS `light_skybox`;
CREATE TABLE `light_skybox`  (
  `Id` int UNSIGNED NOT NULL DEFAULT 0,
  `Name` mediumtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Flags` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `SkyboxFileDataID` int NOT NULL DEFAULT 0,
  `CelestialSkyboxFileDataID` int NOT NULL DEFAULT 0,
  `VerifiedBuild` int NOT NULL DEFAULT 0,
  PRIMARY KEY (`Id`, `VerifiedBuild`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_unicode_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for model_file_data
-- ----------------------------
DROP TABLE IF EXISTS `model_file_data`;
CREATE TABLE `model_file_data`  (
  `Geobox1` float NOT NULL DEFAULT 0,
  `Geobox2` float NOT NULL DEFAULT 0,
  `Geobox3` float NOT NULL DEFAULT 0,
  `Geobox4` float NOT NULL DEFAULT 0,
  `Geobox5` float NOT NULL DEFAULT 0,
  `Geobox6` float NOT NULL DEFAULT 0,
  `ID` int UNSIGNED NOT NULL DEFAULT 0,
  `Flags` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `LogCount` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `ModelID` int UNSIGNED NOT NULL DEFAULT 0,
  `VerifiedBuild` int NOT NULL DEFAULT 0,
  `ModelResourcesID` int NOT NULL,
  PRIMARY KEY (`ID`, `VerifiedBuild`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_unicode_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for npc_model_item_slot_display_info
-- ----------------------------
DROP TABLE IF EXISTS `npc_model_item_slot_display_info`;
CREATE TABLE `npc_model_item_slot_display_info`  (
  `ID` int UNSIGNED NOT NULL DEFAULT 0,
  `DisplayID` int UNSIGNED NOT NULL DEFAULT 0,
  `Slot` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `ExtendedDisplayID` int UNSIGNED NOT NULL DEFAULT 0,
  `VerifiedBuild` int NOT NULL DEFAULT 0,
  PRIMARY KEY (`ID`, `VerifiedBuild`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_unicode_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for screen_effect
-- ----------------------------
DROP TABLE IF EXISTS `screen_effect`;
CREATE TABLE `screen_effect`  (
  `ID` int UNSIGNED NOT NULL DEFAULT 0,
  `DisplayName` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NULL,
  `Param1` int NOT NULL DEFAULT 0,
  `Param2` int NOT NULL DEFAULT 0,
  `Param3` int NOT NULL DEFAULT 0,
  `Param4` int NOT NULL DEFAULT 0,
  `Effect` tinyint NOT NULL DEFAULT 0,
  `FullScreenEffectID` int UNSIGNED NOT NULL DEFAULT 0,
  `LightParamsID` smallint UNSIGNED NOT NULL DEFAULT 0,
  `LightParamsFadeIn` smallint UNSIGNED NOT NULL DEFAULT 0,
  `LightParamsFadeOut` smallint UNSIGNED NOT NULL DEFAULT 0,
  `SoundAmbienceID` int UNSIGNED NOT NULL DEFAULT 0,
  `ZoneMusicID` int UNSIGNED NOT NULL DEFAULT 0,
  `TimeOfDayOverride` smallint NOT NULL DEFAULT 0,
  `EffectMask` tinyint NOT NULL DEFAULT 0,
  `LightFlags` int UNSIGNED NOT NULL DEFAULT 0,
  `VerifiedBuild` int NOT NULL DEFAULT 0,
  PRIMARY KEY (`ID`, `VerifiedBuild`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_unicode_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for sound_kit_advanced
-- ----------------------------
DROP TABLE IF EXISTS `sound_kit_advanced`;
CREATE TABLE `sound_kit_advanced`  (
  `ID` int NOT NULL DEFAULT 0,
  `SoundKitID` int UNSIGNED NOT NULL DEFAULT 0,
  `InnerRadius2D` float NOT NULL DEFAULT 0,
  `OuterRadius2D` float NOT NULL DEFAULT 0,
  `TimeA` int UNSIGNED NOT NULL DEFAULT 0,
  `TimeB` int UNSIGNED NOT NULL DEFAULT 0,
  `TimeC` int UNSIGNED NOT NULL DEFAULT 0,
  `TimeD` int UNSIGNED NOT NULL DEFAULT 0,
  `RandomOffsetRange` int NOT NULL DEFAULT 0,
  `Usage` tinyint NOT NULL DEFAULT 0,
  `TimeIntervalMin` int UNSIGNED NOT NULL DEFAULT 0,
  `TimeIntervalMax` int UNSIGNED NOT NULL DEFAULT 0,
  `DelayMin` int UNSIGNED NOT NULL DEFAULT 0,
  `DelayMax` int UNSIGNED NOT NULL DEFAULT 0,
  `VolumeSliderCategory` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `DuckToSFX` float NOT NULL DEFAULT 0,
  `DuckToMusic` float NOT NULL DEFAULT 0,
  `DuckToAmbience` float NOT NULL DEFAULT 0,
  `DuckToDialog` float NOT NULL DEFAULT 0,
  `DuckToSuppressors` float NOT NULL DEFAULT 0,
  `DuckToCinematicSFX` float NOT NULL DEFAULT 0,
  `DuckToCinematicMusic` float NOT NULL DEFAULT 0,
  `Field_11_2_0_61476_021` float NOT NULL DEFAULT 0,
  `InnerRadiusOfInfluence` float NOT NULL DEFAULT 0,
  `OuterRadiusOfInfluence` float NOT NULL DEFAULT 0,
  `TimeToDuck` int UNSIGNED NOT NULL DEFAULT 0,
  `TimeToUnduck` int UNSIGNED NOT NULL DEFAULT 0,
  `InsideAngle` float NOT NULL DEFAULT 0,
  `OutsideAngle` float NOT NULL DEFAULT 0,
  `OutsideVolume` float NOT NULL DEFAULT 0,
  `MinRandomPosOffset` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `MaxRandomPosOffset` smallint UNSIGNED NOT NULL DEFAULT 0,
  `MsOffset` int NOT NULL DEFAULT 0,
  `TimeCooldownMin` int UNSIGNED NOT NULL DEFAULT 0,
  `TimeCooldownMax` int UNSIGNED NOT NULL DEFAULT 0,
  `MaxInstancesBehavior` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `VolumeControlType` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `VolumeFadeInTimeMin` int NOT NULL DEFAULT 0,
  `VolumeFadeInTimeMax` int NOT NULL DEFAULT 0,
  `VolumeFadeInCurveID` int UNSIGNED NOT NULL DEFAULT 0,
  `VolumeFadeOutTimeMin` int NOT NULL DEFAULT 0,
  `VolumeFadeOutTimeMax` int NOT NULL DEFAULT 0,
  `VolumeFadeOutCurveID` int UNSIGNED NOT NULL DEFAULT 0,
  `ChanceToPlay` float NOT NULL DEFAULT 0,
  `RolloffType` int NOT NULL DEFAULT 0,
  `RolloffParam0` float NOT NULL DEFAULT 0,
  `Field_8_2_0_30080_045` float NOT NULL DEFAULT 0,
  `Field_8_2_0_30080_046` float NOT NULL DEFAULT 0,
  `Field_8_2_0_30080_047` int NOT NULL DEFAULT 0,
  `Field_8_2_0_30080_048` int NOT NULL DEFAULT 0,
  `Field_8_2_0_30080_049` float NOT NULL DEFAULT 0,
  `Field_8_2_0_30080_050` float NOT NULL DEFAULT 0,
  `Field_8_2_0_30080_051` float NOT NULL DEFAULT 0,
  `Field_8_2_0_30080_052` float NOT NULL DEFAULT 0,
  `Field_8_2_0_30080_053` float NOT NULL DEFAULT 0,
  `Field_8_2_0_30080_054` float NOT NULL DEFAULT 0,
  `Field_9_1_0_38312_055` float NOT NULL DEFAULT 0,
  `Field_9_1_0_38312_056` float NOT NULL DEFAULT 0,
  `VerifiedBuild` int NOT NULL DEFAULT 0,
  PRIMARY KEY (`ID`, `VerifiedBuild`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_unicode_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for sound_kit_entry
-- ----------------------------
DROP TABLE IF EXISTS `sound_kit_entry`;
CREATE TABLE `sound_kit_entry`  (
  `ID` int NOT NULL DEFAULT 0,
  `SoundKitID` int UNSIGNED NOT NULL DEFAULT 0,
  `FileDataID` int NOT NULL DEFAULT 0,
  `Frequency` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `Volume` float NOT NULL DEFAULT 0,
  `PlayerConditionID` int UNSIGNED NOT NULL DEFAULT 0,
  `VerifiedBuild` int NOT NULL DEFAULT 0,
  PRIMARY KEY (`ID`, `VerifiedBuild`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_unicode_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for specialization_spells_display
-- ----------------------------
DROP TABLE IF EXISTS `specialization_spells_display`;
CREATE TABLE `specialization_spells_display`  (
  `ID` int UNSIGNED NOT NULL DEFAULT 0,
  `SpecializationID` smallint UNSIGNED NOT NULL DEFAULT 0,
  `SpecllID1` int UNSIGNED NOT NULL DEFAULT 0,
  `SpecllID2` int UNSIGNED NOT NULL DEFAULT 0,
  `SpecllID3` int UNSIGNED NOT NULL DEFAULT 0,
  `SpecllID4` int UNSIGNED NOT NULL DEFAULT 0,
  `SpecllID5` int UNSIGNED NOT NULL DEFAULT 0,
  `SpecllID6` int UNSIGNED NOT NULL DEFAULT 0,
  `VerifiedBuild` int NOT NULL DEFAULT 0,
  PRIMARY KEY (`ID`, `VerifiedBuild`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_unicode_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for spell
-- ----------------------------
DROP TABLE IF EXISTS `spell`;
CREATE TABLE `spell`  (
  `ID` int UNSIGNED NOT NULL DEFAULT 0,
  `NameSubtext` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NULL,
  `Description` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NULL,
  `AuraDescription` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NULL,
  `VerifiedBuild` int NOT NULL DEFAULT 0,
  PRIMARY KEY (`ID`, `VerifiedBuild`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_unicode_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for spell_locale
-- ----------------------------
DROP TABLE IF EXISTS `spell_locale`;
CREATE TABLE `spell_locale`  (
  `ID` int UNSIGNED NOT NULL DEFAULT 0,
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `NameSubtext_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NULL,
  `Description_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NULL,
  `AuraDescription_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NULL,
  `VerifiedBuild` int NOT NULL DEFAULT 0,
  PRIMARY KEY (`ID`, `locale`, `VerifiedBuild`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_unicode_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for texture_file_data
-- ----------------------------
DROP TABLE IF EXISTS `texture_file_data`;
CREATE TABLE `texture_file_data`  (
  `ID` int UNSIGNED NOT NULL,
  `UsageType` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `TextureID` int NOT NULL DEFAULT 0,
  `VerifiedBuild` int NOT NULL DEFAULT 0,
  PRIMARY KEY (`ID`, `VerifiedBuild`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_unicode_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for vehicle_poi_type
-- ----------------------------
DROP TABLE IF EXISTS `vehicle_poi_type`;
CREATE TABLE `vehicle_poi_type`  (
  `ID` int NOT NULL DEFAULT 0,
  `Flags` int NOT NULL DEFAULT 0,
  `TextureWidth` int NOT NULL DEFAULT 0,
  `TextureHeight` int NOT NULL DEFAULT 0,
  `OccupiedTexture` int NOT NULL DEFAULT 0,
  `UnoccupiedTexture` int NOT NULL DEFAULT 0,
  `VerifiedBuild` int NOT NULL DEFAULT 0,
  PRIMARY KEY (`ID`, `VerifiedBuild`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_unicode_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for sound_ambience
-- ----------------------------
DROP TABLE IF EXISTS `sound_ambience`;
CREATE TABLE `sound_ambience`  (
  `ID` int UNSIGNED NOT NULL DEFAULT 0,
  `Flags` int NOT NULL DEFAULT 0,
  `FlavorSoundFilterID` int UNSIGNED NOT NULL DEFAULT 0,
  `AmbienceID1` int UNSIGNED NOT NULL DEFAULT 0,
  `AmbienceID2` int UNSIGNED NOT NULL DEFAULT 0,
  `AmbienceStartID1` int UNSIGNED NOT NULL DEFAULT 0,
  `AmbienceStartID2` int UNSIGNED NOT NULL DEFAULT 0,
  `AmbienceStopID1` int UNSIGNED NOT NULL DEFAULT 0,
  `AmbienceStopID2` int UNSIGNED NOT NULL DEFAULT 0,
  `SoundKitID1` int UNSIGNED NOT NULL DEFAULT 0,
  `SoundKitID2` int UNSIGNED NOT NULL DEFAULT 0,
  `VerifiedBuild` int NOT NULL DEFAULT 0,
  PRIMARY KEY (`ID`, `VerifiedBuild`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_unicode_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for zone_music
-- ----------------------------
DROP TABLE IF EXISTS `zone_music`;
CREATE TABLE `zone_music`  (
  `ID` int UNSIGNED NOT NULL DEFAULT 0,
  `SetName` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NULL,
  `SilenceIntervalMin1` int UNSIGNED NOT NULL DEFAULT 0,
  `SilenceIntervalMin2` int UNSIGNED NOT NULL DEFAULT 0,
  `SilenceIntervalMax1` int UNSIGNED NOT NULL DEFAULT 0,
  `SilenceIntervalMax2` int UNSIGNED NOT NULL DEFAULT 0,
  `Sounds1` int UNSIGNED NOT NULL DEFAULT 0,
  `Sounds2` int UNSIGNED NOT NULL DEFAULT 0,
  `VerifiedBuild` int NOT NULL DEFAULT 0,
  PRIMARY KEY (`ID`, `VerifiedBuild`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_unicode_ci ROW_FORMAT = DYNAMIC;

-- ----------------------------
-- Table structure for zone_music_locale
-- ----------------------------
DROP TABLE IF EXISTS `zone_music_locale`;
CREATE TABLE `zone_music_locale`  (
  `ID` int UNSIGNED NOT NULL DEFAULT 0,
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `SetName_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NULL,
  `VerifiedBuild` int NOT NULL DEFAULT 0,
  PRIMARY KEY (`ID`, `locale`, `VerifiedBuild`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
/*!50500 PARTITION BY LIST  COLUMNS(locale)
(PARTITION deDE VALUES IN ('deDE') ENGINE = InnoDB,
 PARTITION esES VALUES IN ('esES') ENGINE = InnoDB,
 PARTITION esMX VALUES IN ('esMX') ENGINE = InnoDB,
 PARTITION frFR VALUES IN ('frFR') ENGINE = InnoDB,
 PARTITION itIT VALUES IN ('itIT') ENGINE = InnoDB,
 PARTITION koKR VALUES IN ('koKR') ENGINE = InnoDB,
 PARTITION ptBR VALUES IN ('ptBR') ENGINE = InnoDB,
 PARTITION ruRU VALUES IN ('ruRU') ENGINE = InnoDB,
 PARTITION zhCN VALUES IN ('zhCN') ENGINE = InnoDB,
 PARTITION zhTW VALUES IN ('zhTW') ENGINE = InnoDB) */;

-- ----------------------------
-- Table structure for npc_sounds
-- ----------------------------
DROP TABLE IF EXISTS `npc_sounds`;
CREATE TABLE `npc_sounds` (
	`ID` INT(10) UNSIGNED NOT NULL,
	`hello` INT(10) UNSIGNED NOT NULL DEFAULT '0',
	`goodbye` INT(10) UNSIGNED NOT NULL DEFAULT '0',
	`pissed` INT(10) UNSIGNED NOT NULL DEFAULT '0',
	`ack` INT(10) UNSIGNED NOT NULL DEFAULT '0',
	`VerifiedBuild` INT(11) NOT NULL DEFAULT '0',
	PRIMARY KEY (`ID`, `VerifiedBuild`) USING BTREE
) ENGINE=InnoDB CHARACTER SET = utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ----------------------------
-- Table structure for item_display_info
-- ----------------------------
DROP TABLE IF EXISTS `item_display_info`;
CREATE TABLE `item_display_info` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `GeosetGroupOverride` int NOT NULL DEFAULT '0',
  `ItemVisual` int NOT NULL DEFAULT '0',
  `ParticleColorID` int NOT NULL DEFAULT '0',
  `ItemRangedDisplayInfoID` int unsigned NOT NULL DEFAULT '0',
  `OverrideSwooshSoundKitID` int unsigned NOT NULL DEFAULT '0',
  `SheatheTransformMatrixID` int NOT NULL DEFAULT '0',
  `StateSpellVisualKitID` int NOT NULL DEFAULT '0',
  `SheathedSpellVisualKitID` int NOT NULL DEFAULT '0',
  `UnsheathedSpellVisualKitID` int unsigned NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `ModelResourcesID1` int unsigned NOT NULL DEFAULT '0',
  `ModelResourcesID2` int unsigned NOT NULL DEFAULT '0',
  `ModelMaterialResourcesID1` int NOT NULL DEFAULT '0',
  `ModelMaterialResourcesID2` int NOT NULL DEFAULT '0',
  `ModelType1` int NOT NULL DEFAULT '0',
  `ModelType2` int NOT NULL DEFAULT '0',
  `GeosetGroup1` int NOT NULL DEFAULT '0',
  `GeosetGroup2` int NOT NULL DEFAULT '0',
  `GeosetGroup3` int NOT NULL DEFAULT '0',
  `GeosetGroup4` int NOT NULL DEFAULT '0',
  `GeosetGroup5` int NOT NULL DEFAULT '0',
  `GeosetGroup6` int NOT NULL DEFAULT '0',
  `AttachmentGeosetGroup1` int NOT NULL DEFAULT '0',
  `AttachmentGeosetGroup2` int NOT NULL DEFAULT '0',
  `AttachmentGeosetGroup3` int NOT NULL DEFAULT '0',
  `AttachmentGeosetGroup4` int NOT NULL DEFAULT '0',
  `AttachmentGeosetGroup5` int NOT NULL DEFAULT '0',
  `AttachmentGeosetGroup6` int NOT NULL DEFAULT '0',
  `HelmetGeosetVis1` int NOT NULL DEFAULT '0',
  `HelmetGeosetVis2` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ----------------------------
-- Table structure for chr_race_racial_ability
-- ----------------------------
DROP TABLE IF EXISTS `chr_race_racial_ability`;
CREATE TABLE `chr_race_racial_ability` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Name` text,
  `Description` text,
  `DescriptionShort` text,
  `Icon` int NOT NULL DEFAULT '0',
  `Order` int NOT NULL DEFAULT '0',
  `ChrRacesID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `chr_race_racial_ability_locale`;
CREATE TABLE `chr_race_racial_ability_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) NOT NULL,
  `Name_lang` text,
  `Description_lang` text,
  `DescriptionShort_lang` text,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
/*!50500 PARTITION BY LIST  COLUMNS(locale)
(PARTITION deDE VALUES IN ('deDE') ENGINE = InnoDB,
 PARTITION esES VALUES IN ('esES') ENGINE = InnoDB,
 PARTITION esMX VALUES IN ('esMX') ENGINE = InnoDB,
 PARTITION frFR VALUES IN ('frFR') ENGINE = InnoDB,
 PARTITION itIT VALUES IN ('itIT') ENGINE = InnoDB,
 PARTITION koKR VALUES IN ('koKR') ENGINE = InnoDB,
 PARTITION ptBR VALUES IN ('ptBR') ENGINE = InnoDB,
 PARTITION ruRU VALUES IN ('ruRU') ENGINE = InnoDB,
 PARTITION zhCN VALUES IN ('zhCN') ENGINE = InnoDB,
 PARTITION zhTW VALUES IN ('zhTW') ENGINE = InnoDB) */;

--
-- Table structure for global_strings
--
DROP TABLE IF EXISTS `global_strings`;
CREATE TABLE `global_strings` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `BaseTag` text,
  `TagText` text,
  `Flags` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `global_strings_locale`;
CREATE TABLE `global_strings_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) NOT NULL,
  `TagText_lang` text,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
/*!50500 PARTITION BY LIST  COLUMNS(locale)
(PARTITION deDE VALUES IN ('deDE') ENGINE = InnoDB,
 PARTITION esES VALUES IN ('esES') ENGINE = InnoDB,
 PARTITION esMX VALUES IN ('esMX') ENGINE = InnoDB,
 PARTITION frFR VALUES IN ('frFR') ENGINE = InnoDB,
 PARTITION itIT VALUES IN ('itIT') ENGINE = InnoDB,
 PARTITION koKR VALUES IN ('koKR') ENGINE = InnoDB,
 PARTITION ptBR VALUES IN ('ptBR') ENGINE = InnoDB,
 PARTITION ruRU VALUES IN ('ruRU') ENGINE = InnoDB,
 PARTITION zhCN VALUES IN ('zhCN') ENGINE = InnoDB,
 PARTITION zhTW VALUES IN ('zhTW') ENGINE = InnoDB) */;

LOCK TABLES `global_strings` WRITE;
/*!40000 ALTER TABLE `global_strings` DISABLE KEYS */;
/*!40000 ALTER TABLE `global_strings` ENABLE KEYS */;
UNLOCK TABLES;

-- ----------------------------
-- Craft Table structure
-- ----------------------------

DROP TABLE IF EXISTS `research_branch`;
CREATE TABLE IF NOT EXISTS `research_branch` (
  `Id` int unsigned NOT NULL DEFAULT '0',
  `Name` text,
  `ResearchFieldId` tinyint unsigned NOT NULL DEFAULT '0',
  `CurrencyId` smallint unsigned NOT NULL DEFAULT '0',
  `TextureFileId` int NOT NULL DEFAULT '0',
  `BigTextureFileId` int NOT NULL DEFAULT '0',
  `ItemId` int NOT NULL DEFAULT '0',
  `VerifiedBuild` smallint NOT NULL DEFAULT '0',
  PRIMARY KEY (`Id`)
) ENGINE=InnoDB CHARACTER SET = utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `research_branch_locale`;
CREATE TABLE IF NOT EXISTS `research_branch_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) NOT NULL,
  `Name_lang` text,
  `VerifiedBuild` smallint NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`)
) ENGINE=InnoDB CHARACTER SET = utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `research_project`;
CREATE TABLE IF NOT EXISTS `research_project` (
  `Name` text,
  `Description` text,
  `Id` int unsigned NOT NULL DEFAULT '0',
  `Rarity` tinyint unsigned NOT NULL DEFAULT '0',
  `SpellId` int NOT NULL DEFAULT '0',
  `ResearchBranchId` smallint unsigned NOT NULL DEFAULT '0',
  `NumSockets` tinyint unsigned NOT NULL DEFAULT '0',
  `TextureFileId` int NOT NULL DEFAULT '0',
  `RequiredWeight` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` smallint NOT NULL DEFAULT '0',
  PRIMARY KEY (`Id`)
) ENGINE=InnoDB CHARACTER SET = utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `research_project_locale`;
CREATE TABLE IF NOT EXISTS `research_project_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) NOT NULL,
  `Name_lang` text,
  `Description_lang` text,
  `VerifiedBuild` smallint NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`)
) ENGINE=InnoDB CHARACTER SET = utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `research_site`;
CREATE TABLE IF NOT EXISTS `research_site` (
  `Id` int unsigned NOT NULL DEFAULT '0',
  `Name` text,
  `MapId` smallint NOT NULL DEFAULT '0',
  `QuestPoiBlobId` int NOT NULL DEFAULT '0',
  `AreaPOIIconEnum` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` smallint NOT NULL DEFAULT '0',
  PRIMARY KEY (`Id`)
) ENGINE=InnoDB CHARACTER SET = utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `research_site_locale`;
CREATE TABLE IF NOT EXISTS `research_site_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) NOT NULL,
  `Name_lang` text,
  `VerifiedBuild` smallint NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`)
) ENGINE=InnoDB CHARACTER SET = utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `quest_p_o_i_point`;
CREATE TABLE IF NOT EXISTS `quest_p_o_i_point` (
  `ID` int NOT NULL DEFAULT '0',
  `X` smallint NOT NULL DEFAULT '0',
  `Y` smallint NOT NULL DEFAULT '0',
  `Z` smallint NOT NULL DEFAULT '0',
  `QuestPOIBlobID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB CHARACTER SET = utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `pvp_bracket_types`;
CREATE TABLE `pvp_bracket_types` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `BracketID` tinyint NOT NULL DEFAULT '0',
  `WeeklyQuestID_0` int NOT NULL DEFAULT '0',
  `WeeklyQuestID_1` int NOT NULL DEFAULT '0',
  `WeeklyQuestID_2` int NOT NULL DEFAULT '0',
  `WeeklyQuestID_3` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`, `VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

REPLACE INTO `spell_misc` (
    `ID`, 
    `Attributes1`, `Attributes2`, `Attributes3`, `Attributes4`, `Attributes5`, `Attributes6`, `Attributes7`, `Attributes8`, `Attributes9`, `Attributes10`, 
    `Attributes11`, `Attributes12`, `Attributes13`, `Attributes14`, `Attributes15`, `Attributes16`, `Attributes17`, 
    `DifficultyID`, `CastingTimeIndex`, `DurationIndex`, `PvPDurationIndex`, `RangeIndex`, `SchoolMask`, `Speed`, `LaunchDelay`, `MinDuration`, `SpellIconFileDataID`, 
    `ActiveIconFileDataID`, `ContentTuningID`, `ShowFutureSpellPlayerConditionID`, `SpellVisualScript`, `ActiveSpellVisualScript`, `SpellID`, `VerifiedBuild`) 
    VALUES (170768, 
    0, 268435456, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 1, 21, 0, 1, 1, 0, 0, 0, 988194, 0, 0, 0, 0, 0, 196742, 60257);
REPLACE INTO `hotfix_data` VALUES (170768, 170768, 3322146344, 170768, 1, 60257);-- ----- End file: 2.hotfixes.sql -----
-- ----- Begin file: 3.character_db.sql -----
-- Favorites

-- ============================================================================
-- Companion Squad System — Database Setup
-- Run against: world, characters, auth (in that order)
-- ============================================================================
USE `world`;
-- ============================================================================
-- WORLD DATABASE: companion_roster
-- ============================================================================
-- Run: mysql -u root -padmin world < "this_file.sql"
-- (Or apply the world section manually)

CREATE TABLE IF NOT EXISTS `companion_roster` (
    `entry`      INT UNSIGNED NOT NULL COMMENT 'creature_template entry',
    `name`       VARCHAR(64) NOT NULL,
    `role`       TINYINT UNSIGNED NOT NULL COMMENT '0=Tank,1=Melee,2=Ranged,3=Caster,4=Healer',
    `spell1`     INT UNSIGNED NOT NULL DEFAULT 0,
    `spell2`     INT UNSIGNED NOT NULL DEFAULT 0,
    `spell3`     INT UNSIGNED NOT NULL DEFAULT 0,
    `cooldown1`  INT UNSIGNED NOT NULL DEFAULT 8000,
    `cooldown2`  INT UNSIGNED NOT NULL DEFAULT 12000,
    `cooldown3`  INT UNSIGNED NOT NULL DEFAULT 15000,
    PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Companion squad roster definitions';

USE `characters`;

DROP PROCEDURE IF EXISTS add_crafting_columns;
DELIMITER //
CREATE PROCEDURE add_crafting_columns()
BEGIN
    IF NOT EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'item_instance_modifiers' AND COLUMN_NAME = 'craftingModifiedStat1') THEN
        ALTER TABLE `item_instance_modifiers`
            ADD COLUMN `craftingModifiedStat1` INT(10) UNSIGNED DEFAULT 0 NULL AFTER `artifactKnowledgeLevel`,
            ADD COLUMN `craftingModifiedStat2` INT(10) UNSIGNED DEFAULT 0 NULL AFTER `craftingModifiedStat1`;
    END IF;
END //
DELIMITER ;
CALL add_crafting_columns();
DROP PROCEDURE IF EXISTS add_crafting_columns;

ALTER TABLE `character_pet` ADD COLUMN `favorite` tinyint unsigned NOT NULL DEFAULT '0' AFTER `specialization`;

-- Chromie Time Expansion
ALTER TABLE `characters` ADD COLUMN `chromieTimeExpansionId` tinyint unsigned NOT NULL DEFAULT '0' AFTER `transmogOutfitLocked`;

-- ============================================================================
-- WORLD DATABASE: companion_roster
-- ============================================================================
-- Run: mysql -u root -padmin world < "this_file.sql"
-- (Or apply the world section manually)

CREATE TABLE IF NOT EXISTS `companion_roster` (
    `entry`      INT UNSIGNED NOT NULL COMMENT 'creature_template entry',
    `name`       VARCHAR(64) NOT NULL,
    `role`       TINYINT UNSIGNED NOT NULL COMMENT '0=Tank,1=Melee,2=Ranged,3=Caster,4=Healer',
    `spell1`     INT UNSIGNED NOT NULL DEFAULT 0,
    `spell2`     INT UNSIGNED NOT NULL DEFAULT 0,
    `spell3`     INT UNSIGNED NOT NULL DEFAULT 0,
    `cooldown1`  INT UNSIGNED NOT NULL DEFAULT 8000,
    `cooldown2`  INT UNSIGNED NOT NULL DEFAULT 12000,
    `cooldown3`  INT UNSIGNED NOT NULL DEFAULT 15000,
    PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Companion squad roster definitions';


USE `auth`;
-- ============================================================================
-- WORLD DATABASE: companion_roster
-- ============================================================================
-- Run: mysql -u root -padmin world < "this_file.sql"
-- (Or apply the world section manually)

CREATE TABLE IF NOT EXISTS `companion_roster` (
    `entry`      INT UNSIGNED NOT NULL COMMENT 'creature_template entry',
    `name`       VARCHAR(64) NOT NULL,
    `role`       TINYINT UNSIGNED NOT NULL COMMENT '0=Tank,1=Melee,2=Ranged,3=Caster,4=Healer',
    `spell1`     INT UNSIGNED NOT NULL DEFAULT 0,
    `spell2`     INT UNSIGNED NOT NULL DEFAULT 0,
    `spell3`     INT UNSIGNED NOT NULL DEFAULT 0,
    `cooldown1`  INT UNSIGNED NOT NULL DEFAULT 8000,
    `cooldown2`  INT UNSIGNED NOT NULL DEFAULT 12000,
    `cooldown3`  INT UNSIGNED NOT NULL DEFAULT 15000,
    PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Companion squad roster definitions';


-- ============================================================================
-- Companion Squad System — Seed Data
-- Run against: world
-- Creates creature_template entries and companion_roster entries
-- creature_template entries use 500000+ range to avoid conflicts
-- ============================================================================

-- Companion creature templates (minimal — ScriptName is the key part)
-- These use entry range 500001-500010 for companion NPCs
-- HealthModifier/DamageModifier live in creature_template_difficulty, not creature_template
USE `world`;

INSERT IGNORE INTO `creature_template` (`entry`, `name`, `subname`, `ScriptName`, `faction`, `npcflag`, `unit_flags`, `BaseAttackTime`) VALUES
(500001, 'Companion Warrior',   'Tank',       'CompanionAI', 35, 0, 0, 2000),
(500002, 'Companion Rogue',     'Melee DPS',  'CompanionAI', 35, 0, 0, 2000),
(500003, 'Companion Hunter',    'Ranged DPS', 'CompanionAI', 35, 0, 0, 2000),
(500004, 'Companion Mage',      'Caster DPS', 'CompanionAI', 35, 0, 0, 2000),
(500005, 'Companion Priest',    'Healer',     'CompanionAI', 35, 0, 0, 2000);

-- Display models (required for creatures to be visible)
INSERT IGNORE INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`) VALUES
(500001, 0, 3258,  1, 1),   -- Warrior: Stormwind Guard
(500002, 0, 34761, 1, 1),   -- Rogue: SI:7 Agent
(500003, 0, 55028, 1, 1),   -- Hunter: Stormwind Mage (placeholder)
(500004, 0, 64084, 1, 1),   -- Mage: Stormwind Mage
(500005, 0, 18452, 1, 1);   -- Priest: Stormwind Mage (older model)

-- Difficulty entries for scaling (DifficultyID 0 = normal)
INSERT IGNORE INTO `creature_template_difficulty` (`Entry`, `DifficultyID`, `HealthModifier`, `DamageModifier`) VALUES
(500001, 0, 5.0, 1.0),
(500002, 0, 3.0, 1.5),
(500003, 0, 3.0, 1.2),
(500004, 0, 2.5, 1.3),
(500005, 0, 2.5, 0.5);

-- Equipment
INSERT IGNORE INTO `creature_equip_template` (`CreatureID`, `ID`, `ItemID1`, `AppearanceModID1`, `ItemVisual1`, `ItemID2`, `AppearanceModID2`, `ItemVisual2`, `ItemID3`, `AppearanceModID3`, `ItemVisual3`, `VerifiedBuild`) VALUES
(500001, 1, 1899, 0, 0, 143, 0, 0, 0, 0, 0, 0),    -- Warrior: Shortsword + Buckler
(500002, 1, 2704, 0, 0, 2704, 0, 0, 0, 0, 0, 0),   -- Rogue: dual axes (placeholder)
(500003, 1, 0, 0, 0, 0, 0, 0, 2551, 0, 0, 0),      -- Hunter: Bow (ranged)
(500004, 1, 868, 0, 0, 0, 0, 0, 0, 0, 0, 0),       -- Mage: Staff
(500005, 1, 2075, 0, 0, 0, 0, 0, 0, 0, 0, 0);      -- Priest: Mace

-- Roster entries with real spells
-- spell1 = primary, spell2 = secondary, spell3 = utility
REPLACE INTO `companion_roster` (`entry`, `name`, `role`, `spell1`, `spell2`, `spell3`, `cooldown1`, `cooldown2`, `cooldown3`) VALUES
(500001, 'Warrior',  0, 355, 23922, 29567, 8000, 6000, 8000),    -- Tank: Taunt, Shield Slam, Heroic Strike
(500002, 'Rogue',    1, 1752, 53, 0, 4000, 8000, 0),             -- Melee: Sinister Strike, Backstab
(500003, 'Hunter',   2, 6660, 0, 0, 3000, 0, 0),                 -- Ranged: Shoot
(500004, 'Mage',     3, 133, 116, 0, 4000, 5000, 0),             -- Caster: Fireball, Frostbolt
(500005, 'Priest',   4, 2061, 139, 0, 5000, 12000, 0);           -- Healer: Flash Heal, Renew-- ----- End file: 3.character_db.sql -----
-- ----- Begin file: 4.characters.sql -----
USE `characters`;

-- Idempotent crafting stat modifier columns (MySQL 8.0 compatible)
DROP PROCEDURE IF EXISTS add_crafting_columns;
DELIMITER //
CREATE PROCEDURE add_crafting_columns()
BEGIN
    IF NOT EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'item_instance_modifiers' AND COLUMN_NAME = 'craftingModifiedStat1') THEN
        ALTER TABLE `item_instance_modifiers`
            ADD COLUMN `craftingModifiedStat1` INT(10) UNSIGNED DEFAULT 0 NULL AFTER `artifactKnowledgeLevel`,
            ADD COLUMN `craftingModifiedStat2` INT(10) UNSIGNED DEFAULT 0 NULL AFTER `craftingModifiedStat1`;
    END IF;
END //
DELIMITER ;
CALL add_crafting_columns();
DROP PROCEDURE IF EXISTS add_crafting_columns;-- ----- End file: 4.characters.sql -----

-- ----- Begin file: 5.chromie_time.sql -----
-- Chromie Time: terrain swap conditions
-- ConditionType 60 = CONDITION_CHROMIE_TIME (value1: 0=any CT, N=specific expansion)
-- NegativeCondition=1 inverts: "NOT in CT for expansion N"
-- SourceTypeOrReferenceId=25 = CONDITION_SOURCE_TYPE_TERRAIN_SWAP
-- SourceEntry = terrain swap map ID
-- Multiple conditions with same ElseGroup are ANDed

-- Remove terrain swaps from defaults that need to be conditional
-- They will be re-added as conditional via the conditions table
-- Note: terrain_swap_defaults entries without conditions are always active
-- Entries WITH conditions are only active when ALL conditions are met

-- Blasted Lands WoD terrain (1190): should NOT be active for Classic/TBC/WotLK/Cata CT
-- (Blasted Lands was changed by WoD Iron Horde invasion — pre-WoD CT should see original)
USE `world`;
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=25 AND `SourceEntry`=1190;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
(25,0,1190,0,0,60,0,1,0,0,1,0,0,'','Blasted Lands WoD terrain: not active in TBC CT'),
(25,0,1190,0,0,60,0,2,0,0,1,0,0,'','Blasted Lands WoD terrain: not active in WotLK CT'),
(25,0,1190,0,0,60,0,3,0,0,1,0,0,'','Blasted Lands WoD terrain: not active in Cata CT');

-- Silithus: The Wound (1817): should NOT be active for pre-BfA CT (expansions 1-6)
-- (Silithus was changed by BfA sword of Sargeras event)
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=25 AND `SourceEntry`=1817;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
(25,0,1817,0,0,60,0,1,0,0,1,0,0,'','Silithus Wound terrain: not active in TBC CT'),
(25,0,1817,0,0,60,0,2,0,0,1,0,0,'','Silithus Wound terrain: not active in WotLK CT'),
(25,0,1817,0,0,60,0,3,0,0,1,0,0,'','Silithus Wound terrain: not active in Cata CT'),
(25,0,1817,0,0,60,0,4,0,0,1,0,0,'','Silithus Wound terrain: not active in MoP CT'),
(25,0,1817,0,0,60,0,5,0,0,1,0,0,'','Silithus Wound terrain: not active in WoD CT'),
(25,0,1817,0,0,60,0,6,0,0,1,0,0,'','Silithus Wound terrain: not active in Legion CT');

-- Stormwind Gunship Pandaria Start (1066): should NOT be active for pre-MoP CT (expansions 1-3)
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=25 AND `SourceEntry`=1066;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
(25,0,1066,0,0,60,0,1,0,0,1,0,0,'','SW Gunship MoP terrain: not active in TBC CT'),
(25,0,1066,0,0,60,0,2,0,0,1,0,0,'','SW Gunship MoP terrain: not active in WotLK CT'),
(25,0,1066,0,0,60,0,3,0,0,1,0,0,'','SW Gunship MoP terrain: not active in Cata CT');

-- Orgrimmar Gunship Pandaria Start (1074): should NOT be active for pre-MoP CT (expansions 1-3)
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=25 AND `SourceEntry`=1074;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
(25,0,1074,0,0,60,0,1,0,0,1,0,0,'','Org Gunship MoP terrain: not active in TBC CT'),
(25,0,1074,0,0,60,0,2,0,0,1,0,0,'','Org Gunship MoP terrain: not active in WotLK CT'),
(25,0,1074,0,0,60,0,3,0,0,1,0,0,'','Org Gunship MoP terrain: not active in Cata CT');

-- Twilight Highlands Dragonmaw Port (736): should NOT be active for pre-Cata CT (expansions 1-2)
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=25 AND `SourceEntry`=736;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
(25,0,736,0,0,60,0,1,0,0,1,0,0,'','TH Dragonmaw terrain: not active in TBC CT'),
(25,0,736,0,0,60,0,2,0,0,1,0,0,'','TH Dragonmaw terrain: not active in WotLK CT');

-- Mount Hyjal default terrain (719): should NOT be active for pre-Cata CT (expansions 1-2)
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=25 AND `SourceEntry`=719;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
(25,0,719,0,0,60,0,1,0,0,1,0,0,'','Hyjal terrain: not active in TBC CT'),
(25,0,719,0,0,60,0,2,0,0,1,0,0,'','Hyjal terrain: not active in WotLK CT');

-- ============================================================================
-- Chromie NPC (167032) gossip option: open Chromie Time UI
-- OptionNpc=40 (ChromieTimeNpc) triggers NPCInteractionOpenResult with
-- PlayerInteractionType::ChromieTime, which opens the client's CT expansion picker
-- ============================================================================
DELETE FROM `gossip_menu_option` WHERE `MenuID`=25426;
INSERT INTO `gossip_menu_option` (`MenuID`,`GossipOptionID`,`OptionID`,`OptionNpc`,`OptionText`,`OptionBroadcastTextID`,`Language`,`Flags`,`ActionMenuID`,`ActionPoiID`,`GossipNpcOptionID`,`BoxCoded`,`BoxMoney`,`BoxText`,`BoxBroadcastTextID`,`SpellID`,`OverrideIconID`,`VerifiedBuild`) VALUES
(25426,-250000,0,40,'I want to select a different timeline.',0,0,0,0,0,NULL,0,0,NULL,0,NULL,NULL,0);-- ----- End file: 5.chromie_time.sql -----
-- ----- Begin file: 6.darkmoon_farie.sql -----
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
-- ------------------------------ ----- End file: 6.darkmoon_farie.sql -----
