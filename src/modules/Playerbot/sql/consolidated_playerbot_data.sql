-- ============================================================================
-- TrinityCore Playerbot Module - Consolidated Data File
-- ============================================================================
-- This file contains all required seed data for the Playerbot module
-- Based on analysis of existing data files and C++ requirements
-- Generated on: September 19, 2025
-- ============================================================================

-- Set safe mode for bulk operations
SET FOREIGN_KEY_CHECKS = 0;
SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
SET AUTOCOMMIT = 0;
START TRANSACTION;

-- ============================================================================
-- ACTIVITY PATTERNS (Default bot behavior patterns)
-- ============================================================================

-- Clear existing patterns
DELETE FROM `playerbot_activity_patterns` WHERE `is_system_pattern` = 1;

INSERT INTO `playerbot_activity_patterns` (
    `pattern_name`, `display_name`, `description`, `is_system_pattern`,
    `login_chance`, `logout_chance`, `min_session_duration`, `max_session_duration`,
    `min_offline_duration`, `max_offline_duration`, `activity_weight`, `enabled`
) VALUES
-- High activity patterns
('casual_high', 'Casual High Activity', 'Casual players with high online time', 1, 0.8, 0.7, 3600, 14400, 1800, 7200, 2.0, 1),
('hardcore', 'Hardcore Player', 'Dedicated players with very high activity', 1, 0.95, 0.3, 7200, 28800, 900, 3600, 1.5, 1),
('weekend_warrior', 'Weekend Warrior', 'High activity on weekends, low on weekdays', 1, 0.9, 0.6, 5400, 21600, 2700, 10800, 1.8, 1),

-- Medium activity patterns
('casual_medium', 'Casual Medium Activity', 'Average casual players', 1, 0.6, 0.8, 1800, 7200, 3600, 14400, 3.0, 1),
('evening_player', 'Evening Player', 'Players active mainly in evenings', 1, 0.7, 0.75, 2700, 10800, 4500, 18000, 2.5, 1),
('social_player', 'Social Player', 'Players focused on social aspects', 1, 0.65, 0.8, 2400, 9600, 3600, 14400, 2.2, 1),

-- Low activity patterns
('casual_low', 'Casual Low Activity', 'Casual players with lower activity', 1, 0.4, 0.9, 900, 3600, 7200, 28800, 4.0, 1),
('occasional', 'Occasional Player', 'Players who log in occasionally', 1, 0.3, 0.95, 600, 2400, 14400, 86400, 3.5, 1),
('trial_player', 'Trial Player', 'New players trying the game', 1, 0.5, 0.85, 1200, 4800, 5400, 21600, 3.0, 1),

-- Special patterns
('power_leveler', 'Power Leveler', 'Players focused on rapid progression', 1, 0.9, 0.4, 5400, 18000, 1800, 7200, 1.2, 1),
('achievement_hunter', 'Achievement Hunter', 'Players focused on achievements', 1, 0.7, 0.7, 3600, 12600, 2700, 10800, 1.8, 1),
('pvp_focused', 'PvP Focused', 'Players primarily interested in PvP', 1, 0.8, 0.6, 2700, 10800, 3600, 14400, 1.6, 1),
('pve_focused', 'PvE Focused', 'Players primarily interested in PvE', 1, 0.75, 0.65, 3600, 12600, 2700, 10800, 1.7, 1),
('explorer', 'Explorer', 'Players who enjoy exploring the world', 1, 0.6, 0.8, 2400, 9600, 4500, 18000, 2.3, 1),

-- Low-impact testing patterns
('debug_short', 'Debug Short Sessions', 'Short sessions for testing', 1, 1.0, 1.0, 300, 900, 300, 1800, 0.1, 0),
('debug_medium', 'Debug Medium Sessions', 'Medium sessions for testing', 1, 1.0, 1.0, 1800, 3600, 900, 3600, 0.1, 0);

-- ============================================================================
-- ZONE POPULATIONS (Default zone configurations)
-- ============================================================================

-- Clear existing zone data
DELETE FROM `playerbot_zone_populations`;

