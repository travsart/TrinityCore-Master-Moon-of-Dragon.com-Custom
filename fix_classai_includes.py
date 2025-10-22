#!/usr/bin/env python3
"""
Fix incorrect SpatialGridQueryHelpers.h include paths in all ClassAI files
Generated paths were wrong - need to recalculate based on actual file location
"""

import re
from pathlib import Path

BASE_DIR = Path(r"C:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI")
SPATIAL_TARGET = Path(r"C:\TrinityBots\TrinityCore\src\modules\Playerbot\Spatial\SpatialGridQueryHelpers.h")

def calculate_correct_path(filepath):
    """Calculate correct relative path from file to SpatialGridQueryHelpers.h"""
    # Get relative path from file to project root
    try:
        # Both paths need to be absolute
        file_abs = filepath.resolve()
        target_abs = SPATIAL_TARGET.resolve()

        # Calculate relative path
        rel_path = Path(file_abs.parent).relative_to(file_abs.parents[0])

        # Count how many levels deep we are from ClassAI directory
        file_rel_to_classai = filepath.relative_to(BASE_DIR)
        depth = len(file_rel_to_classai.parts) - 1  # -1 for filename

        # From ClassAI to Spatial: ../../../Spatial/SpatialGridQueryHelpers.h
        # From ClassAI/Subdir to Spatial: ../../../../Spatial/SpatialGridQueryHelpers.h
        # Base depth from ClassAI to Playerbot root: 2 (AI -> Playerbot)
        # Then to Spatial: +1
        total_depth = 2 + depth + 1

        prefix = "../" * total_depth
        return f'{prefix}Spatial/SpatialGridQueryHelpers.h'
    except Exception as e:
        print(f"Error calculating path for {filepath}: {e}")
        return None

def fix_include_in_file(filepath):
    """Fix the SpatialGridQueryHelpers.h include path in a file"""
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    # Check if file has the include we need to fix
    if 'SpatialGridQueryHelpers.h' not in content:
        return False

    original_content = content

    # Calculate correct path
    correct_path = calculate_correct_path(filepath)
    if not correct_path:
        return False

    # Replace any existing SpatialGridQueryHelpers.h include with correct one
    pattern = re.compile(
        r'#include\s+"[.\/]*Spatial/SpatialGridQueryHelpers\.h"(\s*//.*)?',
        re.MULTILINE
    )

    replacement = f'#include "{correct_path}"  // PHASE 5F: Thread-safe queries'
    content = pattern.sub(replacement, content)

    if content != original_content:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        return True

    return False

def main():
    """Main execution"""
    print("=" * 80)
    print("Fixing ClassAI Include Paths for SpatialGridQueryHelpers.h")
    print("=" * 80)

    # Get all .cpp and .h files in ClassAI
    all_files = sorted(list(BASE_DIR.rglob("*.cpp")) + list(BASE_DIR.rglob("*.h")))

    fixed = 0
    for filepath in all_files:
        if fix_include_in_file(filepath):
            rel_path = filepath.relative_to(BASE_DIR)
            correct_path = calculate_correct_path(filepath)
            print(f"[FIXED] {rel_path}")
            print(f"        -> {correct_path}")
            fixed += 1

    print("=" * 80)
    print(f"Complete: {fixed} files fixed")
    print("=" * 80)

if __name__ == "__main__":
    main()
