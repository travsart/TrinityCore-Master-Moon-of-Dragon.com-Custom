#!/usr/bin/env python3
"""
Fix missing return statements (C2561 errors)
"""

import re
from pathlib import Path

BUILD_LOG = r"C:\TrinityBots\TrinityCore\debug_rebuild_fresh.log"

# Parse C2561 errors
errors = []

with open(BUILD_LOG, 'r', encoding='utf-8', errors='ignore') as f:
    for line in f:
        match = re.match(r'([^(]+\.cpp)\((\d+),\d+\):\s+error C2561:\s+"([^"]+)":', line)
        if match:
            file_path = match.group(1)
            line_num = int(match.group(2))
            func_name = match.group(3)
            errors.append((file_path, line_num, func_name))

print("=" * 80)
print(f"FIXING {len(errors)} MISSING RETURN ERRORS")
print("=" * 80)

# Group by file
from collections import defaultdict
by_file = defaultdict(list)
for file_path, line_num, func_name in errors:
    by_file[file_path].append((line_num, func_name))

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

        for line_num, func_name in line_data:
            if line_num - 1 >= len(lines):
                continue

            # Find function return type by looking backwards
            return_type = None
            for i in range(max(0, line_num - 50), line_num):
                if i < len(lines):
                    line_text = lines[i]

                    # Check for function signatures
                    if 'bool' in line_text and '::' in line_text and '(' in line_text:
                        return_type = 'bool'
                        break
                    elif 'std::vector' in line_text and '::' in line_text:
                        return_type = 'vector'
                        break
                    elif 'std::set' in line_text and '::' in line_text:
                        return_type = 'set'
                        break
                    elif re.search(r'(Unit|Creature|Player|GameObject)\s*\*.*::', line_text):
                        return_type = 'pointer'
                        break
                    elif re.search(r'\buint32\b.*::', line_text):
                        return_type = 'uint32'
                        break

            # Check if the error line has a bare return; that needs fixing
            error_line = lines[line_num - 1]
            if re.search(r'\breturn\s*;', error_line):
                # Replace bare return with appropriate value
                if return_type == 'bool':
                    lines[line_num - 1] = re.sub(r'\breturn\s*;', 'return false;', error_line)
                    fixes_in_file += 1
                    modified = True
                elif return_type == 'vector':
                    lines[line_num - 1] = re.sub(r'\breturn\s*;', 'return {};', error_line)
                    fixes_in_file += 1
                    modified = True
                elif return_type == 'set':
                    lines[line_num - 1] = re.sub(r'\breturn\s*;', 'return {};', error_line)
                    fixes_in_file += 1
                    modified = True
                elif return_type == 'pointer':
                    lines[line_num - 1] = re.sub(r'\breturn\s*;', 'return nullptr;', error_line)
                    fixes_in_file += 1
                    modified = True
                elif return_type == 'uint32':
                    lines[line_num - 1] = re.sub(r'\breturn\s*;', 'return 0;', error_line)
                    fixes_in_file += 1
                    modified = True
            else:
                # Missing return entirely - add one before the closing brace
                # Look forward for closing brace
                for j in range(line_num - 1, min(line_num + 10, len(lines))):
                    if '}' in lines[j]:
                        indent = len(lines[j]) - len(lines[j].lstrip())

                        if return_type == 'bool':
                            lines.insert(j, ' ' * indent + 'return false; // Auto-fixed\n')
                        elif return_type == 'vector':
                            lines.insert(j, ' ' * indent + 'return {}; // Auto-fixed\n')
                        elif return_type == 'set':
                            lines.insert(j, ' ' * indent + 'return {}; // Auto-fixed\n')
                        elif return_type == 'pointer':
                            lines.insert(j, ' ' * indent + 'return nullptr; // Auto-fixed\n')
                        elif return_type == 'uint32':
                            lines.insert(j, ' ' * indent + 'return 0; // Auto-fixed\n')
                        else:
                            break  # Can't determine type

                        fixes_in_file += 1
                        modified = True
                        break

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
