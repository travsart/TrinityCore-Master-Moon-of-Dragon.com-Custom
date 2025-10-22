#!/usr/bin/env python3
"""
Fix all Debug build errors from comprehensive Cell::Visit batch script
Fixes:
1. Missing ObjectAccessor includes
2. Wrong return statements (void vs bool vs other types)
3. Duplicate variable declarations
4. Map::GetGameObject wrong arguments
"""

import re
from pathlib import Path
from typing import Dict, Set, Tuple

BASE_DIR = Path(r"C:\TrinityBots\TrinityCore\src\modules\Playerbot")

# Files that need ObjectAccessor include
NEED_OBJECTACCESSOR = {
    "AI/Actions/Action.cpp",
    "AI/ClassAI/Monks/MonkAI.cpp",
}

# Files with duplicate spatial grid blocks - need complete rewrite
DUPLICATE_BLOCKS = {
    "Quest/QuestPickup.cpp",
    "Professions/GatheringManager.cpp",
}

# Files with wrong return types in spatial grid blocks
WRONG_RETURNS = {
    "AI/ClassAI/Shamans/ElementalSpecialization.cpp": "std::vector<Unit*>",
    "AI/ClassAI/DemonHunters/DemonHunterAI.cpp": ["std::vector<Unit*>", "uint32"],
    "AI/ClassAI/Hunters/HunterAI.cpp": ["bool", "Unit*", "uint32"],
    "AI/ClassAI/Warlocks/WarlockAI.cpp": ["Unit*", "uint32"],
    "AI/Combat/TargetScanner.cpp": "Unit*",
    "Lifecycle/DeathRecoveryManager.cpp": "Creature*",
    "Interaction/Core/InteractionManager.cpp": "Creature*",
    "AI/ClassAI/Evokers/EvokerAI.cpp": "std::vector<Unit*>",
}

def add_objectaccessor_include(file_path: Path) -> bool:
    """Add ObjectAccessor include if missing"""
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()

        if 'ObjectAccessor.h' in content:
            return False

        # Find last #include and add after it
        lines = content.split('\n')
        last_include = -1
        for i, line in enumerate(lines):
            if line.strip().startswith('#include'):
                last_include = i

        if last_include >= 0:
            lines.insert(last_include + 1, '#include "ObjectAccessor.h"')

            with open(file_path, 'w', encoding='utf-8') as f:
                f.write('\n'.join(lines))
            return True

        return False
    except Exception as e:
        print(f"ERROR adding include to {file_path}: {e}")
        return False

def fix_map_getgameobject(content: str) -> str:
    """Fix Map::GetGameObject calls with wrong arguments"""
    # Pattern: map->GetGameObject(*bot, guid) or similar
    content = re.sub(
        r'->GetGameObject\(\*[^,]+,\s*([^)]+)\)',
        r'->GetGameObject(\1)',
        content
    )
    return content

def fix_duplicate_spatial_blocks(content: str) -> str:
    """Remove duplicate spatial grid declaration blocks"""
    # Find and remove duplicate Map* map declarations
    lines = content.split('\n')
    result = []
    seen_map_decl = False
    skip_until_query = False

    for i, line in enumerate(lines):
        # Check if this is a duplicate Map* map declaration
        if 'Map* map = ' in line and 'spatial grid' in ''.join(lines[max(0, i-3):i]).lower():
            if seen_map_decl:
                skip_until_query = True
                continue
            else:
                seen_map_decl = True

        # Skip duplicate spatial grid setup until we hit the query
        if skip_until_query:
            if 'QueryNearby' in line:
                skip_until_query = False
                # Change variable name to avoid duplicate
                line = re.sub(r'nearbyGuids', 'nearbyGuids2', line)
            else:
                continue

        result.append(line)

    return '\n'.join(result)

