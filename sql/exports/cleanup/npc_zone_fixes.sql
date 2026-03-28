-- ============================================================================
-- NPC Zone Mismatch Fixes
-- Generated: 2026-03-01 19:27:37
-- REVIEW CAREFULLY before applying. These are conservative fixes only.
-- ============================================================================
USE `world`;
-- ============================================================================
-- SECTION 1: zoneId/areaId Inconsistencies
-- The creature's areaId belongs to a different top-level zone than its zoneId.
-- Fix: Update zoneId to match the areaId's actual parent zone.
-- Total: 242 mismatched (npc, zone, area) combos
-- ============================================================================

-- Pattern: zoneId=6941 (Ashran) -> should be 8485 (Ashran) -- 236 NPCs, 1118 spawns
--   NPC 84223: Harrison Jones (areaId=7100, 1 spawns) [QuestGiver]
--   NPC 88682: Misirin Stouttoe (areaId=7100, 1 spawns) [QuestGiver]
--   NPC 82204: Atomik (areaId=7099, 1 spawns) [QuestGiver]
--   ... and 233 more NPCs
UPDATE creature SET zoneId = 8485 WHERE zoneId = 6941 AND areaId IN (7080,7088,7099,7100,7201,7275,7279,7311,7378,7379,7438,7439,7444,7476,7477,7478,7479);

-- Pattern: zoneId=4812 (Icecrown Citadel) -> should be 4859 (The Frozen Throne) -- 3 NPCs, 23 spawns
--   NPC 22515: World Trigger (areaId=4859, 21 spawns)
--   NPC 39190: Wicked Spirit (areaId=4859, 1 spawns)
--   NPC 36597: The Lich King (areaId=4859, 1 spawns)
UPDATE creature SET zoneId = 4859 WHERE zoneId = 4812 AND areaId IN (4859);

-- Pattern: zoneId=1537 (Ironforge) -> should be 1 (Dun Morogh) -- 1 NPCs, 6 spawns
--   NPC 153931: Brewfest Reveler (areaId=809, 6 spawns)
UPDATE creature SET zoneId = 1 WHERE zoneId = 1537 AND areaId IN (809);

-- Pattern: zoneId=1220 (Darnassian Base Camp) -> should be 7502 (Dalaran) -- 1 NPCs, 2 spawns
--   NPC 99895: Dirty Rat (areaId=8045, 2 spawns)
UPDATE creature SET zoneId = 7502 WHERE zoneId = 1220 AND areaId IN (8045);

-- Pattern: zoneId=8392 (Dalaran Sewers) -> should be 7502 (Dalaran) -- 1 NPCs, 1 spawns
--   NPC 97586: Underbelly Guard (areaId=7502, 1 spawns)
UPDATE creature SET zoneId = 7502 WHERE zoneId = 8392 AND areaId IN (7502);


-- ============================================================================
-- SECTION 2: Wowhead Cross-Reference Review Items
-- NPCs whose DB spawn zone doesn't match Wowhead data at all.
-- These require MANUAL verification -- coordinates may need updating.
-- Total: 198 items for review
-- ============================================================================

-- NPC 208384: Courier Nailen (QuestGiver, FlightMaster)
-- Wowhead: Azj-Kahet (14752), DB: Hallowfall (14838), 2 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 208384;

-- NPC 144055: Cedrik Prose (QuestGiver, FlightMaster)
-- Wowhead: Arathi Highlands (45), DB: Arathi Highlands (9734), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 144055;

-- NPC 144069: Urda (QuestGiver, FlightMaster)
-- Wowhead: Arathi Highlands (45), DB: Arathi Highlands (9734), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 144069;

-- NPC 209330: Tekazza of the Aimless Few (QuestGiver, FlightMaster)
-- Wowhead: Azj-Kahet (14752), DB: Hallowfall (14838), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 209330;

-- NPC 29532: Ajay Green (QuestGiver, Vendor, Innkeeper)
-- Wowhead: Dalaran (6611), DB: Dalaran (4395), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 29532;

-- NPC 32413: Isirami Fairwind (QuestGiver, Vendor, Innkeeper)
-- Wowhead: Dalaran (6611), DB: Dalaran (4395), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 32413;

-- NPC 100012: Innkeeper Belm (QuestGiver, Vendor, Innkeeper)
-- Wowhead: Dun Morogh (1), DB: Frostfire Ridge (6720), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 100012;

-- NPC 141732: Vikki Lonsav (QuestGiver, Vendor, Innkeeper)
-- Wowhead: Arathi Highlands (45), DB: Arathi Highlands (9734), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 141732;

