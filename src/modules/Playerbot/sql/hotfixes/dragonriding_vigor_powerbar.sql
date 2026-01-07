-- ============================================================================
-- PLAYERBOT DRAGONRIDING: VIGOR POWER BAR CREATION
-- ============================================================================
-- INSTALLATION:
--   1. Connect to your hotfixes database (e.g., hotfixes_11)
--   2. Run this SQL script
--   3. Restart worldserver
--
-- PURPOSE:
--   Create UnitPowerBar 631 ("Vigor") for dragonriding vigor display.
--   This entry does NOT exist in the base DB2 data and must be created.
--
-- The Dragonrider Energy aura (372773) uses SPELL_AURA_ENABLE_ALT_POWER with
-- MiscValue=631 which references this UnitPowerBar entry to display the UI.
--
-- NOTE: MaxPower=3 is the base value. Server-side progression (DragonridingMgr)
-- increases max vigor dynamically based on Dragon Glyphs/talents (up to 6).
-- ============================================================================

-- ============================================================================
-- STEP 1: CREATE UNIT_POWER_BAR ENTRY
-- ============================================================================
-- Insert the Vigor power bar entry (ID 631)
-- This creates the visual power bar for dragonriding

DELETE FROM unit_power_bar WHERE ID = 631;

INSERT INTO unit_power_bar (
    ID,
    Name,
    Cost,
    OutOfError,
    ToolTip,
    MinPower,
    MaxPower,
    StartPower,
    CenterPower,
    RegenerationPeace,
    RegenerationCombat,
    BarType,
    Flags,
    StartInset,
    EndInset,
    FileDataID1,
    FileDataID2,
    FileDataID3,
    FileDataID4,
    FileDataID5,
    FileDataID6,
    Color1,
    Color2,
    Color3,
    Color4,
    Color5,
    Color6,
    VerifiedBuild
) VALUES (
    631,                     -- ID: Vigor power bar
    'Vigor',                 -- Name: Display name
    '',                      -- Cost: Empty (not used for cost display)
    '',                      -- OutOfError: Error message when depleted
    '',                      -- ToolTip: Hover text
    0,                       -- MinPower: Minimum value
    3,                       -- MaxPower: Base maximum (server overrides based on progression)
    3,                       -- StartPower: Start with full vigor
    0,                       -- CenterPower: Not centered
    0.0,                     -- RegenerationPeace: Handled by server
    0.0,                     -- RegenerationCombat: Handled by server
    6,                       -- BarType: 6 = Alt power bar type for dragonriding
    0,                       -- Flags: No special flags
    0.0,                     -- StartInset: No inset
    0.0,                     -- EndInset: No inset
    0,                       -- FileDataID1: Use default textures
    0,                       -- FileDataID2: Use default textures
    0,                       -- FileDataID3: Use default textures
    0,                       -- FileDataID4: Use default textures
    0,                       -- FileDataID5: Use default textures
    0,                       -- FileDataID6: Use default textures
    16777215,                -- Color1: White (0xFFFFFF) - full bar color
    8421504,                 -- Color2: Gray (0x808080) - empty bar color
    0,                       -- Color3: Black - background
    0,                       -- Color4: Not used
    0,                       -- Color5: Not used
    0,                       -- Color6: Not used
    64978                    -- VerifiedBuild: WoW 11.2 build
);

-- ============================================================================
-- STEP 2: HOTFIX REGISTRATION (AUTOMATIC)
-- ============================================================================
-- NOTE: Hotfix registration is now handled AUTOMATICALLY by the server!
--
-- The PlayerbotDragonridingScript.cpp dynamically registers the hotfix using
-- sDB2Manager.InsertNewHotfix(sUnitPowerBarStore.GetTableHash(), 631)
-- This uses the correct table hash from the loaded DB2 store, avoiding
-- hardcoded hash mismatches.
--
-- You do NOT need to manually insert into hotfix_data anymore.
-- Just run this SQL to create the unit_power_bar entry, and the server
-- will automatically register the hotfix at startup.
-- ============================================================================

-- Clean up any old manual hotfix_data entries (no longer needed)
DELETE FROM hotfix_data WHERE RecordId = 631 AND Id = 900631;

-- ============================================================================
-- VERIFICATION QUERIES
-- ============================================================================

-- Check unit_power_bar entry was created:
SELECT ID, Name, MinPower, MaxPower, StartPower, BarType, Flags
FROM unit_power_bar
WHERE ID = 631;

-- Check hotfix_data entry (should have Status=1):
SELECT * FROM hotfix_data WHERE RecordId = 631;

-- ============================================================================
-- TROUBLESHOOTING
-- ============================================================================
-- If the vigor UI still doesn't display:
--
-- 1. Ensure you ran this SQL in your HOTFIXES database (e.g., hotfixes_11)
--
-- 2. Restart worldserver after running this SQL
--
-- 3. Check server log for successful hotfix registration:
--    Look for: ">>> PLAYERBOT DRAGONRIDING: Registered hotfix for UnitPowerBar 631"
--
-- 4. Verify the Dragonrider Energy aura (372773) is applied when Soar activates:
--    Check server log for: "Dragonrider Energy aura CONFIRMED APPLIED"
--
-- 5. Check if POWER_ALTERNATE_POWER has correct values:
--    Look for: "POWER_ALTERNATE_POWER (10): X/X" where X is 3-6 based on progression
--
-- 6. If BarType 6 doesn't work, try these alternatives:
--    - BarType 0: Default power bar
--    - BarType 2: Alternative style
--    - BarType 4: Widget style
-- ============================================================================
