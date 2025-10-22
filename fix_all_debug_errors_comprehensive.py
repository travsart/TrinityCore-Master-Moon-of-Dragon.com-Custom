#!/usr/bin/env python3
"""
Comprehensive fix for ALL Debug build errors from Cell::Visit batch script
"""

import re
from pathlib import Path

BASE_DIR = Path(r"C:\TrinityBots\TrinityCore\src\modules\Playerbot")

# Files that need ObjectAccessor include
NEED_OBJECTACCESSOR = [
    "AI/ClassAI/Warlocks/AfflictionSpecialization.cpp",
    "AI/ClassAI/Shamans/ElementalSpecialization.cpp",
    "AI/ClassAI/Shamans/EnhancementSpecialization.cpp",
    "AI/Strategy/CombatMovementStrategy.cpp",
]

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
            print(f"  [OK] Added ObjectAccessor to {file_path.name}")
            return True

        return False
    except Exception as e:
        print(f"  [ERR] {file_path}: {e}")
        return False

def fix_void_function_returns(file_path: Path) -> bool:
    """Fix void functions that have 'return false;' or 'return {...};' """
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()

        result = []
        current_function_return_type = None
        modified = False

        for line in lines:
            # Detect function signatures
            func_match = re.match(r'^\s*(void|bool|int|uint32|float|std::\w+|Unit\*|Creature\*|Player\*|Position)\s+\w+::\w+\s*\(', line)
            if func_match:
                current_function_return_type = func_match.group(1)

            # Fix void function returns
            if current_function_return_type == "void":
                if 'return false;' in line:
                    line = line.replace('return false;', 'return;')
                    modified = True
                elif re.search(r'return\s+\{\};', line):
                    line = re.sub(r'return\s+\{\};', 'return;', line)
                    modified = True

            result.append(line)

        if modified:
            with open(file_path, 'w', encoding='utf-8') as f:
                f.writelines(result)
            print(f"  [OK] Fixed void returns in {file_path.name}")
            return True

        return False
    except Exception as e:
        print(f"  [ERR] {file_path}: {e}")
        return False

def fix_non_void_function_returns(file_path: Path) -> bool:
    """Fix non-void functions with missing or wrong returns"""
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()

        # Pattern: functions that should return but have bare 'return;'
        # Fix: return; → return false; (for bool functions)
        # Fix: return; → return {}; (for vector/container functions)
        # Fix: return; → return nullptr; (for pointer functions)

        # For bool functions, check function signature context
        modified = False

        # Fix missing returns in bool functions (lines ending in 'return;' that should be 'return false;')
        # But only if we're in a bool function context
        lines = content.split('\n')
        result = []
        in_bool_function = False
        in_vector_function = False
        in_pointer_function = False
        in_position_function = False

        for i, line in enumerate(lines):
            # Detect bool function signatures
            if re.match(r'^\s*bool\s+\w+::\w+\s*\(', line):
                in_bool_function = True
                in_vector_function = False
                in_pointer_function = False
                in_position_function = False
            # Detect vector function signatures
            elif re.match(r'^\s*std::vector<[^>]+>\s+\w+::\w+\s*\(', line):
                in_vector_function = True
                in_bool_function = False
                in_pointer_function = False
                in_position_function = False
            # Detect pointer function signatures
            elif re.match(r'^\s*(Unit|Creature|Player|GameObject)\*\s+\w+::\w+\s*\(', line):
                in_pointer_function = True
                in_bool_function = False
                in_vector_function = False
                in_position_function = False
            # Detect Position function signatures
            elif re.match(r'^\s*Position\s+\w+::\w+\s*\(', line):
                in_position_function = True
                in_bool_function = False
                in_vector_function = False
                in_pointer_function = False
            # Detect closing brace (end of function)
            elif re.match(r'^\}', line):
                in_bool_function = False
                in_vector_function = False
                in_pointer_function = False
                in_position_function = False

            # Fix returns based on context
            if in_bool_function and re.search(r'^\s*return\s*;\s*$', line):
                line = re.sub(r'return\s*;', 'return false;', line)
                modified = True
            elif in_vector_function and re.search(r'^\s*return\s*;\s*$', line):
                line = re.sub(r'return\s*;', 'return {};', line)
                modified = True
            elif in_pointer_function and re.search(r'^\s*return\s*;\s*$', line):
                line = re.sub(r'return\s*;', 'return nullptr;', line)
                modified = True
            elif in_position_function and line.strip() == 'return false;':
                # Position functions can't return false
                line = line.replace('return false;', 'return Position();')
                modified = True

            result.append(line)

        if modified:
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write('\n'.join(result))
            print(f"  [OK] Fixed non-void returns in {file_path.name}")
            return True

        return False
    except Exception as e:
        print(f"  [ERR] {file_path}: {e}")
        return False

def main():
    print("=" * 80)
    print("COMPREHENSIVE DEBUG BUILD ERROR FIX")
    print("=" * 80)

    # Step 1: Add ObjectAccessor includes
    print("\n[1/3] Adding ObjectAccessor includes...")
    for rel_path in NEED_OBJECTACCESSOR:
        file_path = BASE_DIR / rel_path
        if file_path.exists():
            add_objectaccessor_include(file_path)

    # Step 2: Fix void function returns across all files
    print("\n[2/3] Fixing void function returns...")
    for cpp_file in BASE_DIR.rglob("*.cpp"):
        if 'backup' in str(cpp_file):
            continue
        fix_void_function_returns(cpp_file)

    # Step 3: Fix non-void function returns
    print("\n[3/3] Fixing non-void function returns...")
    for cpp_file in BASE_DIR.rglob("*.cpp"):
        if 'backup' in str(cpp_file):
            continue
        fix_non_void_function_returns(cpp_file)

    print("\n" + "=" * 80)
    print("COMPREHENSIVE FIX COMPLETE")
    print("=" * 80)

if __name__ == "__main__":
    main()