-- NPC 144139: Innkeeper Adegwa (QuestGiver, Vendor, Innkeeper)
-- Wowhead: Arathi Highlands (45), DB: Arathi Highlands (9734), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 144139;

-- NPC 93465: Grimwing (FlightMaster)
-- Wowhead: Broken Shore (7543), DB: Acherus: The Ebon Hold (7679), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 93465;

-- NPC 142008: Grayson Bell (FlightMaster)
-- Wowhead: Arathi Highlands (45), DB: Arathi Highlands (9734), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 142008;

-- NPC 93550: Quartermaster Ozorg (QuestGiver, Vendor, Repair)
-- Wowhead: Broken Shore (7543), DB: Acherus: The Ebon Hold (7679), 2 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 93550;

-- NPC 102193: Filgo Scrapbottom (QuestGiver, Vendor, Repair)
-- Wowhead: Dalaran (7502), DB: Dalaran Sewers (8392), 2 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 102193;

-- NPC 59341: Merchant Tantan (QuestGiver, Vendor, Repair)
-- Wowhead: Kun-Lai Summit (5841), DB: Vale of Eternal Blossoms (5840), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 59341;

-- NPC 93555: Amal'thazad (QuestGiver, Vendor, Repair)
-- Wowhead: Broken Shore (7543), DB: Acherus: The Ebon Hold (7679), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 93555;

-- NPC 97359: Raethan (QuestGiver, Vendor, Repair)
-- Wowhead: Dalaran (7502), DB: Dalaran Sewers (8392), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 97359;

-- NPC 97360: Matthew Rabis (QuestGiver, Vendor, Repair)
-- Wowhead: Dalaran (7502), DB: Dalaran Sewers (8392), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 97360;

-- NPC 97361: Oxana Demonslay (QuestGiver, Vendor, Repair)
-- Wowhead: Dalaran (7502), DB: Dalaran Sewers (8392), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 97361;

-- NPC 97362: Dazzik "Proudmoore" (QuestGiver, Vendor, Repair)
-- Wowhead: Dalaran (7502), DB: Dalaran Sewers (8392), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 97362;

-- NPC 100017: Stokalfr (QuestGiver, Vendor, Repair)
-- Wowhead: Suramar (7637), DB: Shadowmoon Valley (6719), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 100017;

-- NPC 144065: Samuel Hawke (QuestGiver, Vendor, Repair)
-- Wowhead: Arathi Highlands (45), DB: Arathi Highlands (9734), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 144065;

-- NPC 100011: Tannok Frosthammer (QuestGiver, Innkeeper)
-- Wowhead: Dun Morogh (1), DB: Frostfire Ridge (6720), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 100011;

-- NPC 136371: Rakle the Wretched (QuestGiver, Innkeeper)
-- Wowhead: Zuldazar (8499), DB: Zuldazar (9570), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 136371;

-- NPC 210055: Irizi the Unwanted (QuestGiver, Innkeeper)
-- Wowhead: Azj-Kahet (14752), DB: Hallowfall (14838), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 210055;

-- NPC 99531: Aggra (QuestGiver, Trainer)
-- Wowhead: Dalaran (7502), DB: The Maelstrom (7745), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 99531;

-- NPC 15197: Darkcaller Yanka (QuestGiver, Vendor)
-- Wowhead: Undercity (1497), DB: Tirisfal Glades (85), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 15197;

-- NPC 97111: Illanna Dreadmoore (QuestGiver, Vendor)
-- Wowhead: Broken Shore (7543), DB: Acherus: The Ebon Hold (7679), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 97111;

-- NPC 110035: Historian Ju'pa (QuestGiver, Vendor)
-- Wowhead: Orgrimmar (1637), DB: Tanaris (440), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 110035;

-- NPC 110642: Fizzi Liverzapper (QuestGiver, Vendor)
-- Wowhead: Dalaran (7502), DB: Dalaran Sewers (8392), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 110642;

-- NPC 136359: B'wizati (QuestGiver, Vendor)
-- Wowhead: Zuldazar (8499), DB: Zuldazar (9570), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 136359;

-- NPC 141737: Emily Jackson (QuestGiver, Vendor, StableMaster)
-- Wowhead: Arathi Highlands (45), DB: Arathi Highlands (9734), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 141737;

-- NPC 142818: Moj'ito (QuestGiver, Vendor)
-- Wowhead: Zuldazar (8499), DB: Zuldazar (9570), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 142818;

-- NPC 144140: Tharlidun (QuestGiver, Vendor, StableMaster)
-- Wowhead: Arathi Highlands (45), DB: Arathi Highlands (9734), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 144140;