INSERT INTO `playerbot_zone_populations` (
    `zone_id`, `map_id`, `zone_name`, `min_level`, `max_level`, `max_population`,
    `spawn_weight`, `is_enabled`, `is_starter_zone`, `is_endgame_zone`, `faction_preference`
) VALUES
-- Alliance Starter Zones
(12, 0, 'Elwynn Forest', 1, 10, 15, 2.0, 1, 1, 0, 'ALLIANCE'),
(141, 0, 'Teldrassil', 1, 10, 12, 1.8, 1, 1, 0, 'ALLIANCE'),
(1, 0, 'Dun Morogh', 1, 10, 12, 1.8, 1, 1, 0, 'ALLIANCE'),
(3524, 530, 'Azuremyst Isle', 1, 10, 10, 1.5, 1, 1, 0, 'ALLIANCE'),

-- Horde Starter Zones
(14, 1, 'Durotar', 1, 10, 15, 2.0, 1, 1, 0, 'HORDE'),
(215, 1, 'Mulgore', 1, 10, 12, 1.8, 1, 1, 0, 'HORDE'),
(85, 0, 'Tirisfal Glades', 1, 10, 12, 1.8, 1, 1, 0, 'HORDE'),
(3525, 530, 'Bloodmyst Isle', 1, 10, 10, 1.5, 1, 1, 0, 'HORDE'),

-- Low Level Zones (10-20)
(40, 0, 'Westfall', 10, 20, 20, 1.8, 1, 0, 0, 'ALLIANCE'),
(11, 0, 'Wetlands', 20, 30, 15, 1.5, 1, 0, 0, 'ALLIANCE'),
(38, 0, 'Loch Modan', 10, 20, 15, 1.6, 1, 0, 0, 'ALLIANCE'),
(148, 0, 'Darkshore', 10, 20, 18, 1.7, 1, 0, 0, 'ALLIANCE'),
(17, 1, 'The Barrens', 10, 25, 25, 2.2, 1, 0, 0, 'HORDE'),
(130, 0, 'Silverpine Forest', 10, 20, 15, 1.6, 1, 0, 0, 'HORDE'),

-- Mid Level Zones (20-40)
(44, 0, 'Redridge Mountains', 15, 25, 18, 1.6, 1, 0, 0, 'ALLIANCE'),
(3, 0, 'Badlands', 35, 45, 12, 1.3, 1, 0, 0, 'NEUTRAL'),
(4, 0, 'Blasted Lands', 45, 55, 10, 1.2, 1, 0, 0, 'NEUTRAL'),
(51, 0, 'Searing Gorge', 43, 50, 8, 1.1, 1, 0, 0, 'NEUTRAL'),
(46, 0, 'Burning Steppes', 50, 58, 8, 1.1, 1, 0, 0, 'NEUTRAL'),
(33, 0, 'Stranglethorn Vale', 30, 45, 20, 1.8, 1, 0, 0, 'NEUTRAL'),

-- High Level Zones (40-60)
(28, 0, 'Western Plaguelands', 51, 58, 12, 1.3, 1, 0, 0, 'NEUTRAL'),
(139, 0, 'Eastern Plaguelands', 53, 60, 12, 1.3, 1, 0, 0, 'NEUTRAL'),
(361, 0, 'Felwood', 48, 55, 10, 1.2, 1, 0, 0, 'NEUTRAL'),
(490, 0, 'Un\'Goro Crater', 48, 55, 8, 1.1, 1, 0, 0, 'NEUTRAL'),
(618, 0, 'Winterspring', 53, 60, 8, 1.1, 1, 0, 0, 'NEUTRAL'),

-- Outland Zones (58-70)
(3483, 530, 'Hellfire Peninsula', 58, 63, 15, 1.4, 1, 0, 0, 'NEUTRAL'),
(3519, 530, 'Terokkar Forest', 62, 65, 12, 1.2, 1, 0, 0, 'NEUTRAL'),
(3518, 530, 'Nagrand', 64, 67, 10, 1.1, 1, 0, 0, 'NEUTRAL'),
(3520, 530, 'Shadowmoon Valley', 67, 70, 8, 1.0, 1, 0, 0, 'NEUTRAL'),

