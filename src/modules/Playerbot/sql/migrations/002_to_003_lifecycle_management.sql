-- Migration from 002_account_management to 003_lifecycle_management
-- Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
-- Version: 002 -> 003
-- Component: Lifecycle Management System Migration

-- =============================================================================
-- UPGRADE: 002 -> 003
-- =============================================================================

-- Check current version
SELECT
    (SELECT COUNT(*) FROM information_schema.tables
     WHERE table_schema = DATABASE() AND table_name = 'playerbot_accounts') as has_v002,
    (SELECT COUNT(*) FROM information_schema.tables
     WHERE table_schema = DATABASE() AND table_name = 'playerbot_activity_patterns') as has_v003;

-- If has_v002 = 1 AND has_v003 = 0, proceed with upgrade

-- =============================================================================
-- Create Lifecycle Management Tables (Added in Version 003)
-- =============================================================================

-- Activity Patterns Table
CREATE TABLE IF NOT EXISTS `playerbot_activity_patterns` (
    `pattern_name` VARCHAR(50) NOT NULL,
    `display_name` VARCHAR(100) NOT NULL,
    `description` TEXT,

    -- Active time ranges (JSON array of hour pairs)
    `active_hours` JSON NOT NULL COMMENT '[{"start": 6, "end": 12}, {"start": 18, "end": 23}]',

    -- Active days of week (JSON array: 0=Sunday, 6=Saturday)
    `active_days` JSON NOT NULL COMMENT '[1, 2, 3, 4, 5]',

    -- Behavioral parameters
    `login_probability` FLOAT NOT NULL DEFAULT 1.0 COMMENT 'Base login probability (0.0-1.0)',
    `min_session_duration` INT UNSIGNED NOT NULL DEFAULT 1800 COMMENT 'Minimum session in seconds',
    `max_session_duration` INT UNSIGNED NOT NULL DEFAULT 14400 COMMENT 'Maximum session in seconds',
    `average_sessions_per_day` INT UNSIGNED NOT NULL DEFAULT 2 COMMENT 'Target sessions per day',
    `prefer_peak_hours` BOOLEAN NOT NULL DEFAULT TRUE COMMENT 'Weight towards peak hours',

    -- Advanced parameters
    `weekend_multiplier` FLOAT NOT NULL DEFAULT 1.5 COMMENT 'Weekend activity boost',
    `peak_hour_bonus` FLOAT NOT NULL DEFAULT 2.0 COMMENT 'Peak hour login bonus',
    `jitter_minutes` INT UNSIGNED NOT NULL DEFAULT 30 COMMENT 'Random time variation',

    -- Metadata
    `is_system_pattern` BOOLEAN NOT NULL DEFAULT FALSE COMMENT 'Built-in pattern (cannot be deleted)',
    `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,

    PRIMARY KEY (`pattern_name`),
    INDEX `idx_system_patterns` (`is_system_pattern`),
    INDEX `idx_updated` (`updated_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
COMMENT='Bot activity patterns for realistic scheduling';

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
    FOREIGN KEY (`pattern_name`) REFERENCES `playerbot_activity_patterns`(`pattern_name`) ON UPDATE CASCADE,
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

-- =============================================================================
-- Create Views for Common Queries
-- =============================================================================

-- Active bot schedules view
CREATE OR REPLACE VIEW `v_active_bot_schedules` AS
SELECT
    s.bot_guid,
    s.pattern_name,
    s.next_login,
    s.next_logout,
    s.is_active,
    s.total_sessions,
    s.total_playtime,
    p.display_name as pattern_display_name,
    p.average_sessions_per_day
FROM `playerbot_schedules` s
JOIN `playerbot_activity_patterns` p ON s.pattern_name = p.pattern_name
WHERE s.is_scheduled = TRUE;

-- Zone population summary view
CREATE OR REPLACE VIEW `v_zone_population_summary` AS
SELECT
    zp.zone_id,
    zp.map_id,
    zp.current_bots,
    zp.target_population,
    zp.max_population,
    zp.spawn_weight,
    zp.is_enabled,
    ROUND((zp.current_bots / NULLIF(zp.target_population, 0)) * 100, 1) as population_percentage,
    zp.last_spawn,
    zp.total_spawns_today
FROM `playerbot_zone_populations` zp
WHERE zp.is_enabled = TRUE;

-- Recent lifecycle events view
CREATE OR REPLACE VIEW `v_recent_lifecycle_events` AS
SELECT
    event_id,
    event_timestamp,
    event_category,
    event_type,
    severity,
    component,
    bot_guid,
    message,
    execution_time_ms
FROM `playerbot_lifecycle_events`
WHERE event_timestamp >= DATE_SUB(NOW(), INTERVAL 1 HOUR)
ORDER BY event_timestamp DESC;

-- =============================================================================
-- Insert Default Data
-- =============================================================================

-- Insert default activity patterns
INSERT IGNORE INTO `playerbot_activity_patterns`
(`pattern_name`, `display_name`, `description`, `active_hours`, `active_days`,
 `login_probability`, `min_session_duration`, `max_session_duration`,
 `average_sessions_per_day`, `prefer_peak_hours`, `weekend_multiplier`,
 `peak_hour_bonus`, `jitter_minutes`, `is_system_pattern`)
VALUES
('default', 'Default Pattern', 'Standard activity pattern with evening gaming focus',
 '[{"start": 6, "end": 9}, {"start": 18, "end": 23}]',
 '[1, 2, 3, 4, 5, 6, 7]',
 0.8, 1800, 14400, 2, TRUE, 1.5, 2.0, 30, TRUE),

('casual', 'Casual Player', 'Light gaming pattern focused on weekends and evenings',
 '[{"start": 19, "end": 22}]',
 '[1, 2, 3, 4, 5, 6, 7]',
 0.6, 900, 7200, 1, TRUE, 2.0, 1.5, 45, TRUE),

('hardcore', 'Hardcore Gamer', 'Intensive gaming pattern with long sessions',
 '[{"start": 8, "end": 12}, {"start": 14, "end": 18}, {"start": 20, "end": 2}]',
 '[1, 2, 3, 4, 5, 6, 7]',
 0.95, 3600, 28800, 3, TRUE, 1.2, 2.5, 15, TRUE),

('weekend', 'Weekend Warrior', 'Weekend-focused gaming pattern',
 '[{"start": 10, "end": 14}, {"start": 19, "end": 24}]',
 '[6, 7]',
 0.9, 2700, 21600, 2, FALSE, 1.0, 1.0, 60, TRUE);

-- Insert sample zone population data
INSERT IGNORE INTO `playerbot_zone_populations`
(`zone_id`, `map_id`, `target_population`, `max_population`, `min_level`, `max_level`,
 `spawn_weight`, `is_enabled`, `is_starter_zone`, `spawn_cooldown_minutes`)
VALUES
-- Human starter areas
(12, 0, 15, 25, 1, 10, 2.0, TRUE, TRUE, 3),    -- Elwynn Forest
(40, 0, 12, 20, 10, 20, 1.5, TRUE, FALSE, 5),  -- Westfall
(11, 0, 20, 35, 20, 30, 1.8, TRUE, FALSE, 5),  -- Duskwood

-- Orc/Troll starter areas
(14, 1, 15, 25, 1, 10, 2.0, TRUE, TRUE, 3),    -- Durotar
(17, 1, 12, 20, 10, 20, 1.5, TRUE, FALSE, 5),  -- The Barrens

-- Major cities (higher populations)
(1519, 0, 50, 100, 1, 80, 3.0, TRUE, FALSE, 2), -- Stormwind City
(1637, 1, 50, 100, 1, 80, 3.0, TRUE, FALSE, 2), -- Orgrimmar

-- High-level zones (lower populations, level restrictions)
(28, 0, 8, 15, 50, 60, 1.0, TRUE, FALSE, 10),  -- Western Plaguelands
(139, 1, 8, 15, 50, 60, 1.0, TRUE, FALSE, 10); -- Eastern Plaguelands

-- =============================================================================
-- Record Migration
-- =============================================================================

INSERT INTO `playerbot_migrations` (`version`, `description`)
VALUES ('003', 'Lifecycle Management System - Added activity patterns, schedules, spawn logging, zone populations, and lifecycle events')
ON DUPLICATE KEY UPDATE applied_at = CURRENT_TIMESTAMP;

-- =============================================================================
-- DOWNGRADE: 003 -> 002
-- =============================================================================

-- DOWNGRADE SCRIPT (to be used manually if needed):
/*
-- Remove migration record
DELETE FROM `playerbot_migrations` WHERE `version` = '003';

-- Drop views
DROP VIEW IF EXISTS `v_recent_lifecycle_events`;
DROP VIEW IF EXISTS `v_zone_population_summary`;
DROP VIEW IF EXISTS `v_active_bot_schedules`;

-- Drop tables (order matters due to foreign keys)
DROP TABLE IF EXISTS `playerbot_schedules`;
DROP TABLE IF EXISTS `playerbot_lifecycle_events`;
DROP TABLE IF EXISTS `playerbot_spawn_log`;
DROP TABLE IF EXISTS `playerbot_zone_populations`;
DROP TABLE IF EXISTS `playerbot_activity_patterns`;

-- Log downgrade
INSERT INTO `playerbot_migrations` (`version`, `description`)
VALUES ('002_downgrade', 'Downgraded from 003 to 002 - Removed lifecycle management tables');
*/

-- =============================================================================
-- Version Validation
-- =============================================================================

-- Verify migration success
SELECT
    'Migration 002->003 Status' as migration_status,
    (SELECT COUNT(*) FROM information_schema.tables
     WHERE table_schema = DATABASE() AND table_name LIKE 'playerbot_%'
     AND table_name IN ('playerbot_activity_patterns', 'playerbot_schedules', 'playerbot_spawn_log',
                        'playerbot_zone_populations', 'playerbot_lifecycle_events')) as lifecycle_tables_created,
    (SELECT COUNT(*) FROM information_schema.views
     WHERE table_schema = DATABASE() AND table_name LIKE 'v_%') as views_created,
    (SELECT COUNT(*) FROM `playerbot_activity_patterns`) as default_patterns_inserted,
    (SELECT COUNT(*) FROM `playerbot_zone_populations`) as sample_zones_inserted,
    (SELECT COUNT(*) FROM `playerbot_migrations` WHERE version = '003') as migration_recorded;

-- Migration complete marker
SELECT 'MIGRATION_002_TO_003_COMPLETE' as status, NOW() as completed_at;