-- NPC 199705: Trading Post Barker (QuestGiver, Vendor)
-- Wowhead: Thaldraszus (13647), DB: Orgrimmar (1637), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 199705;

-- NPC 144129: Plugger Spazzring (Vendor, Innkeeper)
-- Wowhead: Blackrock Depths (1584), DB: Blackrock Depths (10028), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 144129;

-- NPC 121230: Alleria Windrunner (QuestGiver)
-- Wowhead: Krokuun (8574), DB: Eredath (8701), 11 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 121230;

-- NPC 120533: Prophet Velen (QuestGiver)
-- Wowhead: Krokuun (8574), DB: Eredath (8701), 10 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 120533;

-- NPC 221842: Alchemist Talbax (QuestGiver)
-- Wowhead: Azj-Kahet (14752), DB: Hallowfall (14838), 7 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 221842;

-- NPC 124312: High Exarch Turalyon (QuestGiver)
-- Wowhead: Krokuun (8574), DB: Eredath (8701), 6 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 124312;

-- NPC 202700: Lysander Starshade (QuestGiver)
-- Wowhead: Felwood (361), DB: Stormwind City (1519), 6 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 202700;

-- NPC 207350: Wrathion (QuestGiver)
-- Wowhead: Thaldraszus (13647), DB: Emerald Dream (14529), 6 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 207350;

-- NPC 190000: Kalecgos (QuestGiver)
-- Wowhead: The Azure Span (13646), DB: Brawl'gar Arena (6298), 5 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 190000;

-- NPC 91242: Solog Roark (QuestGiver)
-- Wowhead: Frostfire Ridge (6720), DB: Gorgrond (6721), 4 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 91242;

-- NPC 136683: Trade Prince Gallywix (QuestGiver)
-- Wowhead: Drustvar (8721), DB: Zuldazar (8499), 4 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 136683;

-- NPC 157997: Kelsey Steelspark (QuestGiver)
-- Wowhead: Mechagon (10290), DB: Mechagon City (12825), 4 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 157997;

-- NPC 178908: Al'dalil (QuestGiver)
-- Wowhead: Oribos (10565), DB: Tazavesh, the Veiled Market (13672), 4 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 178908;

-- NPC 100016: Fjolrik (QuestGiver)
-- Wowhead: Suramar (7637), DB: Shadowmoon Valley (6719), 3 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 100016;

-- NPC 119340: Prophet Velen (QuestGiver)
-- Wowhead: Stormwind City (1519), DB: Broken Shore (7543), 3 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 119340;

-- NPC 134478: Kimbul (QuestGiver)
-- Wowhead: Zuldazar (8499), DB: Zuldazar (9570), 3 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 134478;

-- NPC 158145: Prince Erazmin (QuestGiver)
-- Wowhead: Mechagon (10290), DB: Mechagon City (12825), 3 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 158145;

-- NPC 159571: Flouresce Brightgear (QuestGiver)
-- Wowhead: Mechagon (10290), DB: Mechagon City (12825), 3 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 159571;

-- NPC 168162: Baine Bloodhoof (QuestGiver)
-- Wowhead: The Maw (11400), DB: The Maw (13332), 3 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 168162;

-- NPC 231541: Sky-Captain Cableclamp (QuestGiver)
-- Wowhead: Isle of Dorn (14717), DB: Siren Isle (10416), 3 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 231541;

-- NPC 78423: Archmage Khadgar (QuestGiver)
-- Wowhead: Vale of Eternal Blossoms (5840), DB: Blasted Lands (4), 2 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 78423;

-- NPC 96539: Stormcaller Mylra (QuestGiver)
-- Wowhead: The Maelstrom (8046), DB: The Maelstrom (7745), 2 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 96539;

-- NPC 96644: Sky Admiral Rogers (QuestGiver)
-- Wowhead: Stormwind City (1519), DB: Dalaran (7502), 2 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 96644;

-- NPC 110866: Delas Moonfang (QuestGiver)
-- Wowhead: Eastern Plaguelands (139), DB: Niskara (8105), 2 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 110866;

-- NPC 114838: Tyrande Whisperwind (QuestGiver)
-- Wowhead: Suramar (7637), DB: The Nighthold (8025), 2 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 114838;

-- NPC 114841: Lady Liadrin (QuestGiver)
-- Wowhead: Suramar (7637), DB: The Nighthold (8025), 2 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 114841;

-- NPC 115499: Silgryn (QuestGiver)
-- Wowhead: Suramar (7637), DB: The Nighthold (8025), 2 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 115499;

