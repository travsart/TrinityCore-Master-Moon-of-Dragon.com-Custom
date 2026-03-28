-- ============================================================================
-- Companion Squad System — Characters DB tables
-- Run against: characters
-- ============================================================================
USE `characters`;
CREATE TABLE IF NOT EXISTS `character_companion_squad` (
    `guid`          BIGINT UNSIGNED NOT NULL,
    `slot`          TINYINT UNSIGNED NOT NULL COMMENT '0-4',
    `roster_entry`  INT UNSIGNED NOT NULL,
    PRIMARY KEY (`guid`, `slot`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Per-character companion squad slots';

CREATE TABLE IF NOT EXISTS `character_companion_control` (
    `guid`       BIGINT UNSIGNED NOT NULL,
    `mode`       TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '0=Passive,1=Defend,2=Assist',
    `following`  TINYINT UNSIGNED NOT NULL DEFAULT 1,
    PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Per-character companion control state';
