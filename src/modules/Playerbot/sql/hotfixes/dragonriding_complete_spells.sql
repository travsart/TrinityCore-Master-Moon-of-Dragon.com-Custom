-- ============================================================================
-- Playerbot Dragonriding System - Action Bar Override for Soar
--
-- Uses REAL retail WoW 11.2 spell IDs for the dragonriding action bar.
-- The client already knows these spells - we just need to create the
-- override_spell_data entry to define which spells appear on the bar.
--
-- Real retail spell IDs (from wowhead.com):
--   372608 = Surge Forward
--   372610 = Skyward Ascent
--   361584 = Whirling Surge
--   403092 = Aerial Halt
--
-- Install into: hotfixes database
-- Restart worldserver after installation
-- ============================================================================

-- Use current TrinityCore 11.2 build number
SET @BUILD := 58941;
SET @HOTFIX_ID := 1000001;

-- ============================================================================
-- OVERRIDE_SPELL_DATA
-- Action bar override definition - swaps player action bar when activated
-- Our C++ code calls SetOverrideSpellsId(900001) to trigger this
-- TableHash: 3396722460 (OverrideSpellData.db2)
-- ============================================================================

DELETE FROM `override_spell_data` WHERE `ID` = 900001;
INSERT INTO `override_spell_data` (
    `ID`,
    `Spells1`, `Spells2`, `Spells3`, `Spells4`, `Spells5`,
    `Spells6`, `Spells7`, `Spells8`, `Spells9`, `Spells10`,
    `PlayerActionBarFileDataID`,
    `Flags`,
    `VerifiedBuild`
) VALUES (
    900001,
    372608,  -- Surge Forward (retail spell ID)
    372610,  -- Skyward Ascent (retail spell ID)
    361584,  -- Whirling Surge (retail spell ID)
    403092,  -- Aerial Halt (retail spell ID)
    0, 0, 0, 0, 0, 0,
    0,       -- Default action bar visual
    0,       -- No special flags
    @BUILD
);

-- ============================================================================
-- HOTFIX_DATA
-- Critical entry that tells TrinityCore to send override_spell_data to clients
-- TableHash: 3396722460 (OverrideSpellData.db2)
-- ============================================================================

DELETE FROM `hotfix_data` WHERE `TableHash` = 3396722460 AND `RecordId` = 900001;
INSERT INTO `hotfix_data` (`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`) VALUES
(@HOTFIX_ID, @HOTFIX_ID, 3396722460, 900001, 1, @BUILD);

-- ============================================================================
-- TECHNICAL NOTES
--
-- How this works:
-- 1. Player casts Soar (369536)
-- 2. PlayerbotDragonridingScript.cpp HandleOnCast() calls SetOverrideSpellsId(900001)
-- 3. Client receives PLAYER_FIELD_OVERRIDE_SPELLS_ID = 900001
-- 4. Client looks up OverrideSpellData ID 900001 from hotfix data
-- 5. Client shows the real retail dragonriding spells on action bar:
--    - Surge Forward (372608)
--    - Skyward Ascent (372610)
--    - Whirling Surge (361584)
--    - Aerial Halt (403092)
-- 6. When Soar ends, SetOverrideSpellsId(0) returns action bar to normal
--
-- The spells are REAL retail spells - client already has all data for them!
-- ============================================================================
