-- Migration 005: Initial Data Population
-- Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
-- Version: 005
-- Component: Initial Data Population

-- =============================================================================
-- Populate Core Tables with Initial Data (Added in Version 005)
-- =============================================================================

-- Set safe mode for bulk operations
SET FOREIGN_KEY_CHECKS = 0;
SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
SET AUTOCOMMIT = 0;
START TRANSACTION;

-- =============================================================================
-- ACTIVITY PATTERNS (Default bot behavior patterns)
-- =============================================================================

-- Clear existing system patterns
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

-- Low-impact testing patterns (disabled by default)
('debug_short', 'Debug Short Sessions', 'Short sessions for testing', 1, 1.0, 1.0, 300, 900, 300, 1800, 0.1, 0),
('debug_long', 'Debug Long Sessions', 'Extended sessions for testing', 1, 1.0, 0.1, 14400, 43200, 1800, 7200, 0.1, 0);

-- =============================================================================
-- CLASS POPULARITY (Based on WoW 11.2 statistics)
-- =============================================================================

DELETE FROM `playerbots_class_popularity`;

INSERT INTO `playerbots_class_popularity` (
    `class_id`, `class_name`, `popularity_weight`, `min_level`, `max_level`, `enabled`
) VALUES
-- Most Popular Classes (Tier 1)
(2, 'Paladin', 12.8, 1, 80, 1),        -- Versatile, strong in all content
(6, 'Death Knight', 11.9, 55, 80, 1),  -- Popular tank/dps, strong solo
(3, 'Hunter', 11.2, 1, 80, 1),         -- Easy to play, good dps
(1, 'Warrior', 10.5, 1, 80, 1),        -- Classic tank/dps choice

-- High Popularity Classes (Tier 2)
(4, 'Rogue', 9.8, 1, 80, 1),           -- High skill ceiling, PvP favorite
(8, 'Mage', 9.2, 1, 80, 1),            -- Classic caster, utility spells
(11, 'Druid', 8.7, 1, 80, 1),          -- Versatile, all roles available
(5, 'Priest', 8.1, 1, 80, 1),          -- Strong healer, shadow dps

-- Moderate Popularity Classes (Tier 3)
(7, 'Shaman', 7.3, 1, 80, 1),          -- Enhancement popular, good utility
(9, 'Warlock', 6.9, 1, 80, 1),         -- Complex rotation, good dps
(10, 'Monk', 6.4, 1, 80, 1),           -- Mobile, good in dungeons

-- Specialized Classes (Lower overall population)
(12, 'Demon Hunter', 4.2, 98, 80, 1),  -- Limited races, high mobility
(13, 'Evoker', 3.0, 58, 80, 1);        -- Newest class, ranged dps/heal

-- =============================================================================
-- GENDER DISTRIBUTION (Based on player preferences by race)
-- =============================================================================

DELETE FROM `playerbots_gender_distribution`;

INSERT INTO `playerbots_gender_distribution` (
    `race_id`, `male_percentage`, `female_percentage`
) VALUES
-- Alliance Races
(1, 58.0, 42.0),    -- Human: Slight male preference (versatile race)
(3, 72.0, 28.0),    -- Dwarf: Strong male preference (warrior culture)
(4, 35.0, 65.0),    -- Night Elf: Female preference (elegant, mystical)
(7, 55.0, 45.0),    -- Gnome: Slight male preference (technology focus)
(11, 42.0, 58.0),   -- Draenei: Female preference (graceful, holy)
(22, 65.0, 35.0),   -- Worgen: Male preference (beast/feral theme)
(29, 38.0, 62.0),   -- Void Elf: Female preference (elegant void magic)
(32, 62.0, 38.0),   -- Kul Tiran: Male preference (naval/maritime)
(37, 70.0, 30.0),   -- Dark Iron Dwarf: Strong male preference (industrial)
(34, 40.0, 60.0),   -- Lightforged Draenei: Female preference (holy warriors)
(30, 68.0, 32.0),   -- Mechagnome: Male preference (technology)

