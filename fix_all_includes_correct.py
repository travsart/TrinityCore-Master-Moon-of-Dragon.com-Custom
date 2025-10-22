#!/usr/bin/env python3
"""
Fix ALL SpatialGridQueryHelpers.h include paths in ClassAI
The paths were calculated wrong - need to stay within Playerbot module
"""

import re
from pathlib import Path

BASE_DIR = Path(r"C:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI")

def get_correct_path(filepath):
    """Calculate correct relative path based on depth from ClassAI directory"""
    rel_path = filepath.relative_to(BASE_DIR)
    depth = len(rel_path.parts) - 1  # -1 for filename

    # From ClassAI/*.cpp: ../../Spatial/SpatialGridQueryHelpers.h
    # From ClassAI/SubDir/*.cpp: ../../../Spatial/SpatialGridQueryHelpers.h
    # Formula: depth + 2 (AI -> Playerbot, ClassAI -> AI)
    total_depth = depth + 2
    prefix = "../" * total_depth

    return f'{prefix}Spatial/SpatialGridQueryHelpers.h'

def fix_file(filepath):
    """Fix include path in one file"""
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    if 'SpatialGridQueryHelpers.h' not in content:
        return False

    original = content
    correct_path = get_correct_path(filepath)

    # Replace any existing SpatialGridQueryHelpers.h include
    pattern = re.compile(
        r'#include\s+"[.\/]*Spatial/SpatialGridQueryHelpers\.h"(\s*//.*)?',
        re.MULTILINE
    )

    replacement = f'#include "{correct_path}"  // PHASE 5F: Thread-safe queries'
    content = pattern.sub(replacement, content)

    if content != original:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        return True

    return False

def main():
    """Main execution"""
    print("=" * 80)
    print("CORRECTING All ClassAI Include Paths")
    print("=" * 80)

    all_files = sorted(list(BASE_DIR.rglob("*.cpp")) + list(BASE_DIR.rglob("*.h")))
    fixed = 0

    for filepath in all_files:
        correct_path = get_correct_path(filepath)
        if fix_file(filepath):
            print(f"[FIXED] {filepath.relative_to(BASE_DIR)}")
            print(f"        -> {correct_path}")
            fixed += 1

    print("=" * 80)
    print(f"Complete: {fixed} files fixed")
    print("=" * 80)

if __name__ == "__main__":
    main()
