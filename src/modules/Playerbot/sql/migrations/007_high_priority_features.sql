-- ============================================================================
-- Migration 007: High Priority Features
-- ============================================================================
-- Purpose: Add database tables for BotAccountMgr metadata and BotLifecycleMgr events
-- Author: PlayerBot Module
-- Date: 2025-11-10
-- ============================================================================

-- --------------------------------------------------------
-- Bot Account Metadata Table
-- --------------------------------------------------------
-- Purpose: Store extended metadata for bot accounts (email, creation time, etc.)
-- Used by: BotAccountMgr::StoreAccountMetadata()

CREATE TABLE IF NOT EXISTS `bot_account_metadata` (
  `account_id` INT UNSIGNED NOT NULL,
  `bnet_account_id` INT UNSIGNED NOT NULL,
  `email` VARCHAR(255) NULL DEFAULT NULL,
  `character_count` INT UNSIGNED NOT NULL DEFAULT 0,
  `expansion` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=Classic, 1=TBC, 2=WotLK, etc.',
  `locale` VARCHAR(8) NOT NULL DEFAULT 'enUS',
  `last_ip` VARCHAR(45) NULL DEFAULT NULL COMMENT 'IPv4 or IPv6',
  `join_date` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `last_login` TIMESTAMP NULL DEFAULT NULL,
  `total_time_played` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Total playtime in seconds',
  `notes` TEXT NULL DEFAULT NULL COMMENT 'Admin notes',
  `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,

  PRIMARY KEY (`account_id`),
  UNIQUE KEY `idx_bnet_account` (`bnet_account_id`),
  KEY `idx_email` (`email`),
  KEY `idx_character_count` (`character_count`),
  KEY `idx_expansion` (`expansion`),
  KEY `idx_last_login` (`last_login`)

) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
  COMMENT='Extended metadata for bot accounts';

-- Add foreign key constraint to playerbot_accounts if it exists
ALTER TABLE `bot_account_metadata`
  ADD CONSTRAINT `fk_bot_account_metadata`
  FOREIGN KEY (`account_id`) REFERENCES `playerbot_accounts` (`account_id`)
  ON DELETE CASCADE ON UPDATE CASCADE;

-- --------------------------------------------------------
-- Bot Lifecycle Events Table
-- --------------------------------------------------------
-- Purpose: Track all bot lifecycle events (spawn, despawn, state changes, errors)
-- Used by: BotLifecycleMgr::RecordEvent(), UpdateZonePopulations(), CleanupOldEvents()

CREATE TABLE IF NOT EXISTS `bot_lifecycle_events` (
  `event_id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  `bot_guid` INT UNSIGNED NULL DEFAULT NULL COMMENT 'NULL for system-wide events',
  `account_id` INT UNSIGNED NULL DEFAULT NULL,
  `zone_id` INT UNSIGNED NULL DEFAULT NULL,
  `event_category` VARCHAR(50) NOT NULL COMMENT 'SPAWN, STATE, RESOURCE, GROUP, SYSTEM',
  `event_type` VARCHAR(50) NOT NULL COMMENT 'Specific event type within category',
  `severity` ENUM('DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL') NOT NULL DEFAULT 'INFO',
  `message` TEXT NULL DEFAULT NULL,
  `metadata` JSON NULL DEFAULT NULL COMMENT 'Additional event data in JSON format',
  `timestamp` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `processed` TINYINT(1) NOT NULL DEFAULT 0 COMMENT 'For event processing workflows',

  PRIMARY KEY (`event_id`),
  KEY `idx_bot_guid` (`bot_guid`),
  KEY `idx_account_id` (`account_id`),
  KEY `idx_zone_id` (`zone_id`),
  KEY `idx_category_type` (`event_category`, `event_type`),
  KEY `idx_severity` (`severity`),
  KEY `idx_timestamp` (`timestamp`),
  KEY `idx_processed` (`processed`),
  KEY `idx_cleanup` (`timestamp`, `severity`) COMMENT 'For cleanup queries'

) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
  COMMENT='Bot lifecycle event log for monitoring and debugging'
  PARTITION BY RANGE (TO_DAYS(timestamp)) (
    PARTITION p_before_2025 VALUES LESS THAN (TO_DAYS('2025-01-01')),
    PARTITION p_2025_q1 VALUES LESS THAN (TO_DAYS('2025-04-01')),
    PARTITION p_2025_q2 VALUES LESS THAN (TO_DAYS('2025-07-01')),
    PARTITION p_2025_q3 VALUES LESS THAN (TO_DAYS('2025-10-01')),
    PARTITION p_2025_q4 VALUES LESS THAN (TO_DAYS('2026-01-01')),
    PARTITION p_future VALUES LESS THAN MAXVALUE
  );

-- --------------------------------------------------------
-- Zone Population Cache Table
-- --------------------------------------------------------
-- Purpose: Cache current zone populations for fast lookup
-- Used by: BotLifecycleMgr::UpdateZonePopulations()

CREATE TABLE IF NOT EXISTS `bot_zone_population` (
  `zone_id` INT UNSIGNED NOT NULL,
  `bot_count` INT UNSIGNED NOT NULL DEFAULT 0,
  `player_count` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Real player count',
  `total_count` INT UNSIGNED NOT NULL DEFAULT 0,
  `max_capacity` INT UNSIGNED NOT NULL DEFAULT 100,
  `density_score` FLOAT NOT NULL DEFAULT 0.0 COMMENT 'bot_count / max_capacity',
  `last_updated` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,

  PRIMARY KEY (`zone_id`),
  KEY `idx_bot_count` (`bot_count`),
  KEY `idx_density` (`density_score`),
  KEY `idx_last_updated` (`last_updated`)

) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
  COMMENT='Real-time zone population tracking for bot distribution';

