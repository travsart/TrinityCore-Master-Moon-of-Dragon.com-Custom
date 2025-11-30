-- ============================================================================
-- Migration: 008_battle_pet_species.sql
-- Purpose: Add battle pet species metadata for intelligent bot pet selection
-- Author: Playerbot Team
-- Date: 2025-11-28
-- ============================================================================

-- Battle Pet Species table
-- Stores battle pet family types and flags for combat mechanics
-- This data allows bots to:
-- 1. Know pet family type advantages (Aquatic strong vs Elemental)
-- 2. Understand which pets can be captured vs. obtained via quests
-- 3. Make intelligent pet selection for battles

DROP TABLE IF EXISTS `battle_pet_species`;
CREATE TABLE `battle_pet_species` (
    `speciesId` INT UNSIGNED NOT NULL COMMENT 'Battle Pet Species ID',
    `creatureId` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Reference to creature_template.entry',
    `petType` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Pet family: 0=Humanoid, 1=Dragonkin, 2=Flying, 3=Undead, 4=Critter, 5=Magic, 6=Elemental, 7=Beast, 8=Aquatic, 9=Mechanical',
    `flags` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Flags: 0x1=Capturable, 0x2=Tradeable, 0x4=Rare, 0x8=Epic, 0x10=Legendary, 0x20=Untradeable',
    `source` VARCHAR(50) DEFAULT '' COMMENT 'How pet is obtained: wild, vendor, quest, achievement, drop, etc.',
    `description` VARCHAR(255) DEFAULT '' COMMENT 'Optional description',
    PRIMARY KEY (`speciesId`),
    KEY `idx_creature_id` (`creatureId`),
    KEY `idx_pet_type` (`petType`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Battle pet species metadata for bot pet selection';

-- ============================================================================
-- Pet Type Reference:
-- 0 = Humanoid (Strong vs Dragonkin, Weak vs Beast)
-- 1 = Dragonkin (Strong vs Magic, Weak vs Undead)
-- 2 = Flying (Strong vs Aquatic, Weak vs Dragonkin)
-- 3 = Undead (Strong vs Humanoid, Weak vs Aquatic)
-- 4 = Critter (Strong vs Undead, Weak vs Humanoid)
-- 5 = Magic (Strong vs Flying, Weak vs Mechanical)
-- 6 = Elemental (Strong vs Mechanical, Weak vs Critter)
-- 7 = Beast (Strong vs Critter, Weak vs Flying)
-- 8 = Aquatic (Strong vs Elemental, Weak vs Magic)
-- 9 = Mechanical (Strong vs Beast, Weak vs Elemental)
-- ============================================================================

-- ============================================================================
-- CRITTER PETS (Type 4) - Most common wild battle pets
-- ============================================================================
INSERT INTO `battle_pet_species` (`speciesId`, `creatureId`, `petType`, `flags`, `source`, `description`) VALUES
-- Critters from various zones
(39, 61071, 4, 0x3, 'wild', 'Squirrel - Common critter found everywhere'),
(40, 61072, 4, 0x3, 'wild', 'Fawn - Deer companion'),
(41, 61073, 4, 0x3, 'wild', 'Mouse - Small critter'),
(42, 61074, 4, 0x3, 'wild', 'Rabbit - Fast critter'),
(43, 61075, 4, 0x3, 'wild', 'Skunk - Stinky critter'),
(44, 61076, 4, 0x3, 'wild', 'Prairie Dog - Plains critter'),
(45, 61077, 4, 0x3, 'wild', 'Rat - Common vermin'),
(46, 61078, 4, 0x3, 'wild', 'Roach - Cockroach'),
(47, 61079, 4, 0x3, 'wild', 'Beetle - Crawling bug'),
(48, 61080, 4, 0x3, 'wild', 'Black Rat - Dark colored rat'),
(49, 61081, 4, 0x3, 'wild', 'Adder - Small snake'),
(50, 61082, 4, 0x3, 'wild', 'Marmot - Mountain critter'),
(51, 61083, 4, 0x5, 'wild', 'Armadillo Pup - Armored critter'),
(52, 61084, 4, 0x3, 'wild', 'Toad - Amphibian'),
(53, 61085, 4, 0x3, 'wild', 'Frog - Green frog');

-- ============================================================================
-- BEAST PETS (Type 7) - Predatory animals
-- ============================================================================
INSERT INTO `battle_pet_species` (`speciesId`, `creatureId`, `petType`, `flags`, `source`, `description`) VALUES
(100, 62178, 7, 0x3, 'wild', 'Cat - Feline companion'),
(101, 62179, 7, 0x3, 'wild', 'Snow Cub - White cat'),
(102, 62180, 7, 0x3, 'wild', 'Panther Cub - Jungle cat'),
(103, 62181, 7, 0x3, 'wild', 'Cheetah Cub - Fast cat'),
(104, 62182, 7, 0x3, 'wild', 'Black Tabby Cat - Dark cat'),
(105, 62183, 7, 0x3, 'wild', 'Worg Pup - Wolf puppy'),
(106, 62184, 7, 0x3, 'wild', 'Darkhound - Dark wolf'),
(107, 62185, 7, 0x3, 'wild', 'Fox Kit - Young fox'),
(108, 62186, 7, 0x3, 'wild', 'Alpine Fox Kit - Mountain fox'),
(109, 62187, 7, 0x5, 'wild', 'Baby Ape - Primate'),
(110, 62188, 7, 0x3, 'wild', 'Spider - Common spider'),
(111, 62189, 7, 0x3, 'wild', 'Scorpid - Desert scorpion'),
(112, 62190, 7, 0x3, 'wild', 'Snake - Common serpent'),
(113, 62191, 7, 0x5, 'wild', 'Leopard Tree Frog - Spotted frog'),
(114, 62192, 7, 0x3, 'wild', 'Gazelle Fawn - Plains deer');

-- ============================================================================
-- FLYING PETS (Type 2) - Birds and flying creatures
-- ============================================================================
INSERT INTO `battle_pet_species` (`speciesId`, `creatureId`, `petType`, `flags`, `source`, `description`) VALUES
(200, 63098, 2, 0x3, 'wild', 'Parrot - Colorful bird'),
(201, 63099, 2, 0x3, 'wild', 'Cockatiel - Small parrot'),
(202, 63100, 2, 0x3, 'wild', 'Owl - Nocturnal bird'),
(203, 63101, 2, 0x3, 'wild', 'Hawk - Bird of prey'),
(204, 63102, 2, 0x5, 'wild', 'Crow - Black bird'),
(205, 63103, 2, 0x3, 'wild', 'Seagull - Coastal bird'),
(206, 63104, 2, 0x3, 'wild', 'Dragonhawk Hatchling - Baby dragonhawk'),
(207, 63105, 2, 0x3, 'wild', 'Moth - Nocturnal flyer'),
(208, 63106, 2, 0x3, 'wild', 'Firefly - Glowing insect'),
(209, 63107, 2, 0x5, 'wild', 'Bat - Cave dweller'),
(210, 63108, 2, 0x3, 'wild', 'Crane - Wading bird'),
(211, 63109, 2, 0x3, 'wild', 'Chicken - Farm bird'),
(212, 63110, 2, 0x3, 'wild', 'Prairie Chicken - Wild fowl'),
(213, 63111, 2, 0x5, 'wild', 'Turkey - Large bird'),
(214, 63112, 2, 0x3, 'wild', 'Tiny Flamefly - Fire elemental insect');

-- ============================================================================
-- AQUATIC PETS (Type 8) - Water creatures
-- ============================================================================
INSERT INTO `battle_pet_species` (`speciesId`, `creatureId`, `petType`, `flags`, `source`, `description`) VALUES
(300, 64018, 8, 0x3, 'wild', 'Frog - Common frog'),
(301, 64019, 8, 0x3, 'wild', 'Small Frog - Tiny frog'),
(302, 64020, 8, 0x3, 'wild', 'Tree Frog - Tree dweller'),
(303, 64021, 8, 0x3, 'wild', 'Crab - Crustacean'),
(304, 64022, 8, 0x3, 'wild', 'Shore Crab - Coastal crab'),
(305, 64023, 8, 0x5, 'wild', 'Strand Crab - Beach crab'),
(306, 64024, 8, 0x3, 'wild', 'Sea Pony - Seahorse'),
(307, 64025, 8, 0x3, 'wild', 'Tiny Goldfish - Small fish'),
(308, 64026, 8, 0x5, 'wild', 'Pengu - Penguin'),
(309, 64027, 8, 0x3, 'wild', 'Strand Crawler - Beach crawler'),
(310, 64028, 8, 0x3, 'wild', 'Snail - Gastropod'),
(311, 64029, 8, 0x3, 'wild', 'Turtle - Shelled reptile'),
(312, 64030, 8, 0x3, 'wild', 'Softshell Snapling - Young turtle'),
(313, 64031, 8, 0x5, 'wild', 'Salty - Sea creature'),
(314, 64032, 8, 0x3, 'wild', 'Otter Pup - Young otter');

-- ============================================================================
-- ELEMENTAL PETS (Type 6) - Nature spirits and elementals
-- ============================================================================
INSERT INTO `battle_pet_species` (`speciesId`, `creatureId`, `petType`, `flags`, `source`, `description`) VALUES
(400, 65038, 6, 0x5, 'wild', 'Tiny Twister - Wind elemental'),
(401, 65039, 6, 0x5, 'wild', 'Tainted Waveling - Water elemental'),
(402, 65040, 6, 0x5, 'wild', 'Ashstone Core - Fire elemental'),
(403, 65041, 6, 0x5, 'wild', 'Ruby Sapling - Earth elemental'),
(404, 65042, 6, 0x5, 'wild', 'Tiny Snowman - Ice elemental'),
(405, 65043, 6, 0x5, 'wild', 'Singing Sunflower - Plant elemental'),
(406, 65044, 6, 0x5, 'wild', 'Withers - Corrupted tree'),
(407, 65045, 6, 0x5, 'wild', 'Teldrassil Sproutling - Tree spirit'),
(408, 65046, 6, 0x5, 'wild', 'Ashleaf Spriteling - Nature spirit'),
(409, 65047, 6, 0x5, 'wild', 'Crimson Lasher - Thorny plant'),
(410, 65048, 6, 0x5, 'wild', 'Amethyst Shale Hatchling - Crystal being'),
(411, 65049, 6, 0x5, 'wild', 'Topaz Shale Hatchling - Crystal being'),
(412, 65050, 6, 0x5, 'wild', 'Emerald Shale Hatchling - Crystal being'),
(413, 65051, 6, 0x9, 'rare', 'Terrible Turnip - Legendary plant'),
(414, 65052, 6, 0x5, 'wild', 'Lava Beetle - Fire bug');

-- ============================================================================
-- MECHANICAL PETS (Type 9) - Robots and constructs
-- ============================================================================
INSERT INTO `battle_pet_species` (`speciesId`, `creatureId`, `petType`, `flags`, `source`, `description`) VALUES
(500, 66058, 9, 0x5, 'vendor', 'Mechanical Squirrel - Gnomish creation'),
(501, 66059, 9, 0x5, 'vendor', 'Lifelike Mechanical Toad - Engineering pet'),
(502, 66060, 9, 0x5, 'vendor', 'Mechanical Chicken - Clucking robot'),
(503, 66061, 9, 0x5, 'quest', 'Lil XT - Miniature war machine'),
(504, 66062, 9, 0x9, 'rare', 'Personal World Destroyer - Rare mech'),
(505, 66063, 9, 0x5, 'vendor', 'Clockwork Rocket Bot - Rocket powered'),
(506, 66064, 9, 0x5, 'vendor', 'Rocket Bot - Small rocket'),
(507, 66065, 9, 0x5, 'engineering', 'Tranquil Mechanical Yeti - Yeti robot'),
(508, 66066, 9, 0x5, 'engineering', 'De-Weaponized Mechanical Companion - Peace bot'),
(509, 66067, 9, 0x9, 'rare', 'Darkmoon Zeppelin - Darkmoon prize'),
(510, 66068, 9, 0x5, 'engineering', 'Mechanical Pandaren Dragonling - Dragon bot'),
(511, 66069, 9, 0x5, 'engineering', 'Sunreaver Micro-Sentry - Horde bot'),
(512, 66070, 9, 0x5, 'vendor', 'Anodized Robo Cub - Bear bot'),
(513, 66071, 9, 0x5, 'vendor', 'Mechanical Axebeak - Bird bot'),
(514, 66072, 9, 0x5, 'engineering', 'Iron Starlette - Star robot');

-- ============================================================================
-- MAGIC PETS (Type 5) - Mystical creatures
-- ============================================================================
INSERT INTO `battle_pet_species` (`speciesId`, `creatureId`, `petType`, `flags`, `source`, `description`) VALUES
(600, 67078, 5, 0x5, 'rare', 'Mana Wyrmling - Magical serpent'),
(601, 67079, 5, 0x5, 'wild', 'Arcane Eye - Floating eye'),
(602, 67080, 5, 0x5, 'rare', 'Jade Owl - Mystical bird'),
(603, 67081, 5, 0x5, 'rare', 'Sapphire Cub - Blue cat'),
(604, 67082, 5, 0x5, 'rare', 'Enchanted Broom - Animated cleaning'),
(605, 67083, 5, 0x5, 'vendor', 'Enchanted Lantern - Magical light'),
(606, 67084, 5, 0x5, 'vendor', 'Magic Lamp - Wishing lamp'),
(607, 67085, 5, 0x5, 'quest', 'Lunar Lantern - Moon light'),
(608, 67086, 5, 0x5, 'quest', 'Festival Lantern - Celebration lamp'),
(609, 67087, 5, 0x9, 'rare', 'Sprite Darter Hatchling - Fey dragon'),
(610, 67088, 5, 0x5, 'wild', 'Disgusting Oozeling - Magic slime'),
(611, 67089, 5, 0x5, 'quest', 'Mini Mindslayer - Tentacle creature'),
(612, 67090, 5, 0x5, 'rare', 'Ethereal Soul-Trader - Spirit merchant'),
(613, 67091, 5, 0x5, 'rare', 'Lesser Voidcaller - Small void'),
(614, 67092, 5, 0xD, 'legendary', 'Netherspace Abyssal - Legendary void');

-- ============================================================================
-- UNDEAD PETS (Type 3) - Skeletal and ghostly creatures
-- ============================================================================
INSERT INTO `battle_pet_species` (`speciesId`, `creatureId`, `petType`, `flags`, `source`, `description`) VALUES
(700, 68098, 3, 0x5, 'rare', 'Ghostly Skull - Floating skull'),
(701, 68099, 3, 0x5, 'wild', 'Ghost - Spectral being'),
(702, 68100, 3, 0x5, 'wild', 'Spirit Crab - Undead crab'),
(703, 68101, 3, 0x5, 'wild', 'Restless Shadeling - Dark spirit'),
(704, 68102, 3, 0x5, 'rare', 'Fossilized Hatchling - Skeleton pet'),
(705, 68103, 3, 0x9, 'rare', 'Creepy Crate - Animated box'),
(706, 68104, 3, 0x5, 'vendor', 'Crawling Claw - Undead hand'),
(707, 68105, 3, 0x5, 'rare', 'Infected Squirrel - Zombie squirrel'),
(708, 68106, 3, 0x5, 'rare', 'Mr. Bigglesworth - Liches cat'),
(709, 68107, 3, 0x9, 'rare', 'Stitched Pup - Zombie dog'),
(710, 68108, 3, 0x5, 'rare', 'Sen Jin Fetish - Voodoo doll'),
(711, 68109, 3, 0x5, 'rare', 'Vampiric Batling - Vampire bat'),
(712, 68110, 3, 0x5, 'vendor', 'Blighthawk - Undead bird'),
(713, 68111, 3, 0xD, 'legendary', 'Lil Deathwing - Mini dragon'),
(714, 68112, 3, 0x5, 'wild', 'Infected Fawn - Zombie deer');

-- ============================================================================
-- DRAGONKIN PETS (Type 1) - Dragons and drake-like creatures
-- ============================================================================
INSERT INTO `battle_pet_species` (`speciesId`, `creatureId`, `petType`, `flags`, `source`, `description`) VALUES
(800, 69118, 1, 0x5, 'rare', 'Whelpling - Baby dragon'),
(801, 69119, 1, 0x9, 'rare', 'Azure Whelpling - Blue dragon'),
(802, 69120, 1, 0x9, 'rare', 'Crimson Whelpling - Red dragon'),
(803, 69121, 1, 0x9, 'rare', 'Dark Whelpling - Black dragon'),
(804, 69122, 1, 0x9, 'rare', 'Emerald Whelpling - Green dragon'),
(805, 69123, 1, 0xD, 'legendary', 'Sprite Darter - Fey dragon'),
(806, 69124, 1, 0x5, 'quest', 'Nether Faerie Dragon - Outland dragon'),
(807, 69125, 1, 0x5, 'vendor', 'Wild Jade Hatchling - Jade dragon'),
(808, 69126, 1, 0x5, 'vendor', 'Wild Golden Hatchling - Gold dragon'),
(809, 69127, 1, 0x5, 'vendor', 'Wild Crimson Hatchling - Red dragon'),
(810, 69128, 1, 0x5, 'rare', 'Infinite Whelpling - Bronze dragon'),
(811, 69129, 1, 0x5, 'rare', 'Nexus Whelpling - Arcane dragon'),
(812, 69130, 1, 0x5, 'vendor', 'Onyx Whelpling - Black dragon'),
(813, 69131, 1, 0x9, 'rare', 'Thundertail Flapper - Storm dragon'),
(814, 69132, 1, 0xD, 'legendary', 'Yu-lon - Celestial dragon');

-- ============================================================================
-- HUMANOID PETS (Type 0) - Human-like companions
-- ============================================================================
INSERT INTO `battle_pet_species` (`speciesId`, `creatureId`, `petType`, `flags`, `source`, `description`) VALUES
(900, 70138, 0, 0x5, 'vendor', 'Winter Veil Helper - Holiday helper'),
(901, 70139, 0, 0x5, 'vendor', 'Father Winter Helper - Santa helper'),
(902, 70140, 0, 0x5, 'rare', 'Moonkin Hatchling - Baby moonkin'),
(903, 70141, 0, 0x5, 'quest', 'Pandaren Monk - Martial artist'),
(904, 70142, 0, 0x5, 'rare', 'Gregarious Grell - Fel imp'),
(905, 70143, 0, 0x5, 'vendor', 'Curious Oracle Hatchling - Young oracle'),
(906, 70144, 0, 0x5, 'vendor', 'Curious Wolvar Pup - Young wolvar'),
(907, 70145, 0, 0x5, 'rare', 'Kun-Lai Runt - Yeti'),
(908, 70146, 0, 0x5, 'rare', 'Flayer Youngling - Scalding pet'),
(909, 70147, 0, 0x9, 'rare', 'Anubisath Idol - Egyptian statue'),
(910, 70148, 0, 0x5, 'vendor', 'Sporeling Sprout - Mushroom person'),
(911, 70149, 0, 0xD, 'legendary', 'Mini Tyrael - Angel'),
(912, 70150, 0, 0x5, 'vendor', 'Grindgear Toy - Goblin robot'),
(913, 70151, 0, 0x5, 'rare', 'Hopling - Baby hozen'),
(914, 70152, 0, 0x5, 'rare', 'Bonkers - Mad monkey');

-- ============================================================================
-- Update schema version
-- ============================================================================
-- (Schema version tracking handled by PlayerbotMigrationMgr)
