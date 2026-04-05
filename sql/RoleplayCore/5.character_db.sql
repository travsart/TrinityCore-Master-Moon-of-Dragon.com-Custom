-- Favorites

-- ============================================================================
-- Companion Squad System — Database Setup
-- Run against: world, characters, auth (in that order)
-- ============================================================================
USE `world`;
-- ============================================================================
-- WORLD DATABASE: companion_roster
-- ============================================================================
-- Run: mysql -u root -padmin world < "this_file.sql"
-- (Or apply the world section manually)

CREATE TABLE IF NOT EXISTS `companion_roster` (
    `entry`      INT UNSIGNED NOT NULL COMMENT 'creature_template entry',
    `name`       VARCHAR(64) NOT NULL,
    `role`       TINYINT UNSIGNED NOT NULL COMMENT '0=Tank,1=Melee,2=Ranged,3=Caster,4=Healer',
    `spell1`     INT UNSIGNED NOT NULL DEFAULT 0,
    `spell2`     INT UNSIGNED NOT NULL DEFAULT 0,
    `spell3`     INT UNSIGNED NOT NULL DEFAULT 0,
    `cooldown1`  INT UNSIGNED NOT NULL DEFAULT 8000,
    `cooldown2`  INT UNSIGNED NOT NULL DEFAULT 12000,
    `cooldown3`  INT UNSIGNED NOT NULL DEFAULT 15000,
    PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Companion squad roster definitions';

USE `characters`;

ALTER TABLE `character_pet` ADD COLUMN `favorite` tinyint unsigned NOT NULL DEFAULT '0' AFTER `specialization`;

-- Chromie Time Expansion
ALTER TABLE `characters` ADD COLUMN `chromieTimeExpansionId` tinyint unsigned NOT NULL DEFAULT '0' AFTER `transmogOutfitLocked`;

-- ============================================================================
-- WORLD DATABASE: companion_roster
-- ============================================================================
-- Run: mysql -u root -padmin world < "this_file.sql"
-- (Or apply the world section manually)

CREATE TABLE IF NOT EXISTS `companion_roster` (
    `entry`      INT UNSIGNED NOT NULL COMMENT 'creature_template entry',
    `name`       VARCHAR(64) NOT NULL,
    `role`       TINYINT UNSIGNED NOT NULL COMMENT '0=Tank,1=Melee,2=Ranged,3=Caster,4=Healer',
    `spell1`     INT UNSIGNED NOT NULL DEFAULT 0,
    `spell2`     INT UNSIGNED NOT NULL DEFAULT 0,
    `spell3`     INT UNSIGNED NOT NULL DEFAULT 0,
    `cooldown1`  INT UNSIGNED NOT NULL DEFAULT 8000,
    `cooldown2`  INT UNSIGNED NOT NULL DEFAULT 12000,
    `cooldown3`  INT UNSIGNED NOT NULL DEFAULT 15000,
    PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Companion squad roster definitions';


USE `auth`;
-- ============================================================================
-- WORLD DATABASE: companion_roster
-- ============================================================================
-- Run: mysql -u root -padmin world < "this_file.sql"
-- (Or apply the world section manually)

CREATE TABLE IF NOT EXISTS `companion_roster` (
    `entry`      INT UNSIGNED NOT NULL COMMENT 'creature_template entry',
    `name`       VARCHAR(64) NOT NULL,
    `role`       TINYINT UNSIGNED NOT NULL COMMENT '0=Tank,1=Melee,2=Ranged,3=Caster,4=Healer',
    `spell1`     INT UNSIGNED NOT NULL DEFAULT 0,
    `spell2`     INT UNSIGNED NOT NULL DEFAULT 0,
    `spell3`     INT UNSIGNED NOT NULL DEFAULT 0,
    `cooldown1`  INT UNSIGNED NOT NULL DEFAULT 8000,
    `cooldown2`  INT UNSIGNED NOT NULL DEFAULT 12000,
    `cooldown3`  INT UNSIGNED NOT NULL DEFAULT 15000,
    PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Companion squad roster definitions';


DROP PROCEDURE IF EXISTS add_crafting_columns;
DELIMITER //
CREATE PROCEDURE add_crafting_columns()
BEGIN
    IF NOT EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'item_instance_modifiers' AND COLUMN_NAME = 'craftingModifiedStat1') THEN
        ALTER TABLE `item_instance_modifiers`
            ADD COLUMN `craftingModifiedStat1` INT(10) UNSIGNED DEFAULT 0 NULL AFTER `artifactKnowledgeLevel`,
            ADD COLUMN `craftingModifiedStat2` INT(10) UNSIGNED DEFAULT 0 NULL AFTER `craftingModifiedStat1`;
    END IF;
END //
DELIMITER ;
CALL add_crafting_columns();
DROP PROCEDURE IF EXISTS add_crafting_columns;