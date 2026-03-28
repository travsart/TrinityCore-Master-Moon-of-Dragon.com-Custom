-- ============================================================================
-- Wormhole Teleport Fixes — SQL Data for C++ Scripts
-- ============================================================================
-- This file provides all SQL-only teleport fixes (spell_target_position) plus
-- the creature_template updates, gossip data, and spell_script_names needed by
-- the custom wormhole C++ scripts.
--
-- Sections:
--   Part 1: Mechagon spell_target_position (SQL-only fix, no C++ needed)
--   Part 2: spell_script_names for SpellScript-based wormholes
--   Part 3: creature_template updates for NPC gossip wormholes
--   Part 4: Gossip menus and options for NPC gossip wormholes
--   Part 5: creature_template_gossip assignments
--   Part 6: npc_text entries for gossip menu headers
-- ============================================================================
USE `world`;
-- ============================================================================
-- Part 1: Mechagon spell_target_position (SQL-only fix)
-- ============================================================================
-- These spells have TELEPORT_UNITS (Effect 252) at EffectIndex 0 but no
-- destination defined. Map 1643 = Kul Tiras continent (Mechagon is northeast).
-- Pattern matches existing entries like Pandaria wormhole (126756/126757).
--
-- Reference format from existing entries:
--   ID=126756, EffectIndex=0, OrderIndex=0, MapID=870, VerifiedBuild=22996
--   ID=67834,  EffectIndex=1, OrderIndex=0, MapID=571, VerifiedBuild=0

DELETE FROM spell_target_position WHERE ID IN (291981, 291983, 291986, 296111);
INSERT INTO spell_target_position (ID, EffectIndex, OrderIndex, MapID, PositionX, PositionY, PositionZ, Orientation, VerifiedBuild) VALUES
(291981, 0, 0, 1643, 3116.98,  4898.31, 33.6045, 0, 0),  -- Wormhole: Mechagon (main teleport -> Rustbolt)
(291983, 0, 0, 1643, 3150.00,  4920.00, 80.0000, 0, 0),  -- Wormhole: Mechagon Glider (elevated for glider spawn)
(291986, 0, 0, 1643, 3050.00,  5050.00, 120.000, 0, 0),   -- Wormhole: Mechagon Mountaintop (elevated point)
(296111, 0, 0, 1643, 2900.00,  4800.00, 30.0000, 0, 0);   -- Wormhole: Mechagon Prospectus Bay (southern Mechagon)


-- ============================================================================
-- Part 2: spell_script_names for SpellScript-based wormholes
-- ============================================================================
-- These wormholes use SpellScripts that override HandleDummy/HandleEffect to
-- show a destination picker or teleport directly. The C++ scripts use
-- Player::TeleportTo() with hardcoded coordinates.
--
-- 250796 = Wormhole Generator: Argus (Legion engineering toy)
-- 299083 = Wormhole Generator: Kul Tiras (BfA engineering)
-- 299084 = Wormhole Generator: Zandalar (BfA engineering)
-- 199978 = Intra-Dalaran Wormhole Generator (Legion Dalaran teleport)

DELETE FROM spell_script_names WHERE spell_id IN (250796, 299083, 299084, 199978);
INSERT INTO spell_script_names (spell_id, ScriptName) VALUES
(250796, 'spell_wormhole_argus'),
(299083, 'spell_wormhole_kul_tiras'),
(299084, 'spell_wormhole_zandalar'),
(199978, 'spell_wormhole_dalaran');


-- ============================================================================
-- Part 3: creature_template updates for NPC gossip wormholes
-- ============================================================================
-- These NPCs are summoned by engineering items and present a gossip menu with
-- destination choices. They need:
--   - npcflag = 1 (UNIT_NPC_FLAG_GOSSIP) so the player can interact
--   - ScriptName pointing to the custom CreatureScript

