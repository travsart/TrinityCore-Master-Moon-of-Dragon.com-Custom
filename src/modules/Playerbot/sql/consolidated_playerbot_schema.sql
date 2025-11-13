-- ============================================================================
-- TrinityCore Playerbot Module - Consolidated Database Schema
-- ============================================================================
-- This file contains all required table definitions for the Playerbot module
-- Based on analysis of C++ code requirements in PlayerbotDatabaseStatements.cpp
-- Generated on: September 19, 2025
-- ============================================================================

-- Set default engine and charset
SET default_storage_engine = InnoDB;
SET NAMES utf8mb4 COLLATE utf8mb4_unicode_ci;

-- ============================================================================
-- CORE LIFECYCLE TABLES (Required by PlayerbotDatabaseStatements.cpp)
-- ============================================================================

-- Activity patterns for bot behavior scheduling
CREATE TABLE IF NOT EXISTS `playerbot_activity_patterns` (
    `pattern_name` VARCHAR(64) NOT NULL,
    `display_name` VARCHAR(128) NOT NULL,
    `description` TEXT,
    `is_system_pattern` TINYINT(1) NOT NULL DEFAULT 0,
    `login_chance` FLOAT NOT NULL DEFAULT 1.0,
    `logout_chance` FLOAT NOT NULL DEFAULT 1.0,
    `min_session_duration` INT UNSIGNED NOT NULL DEFAULT 1800,
    `max_session_duration` INT UNSIGNED NOT NULL DEFAULT 7200,
    `min_offline_duration` INT UNSIGNED NOT NULL DEFAULT 3600,
    `max_offline_duration` INT UNSIGNED NOT NULL DEFAULT 28800,
    `activity_weight` FLOAT NOT NULL DEFAULT 1.0,
    `enabled` TINYINT(1) NOT NULL DEFAULT 1,
    `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`pattern_name`),
    INDEX `idx_system_patterns` (`is_system_pattern`, `enabled`),
    INDEX `idx_pattern_weight` (`activity_weight`, `enabled`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Bot activity patterns for scheduling';

-- Bot scheduling information
CREATE TABLE IF NOT EXISTS `playerbot_schedules` (
    `bot_guid` BIGINT UNSIGNED NOT NULL,
    `pattern_name` VARCHAR(64) NOT NULL,
    `is_active` TINYINT(1) NOT NULL DEFAULT 0,
    `is_scheduled` TINYINT(1) NOT NULL DEFAULT 1,
    `schedule_priority` INT NOT NULL DEFAULT 0,
    `next_login` TIMESTAMP NULL DEFAULT NULL,
    `next_logout` TIMESTAMP NULL DEFAULT NULL,
    `next_retry` TIMESTAMP NULL DEFAULT NULL,
    `last_login` TIMESTAMP NULL DEFAULT NULL,
    `last_logout` TIMESTAMP NULL DEFAULT NULL,
    `last_activity` TIMESTAMP NULL DEFAULT NULL,
    `last_calculation` TIMESTAMP NULL DEFAULT NULL,
    `total_sessions` INT UNSIGNED NOT NULL DEFAULT 0,
    `total_playtime` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `consecutive_failures` INT UNSIGNED NOT NULL DEFAULT 0,
    `last_failure_reason` VARCHAR(255) DEFAULT NULL,
    `current_session_start` TIMESTAMP NULL DEFAULT NULL,
    `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`bot_guid`),
    FOREIGN KEY (`pattern_name`) REFERENCES `playerbot_activity_patterns` (`pattern_name`) ON DELETE CASCADE ON UPDATE CASCADE,
    INDEX `idx_active_bots` (`is_active`, `is_scheduled`),
    INDEX `idx_login_schedule` (`next_login`, `is_active`),
    INDEX `idx_logout_schedule` (`next_logout`, `is_active`),
    INDEX `idx_pattern_schedule` (`pattern_name`, `is_scheduled`),
    INDEX `idx_schedule_priority` (`schedule_priority`, `is_scheduled`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Bot login/logout scheduling';

-- Bot spawn event logging
CREATE TABLE IF NOT EXISTS `playerbot_spawn_log` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `bot_guid` BIGINT UNSIGNED NOT NULL,
    `event_type` ENUM('SPAWN_REQUEST', 'SPAWN_SUCCESS', 'SPAWN_FAILURE', 'DESPAWN') NOT NULL,
    `event_timestamp` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    `zone_id` INT UNSIGNED DEFAULT NULL,
    `map_id` INT UNSIGNED DEFAULT NULL,
    `position_x` FLOAT DEFAULT NULL,
    `position_y` FLOAT DEFAULT NULL,
    `position_z` FLOAT DEFAULT NULL,
    `error_message` VARCHAR(512) DEFAULT NULL,
    `correlation_id` VARCHAR(64) DEFAULT NULL,
    PRIMARY KEY (`id`),
    INDEX `idx_bot_events` (`bot_guid`, `event_timestamp`),
    INDEX `idx_event_type_time` (`event_type`, `event_timestamp`),
    INDEX `idx_zone_spawns` (`zone_id`, `event_type`, `event_timestamp`),
    INDEX `idx_correlation` (`correlation_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Bot spawn event tracking';

-- Zone population management
CREATE TABLE IF NOT EXISTS `playerbot_zone_populations` (
    `zone_id` INT UNSIGNED NOT NULL,
    `map_id` INT UNSIGNED NOT NULL,
    `zone_name` VARCHAR(128) NOT NULL,
    `min_level` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `max_level` TINYINT UNSIGNED NOT NULL DEFAULT 80,
    `max_population` INT UNSIGNED NOT NULL DEFAULT 10,
    `current_bots` INT UNSIGNED NOT NULL DEFAULT 0,
    `target_population` INT UNSIGNED NOT NULL DEFAULT 0,
    `spawn_weight` FLOAT NOT NULL DEFAULT 1.0,
    `population_multiplier` FLOAT NOT NULL DEFAULT 1.0,
    `spawn_cooldown_minutes` INT UNSIGNED NOT NULL DEFAULT 5,
    `last_spawn` TIMESTAMP NULL DEFAULT NULL,
    `last_updated` TIMESTAMP NULL DEFAULT NULL,
    `total_spawns_today` INT UNSIGNED NOT NULL DEFAULT 0,
    `is_enabled` TINYINT(1) NOT NULL DEFAULT 1,
    `is_starter_zone` TINYINT(1) NOT NULL DEFAULT 0,
    `is_endgame_zone` TINYINT(1) NOT NULL DEFAULT 0,
    `faction_preference` ENUM('ALLIANCE', 'HORDE', 'NEUTRAL') DEFAULT 'NEUTRAL',
    `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`zone_id`, `map_id`),
    INDEX `idx_zone_enabled` (`is_enabled`, `spawn_weight`),
    INDEX `idx_zone_levels` (`min_level`, `max_level`, `is_enabled`),
    INDEX `idx_zone_capacity` (`current_bots`, `max_population`, `is_enabled`),
    INDEX `idx_starter_zones` (`is_starter_zone`, `is_enabled`),
    INDEX `idx_endgame_zones` (`is_endgame_zone`, `is_enabled`),
    INDEX `idx_target_population` (`target_population`, `is_enabled`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Zone population control';

-- General lifecycle event logging
CREATE TABLE IF NOT EXISTS `playerbot_lifecycle_events` (
    `event_id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `event_timestamp` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    `event_category` VARCHAR(64) NOT NULL,
    `event_type` VARCHAR(64) NOT NULL,
    `severity` ENUM('DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL') NOT NULL DEFAULT 'INFO',
    `component` VARCHAR(64) DEFAULT NULL,
    `bot_guid` BIGINT UNSIGNED DEFAULT NULL,
    `account_id` BIGINT UNSIGNED DEFAULT NULL,
    `message` TEXT,
    `details` TEXT DEFAULT NULL,
    `execution_time_ms` INT UNSIGNED DEFAULT NULL,
    `memory_usage_mb` FLOAT DEFAULT NULL,
    `active_bots_count` INT UNSIGNED DEFAULT NULL,
    `correlation_id` VARCHAR(64) DEFAULT NULL,
    PRIMARY KEY (`event_id`),
    INDEX `idx_event_timestamp` (`event_timestamp`),
    INDEX `idx_event_category` (`event_category`, `event_timestamp`),
    INDEX `idx_event_severity` (`severity`, `event_timestamp`),
    INDEX `idx_bot_events` (`bot_guid`, `event_timestamp`),
    INDEX `idx_correlation` (`correlation_id`),
    INDEX `idx_component` (`component`, `event_timestamp`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='General lifecycle event logging';

-- ============================================================================
-- CHARACTER SYSTEM TABLES (Required by BotNameMgr.cpp)
-- ============================================================================

-- Bot name database for character creation
CREATE TABLE IF NOT EXISTS `playerbots_names` (
    `name_id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
    `name` VARCHAR(12) NOT NULL,
    `gender` TINYINT UNSIGNED NOT NULL COMMENT '0=Male, 1=Female',
    `race_mask` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Bitmask of allowed races (0=all)',
    `is_taken` TINYINT(1) NOT NULL DEFAULT 0,
    `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`name_id`),
    UNIQUE KEY `idx_unique_name` (`name`),
    INDEX `idx_gender_available` (`gender`, `is_taken`),
    INDEX `idx_race_names` (`race_mask`, `is_taken`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Available bot names for character creation';

-- Track which names are currently in use
CREATE TABLE IF NOT EXISTS `playerbots_names_used` (
    `name_id` INT UNSIGNED NOT NULL,
    `character_guid` BIGINT UNSIGNED NOT NULL,
    `assigned_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`name_id`),
    UNIQUE KEY `idx_unique_char` (`character_guid`),
    FOREIGN KEY (`name_id`) REFERENCES `playerbots_names` (`name_id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Names currently assigned to characters';

-- ============================================================================
-- CHARACTER DISTRIBUTION TABLES (Required by existing schema files)
-- ============================================================================

-- Class popularity statistics for balanced bot creation
CREATE TABLE IF NOT EXISTS `playerbots_class_popularity` (
    `class_id` TINYINT UNSIGNED NOT NULL,
    `popularity_weight` FLOAT NOT NULL DEFAULT 1.0,
    `min_level` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `max_level` TINYINT UNSIGNED NOT NULL DEFAULT 80,
    `enabled` TINYINT(1) NOT NULL DEFAULT 1,
    `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`class_id`),
    INDEX `idx_class_enabled` (`enabled`, `popularity_weight`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Class popularity for bot generation';

-- Gender distribution preferences
CREATE TABLE IF NOT EXISTS `playerbots_gender_distribution` (
    `race_id` TINYINT UNSIGNED NOT NULL,
    `male_percentage` FLOAT NOT NULL DEFAULT 50.0,
    `female_percentage` FLOAT NOT NULL DEFAULT 50.0,
    `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`race_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Gender distribution by race';

-- Race-class distribution matrix
CREATE TABLE IF NOT EXISTS `playerbots_race_class_distribution` (
    `race_id` TINYINT UNSIGNED NOT NULL,
    `class_id` TINYINT UNSIGNED NOT NULL,
    `distribution_weight` FLOAT NOT NULL DEFAULT 1.0,
    `enabled` TINYINT(1) NOT NULL DEFAULT 1,
    `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`race_id`, `class_id`),
    INDEX `idx_race_class_enabled` (`enabled`, `distribution_weight`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Race-class combination weights';

-- Combined race-class-gender preferences
CREATE TABLE IF NOT EXISTS `playerbots_race_class_gender` (
    `race_id` TINYINT UNSIGNED NOT NULL,
    `class_id` TINYINT UNSIGNED NOT NULL,
    `gender` TINYINT UNSIGNED NOT NULL,
    `preference_weight` FLOAT NOT NULL DEFAULT 1.0,
    `enabled` TINYINT(1) NOT NULL DEFAULT 1,
    `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`race_id`, `class_id`, `gender`),
    INDEX `idx_combo_enabled` (`enabled`, `preference_weight`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Combined race-class-gender preferences';

-- ============================================================================
-- ACCOUNT MANAGEMENT TABLES (Required by migration files)
-- ============================================================================

-- Playerbot account tracking
CREATE TABLE IF NOT EXISTS `playerbot_accounts` (
    `account_id` BIGINT UNSIGNED NOT NULL,
    `account_type` ENUM('RANDOM_BOT', 'GUILD_BOT', 'ARENA_BOT', 'CUSTOM_BOT') NOT NULL DEFAULT 'RANDOM_BOT',
    `max_characters` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `current_characters` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `is_active` TINYINT(1) NOT NULL DEFAULT 1,
    `owner_account_id` BIGINT UNSIGNED DEFAULT NULL COMMENT 'For guild/arena bots',
    `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`account_id`),
    INDEX `idx_account_type` (`account_type`, `is_active`),
    INDEX `idx_owner_account` (`owner_account_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Playerbot account management';

-- Character distribution tracking per account
CREATE TABLE IF NOT EXISTS `playerbot_character_distribution` (
    `account_id` BIGINT UNSIGNED NOT NULL,
    `character_guid` BIGINT UNSIGNED NOT NULL,
    `character_level` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `character_class` TINYINT UNSIGNED NOT NULL,
    `character_race` TINYINT UNSIGNED NOT NULL,
    `character_gender` TINYINT UNSIGNED NOT NULL,
    `zone_preference` INT UNSIGNED DEFAULT NULL,
    `activity_pattern` VARCHAR(64) DEFAULT NULL,
    `is_active` TINYINT(1) NOT NULL DEFAULT 1,
    `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`account_id`, `character_guid`),
    UNIQUE KEY `idx_unique_character` (`character_guid`),
    FOREIGN KEY (`account_id`) REFERENCES `playerbot_accounts` (`account_id`) ON DELETE CASCADE ON UPDATE CASCADE,
    INDEX `idx_character_active` (`is_active`, `character_level`),
    INDEX `idx_activity_pattern` (`activity_pattern`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Character distribution per account';

-- ============================================================================
-- MIGRATION TRACKING TABLE
-- ============================================================================

-- Track applied database migrations
CREATE TABLE IF NOT EXISTS `playerbot_migrations` (
    `version` VARCHAR(16) NOT NULL,
    `description` VARCHAR(255) NOT NULL,
    `applied_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    `checksum` VARCHAR(64) DEFAULT NULL,
    PRIMARY KEY (`version`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Database migration tracking';

-- ============================================================================
-- STORED PROCEDURES FOR MAINTENANCE
-- ============================================================================

DELIMITER $$

-- Cleanup old spawn logs (keep last 30 days)
CREATE PROCEDURE IF NOT EXISTS `sp_cleanup_spawn_logs`()
BEGIN
    DELETE FROM `playerbot_spawn_log`
    WHERE `event_timestamp` < DATE_SUB(NOW(), INTERVAL 30 DAY);

    SELECT ROW_COUNT() as 'Cleaned spawn log entries';
END$$

-- Cleanup old lifecycle events (keep errors/critical forever, others 7 days)
CREATE PROCEDURE IF NOT EXISTS `sp_cleanup_lifecycle_events`()
BEGIN
    DELETE FROM `playerbot_lifecycle_events`
    WHERE `event_timestamp` < DATE_SUB(NOW(), INTERVAL 7 DAY)
    AND `severity` NOT IN ('ERROR', 'CRITICAL');

    SELECT ROW_COUNT() as 'Cleaned lifecycle event entries';
END$$

-- Update zone population counts
CREATE PROCEDURE IF NOT EXISTS `sp_update_zone_populations`()
BEGIN
    UPDATE `playerbot_zone_populations` pzp
    SET `current_bots` = (
        SELECT COUNT(*)
        FROM `playerbot_schedules` ps
        JOIN `playerbot_character_distribution` pcd ON ps.bot_guid = pcd.character_guid
        WHERE ps.is_active = 1
        AND pcd.zone_preference = pzp.zone_id
    );

    SELECT 'Zone populations updated' as 'Status';
END$$

-- Get system statistics
CREATE PROCEDURE IF NOT EXISTS `sp_get_system_stats`()
BEGIN
    SELECT
        (SELECT COUNT(*) FROM `playerbot_schedules` WHERE `is_active` = 1) as `active_bots`,
        (SELECT COUNT(*) FROM `playerbot_schedules` WHERE `is_scheduled` = 1) as `scheduled_bots`,
        (SELECT COUNT(*) FROM `playerbot_accounts` WHERE `is_active` = 1) as `active_accounts`,
        (SELECT COUNT(*) FROM `playerbots_names` WHERE `is_taken` = 0) as `available_names`,
        (SELECT SUM(`current_bots`) FROM `playerbot_zone_populations`) as `total_zone_population`,
        (SELECT COUNT(*) FROM `playerbot_activity_patterns` WHERE `enabled` = 1) as `active_patterns`;
END$$

DELIMITER ;

-- ============================================================================
-- SCHEMA VALIDATION
-- ============================================================================

-- Verify all required tables exist
SELECT 'Schema validation complete' as 'Status',
    (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES
     WHERE TABLE_SCHEMA = DATABASE()
     AND TABLE_NAME LIKE 'playerbot%') as 'Tables created',
    NOW() as 'Created at';