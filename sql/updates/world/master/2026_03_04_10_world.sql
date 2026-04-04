-- 2026_03_04_10_world.sql
-- RoleplayCore -- Add missing creature_template_difficulty (DifficultyID=0) for 11 Stormwind NPCs
--
-- These entries are spawned in Stormwind but lack a CTD row, leaving them
-- stuck at level 1.  Three have Blizzard CreatureDifficulty DB2 data; the
-- remaining eight use the dominant Stormwind values (ContentTuningID 864 /
-- HealthScalingExpansion 11, or 1227/5 for the bunny trigger).
--
-- DB2-backed lore NPCs:
--   140877  Malfurion Stormrage     (ContentTuningID 288, CreatureDifficultyID 147872)
--   148798  Lady Jaina Proudmoore   (ContentTuningID 794, CreatureDifficultyID 162454)
--   162197  Anduin Wrynn            (ContentTuningID 781, CreatureDifficultyID 182331)
--
-- Lore NPCs without DB2 data (dominant Stormwind defaults):
--   126301  Anduin Wrynn (BfA version)
--   141999  Tyrande Whisperwind
--
-- Phase 6666 civilian NPCs:
--   108138  Rebecca Laughlin
--   108192  Milton Sheaf
--   108533  Lenny "Fingers" McCoy
--   188374  Millbert
--
-- Bunny / vehicle:
--   50253    Twilight Hammer Cult Site  (bunny, ContentTuningID 1227)
--   9100577  Shang Xi's Hot Air Balloon (custom import)

INSERT IGNORE INTO `creature_template_difficulty`
  (`Entry`, `DifficultyID`, `ContentTuningID`, `HealthScalingExpansion`, `CreatureDifficultyID`)
VALUES
  -- DB2-backed lore NPCs
  (140877, 0, 288,  8, 147872),  -- Malfurion Stormrage
  (148798, 0, 794,  8, 162454),  -- Lady Jaina Proudmoore
  (162197, 0, 781,  8, 182331),  -- Anduin Wrynn

  -- Lore NPCs (no DB2 data — dominant Stormwind defaults)
  (126301, 0, 864, 11,      0),  -- Anduin Wrynn (BfA)
  (141999, 0, 864, 11,      0),  -- Tyrande Whisperwind

  -- Phase 6666 civilians
  (108138, 0, 864, 11,      0),  -- Rebecca Laughlin
  (108192, 0, 864, 11,      0),  -- Milton Sheaf
  (108533, 0, 864, 11,      0),  -- Lenny "Fingers" McCoy
  (188374, 0, 864, 11,      0),  -- Millbert

  -- Bunny / vehicle
  ( 50253, 0, 1227, 5,      0),  -- Twilight Hammer Cult Site
  (9100577, 0, 864, 11,     0);  -- Shang Xi's Hot Air Balloon