-- WoD Centrifuge Construct (entry 81205)
-- Already has npcflag=1, just needs ScriptName
UPDATE creature_template SET ScriptName = 'npc_wormhole_centrifuge' WHERE entry = 81205;

-- Reaves — Wormhole Mode (entry 104677)
-- Needs both npcflag and ScriptName
UPDATE creature_template SET npcflag = 1, ScriptName = 'npc_wormhole_legion' WHERE entry = 104677;

-- Shadowlands Wormhole (entry 169501)
UPDATE creature_template SET npcflag = 1, ScriptName = 'npc_wormhole_shadowlands' WHERE entry = 169501;

-- Khaz Algar Wormhole (entry 223342)
UPDATE creature_template SET npcflag = 1, ScriptName = 'npc_wormhole_khaz_algar' WHERE entry = 223342;


-- ============================================================================
-- Part 4: npc_text entries for gossip menu headers
-- ============================================================================
-- Each wormhole gossip menu needs an npc_text entry for the greeting text.
-- We use BroadcastTextID 0 (no text) since the visual is the wormhole itself;
-- the player just sees destination options. Using IDs 900201-900204 to match
-- our MenuIDs.
--
-- We reuse BroadcastTextID 35938 ("This tear in the fabric of time and space
-- looks ominous...") from the Northrend wormhole for thematic consistency.

DELETE FROM npc_text WHERE ID IN (900201, 900202, 900203, 900204);
INSERT INTO npc_text (ID, Probability0, Probability1, Probability2, Probability3, Probability4, Probability5, Probability6, Probability7, BroadcastTextID0, BroadcastTextID1, BroadcastTextID2, BroadcastTextID3, BroadcastTextID4, BroadcastTextID5, BroadcastTextID6, BroadcastTextID7, VerifiedBuild) VALUES
(900201, 1, 0, 0, 0, 0, 0, 0, 0, 35938, 0, 0, 0, 0, 0, 0, 0, 0),  -- WoD Centrifuge
(900202, 1, 0, 0, 0, 0, 0, 0, 0, 35938, 0, 0, 0, 0, 0, 0, 0, 0),  -- Legion Reaves
(900203, 1, 0, 0, 0, 0, 0, 0, 0, 35938, 0, 0, 0, 0, 0, 0, 0, 0),  -- Shadowlands
(900204, 1, 0, 0, 0, 0, 0, 0, 0, 35938, 0, 0, 0, 0, 0, 0, 0, 0);  -- Khaz Algar


-- ============================================================================
-- Part 5: Gossip menus for NPC wormholes
-- ============================================================================
-- MenuIDs 900201-900204, matching the npc_text IDs above.
-- The C++ scripts reference these MenuIDs in InitGossipMenuFor() and
-- AddGossipItemFor() calls.

DELETE FROM gossip_menu WHERE MenuID IN (900201, 900202, 900203, 900204);
INSERT INTO gossip_menu (MenuID, TextID, VerifiedBuild) VALUES
(900201, 900201, 0),  -- WoD Centrifuge
(900202, 900202, 0),  -- Legion Reaves Wormhole
(900203, 900203, 0),  -- Shadowlands Wormhole
(900204, 900204, 0);  -- Khaz Algar Wormhole


-- ============================================================================
-- Part 6: Gossip menu options (destination choices)
-- ============================================================================
-- Each option represents one teleport destination. The C++ script reads these
-- via AddGossipItemFor(player, MenuID, OptionID, sender, action).
-- OptionNpc = 0 (no special NPC icon), GossipOptionID = 0 (custom, not from DB2).
-- The C++ OnGossipSelect handler maps actions to TeleportTo() coordinates.
--
-- Column reference:
--   MenuID, GossipOptionID, OptionID, OptionNpc, OptionText,
--   OptionBroadcastTextID, Language, Flags, ActionMenuID, ActionPoiID,
--   GossipNpcOptionID, BoxCoded, BoxMoney, BoxText, BoxBroadcastTextID,
--   SpellID, OverrideIconID, VerifiedBuild

-- ----- WoD Centrifuge (MenuID 900201) — Draenor destinations -----
DELETE FROM gossip_menu_option WHERE MenuID = 900201;
INSERT INTO gossip_menu_option (MenuID, GossipOptionID, OptionID, OptionNpc, OptionText, OptionBroadcastTextID, Language, Flags, ActionMenuID, ActionPoiID, GossipNpcOptionID, BoxCoded, BoxMoney, BoxText, BoxBroadcastTextID, SpellID, OverrideIconID, VerifiedBuild) VALUES
(900201, 0, 0, 0, 'Frostfire Ridge',       0, 0, 0, 0, 0, NULL, 0, 0, '', 0, NULL, NULL, 0),
(900201, 0, 1, 0, 'Gorgrond',              0, 0, 0, 0, 0, NULL, 0, 0, '', 0, NULL, NULL, 0),
(900201, 0, 2, 0, 'Talador',               0, 0, 0, 0, 0, NULL, 0, 0, '', 0, NULL, NULL, 0),
(900201, 0, 3, 0, 'Spires of Arak',        0, 0, 0, 0, 0, NULL, 0, 0, '', 0, NULL, NULL, 0),
(900201, 0, 4, 0, 'Nagrand',               0, 0, 0, 0, 0, NULL, 0, 0, '', 0, NULL, NULL, 0),
(900201, 0, 5, 0, 'Shadowmoon Valley',     0, 0, 0, 0, 0, NULL, 0, 0, '', 0, NULL, NULL, 0),
(900201, 0, 6, 0, 'Ashran',                0, 0, 0, 0, 0, NULL, 0, 0, '', 0, NULL, NULL, 0);

-- ----- Legion Reaves Wormhole (MenuID 900202) — Broken Isles destinations -----
DELETE FROM gossip_menu_option WHERE MenuID = 900202;
INSERT INTO gossip_menu_option (MenuID, GossipOptionID, OptionID, OptionNpc, OptionText, OptionBroadcastTextID, Language, Flags, ActionMenuID, ActionPoiID, GossipNpcOptionID, BoxCoded, BoxMoney, BoxText, BoxBroadcastTextID, SpellID, OverrideIconID, VerifiedBuild) VALUES
(900202, 0, 0, 0, 'Azsuna',                0, 0, 0, 0, 0, NULL, 0, 0, '', 0, NULL, NULL, 0),
(900202, 0, 1, 0, 'Val''sharah',           0, 0, 0, 0, 0, NULL, 0, 0, '', 0, NULL, NULL, 0),
(900202, 0, 2, 0, 'Highmountain',          0, 0, 0, 0, 0, NULL, 0, 0, '', 0, NULL, NULL, 0),
(900202, 0, 3, 0, 'Stormheim',             0, 0, 0, 0, 0, NULL, 0, 0, '', 0, NULL, NULL, 0),
(900202, 0, 4, 0, 'Suramar',               0, 0, 0, 0, 0, NULL, 0, 0, '', 0, NULL, NULL, 0);

-- ----- Shadowlands Wormhole (MenuID 900203) — Shadowlands destinations -----
DELETE FROM gossip_menu_option WHERE MenuID = 900203;
INSERT INTO gossip_menu_option (MenuID, GossipOptionID, OptionID, OptionNpc, OptionText, OptionBroadcastTextID, Language, Flags, ActionMenuID, ActionPoiID, GossipNpcOptionID, BoxCoded, BoxMoney, BoxText, BoxBroadcastTextID, SpellID, OverrideIconID, VerifiedBuild) VALUES
(900203, 0, 0, 0, 'Bastion',               0, 0, 0, 0, 0, NULL, 0, 0, '', 0, NULL, NULL, 0),
(900203, 0, 1, 0, 'Maldraxxus',            0, 0, 0, 0, 0, NULL, 0, 0, '', 0, NULL, NULL, 0),
(900203, 0, 2, 0, 'Ardenweald',            0, 0, 0, 0, 0, NULL, 0, 0, '', 0, NULL, NULL, 0),
(900203, 0, 3, 0, 'Revendreth',            0, 0, 0, 0, 0, NULL, 0, 0, '', 0, NULL, NULL, 0),
(900203, 0, 4, 0, 'The Maw',               0, 0, 0, 0, 0, NULL, 0, 0, '', 0, NULL, NULL, 0),
(900203, 0, 5, 0, 'Oribos',                0, 0, 0, 0, 0, NULL, 0, 0, '', 0, NULL, NULL, 0),
(900203, 0, 6, 0, 'Zereth Mortis',         0, 0, 0, 0, 0, NULL, 0, 0, '', 0, NULL, NULL, 0);

-- ----- Khaz Algar Wormhole (MenuID 900204) — Khaz Algar destinations -----
DELETE FROM gossip_menu_option WHERE MenuID = 900204;
INSERT INTO gossip_menu_option (MenuID, GossipOptionID, OptionID, OptionNpc, OptionText, OptionBroadcastTextID, Language, Flags, ActionMenuID, ActionPoiID, GossipNpcOptionID, BoxCoded, BoxMoney, BoxText, BoxBroadcastTextID, SpellID, OverrideIconID, VerifiedBuild) VALUES
(900204, 0, 0, 0, 'Isle of Dorn',          0, 0, 0, 0, 0, NULL, 0, 0, '', 0, NULL, NULL, 0),
(900204, 0, 1, 0, 'The Ringing Deeps',     0, 0, 0, 0, 0, NULL, 0, 0, '', 0, NULL, NULL, 0),
(900204, 0, 2, 0, 'Hallowfall',            0, 0, 0, 0, 0, NULL, 0, 0, '', 0, NULL, NULL, 0),
(900204, 0, 3, 0, 'Azj-Kahet',             0, 0, 0, 0, 0, NULL, 0, 0, '', 0, NULL, NULL, 0);


-- ============================================================================
-- Part 7: creature_template_gossip assignments
-- ============================================================================
-- Link each NPC to its gossip menu. NPC 81205 already has an entry pointing to
-- MenuID 81205 (with TextID 999999999); we replace it with our custom menu.

DELETE FROM creature_template_gossip WHERE CreatureID IN (81205, 104677, 169501, 223342);
INSERT INTO creature_template_gossip (CreatureID, MenuID, VerifiedBuild) VALUES
(81205,  900201, 0),  -- WoD Centrifuge -> Draenor destinations
(104677, 900202, 0),  -- Reaves Wormhole -> Broken Isles destinations
(169501, 900203, 0),  -- Shadowlands Wormhole -> Shadowlands destinations
(223342, 900204, 0);  -- Khaz Algar Wormhole -> Khaz Algar destinations


-- ============================================================================
-- Summary of C++ script <-> SQL data mapping
-- ============================================================================
-- Script Name                    | Type        | Menu/Spell | Destinations
-- -------------------------------|-------------|------------|------------------
-- spell_wormhole_argus           | SpellScript | 250796     | Hardcoded TeleportTo()
-- spell_wormhole_kul_tiras       | SpellScript | 299083     | Hardcoded TeleportTo()
-- spell_wormhole_zandalar        | SpellScript | 299084     | Hardcoded TeleportTo()
-- spell_wormhole_dalaran         | SpellScript | 199978     | Hardcoded TeleportTo()
-- npc_wormhole_centrifuge        | CreatureAI  | 900201     | Gossip -> TeleportTo()
-- npc_wormhole_legion            | CreatureAI  | 900202     | Gossip -> TeleportTo()
-- npc_wormhole_shadowlands       | CreatureAI  | 900203     | Gossip -> TeleportTo()
-- npc_wormhole_khaz_algar        | CreatureAI  | 900204     | Gossip -> TeleportTo()
-- (no script — SQL only)         | spell_target_position    | 291981/291983/291986/296111
