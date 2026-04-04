---
allowed-tools: Read, Bash(python3:*), Write
description: Validate a SmartAI SQL file against known enum values, deprecated types, and common mistakes
paths: sql/**/*.sql
---

## Context

The user wants to validate a SmartAI SQL file before applying it to the database.
Reference: SmartAI enums are in `src/server/game/AI/SmartScripts/SmartScriptMgr.h`

## Validation Rules

### 1. Deprecated Event Types (should be deleted)
IDs: 12, 14, 18, 30, 39, 66, 67

### 2. Deprecated Action Types (should be deleted)
IDs: 15, 18, 19, 26, 58, 61, 75, 76, 77, 93, 94, 95, 96, 104, 105, 106, 119, 120, 121, 122, 126

### 3. Out-of-Range Values
- Valid event types: 0–90 (SMART_EVENT_END = 91)
- Valid action types: 1–159 (SMART_ACTION_END = 160)
- Valid source_type: 0, 1, 2, 5, 9, 10, 12

### 4. Deprecated Event Flags
Bitmask 0x1E (bits 0x02|0x04|0x08|0x10) = old difficulty flags. Any `event_flags` with these bits set is wrong.

### 5. Boolean Fields > 1
`TC_SAI_IS_BOOLEAN_VALID` rejects values > 1:
- action_type=80 (CALL_TIMED_ACTIONLIST), action_param3 must be 0 or 1
- action_type=1 (TALK), action_param2 must be 0 or 1
- action_type=53 (WP_START), action_param1 must be 0 or 1

### 6. Missing NOT_REPEATABLE Flag
Events 13, 15, 23, 24: if repeat min AND max are both 0 and event_flags bit 0x01 is NOT set, server will error. (Exclude source_type=9)

### 7. Link Chain Integrity
- If a row has `link != 0`, there should be another row with `id == link` and `event_type == 61`
- Orphaned event_type=61 rows (where no row's `link` points to them) are suspicious

### 8. Spell ID Validation (optional)
- Actions 11 (CAST), 85 (SELF_CAST), 86 (CROSS_CAST), 134 (INVOKER_CAST): action_param1 should be a valid spell ID
- Events 8 (SPELLHIT), 22 (SPELLHIT_TARGET), 83-85 (ON_SPELL_*), 89-90 (ON_AURA_*): event_param1 should be a valid spell ID
- For spell validation, first: `import sys, os; sys.path.insert(0, os.path.expanduser('~/VoxCore/wago')); from wago_common import WAGO_CSV_DIR`
- Then cross-reference against: `str(WAGO_CSV_DIR / 'SpellName-enUS.csv')`

## Arguments

$ARGUMENTS should be a path to a SQL file containing `INSERT` or `DELETE`/`INSERT` statements for the `smart_scripts` table.

## Your task

1. Read the SQL file using the Read tool
2. Write a Python script that:
   - Parses INSERT INTO `smart_scripts` VALUES lines, extracting the column values
   - Expected column order: `(entryorguid, source_type, id, link, event_type, event_phase_mask, event_chance, event_flags, event_param1, event_param2, event_param3, event_param4, event_param5, action_type, action_param1, action_param2, action_param3, action_param4, action_param5, action_param6, action_param7, target_type, target_param1, target_param2, target_param3, target_param4, target_x, target_y, target_z, target_o, comment)`
   - Runs all validation checks above
   - For spell validation: loads the SpellName CSV IDs into a set and checks referenced spell IDs
   - Reports issues grouped by category with line references
3. Run the script
4. If no issues found, say "All checks passed"
5. If issues found, list them clearly with suggested fixes
