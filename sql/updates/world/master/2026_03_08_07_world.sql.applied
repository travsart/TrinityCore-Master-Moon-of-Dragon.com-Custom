-- LoreWalker TDB Import — File 7: ContentTuningID Backfill
-- Source: lorewalker_world (build 66102) → Target: world (build 66263)
-- Updates creature_template_difficulty.ContentTuningID where ours = 0 and LW has a value
-- Rows affected: 7,678

UPDATE world.creature_template_difficulty w
JOIN lorewalker_world.creature_template_difficulty l
  ON w.Entry = l.Entry AND w.DifficultyID = l.DifficultyID
SET w.ContentTuningID = l.ContentTuningID
WHERE w.ContentTuningID = 0 AND l.ContentTuningID != 0;
