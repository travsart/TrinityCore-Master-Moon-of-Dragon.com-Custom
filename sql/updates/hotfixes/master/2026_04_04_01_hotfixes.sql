-- Phase 4: Remove hotfix_blob entries for tables that have dedicated SQL tables
-- Session 228: These blob entries are IGNORED by the server when dedicated tables exist
-- Eliminates ~744K "points to a loaded DB2 store" warnings at startup
-- 18,612 blob entries across 11 table hashes:
--   SoundKitEntry (10,582), Spell (4,719), SoundKitAdvanced (2,244),
--   ScreenEffect (363), LightParams (286), QuestPOIPoint (176),
--   ModelFileData (110), SpecializationSpellsDisplay (44),
--   SoundAmbience (44), ZoneMusic (22), VehiclePOIType (22)

DELETE FROM `hotfix_blob` WHERE `TableHash` IN (
    3266400455,  -- SoundKitEntry (0xC2B150C7)
    3776013982,  -- Spell (0xE111669E)
    3232102598,  -- SoundKitAdvanced (0xC0A5F8C6)
    3851993221,  -- ScreenEffect (0xE598C085)
    2294684390,  -- LightParams (0x88C61AE6)
    3205218938,  -- QuestPOIPoint (0xBF0BC27A)
    1992314007,  -- ModelFileData (0x76C04C97)
    2202435563,  -- SpecializationSpellsDisplay (0x83467FEB)
    3656496423,  -- SoundAmbience (0xD9F1B527)
    13326836,    -- ZoneMusic (0xCB59F4)
    2557447376   -- VehiclePOIType (0x986F8CD0)
);

-- Also clean orphan hotfix_data entries for the same table hashes (1,812 rows)
-- These point to records that no longer exist in either blob or dedicated tables
DELETE FROM `hotfix_data` WHERE `TableHash` IN (
    3266400455,  -- SoundKitEntry
    3776013982,  -- Spell
    3232102598,  -- SoundKitAdvanced
    3851993221,  -- ScreenEffect
    2294684390,  -- LightParams
    3205218938,  -- QuestPOIPoint
    1992314007,  -- ModelFileData
    2202435563,  -- SpecializationSpellsDisplay
    3656496423,  -- SoundAmbience
    13326836,    -- ZoneMusic
    2557447376   -- VehiclePOIType
);