-- Horde Races
(2, 75.0, 25.0),    -- Orc: Strong male preference (warrior culture)
(5, 48.0, 52.0),    -- Undead: Balanced, slight female preference
(6, 68.0, 32.0),    -- Tauren: Male preference (tribal/shamanic)
(8, 60.0, 40.0),    -- Troll: Male preference (tribal warriors)
(9, 55.0, 45.0),    -- Goblin: Slight male preference (business/tech)
(10, 25.0, 75.0),   -- Blood Elf: Strong female preference (beauty/magic)
(27, 30.0, 70.0),   -- Nightborne: Strong female preference (elegant elves)
(28, 70.0, 30.0),   -- Highmountain Tauren: Male preference (mountain warriors)
(31, 58.0, 42.0),   -- Zandalari Troll: Male preference (empire builders)
(35, 45.0, 55.0),   -- Vulpera: Female preference (cute factor)
(36, 78.0, 22.0),   -- Mag'har Orc: Strong male preference (pure orcs)

-- Neutral Races
(24, 52.0, 48.0),   -- Pandaren (Alliance): Balanced, slight male preference
(25, 52.0, 48.0),   -- Pandaren (Horde): Balanced, slight male preference
(26, 52.0, 48.0),   -- Pandaren (Neutral): Balanced, slight male preference

-- Hero/Special Classes
(70, 45.0, 55.0),   -- Dracthyr (Alliance): Slight female preference (dragon magic)
(52, 45.0, 55.0);   -- Dracthyr (Horde): Slight female preference (dragon magic)

-- =============================================================================
-- RACE-CLASS DISTRIBUTION (Based on WoW 11.2 statistics)
-- =============================================================================

DELETE FROM `playerbots_race_class_distribution`;

INSERT INTO `playerbots_race_class_distribution` (
    `race_id`, `class_id`, `distribution_weight`, `enabled`
) VALUES
-- Alliance Popular Combinations
-- Human combinations (most popular Alliance race)
(1, 1, 8.5, 1),   -- Human Warrior
(1, 2, 7.2, 1),   -- Human Paladin
(1, 4, 6.8, 1),   -- Human Rogue
(1, 5, 5.9, 1),   -- Human Priest
(1, 8, 5.4, 1),   -- Human Mage
(1, 9, 4.1, 1),   -- Human Warlock
(1, 3, 3.2, 1),   -- Human Hunter
(1, 6, 2.8, 1),   -- Human Death Knight
(1, 10, 2.1, 1),  -- Human Monk

-- Night Elf combinations
(4, 3, 4.8, 1),   -- Night Elf Hunter
(4, 11, 4.2, 1),  -- Night Elf Druid
(4, 4, 3.5, 1),   -- Night Elf Rogue
(4, 1, 2.9, 1),   -- Night Elf Warrior
(4, 5, 2.4, 1),   -- Night Elf Priest
(4, 8, 2.1, 1),   -- Night Elf Mage
(4, 6, 1.8, 1),   -- Night Elf Death Knight
(4, 10, 1.4, 1),  -- Night Elf Monk
(4, 12, 1.2, 1),  -- Night Elf Demon Hunter

-- Dwarf combinations
(3, 3, 3.8, 1),   -- Dwarf Hunter
(3, 2, 3.1, 1),   -- Dwarf Paladin
(3, 1, 2.7, 1),   -- Dwarf Warrior
(3, 5, 2.2, 1),   -- Dwarf Priest
(3, 4, 1.9, 1),   -- Dwarf Rogue
(3, 7, 1.6, 1),   -- Dwarf Shaman
(3, 6, 1.3, 1),   -- Dwarf Death Knight
(3, 10, 1.0, 1),  -- Dwarf Monk

-- Gnome combinations
(7, 8, 2.9, 1),   -- Gnome Mage
(7, 9, 2.4, 1),   -- Gnome Warlock
(7, 1, 1.8, 1),   -- Gnome Warrior
(7, 4, 1.5, 1),   -- Gnome Rogue
(7, 5, 1.2, 1),   -- Gnome Priest
(7, 6, 1.0, 1),   -- Gnome Death Knight
(7, 3, 0.8, 1),   -- Gnome Hunter
(7, 10, 0.6, 1),  -- Gnome Monk

