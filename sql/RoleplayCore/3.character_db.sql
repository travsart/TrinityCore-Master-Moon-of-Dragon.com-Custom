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

ALTER TABLE `character_pet` ADD COLUMN `favorite` tinyint unsigned NOT NULL DEFAULT '0' AFTER `specialization`;

-- Chromie Time Expansion
ALTER TABLE `characters` ADD COLUMN `chromieTimeExpansionId` tinyint unsigned NOT NULL DEFAULT '0' AFTER `transmogOutfitLocked`;

-- Guild flag field
ALTER TABLE `guild` ADD `flags` INT(11) NOT NULL DEFAULT 0 AFTER `leaderguid`;

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


-- ============================================================================
-- Companion Squad System — Seed Data
-- Run against: world
-- Creates creature_template entries and companion_roster entries
-- creature_template entries use 500000+ range to avoid conflicts
-- ============================================================================

-- Companion creature templates (minimal — ScriptName is the key part)
-- These use entry range 500001-500010 for companion NPCs
-- HealthModifier/DamageModifier live in creature_template_difficulty, not creature_template
USE `world`;

INSERT IGNORE INTO `creature_template` (`entry`, `name`, `subname`, `ScriptName`, `faction`, `npcflag`, `unit_flags`, `BaseAttackTime`) VALUES
(500001, 'Companion Warrior',   'Tank',       'CompanionAI', 35, 0, 0, 2000),
(500002, 'Companion Rogue',     'Melee DPS',  'CompanionAI', 35, 0, 0, 2000),
(500003, 'Companion Hunter',    'Ranged DPS', 'CompanionAI', 35, 0, 0, 2000),
(500004, 'Companion Mage',      'Caster DPS', 'CompanionAI', 35, 0, 0, 2000),
(500005, 'Companion Priest',    'Healer',     'CompanionAI', 35, 0, 0, 2000);

-- Display models (required for creatures to be visible)
INSERT IGNORE INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`) VALUES
(500001, 0, 3258,  1, 1),   -- Warrior: Stormwind Guard
(500002, 0, 34761, 1, 1),   -- Rogue: SI:7 Agent
(500003, 0, 55028, 1, 1),   -- Hunter: Stormwind Mage (placeholder)
(500004, 0, 64084, 1, 1),   -- Mage: Stormwind Mage
(500005, 0, 18452, 1, 1);   -- Priest: Stormwind Mage (older model)

-- Difficulty entries for scaling (DifficultyID 0 = normal)
INSERT IGNORE INTO `creature_template_difficulty` (`Entry`, `DifficultyID`, `HealthModifier`, `DamageModifier`) VALUES
(500001, 0, 5.0, 1.0),
(500002, 0, 3.0, 1.5),
(500003, 0, 3.0, 1.2),
(500004, 0, 2.5, 1.3),
(500005, 0, 2.5, 0.5);

-- Equipment
INSERT IGNORE INTO `creature_equip_template` (`CreatureID`, `ID`, `ItemID1`, `AppearanceModID1`, `ItemVisual1`, `ItemID2`, `AppearanceModID2`, `ItemVisual2`, `ItemID3`, `AppearanceModID3`, `ItemVisual3`, `VerifiedBuild`) VALUES
(500001, 1, 1899, 0, 0, 143, 0, 0, 0, 0, 0, 0),    -- Warrior: Shortsword + Buckler
(500002, 1, 2704, 0, 0, 2704, 0, 0, 0, 0, 0, 0),   -- Rogue: dual axes (placeholder)
(500003, 1, 0, 0, 0, 0, 0, 0, 2551, 0, 0, 0),      -- Hunter: Bow (ranged)
(500004, 1, 868, 0, 0, 0, 0, 0, 0, 0, 0, 0),       -- Mage: Staff
(500005, 1, 2075, 0, 0, 0, 0, 0, 0, 0, 0, 0);      -- Priest: Mace

-- Roster entries with real spells
-- spell1 = primary, spell2 = secondary, spell3 = utility
REPLACE INTO `companion_roster` (`entry`, `name`, `role`, `spell1`, `spell2`, `spell3`, `cooldown1`, `cooldown2`, `cooldown3`) VALUES
(500001, 'Warrior',  0, 355, 23922, 29567, 8000, 6000, 8000),    -- Tank: Taunt, Shield Slam, Heroic Strike
(500002, 'Rogue',    1, 1752, 53, 0, 4000, 8000, 0),             -- Melee: Sinister Strike, Backstab
(500003, 'Hunter',   2, 6660, 0, 0, 3000, 0, 0),                 -- Ranged: Shoot
(500004, 'Mage',     3, 133, 116, 0, 4000, 5000, 0),             -- Caster: Fireball, Frostbolt
(500005, 'Priest',   4, 2061, 139, 0, 5000, 12000, 0);           -- Healer: Flash Heal, Renew