-- Northrend Zones (68-80)
(394, 571, 'Borean Tundra', 68, 72, 15, 1.4, 1, 0, 0, 'NEUTRAL'),
(495, 571, 'Howling Fjord', 68, 72, 15, 1.4, 1, 0, 0, 'NEUTRAL'),
(3537, 571, 'Dragonblight', 71, 74, 12, 1.2, 1, 0, 0, 'NEUTRAL'),
(65, 571, 'Grizzly Hills', 73, 75, 10, 1.1, 1, 0, 0, 'NEUTRAL'),
(3711, 571, 'Sholazar Basin', 76, 78, 8, 1.0, 1, 0, 0, 'NEUTRAL'),
(67, 571, 'Icecrown', 77, 80, 12, 1.3, 1, 0, 1, 'NEUTRAL'),

-- Capital Cities (lower population, social hubs)
(1519, 0, 'Stormwind City', 1, 80, 8, 0.8, 1, 0, 0, 'ALLIANCE'),
(1637, 1, 'Orgrimmar', 1, 80, 8, 0.8, 1, 0, 0, 'HORDE'),
(1657, 0, 'Darnassus', 1, 80, 5, 0.6, 1, 0, 0, 'ALLIANCE'),
(1638, 1, 'Thunder Bluff', 1, 80, 5, 0.6, 1, 0, 0, 'HORDE'),
(1537, 0, 'Ironforge', 1, 80, 6, 0.7, 1, 0, 0, 'ALLIANCE'),
(1497, 0, 'Undercity', 1, 80, 6, 0.7, 1, 0, 0, 'HORDE'),
(3703, 530, 'Shattrath City', 58, 80, 6, 0.7, 1, 0, 0, 'NEUTRAL'),
(4395, 571, 'Dalaran', 68, 80, 8, 0.8, 1, 0, 1, 'NEUTRAL');

-- ============================================================================
-- CLASS POPULARITY (Balanced class distribution)
-- ============================================================================

DELETE FROM `playerbots_class_popularity`;

INSERT INTO `playerbots_class_popularity` (
    `class_id`, `popularity_weight`, `min_level`, `max_level`, `enabled`
) VALUES
(1, 1.2, 1, 80, 1),  -- Warrior (slightly more popular)
(2, 1.5, 1, 80, 1),  -- Paladin (popular)
(3, 1.3, 1, 80, 1),  -- Hunter (popular)
(4, 1.0, 1, 80, 1),  -- Rogue (average)
(5, 1.4, 1, 80, 1),  -- Priest (popular for healing)
(6, 0.8, 55, 80, 1), -- Death Knight (limited, starts at 55)
(7, 1.1, 1, 80, 1),  -- Shaman (slightly above average)
(8, 1.3, 1, 80, 1),  -- Mage (popular)
(9, 1.0, 1, 80, 1),  -- Warlock (average)
(11, 0.9, 1, 80, 1); -- Druid (slightly below average)

-- ============================================================================
-- GENDER DISTRIBUTION (Realistic gender ratios by race)
-- ============================================================================

DELETE FROM `playerbots_gender_distribution`;

INSERT INTO `playerbots_gender_distribution` (
    `race_id`, `male_percentage`, `female_percentage`
) VALUES
(1, 55.0, 45.0),  -- Human
(2, 75.0, 25.0),  -- Orc (more male preference)
(3, 60.0, 40.0),  -- Dwarf
(4, 45.0, 55.0),  -- Night Elf (more female preference)
(5, 70.0, 30.0),  -- Undead
(6, 65.0, 35.0),  -- Tauren
(7, 50.0, 50.0),  -- Gnome (balanced)
(8, 60.0, 40.0),  -- Troll
(10, 40.0, 60.0), -- Blood Elf (more female preference)
(11, 45.0, 55.0); -- Draenei (more female preference)

-- ============================================================================
-- RACE-CLASS DISTRIBUTION (Based on lore and gameplay balance)
-- ============================================================================

DELETE FROM `playerbots_race_class_distribution`;

