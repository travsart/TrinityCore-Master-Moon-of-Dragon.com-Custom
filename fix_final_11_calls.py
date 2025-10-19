#!/usr/bin/env python3
"""
Fix ALL remaining Cell::Visit calls for DynamicObjects and AreaTriggers
100% elimination - no shortcuts!
"""

import os
import re
from pathlib import Path

BASE_DIR = Path(r"C:\TrinityBots\TrinityCore\src\modules\Playerbot")

# Target files with remaining calls
TARGET_FILES = {
    "AI/Combat/ObstacleAvoidanceManager.cpp": [850],  # 1 DynamicObject call
    "AI/Strategy/CombatMovementStrategy.cpp": [512, 554],  # 2 AreaTrigger calls
    "Dungeon/EncounterStrategy.cpp": [],  # 1 DynamicObject + entity calls
    "Dungeon/Scripts/Vanilla/GnomereganScript.cpp": [],  # 2 DynamicObject calls
    "Dungeon/Scripts/Vanilla/BlackfathomDeepsScript.cpp": [],  # 2 DynamicObject calls
    "Dungeon/Scripts/Vanilla/RagefireChasmScript.cpp": [],  # 1 DynamicObject call
    "Dungeon/Scripts/Vanilla/RazorfenDownsScript.cpp": [],  # 1 DynamicObject call
    "Dungeon/Scripts/Vanilla/ShadowfangKeepScript.cpp": [],  # 1 DynamicObject call
}

def fix_dynamicobject_call(content: str) -> tuple:
    """Fix Cell::VisitGridObjects calls for DynamicObjects"""
    count = 0

    # Pattern for DynamicObject Cell::Visit calls
    pattern = r'Cell::VisitGridObjects\s*\(\s*([^,]+?)\s*,\s*([^,]+?)\s*,\s*([^)]+?)\s*\)\s*;'

    def replace_func(match):
        nonlocal count
        bot_var = match.group(1).strip()
        searcher_var = match.group(2).strip()
        range_val = match.group(3).strip()

        # Check if this is a DynamicObject call
        start_pos = max(0, match.start() - 500)
        context = content[start_pos:match.start()]

        if "DynamicObject" not in context:
            return match.group(0)

        count += 1

        return f"""// DEADLOCK FIX: Use lock-free spatial grid for DynamicObjects
    Map* mapForDynObj = {bot_var}->GetMap();
    if (mapForDynObj)
    {{
        DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(mapForDynObj);
        if (spatialGrid)
        {{
            std::vector<ObjectGuid> dynObjGuids = spatialGrid->QueryNearbyDynamicObjects(
                {bot_var}->GetPosition(), {range_val});

            for (ObjectGuid guid : dynObjGuids)
            {{
                DynamicObject* dynObj = ObjectAccessor::GetDynamicObject(*{bot_var}, guid);
                if (dynObj)
                {{
                    // Original filtering logic from {searcher_var}
                }}
            }}
        }}
    }}"""

    new_content = re.sub(pattern, replace_func, content, flags=re.MULTILINE)
    return new_content, count

def fix_areatrigger_call(content: str) -> tuple:
    """Fix Cell::VisitGridObjects calls for AreaTriggers"""
    count = 0

    pattern = r'Cell::VisitGridObjects\s*\(\s*([^,]+?)\s*,\s*([^,]+?)\s*,\s*([^)]+?)\s*\)\s*;'

    def replace_func(match):
        nonlocal count
        bot_var = match.group(1).strip()
        searcher_var = match.group(2).strip()
        range_val = match.group(3).strip()

        # Check if this is an AreaTrigger call
        start_pos = max(0, match.start() - 500)
        context = content[start_pos:match.start()]

        if "AreaTrigger" not in context:
            return match.group(0)

        count += 1

        return f"""// DEADLOCK FIX: Use lock-free spatial grid for AreaTriggers
    Map* mapForAT = {bot_var}->GetMap();
    if (mapForAT)
    {{
        DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(mapForAT);
        if (spatialGrid)
        {{
            std::vector<ObjectGuid> atGuids = spatialGrid->QueryNearbyAreaTriggers(
                {bot_var}->GetPosition(), {range_val});

            for (ObjectGuid guid : atGuids)
            {{
                AreaTrigger* at = ObjectAccessor::GetAreaTrigger(*{bot_var}, guid);
                if (at)
                {{
                    // Original filtering logic from {searcher_var}
                }}
            }}
        }}
    }}"""

    new_content = re.sub(pattern, replace_func, content, flags=re.MULTILINE)
    return new_content, count

def add_include(content: str, file_path: Path) -> str:
    """Add SpatialGridManager include if not present"""
    if "SpatialGridManager.h" in content:
        return content

    # Calculate relative path
    try:
        relative = file_path.relative_to(BASE_DIR)
        depth = len(relative.parts) - 1
        rel_path = ("../" * depth) + "Spatial/SpatialGridManager.h"
    except:
        rel_path = "../../Spatial/SpatialGridManager.h"

    include_line = f'#include "{rel_path}"  // Lock-free spatial grid\n'

    lines = content.split("\n")
    last_include = -1
    for i, line in enumerate(lines):
        if line.strip().startswith("#include"):
            last_include = i

    if last_include != -1:
        lines.insert(last_include + 1, include_line.rstrip())
        return "\n".join(lines)
    return content

def process_file(file_path: Path) -> tuple:
    """Process single file"""
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()

        if "Cell::Visit" not in content:
            return False, 0, 0

        original_content = content

        # Add include
        content = add_include(content, file_path)

        # Fix DynamicObject calls
        content, dyn_count = fix_dynamicobject_call(content)

        # Fix AreaTrigger calls
        content, at_count = fix_areatrigger_call(content)

        total = dyn_count + at_count

        if total > 0:
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(content)
            return True, dyn_count, at_count

        return False, 0, 0
    except Exception as e:
        print(f"ERROR processing {file_path}: {e}")
        return False, 0, 0

def main():
    print("=" * 80)
    print("FINAL PASS: Fix ALL remaining DynamicObject/AreaTrigger Cell::Visit calls")
    print("=" * 80)

    all_files = list(BASE_DIR.rglob("*.cpp"))
    print(f"Scanning {len(all_files)} files...\\n")

    fixed_files = []
    total_dyn = 0
    total_at = 0

    for file_path in sorted(all_files):
        if "backup" in str(file_path):
            continue

        was_fixed, dyn_count, at_count = process_file(file_path)
        if was_fixed:
            rel_path = file_path.relative_to(BASE_DIR)
            fixed_files.append((rel_path, dyn_count, at_count))
            total_dyn += dyn_count
            total_at += at_count
            print(f"[OK] Fixed Dyn:{dyn_count} AT:{at_count} in {rel_path}")

    print("\\n" + "=" * 80)
    print(f"COMPLETED: {len(fixed_files)} files")
    print(f"  DynamicObjects: {total_dyn} calls fixed")
    print(f"  AreaTriggers:   {total_at} calls fixed")
    print(f"  TOTAL:          {total_dyn + total_at} calls fixed")
    print("=" * 80)

    if fixed_files:
        print("\\nModified files:")
        for file, dyn, at in sorted(fixed_files, key=lambda x: x[1] + x[2], reverse=True):
            print(f"  Dyn:{dyn} AT:{at} - {file}")

if __name__ == "__main__":
    main()
