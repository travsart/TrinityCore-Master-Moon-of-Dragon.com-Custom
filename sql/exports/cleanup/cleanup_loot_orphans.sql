-- =============================================================================
-- Cleanup Orphaned Loot Table Entries
-- Generated: 2026-02-27
--
-- Removes loot entries that are not referenced by any template/difficulty row.
-- Order matters: leaf tables first, then reference_loot_template last
-- (since deleting leaf orphans may create additional reference orphans).
--
-- Expected deletions:
--   gameobject_loot_template:    ~116,367 rows (5,658 entries) — 48.7% dead
--   creature_loot_template:     ~268,636 rows (4,206 entries)
--   skinning_loot_template:         ~402 rows (121 entries)
--   pickpocketing_loot_template:  ~1,389 rows (332 entries)
--   reference_loot_template:      ~1,122 rows (131+ entries)
-- =============================================================================
USE `world`;

SET innodb_lock_wait_timeout = 120;

-- =============================================================================
-- 1. gameobject_loot_template — remove entries not referenced by any
--    gameobject_template Data1 (types 3,25,50), Data30 (type 3), Data33 (type 3)
-- =============================================================================

SELECT @go_before := COUNT(*) FROM gameobject_loot_template;
SELECT CONCAT('gameobject_loot_template BEFORE: ', @go_before, ' rows') AS status;

DELETE glt FROM gameobject_loot_template glt
LEFT JOIN (
    SELECT DISTINCT Data1 AS LootID FROM gameobject_template WHERE type IN (3,25,50) AND Data1 > 0
    UNION SELECT DISTINCT Data30 FROM gameobject_template WHERE type = 3 AND Data30 > 0
    UNION SELECT DISTINCT Data33 FROM gameobject_template WHERE type = 3 AND Data33 > 0
) valid ON glt.Entry = valid.LootID
WHERE valid.LootID IS NULL;

SELECT @go_after := COUNT(*) FROM gameobject_loot_template;
SELECT CONCAT('gameobject_loot_template AFTER:  ', @go_after, ' rows (deleted ', @go_before - @go_after, ')') AS status;

-- =============================================================================
-- 2. creature_loot_template — remove entries not referenced by any
--    creature_template_difficulty.LootID
-- =============================================================================

SELECT @cl_before := COUNT(*) FROM creature_loot_template;
SELECT CONCAT('creature_loot_template BEFORE: ', @cl_before, ' rows') AS status;

DELETE FROM creature_loot_template
WHERE Entry NOT IN (
    SELECT LootID FROM (
        SELECT DISTINCT LootID FROM creature_template_difficulty WHERE LootID > 0
    ) AS sub
);

SELECT @cl_after := COUNT(*) FROM creature_loot_template;
SELECT CONCAT('creature_loot_template AFTER:  ', @cl_after, ' rows (deleted ', @cl_before - @cl_after, ')') AS status;

-- =============================================================================
-- 3. skinning_loot_template — remove entries not referenced by any
--    creature_template_difficulty.SkinLootID
-- =============================================================================

SELECT @sk_before := COUNT(*) FROM skinning_loot_template;
SELECT CONCAT('skinning_loot_template BEFORE: ', @sk_before, ' rows') AS status;

DELETE FROM skinning_loot_template
WHERE Entry NOT IN (
    SELECT SkinLootID FROM (
        SELECT DISTINCT SkinLootID FROM creature_template_difficulty WHERE SkinLootID > 0
    ) AS sub
);

SELECT @sk_after := COUNT(*) FROM skinning_loot_template;
SELECT CONCAT('skinning_loot_template AFTER:  ', @sk_after, ' rows (deleted ', @sk_before - @sk_after, ')') AS status;

-- =============================================================================
-- 4. pickpocketing_loot_template — remove entries not referenced by any
--    creature_template_difficulty.PickPocketLootID
-- =============================================================================

SELECT @pp_before := COUNT(*) FROM pickpocketing_loot_template;
SELECT CONCAT('pickpocketing_loot_template BEFORE: ', @pp_before, ' rows') AS status;

DELETE FROM pickpocketing_loot_template
WHERE Entry NOT IN (
    SELECT PickPocketLootID FROM (
        SELECT DISTINCT PickPocketLootID FROM creature_template_difficulty WHERE PickPocketLootID > 0
    ) AS sub
);

SELECT @pp_after := COUNT(*) FROM pickpocketing_loot_template;
SELECT CONCAT('pickpocketing_loot_template AFTER:  ', @pp_after, ' rows (deleted ', @pp_before - @pp_after, ')') AS status;

-- =============================================================================
-- 5. reference_loot_template — remove entries not referenced by any other
--    loot table (must run AFTER leaf table cleanups above)
--
--    References come from:
--      creature_loot_template.Reference (where Reference > 0)
--      creature_loot_template.Item (where ItemType = 1)
--      gameobject_loot_template.Item (where ItemType = 1)
--      skinning_loot_template.Item (where ItemType = 1)
--      pickpocketing_loot_template.Item (where ItemType = 1)
--      reference_loot_template.Item (self-ref, where ItemType = 1)
-- =============================================================================

SELECT @rl_before := COUNT(*) FROM reference_loot_template;
SELECT CONCAT('reference_loot_template BEFORE: ', @rl_before, ' rows') AS status;

DELETE FROM reference_loot_template
WHERE Entry NOT IN (
    SELECT ref_id FROM (
        SELECT DISTINCT Reference AS ref_id FROM creature_loot_template WHERE Reference > 0
        UNION SELECT DISTINCT Item FROM creature_loot_template WHERE ItemType = 1
        UNION SELECT DISTINCT Item FROM gameobject_loot_template WHERE ItemType = 1
        UNION SELECT DISTINCT Item FROM skinning_loot_template WHERE ItemType = 1
        UNION SELECT DISTINCT Item FROM pickpocketing_loot_template WHERE ItemType = 1
        UNION SELECT DISTINCT Item FROM reference_loot_template WHERE ItemType = 1
    ) AS sub
);

SELECT @rl_after := COUNT(*) FROM reference_loot_template;
SELECT CONCAT('reference_loot_template AFTER:  ', @rl_after, ' rows (deleted ', @rl_before - @rl_after, ')') AS status;

-- =============================================================================
-- Summary
-- =============================================================================
SELECT '=== CLEANUP COMPLETE ===' AS status;
SELECT CONCAT('Total rows deleted: ',
    (@go_before - @go_after) + (@cl_before - @cl_after) + (@sk_before - @sk_after) +
    (@pp_before - @pp_after) + (@rl_before - @rl_after)
) AS status;
