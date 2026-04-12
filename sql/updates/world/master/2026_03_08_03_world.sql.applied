-- LoreWalker TDB Import — File 3: Quests
-- Source: lorewalker_world (build 66102) → Target: world (build 66263)
-- quest_template: 198 | quest_template_addon: 57,611 | quest_objectives: 888
-- quest_details: 962 | quest_offer_reward: 551 | quest_request_items: 1,894
-- quest_poi: 10,424 | quest_poi_points: 23,476 | quest_visual_effect: 545
-- creature_queststarter: 894 | creature_questender: 719
-- gameobject_queststarter: 256 | gameobject_questender: 264
-- creature_questitem: 232 | gameobject_questitem: 512
-- Total: 99,426 rows

SET autocommit=0;

-- quest_template (PK: ID | 105 cols | VB → 0)
DROP TEMPORARY TABLE IF EXISTS _imp;
CREATE TEMPORARY TABLE _imp AS
SELECT * FROM lorewalker_world.quest_template l
WHERE l.ID < 9100000
  AND NOT EXISTS (SELECT 1 FROM world.quest_template w WHERE w.ID = l.ID);
UPDATE _imp SET VerifiedBuild = 0;
INSERT IGNORE INTO world.quest_template SELECT * FROM _imp;
DROP TEMPORARY TABLE IF EXISTS _imp;

-- quest_template_addon (PK: ID | no VB | identical schema | ALL missing quests)
INSERT IGNORE INTO world.quest_template_addon
SELECT * FROM lorewalker_world.quest_template_addon
WHERE ID < 9100000;

-- quest_objectives (PK: ID | 15 cols | VB → 0)
DROP TEMPORARY TABLE IF EXISTS _imp;
CREATE TEMPORARY TABLE _imp AS
SELECT * FROM lorewalker_world.quest_objectives l
WHERE l.QuestID < 9100000
  AND NOT EXISTS (SELECT 1 FROM world.quest_objectives w WHERE w.ID = l.ID);
UPDATE _imp SET VerifiedBuild = 0;
INSERT IGNORE INTO world.quest_objectives SELECT * FROM _imp;
DROP TEMPORARY TABLE IF EXISTS _imp;

-- quest_details (PK: ID | 10 cols | VB → 0)
INSERT IGNORE INTO world.quest_details
  (ID, Emote1, Emote2, Emote3, Emote4,
   EmoteDelay1, EmoteDelay2, EmoteDelay3, EmoteDelay4, VerifiedBuild)
SELECT l.ID, l.Emote1, l.Emote2, l.Emote3, l.Emote4,
       l.EmoteDelay1, l.EmoteDelay2, l.EmoteDelay3, l.EmoteDelay4, 0
FROM lorewalker_world.quest_details l
WHERE l.ID < 9100000;

-- quest_offer_reward (PK: ID | 11 cols | VB → 0)
INSERT IGNORE INTO world.quest_offer_reward
  (ID, Emote1, Emote2, Emote3, Emote4,
   EmoteDelay1, EmoteDelay2, EmoteDelay3, EmoteDelay4, RewardText, VerifiedBuild)
SELECT l.ID, l.Emote1, l.Emote2, l.Emote3, l.Emote4,
       l.EmoteDelay1, l.EmoteDelay2, l.EmoteDelay3, l.EmoteDelay4, l.RewardText, 0
FROM lorewalker_world.quest_offer_reward l
WHERE l.ID < 9100000;

-- quest_request_items (PK: ID | 7 cols | VB → 0)
INSERT IGNORE INTO world.quest_request_items
  (ID, EmoteOnComplete, EmoteOnIncomplete,
   EmoteOnCompleteDelay, EmoteOnIncompleteDelay, CompletionText, VerifiedBuild)
SELECT l.ID, l.EmoteOnComplete, l.EmoteOnIncomplete,
       l.EmoteOnCompleteDelay, l.EmoteOnIncompleteDelay, l.CompletionText, 0
FROM lorewalker_world.quest_request_items l
WHERE l.ID < 9100000;

