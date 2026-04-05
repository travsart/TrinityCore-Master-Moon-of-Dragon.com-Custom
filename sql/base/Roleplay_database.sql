-- VoxCore Roleplay Database — Base Schema
-- Safe for re-application: uses CREATE TABLE IF NOT EXISTS (no data wipe)

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!50503 SET NAMES utf8mb4 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Table structure for table `bestiary_aggregated`
--
USE `roleplay`;
CREATE TABLE IF NOT EXISTS `bestiary_aggregated` (
  `creature_entry` int unsigned NOT NULL,
  `spell_id` int unsigned NOT NULL,
  `cast_count` int unsigned NOT NULL DEFAULT '0',
  `last_reporter` varchar(64) NOT NULL DEFAULT '',
  `last_seen` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`creature_entry`,`spell_id`),
  KEY `idx_creature` (`creature_entry`),
  KEY `idx_spell` (`spell_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='BestiaryForge multi-player aggregated spell discoveries';

--
-- Table structure for table `creature_extra`
--

CREATE TABLE IF NOT EXISTS `creature_extra` (
  `guid` bigint unsigned NOT NULL,
  `scale` float NOT NULL DEFAULT '-1',
  `id_creator_bnet` int unsigned NOT NULL DEFAULT '0',
  `id_creator_player` bigint unsigned NOT NULL DEFAULT '0',
  `id_modifier_bnet` int unsigned NOT NULL DEFAULT '0',
  `id_modifier_player` bigint unsigned NOT NULL DEFAULT '0',
  `created` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `modified` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `phaseMask` int unsigned NOT NULL DEFAULT '1',
  `displayLock` tinyint unsigned NOT NULL DEFAULT '0',
  `displayId` int unsigned NOT NULL DEFAULT '0',
  `nativeDisplayId` int unsigned NOT NULL DEFAULT '0',
  `genderLock` tinyint unsigned NOT NULL DEFAULT '0',
  `gender` tinyint unsigned NOT NULL DEFAULT '0',
  `swim` tinyint unsigned NOT NULL DEFAULT '1',
  `gravity` tinyint unsigned NOT NULL DEFAULT '1',
  `fly` tinyint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=DYNAMIC;

--
-- Table structure for table `creature_template_extra`
--

CREATE TABLE IF NOT EXISTS `creature_template_extra` (
  `id_entry` int unsigned NOT NULL,
  `disabled` tinyint NOT NULL DEFAULT '0',
  PRIMARY KEY (`id_entry`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=DYNAMIC;

--
-- Table structure for table `custom_npcs`
--

CREATE TABLE IF NOT EXISTS `custom_npcs` (
  `Key` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL,
  `Entry` int unsigned NOT NULL,
  PRIMARY KEY (`Key`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=DYNAMIC;

--
-- Table structure for table `player_morph`
--

CREATE TABLE IF NOT EXISTS `player_morph` (
  `playerGuid` bigint unsigned NOT NULL,
  `morphDisplayId` int unsigned NOT NULL DEFAULT '0',
  `scale` float NOT NULL DEFAULT '1',
  PRIMARY KEY (`playerGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Table structure for table `server_settings`
--

CREATE TABLE IF NOT EXISTS `server_settings` (
  `setting_name` varchar(50) NOT NULL,
  `setting_value` varchar(255) NOT NULL,
  PRIMARY KEY (`setting_name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=DYNAMIC;

--
-- Table structure for table `updates`
--

CREATE TABLE IF NOT EXISTS `updates` (
  `name` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'filename with extension of the update.',
  `hash` char(40) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT '' COMMENT 'sha1 hash of the sql file.',
  `state` enum('RELEASED','ARCHIVED') CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT 'RELEASED' COMMENT 'defines if an update is released or archived.',
  `timestamp` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'timestamp when the query was applied.',
  `speed` int unsigned NOT NULL DEFAULT '0' COMMENT 'time the query takes to apply in ms.',
  PRIMARY KEY (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='List of all applied updates in this database.';

--
-- Table structure for table `updates_include`
--

CREATE TABLE IF NOT EXISTS `updates_include` (
  `path` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'directory to include. $ means relative to the source directory.',
  `state` enum('RELEASED','ARCHIVED') CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT 'RELEASED' COMMENT 'defines if the directory contains released or archived updates.',
  PRIMARY KEY (`path`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='List of directories where we want to include sql updates.';

--
-- Seed data for table `updates_include`
--

INSERT IGNORE INTO `updates_include` VALUES
('$/sql/updates/roleplay','RELEASED');

/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;
