#!/usr/bin/env python3
"""
Diff our world DB against Draconic-WOW's world DB for a given zone.

Finds gameobjects and creatures present in Draconic but missing in ours,
generates idempotent INSERT IGNORE SQL to fill the gaps.

Also imports missing templates, creature_addon, creature_template_addon,
creature_equip_template, and creature_template_movement data.

Usage:
    python diff_draconic.py --zone 1519                    # Stormwind
    python diff_draconic.py --zone 1519 --dry-run          # Preview only
    python diff_draconic.py --zone 1637                    # Orgrimmar
    python diff_draconic.py --zone 1519 --skip-creatures   # GOs only
    python diff_draconic.py --zone 1519 --tolerance 5      # Wider match

Requirements:
    - Our MySQL on port 3306 (root/admin)
    - Draconic MySQL on named pipe (root, skip-grant-tables)
"""

import argparse
import subprocess
import sys
import math
import os
from collections import defaultdict
from datetime import datetime


# =============================================================================
# Configuration
# =============================================================================

MYSQL_BIN = r'"C:/Program Files/MySQL/MySQL Server 8.0/bin/mysql.exe"'
OUR_ARGS = '-u root -padmin --port=3306 world'
DRAC_ARGS = '-u root --pipe world'

# Corrupt GUID ranges to skip in Draconic (import artifacts)
CORRUPT_GUID_MIN = 71_000_000_000

# Position tolerance for matching spawns (world units, ~1 yard = 1 unit)
DEFAULT_TOLERANCE = 3.0

# Creature types to skip (ambient/cosmetic that we don't need)
# 8=Critter, 10=Totem, 11=Non-combat Pet
SKIP_CREATURE_TYPES = {8, 10, 11}

# GUID range for new inserts (high range to avoid conflicts)
NEW_GUID_BASE = 3_100_000_000

# Zone bounding boxes: (min_x, max_x, min_y, max_y) — includes zoneId=0 spawns
# Derived from UiMapAssignment DB2 data
ZONE_BOUNDS = {
    1519: (-9200, -8100, -200, 1800),   # Stormwind City
    1637: (1200, 2100, -5100, -4000),    # Orgrimmar
    1537: (-5800, -4600, -1400, -100),   # Ironforge
    1657: (8800, 10200, 800, 2200),      # Darnassus
    3487: (9200, 10400, -7700, -6500),   # Silvermoon City
    3557: (-4400, -3300, -11900, -11100), # Exodar
    1497: (-800, 900, -600, 1200),       # Undercity
    1638: (-1700, -600, -200, 1000),     # Thunder Bluff
}

# Zone name lookup (for friendly output)
ZONE_NAMES = {
    1: 'Dun Morogh', 12: 'Elwynn Forest', 14: 'Durotar', 15: 'Dustwallow Marsh',
    17: 'The Barrens', 40: 'Westfall', 44: 'Redridge Mountains', 45: 'Arathi Highlands',
    47: 'The Hinterlands', 85: 'Tirisfal Glades', 130: 'Silverpine Forest',
    139: 'Eastern Plaguelands', 141: 'Teldrassil', 148: 'Darkshore',
    215: 'Mulgore', 267: 'Hillsbrad Foothills', 331: 'Ashenvale',
    357: 'Feralas', 361: 'Felwood', 440: 'Tanaris', 490: 'Un\'Goro Crater',
    618: 'Winterspring', 1377: 'Silithus', 1497: 'Undercity', 1519: 'Stormwind City',
    1537: 'Ironforge', 1637: 'Orgrimmar', 1638: 'Thunder Bluff', 1657: 'Darnassus',
    3430: 'Eversong Woods', 3433: 'Ghostlands', 3487: 'Silvermoon City',
    3524: 'Azuremyst Isle', 3525: 'Bloodmyst Isle', 3557: 'The Exodar',
    4080: 'Isle of Quel\'Danas', 4395: 'Dalaran', 4922: 'Twilight Highlands',
    5042: 'Deepholm', 5095: 'Tol Barad', 5416: 'The Maelstrom',
    6170: 'Gorgrond', 6941: 'Ashran', 7502: 'Dalaran (Broken Isles)',
    7503: 'Highmountain', 8499: 'Boralus', 10424: 'Oribos',
    13644: 'Valdrakken', 14771: 'Khaz Algar', 14753: 'Dornogal',
}


# =============================================================================
# Table column definitions (verified identical in both DBs)
# =============================================================================

CREATURE_COLS = [
    'guid', 'id', 'map', 'zoneId', 'areaId', 'spawnDifficulties',
    'phaseUseFlags', 'PhaseId', 'PhaseGroup', 'terrainSwapMap',
    'modelid', 'equipment_id', 'position_x', 'position_y', 'position_z',
    'orientation', 'spawntimesecs', 'wander_distance', 'currentwaypoint',
    'curHealthPct', 'MovementType', 'npcflag', 'unit_flags', 'unit_flags2',
    'unit_flags3', 'ScriptName', 'StringId', 'VerifiedBuild', 'size',
]