-- Draenei combinations
(11, 2, 2.8, 1),  -- Draenei Paladin
(11, 7, 2.5, 1),  -- Draenei Shaman
(11, 5, 2.1, 1),  -- Draenei Priest
(11, 1, 1.8, 1),  -- Draenei Warrior
(11, 3, 1.5, 1),  -- Draenei Hunter
(11, 8, 1.2, 1),  -- Draenei Mage
(11, 6, 1.0, 1),  -- Draenei Death Knight
(11, 10, 0.8, 1), -- Draenei Monk

-- Horde Popular Combinations
-- Orc combinations (most popular Horde race)
(2, 1, 7.8, 1),   -- Orc Warrior
(2, 7, 6.9, 1),   -- Orc Shaman
(2, 3, 5.4, 1),   -- Orc Hunter
(2, 9, 4.2, 1),   -- Orc Warlock
(2, 4, 3.8, 1),   -- Orc Rogue
(2, 6, 2.9, 1),   -- Orc Death Knight
(2, 10, 2.1, 1),  -- Orc Monk

-- Undead combinations
(5, 9, 6.2, 1),   -- Undead Warlock
(5, 5, 5.1, 1),   -- Undead Priest
(5, 8, 4.8, 1),   -- Undead Mage
(5, 4, 4.1, 1),   -- Undead Rogue
(5, 1, 3.2, 1),   -- Undead Warrior
(5, 6, 2.4, 1),   -- Undead Death Knight
(5, 10, 1.8, 1),  -- Undead Monk

-- Tauren combinations
(6, 11, 5.4, 1),  -- Tauren Druid
(6, 7, 4.2, 1),   -- Tauren Shaman
(6, 1, 3.8, 1),   -- Tauren Warrior
(6, 3, 3.1, 1),   -- Tauren Hunter
(6, 2, 2.6, 1),   -- Tauren Paladin
(6, 6, 2.1, 1),   -- Tauren Death Knight
(6, 10, 1.7, 1),  -- Tauren Monk

-- Troll combinations
(8, 7, 5.1, 1),   -- Troll Shaman
(8, 3, 4.4, 1),   -- Troll Hunter
(8, 1, 3.9, 1),   -- Troll Warrior
(8, 4, 3.2, 1),   -- Troll Rogue
(8, 5, 2.8, 1),   -- Troll Priest
(8, 8, 2.3, 1),   -- Troll Mage
(8, 9, 1.9, 1),   -- Troll Warlock
(8, 6, 1.6, 1),   -- Troll Death Knight
(8, 10, 1.3, 1),  -- Troll Monk
(8, 11, 1.0, 1),  -- Troll Druid

-- Blood Elf combinations
(10, 2, 5.8, 1),  -- Blood Elf Paladin
(10, 8, 5.2, 1),  -- Blood Elf Mage
(10, 3, 4.6, 1),  -- Blood Elf Hunter
(10, 4, 4.0, 1),  -- Blood Elf Rogue
(10, 5, 3.4, 1),  -- Blood Elf Priest
(10, 9, 2.9, 1),  -- Blood Elf Warlock
(10, 1, 2.3, 1),  -- Blood Elf Warrior
(10, 6, 1.8, 1),  -- Blood Elf Death Knight
(10, 10, 1.4, 1), -- Blood Elf Monk
(10, 12, 1.1, 1); -- Blood Elf Demon Hunter

-- =============================================================================
-- RACE-CLASS-GENDER OVERRIDES (Specific combinations)
-- =============================================================================

DELETE FROM `playerbots_race_class_gender`;

INSERT INTO `playerbots_race_class_gender` (
    `race_id`, `class_id`, `gender`, `preference_weight`
) VALUES
-- Warrior class tends to be more male across most races
(4, 1, 0, 60.0),   -- Night Elf Warrior (male preference for warriors)
(11, 1, 0, 65.0),  -- Draenei Warrior (male preference for warriors)
(10, 1, 0, 45.0),  -- Blood Elf Warrior (less female preference for warriors)

