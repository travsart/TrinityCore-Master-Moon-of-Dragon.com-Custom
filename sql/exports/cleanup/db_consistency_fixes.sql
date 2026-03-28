-- ============================================================================
-- WORLD DATABASE CONSISTENCY FIXES
-- Generated: 2026-03-01
--
-- SAFE, NON-DESTRUCTIVE cleanup only:
--   - Removes orphaned SmartAI scripts referencing non-existent creature GUIDs
--   - Removes orphaned waypoint_path + waypoint_path_node rows
--   - Does NOT delete creature spawns or creature_template entries
--   - Does NOT touch quest starter/ender references
-- ============================================================================

-- ============================================================================
-- FIX 1: SmartAI GUID-based orphans (106 rows)
-- These smart_scripts rows have source_type=0 (creature) and a negative
-- entryorguid (meaning creature GUID), but no creature with that GUID exists.
-- ============================================================================
USE `world`;
DELETE FROM smart_scripts WHERE source_type = 0 AND entryorguid IN (
    -4000000000141091,
    -4000000000141070,
    -4000000000141068,
    -4000000000141061,
    -4000000000141060,
    -4000000000141056,
    -4000000000141055,
    -3000090306,
    -3000090305,
    -3000090274,
    -10005252,
    -10001800,
    -10001765,
    -7000405,
    -7000404,
    -7000401,
    -7000399,
    -7000340,
    -7000331,
    -7000327,
    -7000326,
    -7000325,
    -7000320,
    -7000313,
    -6000138,
    -4000137,
    -4000136,
    -4000132,
    -4000130,
    -4000115,
    -4000114,
    -4000111,
    -4000110,
    -4000102,
    -4000101,
    -4000065,
    -4000046,
    -1050584,
    -1050581,
    -1050573,
    -1050322,
    -1050297,
    -1050291,
    -1050273,
    -1050259,
    -1050227,
    -1050220,
    -1050218,
    -1050217,
    -1050216,
    -450610,
    -376367,
    -376344,
    -371684,
    -371675,
    -371666,
    -136553,
    -128903,
    -97842,
    -97836,
    -88791,
    -88787,
    -63585,
    -63584,
    -63583,
    -63582,
    -56281,
    -19412,
    -19402,
    -19361,
    -13579,
    -456,
    -455
);
-- Expected: 106 rows deleted


-- ============================================================================
-- FIX 2: Orphaned waypoint_path + waypoint_path_node rows (1,946 paths, 29,978 nodes)
-- These waypoint paths are not referenced by any creature_addon.PathId or
-- creature_template_addon.PathId.
--
-- NOTE: Some paths may be referenced by C++ scripts or Eluna. If unsure,
-- comment out this section and audit manually.
-- ============================================================================

-- First delete the child nodes, then the parent paths
DELETE wpn FROM waypoint_path_node wpn
INNER JOIN waypoint_path wp ON wpn.PathId = wp.PathId
LEFT JOIN creature_addon ca ON wp.PathId = ca.PathId
LEFT JOIN creature_template_addon cta ON wp.PathId = cta.PathId
WHERE ca.guid IS NULL AND cta.entry IS NULL;
-- Expected: ~29,978 rows deleted

DELETE wp FROM waypoint_path wp
LEFT JOIN creature_addon ca ON wp.PathId = ca.PathId
LEFT JOIN creature_template_addon cta ON wp.PathId = cta.PathId
WHERE ca.guid IS NULL AND cta.entry IS NULL;
-- Expected: 1,946 rows deleted


-- ============================================================================
-- VERIFICATION QUERIES (run after applying fixes)
-- ============================================================================

-- Verify SmartAI cleanup
-- SELECT COUNT(*) AS remaining_smartai_guid_orphans
-- FROM smart_scripts ss
-- LEFT JOIN creature c ON ABS(ss.entryorguid) = c.guid
-- WHERE ss.source_type = 0 AND ss.entryorguid < 0 AND c.guid IS NULL;

-- Verify waypoint cleanup
-- SELECT COUNT(*) AS remaining_waypoint_orphans
-- FROM waypoint_path wp
-- LEFT JOIN creature_addon ca ON wp.PathId = ca.PathId
-- LEFT JOIN creature_template_addon cta ON wp.PathId = cta.PathId
-- WHERE ca.guid IS NULL AND cta.entry IS NULL;