GAMEOBJECT_COLS = [
    'guid', 'id', 'map', 'zoneId', 'areaId', 'spawnDifficulties',
    'phaseUseFlags', 'PhaseId', 'PhaseGroup', 'terrainSwapMap',
    'position_x', 'position_y', 'position_z', 'orientation',
    'rotation0', 'rotation1', 'rotation2', 'rotation3',
    'spawntimesecs', 'animprogress', 'state', 'ScriptName', 'StringId',
    'VerifiedBuild', 'size', 'visibility',
]

CREATURE_TEMPLATE_COLS = [
    'entry', 'KillCredit1', 'KillCredit2', 'name', 'femaleName', 'subname',
    'TitleAlt', 'IconName', 'RequiredExpansion', 'VignetteID', 'faction',
    'npcflag', 'speed_walk', 'speed_run', 'scale', 'Classification',
    'dmgschool', 'BaseAttackTime', 'RangeAttackTime', 'BaseVariance',
    'RangeVariance', 'unit_class', 'unit_flags', 'unit_flags2', 'unit_flags3',
    'family', 'trainer_class', 'type', 'VehicleId', 'AIName', 'MovementType',
    'ExperienceModifier', 'RacialLeader', 'movementId', 'WidgetSetID',
    'WidgetSetUnitConditionID', 'RegenHealth', 'CreatureImmunitiesId',
    'flags_extra', 'ScriptName', 'StringId', 'VerifiedBuild',
]

GO_TEMPLATE_COLS = [
    'entry', 'type', 'displayId', 'name', 'IconName', 'castBarCaption',
    'unk1', 'size', 'Data0', 'Data1', 'Data2', 'Data3', 'Data4', 'Data5',
    'Data6', 'Data7', 'Data8', 'Data9', 'Data10', 'Data11', 'Data12',
    'Data13', 'Data14', 'Data15', 'Data16', 'Data17', 'Data18', 'Data19',
    'Data20', 'Data21', 'Data22', 'Data23', 'Data24', 'Data25', 'Data26',
    'Data27', 'Data28', 'Data29', 'Data30', 'Data31', 'Data32', 'Data33',
    'Data34', 'ContentTuningId', 'RequiredLevel', 'AIName', 'ScriptName',
    'VerifiedBuild',
]

CREATURE_ADDON_COLS = [
    'guid', 'PathId', 'mount', 'MountCreatureID', 'StandState', 'AnimTier',
    'VisFlags', 'SheathState', 'PvPFlags', 'emote', 'aiAnimKit',
    'movementAnimKit', 'meleeAnimKit', 'visibilityDistanceType', 'auras',
]

CREATURE_TEMPLATE_ADDON_COLS = [
    'entry', 'PathId', 'mount', 'MountCreatureID', 'StandState', 'AnimTier',
    'VisFlags', 'SheathState', 'PvPFlags', 'emote', 'aiAnimKit',
    'movementAnimKit', 'meleeAnimKit', 'visibilityDistanceType', 'auras',
]

CREATURE_EQUIP_COLS = [
    'CreatureID', 'ID', 'ItemID1', 'AppearanceModID1', 'ItemVisual1',
    'ItemID2', 'AppearanceModID2', 'ItemVisual2',
    'ItemID3', 'AppearanceModID3', 'ItemVisual3', 'VerifiedBuild',
]

CREATURE_TEMPLATE_MOVEMENT_COLS = [
    'CreatureId', 'HoverInitiallyEnabled', 'Chase', 'Random',
    'InteractionPauseTimer',
]

# Which columns are string-typed (need quoting in SQL)
STRING_COLUMNS = {
    'name', 'femaleName', 'subname', 'TitleAlt', 'IconName', 'AIName',
    'ScriptName', 'StringId', 'spawnDifficulties', 'castBarCaption', 'unk1',
    'auras',
}

# Which columns are nullable (can be NULL instead of empty)
NULLABLE_COLUMNS = {
    'PhaseId', 'PhaseGroup', 'npcflag', 'unit_flags', 'unit_flags2',
    'unit_flags3', 'StringId', 'auras',
}


# =============================================================================
# Database helpers
# =============================================================================

def run_query(args: str, query: str) -> list[str]:
    """Run a MySQL query and return raw output lines."""
    cmd = f'{MYSQL_BIN} {args} -N -B -e "{query}"'
    try:
        result = subprocess.run(
            cmd, shell=True, capture_output=True, text=True, timeout=120
        )
    except subprocess.TimeoutExpired:
        print(f"  TIMEOUT: {query[:80]}...", file=sys.stderr)
        return []

    if result.returncode != 0:
        stderr = result.stderr.strip()
        if stderr and 'Using a password' not in stderr:
            print(f"  SQL ERROR: {stderr}", file=sys.stderr)
            return []

    lines = result.stdout.strip().split('\n') if result.stdout.strip() else []
    return lines


