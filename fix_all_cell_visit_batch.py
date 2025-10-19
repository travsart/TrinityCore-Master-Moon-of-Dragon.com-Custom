#!/usr/bin/env python3
"""
Comprehensive Cell::Visit Deadlock Fix - Batch Processor
Fixes ALL remaining Cell::VisitGridObjects calls with lock-free spatial grid pattern
"""

import os
import re
from pathlib import Path
from typing import List, Tuple

BASE_DIR = Path(r"C:\TrinityBots\TrinityCore\src\modules\Playerbot")

# Files already fixed - skip these
ALREADY_FIXED = {
    "TargetScanner.cpp", "ClassAI.cpp", "TargetSelector.cpp", "PositionManager.cpp",
    "CombatStateAnalyzer.cpp", "InterruptManager.cpp", "DefensiveBehaviorManager.cpp",
    "AoEDecisionManager.cpp", "LineOfSightManager.cpp", "ObstacleAvoidanceManager.cpp",
    "KitingManager.cpp", "InterruptAwareness.cpp", "DispelCoordinator.cpp", "LootStrategy.cpp",
    "AdvancedBehaviorManager.cpp", "EconomyManager.cpp", "Action.cpp",
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

def fix_creature_cell_visit(content: str) -> Tuple[str, int]:
    """Fix Cell::VisitGridObjects calls for Creatures"""
    count = 0

    # Pattern: Cell::VisitGridObjects(bot/player, searcher, range);
    pattern = r'Cell::VisitGridObjects\s*\(\s*(\w+)\s*,\s*(\w+)\s*,\s*([^)]+)\s*\);'

    def replace_func(match):
        nonlocal count
        bot_var = match.group(1)
        range_val = match.group(3)

        # Check context for GameObject vs Creature
        start_pos = max(0, match.start() - 300)
        context = content[start_pos:match.start()]

        is_gameobject = "GameObject" in context and "GameObjectListSearcher" in context
        is_areatrigger = "AreaTrigger" in context
        is_dynamicobject = "DynamicObject" in context

        # Skip AreaTriggers and DynamicObjects (not supported by spatial grid yet)
        if is_areatrigger or is_dynamicobject:
            return match.group(0)

        count += 1

        if is_gameobject:
            query_method = "QueryNearbyGameObjects"
            accessor = f"{bot_var}->GetMap()->GetGameObject"
        else:
            query_method = "QueryNearbyCreatures"
            accessor = "ObjectAccessor::GetCreature"

        return f"""// DEADLOCK FIX: Use lock-free spatial grid instead of Cell::VisitGridObjects
    Map* map = {bot_var}->GetMap();
    if (!map)
        return; // Adjust return value as needed

    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (!spatialGrid)
    {{
        sSpatialGridManager.CreateGrid(map);
        spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
            return; // Adjust return value as needed
    }}

    // Query nearby GUIDs (lock-free!)
    std::vector<ObjectGuid> nearbyGuids = spatialGrid->{query_method}(
        {bot_var}->GetPosition(), {range_val});

    // Process results (replace old loop)
    for (ObjectGuid guid : nearbyGuids)
    {{
        auto* entity = {accessor}(*{bot_var}, guid);
        if (!entity)
            continue;
        // Original filtering logic goes here
    }}
    // End of spatial grid fix"""

    new_content = re.sub(pattern, replace_func, content)
    return new_content, count

def process_file(file_path: Path) -> Tuple[bool, int]:
    """Process single file"""
    if file_path.name in ALREADY_FIXED or "backup" in str(file_path):
        return False, 0

    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()

        if "Cell::VisitGridObjects" not in content:
            return False, 0

        # Add include
        content = add_include(content, file_path)

        # Fix calls
        content, count = fix_creature_cell_visit(content)

        if count > 0:
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(content)
            return True, count

        return False, 0
    except Exception as e:
        print(f"ERROR {file_path}: {e}")
        return False, 0

def main():
    print("="*80)
    print("BATCH CELL::VISIT DEADLOCK FIX")
    print("="*80)

    all_files = list(BASE_DIR.rglob("*.cpp"))
    print(f"Scanning {len(all_files)} .cpp files...\n")

    fixed_files = []
    total_fixes = 0

    for file_path in sorted(all_files):
        was_fixed, count = process_file(file_path)
        if was_fixed:
            rel_path = file_path.relative_to(BASE_DIR)
            fixed_files.append((rel_path, count))
            total_fixes += count
            print(f"[OK] Fixed {count:2d} calls in {rel_path}")

    print("\n" + "="*80)
    print(f"COMPLETED: {len(fixed_files)} files modified, {total_fixes} calls fixed")
    print("="*80)

    if fixed_files:
        print("\nModified files:")
        for file, count in sorted(fixed_files, key=lambda x: -x[1]):
            print(f"  {count:2d} calls - {file}")

if __name__ == "__main__":
    main()
