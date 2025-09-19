-- Migration 003: Lifecycle Management System
-- Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
-- Version: 003
-- Component: Lifecycle Management System

-- =============================================================================
-- Create Lifecycle Management Tables (Added in Version 003)
-- =============================================================================

-- Bot Schedules Table
CREATE TABLE IF NOT EXISTS `playerbot_schedules` (
    `bot_guid` INT UNSIGNED NOT NULL,
    `pattern_name` VARCHAR(50) NOT NULL DEFAULT 'default',

    -- Schedule timestamps (UTC)
    `next_login` TIMESTAMP NULL COMMENT 'When bot should next login',
    `next_logout` TIMESTAMP NULL COMMENT 'When bot should logout',
    `last_activity` TIMESTAMP NULL COMMENT 'Last recorded activity',
    `last_calculation` TIMESTAMP DEFAULT CURRENT_TIMESTAMP COMMENT 'When schedule was calculated',

    -- Session statistics
    `total_sessions` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Total login sessions',
    `total_playtime` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Total seconds played',
    `current_session_start` TIMESTAMP NULL COMMENT 'Current session start time',

    -- Schedule state
    `is_scheduled` BOOLEAN NOT NULL DEFAULT FALSE COMMENT 'Bot has active schedule',
    `is_active` BOOLEAN NOT NULL DEFAULT FALSE COMMENT 'Bot is currently logged in',
    `schedule_priority` TINYINT UNSIGNED NOT NULL DEFAULT 5 COMMENT 'Schedule priority (1-10)',

    -- Failure tracking
    `consecutive_failures` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Failed login attempts',
    `last_failure_reason` VARCHAR(100) NULL COMMENT 'Last failure description',
    `next_retry` TIMESTAMP NULL COMMENT 'When to retry after failure',

    -- Metadata
    `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,

    PRIMARY KEY (`bot_guid`),
    INDEX `idx_next_login` (`next_login`),
    INDEX `idx_next_logout` (`next_logout`),
    INDEX `idx_active_schedules` (`is_scheduled`, `is_active`),
    INDEX `idx_pattern` (`pattern_name`),
    INDEX `idx_priority` (`schedule_priority`, `next_login`),
    INDEX `idx_failures` (`consecutive_failures`, `next_retry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
COMMENT='Individual bot schedule state and timing';

-- Spawn Log Table
CREATE TABLE IF NOT EXISTS `playerbot_spawn_log` (
    `log_id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `bot_guid` INT UNSIGNED NOT NULL,
    `account_id` INT UNSIGNED NOT NULL,

    -- Event information
    `event_type` ENUM('SPAWN_REQUEST', 'SPAWN_SUCCESS', 'SPAWN_FAILURE', 'DESPAWN_SCHEDULED', 'DESPAWN_FORCED', 'DESPAWN_ERROR') NOT NULL,
    `event_timestamp` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,

    -- Location information
    `map_id` INT UNSIGNED NULL COMMENT 'Map where event occurred',
    `zone_id` INT UNSIGNED NULL COMMENT 'Zone where event occurred',
    `area_id` INT UNSIGNED NULL COMMENT 'Area where event occurred',
    `position_x` FLOAT NULL,
    `position_y` FLOAT NULL,
    `position_z` FLOAT NULL,

    -- Context information
    `reason` VARCHAR(200) NULL COMMENT 'Reason for spawn/despawn',
    `initiator` ENUM('SCHEDULER', 'SPAWNER', 'ADMIN', 'SYSTEM', 'PLAYER') NOT NULL DEFAULT 'SYSTEM',
    `pattern_name` VARCHAR(50) NULL COMMENT 'Activity pattern used',

    -- Performance metrics
    `processing_time_ms` INT UNSIGNED NULL COMMENT 'Time to process event',
    `queue_wait_time_ms` INT UNSIGNED NULL COMMENT 'Time waiting in queue',

    -- Additional data (JSON for flexibility)
    `extra_data` JSON NULL COMMENT 'Additional event-specific data',

    PRIMARY KEY (`log_id`),
    INDEX `idx_bot_events` (`bot_guid`, `event_timestamp`),
    INDEX `idx_event_type` (`event_type`, `event_timestamp`),
    INDEX `idx_location` (`map_id`, `zone_id`, `event_timestamp`),
    INDEX `idx_pattern_events` (`pattern_name`, `event_timestamp`),
    INDEX `idx_initiator` (`initiator`, `event_timestamp`),
    INDEX `idx_timestamp` (`event_timestamp`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
COMMENT='Historical log of bot spawn and despawn events';

-- Zone Population Table
CREATE TABLE IF NOT EXISTS `playerbot_zone_populations` (
    `zone_id` INT UNSIGNED NOT NULL,
    `map_id` INT UNSIGNED NOT NULL,

    -- Population counts
    `current_bots` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Current bot count in zone',
    `target_population` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Target bot population',
    `max_population` INT UNSIGNED NOT NULL DEFAULT 50 COMMENT 'Maximum allowed bots',
    `min_level` TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT 'Minimum level for zone',
    `max_level` TINYINT UNSIGNED NOT NULL DEFAULT 80 COMMENT 'Maximum level for zone',

    -- Population distribution settings
    `spawn_weight` FLOAT NOT NULL DEFAULT 1.0 COMMENT 'Relative spawn probability',
    `is_enabled` BOOLEAN NOT NULL DEFAULT TRUE COMMENT 'Zone allows bot spawning',
    `is_starter_zone` BOOLEAN NOT NULL DEFAULT FALSE COMMENT 'Low-level character zone',
    `is_endgame_zone` BOOLEAN NOT NULL DEFAULT FALSE COMMENT 'High-level content zone',

    -- Dynamic adjustments
    `population_multiplier` FLOAT NOT NULL DEFAULT 1.0 COMMENT 'Dynamic population adjustment',
    `last_spawn` TIMESTAMP NULL COMMENT 'Last bot spawn in this zone',
    `spawn_cooldown_minutes` INT UNSIGNED NOT NULL DEFAULT 5 COMMENT 'Minimum time between spawns',

    -- Statistics
    `total_spawns_today` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Daily spawn counter',
    `average_session_time` INT UNSIGNED NOT NULL DEFAULT 3600 COMMENT 'Average session length',
    `peak_population` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Peak population reached',
    `peak_timestamp` TIMESTAMP NULL COMMENT 'When peak was reached',

    -- Metadata
    `last_updated` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,

    PRIMARY KEY (`zone_id`, `map_id`),
    INDEX `idx_current_population` (`current_bots`, `target_population`),
    INDEX `idx_spawn_eligibility` (`is_enabled`, `spawn_weight`),
    INDEX `idx_level_range` (`min_level`, `max_level`),
    INDEX `idx_zone_type` (`is_starter_zone`, `is_endgame_zone`),
    INDEX `idx_spawn_cooldown` (`last_spawn`, `spawn_cooldown_minutes`),
    INDEX `idx_updated` (`last_updated`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
COMMENT='Real-time bot population tracking per zone';

-- Lifecycle Events Table
CREATE TABLE IF NOT EXISTS `playerbot_lifecycle_events` (
    `event_id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `event_timestamp` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,

    -- Event classification
    `event_category` ENUM('SCHEDULER', 'SPAWNER', 'SESSION', 'DATABASE', 'SYSTEM', 'ERROR') NOT NULL,
    `event_type` VARCHAR(50) NOT NULL COMMENT 'Specific event type',
    `severity` ENUM('DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL') NOT NULL DEFAULT 'INFO',

    -- Context information
    `component` VARCHAR(50) NOT NULL COMMENT 'Component that generated event',
    `bot_guid` INT UNSIGNED NULL COMMENT 'Related bot (if applicable)',
    `account_id` INT UNSIGNED NULL COMMENT 'Related account (if applicable)',

    -- Event details
    `message` TEXT NOT NULL COMMENT 'Human-readable event description',
    `details` JSON NULL COMMENT 'Structured event data',

    -- Performance data
    `execution_time_ms` INT UNSIGNED NULL COMMENT 'Time to process operation',
    `memory_usage_mb` FLOAT NULL COMMENT 'Memory usage at time of event',
    `active_bots_count` INT UNSIGNED NULL COMMENT 'Number of active bots',

    -- Correlation
    `correlation_id` VARCHAR(36) NULL COMMENT 'UUID for related events',
    `parent_event_id` BIGINT UNSIGNED NULL COMMENT 'Parent event reference',

    PRIMARY KEY (`event_id`),
    INDEX `idx_timestamp` (`event_timestamp`),
    INDEX `idx_category_type` (`event_category`, `event_type`),
    INDEX `idx_severity` (`severity`, `event_timestamp`),
    INDEX `idx_component` (`component`, `event_timestamp`),
    INDEX `idx_bot_events` (`bot_guid`, `event_timestamp`),
    INDEX `idx_correlation` (`correlation_id`),
    INDEX `idx_parent_child` (`parent_event_id`),
    FOREIGN KEY (`parent_event_id`) REFERENCES `playerbot_lifecycle_events`(`event_id`) ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
COMMENT='System-wide lifecycle events for monitoring and debugging';