-- quest_poi (PK: QuestID,BlobIndex,Idx1 | 16 cols | VB → 0)
DROP TEMPORARY TABLE IF EXISTS _imp;
CREATE TEMPORARY TABLE _imp AS
SELECT * FROM lorewalker_world.quest_poi l
WHERE l.QuestID < 9100000
  AND NOT EXISTS (SELECT 1 FROM world.quest_poi w
                  WHERE w.QuestID = l.QuestID AND w.BlobIndex = l.BlobIndex AND w.Idx1 = l.Idx1);
UPDATE _imp SET VerifiedBuild = 0;
INSERT IGNORE INTO world.quest_poi SELECT * FROM _imp;
DROP TEMPORARY TABLE IF EXISTS _imp;

-- quest_poi_points (PK: QuestID,Idx1,Idx2 | 7 cols | VB → 0)
INSERT IGNORE INTO world.quest_poi_points
  (QuestID, Idx1, Idx2, X, Y, Z, VerifiedBuild)
SELECT l.QuestID, l.Idx1, l.Idx2, l.X, l.Y, l.Z, 0
FROM lorewalker_world.quest_poi_points l
WHERE l.QuestID < 9100000;

-- quest_visual_effect (PK: ID,`Index` | 4 cols | VB → 0)
INSERT IGNORE INTO world.quest_visual_effect
  (ID, `Index`, VisualEffect, VerifiedBuild)
SELECT l.ID, l.`Index`, l.VisualEffect, 0
FROM lorewalker_world.quest_visual_effect l;

-- creature_queststarter (PK: id,quest,VerifiedBuild — VB IS IN PK!)
-- Must dedup on business key (id,quest) only. DISTINCT avoids sending 339 duplicate pairs.
INSERT IGNORE INTO world.creature_queststarter (id, quest, VerifiedBuild)
SELECT DISTINCT l.id, l.quest, 0
FROM lorewalker_world.creature_queststarter l
WHERE NOT EXISTS (
  SELECT 1 FROM world.creature_queststarter w
  WHERE w.id = l.id AND w.quest = l.quest
);

-- creature_questender (PK: id,quest,VerifiedBuild — VB IN PK)
INSERT IGNORE INTO world.creature_questender (id, quest, VerifiedBuild)
SELECT DISTINCT l.id, l.quest, 0
FROM lorewalker_world.creature_questender l
WHERE NOT EXISTS (
  SELECT 1 FROM world.creature_questender w
  WHERE w.id = l.id AND w.quest = l.quest
);

-- gameobject_queststarter (PK: id,quest,VerifiedBuild — VB IN PK)
INSERT IGNORE INTO world.gameobject_queststarter (id, quest, VerifiedBuild)
SELECT DISTINCT l.id, l.quest, 0
FROM lorewalker_world.gameobject_queststarter l
WHERE NOT EXISTS (
  SELECT 1 FROM world.gameobject_queststarter w
  WHERE w.id = l.id AND w.quest = l.quest
);

-- gameobject_questender (PK: id,quest,VerifiedBuild — VB IN PK)
INSERT IGNORE INTO world.gameobject_questender (id, quest, VerifiedBuild)
SELECT DISTINCT l.id, l.quest, 0
FROM lorewalker_world.gameobject_questender l
WHERE NOT EXISTS (
  SELECT 1 FROM world.gameobject_questender w
  WHERE w.id = l.id AND w.quest = l.quest
);

-- creature_questitem (PK: CreatureEntry,DifficultyID,Idx | 5 cols | VB → 0)
INSERT IGNORE INTO world.creature_questitem
  (CreatureEntry, DifficultyID, Idx, ItemId, VerifiedBuild)
SELECT l.CreatureEntry, l.DifficultyID, l.Idx, l.ItemId, 0
FROM lorewalker_world.creature_questitem l
WHERE l.CreatureEntry < 9100000;

-- gameobject_questitem (PK: GameObjectEntry,Idx | 4 cols | VB → 0)
INSERT IGNORE INTO world.gameobject_questitem
  (GameObjectEntry, Idx, ItemId, VerifiedBuild)
SELECT l.GameObjectEntry, l.Idx, l.ItemId, 0
FROM lorewalker_world.gameobject_questitem l
WHERE l.GameObjectEntry < 9100000;

COMMIT;
