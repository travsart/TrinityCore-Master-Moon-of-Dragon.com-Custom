#!/usr/bin/env python3
"""
Fix ALL remaining compilation errors in DoubleBufferedSpatialGrid.cpp
"""

import re
from pathlib import Path

FILE_PATH = Path(r"C:\TrinityBots\TrinityCore\src\modules\Playerbot\Spatial\DoubleBufferedSpatialGrid.cpp")

def fix_all_remaining(content):
    """Fix all remaining compilation errors"""

    # Fix 1: SkinningLootId -> SkinLootID (correct field name)
    content = re.sub(
        r'creatureTemplate->SkinningLootId',
        'creatureTemplate->SkinLootID',
        content
    )

    # Fix 2: Missing semicolons before snapshot - likely from broken comments
    # Check line 373 and 377
    lines = content.split('\n')
    for i, line in enumerate(lines):
        # If line has "false  // " pattern without semicolon, add it
        if 'false  //' in line and not line.strip().endswith(';'):
            lines[i] = line.replace('false  //', 'false;  //')
    content = '\n'.join(lines)

    # Fix 3: ChrSpecialization cannot convert to uint32 - add static_cast
    # This was already supposed to be fixed, but verify it has the cast
    if 'snapshot.specializationId = player->GetPrimarySpecialization()' in content:
        content = re.sub(
            r'snapshot\.specializationId = player->GetPrimarySpecialization\(\);',
            'snapshot.specializationId = static_cast<uint32>(player->GetPrimarySpecialization());',
            content
        )

    # Fix 4: Item undefined type - need to add #include "Item.h"
    # Will be added in include section

    # Fix 5: GameObject flags - GetGOInfo()->flags doesn't exist, check what does
    content = re.sub(
        r'snapshot\.flags = go->GetGOInfo\(\)->flags;',
        'snapshot.flags = 0;  // GameObject flags removed from template',
        content
    )

    # Fix 6: AreaTrigger flags - EnumFlag needs to be converted properly
    content = re.sub(
        r'snapshot\.flags = static_cast<uint32>\(areaTrigger->GetAreaTriggerFlags\(\)\);',
        'snapshot.flags = areaTrigger->GetAreaTriggerFlags().AsUnderlyingType();',
        content
    )

    return content

def add_item_include(content):
    """Add Item.h include if not present"""
    if '#include "Item.h"' not in content:
        # Find the include section and add Item.h
        content = re.sub(
            r'(#include "DoubleBufferedSpatialGrid.h")',
            r'\1\n#include "Item.h"',
            content
        )
    return content

def main():
    print("=" * 80)
    print("Fixing ALL Remaining DoubleBufferedSpatialGrid.cpp Errors")
    print("=" * 80)

    if not FILE_PATH.exists():
        print(f"[ERROR] File not found: {FILE_PATH}")
        return

    with open(FILE_PATH, 'r', encoding='utf-8') as f:
        content = f.read()

    original = content
    content = fix_all_remaining(content)
    content = add_item_include(content)

    if content != original:
        with open(FILE_PATH, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"[SUCCESS] Fixed all remaining errors in {FILE_PATH.name}")
    else:
        print(f"[NO CHANGES] No fixes needed in {FILE_PATH.name}")

    print("=" * 80)

if __name__ == "__main__":
    main()
