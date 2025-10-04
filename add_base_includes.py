#!/usr/bin/env python3
import re
import sys

# Mapping of class names to their required base specialization header
INCLUDE_MAP = {
    'DeathKnightRefactored': '#include "DeathKnightSpecialization.h"',
    'MageRefactored': '#include "MageSpecialization.h"',
    'PriestRefactored': '#include "PriestSpecialization.h"',
    'RogueRefactored': '#include "RogueSpecialization.h"',
    'ShamanRefactored': '#include "ShamanSpecialization.h"',
    'WarlockRefactored': '#include "WarlockSpecialization.h"',
    'WarriorRefactored': '#include "WarriorSpecialization.h"',
    'PaladinRefactored': '#include "PaladinSpecialization.h"',
    'MonkRefactored': '#include "MonkSpecialization.h"',
    'DruidRefactored': '#include "DruidSpecialization.h"',
    'DemonHunterRefactored': '#include "DemonHunterSpecialization.h"',
    'EvokerRefactored': '#include "EvokerSpecialization.h"',
    'HunterRefactored': '#include "HunterSpecialization.h"',
}

def add_base_include(file_path):
    """Add base specialization include to refactored files."""

    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Determine which include to add based on filename
    include_to_add = None
    for suffix, include_line in INCLUDE_MAP.items():
        if suffix in file_path:
            include_to_add = include_line
            break

    if not include_to_add:
        return False

    # Check if include already exists
    if include_to_add in content:
        print(f"Include already exists in {file_path}")
        return False

    # Find the #pragma once or #ifndef guard and add include after it
    # Look for pattern: after copyright header and initial includes
    pattern = r'(#pragma once\s*\n|#ifndef.*\n#define.*\n)'

    # Alternative: add after existing includes section
    # Find the last #include line in the header section
    lines = content.split('\n')
    last_include_idx = -1

    for i, line in enumerate(lines):
        if line.strip().startswith('#include'):
            last_include_idx = i
        # Stop searching after we hit the first non-preprocessor, non-comment, non-empty line after includes
        elif last_include_idx > 0 and line.strip() and not line.strip().startswith('//') and not line.strip().startswith('/*') and not line.strip().startswith('*'):
            break

    if last_include_idx >= 0:
        # Add the include after the last existing include
        lines.insert(last_include_idx + 1, include_to_add)
        new_content = '\n'.join(lines)

        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(new_content)

        print(f"Added {include_to_add} to {file_path}")
        return True

    return False

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python add_base_includes.py <file1> <file2> ...")
        sys.exit(1)

    files_modified = 0
    for file_path in sys.argv[1:]:
        try:
            if add_base_include(file_path):
                files_modified += 1
        except Exception as e:
            print(f"Error processing {file_path}: {e}", file=sys.stderr)

    print(f"\nTotal files modified: {files_modified}")
