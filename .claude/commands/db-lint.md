---
allowed-tools: Bash(mysql*), Bash(powershell*), Read, Write, Grep, Glob, Agent
description: Scan databases for common data quality issues and generate fix SQL
paths: sql/**/*.sql
---

# DB Lint

Scan VoxCore databases for common data quality issues — orphan references, invalid IDs, zero-value fields that shouldn't be zero, and schema inconsistencies. Generates fix SQL.

## Arguments

$ARGUMENTS — optional: specific database (`world`, `characters`, `auth`, `hotfixes`, `roleplay`) or `all` (default)

## Process

### Step 1: Select scope

Parse $ARGUMENTS to determine which databases to scan. Default to `all`.

### Step 2: Run lint checks

For each database in scope, run the applicable checks below. Use `mysql -u root -padmin -N <db>` for queries.

#### World DB Checks
```sql
-- Orphan creature_template entries (no spawn)
SELECT ct.entry, ct.name FROM creature_template ct
LEFT JOIN creature c ON c.id1 = ct.entry
WHERE c.guid IS NULL AND ct.entry BETWEEN 400000 AND 499999;

-- creature_template with faction=0 (causes client errors)
SELECT entry, name FROM creature_template WHERE faction = 0 AND entry > 0 LIMIT 50;

-- creature_template with zero health/mana multipliers
SELECT entry, name FROM creature_template WHERE HealthModifier = 0 OR DamageModifier = 0 LIMIT 50;

-- Orphan creature_template_outfits (entry not in creature_template)
SELECT o.entry FROM creature_template_outfits o
LEFT JOIN creature_template ct ON ct.entry = o.entry
WHERE ct.entry IS NULL;

-- companion_roster entries not in creature_template
SELECT r.entry, r.name FROM companion_roster r
LEFT JOIN creature_template ct ON ct.entry = r.entry
WHERE ct.entry IS NULL;

-- creature_template_spell with invalid spell IDs (not in spell_name)
SELECT cts.CreatureID, cts.Spell FROM creature_template_spell cts
LEFT JOIN hotfixes.spell_name sn ON sn.ID = cts.Spell
WHERE sn.ID IS NULL LIMIT 50;

-- SmartAI scripts referencing non-existent creatures
SELECT entryorguid, source_type, event_type, action_type
FROM smart_scripts WHERE source_type = 0
AND entryorguid NOT IN (SELECT entry FROM creature_template) LIMIT 50;
```

#### Characters DB Checks
```sql
-- character_companion_squad referencing non-existent roster entries
SELECT cs.guid, cs.roster_entry FROM character_companion_squad cs
LEFT JOIN world.companion_roster r ON r.entry = cs.roster_entry
WHERE r.entry IS NULL;

-- Orphan character_transmog_outfits (character deleted)
SELECT DISTINCT o.guid FROM character_transmog_outfits o
LEFT JOIN characters c ON c.guid = o.guid
WHERE c.guid IS NULL LIMIT 50;
```

#### Roleplay DB Checks
```sql
-- creature_extra referencing non-existent creature spawns
SELECT ce.guid FROM roleplay.creature_extra ce
LEFT JOIN world.creature c ON c.guid = ce.guid
WHERE c.guid IS NULL LIMIT 50;

-- custom_npcs referencing non-existent creature_template entries
SELECT cn.`Key`, cn.Entry FROM roleplay.custom_npcs cn
LEFT JOIN world.creature_template ct ON ct.entry = cn.Entry
WHERE ct.entry IS NULL;
```

#### Hotfixes DB Checks
```sql
-- hotfix_data entries with no matching table row
-- (Sample check for spell_name)
SELECT hd.Id FROM hotfixes.hotfix_data hd
WHERE hd.TableHash = 0 AND hd.Status = 2 LIMIT 10;

-- Duplicate hotfix_data entries (same RecordId + TableHash)
SELECT TableHash, RecordId, COUNT(*) as cnt
FROM hotfixes.hotfix_data
GROUP BY TableHash, RecordId
HAVING cnt > 1 LIMIT 50;
```

### Step 3: Classify findings

For each issue found, classify:
- **ERROR** — Will cause crashes, client disconnects, or data corruption
- **WARNING** — Gameplay bugs, visual glitches, or wasted DB rows
- **INFO** — Cosmetic issues, dead data, optimization opportunities

### Step 4: Generate fix SQL

For each ERROR and WARNING, generate a fix SQL statement. Write to `sql/updates/pending/db_lint_[date].sql`:

```sql
-- ============================================================================
-- DB Lint Fixes — Generated [date]
-- ============================================================================

-- [Issue description]
-- Severity: [ERROR/WARNING]
[FIX SQL]
```

For INFO items, just report them — don't generate fixes.

### Step 5: Report

```
## DB Lint Report — [date]

### Summary
| Database | Errors | Warnings | Info |
|----------|--------|----------|------|
| world    | N      | N        | N    |
| characters | N   | N        | N    |
| roleplay | N     | N        | N    |
| hotfixes | N     | N        | N    |

### Errors (fix immediately)
1. [description] — [count] rows affected
   Fix: `[SQL preview]`

### Warnings (fix when convenient)
1. [description] — [count] rows affected

### Info (cosmetic)
1. [description]

### Generated Fix File
`sql/updates/pending/db_lint_[date].sql` — [N] fixes, review before applying
```

## Rules
- NEVER auto-apply fixes — only generate the SQL file
- Always use LEFT JOIN patterns to find orphans (never NOT IN with large sets)
- Limit all queries to 50 rows to avoid flooding output
- Skip checks for databases not in scope
- If MySQL is not running, report it and exit gracefully
- Custom creature range is 400000-499999, companion range is 500001-500005
