-- DB Error Cleanup Phase 1: Hotfix blob/data orphans
-- Eliminates ~726K+ startup errors from DBErrors.log
-- Categories: known-store blobs that should be in specific tables, unknown hash references

-- ============================================================================
-- 1. hotfix_blob — rows for DB2 stores the server loads natively (181,468 rows)
--    Error: "Table hash 0xXXX points to a loaded DB2 store X.db2, fill related table instead of hotfix_blob"
--    These are data that should be in the specific hotfix tables (e.g. spell_name, etc.)
--    but are stuck in the generic blob table. The server can't use them from blob.
-- ============================================================================
DELETE FROM hotfix_blob WHERE TableHash IN (
  0xC2B150C7, -- SoundKitEntry.db2         (146,753 rows)
  0xC0A5F8C6, -- SoundKitAdvanced.db2      (26,229 rows)
  0xE111669E, -- Spell.db2                 (4,513 rows)
  0x83467FEB, -- QuestPOIPoint.db2         (2,268 rows)
  0xBF0BC27A, -- GlobalStrings.db2         (858 rows)
  0xC9FF6C36, -- ModifiedCraftingSpellSlot (550 rows)
  0x971037D6, -- CraftingData.db2          (67 rows)
  0xE598C085, -- ModifiedCraftingReagentItem (66 rows)
  0x88C61AE6, -- CreatureDisplayInfoOption  (35 rows)
  0x4F9D237F, -- CraftingReagentQuality     (22 rows)
  0xA161E42C, -- ModelFileData.db2          (22 rows)
  0xB33F3964, -- ScreenEffect.db2           (22 rows)
  0x986F8CD0, -- ItemDisplayInfo.db2        (17 rows)
  0x76C04C97, -- NPCModelItemSlotDisplayInfo (13 rows)
  0x1C6C1D1E, -- SpecializationSpellsDisplay (11 rows)
  0x495495DF, -- NPCSounds.db2              (11 rows)
  0xF24979A5  -- ModifiedCraftingCategory    (11 rows)
);

-- ============================================================================
-- 2. hotfix_data — references to unknown DB2 store hash (41 rows)
--    Error: "Table `hotfix_data` references unknown DB2 store by hash 0xAFC190D1"
-- ============================================================================
DELETE FROM hotfix_data WHERE TableHash = 0xAFC190D1;

-- ============================================================================
-- 3. hotfix_data — references to the known-store hashes we just cleaned from blob
--    These hotfix_data entries point to blob rows we just deleted, so they're now
--    dangling references. Clean them up too.
-- ============================================================================
DELETE FROM hotfix_data WHERE TableHash IN (
  0xC2B150C7,
  0xC0A5F8C6,
  0xE111669E,
  0x83467FEB,
  0xBF0BC27A,
  0xC9FF6C36,
  0x971037D6,
  0xE598C085,
  0x88C61AE6,
  0x4F9D237F,
  0xA161E42C,
  0xB33F3964,
  0x986F8CD0,
  0x76C04C97,
  0x1C6C1D1E,
  0x495495DF,
  0xF24979A5
);
