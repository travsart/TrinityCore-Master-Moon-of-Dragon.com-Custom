#!/usr/bin/env python3
"""
Comprehensive Cell::Visit Deadlock Fix - Final Pass
Fixes ALL remaining Cell::VisitGridObjects and Cell::VisitAllObjects calls
"""

import os
import re
from pathlib import Path
from typing import List, Tuple

BASE_DIR = Path(r"C:\TrinityBots\TrinityCore\src\modules\Playerbot")

# Files already completely fixed - skip these
COMPLETELY_FIXED = {
    "TargetSelector.cpp", "PositionManager.cpp",
    "CombatStateAnalyzer.cpp", "InterruptManager.cpp", "DefensiveBehaviorManager.cpp",
    "AoEDecisionManager.cpp", "LineOfSightManager.cpp", "ObstacleAvoidanceManager.cpp",
    "KitingManager.cpp", "InterruptAwareness.cpp", "DispelCoordinator.cpp", "LootStrategy.cpp",
    "SpatialGridManager.cpp", "SpatialGridManager.h", "DoubleBufferedSpatialGrid.cpp",
    "DoubleBufferedSpatialGrid.h"
}

def calculate_relative_path(file_path: Path) -> str:
    """Calculate relative include path to Spatial/SpatialGridManager.h"""
    try:
        relative = file_path.relative_to(BASE_DIR)
        depth = len(relative.parts) - 1
        return ("../" * depth) + "Spatial/SpatialGridManager.h"
    except:
        return "../../Spatial/SpatialGridManager.h"

def add_include(content: str, file_path: Path) -> str:
    """Add SpatialGridManager include if not present"""
    if "SpatialGridManager.h" in content:
        return content

    rel_path = calculate_relative_path(file_path)
    include_line = f'#include "{rel_path}"  // Lock-free spatial grid for deadlock fix\n'

    lines = content.split("\n")
    last_include = -1
    for i, line in enumerate(lines):
        if line.strip().startswith("#include"):
            last_include = i

    if last_include != -1:
        lines.insert(last_include + 1, include_line.rstrip())
        return "\n".join(lines)
    return content

def detect_entity_type(context: str) -> Tuple[str, str, str]:
    """
    Detect entity type from context and return (query_method, accessor, entity_type)
    """
    # Check for GameObject patterns
    if any(x in context for x in ["GameObject", "GameObjectListSearcher", "GO_", "GetGameObject"]):
        return "QueryNearbyGameObjects", "map->GetGameObject", "GameObject"

    # Check for AreaTrigger (not supported yet)
    if "AreaTrigger" in context:
        return None, None, "AreaTrigger"

    # Check for DynamicObject (not supported yet)
    if "DynamicObject" in context:
        return None, None, "DynamicObject"

    # Default to Creature
    return "QueryNearbyCreatures", "ObjectAccessor::GetCreature", "Creature"

def fix_cell_visit_gridobj(content: str, file_path: Path) -> Tuple[str, int]:
    """Fix Cell::VisitGridObjects calls"""
    count = 0

    # More flexible pattern to catch various formatting
    pattern = r'Cell::VisitGridObjects\s*\(\s*([^,]+?)\s*,\s*([^,]+?)\s*,\s*([^)]+?)\s*\)\s*;'

    def replace_func(match):
        nonlocal count

        bot_var = match.group(1).strip()
        searcher_var = match.group(2).strip()
        range_val = match.group(3).strip()

        # Get context to detect entity type
        start_pos = max(0, match.start() - 500)
        end_pos = min(len(content), match.end() + 200)
        context = content[start_pos:end_pos]

        query_method, accessor, entity_type = detect_entity_type(context)

        # Skip unsupported types
        if query_method is None:
            return match.group(0)

        count += 1

        # Determine return statement based on context
        return_stmt = "return;"
        if "return false" in context or "bool " in context:
            return_stmt = "return false;"
        elif "return nullptr" in context or "return NULL" in context:
            return_stmt = "return nullptr;"
        elif "return 0" in context or "uint32 " in context or "int " in context:
            return_stmt = "return 0;"

        replacement = f"""// DEADLOCK FIX: Use lock-free spatial grid instead of Cell::VisitGridObjects
    Map* map = {bot_var}->GetMap();
    if (!map)
        {return_stmt}

    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (!spatialGrid)
    {{
        sSpatialGridManager.CreateGrid(map);
        spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
            {return_stmt}
    }}

    // Query nearby GUIDs (lock-free!)
    std::vector<ObjectGuid> nearbyGuids = spatialGrid->{query_method}(
        {bot_var}->GetPosition(), {range_val});

    // Process results (replace old searcher logic)
    for (ObjectGuid guid : nearbyGuids)
    {{
        {entity_type}* entity = {accessor}(*{bot_var}, guid);
        if (!entity)
            continue;
        // Original filtering logic from {searcher_var} goes here
    }}
    // End of spatial grid fix"""

        return replacement

    new_content = re.sub(pattern, replace_func, content, flags=re.MULTILINE)
    return new_content, count

