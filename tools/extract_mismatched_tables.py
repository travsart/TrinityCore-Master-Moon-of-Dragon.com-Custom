"""Extract INSERT data for mismatched tables from TC TDB and rewrite with explicit column names.

For tables where LoreWalker added extra columns, TC's INSERT INTO `table` VALUES(...)
fails because column counts don't match. This script extracts the INSERT data and
rewrites it as INSERT IGNORE INTO `table` (col1, col2, ...) VALUES(...) so extra
columns get default values.
"""
import re
import sys
import time

# Tables and their TC column names (in order)
TABLES = {
    'creature': ['guid', 'id', 'map', 'zoneId', 'areaId', 'spawnDifficulties',
                  'phaseUseFlags', 'PhaseId', 'PhaseGroup', 'terrainSwapMap',
                  'modelid', 'equipment_id', 'position_x', 'position_y', 'position_z',
                  'orientation', 'spawntimesecs', 'wander_distance', 'currentwaypoint',
                  'curHealthPct', 'MovementType', 'npcflag', 'unit_flags', 'unit_flags2',
                  'unit_flags3', 'ScriptName', 'StringId', 'VerifiedBuild'],
    'creature_loot_template': ['Entry', 'ItemType', 'Item', 'Chance', 'QuestRequired',
                                'LootMode', 'GroupId', 'MinCount', 'MaxCount', 'Comment'],
    'gameobject': ['guid', 'id', 'map', 'zoneId', 'areaId', 'spawnDifficulties',
                   'phaseUseFlags', 'PhaseId', 'PhaseGroup', 'terrainSwapMap',
                   'position_x', 'position_y', 'position_z', 'orientation',
                   'rotation0', 'rotation1', 'rotation2', 'rotation3',
                   'spawntimesecs', 'animprogress', 'state', 'ScriptName',
                   'StringId', 'VerifiedBuild'],
    'npc_vendor': ['entry', 'slot', 'item', 'maxcount', 'incrtime', 'ExtendedCost',
                   'type', 'BonusListIDs', 'PlayerConditionID', 'IgnoreFiltering',
                   'VerifiedBuild'],
    'scene_template': ['SceneId', 'Flags', 'ScriptPackageID', 'Encrypted', 'ScriptName'],
}

def extract(input_path, output_path):
    current_table = None
    in_target_data = False
    tables_extracted = set()

    start = time.time()

    with open(input_path, 'r', encoding='utf-8', errors='replace') as fin, \
         open(output_path, 'w', encoding='utf-8') as fout:

        fout.write("-- TC TDB data for tables with column mismatches\n")
        fout.write("-- Rewritten with explicit column names for INSERT IGNORE\n")
        fout.write(f"-- Source: {input_path.split('/')[-1]}\n\n")
        fout.write("SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0;\n\n")

        for line in fin:
            stripped = line.strip()

            # Track current table from LOCK TABLES or DROP TABLE
            m = re.match(r"DROP TABLE IF EXISTS `([^`]+)`", stripped)
            if m:
                current_table = m.group(1)
                if current_table in TABLES:
                    in_target_data = True
                    tables_extracted.add(current_table)
                    cols = ', '.join(f'`{c}`' for c in TABLES[current_table])
                    fout.write(f"-- Table: {current_table}\n")
                else:
                    in_target_data = False
                continue

            if not in_target_data:
                continue

            # Convert INSERT INTO to INSERT IGNORE INTO with column names
            if stripped.startswith('INSERT INTO'):
                cols = ', '.join(f'`{c}`' for c in TABLES[current_table])
                new_line = line.replace(
                    f"INSERT INTO `{current_table}` VALUES",
                    f"INSERT IGNORE INTO `{current_table}` ({cols}) VALUES",
                    1
                )
                fout.write(new_line)
                continue

            # Keep LOCK/UNLOCK and DISABLE/ENABLE KEYS
            if any(x in stripped for x in ['LOCK TABLES', 'UNLOCK TABLES', 'DISABLE KEYS', 'ENABLE KEYS']):
                fout.write(line)
                if 'UNLOCK' in stripped:
                    fout.write("\n")

        fout.write("SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS;\n")

    elapsed = time.time() - start
    print(f"Extracted {len(tables_extracted)} tables in {elapsed:.1f}s: {tables_extracted}", file=sys.stderr)

if __name__ == '__main__':
    input_path = sys.argv[1]
    output_path = sys.argv[2]
    extract(input_path, output_path)
