-- Migration 001: Initial Playerbot Schema
-- Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
-- Version: 001
-- Component: Initial Schema Creation

-- =============================================================================
-- Create Initial Playerbot Tables (Version 001)
-- =============================================================================

-- Activity patterns table
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
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Names table (no foreign keys)
CREATE TABLE IF NOT EXISTS `playerbots_names` (
    `name_id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
    `name` VARCHAR(12) NOT NULL,
    `gender` TINYINT UNSIGNED NOT NULL DEFAULT 2 COMMENT '0=male, 1=female, 2=neutral',
    `race_mask` INT UNSIGNED NOT NULL DEFAULT 4294967295 COMMENT 'Bitmask of compatible races',
    `is_taken` TINYINT(1) NOT NULL DEFAULT 0,
    `is_used` TINYINT(1) NOT NULL DEFAULT 0,
    `used_by_guid` BIGINT UNSIGNED NULL DEFAULT NULL,
    `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`name_id`),
    UNIQUE KEY `idx_unique_name` (`name`),
    INDEX `idx_available_names` (`is_taken`, `gender`, `race_mask`),
    INDEX `idx_used_names` (`is_used`, `used_by_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Class popularity table (no foreign keys)
CREATE TABLE IF NOT EXISTS `playerbots_class_popularity` (
    `class_id` TINYINT UNSIGNED NOT NULL,
    `class_name` VARCHAR(32) NOT NULL,
    `popularity_weight` FLOAT NOT NULL DEFAULT 1.0,
    `min_level` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `max_level` TINYINT UNSIGNED NOT NULL DEFAULT 80,
    `enabled` TINYINT(1) NOT NULL DEFAULT 1,
    `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`class_id`),
    INDEX `idx_enabled_classes` (`enabled`, `popularity_weight`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Names used table (with foreign key to names table)
CREATE TABLE IF NOT EXISTS `playerbots_names_used` (
    `name_id` INT UNSIGNED NOT NULL,
    `character_guid` BIGINT UNSIGNED NOT NULL,
    `assigned_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`character_guid`),
    UNIQUE KEY `idx_unique_name` (`name_id`),
    CONSTRAINT `fk_names_used_name_id` FOREIGN KEY (`name_id`) REFERENCES `playerbots_names` (`name_id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;