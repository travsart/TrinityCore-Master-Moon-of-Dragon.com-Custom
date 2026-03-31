--
-- Add size column to creature table (TC upstream: spawn size override)
--
ALTER TABLE `creature` ADD COLUMN `size` float NOT NULL DEFAULT 0 AFTER `StringId`;

--
-- Add size and visibility columns to gameobject table (TC upstream: spawn overrides)
--
ALTER TABLE `gameobject` ADD COLUMN `size` float NOT NULL DEFAULT 0 AFTER `StringId`;
ALTER TABLE `gameobject` ADD COLUMN `visibility` float NOT NULL DEFAULT 0 AFTER `size`;
