-- Migration from 001_initial_schema to 002_account_management
-- Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
-- Version: 001 -> 002
-- Component: Account Management System Migration

-- =============================================================================
-- UPGRADE: 001 -> 002
-- =============================================================================

-- Check current version
SELECT COUNT(*) as version_check FROM information_schema.tables
WHERE table_schema = DATABASE() AND table_name = 'playerbot_accounts';

-- If version_check = 0, proceed with upgrade
-- Note: This should be wrapped in application logic that checks the count

-- =============================================================================
-- Create Account Management Tables (Added in Version 002)
-- =============================================================================

-- Bot accounts table
CREATE TABLE IF NOT EXISTS `playerbot_accounts` (
    `account_id` INT UNSIGNED NOT NULL,
    `account_name` VARCHAR(32) NOT NULL,
    `email` VARCHAR(64) NOT NULL,
    `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    `is_bot_account` BOOLEAN NOT NULL DEFAULT TRUE,
    `bot_count` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `max_bots` TINYINT UNSIGNED NOT NULL DEFAULT 10,
    `account_status` ENUM('ACTIVE', 'SUSPENDED', 'DISABLED') NOT NULL DEFAULT 'ACTIVE',
    `last_activity` TIMESTAMP NULL,

    PRIMARY KEY (`account_id`),
    UNIQUE KEY `uk_account_name` (`account_name`),
    UNIQUE KEY `uk_email` (`email`),
    INDEX `idx_bot_accounts` (`is_bot_account`, `account_status`),
    INDEX `idx_activity` (`last_activity`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
COMMENT='Bot account management';

-- Character distribution table
CREATE TABLE IF NOT EXISTS `playerbot_character_distribution` (
    `race` TINYINT UNSIGNED NOT NULL,
    `class` TINYINT UNSIGNED NOT NULL,
    `faction` TINYINT UNSIGNED NOT NULL,
    `target_percentage` FLOAT NOT NULL DEFAULT 0.0,
    `current_count` INT UNSIGNED NOT NULL DEFAULT 0,
    `max_count` INT UNSIGNED NOT NULL DEFAULT 1000,
    `is_enabled` BOOLEAN NOT NULL DEFAULT TRUE,
    `priority_weight` FLOAT NOT NULL DEFAULT 1.0,
    `last_created` TIMESTAMP NULL,
    `created_today` INT UNSIGNED NOT NULL DEFAULT 0,

    PRIMARY KEY (`race`, `class`),
    INDEX `idx_faction` (`faction`, `is_enabled`),
    INDEX `idx_distribution` (`target_percentage`, `current_count`),
    INDEX `idx_priority` (`priority_weight`, `is_enabled`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
COMMENT='Character race/class distribution control';

-- =============================================================================
-- Add Migration Tracking
-- =============================================================================

-- Create migration tracking table if it doesn't exist
CREATE TABLE IF NOT EXISTS `playerbot_migrations` (
    `version` VARCHAR(20) NOT NULL,
    `description` VARCHAR(255) NOT NULL,
    `applied_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    `checksum` VARCHAR(64) NULL,
    `execution_time_ms` INT UNSIGNED NULL,

    PRIMARY KEY (`version`),
    INDEX `idx_applied` (`applied_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
COMMENT='Database migration tracking';

-- Record this migration
INSERT INTO `playerbot_migrations` (`version`, `description`)
VALUES ('002', 'Account Management System - Added bot accounts and character distribution tables')
ON DUPLICATE KEY UPDATE applied_at = CURRENT_TIMESTAMP;

-- =============================================================================
-- DOWNGRADE: 002 -> 001
-- =============================================================================

-- DOWNGRADE SCRIPT (to be used manually if needed):
/*
-- Remove migration record
DELETE FROM `playerbot_migrations` WHERE `version` = '002';

-- Drop tables added in version 002
DROP TABLE IF EXISTS `playerbot_character_distribution`;
DROP TABLE IF EXISTS `playerbot_accounts`;

-- Log downgrade
INSERT INTO `playerbot_migrations` (`version`, `description`)
VALUES ('001_downgrade', 'Downgraded from 002 to 001 - Removed account management tables');
*/

-- =============================================================================
-- Version Validation
-- =============================================================================

-- Verify migration success
SELECT
    'Migration 001->002 Status' as migration_status,
    (SELECT COUNT(*) FROM information_schema.tables
     WHERE table_schema = DATABASE() AND table_name = 'playerbot_accounts') as accounts_table_exists,
    (SELECT COUNT(*) FROM information_schema.tables
     WHERE table_schema = DATABASE() AND table_name = 'playerbot_character_distribution') as distribution_table_exists,
    (SELECT COUNT(*) FROM `playerbot_migrations` WHERE version = '002') as migration_recorded;

-- Migration complete marker
SELECT 'MIGRATION_001_TO_002_COMPLETE' as status, NOW() as completed_at;