USE `roleplay`;
CREATE TABLE IF NOT EXISTS `player_morph` (
  `playerGuid` bigint unsigned NOT NULL,
  `morphDisplayId` int unsigned NOT NULL DEFAULT 0,
  `scale` float NOT NULL DEFAULT 1,
  PRIMARY KEY (`playerGuid`)
) ENGINE=InnoDB;
