#!/usr/bin/env python3
"""
Fix duplicate snapshot_entity declarations within the same function scope.
Pattern:
    const auto* snapshot_entity = SpatialGridQueryHelpers::FindXXX(...);
    Creature* entity = nullptr;
    if (snapshot_entity) { ... entity = ObjectAccessor::GetXXX(...); }
    const auto* snapshot_entity = SpatialGridQueryHelpers::FindXXX(...);  <-- REMOVE THIS
    entity = nullptr;  <-- REMOVE
    if (snapshot_entity) { ... entity = ObjectAccessor::GetXXX(...); }  <-- REMOVE
"""

import re
from pathlib import Path

BASE_DIR = Path(r"C:\TrinityBots\TrinityCore\src\modules\Playerbot")

PROBLEM_FILES = [
    "AI/ClassAI/Warlocks/DestructionSpecialization.cpp",
    "AI/ClassAI/Shamans/ShamanAI.cpp",
    "AI/ClassAI/Hunters/HunterAI.cpp",
    "AI/ClassAI/Warriors/WarriorSpecialization.cpp",
    "AI/ClassAI/Monks/MonkSpecialization.cpp",
    "AI/ClassAI/Druids/DruidSpecialization.cpp",
    "AI/ClassAI/Evokers/EvokerAI.cpp",
    "AI/ClassAI/Warlocks/WarlockAI.cpp",
]

def remove_duplicate_snapshot_blocks(content):
    """Remove duplicate snapshot_entity blocks"""

    # Pattern: Find duplicate snapshot_entity declarations
    # First occurrence is at line N, second at line N+X
    pattern = re.compile(
        r'(const\s+auto\*\s+snapshot_entity\s*=\s*SpatialGridQueryHelpers::(Find\w+)\([^)]+\);)\s*\n'
        r'(\s*Creature\*\s+entity\s*=\s*nullptr;\s*\n'
        r'\s*if\s*\(\s*snapshot_entity\s*\)\s*\n'
        r'\s*\{\s*\n'
        r'\s*entity\s*=\s*ObjectAccessor::(GetUnit|GetCreature)\([^)]+,[^)]+\);\s*\n'
        r'\s*\}\s*\n)'
        r'(\s*const\s+auto\*\s+snapshot_entity\s*=\s*SpatialGridQueryHelpers::\2\([^)]+\);'  # Duplicate!
        r'\s*\n'
        r'\s*entity\s*=\s*nullptr;\s*\n'  # Duplicate!
        r'\s*if\s*\(\s*snapshot_entity\s*\)\s*\n'  # Duplicate!
        r'\s*\{\s*\n'
        r'\s*entity\s*=\s*ObjectAccessor::\4\([^)]+,[^)]+\);\s*\n'  # Duplicate!
        r'\s*\})',
        re.MULTILINE | re.DOTALL
    )

    # Keep first occurrence, remove second
    content = pattern.sub(r'\1\3', content)

    return content

def process_file(filepath):
    """Fix one file"""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
    except Exception as e:
        print(f"[ERROR] Could not read {filepath}: {e}")
        return False

    original = content
    content = remove_duplicate_snapshot_blocks(content)

    if content != original:
        try:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(content)
            return True
        except Exception as e:
            print(f"[ERROR] Could not write {filepath}: {e}")
            return False
    return False

def main():
    """Main execution"""
    print("=" * 80)
    print("Fixing Duplicate snapshot_entity Declarations")
    print("=" * 80)

    fixed = 0
    for rel_path in PROBLEM_FILES:
        filepath = BASE_DIR / rel_path
        if not filepath.exists():
            print(f"[SKIP] {rel_path} - not found")
            continue

        if process_file(filepath):
            print(f"[FIXED] {rel_path}")
            fixed += 1
        else:
            print(f"[NO CHANGE] {rel_path}")

    print("=" * 80)
    print(f"Complete: {fixed} files fixed")
    print("=" * 80)

if __name__ == "__main__":
    main()