def run_query_dicts(args: str, query: str, columns: list[str]) -> list[dict]:
    """Run query, parse tab-separated output into dicts."""
    lines = run_query(args, query)
    results = []
    for line in lines:
        fields = line.split('\t')
        if len(fields) == len(columns):
            row = {}
            for col, val in zip(columns, fields):
                row[col] = val
            results.append(row)
        elif len(fields) > 0:
            # Column count mismatch — log and skip
            print(f"  WARNING: Column mismatch ({len(fields)} vs {len(columns)} expected), skipping row",
                  file=sys.stderr)
    return results


def get_max_guid(args: str, table: str) -> int:
    """Get the max guid from a table (excluding corrupt range)."""
    lines = run_query(args, f"SELECT MAX(guid) FROM {table} WHERE guid < {CORRUPT_GUID_MIN}")
    if lines and lines[0] and lines[0] != 'NULL':
        return int(lines[0])
    return 0


def verify_connection(args: str, label: str) -> bool:
    """Verify a MySQL connection works."""
    result = run_query(args, "SELECT 1")
    if result and result[0].strip() == '1':
        print(f"  {label}: OK")
        return True
    print(f"  {label}: FAILED")
    return False


def verify_columns(args: str, table: str, expected_cols: list[str], label: str) -> bool:
    """Verify table has expected columns."""
    lines = run_query(args, f"SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "
                            f"WHERE TABLE_SCHEMA='world' AND TABLE_NAME='{table}' "
                            f"ORDER BY ORDINAL_POSITION")
    actual_cols = {line.strip() for line in lines if line.strip()}
    missing = [c for c in expected_cols if c not in actual_cols]
    if missing:
        print(f"  SCHEMA MISMATCH [{label}.{table}]: missing columns: {missing}", file=sys.stderr)
        return False
    return True


# =============================================================================
# Core diff logic
# =============================================================================

def pos_distance(a: dict, b: dict) -> float:
    """Euclidean distance between two spawn positions."""
    try:
        dx = float(a['position_x']) - float(b['position_x'])
        dy = float(a['position_y']) - float(b['position_y'])
        dz = float(a['position_z']) - float(b['position_z'])
        return math.sqrt(dx * dx + dy * dy + dz * dz)
    except (ValueError, KeyError):
        return 999999.0


def build_spatial_index(rows: list[dict]) -> dict:
    """Index rows by entry ID for fast lookup."""
    idx = defaultdict(list)
    for row in rows:
        idx[row['id']].append(row)
    return idx


def find_missing_spawns(draconic_rows: list[dict], our_rows: list[dict],
                        tolerance: float) -> list[dict]:
    """Find spawns in Draconic that don't exist in ours (by entry + position)."""
    our_index = build_spatial_index(our_rows)
    missing = []

    for drow in draconic_rows:
        guid = int(drow['guid'])
        # Skip corrupt guids
        if guid >= CORRUPT_GUID_MIN:
            continue

        # Skip spawns with Z=0 (broken data)
        try:
            if abs(float(drow['position_z'])) < 0.01:
                continue
        except ValueError:
            continue

        # Skip spawns with NaN/inf or near-origin positions (broken data)
        try:
            px = float(drow['position_x'])
            py = float(drow['position_y'])
            pz = float(drow['position_z'])
            if any(math.isnan(v) or math.isinf(v) for v in (px, py, pz)):
                continue
            # Filter world-origin spawns (all coords near 0 = broken)
            if abs(px) < 10 and abs(py) < 10:
                continue
        except ValueError:
            continue

        entry_id = drow['id']
        candidates = our_index.get(entry_id, [])

        # Check if any of our spawns match within position tolerance
        matched = False
        for orow in candidates:
            if pos_distance(drow, orow) < tolerance:
                matched = True
                break

        if not matched:
            missing.append(drow)

    return missing


def filter_by_creature_type(missing_spawns: list[dict], drac_args: str) -> list[dict]:
    """Remove critters, totems, non-combat pets from the missing list."""
    if not missing_spawns or not SKIP_CREATURE_TYPES:
        return missing_spawns

    entry_ids = set(row['id'] for row in missing_spawns)
    if not entry_ids:
        return missing_spawns

    entry_list = ','.join(entry_ids)
    type_list = ','.join(str(t) for t in SKIP_CREATURE_TYPES)
    lines = run_query(
        drac_args,
        f"SELECT entry FROM creature_template WHERE entry IN ({entry_list}) AND type IN ({type_list})"
    )
    skip_entries = {line.strip() for line in lines if line.strip()}

    if not skip_entries:
        return missing_spawns

    filtered = [row for row in missing_spawns if row['id'] not in skip_entries]
    removed = len(missing_spawns) - len(filtered)
    if removed:
        print(f"  Filtered {removed} critter/totem/pet spawns")
    return filtered


# =============================================================================
# Template + support data importers
# =============================================================================

