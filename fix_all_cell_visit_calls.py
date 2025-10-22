#!/usr/bin/env python3
"""
Automated Cell::Visit Deadlock Fix Script
Systematically replaces all Cell::VisitGridObjects and Cell::VisitAllObjects calls
with lock-free sSpatialGridManager pattern across the entire Playerbot codebase.
"""

import os
import re
from pathlib import Path
from typing import List, Tuple

# Base directory
BASE_DIR = Path(r"C:\TrinityBots\TrinityCore\src\modules\Playerbot")

# Files to exclude (already fixed or backup files)
EXCLUDE_FILES = {
    "TargetScanner.cpp",
    "ClassAI.cpp",
    "TargetSelector.cpp",
    "PositionManager.cpp",
    "CombatStateAnalyzer.cpp",
    "InterruptManager.cpp",
    "DefensiveBehaviorManager.cpp",
    "AoEDecisionManager.cpp",
    "LineOfSightManager.cpp",
    "ObstacleAvoidanceManager.cpp",
    "KitingManager.cpp",
    "InterruptAwareness.cpp",
    "DispelCoordinator.cpp",
    "LootStrategy.cpp",
    "SpatialGridManager.cpp",  # Implementation file
    "SpatialGridManager.h",     # Definition file
    "DoubleBufferedSpatialGrid.cpp",
    "DoubleBufferedSpatialGrid.h",
}

# Skip files with these substrings (backups)
SKIP_PATTERNS = ["backup", ".phase2_backup", "~", ".bak"]


def should_skip_file(file_path: Path) -> bool:
    """Check if file should be skipped."""
    if file_path.name in EXCLUDE_FILES:
        return True
    for pattern in SKIP_PATTERNS:
        if pattern in str(file_path):
            return True
    return False


def calculate_relative_path(file_path: Path) -> str:
    """
    Calculate relative path from file to Spatial/SpatialGridManager.h

    Examples:
        AI/ClassAI/Warriors/WarriorAI.cpp -> ../../../Spatial/SpatialGridManager.h
        AI/Combat/TargetScanner.cpp -> ../../Spatial/SpatialGridManager.h
        Quest/QuestCompletion.cpp -> ../Spatial/SpatialGridManager.h
    """
    try:
        relative_to_playerbot = file_path.relative_to(BASE_DIR)
        depth = len(relative_to_playerbot.parts) - 1  # Subtract 1 for filename itself
        return ("../" * depth) + "Spatial/SpatialGridManager.h"
    except ValueError:
        # File not in BASE_DIR, use default
        return "../../Spatial/SpatialGridManager.h"


def add_spatial_grid_include(content: str, file_path: Path) -> str:
    """Add SpatialGridManager include if not present."""
    if "SpatialGridManager.h" in content:
        return content  # Already has include

    relative_path = calculate_relative_path(file_path)
    include_line = f'#include "{relative_path}"  // Lock-free spatial grid for deadlock fix\n'

    # Find last #include line
    lines = content.split("\n")
    last_include_idx = -1
    for i, line in enumerate(lines):
        if line.strip().startswith("#include"):
            last_include_idx = i

    if last_include_idx != -1:
        lines.insert(last_include_idx + 1, include_line.rstrip())
        return "\n".join(lines)

    return content  # No includes found, skip


