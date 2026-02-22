-- --------------------------------------------------------
-- Character Management Tables
-- --------------------------------------------------------
USE `playerbot`;
-- Table: playerbot_characters
-- Purpose: Track bot characters and their properties
CREATE TABLE IF NOT EXISTS `playerbot_characters` (
  `guid` INT UNSIGNED NOT NULL,
  `account_id` INT UNSIGNED NOT NULL,
  `name` VARCHAR(50) NOT NULL,
  `race` TINYINT UNSIGNED NOT NULL,
  `class` TINYINT UNSIGNED NOT NULL,
  `level` TINYINT UNSIGNED NOT NULL DEFAULT 1,
  `role` ENUM('tank', 'healer', 'dps', 'hybrid') DEFAULT 'dps',
  `last_login` TIMESTAMP NULL DEFAULT NULL,
  `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`guid`),
  KEY `idx_account` (`account_id`),
  KEY `idx_level` (`level`),
  KEY `idx_role` (`role`),
  CONSTRAINT `fk_bot_account` FOREIGN KEY (`account_id`) 
    REFERENCES `playerbot_accounts` (`account_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci 
  COMMENT='Bot character metadata';
DELETE FROM `playerbot_characters`;
-- --------------------------------------------------------
-- Character Distribution Tables
-- --------------------------------------------------------

-- Table: playerbot_race_distribution
-- Purpose: Configurable race distribution for bot generation
CREATE TABLE IF NOT EXISTS `playerbot_race_distribution` (
  `race` TINYINT UNSIGNED NOT NULL,
  `weight` FLOAT NOT NULL DEFAULT 1.0,
  `male_ratio` FLOAT NOT NULL DEFAULT 0.5,
  `faction` ENUM('alliance', 'horde', 'neutral') NOT NULL,
  `enabled` TINYINT(1) DEFAULT 1,
  PRIMARY KEY (`race`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
  COMMENT='Race distribution weights for bot generation';
  DELETE FROM `playerbot_race_distribution`;

-- Insert default race distribution based on WoW statistics
INSERT INTO `playerbot_race_distribution` (`race`, `weight`, `male_ratio`, `faction`) VALUES
(1, 29.5, 0.52, 'alliance'),  -- Human
(2, 8.7, 0.75, 'horde'),      -- Orc
(3, 7.2, 0.75, 'alliance'),   -- Dwarf
(4, 20.8, 0.35, 'alliance'),  -- Night Elf
(5, 11.2, 0.70, 'horde'),     -- Undead
(6, 6.4, 0.72, 'horde'),      -- Tauren
(7, 5.3, 0.50, 'alliance'),   -- Gnome
(8, 7.1, 0.70, 'horde'),      -- Troll
(10, 35.8, 0.35, 'horde'),    -- Blood Elf
(11, 8.9, 0.40, 'alliance')   -- Draenei
ON DUPLICATE KEY UPDATE weight = VALUES(weight);

-- Table: playerbot_class_distribution
-- Purpose: Configurable class distribution for bot generation
CREATE TABLE IF NOT EXISTS `playerbot_class_distribution` (
  `class` TINYINT UNSIGNED NOT NULL,
  `weight` FLOAT NOT NULL DEFAULT 1.0,
  `tank_capable` TINYINT(1) DEFAULT 0,
  `healer_capable` TINYINT(1) DEFAULT 0,
  `enabled` TINYINT(1) DEFAULT 1,
  PRIMARY KEY (`class`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
  COMMENT='Class distribution weights for bot generation';

-- Insert default class distribution based on WoW statistics
INSERT INTO `playerbot_class_distribution` (`class`, `weight`, `tank_capable`, `healer_capable`) VALUES
(1, 9.8, 1, 0),   -- Warrior
(2, 11.5, 1, 1),  -- Paladin
(3, 11.8, 0, 0),  -- Hunter
(4, 7.3, 0, 0),   -- Rogue
(5, 8.7, 0, 1),   -- Priest
(6, 9.2, 1, 0),   -- Death Knight
(7, 6.2, 0, 1),   -- Shaman
(8, 8.4, 0, 0),   -- Mage
(9, 6.8, 0, 0),   -- Warlock
(10, 1.5, 1, 1),  -- Monk
(11, 10.9, 1, 1), -- Druid
(12, 7.9, 1, 0)   -- Demon Hunter
ON DUPLICATE KEY UPDATE weight = VALUES(weight);
DELETE FROM `playerbot_class_distribution`;

-- Table: playerbot_race_class_multipliers
-- Purpose: Adjust probability of specific race/class combinations
CREATE TABLE IF NOT EXISTS `playerbot_race_class_multipliers` (
  `race` TINYINT UNSIGNED NOT NULL,
  `class` TINYINT UNSIGNED NOT NULL,
  `multiplier` FLOAT NOT NULL DEFAULT 1.0 COMMENT 'Higher = more common',
  PRIMARY KEY (`race`, `class`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
  COMMENT='Race/class combination probability multipliers';

-- Insert popular combinations with higher multipliers
INSERT INTO `playerbot_race_class_multipliers` (`race`, `class`, `multiplier`) VALUES
(10, 2, 2.5),  -- Blood Elf Paladin
(4, 12, 3.0),  -- Night Elf Demon Hunter
(1, 2, 2.0),   -- Human Paladin
(10, 3, 2.0),  -- Blood Elf Hunter
(4, 11, 2.2),  -- Night Elf Druid
(1, 1, 1.8),   -- Human Warrior
(2, 1, 2.0),   -- Orc Warrior
(5, 4, 1.8),   -- Undead Rogue
(6, 11, 2.0),  -- Tauren Druid
(3, 3, 1.8)    -- Dwarf Hunter
ON DUPLICATE KEY UPDATE multiplier = VALUES(multiplier);

DELETE FROM `playerbot_race_class_multipliers`;
-- --------------------------------------------------------
-- Configuration Tables
-- --------------------------------------------------------

-- Table: playerbot_config
-- Purpose: Store runtime configuration values
CREATE TABLE IF NOT EXISTS `playerbot_config` (
  `key` VARCHAR(100) NOT NULL,
  `value` TEXT,
  `description` TEXT,
  `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci 
  COMMENT='Runtime configuration for playerbot system';
DELETE FROM `playerbot_config`;

-- Insert default configuration values
INSERT INTO `playerbot_config` (`key`, `value`, `description`) VALUES
('schema_version', '1', 'Database schema version'),
('max_characters_per_account', '10', 'Maximum characters per bot account'),
('strict_character_limit', '1', 'Enforce character limit strictly'),
('auto_fix_character_limit', '0', 'Automatically fix character limit violations')
ON DUPLICATE KEY UPDATE value = VALUES(value);

-- --------------------------------------------------------
-- Performance Monitoring Tables
-- --------------------------------------------------------

-- Table: playerbot_performance_metrics
-- Purpose: Track performance metrics for optimization
CREATE TABLE IF NOT EXISTS `playerbot_performance_metrics` (
  `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  `metric_name` VARCHAR(100) NOT NULL,
  `value` FLOAT NOT NULL,
  `recorded_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  KEY `idx_metric_time` (`metric_name`, `recorded_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci 
  COMMENT='Performance metrics tracking';
DELETE FROM `playerbot_performance_metrics`;
-- --------------------------------------------------------
-- Database Triggers for Character Count Management
-- --------------------------------------------------------

USE `characters`;
DELIMITER $$

-- Trigger: Update character count on character creation
DROP TRIGGER IF EXISTS `trg_bot_character_insert`$$
CREATE TRIGGER `trg_bot_character_insert` 
AFTER INSERT ON `characters`
FOR EACH ROW
BEGIN
    IF EXISTS (SELECT 1 FROM playerbot.playerbot_accounts WHERE account_id = NEW.account AND is_bot = 1) THEN
        UPDATE playerbot.playerbot_accounts 
        SET character_count = character_count + 1 
        WHERE account_id = NEW.account;
    END IF;
END$$

-- Trigger: Update character count on character deletion
DROP TRIGGER IF EXISTS `trg_bot_character_delete`$$
CREATE TRIGGER `trg_bot_character_delete` 
AFTER DELETE ON `characters`
FOR EACH ROW
BEGIN
    IF EXISTS (SELECT 1 FROM playerbot.playerbot_accounts WHERE account_id = OLD.account AND is_bot = 1) THEN
        UPDATE playerbot.playerbot_accounts 
        SET character_count = GREATEST(0, character_count - 1)
        WHERE account_id = OLD.account;
        
        -- Also release the name
        DELETE FROM playerbot.playerbots_names_used WHERE character_guid = OLD.guid;
    END IF;
END$$

DELIMITER ;

USE `playerbot`;
-- --------------------------------------------------------
-- Indexes for Performance
-- --------------------------------------------------------

-- Helper procedure to add index if not exists (MySQL-compatible)
DROP PROCEDURE IF EXISTS `pb_safe_add_index`;
DELIMITER //
CREATE PROCEDURE `pb_safe_add_index`(
    IN p_table VARCHAR(64),
    IN p_index VARCHAR(64),
    IN p_columns VARCHAR(255)
)
BEGIN
    DECLARE index_exists INT DEFAULT 0;

    SELECT COUNT(*) INTO index_exists
    FROM `information_schema`.`STATISTICS`
    WHERE `TABLE_SCHEMA` = DATABASE()
      AND `TABLE_NAME` = p_table
      AND `INDEX_NAME` = p_index;

    IF index_exists = 0 THEN
        SET @sql = CONCAT('CREATE INDEX `', p_index, '` ON `', p_table, '`(', p_columns, ')');
        PREPARE stmt FROM @sql;
        EXECUTE stmt;
        DEALLOCATE PREPARE stmt;
    END IF;
END //
DELIMITER ;

CALL `pb_safe_add_index`('playerbots_names', 'idx_playerbots_names_gender', 'gender');
CALL `pb_safe_add_index`('playerbots_names', 'idx_playerbots_names_name_gender', 'name, gender');

DROP PROCEDURE IF EXISTS `pb_safe_add_index`;

-- -- --------------------------------------------------------
-- -- Stored Procedures for Name Management
-- -- --------------------------------------------------------
-- DELIMITER $$

-- -- Procedure: AllocateName
-- -- Purpose: Allocate a name to a character
-- DROP PROCEDURE IF EXISTS AllocateName$$
-- CREATE PROCEDURE AllocateName(
--     IN p_gender TINYINT(3),
--     IN p_character_guid INT,
--     OUT p_name_id INT(11),
--     OUT p_name VARCHAR(255),
--     OUT p_success BOOLEAN
-- )
-- BEGIN
--     DECLARE EXIT HANDLER FOR SQLEXCEPTION
--     BEGIN
--         ROLLBACK;
--         SET p_success = FALSE;
--     END;
    
--     START TRANSACTION;
    
--     -- Get random available name
--     CALL GetRandomAvailableName(p_gender, p_name_id, p_name);
    
--     IF p_name_id IS NOT NULL THEN
--         -- Mark name as used
--         INSERT INTO playerbots_names_used (name_id, character_guid)
--         VALUES (p_name_id, p_character_guid);
        
--         SET p_success = TRUE;
--         COMMIT;
--     ELSE
--         SET p_success = FALSE;
--         ROLLBACK;
--     END IF;
-- END$$

-- -- Procedure: ReleaseName
-- -- Purpose: Release a name back to the pool
-- DROP PROCEDURE IF EXISTS ReleaseName$$
-- CREATE PROCEDURE ReleaseName(
--     IN p_character_guid INT
-- )
-- BEGIN
--     DELETE FROM playerbots_names_used 
--     WHERE character_guid = p_character_guid;
-- END$$

-- DELIMITER ;
-- -- --------------------------------------------------------
-- -- Function to check name availability
-- -- --------------------------------------------------------

-- DELIMITER $$

-- DROP FUNCTION IF EXISTS IsNameAvailable$$
-- CREATE FUNCTION IsNameAvailable(p_name VARCHAR(255)) 
-- RETURNS BOOLEAN
-- DETERMINISTIC
-- READS SQL DATA
-- BEGIN
--     DECLARE v_is_available BOOLEAN DEFAULT FALSE;
--     DECLARE v_name_id INT;
    
--     -- Get name_id for the name
--     SELECT name_id INTO v_name_id
--     FROM playerbots_names
--     WHERE name = p_name
--     LIMIT 1;
    
--     -- Check if name_id exists and is not in use
--     IF v_name_id IS NOT NULL THEN
--         SELECT NOT EXISTS(
--             SELECT 1 FROM playerbots_names_used 
--             WHERE name_id = v_name_id
--         ) INTO v_is_available;
--     END IF;
    
--     RETURN v_is_available;
-- END$$
-- DELIMITER ;