def find_missing_entries(missing_spawns: list[dict], our_args: str,
                         template_table: str) -> set[str]:
    """Return entry IDs from missing spawns that don't have templates in our DB."""
    entry_ids = set(row['id'] for row in missing_spawns)
    if not entry_ids:
        return set()

    entry_list = ','.join(entry_ids)
    lines = run_query(our_args, f"SELECT entry FROM {template_table} WHERE entry IN ({entry_list})")
    our_entries = {line.strip() for line in lines if line.strip()}
    return entry_ids - our_entries


def fetch_templates(drac_args: str, entry_ids: set[str],
                    template_table: str, columns: list[str]) -> list[dict]:
    """Fetch full template rows from Draconic for given entry IDs."""
    if not entry_ids:
        return []

    entry_list = ','.join(entry_ids)
    col_str = ','.join(f'`{c}`' for c in columns)
    return run_query_dicts(
        drac_args,
        f"SELECT {col_str} FROM {template_table} WHERE entry IN ({entry_list})",
        columns
    )


def fetch_creature_addon_for_guids(drac_args: str, drac_guids: list[int]) -> list[dict]:
    """Fetch creature_addon rows from Draconic by original GUID."""
    if not drac_guids:
        return []

    # Batch in chunks of 1000 to avoid query length limits
    all_rows = []
    for i in range(0, len(drac_guids), 1000):
        batch = drac_guids[i:i + 1000]
        guid_list = ','.join(str(g) for g in batch)
        col_str = ','.join(f'`{c}`' for c in CREATURE_ADDON_COLS)
        rows = run_query_dicts(
            drac_args,
            f"SELECT {col_str} FROM creature_addon WHERE guid IN ({guid_list})",
            CREATURE_ADDON_COLS
        )
        all_rows.extend(rows)
    return all_rows


def fetch_support_data_for_entries(our_args: str, drac_args: str,
                                   missing_entries: set[str],
                                   table: str, columns: list[str],
                                   id_col: str) -> list[dict]:
    """Fetch rows from a support table in Draconic for entries missing from ours."""
    if not missing_entries:
        return []

    entry_list = ','.join(missing_entries)

    # Check which entries already have rows in our DB
    lines = run_query(our_args, f"SELECT `{id_col}` FROM `{table}` WHERE `{id_col}` IN ({entry_list})")
    our_entries = {line.strip() for line in lines if line.strip()}
    need_entries = missing_entries - our_entries

    if not need_entries:
        return []

    entry_list = ','.join(need_entries)
    col_str = ','.join(f'`{c}`' for c in columns)
    return run_query_dicts(
        drac_args,
        f"SELECT {col_str} FROM `{table}` WHERE `{id_col}` IN ({entry_list})",
        columns
    )


# =============================================================================
# SQL generation
# =============================================================================

def escape_sql(val: str) -> str:
    """Escape a string value for SQL."""
    if val == 'NULL' or val == '\\N':
        return 'NULL'
    return val.replace('\\', '\\\\').replace("'", "\\'")


def format_value(col: str, val: str) -> str:
    """Format a single column value for SQL INSERT."""
    # Always zero out VerifiedBuild for imported data
    if col == 'VerifiedBuild':
        return '0'

    if val == 'NULL' or val == '\\N':
        if col in NULLABLE_COLUMNS:
            return 'NULL'
        elif col in STRING_COLUMNS:
            return "''"
        else:
            return '0'

    if col in STRING_COLUMNS:
        val = val.strip()  # Trim leading/trailing whitespace
        # ScriptName/AIName '0' is invalid — treat as empty
        if col in ('ScriptName', 'AIName') and val == '0':
            val = ''
        if val == '':
            if col == 'StringId':
                return 'NULL'
            return "''"
        return f"'{escape_sql(val)}'"

    # Numeric — strip whitespace and pass through
    return val.strip() if val else '0'


def generate_insert_sql(table: str, columns: list[str], rows: list[dict],
                        guid_col: str = 'guid', guid_base: int = 0,
                        guid_map: dict = None) -> tuple[list[str], int, dict]:
    """
    Generate INSERT IGNORE statements.

    If guid_base > 0, assigns new GUIDs starting from guid_base.
    Returns (statements, next_guid, {old_guid: new_guid}).
    """
    statements = []
    next_guid = guid_base
    new_guid_map = guid_map if guid_map is not None else {}

    for row in rows:
        values = []

        for col in columns:
            val = row.get(col, '')

            if col == guid_col and guid_base > 0:
                values.append(str(next_guid))
                new_guid_map[row.get(guid_col, '')] = next_guid
                next_guid += 1
            else:
                values.append(format_value(col, val))

        col_str = ','.join(f'`{c}`' for c in columns)
        val_str = ','.join(values)
        statements.append(f"INSERT IGNORE INTO `{table}` ({col_str}) VALUES ({val_str});")

    return statements, next_guid, new_guid_map