-- NPC 120844: Alleria Windrunner (QuestGiver)
-- Wowhead: Antoran Wastes (8899), DB: Krokuun (8574), 2 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 120844;

-- NPC 121263: Grand Artificer Romuul (QuestGiver)
-- Wowhead: Azuremyst Isle (8840), DB: Krokuun (8574), 2 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 121263;

-- NPC 122065: Lady Liadrin (QuestGiver)
-- Wowhead: Durotar (14), DB: The Exodar (8842), 2 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 122065;

-- NPC 134493: Pa'ku (QuestGiver)
-- Wowhead: Zuldazar (8499), DB: Zuldazar (9570), 2 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 134493;

-- NPC 144152: Moira Thaurissan (QuestGiver)
-- Wowhead: Blackrock Depths (1584), DB: Blackrock Depths (10028), 2 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 144152;

-- NPC 158713: The Curator (QuestGiver)
-- Wowhead: Torghast, Tower of the Damned (10472), DB: Revendreth (10413), 2 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 158713;

-- NPC 159545: Stuard Sharpsprocket (QuestGiver)
-- Wowhead: Mechagon (10290), DB: Mechagon City (12825), 2 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 159545;

-- NPC 159558: Pegi Cogster (QuestGiver)
-- Wowhead: Mechagon (10290), DB: Mechagon City (12825), 2 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 159558;

-- NPC 159559: Elya Codepunch (QuestGiver)
-- Wowhead: Mechagon (10290), DB: Mechagon City (12825), 2 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 159559;

-- NPC 159567: Idee Quickcoil (QuestGiver)
-- Wowhead: Mechagon (10290), DB: Mechagon City (12825), 2 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 159567;

-- NPC 159587: Gelbin Mekkatorque (QuestGiver)
-- Wowhead: Mechagon (10290), DB: Mechagon City (12825), 2 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 159587;

-- NPC 171770: Ve'nari (QuestGiver)
-- Wowhead: The Maw (13587), DB: The Maw (11400), 2 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 171770;

-- NPC 184000: Pelagos (QuestGiver)
-- Wowhead: Oribos (10565), DB: Zereth Mortis (13536), 2 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 184000;

-- NPC 208782: Executor Nizrek (QuestGiver)
-- Wowhead: Azj-Kahet (14752), DB: Hallowfall (14838), 2 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 208782;

-- NPC 211721: Sir Jonathan Trueheart (QuestGiver)
-- Wowhead: Azj-Kahet (14752), DB: Hallowfall (14838), 2 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 211721;

-- NPC 217692: Ar'syn (QuestGiver)
-- Wowhead: Azj-Kahet (14752), DB: Hallowfall (14838), 2 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 217692;

-- NPC 7313: Priestess A'moora (QuestGiver)
-- Wowhead: Darnassus (1657), DB: Teldrassil (141), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 7313;

-- NPC 14387: Lothos Riftwaker (QuestGiver)
-- Wowhead: Blackrock Mountain (25), DB: Burning Steppes (46), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 14387;

-- NPC 15309: Spoops (QuestGiver)
-- Wowhead: Undercity (1497), DB: Tirisfal Glades (85), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 15309;

-- NPC 15502: Andorgos (QuestGiver)
-- Wowhead: Dragon Soul (5892), DB: Ahn'Qiraj (3428), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 15502;

-- NPC 25975: Master Fire Eater (QuestGiver)
-- Wowhead: Ironforge (1537), DB: The Exodar (3557), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 25975;

-- NPC 26113: Master Flame Eater (QuestGiver)
-- Wowhead: Thunder Bluff (1638), DB: Silvermoon City (Burning Crusade) (3487), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 26113;

-- NPC 26401: Summer Scorchling (QuestGiver)
-- Wowhead: Elwynn Forest (12), DB: Blasted Lands (4), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 26401;

-- NPC 30536: Elder Igasho (QuestGiver)
-- Wowhead: The Nexus (5786), DB: The Nexus (4265), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 30536;

-- NPC 34476: Cheerful Forsaken Spirit (QuestGiver)
-- Wowhead: Undercity (1497), DB: Tirisfal Glades (85), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 34476;

-- NPC 37776: Apprentice Nelphi (QuestGiver)
-- Wowhead: Dalaran (6611), DB: Dalaran (4395), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 37776;

-- NPC 37780: Dark Ranger Vorel (QuestGiver)
-- Wowhead: Dalaran (6611), DB: Dalaran (4395), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 37780;

-- NPC 38293: Junior Inspector (QuestGiver)
-- Wowhead: Ironforge (1537), DB: The Exodar (3557), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 38293;

