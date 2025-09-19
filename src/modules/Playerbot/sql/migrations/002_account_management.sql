-- Migration 002: Account Management System
-- Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
-- Version: 002
-- Component: Account Management Enhancement

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