def generate_addon_sql_remapped(addon_rows: list[dict], guid_map: dict) -> list[str]:
    """Generate INSERT IGNORE for creature_addon using remapped GUIDs."""
    statements = []

    for row in addon_rows:
        old_guid = row['guid']
        if old_guid not in guid_map:
            continue  # No matching new creature spawn

        values = []
        for col in CREATURE_ADDON_COLS:
            val = row.get(col, '')
            if col == 'guid':
                values.append(str(guid_map[old_guid]))
            else:
                values.append(format_value(col, val))

        col_str = ','.join(f'`{c}`' for c in CREATURE_ADDON_COLS)
        val_str = ','.join(values)
        statements.append(f"INSERT IGNORE INTO `creature_addon` ({col_str}) VALUES ({val_str});")

    return statements


# =============================================================================
# Reporting
# =============================================================================

def get_entry_names(args: str, entry_ids: set[str], template_table: str) -> dict:
    """Get names for entry IDs."""
    if not entry_ids:
        return {}

    entry_list = ','.join(entry_ids)
    lines = run_query(args, f"SELECT entry, name FROM {template_table} WHERE entry IN ({entry_list})")
    names = {}
    for line in lines:
        parts = line.split('\t', 1)
        if len(parts) == 2:
            names[parts[0]] = parts[1]
    return names


def print_summary(table_name: str, drac_count: int, our_count: int,
                  missing_count: int, missing_templates: set,
                  missing_rows: list[dict], drac_args: str):
    """Print a summary of the diff."""
    template_table = 'creature_template' if table_name == 'creature' else 'gameobject_template'

    print(f"\n{'=' * 70}")
    print(f"  {table_name.upper()} DIFF SUMMARY")
    print(f"{'=' * 70}")
    print(f"  Draconic:  {drac_count:>6} spawns")
    print(f"  Ours:      {our_count:>6} spawns")
    print(f"  Missing:   {missing_count:>6} spawns")

    if missing_templates:
        print(f"  Missing templates: {len(missing_templates)} entries (will import from Draconic)")

    if missing_rows:
        entry_counts = defaultdict(int)
        for row in missing_rows:
            entry_counts[row['id']] += 1

        entry_ids = set(entry_counts.keys())
        names = get_entry_names(drac_args, entry_ids, template_table)

        print(f"\n  Top missing entries:")
        sorted_entries = sorted(entry_counts.items(), key=lambda x: -x[1])[:30]
        for entry_id, count in sorted_entries:
            name = names.get(entry_id, '???')
            tmpl = ' [+TEMPLATE]' if entry_id in missing_templates else ''
            print(f"    {entry_id:>8} x{count:<4} {name}{tmpl}")

        if len(sorted_entries) < len(entry_counts):
            print(f"    ... and {len(entry_counts) - len(sorted_entries)} more entries")


# =============================================================================
# QA Validation
# =============================================================================

def validate_sql(sql_lines: list[str]) -> list[str]:
    """Run validation checks on generated SQL. Returns list of warnings."""
    warnings = []
    guid_sets = defaultdict(set)  # table -> set of guids

    for line in sql_lines:
        if not line.startswith('INSERT'):
            continue

        # Check for basic syntax issues
        if line.count('(') != line.count(')'):
            warnings.append(f"Unbalanced parentheses: {line[:80]}...")

        if line.count("'") % 2 != 0:
            # Check for escaped quotes (don't count \')
            unescaped = line.replace("\\'", "")
            if unescaped.count("'") % 2 != 0:
                warnings.append(f"Unbalanced quotes: {line[:80]}...")

        # Extract table name and check for GUID uniqueness
        parts = line.split('`')
        if len(parts) >= 3:
            table = parts[1]
            # Extract first value (guid) from VALUES (...)
            val_start = line.find('VALUES (')
            if val_start > 0:
                val_section = line[val_start + 8:]
                first_val = val_section.split(',')[0].strip()
                if first_val.isdigit():
                    guid = int(first_val)
                    if guid in guid_sets[table]:
                        warnings.append(f"Duplicate GUID {guid} in {table}")
                    guid_sets[table].add(guid)

    # Check for GUID range collisions between tables
    for table, guids in guid_sets.items():
        if guids:
            min_g = min(guids)
            max_g = max(guids)
            if min_g < NEW_GUID_BASE and table in ('creature', 'gameobject'):
                warnings.append(f"{table} GUIDs start at {min_g} (below safe range {NEW_GUID_BASE})")

    return warnings


# =============================================================================
# Main workflow
# =============================================================================