-- NPC 38295: Junior Detective (QuestGiver)
-- Wowhead: Thunder Bluff (1638), DB: Silvermoon City (Burning Crusade) (3487), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 38295;

-- NPC 44058: Horton Hornblower (QuestGiver)
-- Wowhead: Durotar (14), DB: Northern Barrens (17), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 44058;

-- NPC 44402: Auld Stonespire (QuestGiver)
-- Wowhead: Razorfen Kraul (491), DB: Stonetalon Mountains (406), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 44402;

-- NPC 46071: Lord Itharius (QuestGiver)
-- Wowhead: The Temple of Atal'Hakkar (1477), DB: Swamp of Sorrows (8), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 46071;

-- NPC 53763: Candace Fenlow (QuestGiver)
-- Wowhead: Undercity (1497), DB: Tirisfal Glades (85), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 53763;

-- NPC 57770: Zazzo Twinklefingers (QuestGiver)
-- Wowhead: Deadwind Pass (41), DB: Ruins of Gilneas (4706), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 57770;

-- NPC 57864: Alurmi (QuestGiver)
-- Wowhead: Well of Eternity (5788), DB: End Time (5789), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 57864;

-- NPC 63314: Wodin the Troll-Servant (QuestGiver)
-- Wowhead: Arena of Annihilation (6219), DB: Kun-Lai Summit (5841), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 63314;

-- NPC 63315: Gurgthock (QuestGiver)
-- Wowhead: Arena of Annihilation (6219), DB: Kun-Lai Summit (5841), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 63315;

-- NPC 64562: Talking Skull (QuestGiver)
-- Wowhead: Scholomance (2057), DB: Scholomance (6066), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 64562;

-- NPC 64563: Talking Skull (QuestGiver)
-- Wowhead: Scholomance (2057), DB: Scholomance (6066), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 64563;

-- NPC 66292: Sky Admiral Rogers (QuestGiver)
-- Wowhead: The Jade Forest (5785), DB: Stormwind City (1519), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 66292;

-- NPC 66741: Aki the Chosen (QuestGiver)
-- Wowhead: Vale of Eternal Blossoms (9105), DB: Vale of Eternal Blossoms (5840), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 66741;

-- NPC 73632: Cowardly Zue (QuestGiver, StableMaster)
-- Wowhead: Celestial Tournament (6771), DB: Timeless Isle (6757), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 73632;

-- NPC 78192: Choluna (QuestGiver)
-- Wowhead: Frostfire Ridge (6720), DB: Gorgrond (6721), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 78192;

-- NPC 81765: Vivianne (QuestGiver)
-- Wowhead: Frostfire Ridge (6720), DB: Ashran (6941), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 81765;

-- NPC 93491: Lord Thorval (QuestGiver)
-- Wowhead: Broken Shore (7543), DB: Acherus: The Ebon Hold (7679), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 93491;

-- NPC 93568: Siouxsie the Banshee (QuestGiver)
-- Wowhead: Broken Shore (7543), DB: Acherus: The Ebon Hold (7679), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 93568;

-- NPC 95900: Minerva Ravensorrow (QuestGiver)
-- Wowhead: Broken Shore (7543), DB: Acherus: The Ebon Hold (7679), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 95900;

-- NPC 96347: Flitz (QuestGiver)
-- Wowhead: Dalaran (7502), DB: Dalaran Sewers (8392), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 96347;

-- NPC 96527: Thrall (QuestGiver)
-- Wowhead: Deepholm (7955), DB: The Maelstrom (7745), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 96527;

-- NPC 96530: Erunak Stonespeaker (QuestGiver)
-- Wowhead: The Maelstrom (8046), DB: The Maelstrom (7745), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 96530;

-- NPC 96541: Rehgar Earthfury (QuestGiver)
-- Wowhead: The Maelstrom (8046), DB: The Maelstrom (7745), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 96541;

-- NPC 97072: Grand Master Siegesmith Corvus (QuestGiver)
-- Wowhead: Broken Shore (7543), DB: Acherus: The Ebon Hold (7679), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 97072;

-- NPC 98100: Taoshi (QuestGiver)
-- Wowhead: Dalaran (7502), DB: The Great Sea (8445), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 98100;

-- NPC 100005: Dorro Highmountain (QuestGiver)
-- Wowhead: Stormheim (7541), DB: Shadowmoon Valley (6719), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 100005;

-- NPC 101456: Ritssyn Flamescowl (QuestGiver)
-- Wowhead: Dalaran (7502), DB: Dalaran Sewers (8392), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 101456;