def fix_return_statements(content: str, return_type: str) -> str:
    """Fix return statements based on function return type"""
    # Pattern: return; // Adjust return value as needed
    if return_type == "void":
        content = re.sub(
            r'return\s+false;(\s*//\s*Adjust.*)?',
            'return;',
            content
        )
        content = re.sub(
            r'return\s+true;(\s*//\s*Adjust.*)?',
            'return;',
            content
        )
        content = re.sub(
            r'return\s+\{\};(\s*//\s*Adjust.*)?',
            'return;',
            content
        )
    elif 'vector' in return_type.lower():
        content = re.sub(
            r'return\s*;(\s*//\s*Adjust.*)?',
            'return {};',
            content
        )
        content = re.sub(
            r'return\s+false;(\s*//\s*Adjust.*)?',
            'return {};',
            content
        )
    elif return_type == "bool":
        content = re.sub(
            r'return\s*;(\s*//\s*Adjust.*)?',
            'return false;',
            content
        )
    elif '*' in return_type:  # Pointer type
        content = re.sub(
            r'return\s*;(\s*//\s*Adjust.*)?',
            'return nullptr;',
            content
        )
        content = re.sub(
            r'return\s+false;(\s*//\s*Adjust.*)?',
            'return nullptr;',
            content
        )
    elif 'uint' in return_type.lower() or 'int' in return_type.lower():
        content = re.sub(
            r'return\s*;(\s*//\s*Adjust.*)?',
            'return 0;',
            content
        )
        content = re.sub(
            r'return\s+false;(\s*//\s*Adjust.*)?',
            'return 0;',
            content
        )

    return content

def main():
    print("=" * 80)
    print("FIXING ALL DEBUG BUILD ERRORS")
    print("=" * 80)

    fixed_files = []

    # Step 1: Add ObjectAccessor includes
    print("\n[1/4] Adding missing ObjectAccessor includes...")
    for rel_path in NEED_OBJECTACCESSOR:
        file_path = BASE_DIR / rel_path
        if file_path.exists():
            if add_objectaccessor_include(file_path):
                fixed_files.append((rel_path, "Added ObjectAccessor include"))
                print(f"  [OK] {rel_path}")

    # Step 2: Fix Map::GetGameObject calls
    print("\n[2/4] Fixing Map::GetGameObject arguments...")
    for cpp_file in BASE_DIR.rglob("*.cpp"):
        if 'backup' in str(cpp_file):
            continue

        try:
            with open(cpp_file, 'r', encoding='utf-8', errors='ignore') as f:
                original = f.read()

            fixed = fix_map_getgameobject(original)

            if fixed != original:
                with open(cpp_file, 'w', encoding='utf-8') as f:
                    f.write(fixed)
                rel_path = cpp_file.relative_to(BASE_DIR)
                fixed_files.append((rel_path, "Fixed GetGameObject args"))
                print(f"  [OK] {rel_path}")
        except Exception as e:
            print(f"  [ERR] {cpp_file}: {e}")

    # Step 3: Fix duplicate spatial blocks
    print("\n[3/4] Fixing duplicate spatial grid blocks...")
    for rel_path in DUPLICATE_BLOCKS:
        file_path = BASE_DIR / rel_path
        if not file_path.exists():
            continue

        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()

            fixed = fix_duplicate_spatial_blocks(content)
            fixed = fix_map_getgameobject(fixed)

            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(fixed)

            fixed_files.append((rel_path, "Fixed duplicate blocks"))
            print(f"  [OK] {rel_path}")
        except Exception as e:
            print(f"  [ERR] {rel_path}: {e}")

    # Step 4: Fix return statements
    print("\n[4/4] Fixing return statements...")
    for rel_path, return_types in WRONG_RETURNS.items():
        file_path = BASE_DIR / rel_path
        if not file_path.exists():
            continue

        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()

            # Handle multiple return types
            if isinstance(return_types, list):
                for ret_type in return_types:
                    content = fix_return_statements(content, ret_type)
            else:
                content = fix_return_statements(content, return_types)

            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(content)

            fixed_files.append((rel_path, "Fixed return statements"))
            print(f"  [OK] {rel_path}")
        except Exception as e:
            print(f"  [ERR] {rel_path}: {e}")

    print("\n" + "=" * 80)
    print(f"COMPLETED: {len(fixed_files)} files fixed")
    print("=" * 80)

    if fixed_files:
        print("\nFixed files:")
        for file, fix in fixed_files:
            print(f"  - {file}: {fix}")

if __name__ == "__main__":
    main()
