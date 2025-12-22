-- ============================================================================
-- Playerbot Dragonriding System - Hotfix Data
--
-- This SQL adds a custom OverrideSpellData entry to enable action bar swap
-- when dragonriding (Soar) is activated. This is how retail WoW handles
-- showing dragonriding abilities on the action bar.
--
-- Install into: hotfixes database
-- ============================================================================

-- ============================================================================
-- OVERRIDE_SPELL_DATA - Action Bar Override for Dragonriding
--
-- ID: 900001 (custom range to avoid conflicts with retail data)
-- Contains the dragonriding boost abilities that appear on action bar:
--   - Surge Forward (900002)
--   - Skyward Ascent (900003)
--   - Whirling Surge (900004)
--   - Aerial Halt (900005)
-- ============================================================================

-- Delete any existing entry with our custom ID (for clean reinstall)
DELETE FROM `override_spell_data` WHERE `ID` = 900001;

-- Insert the dragonriding action bar override
-- Spells1-4 contain our dragonriding abilities
-- Spells5-10 are empty (0)
-- PlayerActionBarFileDataID = 0 (use default action bar visual)
-- Flags = 0 (no special flags)
INSERT INTO `override_spell_data` (
    `ID`,
    `Spells1`, `Spells2`, `Spells3`, `Spells4`, `Spells5`,
    `Spells6`, `Spells7`, `Spells8`, `Spells9`, `Spells10`,
    `PlayerActionBarFileDataID`,
    `Flags`,
    `VerifiedBuild`
) VALUES (
    900001,
    900002,  -- Surge Forward
    900003,  -- Skyward Ascent
    900004,  -- Whirling Surge (requires talent)
    900005,  -- Aerial Halt (requires talent)
    0,       -- Empty slot
    0,       -- Empty slot
    0,       -- Empty slot
    0,       -- Empty slot
    0,       -- Empty slot
    0,       -- Empty slot
    0,       -- Default action bar visual
    0,       -- No special flags
    1        -- VerifiedBuild > 0 means this entry is loaded
);

-- ============================================================================
-- NOTES:
--
-- The OverrideSpellData system works as follows:
-- 1. When an aura with SPELL_AURA_OVERRIDE_SPELLS (293) is applied,
--    the MiscValue points to the OverrideSpellData ID
-- 2. TrinityCore calls SetOverrideSpellsId() and AddTemporarySpell()
-- 3. The client shows the spells from this entry on the action bar
-- 4. When the aura is removed, the action bar returns to normal
--
-- Since our Soar spell uses custom spell scripts instead of DB2 auras,
-- we manually call SetOverrideSpellsId() and AddTemporarySpell() in
-- PlayerbotDragonridingScript.cpp when Soar is cast/removed.
-- ============================================================================
