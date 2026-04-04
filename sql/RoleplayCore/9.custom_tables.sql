-- ============================================================================
-- VoxCore Custom Tables — Consolidated DDL
-- ============================================================================
-- Run this after any fresh TDB import to recreate all VoxCore-custom tables.
-- Order: auth → hotfixes (skip, already in TDB) → world → characters → roleplay
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
  `Entry` mediumint UNSIGNED NOT NULL DEFAULT 0,
  `Item` mediumint NOT NULL DEFAULT 0,
  `Reference` mediumint UNSIGNED NOT NULL DEFAULT 0,
  `Chance` float NOT NULL DEFAULT 100,
  `QuestRequired` tinyint(1) NOT NULL DEFAULT 0,
  `LootMode` smallint UNSIGNED NOT NULL DEFAULT 1,
  `GroupId` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `MinCount` int UNSIGNED NOT NULL DEFAULT 1,
  `MaxCount` int UNSIGNED NOT NULL DEFAULT 1,
  `Comment` varchar(255) CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NULL DEFAULT NULL,
  PRIMARY KEY (`Entry`, `Item`) USING BTREE
) ENGINE=MyISAM CHARACTER SET=utf8mb3 COLLATE=utf8mb3_general_ci ROW_FORMAT=FIXED;

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