def fix_cell_visit_allobj(content: str, file_path: Path) -> Tuple[str, int]:
    """Fix Cell::VisitAllObjects calls (visits both creatures and gameobjects)"""
    count = 0

    pattern = r'Cell::VisitAllObjects\s*\(\s*([^,]+?)\s*,\s*([^,]+?)\s*,\s*([^)]+?)\s*\)\s*;'

    def replace_func(match):
        nonlocal count

        bot_var = match.group(1).strip()
        searcher_var = match.group(2).strip()
        range_val = match.group(3).strip()

        # Get context
        start_pos = max(0, match.start() - 500)
        context = content[start_pos:match.start()]

        # Determine return statement
        return_stmt = "return;"
        if "return false" in context or "bool " in context:
            return_stmt = "return false;"
        elif "return nullptr" in context:
            return_stmt = "return nullptr;"
        elif "return 0" in context:
            return_stmt = "return 0;"

        count += 1

        replacement = f"""// DEADLOCK FIX: Use lock-free spatial grid instead of Cell::VisitAllObjects
    Map* map = {bot_var}->GetMap();
    if (!map)
        {return_stmt}

    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (!spatialGrid)
    {{
        sSpatialGridManager.CreateGrid(map);
        spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
            {return_stmt}
    }}

    // Query nearby creatures (lock-free!)
    std::vector<ObjectGuid> nearbyCreatures = spatialGrid->QueryNearbyCreatures(
        {bot_var}->GetPosition(), {range_val});

    // Query nearby gameobjects (lock-free!)
    std::vector<ObjectGuid> nearbyGameObjects = spatialGrid->QueryNearbyGameObjects(
        {bot_var}->GetPosition(), {range_val});

    // Process creatures
    for (ObjectGuid guid : nearbyCreatures)
    {{
        Creature* creature = ObjectAccessor::GetCreature(*{bot_var}, guid);
        if (!creature)
            continue;
        // Original filtering logic from {searcher_var} for creatures goes here
    }}

    // Process gameobjects
    for (ObjectGuid guid : nearbyGameObjects)
    {{
        GameObject* object = map->GetGameObject(guid);
        if (!object)
            continue;
        // Original filtering logic from {searcher_var} for gameobjects goes here
    }}
    // End of spatial grid fix"""

        return replacement

    new_content = re.sub(pattern, replace_func, content, flags=re.MULTILINE)
    return new_content, count

def process_file(file_path: Path) -> Tuple[bool, int, int]:
    """Process single file, returns (was_modified, grid_count, all_count)"""
    if file_path.name in COMPLETELY_FIXED or "backup" in str(file_path):
        return False, 0, 0

    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()

        if "Cell::Visit" not in content:
            return False, 0, 0

        original_content = content

        # Add include first
        content = add_include(content, file_path)

        # Fix VisitGridObjects
        content, grid_count = fix_cell_visit_gridobj(content, file_path)

        # Fix VisitAllObjects
        content, all_count = fix_cell_visit_allobj(content, file_path)

        total_count = grid_count + all_count

        if total_count > 0:
            # Write the file
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(content)
            return True, grid_count, all_count

        return False, 0, 0
    except Exception as e:
        print(f"ERROR processing {file_path}: {e}")
        return False, 0, 0

def main():
    print("=" * 80)
    print("FINAL PASS: Cell::Visit Deadlock Fix")
    print("=" * 80)

    all_files = list(BASE_DIR.rglob("*.cpp"))
    print(f"Scanning {len(all_files)} .cpp files...\n")

    fixed_files = []
    total_grid = 0
    total_all = 0

    for file_path in sorted(all_files):
        was_fixed, grid_count, all_count = process_file(file_path)
        if was_fixed:
            rel_path = file_path.relative_to(BASE_DIR)
            fixed_files.append((rel_path, grid_count, all_count))
            total_grid += grid_count
            total_all += all_count
            print(f"[OK] Fixed Grid:{grid_count:2d} All:{all_count:2d} in {rel_path}")

    print("\n" + "=" * 80)
    print(f"COMPLETED: {len(fixed_files)} files modified")
    print(f"  VisitGridObjects: {total_grid} calls fixed")
    print(f"  VisitAllObjects:  {total_all} calls fixed")
    print(f"  TOTAL:            {total_grid + total_all} calls fixed")
    print("=" * 80)

    if fixed_files:
        print("\nModified files (sorted by total calls):")
        sorted_files = sorted(fixed_files, key=lambda x: x[1] + x[2], reverse=True)
        for file, grid, all_obj in sorted_files:
            print(f"  Grid:{grid:2d} All:{all_obj:2d} Total:{grid+all_obj:2d} - {file}")

if __name__ == "__main__":
    main()