INSERT INTO `playerbots_race_class_distribution` (
    `race_id`, `class_id`, `distribution_weight`, `enabled`
) VALUES
-- Human (versatile, all classes well-represented)
(1, 1, 1.2, 1), (1, 2, 1.3, 1), (1, 3, 1.0, 1), (1, 4, 1.0, 1), (1, 5, 1.2, 1),
(1, 8, 1.1, 1), (1, 9, 1.0, 1),

-- Orc (warrior culture, shamanic traditions)
(2, 1, 1.5, 1), (2, 3, 1.2, 1), (2, 4, 0.8, 1), (2, 6, 1.1, 1), (2, 7, 1.3, 1),
(2, 8, 0.7, 1), (2, 9, 1.0, 1),

-- Dwarf (hardy warriors, priests, hunters)
(3, 1, 1.4, 1), (3, 2, 1.2, 1), (3, 3, 1.3, 1), (3, 4, 0.9, 1), (3, 5, 1.1, 1),

-- Night Elf (nature-based, agile)
(4, 1, 1.0, 1), (4, 3, 1.4, 1), (4, 4, 1.2, 1), (4, 5, 1.1, 1), (4, 11, 1.5, 1),

-- Undead (dark magic, rogues)
(5, 1, 1.0, 1), (5, 4, 1.3, 1), (5, 5, 1.2, 1), (5, 8, 1.1, 1), (5, 9, 1.4, 1),

-- Tauren (shamanic, druidic, warrior culture)
(6, 1, 1.3, 1), (6, 3, 1.2, 1), (6, 7, 1.5, 1), (6, 11, 1.4, 1),

-- Gnome (magical, engineering)
(7, 1, 0.8, 1), (7, 4, 1.0, 1), (7, 8, 1.4, 1), (7, 9, 1.2, 1),

-- Troll (shamanic, hunter culture)
(8, 1, 1.1, 1), (8, 3, 1.3, 1), (8, 4, 1.0, 1), (8, 5, 1.2, 1), (8, 7, 1.4, 1),
(8, 8, 1.0, 1),

-- Blood Elf (magical heritage)
(10, 2, 1.3, 1), (10, 3, 1.0, 1), (10, 4, 1.0, 1), (10, 5, 1.1, 1), (10, 8, 1.4, 1),
(10, 9, 1.2, 1),

-- Draenei (holy magic, shamanic)
(11, 1, 1.1, 1), (11, 2, 1.4, 1), (11, 3, 1.0, 1), (11, 5, 1.3, 1), (11, 7, 1.2, 1),
(11, 8, 1.1, 1);

-- ============================================================================
-- RACE-CLASS-GENDER COMBINATIONS (Fine-tuned preferences)
-- ============================================================================

DELETE FROM `playerbots_race_class_gender`;

-- Generate balanced race-class-gender combinations
-- This provides fine-tuned control over character creation
INSERT INTO `playerbots_race_class_gender` (
    `race_id`, `class_id`, `gender`, `preference_weight`, `enabled`
)
SELECT
    rcd.race_id,
    rcd.class_id,
    CASE WHEN RAND() < (gd.male_percentage / 100.0) THEN 0 ELSE 1 END as gender,
    rcd.distribution_weight * (1.0 + (RAND() * 0.2 - 0.1)) as preference_weight,
    1 as enabled
FROM `playerbots_race_class_distribution` rcd
JOIN `playerbots_gender_distribution` gd ON rcd.race_id = gd.race_id
WHERE rcd.enabled = 1;

-- ============================================================================
-- SAMPLE NAMES (Basic name set for testing)
-- ============================================================================

DELETE FROM `playerbots_names`;

