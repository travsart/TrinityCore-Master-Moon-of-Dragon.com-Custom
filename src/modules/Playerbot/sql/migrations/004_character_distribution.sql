-- Migration 004: Character Distribution Tables
-- Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
-- Version: 004
-- Component: Character Distribution System

-- =============================================================================
-- Create Character Distribution Tables (Added in Version 004)
-- =============================================================================

-- Gender Distribution Table
CREATE TABLE IF NOT EXISTS `playerbots_gender_distribution` (
    `race_id` TINYINT UNSIGNED NOT NULL,
    `male_percentage` FLOAT NOT NULL DEFAULT 50.0,
    `female_percentage` FLOAT NOT NULL DEFAULT 50.0,
    `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`race_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
COMMENT='Gender distribution by race';

-- Race-Class Distribution Matrix
CREATE TABLE IF NOT EXISTS `playerbots_race_class_distribution` (
    `race_id` TINYINT UNSIGNED NOT NULL,
    `class_id` TINYINT UNSIGNED NOT NULL,
    `distribution_weight` FLOAT NOT NULL DEFAULT 1.0,
    `enabled` TINYINT(1) NOT NULL DEFAULT 1,
    `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`race_id`, `class_id`),
    INDEX `idx_race_class_enabled` (`enabled`, `distribution_weight`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
COMMENT='Race-class combination weights';

-- Combined Race-Class-Gender Preferences
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
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
COMMENT='Combined race-class-gender preferences';