-- --------------------------------------------------------
-- Stored Procedures for Common Operations
-- --------------------------------------------------------

DELIMITER $$

-- Procedure: Record a lifecycle event
CREATE PROCEDURE IF NOT EXISTS `sp_record_lifecycle_event`(
  IN p_bot_guid INT UNSIGNED,
  IN p_account_id INT UNSIGNED,
  IN p_zone_id INT UNSIGNED,
  IN p_category VARCHAR(50),
  IN p_type VARCHAR(50),
  IN p_severity ENUM('DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL'),
  IN p_message TEXT,
  IN p_metadata JSON
)
BEGIN
  INSERT INTO `bot_lifecycle_events` (
    `bot_guid`, `account_id`, `zone_id`, `event_category`,
    `event_type`, `severity`, `message`, `metadata`
  ) VALUES (
    p_bot_guid, p_account_id, p_zone_id, p_category,
    p_type, p_severity, p_message, p_metadata
  );
END$$

-- Procedure: Cleanup old events (called periodically)
CREATE PROCEDURE IF NOT EXISTS `sp_cleanup_old_lifecycle_events`(
  IN p_retention_days INT UNSIGNED
)
BEGIN
  DECLARE v_cutoff_timestamp TIMESTAMP;
  SET v_cutoff_timestamp = DATE_SUB(NOW(), INTERVAL p_retention_days DAY);

  -- Delete old DEBUG and INFO events
  DELETE FROM `bot_lifecycle_events`
  WHERE `timestamp` < v_cutoff_timestamp
    AND `severity` IN ('DEBUG', 'INFO')
    AND `processed` = 1;

  -- Keep WARNING/ERROR/CRITICAL events longer (2x retention)
  DELETE FROM `bot_lifecycle_events`
  WHERE `timestamp` < DATE_SUB(NOW(), INTERVAL (p_retention_days * 2) DAY)
    AND `severity` IN ('WARNING', 'ERROR', 'CRITICAL')
    AND `processed` = 1;

  -- Optimize table after cleanup
  OPTIMIZE TABLE `bot_lifecycle_events`;
END$$

-- Procedure: Update zone population
CREATE PROCEDURE IF NOT EXISTS `sp_update_zone_population`(
  IN p_zone_id INT UNSIGNED,
  IN p_bot_count INT UNSIGNED,
  IN p_player_count INT UNSIGNED,
  IN p_max_capacity INT UNSIGNED
)
BEGIN
  INSERT INTO `bot_zone_population` (
    `zone_id`, `bot_count`, `player_count`, `total_count`,
    `max_capacity`, `density_score`
  ) VALUES (
    p_zone_id, p_bot_count, p_player_count,
    (p_bot_count + p_player_count), p_max_capacity,
    (p_bot_count / GREATEST(p_max_capacity, 1))
  )
  ON DUPLICATE KEY UPDATE
    `bot_count` = p_bot_count,
    `player_count` = p_player_count,
    `total_count` = (p_bot_count + p_player_count),
    `max_capacity` = p_max_capacity,
    `density_score` = (p_bot_count / GREATEST(p_max_capacity, 1)),
    `last_updated` = CURRENT_TIMESTAMP;
END$$

-- Procedure: Get zone population statistics
CREATE PROCEDURE IF NOT EXISTS `sp_get_zone_population_stats`(
  IN p_zone_id INT UNSIGNED
)
BEGIN
  SELECT
    `zone_id`,
    `bot_count`,
    `player_count`,
    `total_count`,
    `max_capacity`,
    `density_score`,
    `last_updated`,
    CASE
      WHEN `density_score` >= 0.9 THEN 'FULL'
      WHEN `density_score` >= 0.7 THEN 'HIGH'
      WHEN `density_score` >= 0.4 THEN 'MEDIUM'
      WHEN `density_score` >= 0.1 THEN 'LOW'
      ELSE 'EMPTY'
    END AS `population_status`
  FROM `bot_zone_population`
  WHERE `zone_id` = p_zone_id;
END$$

DELIMITER ;

-- --------------------------------------------------------
-- Indexes for Performance
-- --------------------------------------------------------

-- Composite index for frequent event queries
CREATE INDEX `idx_event_lookup`
  ON `bot_lifecycle_events` (`event_category`, `event_type`, `timestamp`);

-- Index for bot-specific event history
CREATE INDEX `idx_bot_history`
  ON `bot_lifecycle_events` (`bot_guid`, `timestamp` DESC);

-- --------------------------------------------------------
-- Initial Data
-- --------------------------------------------------------

-- Seed some example zone population data (will be overwritten by actual data)
INSERT IGNORE INTO `bot_zone_population` (`zone_id`, `max_capacity`) VALUES
(1, 100),   -- Dun Morogh
(12, 150),  -- Elwynn Forest
(14, 100),  -- Durotar
(85, 100),  -- Tirisfal Glades
(141, 100), -- Teldrassil
(215, 150), -- Mulgore
(3430, 200),-- Dornogal (TWW)
(2248, 200);-- Isle of Dorn (TWW)

-- ============================================================================
-- Migration Complete
-- ============================================================================