-- Insert a basic set of names for initial testing
-- In production, this would be much larger (thousands of names)
INSERT INTO `playerbots_names` (`name`, `gender`, `race_mask`) VALUES
-- Male names (gender = 0)
('Theron', 0, 0), ('Anduin', 0, 1), ('Varian', 0, 1), ('Arthas', 0, 1), ('Tyrande', 0, 0),
('Thrall', 0, 2), ('Garrosh', 0, 2), ('Gromash', 0, 2), ('Durotan', 0, 2), ('Orgrim', 0, 2),
('Muradin', 0, 4), ('Bronzebeard', 0, 4), ('Ironforge', 0, 4), ('Magni', 0, 4), ('Brann', 0, 4),
('Malfurion', 0, 8), ('Illidan', 0, 8), ('Tyrande', 0, 8), ('Cenarius', 0, 8), ('Furion', 0, 8),
('Sylvanas', 0, 16), ('Arthas', 0, 16), ('Kelthuzad', 0, 16), ('Anubarak', 0, 16), ('Nerzhul', 0, 16),
('Cairne', 0, 32), ('Baine', 0, 32), ('Hamuul', 0, 32), ('Magatha', 0, 32), ('Thunderbluff', 0, 32),
('Mekkatorque', 0, 64), ('Tinker', 0, 64), ('Gelbin', 0, 64), ('Gnomeregan', 0, 64), ('Mechano', 0, 64),
('Voljin', 0, 128), ('Zulthrash', 0, 128), ('Rastakhan', 0, 128), ('Zulaman', 0, 128), ('Gurubashi', 0, 128),
('Kaelthas', 0, 512), ('Rommath', 0, 512), ('Silvermoon', 0, 512), ('Sunstrider', 0, 512), ('Dathremar', 0, 512),
('Velen', 0, 1024), ('Akama', 0, 1024), ('Nobundo', 0, 1024), ('Exodar', 0, 1024), ('Draenor', 0, 1024),

-- Female names (gender = 1)
('Jaina', 1, 1), ('Tyrande', 1, 1), ('Alexstrasza', 1, 1), ('Alleria', 1, 1), ('Vereesa', 1, 1),
('Sylvanas', 1, 2), ('Aggra', 1, 2), ('Draka', 1, 2), ('Garona', 1, 2), ('Nazgrel', 1, 2),
('Moira', 1, 4), ('Muradin', 1, 4), ('Thelsamar', 1, 4), ('Ironforge', 1, 4), ('Bronzebeard', 1, 4),
('Tyrande', 1, 8), ('Maiev', 1, 8), ('Shandris', 1, 8), ('Elune', 1, 8), ('Nightsong', 1, 8),
('Sylvanas', 1, 16), ('Calia', 1, 16), ('Lillian', 1, 16), ('Belmont', 1, 16), ('Forsaken', 1, 16),
('Magatha', 1, 32), ('Eitrigg', 1, 32), ('Spiritwalker', 1, 32), ('Bloodhoof', 1, 32), ('Windfury', 1, 32),
('Chromie', 1, 64), ('Tinkmaster', 1, 64), ('Sparkle', 1, 64), ('Gearwrench', 1, 64), ('Cogwheel', 1, 64),
('Zentabra', 1, 128), ('Voljin', 1, 128), ('Shakkari', 1, 128), ('Gurubashi', 1, 128), ('Amani', 1, 128),
('Liadrin', 1, 512), ('Vashj', 1, 512), ('Sunfury', 1, 512), ('Silvermoon', 1, 512), ('Dawnblade', 1, 512),
('Yrel', 1, 1024), ('Ishanah', 1, 1024), ('Iridi', 1, 1024), ('Vindicator', 1, 1024), ('Anchorite', 1, 1024);

-- ============================================================================
-- RECORD MIGRATION APPLICATION
-- ============================================================================

INSERT INTO `playerbot_migrations` (`version`, `description`, `checksum`)
VALUES ('consolidated', 'Consolidated schema and data installation', MD5(CONCAT(NOW(), 'consolidated')));

-- ============================================================================
-- FINALIZE INSTALLATION
-- ============================================================================

COMMIT;
SET FOREIGN_KEY_CHECKS = 1;

-- Display installation summary
SELECT 'Data installation complete' as 'Status',
    (SELECT COUNT(*) FROM `playerbot_activity_patterns`) as 'Activity patterns',
    (SELECT COUNT(*) FROM `playerbot_zone_populations`) as 'Zone configurations',
    (SELECT COUNT(*) FROM `playerbots_names`) as 'Available names',
    (SELECT COUNT(*) FROM `playerbots_class_popularity`) as 'Class configs',
    (SELECT COUNT(*) FROM `playerbots_race_class_distribution`) as 'Race-class combos',
    NOW() as 'Installed at';