-- NPC 101492: Ms. Xiulan (QuestGiver)
-- Wowhead: Dalaran (7502), DB: Dalaran Sewers (8392), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 101492;

-- NPC 102846: Alodi (QuestGiver)
-- Wowhead: The Violet Hold (7777), DB: Hall of the Guardian (7879), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 102846;

-- NPC 105464: Val'zuun (QuestGiver)
-- Wowhead: Dalaran (7502), DB: Dalaran Sewers (8392), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 105464;

-- NPC 110738: Kaela Grimlocket (QuestGiver)
-- Wowhead: Dalaran (7502), DB: Dalaran Sewers (8392), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 110738;

-- NPC 110904: Archmage Nielthende (QuestGiver)
-- Wowhead: Azsuna (7334), DB: Suramar (7637), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 110904;

-- NPC 117694: Brann Bronzebeard (QuestGiver)
-- Wowhead: The Maelstrom (8549), DB: Netherlight Temple (7834), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 117694;

-- NPC 122052: Light Crystal (QuestGiver)
-- Wowhead: Azuremyst Isle (8840), DB: The Exodar (8842), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 122052;

-- NPC 122347: Pterrordax (QuestGiver)
-- Wowhead: Nazmir (8500), DB: Zuldazar (8499), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 122347;

-- NPC 136356: Bri'tani (QuestGiver)
-- Wowhead: Zuldazar (8499), DB: Zuldazar (9570), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 136356;

-- NPC 136357: Chronicler Bah'kini (QuestGiver)
-- Wowhead: Zuldazar (8499), DB: Zuldazar (9570), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 136357;

-- NPC 136358: No'ci the Scribe (QuestGiver)
-- Wowhead: Zuldazar (8499), DB: Zuldazar (9570), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 136358;

-- NPC 136360: Headhunter Lani (QuestGiver)
-- Wowhead: Zuldazar (8499), DB: Zuldazar (9570), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 136360;

-- NPC 136367: Vessel Iluna (QuestGiver)
-- Wowhead: Zuldazar (8499), DB: Zuldazar (9570), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 136367;

-- NPC 136368: Vessel Zetoa (QuestGiver)
-- Wowhead: Zuldazar (8499), DB: Zuldazar (9570), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 136368;

-- NPC 136369: Madam Konawla (QuestGiver)
-- Wowhead: Zuldazar (8499), DB: Zuldazar (9570), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 136369;

-- NPC 136370: Old Tella (QuestGiver)
-- Wowhead: Zuldazar (8499), DB: Zuldazar (9570), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 136370;

-- NPC 136372: Toko (QuestGiver)
-- Wowhead: Zuldazar (8499), DB: Zuldazar (9570), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 136372;

-- NPC 143018: Captain Roderick Brewston (QuestGiver)
-- Wowhead: Arathi Highlands (45), DB: Arathi Highlands (9734), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 143018;

-- NPC 144119: Kasea Angerforge (QuestGiver, StableMaster)
-- Wowhead: Blackrock Depths (1584), DB: Blackrock Depths (10028), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 144119;

-- NPC 144130: Mistress Nagmara (QuestGiver)
-- Wowhead: Blackrock Depths (1584), DB: Blackrock Depths (10028), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 144130;

-- NPC 144154: Anvil-Thane Thurgaden (QuestGiver)
-- Wowhead: Blackrock Depths (1584), DB: Blackrock Depths (10028), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 144154;

-- NPC 156187: Disgruntled Laborer (QuestGiver)
-- Wowhead: Zuldazar (8499), DB: Nazjatar (10052), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 156187;

-- NPC 158635: Xolartios (QuestGiver)
-- Wowhead: Isle of Dorn (14717), DB: Zaralek Cavern (14022), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 158635;

-- NPC 199261: Holiday Enthusiast (QuestGiver)
-- Wowhead: Thaldraszus (13647), DB: Isle of Dorn (14717), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 199261;

-- NPC 204198: Assistant Phineas (QuestGiver)
-- Wowhead: Darkmoon Island (5861), DB: Stormwind City (1519), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 204198;

-- NPC 205647: Kalecgos (QuestGiver)
-- Wowhead: Thaldraszus (13647), DB: Zaralek Cavern (14022), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 205647;

-- NPC 206399: Tyrande Whisperwind (QuestGiver)
-- Wowhead: Emerald Dream (14529), DB: Ohn'ahran Plains (13645), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 206399;

-- NPC 24935: Vend-O-Tron D-Luxe (Vendor, Repair)
-- Wowhead: Mulgore (215), DB: The Deadmines (1581), 5 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 24935;

