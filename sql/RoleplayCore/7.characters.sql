USE `characters`;

-- Idempotent crafting stat modifier columns (MySQL 8.0 compatible)
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