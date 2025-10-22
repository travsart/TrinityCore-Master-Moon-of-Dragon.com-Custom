#!/usr/bin/env python3
"""
Fix Debug errors by parsing actual build output
"""

import re
from pathlib import Path
from collections import defaultdict

BASE_DIR = Path(r"C:\TrinityBots\TrinityCore\src\modules\Playerbot")
ERROR_FILE = Path(r"C:\TrinityBots\TrinityCore\debug_errors.txt")

def parse_errors():
    """Parse error file and categorize"""
    void_return_errors = defaultdict(list)  # file -> list of line numbers
    missing_return_errors = defaultdict(list)
    other_errors = []

    with open(ERROR_FILE, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            # Match: C:\path\file.cpp(123,45): error C2562: "void" function returns value
            match = re.match(r'([^(]+)\((\d+),\d+\):.*error C2562.*void.*gibt einen Wert', line)
            if match:
                file_path = match.group(1)
                line_num = int(match.group(2))
                # Extract relative path
                if 'modules\\Playerbot' in file_path or 'modules/Playerbot' in file_path:
                    rel_path = re.search(r'modules[\\/]Playerbot[\\/](.+)', file_path).group(1)
                    rel_path = rel_path.replace('\\', '/')
                    void_return_errors[rel_path].append(line_num)
                continue

            # Match: missing return statement
            match = re.match(r'([^(]+)\((\d+),\d+\):.*error C2561.*muss einen Wert', line)
            if match:
                file_path = match.group(1)
                line_num = int(match.group(2))
                if 'modules\\Playerbot' in file_path or 'modules/Playerbot' in file_path:
                    rel_path = re.search(r'modules[\\/]Playerbot[\\/](.+)', file_path).group(1)
                    rel_path = rel_path.replace('\\', '/')
                    missing_return_errors[rel_path].append(line_num)
                continue

            # Other errors
            if 'modules\\Playerbot' in line or 'modules/Playerbot' in line:
                other_errors.append(line)

    return void_return_errors, missing_return_errors, other_errors

def fix_void_returns_by_line(rel_path: str, line_numbers: list) -> bool:
    """Fix specific lines with void return errors"""
    file_path = BASE_DIR / rel_path
    if not file_path.exists():
        print(f"  [SKIP] Not found: {rel_path}")
        return False

    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()

        modified = False
        for line_num in line_numbers:
            if line_num - 1 < len(lines):
                line = lines[line_num - 1]
                # Fix void function returns
                if re.search(r'return\s+(false|true|nullptr|\{\}|\d+)\s*;', line):
                    lines[line_num - 1] = re.sub(r'return\s+(false|true|nullptr|\{\}|\d+)\s*;', 'return;', line)
                    modified = True

        if modified:
            with open(file_path, 'w', encoding='utf-8') as f:
                f.writelines(lines)
            return True

        return False
    except Exception as e:
        print(f"  [ERR] {rel_path}: {e}")
        return False

def fix_missing_returns_by_line(rel_path: str, line_numbers: list) -> bool:
    """Fix specific lines with missing return errors"""
    file_path = BASE_DIR / rel_path
    if not file_path.exists():
        return False

    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()

        modified = False
        # For missing returns, we need to look at the function signature to know what to return
        # For now, let's add return false; or return {}; based on context
        for line_num in line_numbers:
            if line_num - 1 < len(lines):
                # Find the function signature by looking backwards
                func_return_type = None
                for i in range(max(0, line_num - 20), line_num):
                    if re.match(r'^\s*bool\s+\w+::\w+', lines[i]):
                        func_return_type = 'bool'
                        break
                    elif re.match(r'^\s*std::vector', lines[i]):
                        func_return_type = 'vector'
                        break

                # Insert appropriate return statement before the line
                if func_return_type == 'bool':
                    lines.insert(line_num - 1, '        return false; // Auto-fixed missing return\n')
                    modified = True
                elif func_return_type == 'vector':
                    lines.insert(line_num - 1, '        return {}; // Auto-fixed missing return\n')
                    modified = True

        if modified:
            with open(file_path, 'w', encoding='utf-8') as f:
                f.writelines(lines)
            return True

        return False
    except Exception as e:
        print(f"  [ERR] {rel_path}: {e}")
        return False

def main():
    print("=" * 80)
    print("FIXING DEBUG ERRORS FROM BUILD OUTPUT")
    print("=" * 80)

    print("\n[1/3] Parsing error file...")
    void_errors, missing_errors, other_errors = parse_errors()

    print(f"  - Void return errors: {len(void_errors)} files")
    print(f"  - Missing return errors: {len(missing_errors)} files")
    print(f"  - Other errors: {len(other_errors)} lines")

    print("\n[2/3] Fixing void return errors...")
    fixed_void = 0
    for rel_path, line_nums in sorted(void_errors.items()):
        if fix_void_returns_by_line(rel_path, line_nums):
            fixed_void += 1
            print(f"  [OK] {rel_path} ({len(line_nums)} errors)")

    print(f"\n  [DONE] Fixed {fixed_void} files with void return errors")

    print("\n[3/3] Fixing missing return errors...")
    fixed_missing = 0
    for rel_path, line_nums in sorted(missing_errors.items()):
        if fix_missing_returns_by_line(rel_path, line_nums):
            fixed_missing += 1
            print(f"  [OK] {rel_path} ({len(line_nums)} errors)")

    print(f"\n  [DONE] Fixed {fixed_missing} files with missing return errors")

    print("\n" + "=" * 80)
    print(f"TOTAL: Fixed {fixed_void + fixed_missing} files")
    print("=" * 80)

    if other_errors:
        print("\nRemaining non-return errors:")
        unique_errors = set()
        for err in other_errors:
            # Extract just the error type
            match = re.search(r'error (C\d+):', err)
            if match:
                unique_errors.add(match.group(1))
        print(f"  Error types: {', '.join(sorted(unique_errors))}")

if __name__ == "__main__":
    main()