def fix_cell_visit_creatures(content: str) -> Tuple[str, int]:
    """
    Replace Cell::VisitGridObjects/VisitAllObjects for Creatures with spatial grid pattern.
    Returns (modified_content, num_replacements)
    """
    replacements = 0

    # Pattern 1: Cell::VisitGridObjects(bot/player, searcher, range)
    # Pattern 2: Cell::VisitAllObjects(bot/player, searcher, range)
    pattern = r'Cell::(VisitGridObjects|VisitAllObjects)\s*\(\s*(\w+)\s*,\s*(\w+)\s*,\s*([^)]+)\s*\);'

    def replacement_func(match):
        nonlocal replacements
        method = match.group(1)  # VisitGridObjects or VisitAllObjects
        bot_var = match.group(2)  # bot, _bot, player, _observer, etc.
        searcher = match.group(3)  # searcher variable (unused in new pattern)
        range_val = match.group(4)  # range value

        # Check if this is likely a Creature search (not AreaTrigger or DynamicObject)
        # We'll handle Creatures and GameObjects, skip AreaTriggers/DynamicObjects
        context_before = content[max(0, match.start() - 500):match.start()]

        # Skip if it's an AreaTrigger or DynamicObject search
        if "AreaTrigger" in context_before or "DynamicObject" in context_before:
            return match.group(0)  # Keep original

        # Determine query type based on context
        query_type = "QueryNearbyCreatures"
        object_accessor = "ObjectAccessor::GetUnit"

        if "GameObject" in context_before:
            query_type = "QueryNearbyGameObjects"
            object_accessor = f"{bot_var}->GetMap()->GetGameObject"

        replacements += 1

        # Generate replacement code
        return f"""// DEADLOCK FIX: Use lock-free spatial grid instead of Cell::{method}
    Map* map = {bot_var}->GetMap();
    if (!map)
        return;  // TODO: Adjust return value based on function signature

    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (!spatialGrid)
    {{
        sSpatialGridManager.CreateGrid(map);
        spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
            return;  // TODO: Adjust return value based on function signature
    }}

    // Query nearby GUIDs (lock-free!)
    std::vector<ObjectGuid> nearbyGuids = spatialGrid->{query_type}(
        {bot_var}->GetPosition(), {range_val});

    // Resolve GUIDs to pointers
    for (ObjectGuid guid : nearbyGuids)
    {{
        auto* entity = {object_accessor}(*{bot_var}, guid);
        if (!entity)
            continue;

        // TODO: Add filtering logic from original searcher here
        // Original searcher: {searcher}
    }}"""

    new_content = re.sub(pattern, replacement_func, content, flags=re.MULTILINE)

    return new_content, replacements


def process_file(file_path: Path) -> Tuple[bool, int]:
    """
    Process a single file to fix Cell::Visit calls.
    Returns (was_modified, num_replacements)
    """
    if should_skip_file(file_path):
        return False, 0

    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()

        # Check if file has Cell::Visit calls
        if "Cell::VisitGridObjects" not in content and "Cell::VisitAllObjects" not in content:
            return False, 0

        # Add include
        content = add_spatial_grid_include(content, file_path)

        # Fix Cell::Visit calls
        content, num_replacements = fix_cell_visit_creatures(content)

        if num_replacements > 0:
            # Write back
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(content)
            return True, num_replacements

        return False, 0

    except Exception as e:
        print(f"ERROR processing {file_path}: {e}")
        return False, 0


def main():
    """Main execution function."""
    print("=" * 80)
    print("Cell::Visit Deadlock Fix - Automated Batch Processing")
    print("=" * 80)
    print(f"Base directory: {BASE_DIR}")
    print(f"Excluding {len(EXCLUDE_FILES)} already-fixed files")
    print()

    # Find all .cpp and .h files
    all_files = list(BASE_DIR.rglob("*.cpp")) + list(BASE_DIR.rglob("*.h"))
    print(f"Found {len(all_files)} total files to scan")
    print()

    files_modified = 0
    total_replacements = 0
    modified_files_list = []

    for file_path in sorted(all_files):
        was_modified, num_replacements = process_file(file_path)

        if was_modified:
            files_modified += 1
            total_replacements += num_replacements
            relative_path = file_path.relative_to(BASE_DIR)
            modified_files_list.append((relative_path, num_replacements))
            print(f"✓ Fixed {num_replacements:2d} calls in {relative_path}")

    print()
    print("=" * 80)
    print(f"SUMMARY:")
    print(f"  Files modified: {files_modified}")
    print(f"  Total Cell::Visit calls fixed: {total_replacements}")
    print("=" * 80)

    if modified_files_list:
        print()
        print("Modified files:")
        for file_rel, count in modified_files_list:
            print(f"  - {file_rel} ({count} calls)")

    print()
    print("IMPORTANT: Review generated code for:")
    print("  1. Correct return values (some functions may not return void)")
    print("  2. Original searcher filtering logic (marked with TODO)")
    print("  3. Proper entity type (Creature vs GameObject)")
    print()
    print("Next steps:")
    print("  1. Review changes with: git diff")
    print("  2. Compile with: cd build && cmake --build . --target playerbot")
    print("  3. Fix any compilation errors")
    print("  4. Test with 100 bots for deadlock verification")


if __name__ == "__main__":
    main()
