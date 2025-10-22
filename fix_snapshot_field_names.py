#!/usr/bin/env python3
"""
Fix incorrect snapshot field name references
- currentTarget -> victim (CreatureSnapshot)
- isAlive -> isAlive (already exists, check casing)
- isMineable, isHerbalism, isFishingNode -> need to determine from goType
"""

import re
from pathlib import Path

BASE_DIR = Path(r"C:\TrinityBots\TrinityCore\src\modules\Playerbot")

def fix_target_scanner(filepath):
    """Fix TargetScanner.cpp field names"""
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    original = content

    # Fix: currentTarget -> victim
    content = re.sub(r'->currentTarget\b', '->victim', content)
    content = re.sub(r'\.currentTarget\b', '.victim', content)

    # Note: isAlive already exists in snapshot, might be casing issue
    # Check the actual error line

    if content != original:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        return True
    return False

def fix_gathering_manager(filepath):
    """Fix GatheringManager.cpp - remove non-existent fields, use goType instead"""
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    original = content

    # These fields don't exist - need to check goType instead
    # GameObjectSnapshot has: goType field
    # We need to check GameObject type constants

    # Pattern: if (snapshot.isMineable)
    content = re.sub(
        r'if\s*\(\s*(\w+)\.isMineable\s*\)',
        r'if (\1.goType == 3) // GAMEOBJECT_TYPE_CHEST (ore nodes)',
        content
    )

    # Pattern: if (snapshot.isHerbalism)
    content = re.sub(
        r'if\s*\(\s*(\w+)\.isHerbalism\s*\)',
        r'if (\1.goType == 3) // GAMEOBJECT_TYPE_CHEST (herb nodes)',
        content
    )

    # Pattern: if (snapshot.isFishingNode)
    content = re.sub(
        r'if\s*\(\s*(\w+)\.isFishingNode\s*\)',
        r'if (\1.goType == 25) // GAMEOBJECT_TYPE_FISHINGNODE',
        content
    )

    if content != original:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        return True
    return False

def main():
    """Main execution"""
    print("=" * 80)
    print("Fixing Snapshot Field Name Errors")
    print("=" * 80)

    files = {
        "AI/Combat/TargetScanner.cpp": fix_target_scanner,
        "Professions/GatheringManager.cpp": fix_gathering_manager,
    }

    fixed = 0
    for rel_path, fix_func in files.items():
        filepath = BASE_DIR / rel_path
        if not filepath.exists():
            print(f"[SKIP] {rel_path} - not found")
            continue

        if fix_func(filepath):
            print(f"[FIXED] {rel_path}")
            fixed += 1
        else:
            print(f"[NO CHANGE] {rel_path}")

    print("=" * 80)
    print(f"Complete: {fixed} files fixed")
    print("=" * 80)

if __name__ == "__main__":
    main()