-- NPC 16678: Rahein (Vendor, Repair)
-- Wowhead: Silvermoon City (Burning Crusade) (3487), DB: Eversong Woods (Burning Crusade) (3430), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 16678;

-- NPC 97363: K'huta (Vendor, Repair)
-- Wowhead: Dalaran (7502), DB: Dalaran Sewers (8392), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 97363;

-- NPC 97364: Laura Malley (Vendor, Repair)
-- Wowhead: Dalaran (7502), DB: Dalaran Sewers (8392), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 97364;

-- NPC 97366: The Widow (Vendor, Repair)
-- Wowhead: Dalaran (7502), DB: Dalaran Sewers (8392), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 97366;

-- NPC 107760: Strap Bucklebolt (Vendor, Repair)
-- Wowhead: Dalaran (7502), DB: Dalaran Sewers (8392), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 107760;

-- NPC 107764: Thuni (Vendor, Repair)
-- Wowhead: Dalaran (7502), DB: Dalaran Sewers (8392), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 107764;

-- NPC 133890: Jonathan Flynn (Vendor, Repair)
-- Wowhead: Arathi Highlands (45), DB: Arathi Highlands (9734), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 133890;

-- NPC 141734: Jannos Ironwill (Vendor, Repair)
-- Wowhead: Arathi Highlands (45), DB: Arathi Highlands (9734), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 141734;

-- NPC 144062: Mu'uta (Vendor, Repair)
-- Wowhead: Arathi Highlands (45), DB: Arathi Highlands (9734), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 144062;

-- NPC 190105: Meela Dynaspark (Vendor, Repair)
-- Wowhead: Zaralek Cavern (14022), DB: Ohn'ahran Plains (13645), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 190105;

-- NPC 223596: Merchant Felagunne (Vendor, Repair)
-- Wowhead: Ruins of Gilneas (4706), DB: Amirdrassil (15105), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 223596;

-- NPC 29530: Binzik Goldbook (Banker)
-- Wowhead: Dalaran (6611), DB: Dalaran (4395), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 29530;

-- NPC 17510: Izmir (Trainer)
-- Wowhead: The Exodar (8277), DB: The Exodar (3557), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 17510;

-- NPC 28694: Alard Schmied (Trainer)
-- Wowhead: Dalaran (6611), DB: Dalaran (4395), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 28694;

-- NPC 29506: Orland Schaeffer (Trainer)
-- Wowhead: Dalaran (6611), DB: Dalaran (4395), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 29506;

-- NPC 47579: Dariness the Learned (Trainer)
-- Wowhead: Dalaran (6611), DB: Dalaran (4395), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 47579;

-- NPC 93509: Lady Alistra (Trainer)
-- Wowhead: Broken Shore (7543), DB: Acherus: The Ebon Hold (7679), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 93509;

-- NPC 96954: Nelur Lightsown (Trainer)
-- Wowhead: Dalaran (7502), DB: The Great Sea (8445), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 96954;

-- NPC 100003: Hogral Bakkan (Trainer)
-- Wowhead: Dun Morogh (1), DB: Talador (6662), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 100003;

-- NPC 100013: Thamner Pol (Trainer)
-- Wowhead: Dun Morogh (1), DB: Frostfire Ridge (6720), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 100013;

-- NPC 144066: Slagg (Trainer)
-- Wowhead: Arathi Highlands (45), DB: Arathi Highlands (9734), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 144066;

-- NPC 50305: Moon Priestess Lasara (Vendor)
-- Wowhead: Stormwind City (1519), DB: Darnassus (1657), 2 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 50305;

-- NPC 89830: Brew Vendor (Vendor)
-- Wowhead: Ironforge (1537), DB: Orgrimmar (1637), 2 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 89830;

-- NPC 212059: Sofee Batalsworn (Vendor)
-- Wowhead: Azj-Kahet (14752), DB: Hallowfall (14838), 2 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 212059;

-- NPC 231824: Kari Bridgeblaster (Vendor)
-- Wowhead: Undermine (15347), DB: Liberation of Undermine (15522), 2 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 231824;

-- NPC 844: Antonio Perelli (Vendor)
-- Wowhead: Redridge Mountains (44), DB: Elwynn Forest (12), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 844;

-- NPC 7978: Bimble Longberry (Vendor)
-- Wowhead: Ironforge (1537), DB: Dun Morogh (1), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 7978;

-- NPC 16757: Bildine (Vendor)
-- Wowhead: The Exodar (8277), DB: The Exodar (3557), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 16757;

