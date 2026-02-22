-- MySQL dump 10.13  Distrib 8.0.19, for Win64 (x86_64)
--
-- Host: localhost    Database: playerbot
-- ------------------------------------------------------
-- Server version	9.4.0

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
-- Table structure for table `battle_pet_ability`
--
use `playerbot`;
DROP TABLE IF EXISTS `battle_pet_ability`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `battle_pet_ability` (
  `abilityId` int unsigned NOT NULL COMMENT 'Ability ID',
  `name` varchar(64) COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '' COMMENT 'Ability name',
  `petType` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Pet type (0=Humanoid, 1=Dragonkin, etc.)',
  `baseDamage` int unsigned NOT NULL DEFAULT '0' COMMENT 'Base damage value',
  `cooldownDuration` int unsigned NOT NULL DEFAULT '0' COMMENT 'Cooldown in rounds',
  `flags` int unsigned NOT NULL DEFAULT '0' COMMENT 'Ability flags (bit 0=multi-turn)',
  `effectCount` tinyint unsigned NOT NULL DEFAULT '1' COMMENT 'Number of effects',
  `visualId` int unsigned NOT NULL DEFAULT '0' COMMENT 'Visual effect ID',
  PRIMARY KEY (`abilityId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `battle_pet_ability`
--

LOCK TABLES `battle_pet_ability` WRITE;
/*!40000 ALTER TABLE `battle_pet_ability` DISABLE KEYS */;
/*!40000 ALTER TABLE `battle_pet_ability` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `battle_pet_species`
--

DROP TABLE IF EXISTS `battle_pet_species`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `battle_pet_species` (
  `speciesId` int unsigned NOT NULL COMMENT 'Battle Pet Species ID',
  `creatureId` int unsigned NOT NULL DEFAULT '0' COMMENT 'Reference to creature_template.entry',
  `petType` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Pet family: 0=Humanoid, 1=Dragonkin, 2=Flying, 3=Undead, 4=Critter, 5=Magic, 6=Elemental, 7=Beast, 8=Aquatic, 9=Mechanical',
  `flags` int unsigned NOT NULL DEFAULT '0' COMMENT 'Flags: 0x1=Capturable, 0x2=Tradeable, 0x4=Rare, 0x8=Epic, 0x10=Legendary, 0x20=Untradeable',
  `source` varchar(50) DEFAULT '' COMMENT 'How pet is obtained: wild, vendor, quest, achievement, drop, etc.',
  `description` varchar(255) DEFAULT '' COMMENT 'Optional description',
  PRIMARY KEY (`speciesId`),
  KEY `idx_creature_id` (`creatureId`),
  KEY `idx_pet_type` (`petType`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Battle pet species metadata for bot pet selection';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `battle_pet_species`
--

LOCK TABLES `battle_pet_species` WRITE;
/*!40000 ALTER TABLE `battle_pet_species` DISABLE KEYS */;
INSERT INTO `battle_pet_species` VALUES (39,61071,4,3,'wild','Squirrel - Common critter found everywhere'),(40,61072,4,3,'wild','Fawn - Deer companion'),(41,61073,4,3,'wild','Mouse - Small critter'),(42,61074,4,3,'wild','Rabbit - Fast critter'),(43,61075,4,3,'wild','Skunk - Stinky critter'),(44,61076,4,3,'wild','Prairie Dog - Plains critter'),(45,61077,4,3,'wild','Rat - Common vermin'),(46,61078,4,3,'wild','Roach - Cockroach'),(47,61079,4,3,'wild','Beetle - Crawling bug'),(48,61080,4,3,'wild','Black Rat - Dark colored rat'),(49,61081,4,3,'wild','Adder - Small snake'),(50,61082,4,3,'wild','Marmot - Mountain critter'),(51,61083,4,5,'wild','Armadillo Pup - Armored critter'),(52,61084,4,3,'wild','Toad - Amphibian'),(53,61085,4,3,'wild','Frog - Green frog'),(100,62178,7,3,'wild','Cat - Feline companion'),(101,62179,7,3,'wild','Snow Cub - White cat'),(102,62180,7,3,'wild','Panther Cub - Jungle cat'),(103,62181,7,3,'wild','Cheetah Cub - Fast cat'),(104,62182,7,3,'wild','Black Tabby Cat - Dark cat'),(105,62183,7,3,'wild','Worg Pup - Wolf puppy'),(106,62184,7,3,'wild','Darkhound - Dark wolf'),(107,62185,7,3,'wild','Fox Kit - Young fox'),(108,62186,7,3,'wild','Alpine Fox Kit - Mountain fox'),(109,62187,7,5,'wild','Baby Ape - Primate'),(110,62188,7,3,'wild','Spider - Common spider'),(111,62189,7,3,'wild','Scorpid - Desert scorpion'),(112,62190,7,3,'wild','Snake - Common serpent'),(113,62191,7,5,'wild','Leopard Tree Frog - Spotted frog'),(114,62192,7,3,'wild','Gazelle Fawn - Plains deer'),(200,63098,2,3,'wild','Parrot - Colorful bird'),(201,63099,2,3,'wild','Cockatiel - Small parrot'),(202,63100,2,3,'wild','Owl - Nocturnal bird'),(203,63101,2,3,'wild','Hawk - Bird of prey'),(204,63102,2,5,'wild','Crow - Black bird'),(205,63103,2,3,'wild','Seagull - Coastal bird'),(206,63104,2,3,'wild','Dragonhawk Hatchling - Baby dragonhawk'),(207,63105,2,3,'wild','Moth - Nocturnal flyer'),(208,63106,2,3,'wild','Firefly - Glowing insect'),(209,63107,2,5,'wild','Bat - Cave dweller'),(210,63108,2,3,'wild','Crane - Wading bird'),(211,63109,2,3,'wild','Chicken - Farm bird'),(212,63110,2,3,'wild','Prairie Chicken - Wild fowl'),(213,63111,2,5,'wild','Turkey - Large bird'),(214,63112,2,3,'wild','Tiny Flamefly - Fire elemental insect'),(300,64018,8,3,'wild','Frog - Common frog'),(301,64019,8,3,'wild','Small Frog - Tiny frog'),(302,64020,8,3,'wild','Tree Frog - Tree dweller'),(303,64021,8,3,'wild','Crab - Crustacean'),(304,64022,8,3,'wild','Shore Crab - Coastal crab'),(305,64023,8,5,'wild','Strand Crab - Beach crab'),(306,64024,8,3,'wild','Sea Pony - Seahorse'),(307,64025,8,3,'wild','Tiny Goldfish - Small fish'),(308,64026,8,5,'wild','Pengu - Penguin'),(309,64027,8,3,'wild','Strand Crawler - Beach crawler'),(310,64028,8,3,'wild','Snail - Gastropod'),(311,64029,8,3,'wild','Turtle - Shelled reptile'),(312,64030,8,3,'wild','Softshell Snapling - Young turtle'),(313,64031,8,5,'wild','Salty - Sea creature'),(314,64032,8,3,'wild','Otter Pup - Young otter'),(400,65038,6,5,'wild','Tiny Twister - Wind elemental'),(401,65039,6,5,'wild','Tainted Waveling - Water elemental'),(402,65040,6,5,'wild','Ashstone Core - Fire elemental'),(403,65041,6,5,'wild','Ruby Sapling - Earth elemental'),(404,65042,6,5,'wild','Tiny Snowman - Ice elemental'),(405,65043,6,5,'wild','Singing Sunflower - Plant elemental'),(406,65044,6,5,'wild','Withers - Corrupted tree'),(407,65045,6,5,'wild','Teldrassil Sproutling - Tree spirit'),(408,65046,6,5,'wild','Ashleaf Spriteling - Nature spirit'),(409,65047,6,5,'wild','Crimson Lasher - Thorny plant'),(410,65048,6,5,'wild','Amethyst Shale Hatchling - Crystal being'),(411,65049,6,5,'wild','Topaz Shale Hatchling - Crystal being'),(412,65050,6,5,'wild','Emerald Shale Hatchling - Crystal being'),(413,65051,6,9,'rare','Terrible Turnip - Legendary plant'),(414,65052,6,5,'wild','Lava Beetle - Fire bug'),(500,66058,9,5,'vendor','Mechanical Squirrel - Gnomish creation'),(501,66059,9,5,'vendor','Lifelike Mechanical Toad - Engineering pet'),(502,66060,9,5,'vendor','Mechanical Chicken - Clucking robot'),(503,66061,9,5,'quest','Lil XT - Miniature war machine'),(504,66062,9,9,'rare','Personal World Destroyer - Rare mech'),(505,66063,9,5,'vendor','Clockwork Rocket Bot - Rocket powered'),(506,66064,9,5,'vendor','Rocket Bot - Small rocket'),(507,66065,9,5,'engineering','Tranquil Mechanical Yeti - Yeti robot'),(508,66066,9,5,'engineering','De-Weaponized Mechanical Companion - Peace bot'),(509,66067,9,9,'rare','Darkmoon Zeppelin - Darkmoon prize'),(510,66068,9,5,'engineering','Mechanical Pandaren Dragonling - Dragon bot'),(511,66069,9,5,'engineering','Sunreaver Micro-Sentry - Horde bot'),(512,66070,9,5,'vendor','Anodized Robo Cub - Bear bot'),(513,66071,9,5,'vendor','Mechanical Axebeak - Bird bot'),(514,66072,9,5,'engineering','Iron Starlette - Star robot'),(600,67078,5,5,'rare','Mana Wyrmling - Magical serpent'),(601,67079,5,5,'wild','Arcane Eye - Floating eye'),(602,67080,5,5,'rare','Jade Owl - Mystical bird'),(603,67081,5,5,'rare','Sapphire Cub - Blue cat'),(604,67082,5,5,'rare','Enchanted Broom - Animated cleaning'),(605,67083,5,5,'vendor','Enchanted Lantern - Magical light'),(606,67084,5,5,'vendor','Magic Lamp - Wishing lamp'),(607,67085,5,5,'quest','Lunar Lantern - Moon light'),(608,67086,5,5,'quest','Festival Lantern - Celebration lamp'),(609,67087,5,9,'rare','Sprite Darter Hatchling - Fey dragon'),(610,67088,5,5,'wild','Disgusting Oozeling - Magic slime'),(611,67089,5,5,'quest','Mini Mindslayer - Tentacle creature'),(612,67090,5,5,'rare','Ethereal Soul-Trader - Spirit merchant'),(613,67091,5,5,'rare','Lesser Voidcaller - Small void'),(614,67092,5,13,'legendary','Netherspace Abyssal - Legendary void'),(700,68098,3,5,'rare','Ghostly Skull - Floating skull'),(701,68099,3,5,'wild','Ghost - Spectral being'),(702,68100,3,5,'wild','Spirit Crab - Undead crab'),(703,68101,3,5,'wild','Restless Shadeling - Dark spirit'),(704,68102,3,5,'rare','Fossilized Hatchling - Skeleton pet'),(705,68103,3,9,'rare','Creepy Crate - Animated box'),(706,68104,3,5,'vendor','Crawling Claw - Undead hand'),(707,68105,3,5,'rare','Infected Squirrel - Zombie squirrel'),(708,68106,3,5,'rare','Mr. Bigglesworth - Liches cat'),(709,68107,3,9,'rare','Stitched Pup - Zombie dog'),(710,68108,3,5,'rare','Sen Jin Fetish - Voodoo doll'),(711,68109,3,5,'rare','Vampiric Batling - Vampire bat'),(712,68110,3,5,'vendor','Blighthawk - Undead bird'),(713,68111,3,13,'legendary','Lil Deathwing - Mini dragon'),(714,68112,3,5,'wild','Infected Fawn - Zombie deer'),(800,69118,1,5,'rare','Whelpling - Baby dragon'),(801,69119,1,9,'rare','Azure Whelpling - Blue dragon'),(802,69120,1,9,'rare','Crimson Whelpling - Red dragon'),(803,69121,1,9,'rare','Dark Whelpling - Black dragon'),(804,69122,1,9,'rare','Emerald Whelpling - Green dragon'),(805,69123,1,13,'legendary','Sprite Darter - Fey dragon'),(806,69124,1,5,'quest','Nether Faerie Dragon - Outland dragon'),(807,69125,1,5,'vendor','Wild Jade Hatchling - Jade dragon'),(808,69126,1,5,'vendor','Wild Golden Hatchling - Gold dragon'),(809,69127,1,5,'vendor','Wild Crimson Hatchling - Red dragon'),(810,69128,1,5,'rare','Infinite Whelpling - Bronze dragon'),(811,69129,1,5,'rare','Nexus Whelpling - Arcane dragon'),(812,69130,1,5,'vendor','Onyx Whelpling - Black dragon'),(813,69131,1,9,'rare','Thundertail Flapper - Storm dragon'),(814,69132,1,13,'legendary','Yu-lon - Celestial dragon'),(900,70138,0,5,'vendor','Winter Veil Helper - Holiday helper'),(901,70139,0,5,'vendor','Father Winter Helper - Santa helper'),(902,70140,0,5,'rare','Moonkin Hatchling - Baby moonkin'),(903,70141,0,5,'quest','Pandaren Monk - Martial artist'),(904,70142,0,5,'rare','Gregarious Grell - Fel imp'),(905,70143,0,5,'vendor','Curious Oracle Hatchling - Young oracle'),(906,70144,0,5,'vendor','Curious Wolvar Pup - Young wolvar'),(907,70145,0,5,'rare','Kun-Lai Runt - Yeti'),(908,70146,0,5,'rare','Flayer Youngling - Scalding pet'),(909,70147,0,9,'rare','Anubisath Idol - Egyptian statue'),(910,70148,0,5,'vendor','Sporeling Sprout - Mushroom person'),(911,70149,0,13,'legendary','Mini Tyrael - Angel'),(912,70150,0,5,'vendor','Grindgear Toy - Goblin robot'),(913,70151,0,5,'rare','Hopling - Baby hozen'),(914,70152,0,5,'rare','Bonkers - Mad monkey');
/*!40000 ALTER TABLE `battle_pet_species` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `battle_pet_species_abilities`
--

DROP TABLE IF EXISTS `battle_pet_species_abilities`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `battle_pet_species_abilities` (
  `speciesId` int unsigned NOT NULL COMMENT 'Battle pet species ID',
  `abilityId1` int unsigned NOT NULL DEFAULT '0' COMMENT 'First ability slot',
  `abilityId2` int unsigned NOT NULL DEFAULT '0' COMMENT 'Second ability slot',
  `abilityId3` int unsigned NOT NULL DEFAULT '0' COMMENT 'Third ability slot',
  `abilityId4` int unsigned NOT NULL DEFAULT '0' COMMENT 'Fourth ability slot (alternate)',
  `abilityId5` int unsigned NOT NULL DEFAULT '0' COMMENT 'Fifth ability slot (alternate)',
  `abilityId6` int unsigned NOT NULL DEFAULT '0' COMMENT 'Sixth ability slot (alternate)',
  PRIMARY KEY (`speciesId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `battle_pet_species_abilities`
--

LOCK TABLES `battle_pet_species_abilities` WRITE;
/*!40000 ALTER TABLE `battle_pet_species_abilities` DISABLE KEYS */;
/*!40000 ALTER TABLE `battle_pet_species_abilities` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `playerbot_accounts`
--

DROP TABLE IF EXISTS `playerbot_accounts`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_accounts` (
  `account_id` int unsigned NOT NULL,
  `account_name` varchar(32) COLLATE utf8mb4_unicode_ci NOT NULL,
  `email` varchar(64) COLLATE utf8mb4_unicode_ci NOT NULL,
  `created_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `is_bot_account` tinyint(1) NOT NULL DEFAULT '1',
  `bot_count` tinyint unsigned NOT NULL DEFAULT '0',
  `max_bots` tinyint unsigned NOT NULL DEFAULT '10',
  `account_status` enum('ACTIVE','SUSPENDED','DISABLED') COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT 'ACTIVE',
  `last_activity` timestamp NULL DEFAULT NULL,
  PRIMARY KEY (`account_id`),
  UNIQUE KEY `uk_account_name` (`account_name`),
  UNIQUE KEY `uk_email` (`email`),
  KEY `idx_bot_accounts` (`is_bot_account`,`account_status`),
  KEY `idx_activity` (`last_activity`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Bot account management';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `playerbot_accounts`
--

LOCK TABLES `playerbot_accounts` WRITE;
/*!40000 ALTER TABLE `playerbot_accounts` DISABLE KEYS */;
/*!40000 ALTER TABLE `playerbot_accounts` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `playerbot_activity_patterns`
--

DROP TABLE IF EXISTS `playerbot_activity_patterns`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_activity_patterns` (
  `pattern_name` varchar(64) COLLATE utf8mb4_unicode_ci NOT NULL,
  `display_name` varchar(128) COLLATE utf8mb4_unicode_ci NOT NULL,
  `description` text COLLATE utf8mb4_unicode_ci,
  `is_system_pattern` tinyint(1) NOT NULL DEFAULT '0',
  `login_chance` float NOT NULL DEFAULT '1',
  `logout_chance` float NOT NULL DEFAULT '1',
  `min_session_duration` int unsigned NOT NULL DEFAULT '1800',
  `max_session_duration` int unsigned NOT NULL DEFAULT '7200',
  `min_offline_duration` int unsigned NOT NULL DEFAULT '3600',
  `max_offline_duration` int unsigned NOT NULL DEFAULT '28800',
  `activity_weight` float NOT NULL DEFAULT '1',
  `enabled` tinyint(1) NOT NULL DEFAULT '1',
  `created_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`pattern_name`),
  KEY `idx_system_patterns` (`is_system_pattern`,`enabled`),
  KEY `idx_pattern_weight` (`activity_weight`,`enabled`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `playerbot_activity_patterns`
--

LOCK TABLES `playerbot_activity_patterns` WRITE;
/*!40000 ALTER TABLE `playerbot_activity_patterns` DISABLE KEYS */;
INSERT INTO `playerbot_activity_patterns` VALUES ('achievement_hunter','Achievement Hunter','Players focused on achievements',1,0.7,0.7,3600,12600,2700,10800,1.8,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),('casual_high','Casual High Activity','Casual players with high online time',1,0.8,0.7,3600,14400,1800,7200,2,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),('casual_low','Casual Low Activity','Casual players with lower activity',1,0.4,0.9,900,3600,7200,28800,4,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),('casual_medium','Casual Medium Activity','Average casual players',1,0.6,0.8,1800,7200,3600,14400,3,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),('debug_long','Debug Long Sessions','Extended sessions for testing',1,1,0.1,14400,43200,1800,7200,0.1,0,'2025-10-08 17:30:47','2025-10-08 17:30:47'),('debug_short','Debug Short Sessions','Short sessions for testing',1,1,1,300,900,300,1800,0.1,0,'2025-10-08 17:30:47','2025-10-08 17:30:47'),('evening_player','Evening Player','Players active mainly in evenings',1,0.7,0.75,2700,10800,4500,18000,2.5,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),('explorer','Explorer','Players who enjoy exploring the world',1,0.6,0.8,2400,9600,4500,18000,2.3,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),('hardcore','Hardcore Player','Dedicated players with very high activity',1,0.95,0.3,7200,28800,900,3600,1.5,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),('occasional','Occasional Player','Players who log in occasionally',1,0.3,0.95,600,2400,14400,86400,3.5,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),('power_leveler','Power Leveler','Players focused on rapid progression',1,0.9,0.4,5400,18000,1800,7200,1.2,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),('pve_focused','PvE Focused','Players primarily interested in PvE',1,0.75,0.65,3600,12600,2700,10800,1.7,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),('pvp_focused','PvP Focused','Players primarily interested in PvP',1,0.8,0.6,2700,10800,3600,14400,1.6,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),('social_player','Social Player','Players focused on social aspects',1,0.65,0.8,2400,9600,3600,14400,2.2,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),('trial_player','Trial Player','New players trying the game',1,0.5,0.85,1200,4800,5400,21600,3,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),('weekend_warrior','Weekend Warrior','High activity on weekends, low on weekdays',1,0.9,0.6,5400,21600,2700,10800,1.8,1,'2025-10-08 17:30:47','2025-10-08 17:30:47');
/*!40000 ALTER TABLE `playerbot_activity_patterns` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `playerbot_character_distribution`
--

DROP TABLE IF EXISTS `playerbot_character_distribution`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_character_distribution` (
  `race` tinyint unsigned NOT NULL,
  `class` tinyint unsigned NOT NULL,
  `faction` tinyint unsigned NOT NULL,
  `target_percentage` float NOT NULL DEFAULT '0',
  `current_count` int unsigned NOT NULL DEFAULT '0',
  `max_count` int unsigned NOT NULL DEFAULT '1000',
  `is_enabled` tinyint(1) NOT NULL DEFAULT '1',
  `priority_weight` float NOT NULL DEFAULT '1',
  `last_created` timestamp NULL DEFAULT NULL,
  `created_today` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`race`,`class`),
  KEY `idx_faction` (`faction`,`is_enabled`),
  KEY `idx_distribution` (`target_percentage`,`current_count`),
  KEY `idx_priority` (`priority_weight`,`is_enabled`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Character race/class distribution control';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `playerbot_character_distribution`
--

LOCK TABLES `playerbot_character_distribution` WRITE;
/*!40000 ALTER TABLE `playerbot_character_distribution` DISABLE KEYS */;
/*!40000 ALTER TABLE `playerbot_character_distribution` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `playerbot_lifecycle_events`
--

DROP TABLE IF EXISTS `playerbot_lifecycle_events`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_lifecycle_events` (
  `event_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `event_timestamp` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `event_category` enum('SCHEDULER','SPAWNER','SESSION','DATABASE','SYSTEM','ERROR') COLLATE utf8mb4_unicode_ci NOT NULL,
  `event_type` varchar(50) COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'Specific event type',
  `severity` enum('DEBUG','INFO','WARNING','ERROR','CRITICAL') COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT 'INFO',
  `component` varchar(50) COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'Component that generated event',
  `bot_guid` int unsigned DEFAULT NULL COMMENT 'Related bot (if applicable)',
  `account_id` int unsigned DEFAULT NULL COMMENT 'Related account (if applicable)',
  `message` text COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'Human-readable event description',
  `details` json DEFAULT NULL COMMENT 'Structured event data',
  `execution_time_ms` int unsigned DEFAULT NULL COMMENT 'Time to process operation',
  `memory_usage_mb` float DEFAULT NULL COMMENT 'Memory usage at time of event',
  `active_bots_count` int unsigned DEFAULT NULL COMMENT 'Number of active bots',
  `correlation_id` varchar(36) COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT 'UUID for related events',
  `parent_event_id` bigint unsigned DEFAULT NULL COMMENT 'Parent event reference',
  PRIMARY KEY (`event_id`),
  KEY `idx_timestamp` (`event_timestamp`),
  KEY `idx_category_type` (`event_category`,`event_type`),
  KEY `idx_severity` (`severity`,`event_timestamp`),
  KEY `idx_component` (`component`,`event_timestamp`),
  KEY `idx_bot_events` (`bot_guid`,`event_timestamp`),
  KEY `idx_correlation` (`correlation_id`),
  KEY `idx_parent_child` (`parent_event_id`),
  CONSTRAINT `playerbot_lifecycle_events_ibfk_1` FOREIGN KEY (`parent_event_id`) REFERENCES `playerbot_lifecycle_events` (`event_id`) ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='System-wide lifecycle events for monitoring and debugging';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `playerbot_lifecycle_events`
--

LOCK TABLES `playerbot_lifecycle_events` WRITE;
/*!40000 ALTER TABLE `playerbot_lifecycle_events` DISABLE KEYS */;
/*!40000 ALTER TABLE `playerbot_lifecycle_events` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `playerbot_migrations`
--

DROP TABLE IF EXISTS `playerbot_migrations`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_migrations` (
  `version` varchar(20) COLLATE utf8mb4_unicode_ci NOT NULL,
  `description` varchar(255) COLLATE utf8mb4_unicode_ci NOT NULL,
  `applied_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `checksum` varchar(64) COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `execution_time_ms` int unsigned DEFAULT '0',
  PRIMARY KEY (`version`),
  KEY `idx_applied` (`applied_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Database migration tracking';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `playerbot_migrations`
--

LOCK TABLES `playerbot_migrations` WRITE;
/*!40000 ALTER TABLE `playerbot_migrations` DISABLE KEYS */;
INSERT INTO `playerbot_migrations` VALUES ('001','Initial schema','2025-10-08 15:47:10','4746698005981500221',9),('002','Account management','2025-10-08 15:47:10','1158484387088916919',1),('003','Lifecycle management','2025-10-08 15:47:10','14635397018373423315',9),('004','Character distribution','2025-10-08 15:47:10','11745451986299579065',7),('005','Initial data','2025-10-08 15:47:10','15054733545134552835',14),('006','Bot names','2025-10-08 15:47:10','17822103708217178016',53);
/*!40000 ALTER TABLE `playerbot_migrations` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `playerbot_schedules`
--

DROP TABLE IF EXISTS `playerbot_schedules`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_schedules` (
  `bot_guid` int unsigned NOT NULL,
  `pattern_name` varchar(50) COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT 'default',
  `next_login` timestamp NULL DEFAULT NULL COMMENT 'When bot should next login',
  `next_logout` timestamp NULL DEFAULT NULL COMMENT 'When bot should logout',
  `last_activity` timestamp NULL DEFAULT NULL COMMENT 'Last recorded activity',
  `last_calculation` timestamp NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'When schedule was calculated',
  `total_sessions` int unsigned NOT NULL DEFAULT '0' COMMENT 'Total login sessions',
  `total_playtime` int unsigned NOT NULL DEFAULT '0' COMMENT 'Total seconds played',
  `current_session_start` timestamp NULL DEFAULT NULL COMMENT 'Current session start time',
  `is_scheduled` tinyint(1) NOT NULL DEFAULT '0' COMMENT 'Bot has active schedule',
  `is_active` tinyint(1) NOT NULL DEFAULT '0' COMMENT 'Bot is currently logged in',
  `schedule_priority` tinyint unsigned NOT NULL DEFAULT '5' COMMENT 'Schedule priority (1-10)',
  `consecutive_failures` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Failed login attempts',
  `last_failure_reason` varchar(100) COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT 'Last failure description',
  `next_retry` timestamp NULL DEFAULT NULL COMMENT 'When to retry after failure',
  `created_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`bot_guid`),
  KEY `idx_next_login` (`next_login`),
  KEY `idx_next_logout` (`next_logout`),
  KEY `idx_active_schedules` (`is_scheduled`,`is_active`),
  KEY `idx_pattern` (`pattern_name`),
  KEY `idx_priority` (`schedule_priority`,`next_login`),
  KEY `idx_failures` (`consecutive_failures`,`next_retry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Individual bot schedule state and timing';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `playerbot_schedules`
--

LOCK TABLES `playerbot_schedules` WRITE;
/*!40000 ALTER TABLE `playerbot_schedules` DISABLE KEYS */;
/*!40000 ALTER TABLE `playerbot_schedules` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `playerbot_spawn_log`
--

DROP TABLE IF EXISTS `playerbot_spawn_log`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_spawn_log` (
  `log_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `bot_guid` int unsigned NOT NULL,
  `account_id` int unsigned NOT NULL,
  `event_type` enum('SPAWN_REQUEST','SPAWN_SUCCESS','SPAWN_FAILURE','DESPAWN_SCHEDULED','DESPAWN_FORCED','DESPAWN_ERROR') COLLATE utf8mb4_unicode_ci NOT NULL,
  `event_timestamp` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `map_id` int unsigned DEFAULT NULL COMMENT 'Map where event occurred',
  `zone_id` int unsigned DEFAULT NULL COMMENT 'Zone where event occurred',
  `area_id` int unsigned DEFAULT NULL COMMENT 'Area where event occurred',
  `position_x` float DEFAULT NULL,
  `position_y` float DEFAULT NULL,
  `position_z` float DEFAULT NULL,
  `reason` varchar(200) COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT 'Reason for spawn/despawn',
  `initiator` enum('SCHEDULER','SPAWNER','ADMIN','SYSTEM','PLAYER') COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT 'SYSTEM',
  `pattern_name` varchar(50) COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT 'Activity pattern used',
  `processing_time_ms` int unsigned DEFAULT NULL COMMENT 'Time to process event',
  `queue_wait_time_ms` int unsigned DEFAULT NULL COMMENT 'Time waiting in queue',
  `extra_data` json DEFAULT NULL COMMENT 'Additional event-specific data',
  PRIMARY KEY (`log_id`),
  KEY `idx_bot_events` (`bot_guid`,`event_timestamp`),
  KEY `idx_event_type` (`event_type`,`event_timestamp`),
  KEY `idx_location` (`map_id`,`zone_id`,`event_timestamp`),
  KEY `idx_pattern_events` (`pattern_name`,`event_timestamp`),
  KEY `idx_initiator` (`initiator`,`event_timestamp`),
  KEY `idx_timestamp` (`event_timestamp`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Historical log of bot spawn and despawn events';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `playerbot_spawn_log`
--

LOCK TABLES `playerbot_spawn_log` WRITE;
/*!40000 ALTER TABLE `playerbot_spawn_log` DISABLE KEYS */;
/*!40000 ALTER TABLE `playerbot_spawn_log` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `playerbot_talent_loadouts`
--

DROP TABLE IF EXISTS `playerbot_talent_loadouts`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_talent_loadouts` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `class_id` tinyint unsigned NOT NULL COMMENT 'Class ID (1-13)',
  `spec_id` tinyint unsigned NOT NULL COMMENT 'Specialization Index (0-3 per class)',
  `min_level` tinyint unsigned NOT NULL COMMENT 'Minimum level for this loadout',
  `max_level` tinyint unsigned NOT NULL COMMENT 'Maximum level for this loadout',
  `talent_string` text NOT NULL COMMENT 'Comma-separated talent entry IDs',
  `hero_talent_string` text COMMENT 'Comma-separated hero talent entry IDs (71+)',
  `description` varchar(255) DEFAULT NULL COMMENT 'Loadout description',
  `created_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  KEY `idx_class_spec` (`class_id`,`spec_id`),
  KEY `idx_level` (`min_level`,`max_level`)
) ENGINE=InnoDB AUTO_INCREMENT=313 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Talent loadouts for automated bot creation';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `playerbot_talent_loadouts`
--

LOCK TABLES `playerbot_talent_loadouts` WRITE;
/*!40000 ALTER TABLE `playerbot_talent_loadouts` DISABLE KEYS */;
INSERT INTO `playerbot_talent_loadouts` VALUES (1,1,0,1,10,'22624,22360,22371,22548,22392,22394,22397',NULL,'Arms Warrior 1-10: Single-target burst with Execute mastery','2025-11-06 20:24:00','2025-11-06 20:24:00'),(2,1,0,11,20,'22624,22360,22371,22548,22392,22394,22397',NULL,'Arms Warrior 11-20: Single-target burst with Execute mastery','2025-11-06 20:24:00','2025-11-06 20:24:00'),(3,1,0,21,30,'22624,22360,22371,22548,22392,22394,22397',NULL,'Arms Warrior 21-30: Single-target burst with Execute mastery','2025-11-06 20:24:00','2025-11-06 20:24:00'),(4,1,0,31,40,'22624,22360,22371,22548,22392,22394,22397',NULL,'Arms Warrior 31-40: Single-target burst with Execute mastery','2025-11-06 20:24:00','2025-11-06 20:24:00'),(5,1,0,41,50,'22624,22360,22371,22548,22392,22394,22397',NULL,'Arms Warrior 41-50: Single-target burst with Execute mastery','2025-11-06 20:24:00','2025-11-06 20:24:00'),(6,1,0,51,60,'22624,22360,22371,22548,22392,22394,22397',NULL,'Arms Warrior 51-60: Single-target burst with Execute mastery','2025-11-06 20:24:00','2025-11-06 20:24:00'),(7,1,0,61,70,'22624,22360,22371,22548,22392,22394,22397',NULL,'Arms Warrior 61-70: Single-target burst with Execute mastery','2025-11-06 20:24:00','2025-11-06 20:24:00'),(8,1,0,71,80,'22624,22360,22371,22548,22392,22394,22397','','Arms Warrior 71-80: Raid build with Hero Talents (Colossus/Slayer)','2025-11-06 20:24:00','2025-11-06 20:24:00'),(9,1,1,1,10,'22632,22491,22489,22490,22627',NULL,'Fury Warrior 1-10: High sustain with self-healing','2025-11-06 20:24:00','2025-11-06 20:24:00'),(10,1,1,11,20,'22632,22491,22489,22490,22627',NULL,'Fury Warrior 11-20: High sustain with self-healing','2025-11-06 20:24:00','2025-11-06 20:24:00'),(11,1,1,21,30,'22632,22491,22489,22490,22627',NULL,'Fury Warrior 21-30: High sustain with self-healing','2025-11-06 20:24:00','2025-11-06 20:24:00'),(12,1,1,31,40,'22632,22491,22489,22490,22627',NULL,'Fury Warrior 31-40: High sustain with self-healing','2025-11-06 20:24:00','2025-11-06 20:24:00'),(13,1,1,41,50,'22632,22491,22489,22490,22627',NULL,'Fury Warrior 41-50: High sustain with self-healing','2025-11-06 20:24:00','2025-11-06 20:24:00'),(14,1,1,51,60,'22632,22491,22489,22490,22627',NULL,'Fury Warrior 51-60: High sustain with self-healing','2025-11-06 20:24:00','2025-11-06 20:24:00'),(15,1,1,61,70,'22632,22491,22489,22490,22627',NULL,'Fury Warrior 61-70: High sustain with self-healing','2025-11-06 20:24:00','2025-11-06 20:24:00'),(16,1,1,71,80,'22632,22633,22491,22489,22490,22627,22628','','Fury Warrior 71-80: Reckless Abandon for sustained DPS (Colossus/Slayer)','2025-11-06 20:24:00','2025-11-06 20:24:00'),(17,1,2,1,10,'15759',NULL,'Protection Warrior 1-10: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(18,1,2,11,20,'15759,15774,16038,16039,19676,22409,22629,16040,22378,22626,23260,16041,22488,22627,23096,16042,22384,22631,22800,15764,22395',NULL,'Protection Warrior 11-20: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(19,1,2,21,30,'15759,15774,16038,16039,19676,22409,22629,16040,22378,22626,23260,16041,22488,22627,23096,16042,22384,22631,22800,15764,22395,22401,22544,22406,23099,23455',NULL,'Protection Warrior 21-30: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(20,1,2,31,40,'15759,15774,16038,16039,19676,22409,22629,16040,22378,22626,23260,16041,22488,22627,23096,16042,22384,22631,22800,15764,22395,22401,22544,22406,23099,23455',NULL,'Protection Warrior 31-40: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(21,1,2,41,50,'15759,15774,16038,16039,19676,22409,22629,16040,22378,22626,23260,16041,22488,22627,23096,16042,22384,22631,22800,15764,22395,22401,22544,22406,23099,23455',NULL,'Protection Warrior 41-50: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(22,1,2,51,60,'15759,15774,16038,16039,19676,22409,22629,16040,22378,22626,23260,16041,22488,22627,23096,16042,22384,22631,22800,15764,22395,22401,22544,22406,23099,23455',NULL,'Protection Warrior 51-60: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(23,1,2,61,70,'15759,15774,16038,16039,19676,22409,22629,16040,22378,22626,23260,16041,22488,22627,23096,16042,22384,22631,22800,15764,22395,22401,22544,22406,23099,23455',NULL,'Protection Warrior 61-70: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(24,1,2,71,80,'15759,15774,16038,16039,19676,22409,22629,16040,22378,22626,23260,16041,22488,22627,23096,16042,22384,22631,22800,15764,22395,22401,22544,22406,23099,23455','','Protection Warrior 71-80: Requires manual talent curation (Colossus/Mountain Thane)','2025-11-06 20:24:01','2025-12-19 14:54:49'),(25,2,0,1,10,'17565',NULL,'Holy Paladin 1-10: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(26,2,0,11,20,'17565,17567,17569,17571,17575,17577,17579,22176,17587,21811,22179,22180,17593,17595,22433,22434,17597,17599,17603,17611,22484',NULL,'Holy Paladin 11-20: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(27,2,0,21,30,'17565,17567,17569,17571,17575,17577,17579,22176,17587,21811,22179,22180,17593,17595,22433,22434,17597,17599,17603,17611,22484,23191,23680,21201,21671,23681',NULL,'Holy Paladin 21-30: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(28,2,0,31,40,'17565,17567,17569,17571,17575,17577,17579,22176,17587,21811,22179,22180,17593,17595,22433,22434,17597,17599,17603,17611,22484,23191,23680,21201,21671,23681',NULL,'Holy Paladin 31-40: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(29,2,0,41,50,'17565,17567,17569,17571,17575,17577,17579,22176,17587,21811,22179,22180,17593,17595,22433,22434,17597,17599,17603,17611,22484,23191,23680,21201,21671,23681',NULL,'Holy Paladin 41-50: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(30,2,0,51,60,'17565,17567,17569,17571,17575,17577,17579,22176,17587,21811,22179,22180,17593,17595,22433,22434,17597,17599,17603,17611,22484,23191,23680,21201,21671,23681',NULL,'Holy Paladin 51-60: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(31,2,0,61,70,'17565,17567,17569,17571,17575,17577,17579,22176,17587,21811,22179,22180,17593,17595,22433,22434,17597,17599,17603,17611,22484,23191,23680,21201,21671,23681',NULL,'Holy Paladin 61-70: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(32,2,0,71,80,'17565,17567,17569,17571,17575,17577,17579,22176,17587,21811,22179,22180,17593,17595,22433,22434,17597,17599,17603,17611,22484,23191,23680,21201,21671,23681','','Holy Paladin 71-80: Requires manual talent curation (Lightsmith/Herald of the Sun)','2025-11-06 20:24:01','2025-12-19 14:54:49'),(33,2,1,1,10,'17569',NULL,'Protection Paladin 1-10: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(34,2,1,11,20,'17569,17571,22428,22558,23469,17577,17579,22431,22604,23468,17587,21811,22179,22180,17595,22433,22434,22435,17597,17599,17603',NULL,'Protection Paladin 11-20: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(35,2,1,21,30,'17569,17571,22428,22558,23469,17577,17579,22431,22604,23468,17587,21811,22179,22180,17595,22433,22434,22435,17597,17599,17603,17611,22189,22438,23087,21202,22645,23457',NULL,'Protection Paladin 21-30: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(36,2,1,31,40,'17569,17571,22428,22558,23469,17577,17579,22431,22604,23468,17587,21811,22179,22180,17595,22433,22434,22435,17597,17599,17603,17611,22189,22438,23087,21202,22645,23457',NULL,'Protection Paladin 31-40: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(37,2,1,41,50,'17569,17571,22428,22558,23469,17577,17579,22431,22604,23468,17587,21811,22179,22180,17595,22433,22434,22435,17597,17599,17603,17611,22189,22438,23087,21202,22645,23457',NULL,'Protection Paladin 41-50: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(38,2,1,51,60,'17569,17571,22428,22558,23469,17577,17579,22431,22604,23468,17587,21811,22179,22180,17595,22433,22434,22435,17597,17599,17603,17611,22189,22438,23087,21202,22645,23457',NULL,'Protection Paladin 51-60: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(39,2,1,61,70,'17569,17571,22428,22558,23469,17577,17579,22431,22604,23468,17587,21811,22179,22180,17595,22433,22434,22435,17597,17599,17603,17611,22189,22438,23087,21202,22645,23457',NULL,'Protection Paladin 61-70: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(40,2,1,71,80,'17569,17571,22428,22558,23469,17577,17579,22431,22604,23468,17587,21811,22179,22180,17595,22433,22434,22435,17597,17599,17603,17611,22189,22438,23087,21202,22645,23457','','Protection Paladin 71-80: Requires manual talent curation (Lightsmith/Templar)','2025-11-06 20:24:01','2025-12-19 14:54:49'),(41,2,2,1,10,'22586,22587,22591,22592,23110,23111,23112',NULL,'Retribution Paladin 1-10: Templar Verdict with Wake of Ashes','2025-11-06 20:24:01','2025-11-06 20:24:01'),(42,2,2,11,20,'22586,22587,22591,22592,23110,23111,23112',NULL,'Retribution Paladin 11-20: Templar Verdict with Wake of Ashes','2025-11-06 20:24:01','2025-11-06 20:24:01'),(43,2,2,21,30,'22586,22587,22591,22592,23110,23111,23112',NULL,'Retribution Paladin 21-30: Templar Verdict with Wake of Ashes','2025-11-06 20:24:01','2025-11-06 20:24:01'),(44,2,2,31,40,'22586,22587,22591,22592,23110,23111,23112',NULL,'Retribution Paladin 31-40: Templar Verdict with Wake of Ashes','2025-11-06 20:24:01','2025-11-06 20:24:01'),(45,2,2,41,50,'22586,22587,22591,22592,23110,23111,23112',NULL,'Retribution Paladin 41-50: Templar Verdict with Wake of Ashes','2025-11-06 20:24:01','2025-11-06 20:24:01'),(46,2,2,51,60,'22586,22587,22591,22592,23110,23111,23112',NULL,'Retribution Paladin 51-60: Templar Verdict with Wake of Ashes','2025-11-06 20:24:01','2025-11-06 20:24:01'),(47,2,2,61,70,'22586,22587,22591,22592,23110,23111,23112',NULL,'Retribution Paladin 61-70: Templar Verdict with Wake of Ashes','2025-11-06 20:24:01','2025-11-06 20:24:01'),(48,2,2,71,80,'22586,22587,22591,22592,23110,23111,23112','','Retribution Paladin 71-80: Raid build (Herald of the Sun/Templar)','2025-11-06 20:24:01','2025-11-06 20:24:01'),(49,3,0,1,10,'22276,22277,22286,22287',NULL,'Beast Mastery Hunter 1-10: Pet tanking with ranged safety','2025-11-06 20:24:01','2025-11-06 20:24:01'),(50,3,0,11,20,'22276,22277,22286,22287',NULL,'Beast Mastery Hunter 11-20: Pet tanking with ranged safety','2025-11-06 20:24:01','2025-11-06 20:24:01'),(51,3,0,21,30,'22276,22277,22286,22287',NULL,'Beast Mastery Hunter 21-30: Pet tanking with ranged safety','2025-11-06 20:24:01','2025-11-06 20:24:01'),(52,3,0,31,40,'22276,22277,22286,22287',NULL,'Beast Mastery Hunter 31-40: Pet tanking with ranged safety','2025-11-06 20:24:01','2025-11-06 20:24:01'),(53,3,0,41,50,'22276,22277,22286,22287',NULL,'Beast Mastery Hunter 41-50: Pet tanking with ranged safety','2025-11-06 20:24:01','2025-11-06 20:24:01'),(54,3,0,51,60,'22276,22277,22286,22287',NULL,'Beast Mastery Hunter 51-60: Pet tanking with ranged safety','2025-11-06 20:24:01','2025-11-06 20:24:01'),(55,3,0,61,70,'22276,22277,22286,22287',NULL,'Beast Mastery Hunter 61-70: Pet tanking with ranged safety','2025-11-06 20:24:01','2025-11-06 20:24:01'),(56,3,0,71,80,'22276,22277,22286,22287','','Beast Mastery Hunter 71-80: Raid build (Dark Ranger/Pack Leader)','2025-11-06 20:24:01','2025-11-06 20:24:01'),(57,3,1,1,10,'22279,22280,22289,22290,23260,23261,23262',NULL,'Marksmanship Hunter 1-10: Aimed Shot with Serpent Sting','2025-11-06 20:24:01','2025-11-06 20:24:01'),(58,3,1,11,20,'22279,22280,22289,22290,23260,23261,23262',NULL,'Marksmanship Hunter 11-20: Aimed Shot with Serpent Sting','2025-11-06 20:24:01','2025-11-06 20:24:01'),(59,3,1,21,30,'22279,22280,22289,22290,23260,23261,23262',NULL,'Marksmanship Hunter 21-30: Aimed Shot with Serpent Sting','2025-11-06 20:24:01','2025-11-06 20:24:01'),(60,3,1,31,40,'22279,22280,22289,22290,23260,23261,23262',NULL,'Marksmanship Hunter 31-40: Aimed Shot with Serpent Sting','2025-11-06 20:24:01','2025-11-06 20:24:01'),(61,3,1,41,50,'22279,22280,22289,22290,23260,23261,23262',NULL,'Marksmanship Hunter 41-50: Aimed Shot with Serpent Sting','2025-11-06 20:24:01','2025-11-06 20:24:01'),(62,3,1,51,60,'22279,22280,22289,22290,23260,23261,23262',NULL,'Marksmanship Hunter 51-60: Aimed Shot with Serpent Sting','2025-11-06 20:24:01','2025-11-06 20:24:01'),(63,3,1,61,70,'22279,22280,22289,22290,23260,23261,23262',NULL,'Marksmanship Hunter 61-70: Aimed Shot with Serpent Sting','2025-11-06 20:24:01','2025-11-06 20:24:01'),(64,3,1,71,80,'22279,22280,22289,22290,23260,23261,23262','','Marksmanship Hunter 71-80: Raid build (Dark Ranger/Sentinel)','2025-11-06 20:24:01','2025-11-06 20:24:01'),(65,3,2,1,10,'22275',NULL,'Survival Hunter 1-10: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(66,3,2,11,20,'22275,22283,22296,21997,22297,22769,19347,19348,23100,19361,22277,22299,22268,22276,22499,22271,22278,22300,22272,22301,23105',NULL,'Survival Hunter 11-20: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(67,3,2,21,30,'22275,22283,22296,21997,22297,22769,19347,19348,23100,19361,22277,22299,22268,22276,22499,22271,22278,22300,22272,22301,23105',NULL,'Survival Hunter 21-30: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(68,3,2,31,40,'22275,22283,22296,21997,22297,22769,19347,19348,23100,19361,22277,22299,22268,22276,22499,22271,22278,22300,22272,22301,23105',NULL,'Survival Hunter 31-40: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(69,3,2,41,50,'22275,22283,22296,21997,22297,22769,19347,19348,23100,19361,22277,22299,22268,22276,22499,22271,22278,22300,22272,22301,23105',NULL,'Survival Hunter 41-50: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(70,3,2,51,60,'22275,22283,22296,21997,22297,22769,19347,19348,23100,19361,22277,22299,22268,22276,22499,22271,22278,22300,22272,22301,23105',NULL,'Survival Hunter 51-60: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(71,3,2,61,70,'22275,22283,22296,21997,22297,22769,19347,19348,23100,19361,22277,22299,22268,22276,22499,22271,22278,22300,22272,22301,23105',NULL,'Survival Hunter 61-70: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(72,3,2,71,80,'22275,22283,22296,21997,22297,22769,19347,19348,23100,19361,22277,22299,22268,22276,22499,22271,22278,22300,22272,22301,23105','','Survival Hunter 71-80: Requires manual talent curation (Pack Leader/Sentinel)','2025-11-06 20:24:01','2025-12-19 14:54:49'),(73,4,0,1,10,'22113,22114,22121,22122,23174,23172,23171',NULL,'Assassination Rogue 1-10: Poison-focused with Deathmark burst','2025-11-06 20:24:01','2025-11-06 20:24:01'),(74,4,0,11,20,'22113,22114,22121,22122,23174,23172,23171',NULL,'Assassination Rogue 11-20: Poison-focused with Deathmark burst','2025-11-06 20:24:01','2025-11-06 20:24:01'),(75,4,0,21,30,'22113,22114,22121,22122,23174,23172,23171',NULL,'Assassination Rogue 21-30: Poison-focused with Deathmark burst','2025-11-06 20:24:01','2025-11-06 20:24:01'),(76,4,0,31,40,'22113,22114,22121,22122,23174,23172,23171',NULL,'Assassination Rogue 31-40: Poison-focused with Deathmark burst','2025-11-06 20:24:01','2025-11-06 20:24:01'),(77,4,0,41,50,'22113,22114,22121,22122,23174,23172,23171',NULL,'Assassination Rogue 41-50: Poison-focused with Deathmark burst','2025-11-06 20:24:01','2025-11-06 20:24:01'),(78,4,0,51,60,'22113,22114,22121,22122,23174,23172,23171',NULL,'Assassination Rogue 51-60: Poison-focused with Deathmark burst','2025-11-06 20:24:01','2025-11-06 20:24:01'),(79,4,0,61,70,'22113,22114,22121,22122,23174,23172,23171',NULL,'Assassination Rogue 61-70: Poison-focused with Deathmark burst','2025-11-06 20:24:01','2025-11-06 20:24:01'),(80,4,0,71,80,'22113,22114,22121,22122,23174,23172,23171','','Assassination Rogue 71-80: Raid build (Deathstalker/Fatebound)','2025-11-06 20:24:01','2025-11-06 20:24:01'),(81,4,1,1,10,'22138,22139,22155,22156,23175,23176,23177',NULL,'Outlaw Rogue 1-10: Roll the Bones with Slice and Dice','2025-11-06 20:24:01','2025-11-06 20:24:01'),(82,4,1,11,20,'22138,22139,22155,22156,23175,23176,23177',NULL,'Outlaw Rogue 11-20: Roll the Bones with Slice and Dice','2025-11-06 20:24:01','2025-11-06 20:24:01'),(83,4,1,21,30,'22138,22139,22155,22156,23175,23176,23177',NULL,'Outlaw Rogue 21-30: Roll the Bones with Slice and Dice','2025-11-06 20:24:01','2025-11-06 20:24:01'),(84,4,1,31,40,'22138,22139,22155,22156,23175,23176,23177',NULL,'Outlaw Rogue 31-40: Roll the Bones with Slice and Dice','2025-11-06 20:24:01','2025-11-06 20:24:01'),(85,4,1,41,50,'22138,22139,22155,22156,23175,23176,23177',NULL,'Outlaw Rogue 41-50: Roll the Bones with Slice and Dice','2025-11-06 20:24:01','2025-11-06 20:24:01'),(86,4,1,51,60,'22138,22139,22155,22156,23175,23176,23177',NULL,'Outlaw Rogue 51-60: Roll the Bones with Slice and Dice','2025-11-06 20:24:01','2025-11-06 20:24:01'),(87,4,1,61,70,'22138,22139,22155,22156,23175,23176,23177',NULL,'Outlaw Rogue 61-70: Roll the Bones with Slice and Dice','2025-11-06 20:24:01','2025-11-06 20:24:01'),(88,4,1,71,80,'22138,22139,22155,22156,23175,23176,23177','','Outlaw Rogue 71-80: Raid build (Fatebound/Trickster)','2025-11-06 20:24:01','2025-11-06 20:24:01'),(89,4,2,1,10,'19233',NULL,'Subtlety Rogue 1-10: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(90,4,2,11,20,'19233,19234,19235,22338,22331,22332,22333,19239,19240,19241,22122,22123,22128,22115,23036,23078,19249,22335,22336,21188,22132',NULL,'Subtlety Rogue 11-20: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(91,4,2,21,30,'19233,19234,19235,22338,22331,22332,22333,19239,19240,19241,22122,22123,22128,22115,23036,23078,19249,22335,22336,21188,22132,23183',NULL,'Subtlety Rogue 21-30: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(92,4,2,31,40,'19233,19234,19235,22338,22331,22332,22333,19239,19240,19241,22122,22123,22128,22115,23036,23078,19249,22335,22336,21188,22132,23183',NULL,'Subtlety Rogue 31-40: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(93,4,2,41,50,'19233,19234,19235,22338,22331,22332,22333,19239,19240,19241,22122,22123,22128,22115,23036,23078,19249,22335,22336,21188,22132,23183',NULL,'Subtlety Rogue 41-50: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(94,4,2,51,60,'19233,19234,19235,22338,22331,22332,22333,19239,19240,19241,22122,22123,22128,22115,23036,23078,19249,22335,22336,21188,22132,23183',NULL,'Subtlety Rogue 51-60: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(95,4,2,61,70,'19233,19234,19235,22338,22331,22332,22333,19239,19240,19241,22122,22123,22128,22115,23036,23078,19249,22335,22336,21188,22132,23183',NULL,'Subtlety Rogue 61-70: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(96,4,2,71,80,'19233,19234,19235,22338,22331,22332,22333,19239,19240,19241,22122,22123,22128,22115,23036,23078,19249,22335,22336,21188,22132,23183','','Subtlety Rogue 71-80: Requires manual talent curation (Deathstalker/Trickster)','2025-11-06 20:24:01','2025-12-19 14:54:50'),(97,5,0,1,10,'19752',NULL,'Discipline Priest 1-10: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(98,5,0,11,20,'19752,22313,22329,19758,22315,22316,22094,22440,19759,19761,19769,19765,19766,22330,19760,19763,19767,22161,21183,21184',NULL,'Discipline Priest 11-20: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(99,5,0,21,30,'19752,22313,22329,19758,22315,22316,22094,22440,19759,19761,19769,19765,19766,22330,19760,19763,19767,22161,21183,21184',NULL,'Discipline Priest 21-30: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(100,5,0,31,40,'19752,22313,22329,19758,22315,22316,22094,22440,19759,19761,19769,19765,19766,22330,19760,19763,19767,22161,21183,21184',NULL,'Discipline Priest 31-40: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(101,5,0,41,50,'19752,22313,22329,19758,22315,22316,22094,22440,19759,19761,19769,19765,19766,22330,19760,19763,19767,22161,21183,21184',NULL,'Discipline Priest 41-50: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(102,5,0,51,60,'19752,22313,22329,19758,22315,22316,22094,22440,19759,19761,19769,19765,19766,22330,19760,19763,19767,22161,21183,21184',NULL,'Discipline Priest 51-60: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(103,5,0,61,70,'19752,22313,22329,19758,22315,22316,22094,22440,19759,19761,19769,19765,19766,22330,19760,19763,19767,22161,21183,21184',NULL,'Discipline Priest 61-70: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(104,5,0,71,80,'19752,22313,22329,19758,22315,22316,22094,22440,19759,19761,19769,19765,19766,22330,19760,19763,19767,22161,21183,21184','','Discipline Priest 71-80: Requires manual talent curation (Archon/Oracle)','2025-11-06 20:24:01','2025-12-19 14:54:50'),(105,5,1,1,10,'19752',NULL,'Holy Priest 1-10: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(106,5,1,11,20,'19752,19753,19754,22312,19758,22315,22325,22326,22094,22095,22440,22487,22562,19761,21750,21977,19764,19765,19766,21754,22327',NULL,'Holy Priest 11-20: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(107,5,1,21,30,'19752,19753,19754,22312,19758,22315,22325,22326,22094,22095,22440,22487,22562,19761,21750,21977,19764,19765,19766,21754,22327,19760,19763,19767,21184,21636,21644',NULL,'Holy Priest 21-30: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(108,5,1,31,40,'19752,19753,19754,22312,19758,22315,22325,22326,22094,22095,22440,22487,22562,19761,21750,21977,19764,19765,19766,21754,22327,19760,19763,19767,21184,21636,21644',NULL,'Holy Priest 31-40: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(109,5,1,41,50,'19752,19753,19754,22312,19758,22315,22325,22326,22094,22095,22440,22487,22562,19761,21750,21977,19764,19765,19766,21754,22327,19760,19763,19767,21184,21636,21644',NULL,'Holy Priest 41-50: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(110,5,1,51,60,'19752,19753,19754,22312,19758,22315,22325,22326,22094,22095,22440,22487,22562,19761,21750,21977,19764,19765,19766,21754,22327,19760,19763,19767,21184,21636,21644',NULL,'Holy Priest 51-60: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(111,5,1,61,70,'19752,19753,19754,22312,19758,22315,22325,22326,22094,22095,22440,22487,22562,19761,21750,21977,19764,19765,19766,21754,22327,19760,19763,19767,21184,21636,21644',NULL,'Holy Priest 61-70: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(112,5,1,71,80,'19752,19753,19754,22312,19758,22315,22325,22326,22094,22095,22440,22487,22562,19761,21750,21977,19764,19765,19766,21754,22327,19760,19763,19767,21184,21636,21644','','Holy Priest 71-80: Requires manual talent curation (Archon/Oracle)','2025-11-06 20:24:01','2025-12-19 14:54:50'),(113,5,2,1,10,'22312,22313,22314,22331,23046,23047,23048',NULL,'Shadow Priest 1-10: Void Eruption with Dark Ascension','2025-11-06 20:24:01','2025-11-06 20:24:01'),(114,5,2,11,20,'22312,22313,22314,22331,23046,23047,23048',NULL,'Shadow Priest 11-20: Void Eruption with Dark Ascension','2025-11-06 20:24:01','2025-11-06 20:24:01'),(115,5,2,21,30,'22312,22313,22314,22331,23046,23047,23048',NULL,'Shadow Priest 21-30: Void Eruption with Dark Ascension','2025-11-06 20:24:01','2025-11-06 20:24:01'),(116,5,2,31,40,'22312,22313,22314,22331,23046,23047,23048',NULL,'Shadow Priest 31-40: Void Eruption with Dark Ascension','2025-11-06 20:24:01','2025-11-06 20:24:01'),(117,5,2,41,50,'22312,22313,22314,22331,23046,23047,23048',NULL,'Shadow Priest 41-50: Void Eruption with Dark Ascension','2025-11-06 20:24:01','2025-11-06 20:24:01'),(118,5,2,51,60,'22312,22313,22314,22331,23046,23047,23048',NULL,'Shadow Priest 51-60: Void Eruption with Dark Ascension','2025-11-06 20:24:01','2025-11-06 20:24:01'),(119,5,2,61,70,'22312,22313,22314,22331,23046,23047,23048',NULL,'Shadow Priest 61-70: Void Eruption with Dark Ascension','2025-11-06 20:24:01','2025-11-06 20:24:01'),(120,5,2,71,80,'22312,22313,22314,22331,23046,23047,23048','','Shadow Priest 71-80: Raid build (Archon/Voidweaver)','2025-11-06 20:24:01','2025-11-06 20:24:01'),(121,6,0,1,10,'19165',NULL,'Blood Death Knight 1-10: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(122,6,0,11,20,'19165,19166,23454,19218,19219,19220,19221,22134,22135,22013,22014,22015,19226,19227,19228,23373,19230,19231,19232,21207,21208',NULL,'Blood Death Knight 11-20: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(123,6,0,21,30,'19165,19166,23454,19218,19219,19220,19221,22134,22135,22013,22014,22015,19226,19227,19228,23373,19230,19231,19232,21207,21208,21209',NULL,'Blood Death Knight 21-30: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(124,6,0,31,40,'19165,19166,23454,19218,19219,19220,19221,22134,22135,22013,22014,22015,19226,19227,19228,23373,19230,19231,19232,21207,21208,21209',NULL,'Blood Death Knight 31-40: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(125,6,0,41,50,'19165,19166,23454,19218,19219,19220,19221,22134,22135,22013,22014,22015,19226,19227,19228,23373,19230,19231,19232,21207,21208,21209',NULL,'Blood Death Knight 41-50: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(126,6,0,51,60,'19165,19166,23454,19218,19219,19220,19221,22134,22135,22013,22014,22015,19226,19227,19228,23373,19230,19231,19232,21207,21208,21209',NULL,'Blood Death Knight 51-60: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(127,6,0,61,70,'19165,19166,23454,19218,19219,19220,19221,22134,22135,22013,22014,22015,19226,19227,19228,23373,19230,19231,19232,21207,21208,21209',NULL,'Blood Death Knight 61-70: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(128,6,0,71,80,'19165,19166,23454,19218,19219,19220,19221,22134,22135,22013,22014,22015,19226,19227,19228,23373,19230,19231,19232,21207,21208,21209','','Blood Death Knight 71-80: Requires manual talent curation (Deathbringer/San\'layn)','2025-11-06 20:24:01','2025-12-19 14:54:49'),(129,6,1,1,10,'22016,22017,22020,22021,23093,23094,23092',NULL,'Frost Death Knight 1-10: Breath of Sindragosa build','2025-11-06 20:24:01','2025-11-06 20:24:01'),(130,6,1,11,20,'22016,22017,22020,22021,23093,23094,23092',NULL,'Frost Death Knight 11-20: Breath of Sindragosa build','2025-11-06 20:24:01','2025-11-06 20:24:01'),(131,6,1,21,30,'22016,22017,22020,22021,23093,23094,23092',NULL,'Frost Death Knight 21-30: Breath of Sindragosa build','2025-11-06 20:24:01','2025-11-06 20:24:01'),(132,6,1,31,40,'22016,22017,22020,22021,23093,23094,23092',NULL,'Frost Death Knight 31-40: Breath of Sindragosa build','2025-11-06 20:24:01','2025-11-06 20:24:01'),(133,6,1,41,50,'22016,22017,22020,22021,23093,23094,23092',NULL,'Frost Death Knight 41-50: Breath of Sindragosa build','2025-11-06 20:24:01','2025-11-06 20:24:01'),(134,6,1,51,60,'22016,22017,22020,22021,23093,23094,23092',NULL,'Frost Death Knight 51-60: Breath of Sindragosa build','2025-11-06 20:24:01','2025-11-06 20:24:01'),(135,6,1,61,70,'22016,22017,22020,22021,23093,23094,23092',NULL,'Frost Death Knight 61-70: Breath of Sindragosa build','2025-11-06 20:24:01','2025-11-06 20:24:01'),(136,6,1,71,80,'22016,22017,22020,22021,23093,23094,23092','','Frost Death Knight 71-80: Raid build (Deathbringer/Rider of the Apocalypse)','2025-11-06 20:24:01','2025-11-06 20:24:01'),(137,6,2,1,10,'22006,22007,22024,22025,23095,23096,23097',NULL,'Unholy Death Knight 1-10: Disease and minion focused','2025-11-06 20:24:01','2025-11-06 20:24:01'),(138,6,2,11,20,'22006,22007,22024,22025,23095,23096,23097',NULL,'Unholy Death Knight 11-20: Disease and minion focused','2025-11-06 20:24:01','2025-11-06 20:24:01'),(139,6,2,21,30,'22006,22007,22024,22025,23095,23096,23097',NULL,'Unholy Death Knight 21-30: Disease and minion focused','2025-11-06 20:24:01','2025-11-06 20:24:01'),(140,6,2,31,40,'22006,22007,22024,22025,23095,23096,23097',NULL,'Unholy Death Knight 31-40: Disease and minion focused','2025-11-06 20:24:01','2025-11-06 20:24:01'),(141,6,2,41,50,'22006,22007,22024,22025,23095,23096,23097',NULL,'Unholy Death Knight 41-50: Disease and minion focused','2025-11-06 20:24:01','2025-11-06 20:24:01'),(142,6,2,51,60,'22006,22007,22024,22025,23095,23096,23097',NULL,'Unholy Death Knight 51-60: Disease and minion focused','2025-11-06 20:24:01','2025-11-06 20:24:01'),(143,6,2,61,70,'22006,22007,22024,22025,23095,23096,23097',NULL,'Unholy Death Knight 61-70: Disease and minion focused','2025-11-06 20:24:01','2025-11-06 20:24:01'),(144,6,2,71,80,'22006,22007,22024,22025,23095,23096,23097','','Unholy Death Knight 71-80: Raid build (Rider of the Apocalypse/San\'layn)','2025-11-06 20:24:01','2025-11-06 20:24:01'),(145,7,0,1,10,'22356,22357,22358,22361,23066,23067,23068',NULL,'Elemental Shaman 1-10: Lava Burst with Elemental Blast','2025-11-06 20:24:01','2025-11-06 20:24:01'),(146,7,0,11,20,'22356,22357,22358,22361,23066,23067,23068',NULL,'Elemental Shaman 11-20: Lava Burst with Elemental Blast','2025-11-06 20:24:01','2025-11-06 20:24:01'),(147,7,0,21,30,'22356,22357,22358,22361,23066,23067,23068',NULL,'Elemental Shaman 21-30: Lava Burst with Elemental Blast','2025-11-06 20:24:01','2025-11-06 20:24:01'),(148,7,0,31,40,'22356,22357,22358,22361,23066,23067,23068',NULL,'Elemental Shaman 31-40: Lava Burst with Elemental Blast','2025-11-06 20:24:01','2025-11-06 20:24:01'),(149,7,0,41,50,'22356,22357,22358,22361,23066,23067,23068',NULL,'Elemental Shaman 41-50: Lava Burst with Elemental Blast','2025-11-06 20:24:01','2025-11-06 20:24:01'),(150,7,0,51,60,'22356,22357,22358,22361,23066,23067,23068',NULL,'Elemental Shaman 51-60: Lava Burst with Elemental Blast','2025-11-06 20:24:01','2025-11-06 20:24:01'),(151,7,0,61,70,'22356,22357,22358,22361,23066,23067,23068',NULL,'Elemental Shaman 61-70: Lava Burst with Elemental Blast','2025-11-06 20:24:01','2025-11-06 20:24:01'),(152,7,0,71,80,'22356,22357,22358,22361,23066,23067,23068','','Elemental Shaman 71-80: Raid build (Farseer/Stormbringer)','2025-11-06 20:24:01','2025-11-06 20:24:01'),(153,7,1,1,10,'22354,22355,22359,22360,23064,23065,23063',NULL,'Enhancement Shaman 1-10: Windfury with Doom Winds','2025-11-06 20:24:01','2025-11-06 20:24:01'),(154,7,1,11,20,'22354,22355,22359,22360,23064,23065,23063',NULL,'Enhancement Shaman 11-20: Windfury with Doom Winds','2025-11-06 20:24:01','2025-11-06 20:24:01'),(155,7,1,21,30,'22354,22355,22359,22360,23064,23065,23063',NULL,'Enhancement Shaman 21-30: Windfury with Doom Winds','2025-11-06 20:24:01','2025-11-06 20:24:01'),(156,7,1,31,40,'22354,22355,22359,22360,23064,23065,23063',NULL,'Enhancement Shaman 31-40: Windfury with Doom Winds','2025-11-06 20:24:01','2025-11-06 20:24:01'),(157,7,1,41,50,'22354,22355,22359,22360,23064,23065,23063',NULL,'Enhancement Shaman 41-50: Windfury with Doom Winds','2025-11-06 20:24:01','2025-11-06 20:24:01'),(158,7,1,51,60,'22354,22355,22359,22360,23064,23065,23063',NULL,'Enhancement Shaman 51-60: Windfury with Doom Winds','2025-11-06 20:24:01','2025-11-06 20:24:01'),(159,7,1,61,70,'22354,22355,22359,22360,23064,23065,23063',NULL,'Enhancement Shaman 61-70: Windfury with Doom Winds','2025-11-06 20:24:01','2025-11-06 20:24:01'),(160,7,1,71,80,'22354,22355,22359,22360,23064,23065,23063','','Enhancement Shaman 71-80: Raid build (Stormbringer/Totemic)','2025-11-06 20:24:01','2025-11-06 20:24:01'),(161,7,2,1,10,'19262',NULL,'Restoration Shaman 1-10: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(162,7,2,11,20,'19262,19263,19264,19259,21963,23461,19275,22127,23110,22152,22322,22323,19269,21966,22144,19265,21968,21971,21199,21969,22359',NULL,'Restoration Shaman 11-20: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(163,7,2,21,30,'19262,19263,19264,19259,21963,23461,19275,22127,23110,22152,22322,22323,19269,21966,22144,19265,21968,21971,21199,21969,22359',NULL,'Restoration Shaman 21-30: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(164,7,2,31,40,'19262,19263,19264,19259,21963,23461,19275,22127,23110,22152,22322,22323,19269,21966,22144,19265,21968,21971,21199,21969,22359',NULL,'Restoration Shaman 31-40: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(165,7,2,41,50,'19262,19263,19264,19259,21963,23461,19275,22127,23110,22152,22322,22323,19269,21966,22144,19265,21968,21971,21199,21969,22359',NULL,'Restoration Shaman 41-50: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(166,7,2,51,60,'19262,19263,19264,19259,21963,23461,19275,22127,23110,22152,22322,22323,19269,21966,22144,19265,21968,21971,21199,21969,22359',NULL,'Restoration Shaman 51-60: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(167,7,2,61,70,'19262,19263,19264,19259,21963,23461,19275,22127,23110,22152,22322,22323,19269,21966,22144,19265,21968,21971,21199,21969,22359',NULL,'Restoration Shaman 61-70: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(168,7,2,71,80,'19262,19263,19264,19259,21963,23461,19275,22127,23110,22152,22322,22323,19269,21966,22144,19265,21968,21971,21199,21969,22359','','Restoration Shaman 71-80: Requires manual talent curation (Farseer/Totemic)','2025-11-06 20:24:01','2025-12-19 14:54:50'),(169,8,0,1,10,'22443,22444,22464,23087,23088,23089,23090',NULL,'Arcane Mage 1-10: Arcane Surge burst window optimization','2025-11-06 20:24:01','2025-11-06 20:24:01'),(170,8,0,11,20,'22443,22444,22464,23087,23088,23089,23090',NULL,'Arcane Mage 11-20: Arcane Surge burst window optimization','2025-11-06 20:24:01','2025-11-06 20:24:01'),(171,8,0,21,30,'22443,22444,22464,23087,23088,23089,23090',NULL,'Arcane Mage 21-30: Arcane Surge burst window optimization','2025-11-06 20:24:01','2025-11-06 20:24:01'),(172,8,0,31,40,'22443,22444,22464,23087,23088,23089,23090',NULL,'Arcane Mage 31-40: Arcane Surge burst window optimization','2025-11-06 20:24:01','2025-11-06 20:24:01'),(173,8,0,41,50,'22443,22444,22464,23087,23088,23089,23090',NULL,'Arcane Mage 41-50: Arcane Surge burst window optimization','2025-11-06 20:24:01','2025-11-06 20:24:01'),(174,8,0,51,60,'22443,22444,22464,23087,23088,23089,23090',NULL,'Arcane Mage 51-60: Arcane Surge burst window optimization','2025-11-06 20:24:01','2025-11-06 20:24:01'),(175,8,0,61,70,'22443,22444,22464,23087,23088,23089,23090',NULL,'Arcane Mage 61-70: Arcane Surge burst window optimization','2025-11-06 20:24:01','2025-11-06 20:24:01'),(176,8,0,71,80,'22443,22444,22464,23087,23088,23089,23090','','Arcane Mage 71-80: Raid build (Spellslinger/Sunfury)','2025-11-06 20:24:01','2025-11-06 20:24:01'),(177,8,1,1,10,'22456,22458,22463,22465',NULL,'Fire Mage 1-10: AoE focused for quest efficiency','2025-11-06 20:24:01','2025-11-06 20:24:01'),(178,8,1,11,20,'22456,22458,22463,22465',NULL,'Fire Mage 11-20: AoE focused for quest efficiency','2025-11-06 20:24:01','2025-11-06 20:24:01'),(179,8,1,21,30,'22456,22458,22463,22465',NULL,'Fire Mage 21-30: AoE focused for quest efficiency','2025-11-06 20:24:01','2025-11-06 20:24:01'),(180,8,1,31,40,'22456,22458,22463,22465',NULL,'Fire Mage 31-40: AoE focused for quest efficiency','2025-11-06 20:24:01','2025-11-06 20:24:01'),(181,8,1,41,50,'22456,22458,22463,22465',NULL,'Fire Mage 41-50: AoE focused for quest efficiency','2025-11-06 20:24:01','2025-11-06 20:24:01'),(182,8,1,51,60,'22456,22458,22463,22465',NULL,'Fire Mage 51-60: AoE focused for quest efficiency','2025-11-06 20:24:01','2025-11-06 20:24:01'),(183,8,1,61,70,'22456,22458,22463,22465',NULL,'Fire Mage 61-70: AoE focused for quest efficiency','2025-11-06 20:24:01','2025-11-06 20:24:01'),(184,8,1,71,80,'22456,22458,22463,22465','','Fire Mage 71-80: Raid build (Frostfire/Sunfury)','2025-11-06 20:24:01','2025-11-06 20:24:01'),(185,8,2,1,10,'22446,22447,22448,23073,23074,23078,23091',NULL,'Frost Mage 1-10: Glacial Spike build for single-target','2025-11-06 20:24:01','2025-11-06 20:24:01'),(186,8,2,11,20,'22446,22447,22448,23073,23074,23078,23091',NULL,'Frost Mage 11-20: Glacial Spike build for single-target','2025-11-06 20:24:01','2025-11-06 20:24:01'),(187,8,2,21,30,'22446,22447,22448,23073,23074,23078,23091',NULL,'Frost Mage 21-30: Glacial Spike build for single-target','2025-11-06 20:24:01','2025-11-06 20:24:01'),(188,8,2,31,40,'22446,22447,22448,23073,23074,23078,23091',NULL,'Frost Mage 31-40: Glacial Spike build for single-target','2025-11-06 20:24:01','2025-11-06 20:24:01'),(189,8,2,41,50,'22446,22447,22448,23073,23074,23078,23091',NULL,'Frost Mage 41-50: Glacial Spike build for single-target','2025-11-06 20:24:01','2025-11-06 20:24:01'),(190,8,2,51,60,'22446,22447,22448,23073,23074,23078,23091',NULL,'Frost Mage 51-60: Glacial Spike build for single-target','2025-11-06 20:24:01','2025-11-06 20:24:01'),(191,8,2,61,70,'22446,22447,22448,23073,23074,23078,23091',NULL,'Frost Mage 61-70: Glacial Spike build for single-target','2025-11-06 20:24:01','2025-11-06 20:24:01'),(192,8,2,71,80,'22446,22447,22448,23073,23074,23078,23091','','Frost Mage 71-80: Raid build (Frostfire/Spellslinger)','2025-11-06 20:24:01','2025-11-06 20:24:01'),(193,9,0,1,10,'22039,22040,22041,22063,23137,23138,23139',NULL,'Affliction Warlock 1-10: Multi-DoT with Drain Soul execute','2025-11-06 20:24:01','2025-11-06 20:24:01'),(194,9,0,11,20,'22039,22040,22041,22063,23137,23138,23139',NULL,'Affliction Warlock 11-20: Multi-DoT with Drain Soul execute','2025-11-06 20:24:01','2025-11-06 20:24:01'),(195,9,0,21,30,'22039,22040,22041,22063,23137,23138,23139',NULL,'Affliction Warlock 21-30: Multi-DoT with Drain Soul execute','2025-11-06 20:24:01','2025-11-06 20:24:01'),(196,9,0,31,40,'22039,22040,22041,22063,23137,23138,23139',NULL,'Affliction Warlock 31-40: Multi-DoT with Drain Soul execute','2025-11-06 20:24:01','2025-11-06 20:24:01'),(197,9,0,41,50,'22039,22040,22041,22063,23137,23138,23139',NULL,'Affliction Warlock 41-50: Multi-DoT with Drain Soul execute','2025-11-06 20:24:01','2025-11-06 20:24:01'),(198,9,0,51,60,'22039,22040,22041,22063,23137,23138,23139',NULL,'Affliction Warlock 51-60: Multi-DoT with Drain Soul execute','2025-11-06 20:24:01','2025-11-06 20:24:01'),(199,9,0,61,70,'22039,22040,22041,22063,23137,23138,23139',NULL,'Affliction Warlock 61-70: Multi-DoT with Drain Soul execute','2025-11-06 20:24:01','2025-11-06 20:24:01'),(200,9,0,71,80,'22039,22040,22041,22063,23137,23138,23139','','Affliction Warlock 71-80: Raid build (Hellcaller/Soul Harvester)','2025-11-06 20:24:01','2025-11-06 20:24:01'),(201,9,1,1,10,'19290',NULL,'Demonology Warlock 1-10: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(202,9,1,11,20,'19290,22048,23138,21694,22045,23158,19280,19285,19286,19292,22042,22477,23160,19291,22047,23465,19295,21717,23146,23147,19284',NULL,'Demonology Warlock 11-20: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(203,9,1,21,30,'19290,22048,23138,21694,22045,23158,19280,19285,19286,19292,22042,22477,23160,19291,22047,23465,19295,21717,23146,23147,19284,22479,23091,23161',NULL,'Demonology Warlock 21-30: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(204,9,1,31,40,'19290,22048,23138,21694,22045,23158,19280,19285,19286,19292,22042,22477,23160,19291,22047,23465,19295,21717,23146,23147,19284,22479,23091,23161',NULL,'Demonology Warlock 31-40: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(205,9,1,41,50,'19290,22048,23138,21694,22045,23158,19280,19285,19286,19292,22042,22477,23160,19291,22047,23465,19295,21717,23146,23147,19284,22479,23091,23161',NULL,'Demonology Warlock 41-50: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(206,9,1,51,60,'19290,22048,23138,21694,22045,23158,19280,19285,19286,19292,22042,22477,23160,19291,22047,23465,19295,21717,23146,23147,19284,22479,23091,23161',NULL,'Demonology Warlock 51-60: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(207,9,1,61,70,'19290,22048,23138,21694,22045,23158,19280,19285,19286,19292,22042,22477,23160,19291,22047,23465,19295,21717,23146,23147,19284,22479,23091,23161',NULL,'Demonology Warlock 61-70: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(208,9,1,71,80,'19290,22048,23138,21694,22045,23158,19280,19285,19286,19292,22042,22477,23160,19291,22047,23465,19295,21717,23146,23147,19284,22479,23091,23161','','Demonology Warlock 71-80: Requires manual talent curation (Diabolist/Soul Harvester)','2025-11-06 20:24:01','2025-12-19 14:54:50'),(209,9,2,1,10,'22045,22046,22047,22069,23140,23141,23142',NULL,'Destruction Warlock 1-10: Chaos Bolt burst with Infernal','2025-11-06 20:24:01','2025-11-06 20:24:01'),(210,9,2,11,20,'22045,22046,22047,22069,23140,23141,23142',NULL,'Destruction Warlock 11-20: Chaos Bolt burst with Infernal','2025-11-06 20:24:01','2025-11-06 20:24:01'),(211,9,2,21,30,'22045,22046,22047,22069,23140,23141,23142',NULL,'Destruction Warlock 21-30: Chaos Bolt burst with Infernal','2025-11-06 20:24:01','2025-11-06 20:24:01'),(212,9,2,31,40,'22045,22046,22047,22069,23140,23141,23142',NULL,'Destruction Warlock 31-40: Chaos Bolt burst with Infernal','2025-11-06 20:24:01','2025-11-06 20:24:01'),(213,9,2,41,50,'22045,22046,22047,22069,23140,23141,23142',NULL,'Destruction Warlock 41-50: Chaos Bolt burst with Infernal','2025-11-06 20:24:01','2025-11-06 20:24:01'),(214,9,2,51,60,'22045,22046,22047,22069,23140,23141,23142',NULL,'Destruction Warlock 51-60: Chaos Bolt burst with Infernal','2025-11-06 20:24:01','2025-11-06 20:24:01'),(215,9,2,61,70,'22045,22046,22047,22069,23140,23141,23142',NULL,'Destruction Warlock 61-70: Chaos Bolt burst with Infernal','2025-11-06 20:24:01','2025-11-06 20:24:01'),(216,9,2,71,80,'22045,22046,22047,22069,23140,23141,23142','','Destruction Warlock 71-80: Raid build (Diabolist/Hellcaller)','2025-11-06 20:24:01','2025-11-06 20:24:01'),(217,10,0,1,10,'19820',NULL,'Brewmaster Monk 1-10: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(218,10,0,11,20,'19820,20185,23106,19302,19304,19818,19992,22097,22099,19993,19994,19995,20173,20174,20175,23363,19819,20184,22103,22104,22106',NULL,'Brewmaster Monk 11-20: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(219,10,0,21,30,'19820,20185,23106,19302,19304,19818,19992,22097,22099,19993,19994,19995,20173,20174,20175,23363,19819,20184,22103,22104,22106,22108',NULL,'Brewmaster Monk 21-30: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(220,10,0,31,40,'19820,20185,23106,19302,19304,19818,19992,22097,22099,19993,19994,19995,20173,20174,20175,23363,19819,20184,22103,22104,22106,22108',NULL,'Brewmaster Monk 31-40: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(221,10,0,41,50,'19820,20185,23106,19302,19304,19818,19992,22097,22099,19993,19994,19995,20173,20174,20175,23363,19819,20184,22103,22104,22106,22108',NULL,'Brewmaster Monk 41-50: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(222,10,0,51,60,'19820,20185,23106,19302,19304,19818,19992,22097,22099,19993,19994,19995,20173,20174,20175,23363,19819,20184,22103,22104,22106,22108',NULL,'Brewmaster Monk 51-60: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(223,10,0,61,70,'19820,20185,23106,19302,19304,19818,19992,22097,22099,19993,19994,19995,20173,20174,20175,23363,19819,20184,22103,22104,22106,22108',NULL,'Brewmaster Monk 61-70: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(224,10,0,71,80,'19820,20185,23106,19302,19304,19818,19992,22097,22099,19993,19994,19995,20173,20174,20175,23363,19819,20184,22103,22104,22106,22108','','Brewmaster Monk 71-80: Requires manual talent curation (Master of Harmony/Shado-Pan)','2025-11-06 20:24:01','2025-12-19 14:54:50'),(225,10,2,1,10,'22098,22099,22107,22108,23167,23168,23169',NULL,'Windwalker Monk 1-10: Strike of the Windlord with Serenity','2025-11-06 20:24:01','2025-11-06 20:24:01'),(226,10,2,11,20,'22098,22099,22107,22108,23167,23168,23169',NULL,'Windwalker Monk 11-20: Strike of the Windlord with Serenity','2025-11-06 20:24:01','2025-11-06 20:24:01'),(227,10,2,21,30,'22098,22099,22107,22108,23167,23168,23169',NULL,'Windwalker Monk 21-30: Strike of the Windlord with Serenity','2025-11-06 20:24:01','2025-11-06 20:24:01'),(228,10,2,31,40,'22098,22099,22107,22108,23167,23168,23169',NULL,'Windwalker Monk 31-40: Strike of the Windlord with Serenity','2025-11-06 20:24:01','2025-11-06 20:24:01'),(229,10,2,41,50,'22098,22099,22107,22108,23167,23168,23169',NULL,'Windwalker Monk 41-50: Strike of the Windlord with Serenity','2025-11-06 20:24:01','2025-11-06 20:24:01'),(230,10,2,51,60,'22098,22099,22107,22108,23167,23168,23169',NULL,'Windwalker Monk 51-60: Strike of the Windlord with Serenity','2025-11-06 20:24:01','2025-11-06 20:24:01'),(231,10,2,61,70,'22098,22099,22107,22108,23167,23168,23169',NULL,'Windwalker Monk 61-70: Strike of the Windlord with Serenity','2025-11-06 20:24:01','2025-11-06 20:24:01'),(232,10,2,71,80,'22098,22099,22107,22108,23167,23168,23169','','Windwalker Monk 71-80: Raid build (Conduit of the Celestials/Shado-Pan)','2025-11-06 20:24:01','2025-11-06 20:24:01'),(233,10,1,1,10,'19820',NULL,'Mistweaver Monk 1-10: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(234,10,1,11,20,'19820,19823,20185,23106,19302,19304,19818,22166,22168,19993,19994,19995,22219,20173,20175,23371,22101,23107,22169,22170,22218',NULL,'Mistweaver Monk 11-20: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(235,10,1,21,30,'19820,19823,20185,23106,19302,19304,19818,22166,22168,19993,19994,19995,22219,20173,20175,23371,22101,23107,22169,22170,22218',NULL,'Mistweaver Monk 21-30: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(236,10,1,31,40,'19820,19823,20185,23106,19302,19304,19818,22166,22168,19993,19994,19995,22219,20173,20175,23371,22101,23107,22169,22170,22218',NULL,'Mistweaver Monk 31-40: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(237,10,1,41,50,'19820,19823,20185,23106,19302,19304,19818,22166,22168,19993,19994,19995,22219,20173,20175,23371,22101,23107,22169,22170,22218',NULL,'Mistweaver Monk 41-50: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(238,10,1,51,60,'19820,19823,20185,23106,19302,19304,19818,22166,22168,19993,19994,19995,22219,20173,20175,23371,22101,23107,22169,22170,22218',NULL,'Mistweaver Monk 51-60: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(239,10,1,61,70,'19820,19823,20185,23106,19302,19304,19818,22166,22168,19993,19994,19995,22219,20173,20175,23371,22101,23107,22169,22170,22218',NULL,'Mistweaver Monk 61-70: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(240,10,1,71,80,'19820,19823,20185,23106,19302,19304,19818,22166,22168,19993,19994,19995,22219,20173,20175,23371,22101,23107,22169,22170,22218','','Mistweaver Monk 71-80: Requires manual talent curation (Conduit of the Celestials/Master of Harmony)','2025-11-06 20:24:01','2025-12-19 14:54:50'),(241,11,0,1,10,'22366,22367,22370,22373,23073,23074,23075',NULL,'Balance Druid 1-10: Astral Power focused with Incarnation','2025-11-06 20:24:01','2025-11-06 20:24:01'),(242,11,0,11,20,'22366,22367,22370,22373,23073,23074,23075',NULL,'Balance Druid 11-20: Astral Power focused with Incarnation','2025-11-06 20:24:01','2025-11-06 20:24:01'),(243,11,0,21,30,'22366,22367,22370,22373,23073,23074,23075',NULL,'Balance Druid 21-30: Astral Power focused with Incarnation','2025-11-06 20:24:01','2025-11-06 20:24:01'),(244,11,0,31,40,'22366,22367,22370,22373,23073,23074,23075',NULL,'Balance Druid 31-40: Astral Power focused with Incarnation','2025-11-06 20:24:01','2025-11-06 20:24:01'),(245,11,0,41,50,'22366,22367,22370,22373,23073,23074,23075',NULL,'Balance Druid 41-50: Astral Power focused with Incarnation','2025-11-06 20:24:01','2025-11-06 20:24:01'),(246,11,0,51,60,'22366,22367,22370,22373,23073,23074,23075',NULL,'Balance Druid 51-60: Astral Power focused with Incarnation','2025-11-06 20:24:01','2025-11-06 20:24:01'),(247,11,0,61,70,'22366,22367,22370,22373,23073,23074,23075',NULL,'Balance Druid 61-70: Astral Power focused with Incarnation','2025-11-06 20:24:01','2025-11-06 20:24:01'),(248,11,0,71,80,'22366,22367,22370,22373,23073,23074,23075','','Balance Druid 71-80: Raid build (Elune\'s Chosen/Keeper of the Grove)','2025-11-06 20:24:01','2025-11-06 20:24:01'),(249,11,1,1,10,'22363,22364,22368,22369,23069,23070,23071',NULL,'Feral Druid 1-10: Bleed-focused with Berserk','2025-11-06 20:24:01','2025-11-06 20:24:01'),(250,11,1,11,20,'22363,22364,22368,22369,23069,23070,23071',NULL,'Feral Druid 11-20: Bleed-focused with Berserk','2025-11-06 20:24:01','2025-11-06 20:24:01'),(251,11,1,21,30,'22363,22364,22368,22369,23069,23070,23071',NULL,'Feral Druid 21-30: Bleed-focused with Berserk','2025-11-06 20:24:01','2025-11-06 20:24:01'),(252,11,1,31,40,'22363,22364,22368,22369,23069,23070,23071',NULL,'Feral Druid 31-40: Bleed-focused with Berserk','2025-11-06 20:24:01','2025-11-06 20:24:01'),(253,11,1,41,50,'22363,22364,22368,22369,23069,23070,23071',NULL,'Feral Druid 41-50: Bleed-focused with Berserk','2025-11-06 20:24:01','2025-11-06 20:24:01'),(254,11,1,51,60,'22363,22364,22368,22369,23069,23070,23071',NULL,'Feral Druid 51-60: Bleed-focused with Berserk','2025-11-06 20:24:01','2025-11-06 20:24:01'),(255,11,1,61,70,'22363,22364,22368,22369,23069,23070,23071',NULL,'Feral Druid 61-70: Bleed-focused with Berserk','2025-11-06 20:24:01','2025-11-06 20:24:01'),(256,11,1,71,80,'22363,22364,22368,22369,23069,23070,23071','','Feral Druid 71-80: Raid build (Druid of the Claw/Wildstalker)','2025-11-06 20:24:01','2025-11-06 20:24:01'),(257,11,2,1,10,'22418',NULL,'Guardian Druid 1-10: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(258,11,2,11,20,'22418,22419,18570,18571,19283,22156,22159,22163,18576,18577,21778,21707,21709,22388,21713,22390,22423,22426,22427',NULL,'Guardian Druid 11-20: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(259,11,2,21,30,'22418,22419,18570,18571,19283,22156,22159,22163,18576,18577,21778,21707,21709,22388,21713,22390,22423,22426,22427',NULL,'Guardian Druid 21-30: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(260,11,2,31,40,'22418,22419,18570,18571,19283,22156,22159,22163,18576,18577,21778,21707,21709,22388,21713,22390,22423,22426,22427',NULL,'Guardian Druid 31-40: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(261,11,2,41,50,'22418,22419,18570,18571,19283,22156,22159,22163,18576,18577,21778,21707,21709,22388,21713,22390,22423,22426,22427',NULL,'Guardian Druid 41-50: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(262,11,2,51,60,'22418,22419,18570,18571,19283,22156,22159,22163,18576,18577,21778,21707,21709,22388,21713,22390,22423,22426,22427',NULL,'Guardian Druid 51-60: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(263,11,2,61,70,'22418,22419,18570,18571,19283,22156,22159,22163,18576,18577,21778,21707,21709,22388,21713,22390,22423,22426,22427',NULL,'Guardian Druid 61-70: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(264,11,2,71,80,'22418,22419,18570,18571,19283,22156,22159,22163,18576,18577,21778,21707,21709,22388,21713,22390,22423,22426,22427','','Guardian Druid 71-80: Requires manual talent curation (Druid of the Claw/Elune\'s Chosen)','2025-11-06 20:24:01','2025-12-19 14:54:50'),(265,11,3,1,10,'18569',NULL,'Restoration Druid 1-10: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(266,11,3,11,20,'18569,18572,18574,18570,18571,19283,22159,22160,22163,22366,22367,18576,18577,21778,21705,21710,22421,18585,21716,22422,21651',NULL,'Restoration Druid 11-20: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(267,11,3,21,30,'18569,18572,18574,18570,18571,19283,22159,22160,22163,22366,22367,18576,18577,21778,21705,21710,22421,18585,21716,22422,21651,22403,22404',NULL,'Restoration Druid 21-30: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(268,11,3,31,40,'18569,18572,18574,18570,18571,19283,22159,22160,22163,22366,22367,18576,18577,21778,21705,21710,22421,18585,21716,22422,21651,22403,22404',NULL,'Restoration Druid 31-40: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(269,11,3,41,50,'18569,18572,18574,18570,18571,19283,22159,22160,22163,22366,22367,18576,18577,21778,21705,21710,22421,18585,21716,22422,21651,22403,22404',NULL,'Restoration Druid 41-50: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(270,11,3,51,60,'18569,18572,18574,18570,18571,19283,22159,22160,22163,22366,22367,18576,18577,21778,21705,21710,22421,18585,21716,22422,21651,22403,22404',NULL,'Restoration Druid 51-60: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(271,11,3,61,70,'18569,18572,18574,18570,18571,19283,22159,22160,22163,22366,22367,18576,18577,21778,21705,21710,22421,18585,21716,22422,21651,22403,22404',NULL,'Restoration Druid 61-70: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(272,11,3,71,80,'18569,18572,18574,18570,18571,19283,22159,22160,22163,22366,22367,18576,18577,21778,21705,21710,22421,18585,21716,22422,21651,22403,22404','','Restoration Druid 71-80: Requires manual talent curation (Keeper of the Grove/Wildstalker)','2025-11-06 20:24:01','2025-12-19 14:54:50'),(273,12,0,1,10,'21854,21855,21862,21863,22493,22494,22495',NULL,'Havoc Demon Hunter 1-10: Fel Barrage with Momentum','2025-11-06 20:24:01','2025-11-06 20:24:01'),(274,12,0,11,20,'21854,21855,21862,21863,22493,22494,22495',NULL,'Havoc Demon Hunter 11-20: Fel Barrage with Momentum','2025-11-06 20:24:01','2025-11-06 20:24:01'),(275,12,0,21,30,'21854,21855,21862,21863,22493,22494,22495',NULL,'Havoc Demon Hunter 21-30: Fel Barrage with Momentum','2025-11-06 20:24:01','2025-11-06 20:24:01'),(276,12,0,31,40,'21854,21855,21862,21863,22493,22494,22495',NULL,'Havoc Demon Hunter 31-40: Fel Barrage with Momentum','2025-11-06 20:24:01','2025-11-06 20:24:01'),(277,12,0,41,50,'21854,21855,21862,21863,22493,22494,22495',NULL,'Havoc Demon Hunter 41-50: Fel Barrage with Momentum','2025-11-06 20:24:01','2025-11-06 20:24:01'),(278,12,0,51,60,'21854,21855,21862,21863,22493,22494,22495',NULL,'Havoc Demon Hunter 51-60: Fel Barrage with Momentum','2025-11-06 20:24:01','2025-11-06 20:24:01'),(279,12,0,61,70,'21854,21855,21862,21863,22493,22494,22495',NULL,'Havoc Demon Hunter 61-70: Fel Barrage with Momentum','2025-11-06 20:24:01','2025-11-06 20:24:01'),(280,12,0,71,80,'21854,21855,21862,21863,22493,22494,22495','','Havoc Demon Hunter 71-80: Raid build (Aldrachi Reaver/Fel-Scarred)','2025-11-06 20:24:01','2025-11-06 20:24:01'),(281,12,1,1,10,'22502',NULL,'Vengeance Demon Hunter 1-10: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(282,12,1,11,20,'22502,22503,22504,22505,22507,22766,22324,22540,22541,22508,22509,22770,22510,22511,22546,22512,22513,22768,21902,22543,23464',NULL,'Vengeance Demon Hunter 11-20: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(283,12,1,21,30,'22502,22503,22504,22505,22507,22766,22324,22540,22541,22508,22509,22770,22510,22511,22546,22512,22513,22768,21902,22543,23464',NULL,'Vengeance Demon Hunter 21-30: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(284,12,1,31,40,'22502,22503,22504,22505,22507,22766,22324,22540,22541,22508,22509,22770,22510,22511,22546,22512,22513,22768,21902,22543,23464',NULL,'Vengeance Demon Hunter 31-40: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(285,12,1,41,50,'22502,22503,22504,22505,22507,22766,22324,22540,22541,22508,22509,22770,22510,22511,22546,22512,22513,22768,21902,22543,23464',NULL,'Vengeance Demon Hunter 41-50: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:49'),(286,12,1,51,60,'22502,22503,22504,22505,22507,22766,22324,22540,22541,22508,22509,22770,22510,22511,22546,22512,22513,22768,21902,22543,23464',NULL,'Vengeance Demon Hunter 51-60: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(287,12,1,61,70,'22502,22503,22504,22505,22507,22766,22324,22540,22541,22508,22509,22770,22510,22511,22546,22512,22513,22768,21902,22543,23464',NULL,'Vengeance Demon Hunter 61-70: Requires manual talent curation','2025-11-06 20:24:01','2025-12-19 14:54:50'),(288,12,1,71,80,'22502,22503,22504,22505,22507,22766,22324,22540,22541,22508,22509,22770,22510,22511,22546,22512,22513,22768,21902,22543,23464','','Vengeance Demon Hunter 71-80: Requires manual talent curation (Aldrachi Reaver/Fel-Scarred)','2025-11-06 20:24:01','2025-12-19 14:54:49'),(289,13,0,1,10,'23506,23507,23508,23509,23550,23551,23552',NULL,'Devastation Evoker 1-10: Dragonrage burst with Fire Breath','2025-11-06 20:24:01','2025-11-06 20:24:01'),(290,13,0,11,20,'23506,23507,23508,23509,23550,23551,23552',NULL,'Devastation Evoker 11-20: Dragonrage burst with Fire Breath','2025-11-06 20:24:01','2025-11-06 20:24:01'),(291,13,0,21,30,'23506,23507,23508,23509,23550,23551,23552',NULL,'Devastation Evoker 21-30: Dragonrage burst with Fire Breath','2025-11-06 20:24:01','2025-11-06 20:24:01'),(292,13,0,31,40,'23506,23507,23508,23509,23550,23551,23552',NULL,'Devastation Evoker 31-40: Dragonrage burst with Fire Breath','2025-11-06 20:24:01','2025-11-06 20:24:01'),(293,13,0,41,50,'23506,23507,23508,23509,23550,23551,23552',NULL,'Devastation Evoker 41-50: Dragonrage burst with Fire Breath','2025-11-06 20:24:01','2025-11-06 20:24:01'),(294,13,0,51,60,'23506,23507,23508,23509,23550,23551,23552',NULL,'Devastation Evoker 51-60: Dragonrage burst with Fire Breath','2025-11-06 20:24:01','2025-11-06 20:24:01'),(295,13,0,61,70,'23506,23507,23508,23509,23550,23551,23552',NULL,'Devastation Evoker 61-70: Dragonrage burst with Fire Breath','2025-11-06 20:24:01','2025-11-06 20:24:01'),(296,13,0,71,80,'23506,23507,23508,23509,23550,23551,23552','','Devastation Evoker 71-80: Raid build (Chronowarden/Flameshaper)','2025-11-06 20:24:01','2025-11-06 20:24:01'),(297,13,1,1,10,'',NULL,'Preservation Evoker 1-10: Requires manual talent curation','2025-11-06 20:24:01','2025-11-06 20:24:01'),(298,13,1,11,20,'',NULL,'Preservation Evoker 11-20: Requires manual talent curation','2025-11-06 20:24:01','2025-11-06 20:24:01'),(299,13,1,21,30,'',NULL,'Preservation Evoker 21-30: Requires manual talent curation','2025-11-06 20:24:01','2025-11-06 20:24:01'),(300,13,1,31,40,'',NULL,'Preservation Evoker 31-40: Requires manual talent curation','2025-11-06 20:24:01','2025-11-06 20:24:01'),(301,13,1,41,50,'',NULL,'Preservation Evoker 41-50: Requires manual talent curation','2025-11-06 20:24:01','2025-11-06 20:24:01'),(302,13,1,51,60,'',NULL,'Preservation Evoker 51-60: Requires manual talent curation','2025-11-06 20:24:01','2025-11-06 20:24:01'),(303,13,1,61,70,'',NULL,'Preservation Evoker 61-70: Requires manual talent curation','2025-11-06 20:24:01','2025-11-06 20:24:01'),(304,13,1,71,80,'','','Preservation Evoker 71-80: Requires manual talent curation (Chronowarden/Flameshaper)','2025-11-06 20:24:01','2025-11-06 20:24:01'),(305,13,2,1,10,'',NULL,'Augmentation Evoker 1-10: Requires manual talent curation','2025-11-06 20:24:01','2025-11-06 20:24:01'),(306,13,2,11,20,'',NULL,'Augmentation Evoker 11-20: Requires manual talent curation','2025-11-06 20:24:01','2025-11-06 20:24:01'),(307,13,2,21,30,'',NULL,'Augmentation Evoker 21-30: Requires manual talent curation','2025-11-06 20:24:01','2025-11-06 20:24:01'),(308,13,2,31,40,'',NULL,'Augmentation Evoker 31-40: Requires manual talent curation','2025-11-06 20:24:01','2025-11-06 20:24:01'),(309,13,2,41,50,'',NULL,'Augmentation Evoker 41-50: Requires manual talent curation','2025-11-06 20:24:01','2025-11-06 20:24:01'),(310,13,2,51,60,'',NULL,'Augmentation Evoker 51-60: Requires manual talent curation','2025-11-06 20:24:01','2025-11-06 20:24:01'),(311,13,2,61,70,'',NULL,'Augmentation Evoker 61-70: Requires manual talent curation','2025-11-06 20:24:01','2025-11-06 20:24:01'),(312,13,2,71,80,'','','Augmentation Evoker 71-80: Requires manual talent curation (Chronowarden/Scalecommander)','2025-11-06 20:24:01','2025-11-06 20:24:01');
/*!40000 ALTER TABLE `playerbot_talent_loadouts` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `playerbot_zone_populations`
--

DROP TABLE IF EXISTS `playerbot_zone_populations`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_zone_populations` (
  `zone_id` int unsigned NOT NULL,
  `map_id` int unsigned NOT NULL,
  `current_bots` int unsigned NOT NULL DEFAULT '0' COMMENT 'Current bot count in zone',
  `target_population` int unsigned NOT NULL DEFAULT '0' COMMENT 'Target bot population',
  `max_population` int unsigned NOT NULL DEFAULT '50' COMMENT 'Maximum allowed bots',
  `min_level` tinyint unsigned NOT NULL DEFAULT '1' COMMENT 'Minimum level for zone',
  `max_level` tinyint unsigned NOT NULL DEFAULT '80' COMMENT 'Maximum level for zone',
  `spawn_weight` float NOT NULL DEFAULT '1' COMMENT 'Relative spawn probability',
  `is_enabled` tinyint(1) NOT NULL DEFAULT '1' COMMENT 'Zone allows bot spawning',
  `is_starter_zone` tinyint(1) NOT NULL DEFAULT '0' COMMENT 'Low-level character zone',
  `is_endgame_zone` tinyint(1) NOT NULL DEFAULT '0' COMMENT 'High-level content zone',
  `population_multiplier` float NOT NULL DEFAULT '1' COMMENT 'Dynamic population adjustment',
  `last_spawn` timestamp NULL DEFAULT NULL COMMENT 'Last bot spawn in this zone',
  `spawn_cooldown_minutes` int unsigned NOT NULL DEFAULT '5' COMMENT 'Minimum time between spawns',
  `total_spawns_today` int unsigned NOT NULL DEFAULT '0' COMMENT 'Daily spawn counter',
  `average_session_time` int unsigned NOT NULL DEFAULT '3600' COMMENT 'Average session length',
  `peak_population` int unsigned NOT NULL DEFAULT '0' COMMENT 'Peak population reached',
  `peak_timestamp` timestamp NULL DEFAULT NULL COMMENT 'When peak was reached',
  `last_updated` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `created_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`zone_id`,`map_id`),
  KEY `idx_current_population` (`current_bots`,`target_population`),
  KEY `idx_spawn_eligibility` (`is_enabled`,`spawn_weight`),
  KEY `idx_level_range` (`min_level`,`max_level`),
  KEY `idx_zone_type` (`is_starter_zone`,`is_endgame_zone`),
  KEY `idx_spawn_cooldown` (`last_spawn`,`spawn_cooldown_minutes`),
  KEY `idx_updated` (`last_updated`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Real-time bot population tracking per zone';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `playerbot_zone_populations`
--

LOCK TABLES `playerbot_zone_populations` WRITE;
/*!40000 ALTER TABLE `playerbot_zone_populations` DISABLE KEYS */;
/*!40000 ALTER TABLE `playerbot_zone_populations` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `playerbots_class_popularity`
--

DROP TABLE IF EXISTS `playerbots_class_popularity`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbots_class_popularity` (
  `class_id` tinyint unsigned NOT NULL,
  `class_name` varchar(32) COLLATE utf8mb4_unicode_ci NOT NULL,
  `popularity_weight` float NOT NULL DEFAULT '1',
  `min_level` tinyint unsigned NOT NULL DEFAULT '1',
  `max_level` tinyint unsigned NOT NULL DEFAULT '80',
  `enabled` tinyint(1) NOT NULL DEFAULT '1',
  `created_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`class_id`),
  KEY `idx_enabled_classes` (`enabled`,`popularity_weight`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `playerbots_class_popularity`
--

LOCK TABLES `playerbots_class_popularity` WRITE;
/*!40000 ALTER TABLE `playerbots_class_popularity` DISABLE KEYS */;
INSERT INTO `playerbots_class_popularity` VALUES (1,'Warrior',10.5,1,80,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(2,'Paladin',12.8,1,80,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(3,'Hunter',11.2,1,80,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(4,'Rogue',9.8,1,80,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(5,'Priest',8.1,1,80,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(6,'Death Knight',11.9,55,80,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(7,'Shaman',7.3,1,80,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(8,'Mage',9.2,1,80,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(9,'Warlock',6.9,1,80,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(10,'Monk',6.4,1,80,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(11,'Druid',8.7,1,80,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(12,'Demon Hunter',4.2,98,80,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(13,'Evoker',3,58,80,1,'2025-10-08 17:30:47','2025-10-08 17:30:47');
/*!40000 ALTER TABLE `playerbots_class_popularity` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `playerbots_gender_distribution`
--

DROP TABLE IF EXISTS `playerbots_gender_distribution`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbots_gender_distribution` (
  `race_id` tinyint unsigned NOT NULL,
  `male_percentage` float NOT NULL DEFAULT '50',
  `female_percentage` float NOT NULL DEFAULT '50',
  `created_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`race_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Gender distribution by race';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `playerbots_gender_distribution`
--

LOCK TABLES `playerbots_gender_distribution` WRITE;
/*!40000 ALTER TABLE `playerbots_gender_distribution` DISABLE KEYS */;
INSERT INTO `playerbots_gender_distribution` VALUES (1,58,42,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(2,75,25,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(3,72,28,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(4,35,65,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(5,48,52,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(6,68,32,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(7,55,45,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(8,60,40,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(9,55,45,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(10,25,75,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(11,42,58,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(22,65,35,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(24,52,48,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(25,52,48,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(26,52,48,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(27,30,70,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(28,70,30,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(29,38,62,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(30,68,32,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(31,58,42,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(32,62,38,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(34,40,60,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(35,45,55,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(36,78,22,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(37,70,30,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(52,45,55,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(70,45,55,'2025-10-08 17:30:47','2025-10-08 17:30:47');
/*!40000 ALTER TABLE `playerbots_gender_distribution` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `playerbots_names`
--

DROP TABLE IF EXISTS `playerbots_names`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbots_names` (
  `name_id` int unsigned NOT NULL AUTO_INCREMENT,
  `name` varchar(12) COLLATE utf8mb4_unicode_ci NOT NULL,
  `gender` tinyint unsigned NOT NULL DEFAULT '2' COMMENT '0=male, 1=female, 2=neutral',
  `race_mask` int unsigned NOT NULL DEFAULT '4294967295' COMMENT 'Bitmask of compatible races',
  `is_taken` tinyint(1) NOT NULL DEFAULT '0',
  `is_used` tinyint(1) NOT NULL DEFAULT '0',
  `used_by_guid` bigint unsigned DEFAULT NULL,
  `created_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`name_id`),
  UNIQUE KEY `idx_unique_name` (`name`),
  KEY `idx_available_names` (`is_taken`,`gender`,`race_mask`),
  KEY `idx_used_names` (`is_used`,`used_by_guid`)
) ENGINE=InnoDB AUTO_INCREMENT=107340 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `playerbots_names`
--

LOCK TABLES `playerbots_names` WRITE;
/*!40000 ALTER TABLE `playerbots_names` DISABLE KEYS */;
-- Sample bot names for the Playerbot system
-- This file populates the playerbots_names table with a variety of names

TRUNCATE TABLE playerbots_names;

-- Male names (gender = 0)
INSERT INTO `playerbots_names` (`name`, `gender`) VALUES
-- Common Human/Alliance style names
('Aldric', 0), ('Gareth', 0), ('Marcus', 0), ('William', 0), ('Edward', 0),
('Richard', 0), ('Thomas', 0), ('Robert', 0), ('James', 0), ('Henry', 0),
('Arthur', 0), ('Charles', 0), ('George', 0), ('Francis', 0), ('Albert', 0),
('Frederick', 0), ('Samuel', 0), ('Benjamin', 0), ('Nicholas', 0), ('Alexander', 0),
('Jonathan', 0), ('Christopher', 0), ('Matthew', 0), ('Daniel', 0), ('Michael', 0),
('Stephen', 0), ('Andrew', 0), ('Patrick', 0), ('Lawrence', 0), ('Vincent', 0),
('Gregory', 0), ('Raymond', 0), ('Timothy', 0), ('Kenneth', 0), ('Eugene', 0),
('Russell', 0), ('Walter', 0), ('Harold', 0), ('Douglas', 0), ('Gerald', 0),

-- Dwarf style names
('Thorgrim', 0), ('Balin', 0), ('Thorin', 0), ('Gimli', 0), ('Durin', 0),
('Dwalin', 0), ('Gloin', 0), ('Oin', 0), ('Bifur', 0), ('Bofur', 0),
('Bombur', 0), ('Nori', 0), ('Dori', 0), ('Ori', 0), ('Flint', 0),
('Magni', 0), ('Muradin', 0), ('Brann', 0), ('Thaurissan', 0), ('Ironforge', 0),

-- Night Elf style names
('Malfurion', 0), ('Illidan', 0), ('Tyrande', 0), ('Cenarius', 0), ('Xavius', 0),
('Fandral', 0), ('Broll', 0), ('Jarod', 0), ('Ravencrest', 0), ('Shadowsong', 0),
('Stormrage', 0), ('Whisperwind', 0), ('Staghelm', 0), ('Bearmantle', 0), ('Moonrage', 0),

-- Gnome style names
('Gelbin', 0), ('Sicco', 0), ('Tinker', 0), ('Mekkatorque', 0), ('Thermaplugg', 0),
('Cogspinner', 0), ('Geargrind', 0), ('Fizzlebolt', 0), ('Sparkwrench', 0), ('Cogspin', 0),

-- Draenei style names
('Velen', 0), ('Akama', 0), ('Nobundo', 0), ('Maraad', 0), ('Hatuun', 0),
('Khadgar', 0), ('Turalyon', 0), ('Kurdran', 0), ('Alleria', 0), ('Danath', 0),

-- Orc style names
('Thrall', 0), ('Durotan', 0), ('Orgrim', 0), ('Grommash', 0), ('Garrosh', 0),
('Doomhammer', 0), ('Hellscream', 0), ('Blackhand', 0), ('Kilrogg', 0), ('Kargath', 0),
('Nazgrel', 0), ('Saurfang', 0), ('Eitrigg', 0), ('Rehgar', 0), ('Jorin', 0),
('Broxigar', 0), ('Varok', 0), ('Nazgrim', 0), ('Malkorok', 0), ('Zaela', 0),

-- Undead style names
('Sylvana', 0), ('Nathanos', 0), ('Putress', 0), ('Faranell', 0), ('Belmont', 0),
('Varimathras', 0), ('Balnazzar', 0), ('Detheroc', 0), ('Tichondrius', 0), ('Anetheron', 0),
('Archimonde', 0), ('Mannoroth', 0), ('Magtheridon', 0), ('Azgalor', 0), ('Kazzak', 0),

-- Tauren style names
('Cairne', 0), ('Baine', 0), ('Hamuul', 0), ('Runetotem', 0), ('Bloodhoof', 0),
('Thunderhorn', 0), ('Skychaser', 0), ('Wildmane', 0), ('Ragetotem', 0), ('Grimtotem', 0),
('Highmountain', 0), ('Rivermane', 0), ('Winterhoof', 0), ('Mistrunner', 0), ('Dawnstrider', 0),

-- Troll style names
('Voljin', 0), ('Senjin', 0), ('Rokhan', 0), ('Zalazane', 0), ('Zuljin', 0),
('Jintha', 0), ('Venoxis', 0), ('Mandokir', 0), ('Marli', 0), ('Thekal', 0),
('Arlokk', 0), ('Jeklik', 0), ('Hakkari', 0), ('Gurubashi', 0), ('Amani', 0),

-- Blood Elf style names
('Kaelthas', 0), ('Rommath', 0), ('Lorthemar', 0), ('Halduron', 0), ('Aethas', 0),
('Theron', 0), ('Sunstrider', 0), ('Brightwing', 0), ('Dawnseeker', 0), ('Sunreaver', 0),
('Bloodsworn', 0), ('Spellbreaker', 0), ('Farstrider', 0), ('Sunfury', 0), ('Dawncaller', 0),

-- Additional generic fantasy names
('Aiden', 0), ('Blake', 0), ('Connor', 0), ('Derek', 0), ('Ethan', 0),
('Felix', 0), ('Gabriel', 0), ('Hunter', 0), ('Ivan', 0), ('Jacob', 0),
('Kyle', 0), ('Liam', 0), ('Mason', 0), ('Nathan', 0), ('Oliver', 0),
('Peter', 0), ('Quinn', 0), ('Ryan', 0), ('Sean', 0), ('Tyler', 0),
('Ulrich', 0), ('Victor', 0), ('Wesley', 0), ('Xavier', 0), ('Zachary', 0);

-- Female names (gender = 1)
INSERT INTO `playerbots_names` (`name`, `gender`) VALUES
-- Common Human/Alliance style names
('Jaina', 1), ('Katherine', 1), ('Elizabeth', 1), ('Margaret', 1), ('Dorothy', 1),
('Sarah', 1), ('Jessica', 1), ('Michelle', 1), ('Amanda', 1), ('Melissa', 1),
('Jennifer', 1), ('Patricia', 1), ('Barbara', 1), ('Susan', 1), ('Linda', 1),
('Mary', 1), ('Lisa', 1), ('Nancy', 1), ('Karen', 1), ('Betty', 1),
('Helen', 1), ('Sandra', 1), ('Donna', 1), ('Carol', 1), ('Ruth', 1),
('Sharon', 1), ('Laura', 1), ('Cynthia', 1), ('Amy', 1), ('Angela', 1),
('Brenda', 1), ('Emma', 1), ('Anna', 1), ('Marie', 1), ('Christine', 1),
('Deborah', 1), ('Martha', 1), ('Maria', 1), ('Heather', 1), ('Diane', 1),

-- Night Elf style names
('Tyranda', 1), ('Maiev', 1), ('Shandris', 1), ('Azshara', 1), ('Elune', 1),
('Feathermoon', 1), ('Awhisperwind', 1), ('Starweaver', 1), ('Moonwhisper', 1), ('Ashadowsong', 1),
('Starfall', 1), ('Nightwhisper', 1), ('Dawnweaver', 1), ('Moonfire', 1), ('Starlight', 1),

-- Dwarf style names
('Moira', 1), ('Bronzebeard', 1), ('Forgehammer', 1), ('Wildhammer', 1), ('Anvilmar', 1),
('Stormhammer', 1), ('Goldbeard', 1), ('Ironfoot', 1), ('Stoneform', 1), ('Deepforge', 1),

-- Gnome style names
('Chromie', 1), ('Millhouse', 1), ('Tinkerspell', 1), ('Cogwheel', 1), ('Sprocket', 1),
('Gearshift', 1), ('Wrenchcrank', 1), ('Boltbucket', 1), ('Fizzlewick', 1), ('Sparkplug', 1),

-- Draenei style names
('Yrel', 1), ('Ishanah', 1), ('Naielle', 1), ('Askara', 1), ('Dornaa', 1),
('Nobundoua', 1), ('Khallia', 1), ('Emony', 1), ('Jessera', 1), ('Anchorite', 1),

-- Orc style names
('Draka', 1), ('Aggra', 1), ('Garona', 1), ('Zaelea', 1), ('Geyah', 1),
('Kashur', 1), ('Greatmother', 1), ('Mankrik', 1), ('Sheyala', 1), ('Thura', 1),

-- Undead style names
('Sylvanas', 1), ('Calia', 1), ('Lilian', 1), ('Voss', 1), ('Faranall', 1),
('Velonara', 1), ('Delaryn', 1), ('Sira', 1), ('Arthura', 1), ('Renee', 1),

-- Tauren style names
('Magatha', 1), ('Hamuulaa', 1), ('Melor', 1), ('Tawnbranch', 1), ('Windtotem', 1),
('Skyhorn', 1), ('Highmount', 1), ('Riverwind', 1), ('Eagletalon', 1), ('Moondarkwhisper', 1),

-- Troll style names
('Zentabra', 1), ('Vanira', 1), ('Shadra', 1), ('Bethekk', 1), ('Shirvallah', 1),
('Hireek', 1), ('Jeklikia', 1), ('Marlii', 1), ('Arlok', 1), ('Shadehunter', 1),

-- Blood Elf style names
('Liadrin', 1), ('Valeera', 1), ('Allea', 1), ('Vereesa', 1), ('Sylvannas', 1),
('Sunweaver', 1), ('Dawnblade', 1), ('Goldensword', 1), ('Brightfeather', 1), ('Sunseeker', 1),
('Dawnrunner', 1), ('Sunsorrow', 1), ('Bloodwatcher', 1), ('Spellbinder', 1), ('Sunreaverous', 1),

-- Additional generic fantasy names
('Alexis', 1), ('Brianna', 1), ('Catherine', 1), ('Diana', 1), ('Elena', 1),
('Fiona', 1), ('Gabrielle', 1), ('Hannah', 1), ('Isabella', 1), ('Jasmine', 1),
('Kaitlyn', 1), ('Lily', 1), ('Madison', 1), ('Natalie', 1), ('Olivia', 1),
('Penelope', 1), ('Quinna', 1), ('Rachel', 1), ('Sophia', 1), ('Taylor', 1),
('Uma', 1), ('Victoria', 1), ('Wendy', 1), ('Xena', 1), ('Yasmine', 1), ('Zoe', 1),

-- Additional fantasy-themed female names
('Aerith', 1), ('Aria', 1), ('Aurora', 1), ('Celeste', 1), ('Luna', 1),
('Nova', 1), ('Seraphina', 1), ('Stella', 1), ('Terra', 1), ('Vega', 1),
('Lyra', 1), ('Nyx', 1), ('Phoenix', 1), ('Raven', 1), ('Scarlett', 1),
('Violet', 1), ('Willow', 1), ('Winter', 1), ('Iris', 1), ('Jade', 1),
('Pearl', 1), ('Rose', 1), ('Ruby', 1), ('Sage', 1), ('Sky', 1);

-- Verify the count
SELECT 
    'Total names loaded:' AS info,
    COUNT(*) AS total,
    SUM(CASE WHEN gender = 0 THEN 1 ELSE 0 END) AS male_names,
    SUM(CASE WHEN gender = 1 THEN 1 ELSE 0 END) AS female_names
FROM playerbots_names;

/*!40000 ALTER TABLE `playerbots_names` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `playerbots_names_used`
--

DROP TABLE IF EXISTS `playerbots_names_used`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbots_names_used` (
  `name_id` int unsigned NOT NULL,
  `character_guid` bigint unsigned NOT NULL,
  `assigned_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`character_guid`),
  UNIQUE KEY `idx_unique_name` (`name_id`),
  CONSTRAINT `fk_names_used_name_id` FOREIGN KEY (`name_id`) REFERENCES `playerbots_names` (`name_id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `playerbots_names_used`
--

LOCK TABLES `playerbots_names_used` WRITE;
/*!40000 ALTER TABLE `playerbots_names_used` DISABLE KEYS */;
/*!40000 ALTER TABLE `playerbots_names_used` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `playerbots_race_class_distribution`
--

DROP TABLE IF EXISTS `playerbots_race_class_distribution`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbots_race_class_distribution` (
  `race_id` tinyint unsigned NOT NULL,
  `class_id` tinyint unsigned NOT NULL,
  `distribution_weight` float NOT NULL DEFAULT '1',
  `enabled` tinyint(1) NOT NULL DEFAULT '1',
  `created_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`race_id`,`class_id`),
  KEY `idx_race_class_enabled` (`enabled`,`distribution_weight`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Race-class combination weights';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `playerbots_race_class_distribution`
--

LOCK TABLES `playerbots_race_class_distribution` WRITE;
/*!40000 ALTER TABLE `playerbots_race_class_distribution` DISABLE KEYS */;
INSERT INTO `playerbots_race_class_distribution` VALUES (1,1,8.5,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(1,2,7.2,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(1,3,3.2,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(1,4,6.8,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(1,5,5.9,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(1,6,2.8,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(1,8,5.4,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(1,9,4.1,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(1,10,2.1,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(2,1,7.8,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(2,3,5.4,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(2,4,3.8,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(2,6,2.9,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(2,7,6.9,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(2,9,4.2,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(2,10,2.1,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(3,1,2.7,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(3,2,3.1,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(3,3,3.8,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(3,4,1.9,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(3,5,2.2,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(3,6,1.3,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(3,7,1.6,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(3,10,1,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(4,1,2.9,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(4,3,4.8,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(4,4,3.5,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(4,5,2.4,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(4,6,1.8,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(4,8,2.1,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(4,10,1.4,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(4,11,4.2,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(4,12,1.2,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(5,1,3.2,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(5,4,4.1,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(5,5,5.1,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(5,6,2.4,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(5,8,4.8,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(5,9,6.2,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(5,10,1.8,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(6,1,3.8,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(6,2,2.6,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(6,3,3.1,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(6,6,2.1,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(6,7,4.2,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(6,10,1.7,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(6,11,5.4,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(7,1,1.8,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(7,3,0.8,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(7,4,1.5,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(7,5,1.2,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(7,6,1,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(7,8,2.9,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(7,9,2.4,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(7,10,0.6,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(8,1,3.9,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(8,3,4.4,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(8,4,3.2,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(8,5,2.8,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(8,6,1.6,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(8,7,5.1,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(8,8,2.3,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(8,9,1.9,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(8,10,1.3,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(8,11,1,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(10,1,2.3,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(10,2,5.8,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(10,3,4.6,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(10,4,4,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(10,5,3.4,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(10,6,1.8,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(10,8,5.2,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(10,9,2.9,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(10,10,1.4,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(10,12,1.1,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(11,1,1.8,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(11,2,2.8,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(11,3,1.5,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(11,5,2.1,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(11,6,1,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(11,7,2.5,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(11,8,1.2,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(11,10,0.8,1,'2025-10-08 17:30:47','2025-10-08 17:30:47');
/*!40000 ALTER TABLE `playerbots_race_class_distribution` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `playerbots_race_class_gender`
--

DROP TABLE IF EXISTS `playerbots_race_class_gender`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbots_race_class_gender` (
  `race_id` tinyint unsigned NOT NULL,
  `class_id` tinyint unsigned NOT NULL,
  `gender` tinyint unsigned NOT NULL,
  `preference_weight` float NOT NULL DEFAULT '1',
  `enabled` tinyint(1) NOT NULL DEFAULT '1',
  `created_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`race_id`,`class_id`,`gender`),
  KEY `idx_combo_enabled` (`enabled`,`preference_weight`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Combined race-class-gender preferences';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `playerbots_race_class_gender`
--

LOCK TABLES `playerbots_race_class_gender` WRITE;
/*!40000 ALTER TABLE `playerbots_race_class_gender` DISABLE KEYS */;
INSERT INTO `playerbots_race_class_gender` VALUES (1,4,0,65,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(1,8,0,50,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(1,9,0,65,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(1,10,0,55,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(4,1,0,60,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(4,2,0,55,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(4,3,1,75,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(4,4,1,70,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(4,6,0,55,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(4,11,1,75,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(4,12,0,55,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(5,5,0,40,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(5,9,0,55,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(6,7,0,75,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(6,11,0,70,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(7,8,0,45,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(8,7,0,65,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(8,11,0,50,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(10,1,0,45,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(10,2,0,35,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(10,3,1,70,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(10,4,1,65,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(10,5,1,80,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(10,6,0,45,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(10,8,1,80,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(10,9,1,70,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(10,12,0,40,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(11,1,0,65,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(11,2,0,50,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(11,5,1,65,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(11,6,0,60,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(11,7,0,55,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(24,10,0,60,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(25,10,0,60,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(52,13,0,50,1,'2025-10-08 17:30:47','2025-10-08 17:30:47'),(70,13,0,50,1,'2025-10-08 17:30:47','2025-10-08 17:30:47');
/*!40000 ALTER TABLE `playerbots_race_class_gender` ENABLE KEYS */;
UNLOCK TABLES;


DROP TABLE IF EXISTS `bot_account_metadata`;

CREATE TABLE `bot_account_metadata` (
  `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `account_id` int unsigned NOT NULL,
  `bnet_account_id`  int unsigned NOT NULL,
  `email` varchar(64) COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'Email address of the account',
  `character_count` int UNSIGNED NOT NULL COMMENT 'Number of bots created on this account',
  `expansion` tinyint unsigned NOT NULL DEFAULT '10',
  `locale` VARCHAR(32) DEFAULT 'enUS',
  `last_ip` VARCHAR(45) DEFAULT NULL COMMENT 'Last known IP address',
  `join_date` TIMESTAMP DEFAULT CURRENT_TIMESTAMP COMMENT 'Account join date',
  `last_login` TIMESTAMP NULL DEFAULT NULL COMMENT 'Last login time',
  `total_time_played` INT UNSIGNED NOT NULL DEFAULT '0' COMMENT 'Total time played across all bots (in seconds)',
  `notes` VARCHAR(255) DEFAULT NULL COMMENT 'Notes',
  `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Bot account metadata';
--
-- Dumping routines for database 'playerbot_playerbot'
--
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2026-01-06  6:17:43
