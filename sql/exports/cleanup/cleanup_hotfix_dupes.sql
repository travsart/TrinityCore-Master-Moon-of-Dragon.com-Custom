-- Hotfixes DB cleanup: Remove duplicate hotfix_data entries
-- For each (TableHash, RecordId) with multiple rows, keep only the highest UniqueId
-- Generated 2026-02-27
SET innodb_lock_wait_timeout=120;

-- =============================================================================
-- Step 7: Hotfix duplicate cleanup
-- =============================================================================
USE `hotfixes`;

DELETE hd FROM hotfix_data hd
INNER JOIN (
    SELECT TableHash, RecordId, MAX(UniqueId) AS maxUid
    FROM hotfix_data
    GROUP BY TableHash, RecordId
    HAVING COUNT(*) > 1
) dup ON hd.TableHash = dup.TableHash AND hd.RecordId = dup.RecordId AND hd.UniqueId < dup.maxUid;
