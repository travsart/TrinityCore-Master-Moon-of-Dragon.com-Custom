#!/usr/bin/env python3
"""
Fix ALL remaining 'auto* = nullptr' errors across all ClassAI files.
This error occurs when there are malformed snapshot blocks.
"""

import re
from pathlib import Path

BASE_DIR = Path(r"C:\TrinityBots\TrinityCore\src\modules\Playerbot")

def fix_auto_nullptr_and_duplicates(content):
    """Fix auto* = nullptr and duplicate snapshot blocks"""
    original = content

    # Pattern 1: Fix 'auto* entity = nullptr;' -> 'Creature* entity = nullptr;'
    pattern1 = re.compile(r'(\s+)auto\*\s+(\w+)\s*=\s*nullptr;', re.MULTILINE)
    content = pattern1.sub(r'\1Creature* \2 = nullptr;', content)

    # Pattern 2: Remove duplicate snapshot blocks (multiple declarations of snapshot_entity)
    # This pattern finds duplicate snapshot assignments in the same scope
    pattern2 = re.compile(
        r'(\s+)(const\s+\w+::)?DoubleBufferedSpatialGrid::CreatureSnapshot\s*\*\s*snapshot_(\w+)\s*=\s*'
        r'SpatialGridQueryHelpers::(Find\w+)\([^)]+\);\s*\n'
        r'[^;]*\s*=\s*nullptr;\s*\n'
        r'\s*if\s*\(\s*snapshot_\3\s*\)\s*\n'
        r'\s*\{\s*\n'
        r'(?:[^}]+\n)*?\s*\}\s*\n'
        r'\s+const\s+\w+::DoubleBufferedSpatialGrid::CreatureSnapshot\s*\*\s*snapshot_\3\s*=',
        re.MULTILINE | re.DOTALL
    )

    # Keep only first occurrence, remove second
    def remove_duplicate(match):
        return match.group(0).rsplit('const ', 1)[0]  # Keep everything before the second 'const'

    content = pattern2.sub(remove_duplicate, content)

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
    content = fix_auto_nullptr_and_duplicates(content)

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
    print("Fixing ALL auto* = nullptr Errors in ClassAI Files")
    print("=" * 80)

    # Find all .cpp files in ClassAI directory
    classai_dir = BASE_DIR / "AI" / "ClassAI"
    cpp_files = list(classai_dir.rglob("*.cpp"))

    fixed = 0
    for filepath in cpp_files:
        rel_path = filepath.relative_to(BASE_DIR)
        if process_file(filepath):
            print(f"[FIXED] {rel_path}")
            fixed += 1

    print("=" * 80)
    print(f"Complete: {fixed} files fixed out of {len(cpp_files)} total files")
    print("=" * 80)

if __name__ == "__main__":
    main()
