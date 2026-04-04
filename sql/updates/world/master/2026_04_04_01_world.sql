-- DB Error Cleanup Phase 2a: SmartAI links, quest orphans, game_event orphans
-- These fixes don't require cross-database joins
-- APPLIED: 2026-04-04

-- ============================================================================
-- 1. SmartAI broken link chains (163,824 rows deleted)
--    Error: "Entry X SourceType 0, Event Y, Link Event Z not found or invalid."
--    These events link to targets that don't have event_type=61 (SMART_EVENT_LINK).
--    Link is part of PK so can't UPDATE — DELETE the broken-link events instead.
-- ============================================================================
DELETE ss FROM smart_scripts ss
WHERE ss.link > 0
  AND NOT EXISTS (
    SELECT 1 FROM (
      SELECT entryorguid, source_type, id
      FROM smart_scripts
      WHERE event_type = 61
    ) ss2
    WHERE ss2.entryorguid = ss.entryorguid
      AND ss2.source_type = ss.source_type
      AND ss2.id = ss.link
  );

-- ============================================================================
-- 2. quest_poi orphans (6,220 rows deleted)
--    Error: "`quest_poi` quest id (X) Idx1 (Y) does not exist in `quest_template`"
-- ============================================================================
DELETE qp FROM quest_poi qp
LEFT JOIN quest_template qt ON qp.QuestID = qt.ID
WHERE qt.ID IS NULL;

-- ============================================================================
-- 3. quest_poi_points orphans (14,711 rows deleted)
--    Cascade cleanup for quest_poi deletions above.
-- ============================================================================
DELETE qpp FROM quest_poi_points qpp
LEFT JOIN quest_poi qp ON qpp.QuestID = qp.QuestID AND qpp.Idx1 = qp.Idx1
WHERE qp.QuestID IS NULL;

-- ============================================================================
-- 4. game_event_creature orphans (1,123 rows deleted)
--    Error: "`game_event_creature` contains creature (GUID: X) not found in `creature` table."
-- ============================================================================
DELETE gec FROM game_event_creature gec
LEFT JOIN creature c ON gec.guid = c.guid
WHERE c.guid IS NULL;

-- ============================================================================
-- 5. game_event_gameobject orphans (2,361 rows deleted)
--    Error: "`game_event_gameobject` contains gameobject (GUID: X) not found in `gameobject` table."
-- ============================================================================
DELETE geg FROM game_event_gameobject geg
LEFT JOIN gameobject g ON geg.guid = g.guid
WHERE g.guid IS NULL;
