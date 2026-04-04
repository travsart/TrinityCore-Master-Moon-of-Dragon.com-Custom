-- Idempotent custom column additions (MySQL 8.0 compatible)
USE `world`;
DROP PROCEDURE IF EXISTS add_custom_columns;
DELIMITER //
CREATE PROCEDURE add_custom_columns()
BEGIN
    -- gameobject.size
    IF NOT EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'gameobject' AND COLUMN_NAME = 'size') THEN
        ALTER TABLE `gameobject` ADD COLUMN `size` FLOAT NOT NULL DEFAULT '-1';
    END IF;

    -- gameobject.visibility
    IF NOT EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'gameobject' AND COLUMN_NAME = 'visibility') THEN
        ALTER TABLE `gameobject` ADD COLUMN `visibility` FLOAT NOT NULL DEFAULT '256';
    END IF;

    -- creature.size
    IF NOT EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'creature' AND COLUMN_NAME = 'size') THEN
        ALTER TABLE `creature` ADD COLUMN `size` FLOAT NOT NULL DEFAULT '-1' AFTER `StringId`;
    END IF;

    -- npc_vendor.OverrideGoldCost
    IF NOT EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'npc_vendor' AND COLUMN_NAME = 'OverrideGoldCost') THEN
        ALTER TABLE `npc_vendor` ADD COLUMN `OverrideGoldCost` INT NOT NULL DEFAULT '-1';
    END IF;

    -- scrapping_loot_template.ItemType
    IF NOT EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'scrapping_loot_template' AND COLUMN_NAME = 'ItemType') THEN
        ALTER TABLE `scrapping_loot_template` ADD COLUMN `ItemType` tinyint NOT NULL DEFAULT 0 AFTER `Entry`;
    END IF;

    -- creature_loot_template.Reference
    IF NOT EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'creature_loot_template' AND COLUMN_NAME = 'Reference') THEN
        ALTER TABLE `creature_loot_template` ADD COLUMN `Reference` VARCHAR(255) DEFAULT NULL AFTER `Comment`;
    END IF;
END //
DELIMITER ;
CALL add_custom_columns();
DROP PROCEDURE IF EXISTS add_custom_columns;