-- Paladin class - holy warriors tend to be more balanced
(4, 2, 0, 55.0),   -- Night Elf Paladin (more male for holy warrior theme)
(10, 2, 0, 35.0),  -- Blood Elf Paladin (less extreme female preference)
(11, 2, 0, 50.0),  -- Draenei Paladin (balanced for holy warriors)

-- Hunter class - some races prefer female hunters
(4, 3, 1, 75.0),   -- Night Elf Hunter (strong female preference, ranger theme)
(10, 3, 1, 70.0),  -- Blood Elf Hunter (female preference for elegant archer)

-- Rogue class - stealth and agility themes
(4, 4, 1, 70.0),   -- Night Elf Rogue (female shadowmeld rogues popular)
(10, 4, 1, 65.0),  -- Blood Elf Rogue (elegant assassin theme)
(1, 4, 0, 65.0),   -- Human Rogue (male preference for human thieves)

-- Priest class - healing and shadow themes
(5, 5, 0, 40.0),   -- Undead Priest (shadow priest theme, more balanced)
(10, 5, 1, 80.0),  -- Blood Elf Priest (strong female preference for holy magic)
(11, 5, 1, 65.0),  -- Draenei Priest (holy female priests popular)

-- Death Knight class - tends to be more male regardless of race
(4, 6, 0, 55.0),   -- Night Elf Death Knight (death theme overrides elegance)
(10, 6, 0, 45.0),  -- Blood Elf Death Knight (death knight masculinity)
(11, 6, 0, 60.0),  -- Draenei Death Knight (fallen paladin theme)

-- Shaman class - elemental themes
(6, 7, 0, 75.0),   -- Tauren Shaman (spiritual leader, male preference)
(8, 7, 0, 65.0),   -- Troll Shaman (witch doctor theme, male preference)
(11, 7, 0, 55.0),  -- Draenei Shaman (elemental warrior theme)

-- Mage class - intellectual spellcasters
(1, 8, 0, 50.0),   -- Human Mage (balanced for scholarly theme)
(7, 8, 0, 45.0),   -- Gnome Mage (intelligence over gender stereotypes)
(10, 8, 1, 80.0),  -- Blood Elf Mage (arcane elegance, female preference)

-- Warlock class - dark magic themes
(1, 9, 0, 65.0),   -- Human Warlock (dark male warlock stereotype)
(5, 9, 0, 55.0),   -- Undead Warlock (slightly more male for dark magic)
(10, 9, 1, 70.0),  -- Blood Elf Warlock (sophisticated dark magic)

-- Monk class - martial arts themes
(24, 10, 0, 60.0), -- Pandaren Monk (martial arts master stereotype)
(25, 10, 0, 60.0), -- Pandaren Monk (Horde)
(1, 10, 0, 55.0),  -- Human Monk (balanced martial artist)

-- Druid class - nature themes vary by race
(4, 11, 1, 75.0),  -- Night Elf Druid (nature goddess theme, female preference)
(6, 11, 0, 70.0),  -- Tauren Druid (spiritual leader theme, male preference)
(8, 11, 0, 50.0),  -- Troll Druid (balanced nature connection)

-- Demon Hunter class - vengeance themes
(4, 12, 0, 55.0),  -- Night Elf Demon Hunter (vengeance warrior, more male)
(10, 12, 0, 40.0), -- Blood Elf Demon Hunter (sacrificial theme, less female)

-- Evoker class - draconic magic themes
(70, 13, 0, 50.0), -- Dracthyr Evoker Alliance (balanced dragon magic)
(52, 13, 0, 50.0); -- Dracthyr Evoker Horde (balanced dragon magic)

-- =============================================================================
-- NOTE: Names are populated by migration 006_bot_names.sql
-- =============================================================================

-- Commit the transaction
COMMIT;
SET FOREIGN_KEY_CHECKS = 1;
SET AUTOCOMMIT = 1;