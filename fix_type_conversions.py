#!/usr/bin/env python3
"""
Fix C2440 type conversion errors
"""

import re
from pathlib import Path

BUILD_LOG = r"C:\TrinityBots\TrinityCore\debug_rebuild_after_fixes.log"

# Parse C2440 errors
errors = []

with open(BUILD_LOG, 'r', encoding='utf-8', errors='ignore') as f:
    for line in f:
        # Match: file.cpp(line,col): error C2440: "return": "X" kann nicht in "Y" konvertiert werden
        match = re.match(r'([^(]+\.cpp)\((\d+),\d+\):\s+error C2440:.*"return":\s+"([^"]+)".*"([^"]+)"', line)
        if match:
            file_path = match.group(1)
            line_num = int(match.group(2))
            from_type = match.group(3)
            to_type = match.group(4)
            errors.append((file_path, line_num, from_type, to_type))

print("=" * 80)
print(f"FIXING {len(errors)} TYPE CONVERSION ERRORS (C2440)")
print("=" * 80)

# Group by file
from collections import defaultdict
by_file = defaultdict(list)
for file_path, line_num, from_type, to_type in errors:
    by_file[file_path].append((line_num, from_type, to_type))

total_fixed = 0
files_fixed = 0

for file_path, line_data in sorted(by_file.items()):
    try:
        path = Path(file_path)
        if not path.exists():
            print(f"  [SKIP] Not found: {path.name}")
            continue

        with open(path, 'r', encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()

        modified = False
        fixes_in_file = 0

        for line_num, from_type, to_type in line_data:
            if line_num - 1 >= len(lines):
                continue

            line = lines[line_num - 1]
            old_line = line

            # Pattern 1: return nullptr; in bool function → return false;
            if 'nullptr' in from_type and 'bool' in to_type:
                line = re.sub(r'(\s+)return\s+nullptr\s*;', r'\1return false;', line)

            # Pattern 2: return false; in pointer function → return nullptr;
            elif 'bool' in from_type and '*' in to_type:
                line = re.sub(r'(\s+)return\s+false\s*;', r'\1return nullptr;', line)

            # Pattern 3: return false; in Unit* function → return nullptr;
            elif 'false' in from_type.lower() and ('Unit' in to_type or 'Creature' in to_type or 'Player' in to_type):
                line = re.sub(r'(\s+)return\s+false\s*;', r'\1return nullptr;', line)

            # Pattern 4: return true; in pointer function → return nullptr;
            elif 'true' in from_type.lower() and '*' in to_type:
                line = re.sub(r'(\s+)return\s+true\s*;', r'\1return nullptr;', line)

            # Pattern 5: return 0; in pointer function → return nullptr;
            elif from_type == '0' and '*' in to_type:
                line = re.sub(r'(\s+)return\s+0\s*;', r'\1return nullptr;', line)

            if line != old_line:
                lines[line_num - 1] = line
                fixes_in_file += 1
                modified = True

        if modified:
            with open(path, 'w', encoding='utf-8') as f:
                f.writelines(lines)

            print(f"  [OK] {path.relative_to(path.parents[5])} ({fixes_in_file} fixes)")
            files_fixed += 1
            total_fixed += fixes_in_file

    except Exception as e:
        print(f"  [ERR] {path.name}: {e}")

print()
print("=" * 80)
print(f"TOTAL: {total_fixed} fixes in {files_fixed} files")
print("=" * 80)