def diff_zone(zone_id: int, map_id: int, tolerance: float,
              skip_creatures: bool, skip_gameobjects: bool,
              dry_run: bool, output_path: str):
    """Run the full diff workflow for a zone."""
    zone_name = ZONE_NAMES.get(zone_id, f'Zone {zone_id}')

    print(f"Draconic World DB Diff Tool")
    print(f"Zone: {zone_id} ({zone_name}), Map: {map_id}, Tolerance: {tolerance} yards")
    print(f"{'=' * 70}")

    # ---- Verify connections ----
    print("\nVerifying database connections...")
    if not verify_connection(OUR_ARGS, "Our DB"):
        print("ERROR: Cannot connect to our MySQL (port 3306)")
        sys.exit(1)
    if not verify_connection(DRAC_ARGS, "Draconic DB"):
        print("ERROR: Cannot connect to Draconic MySQL (named pipe)")
        print("  Make sure Draconic's mysqld is running with --skip-grant-tables")
        sys.exit(1)

    # ---- Verify schemas ----
    print("\nVerifying table schemas...")
    schema_ok = True
    for label, args in [("Ours", OUR_ARGS), ("Draconic", DRAC_ARGS)]:
        if not skip_creatures:
            schema_ok &= verify_columns(args, 'creature', CREATURE_COLS, label)
            schema_ok &= verify_columns(args, 'creature_template', CREATURE_TEMPLATE_COLS, label)
            schema_ok &= verify_columns(args, 'creature_addon', CREATURE_ADDON_COLS, label)
        if not skip_gameobjects:
            schema_ok &= verify_columns(args, 'gameobject', GAMEOBJECT_COLS, label)
            schema_ok &= verify_columns(args, 'gameobject_template', GO_TEMPLATE_COLS, label)
    if not schema_ok:
        print("ERROR: Schema mismatch detected. Aborting.")
        sys.exit(1)
    print("  All schemas verified OK")

    # ---- Build zone query clause (includes zoneId=0 spawns within bounds) ----
    bounds = ZONE_BOUNDS.get(zone_id)
    if bounds:
        min_x, max_x, min_y, max_y = bounds
        zone_clause = (
            f"(zoneId = {zone_id} OR "
            f"(zoneId = 0 AND position_x BETWEEN {min_x} AND {max_x} "
            f"AND position_y BETWEEN {min_y} AND {max_y} "
            f"AND position_z > 0))"
        )
        print(f"  Using coordinate bounds: X[{min_x},{max_x}] Y[{min_y},{max_y}]")
    else:
        zone_clause = f"zoneId = {zone_id}"
        print(f"  WARNING: No coordinate bounds for zone {zone_id}, using zoneId match only")

    # ---- Begin SQL output ----
    all_sql = [
        f"-- ============================================================================",
        f"-- Draconic diff import: {zone_name} (zone {zone_id}, map {map_id})",
        f"-- Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
        f"-- Position tolerance: {tolerance} yards",
        f"-- Source: Draconic-WOW build 66666 (named pipe)",
        f"-- ============================================================================",
        f"",
        f"SET innodb_lock_wait_timeout = 120;",
        f"",
    ]

    total_creature_inserts = 0
    total_go_inserts = 0
    total_template_inserts = 0
    total_addon_inserts = 0

    # ==== GAMEOBJECT DIFF ====
    if not skip_gameobjects:
        print("\n--- GAMEOBJECT DIFF ---")
        go_col_str = ','.join(f'`{c}`' for c in GAMEOBJECT_COLS)

        print("  Fetching Draconic gameobjects...")
        drac_go = run_query_dicts(
            DRAC_ARGS,
            f"SELECT {go_col_str} FROM gameobject WHERE map = {map_id} AND {zone_clause}",
            GAMEOBJECT_COLS
        )
        print(f"  Draconic: {len(drac_go)} gameobjects")

        print("  Fetching our gameobjects...")
        our_go = run_query_dicts(
            OUR_ARGS,
            f"SELECT {go_col_str} FROM gameobject WHERE map = {map_id} AND {zone_clause}",
            GAMEOBJECT_COLS
        )
        print(f"  Ours: {len(our_go)} gameobjects")

        print("  Finding missing spawns...")
        missing_go = find_missing_spawns(drac_go, our_go, tolerance)
        print(f"  Missing: {len(missing_go)} gameobjects")

        # Check for missing GO templates
        print("  Checking templates...")
        missing_go_entries = find_missing_entries(missing_go, OUR_ARGS, 'gameobject_template')

        # Fetch missing GO templates from Draconic
        go_template_data = fetch_templates(DRAC_ARGS, missing_go_entries,
                                           'gameobject_template', GO_TEMPLATE_COLS)
        imported_go_entries = {t['entry'] for t in go_template_data}

        # Only include spawns whose templates exist (or will be imported)
        spawnable_go = [
            row for row in missing_go
            if row['id'] not in missing_go_entries or row['id'] in imported_go_entries
        ]

        print_summary('gameobject', len(drac_go), len(our_go), len(missing_go),
                       missing_go_entries, missing_go, DRAC_ARGS)

        # Generate GO template SQL
        if go_template_data:
            all_sql.append(f"-- ============================================================================")
            all_sql.append(f"-- Missing gameobject_template entries ({len(go_template_data)} rows)")
            all_sql.append(f"-- ============================================================================")
            all_sql.append(f"")
            stmts, _, _ = generate_insert_sql('gameobject_template', GO_TEMPLATE_COLS,
                                               go_template_data, guid_col='entry', guid_base=0)
            all_sql.extend(stmts)
            all_sql.append(f"")
            total_template_inserts += len(stmts)

        # Generate GO spawn SQL
        if spawnable_go:
            our_max = get_max_guid(OUR_ARGS, 'gameobject')
            go_guid_base = max(our_max + 1, NEW_GUID_BASE)

            all_sql.append(f"-- ============================================================================")
            all_sql.append(f"-- Missing gameobject spawns ({len(spawnable_go)} rows)")
            all_sql.append(f"-- GUID range: {go_guid_base} - {go_guid_base + len(spawnable_go) - 1}")
            all_sql.append(f"-- ============================================================================")
            all_sql.append(f"")

            go_stmts, _, _ = generate_insert_sql('gameobject', GAMEOBJECT_COLS,
                                                  spawnable_go, 'guid', go_guid_base)
            all_sql.extend(go_stmts)
            all_sql.append(f"")
            total_go_inserts = len(go_stmts)

    # ==== CREATURE DIFF ====
    if not skip_creatures:
        print("\n--- CREATURE DIFF ---")
        cr_col_str = ','.join(f'`{c}`' for c in CREATURE_COLS)

        print("  Fetching Draconic creatures...")
        drac_cr = run_query_dicts(
            DRAC_ARGS,
            f"SELECT {cr_col_str} FROM creature WHERE map = {map_id} AND {zone_clause}",
            CREATURE_COLS
        )
        print(f"  Draconic: {len(drac_cr)} creatures")

        print("  Fetching our creatures...")
        our_cr = run_query_dicts(
            OUR_ARGS,
            f"SELECT {cr_col_str} FROM creature WHERE map = {map_id} AND {zone_clause}",
            CREATURE_COLS
        )
        print(f"  Ours: {len(our_cr)} creatures")

        print("  Finding missing spawns...")
        missing_cr = find_missing_spawns(drac_cr, our_cr, tolerance)
        print(f"  Missing (before filter): {len(missing_cr)} creatures")

        # Filter out critters/totems/pets
        missing_cr = filter_by_creature_type(missing_cr, DRAC_ARGS)
        print(f"  Missing (after filter): {len(missing_cr)} creatures")

        # Check for missing creature_templates
        print("  Checking templates...")
        missing_cr_entries = find_missing_entries(missing_cr, OUR_ARGS, 'creature_template')

        # Fetch missing creature_templates from Draconic
        cr_template_data = fetch_templates(DRAC_ARGS, missing_cr_entries,
                                           'creature_template', CREATURE_TEMPLATE_COLS)
        imported_cr_entries = {t['entry'] for t in cr_template_data}

        # Fetch support data for newly imported entries
        cr_template_addon_data = fetch_support_data_for_entries(
            OUR_ARGS, DRAC_ARGS, imported_cr_entries,
            'creature_template_addon', CREATURE_TEMPLATE_ADDON_COLS, 'entry'
        )
        cr_equip_data = fetch_support_data_for_entries(
            OUR_ARGS, DRAC_ARGS, imported_cr_entries,
            'creature_equip_template', CREATURE_EQUIP_COLS, 'CreatureID'
        )
        cr_movement_data = fetch_support_data_for_entries(
            OUR_ARGS, DRAC_ARGS, imported_cr_entries,
            'creature_template_movement', CREATURE_TEMPLATE_MOVEMENT_COLS, 'CreatureId'
        )

        # Only include spawns whose templates exist or will be imported
        spawnable_cr = [
            row for row in missing_cr
            if row['id'] not in missing_cr_entries or row['id'] in imported_cr_entries
        ]

        # Spawns we had to skip (template missing in BOTH databases)
        unskippable = missing_cr_entries - imported_cr_entries
        if unskippable:
            print(f"  WARNING: {len(unskippable)} entries exist in neither DB — skipping those spawns")

        print_summary('creature', len(drac_cr), len(our_cr), len(missing_cr),
                       missing_cr_entries, missing_cr, DRAC_ARGS)

        # Generate creature_template SQL
        if cr_template_data:
            all_sql.append(f"-- ============================================================================")
            all_sql.append(f"-- Missing creature_template entries ({len(cr_template_data)} rows)")
            all_sql.append(f"-- ============================================================================")
            all_sql.append(f"")
            stmts, _, _ = generate_insert_sql('creature_template', CREATURE_TEMPLATE_COLS,
                                               cr_template_data, guid_col='entry', guid_base=0)
            all_sql.extend(stmts)
            all_sql.append(f"")
            total_template_inserts += len(stmts)

        # Generate creature_template_addon SQL
        if cr_template_addon_data:
            all_sql.append(f"-- Missing creature_template_addon ({len(cr_template_addon_data)} rows)")
            stmts, _, _ = generate_insert_sql('creature_template_addon', CREATURE_TEMPLATE_ADDON_COLS,
                                               cr_template_addon_data, guid_col='entry', guid_base=0)
            all_sql.extend(stmts)
            all_sql.append(f"")
            total_addon_inserts += len(stmts)

        # Generate creature_equip_template SQL
        if cr_equip_data:
            all_sql.append(f"-- Missing creature_equip_template ({len(cr_equip_data)} rows)")
            stmts, _, _ = generate_insert_sql('creature_equip_template', CREATURE_EQUIP_COLS,
                                               cr_equip_data, guid_col='CreatureID', guid_base=0)
            all_sql.extend(stmts)
            all_sql.append(f"")
            total_addon_inserts += len(stmts)

        # Generate creature_template_movement SQL
        if cr_movement_data:
            all_sql.append(f"-- Missing creature_template_movement ({len(cr_movement_data)} rows)")
            stmts, _, _ = generate_insert_sql('creature_template_movement', CREATURE_TEMPLATE_MOVEMENT_COLS,
                                               cr_movement_data, guid_col='CreatureId', guid_base=0)
            all_sql.extend(stmts)
            all_sql.append(f"")
            total_addon_inserts += len(stmts)

        # Generate creature spawn SQL (with new GUIDs)
        if spawnable_cr:
            our_max = get_max_guid(OUR_ARGS, 'creature')
            cr_guid_base = max(our_max + 1, NEW_GUID_BASE)

            all_sql.append(f"-- ============================================================================")
            all_sql.append(f"-- Missing creature spawns ({len(spawnable_cr)} rows)")
            all_sql.append(f"-- GUID range: {cr_guid_base} - {cr_guid_base + len(spawnable_cr) - 1}")
            all_sql.append(f"-- ============================================================================")
            all_sql.append(f"")

            cr_stmts, next_cr_guid, guid_map = generate_insert_sql(
                'creature', CREATURE_COLS, spawnable_cr, 'guid', cr_guid_base
            )
            all_sql.extend(cr_stmts)
            all_sql.append(f"")
            total_creature_inserts = len(cr_stmts)

            # Fetch and remap creature_addon for imported spawns
            print("  Fetching creature_addon for missing spawns...")
            drac_guids = [int(row['guid']) for row in spawnable_cr]
            addon_rows = fetch_creature_addon_for_guids(DRAC_ARGS, drac_guids)
            if addon_rows:
                all_sql.append(f"-- creature_addon for imported spawns ({len(addon_rows)} rows)")
                addon_stmts = generate_addon_sql_remapped(addon_rows, guid_map)
                all_sql.extend(addon_stmts)
                all_sql.append(f"")
                total_addon_inserts += len(addon_stmts)
                print(f"  Imported {len(addon_stmts)} creature_addon rows")

    # ==== QA VALIDATION ====
    print(f"\n{'=' * 70}")
    print(f"  QA VALIDATION")
    print(f"{'=' * 70}")
    warnings = validate_sql(all_sql)
    if warnings:
        for w in warnings:
            print(f"  WARNING: {w}")
    else:
        print(f"  All checks passed")

    total_inserts = sum(1 for l in all_sql if l.startswith('INSERT'))
    print(f"\n  Total INSERT IGNORE statements: {total_inserts}")
    print(f"    Templates:      {total_template_inserts}")
    print(f"    Creatures:      {total_creature_inserts}")
    print(f"    Gameobjects:    {total_go_inserts}")
    print(f"    Addon/support:  {total_addon_inserts}")

    # ==== Output ====
    if dry_run:
        print(f"\n  DRY RUN — no SQL written")
        print(f"  Would generate {len(all_sql)} lines of SQL")
    else:
        if not output_path:
            os.makedirs('sql/exports', exist_ok=True)
            output_path = f"sql/exports/draconic_diff_zone_{zone_id}.sql"

        with open(output_path, 'w', encoding='utf-8') as f:
            f.write('\n'.join(all_sql) + '\n')

        print(f"\n  SQL written to: {output_path}")

    print(f"{'=' * 70}")
    return all_sql


def main():
    parser = argparse.ArgumentParser(description='Diff world DB against Draconic')
    parser.add_argument('--zone', type=int, required=True,
                        help='Zone ID to diff (e.g., 1519 for Stormwind)')
    parser.add_argument('--map', type=int, default=0,
                        help='Map ID (default: 0 for Eastern Kingdoms)')
    parser.add_argument('--dry-run', action='store_true',
                        help='Preview only, do not write SQL')
    parser.add_argument('--output', type=str, default=None,
                        help='Output SQL file path')
    parser.add_argument('--skip-creatures', action='store_true',
                        help='Skip creature diff')
    parser.add_argument('--skip-gameobjects', action='store_true',
                        help='Skip gameobject diff')
    parser.add_argument('--tolerance', type=float, default=DEFAULT_TOLERANCE,
                        help=f'Position match tolerance in yards (default: {DEFAULT_TOLERANCE})')
    args = parser.parse_args()

    diff_zone(
        zone_id=args.zone,
        map_id=args.map,
        tolerance=args.tolerance,
        skip_creatures=args.skip_creatures,
        skip_gameobjects=args.skip_gameobjects,
        dry_run=args.dry_run,
        output_path=args.output,
    )


if __name__ == '__main__':
    main()
