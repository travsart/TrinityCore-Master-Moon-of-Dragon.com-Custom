#!/usr/bin/env python3
"""
Fix void function return errors by targeting specific lines from build log
"""

import re
from pathlib import Path

# Parse build log for void return errors
BUILD_LOG = r"C:\TrinityBots\TrinityCore\debug_rebuild_fresh.log"

# Extract all void return errors
errors = []  # [(file_path, line_num)]

with open(BUILD_LOG, 'r', encoding='utf-8', errors='ignore') as f:
    for line in f:
        # Match: C:\...\file.cpp(line,col): error C2562: ... "void"
        match = re.match(r'([^(]+\.cpp)\((\d+),\d+\):\s+error C2562:.*void', line)
        if match:
            file_path = match.group(1)
            line_num = int(match.group(2))
            errors.append((file_path, line_num))

print("=" * 80)
print(f"FIXING {len(errors)} VOID RETURN ERRORS")
print("=" * 80)

# Group by file
from collections import defaultdict
by_file = defaultdict(list)
for file_path, line_num in errors:
    by_file[file_path].append(line_num)

total_fixed = 0
files_fixed = 0

for file_path, line_nums in sorted(by_file.items()):
    try:
        path = Path(file_path)
        if not path.exists():
            print(f"  [SKIP] Not found: {path.name}")
            continue

        with open(path, 'r', encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()

        modified = False
        fixes_in_file = 0

        for line_num in line_nums:
            if line_num - 1 < len(lines):
                line = lines[line_num - 1]

                # Fix void return patterns
                old_line = line
                # Pattern 1: return false;
                line = re.sub(r'(\s+)return\s+false\s*;', r'\1return;', line)
                # Pattern 2: return true;
                line = re.sub(r'(\s+)return\s+true\s*;', r'\1return;', line)
                # Pattern 3: return nullptr;
                line = re.sub(r'(\s+)return\s+nullptr\s*;', r'\1return;', line)
                # Pattern 4: return 0;
                line = re.sub(r'(\s+)return\s+0\s*;', r'\1return;', line)
                # Pattern 5: return {};
                line = re.sub(r'(\s+)return\s+\{\}\s*;', r'\1return;', line)
                # Pattern 6: return Position();
                line = re.sub(r'(\s+)return\s+Position\(\)\s*;', r'\1return;', line)

                if line != old_line:
                    lines[line_num - 1] = line
                    modified = True
                    fixes_in_file += 1

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