-- NPC 26123: Midsummer Supplier (Vendor)
-- Wowhead: Ironforge (1537), DB: The Exodar (3557), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 26123;

-- NPC 26124: Midsummer Merchant (Vendor)
-- Wowhead: Orgrimmar (1637), DB: Silvermoon City (Burning Crusade) (3487), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 26124;

-- NPC 28707: Angelo Pescatore (Vendor)
-- Wowhead: Dalaran (6611), DB: Dalaran (4395), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 28707;

-- NPC 35342: Bountiful Barrel (Vendor)
-- Wowhead: Durotar (14), DB: Eversong Woods (Burning Crusade) (3430), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 35342;

-- NPC 35826: Kaye Toogie (Vendor)
-- Wowhead: Crystalsong Forest (2817), DB: Dalaran (4395), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 35826;

-- NPC 50304: Captain Donald Adams (Vendor)
-- Wowhead: Orgrimmar (1637), DB: Undercity (1497), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 50304;

-- NPC 50307: Lord Candren (Vendor)
-- Wowhead: Stormwind City (1519), DB: Darnassus (1657), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 50307;

-- NPC 53756: Darla (Vendor)
-- Wowhead: Undercity (1497), DB: Tirisfal Glades (85), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 53756;

-- NPC 53757: Chub (Vendor)
-- Wowhead: Undercity (1497), DB: Tirisfal Glades (85), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 53757;

-- NPC 53760: Farina (Vendor)
-- Wowhead: Undercity (1497), DB: Tirisfal Glades (85), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 53760;

-- NPC 73435: Grimthorn Redbeard (Vendor)
-- Wowhead: Celestial Tournament (6771), DB: Timeless Isle (6757), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 73435;

-- NPC 73816: Master Miantiao (Vendor)
-- Wowhead: Celestial Tournament (6771), DB: Timeless Isle (6757), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 73816;

-- NPC 73817: Smiling Jade (Vendor)
-- Wowhead: Celestial Tournament (6771), DB: Timeless Isle (6757), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 73817;

-- NPC 93551: Fester (Vendor)
-- Wowhead: Broken Shore (7543), DB: Acherus: The Ebon Hold (7679), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 93551;

-- NPC 97105: Alchemist Karloff (Vendor)
-- Wowhead: Broken Shore (7543), DB: Acherus: The Ebon Hold (7679), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 97105;

-- NPC 136355: Trader Haw'li (Vendor)
-- Wowhead: Zuldazar (8499), DB: Zuldazar (9570), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 136355;

-- NPC 141733: Narj Deepslice (Vendor)
-- Wowhead: Arathi Highlands (45), DB: Arathi Highlands (9734), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 141733;

-- NPC 141735: Drovnar Strongbrew (Vendor)
-- Wowhead: Arathi Highlands (45), DB: Arathi Highlands (9734), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 141735;

-- NPC 141736: Hammon Karwn (Vendor)
-- Wowhead: Arathi Highlands (45), DB: Arathi Highlands (9734), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 141736;

-- NPC 141761: Targot Jinglepocket (Vendor)
-- Wowhead: Arathi Highlands (45), DB: Arathi Highlands (9734), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 141761;

-- NPC 144057: Graud (Vendor)
-- Wowhead: Arathi Highlands (45), DB: Arathi Highlands (9734), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 144057;

-- NPC 144059: Jun'ha (Vendor)
-- Wowhead: Arathi Highlands (45), DB: Arathi Highlands (9734), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 144059;

-- NPC 144060: Keena (Vendor)
-- Wowhead: Arathi Highlands (45), DB: Arathi Highlands (9734), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 144060;

-- NPC 144068: Tunkk (Vendor)
-- Wowhead: Arathi Highlands (45), DB: Arathi Highlands (9734), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 144068;

-- NPC 144070: Uttnar (Vendor)
-- Wowhead: Arathi Highlands (45), DB: Arathi Highlands (9734), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 144070;

-- NPC 223597: Galley Chief Alunwea (Vendor)
-- Wowhead: Dragon Isles (13642), DB: Amirdrassil (15105), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 223597;

-- NPC 112881: Tanjin the Ironshaper (Repair)
-- Wowhead: Broken Shore (7543), DB: Acherus: The Ebon Hold (7679), 1 spawns
-- REVIEW: Spawns may be in wrong zone. Verify coordinates before fixing.
-- SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z FROM creature WHERE id = 112881;


-- ============================================================================
-- Summary:
--   Section 1 (zoneId/areaId fixes): 242 combos across 5 patterns
--   Section 2 (review items):        198 NPCs
-- ============================================================================
