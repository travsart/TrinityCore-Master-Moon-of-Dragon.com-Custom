#!/usr/bin/env python3
"""
Final comprehensive Debug error fix using actual build log
"""

import re
from pathlib import Path
from collections import defaultdict

BASE_DIR = Path(r"C:\TrinityBots\TrinityCore\src\modules\Playerbot")
ERROR_LOG = Path(r"C:\TrinityBots\TrinityCore\debug_build_errors.log")

def parse_build_errors():
    """Parse build log and categorize all errors"""
    void_return = defaultdict(list)  # file -> [(line, func_name)]
    missing_return = defaultdict(list)
    other_errors = []

    with open(ERROR_LOG, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()

    # Pattern 1: void function returns value
    # C:\...\file.cpp(123,45): error C2562: "Playerbot::ClassName::FunctionName": "void"-Funktion gibt einen Wert zurück
    for match in re.finditer(r'([^(]+)\((\d+),\d+\):.*error C2562.*"([^"]+)".*void.*gibt', content):
        file_path = match.group(1)
        line_num = int(match.group(2))
        func_sig = match.group(3)

        if 'Playerbot' in file_path:
            rel_path = re.search(r'Playerbot[\\/](.+)', file_path).group(1).replace('\\', '/')
            func_name = func_sig.split('::')[-1] if '::' in func_sig else func_sig
            void_return[rel_path].append((line_num, func_name))

    # Pattern 2: function must return value
    # C:\...\file.cpp(123,45): error C2561: "Function": Funktion muss einen Wert zurückgeben
    for match in re.finditer(r'([^(]+)\((\d+),\d+\):.*error C2561.*"([^"]+)".*muss', content):
        file_path = match.group(1)
        line_num = int(match.group(2))
        func_sig = match.group(3)

        if 'Playerbot' in file_path:
            rel_path = re.search(r'Playerbot[\\/](.+)', file_path).group(1).replace('\\', '/')
            missing_return[rel_path].append((line_num, func_sig))

    # Pattern 3: other errors
    for match in re.finditer(r'([^(]+)\(\d+,\d+\):.*error (C\d+):', content):
        file_path = match.group(1)
        error_code = match.group(2)

        if 'Playerbot' in file_path and error_code not in ['C2562', 'C2561']:
            if 'Playerbot' in file_path:
                rel_path = re.search(r'Playerbot[\\/](.+)', file_path).group(1).replace('\\', '/')
                other_errors.append((rel_path, error_code))

    return void_return, missing_return, other_errors

def fix_void_returns(rel_path: str, errors: list) -> bool:
    """Fix void function return errors"""
    file_path = BASE_DIR / rel_path
    if not file_path.exists():
        return False

    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()

        modified = False
        for line_num, func_name in errors:
            if line_num - 1 < len(lines):
                line = lines[line_num - 1]
                # Fix: return <value>; → return;
                if re.search(r'\breturn\s+(false|true|nullptr|\{\}|\d+|Position\(\))\s*;', line):
                    lines[line_num - 1] = re.sub(
                        r'\breturn\s+(false|true|nullptr|\{\}|\d+|Position\(\))\s*;',
                        'return;',
                        line
                    )
                    modified = True

        if modified:
            with open(file_path, 'w', encoding='utf-8') as f:
                f.writelines(lines)
            return True

        return False
    except Exception as e:
        print(f"  [ERR] {rel_path}: {e}")
        return False

def fix_missing_returns(rel_path: str, errors: list) -> bool:
    """Fix missing return statements"""
    file_path = BASE_DIR / rel_path
    if not file_path.exists():
        return False

    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()

        modified = False

        for line_num, func_sig in errors:
            # Find function signature by looking backwards from error line
            func_return_type = None
            for i in range(max(0, line_num - 30), line_num):
                if i < len(lines):
                    # Check for function signature
                    if 'bool' in lines[i] and '::' in lines[i] and '(' in lines[i]:
                        func_return_type = 'bool'
                        break
                    elif 'std::vector' in lines[i] and '::' in lines[i]:
                        func_return_type = 'vector'
                        break
                    elif re.search(r'(Unit|Creature|Player|GameObject)\*.*::', lines[i]):
                        func_return_type = 'pointer'
                        break

            # Add return before the error line
            if line_num - 1 < len(lines):
                indent = len(lines[line_num - 1]) - len(lines[line_num - 1].lstrip())

                if func_return_type == 'bool':
                    lines.insert(line_num - 1, ' ' * indent + 'return false; // Auto-fixed\n')
                    modified = True
                elif func_return_type == 'vector':
                    lines.insert(line_num - 1, ' ' * indent + 'return {}; // Auto-fixed\n')
                    modified = True
                elif func_return_type == 'pointer':
                    lines.insert(line_num - 1, ' ' * indent + 'return nullptr; // Auto-fixed\n')
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
    print("FINAL DEBUG ERROR FIX - SYSTEMATIC APPROACH")
    print("=" * 80)

    print("\n[1/4] Parsing build errors...")
    void_return, missing_return, other_errors = parse_build_errors()

    print(f"  - Void return errors: {sum(len(v) for v in void_return.values())} in {len(void_return)} files")
    print(f"  - Missing return errors: {sum(len(v) for v in missing_return.values())} in {len(missing_return)} files")
    print(f"  - Other errors: {len(set(other_errors))} unique")

    print("\n[2/4] Fixing void return errors...")
    fixed_void = 0
    for rel_path, errors in sorted(void_return.items()):
        if fix_void_returns(rel_path, errors):
            fixed_void += 1
            print(f"  [OK] {rel_path} ({len(errors)} fixes)")

    print(f"\n  [DONE] Fixed {fixed_void} files")

    print("\n[3/4] Fixing missing return errors...")
    fixed_missing = 0
    for rel_path, errors in sorted(missing_return.items()):
        if fix_missing_returns(rel_path, errors):
            fixed_missing += 1
            print(f"  [OK] {rel_path} ({len(errors)} fixes)")

    print(f"\n  [DONE] Fixed {fixed_missing} files")

    print("\n[4/4] Other error types:")
    if other_errors:
        error_types = {}
        for rel_path, code in other_errors:
            if code not in error_types:
                error_types[code] = []
            error_types[code].append(rel_path)

        for code, files in sorted(error_types.items()):
            print(f"  {code}: {len(set(files))} files")
            if code in ['C2039', 'C2065', 'C2275', 'C2672', 'C3108', 'C3861']:
                print(f"    Sample: {list(set(files))[:3]}")

    print("\n" + "=" * 80)
    print(f"TOTAL FIXED: {fixed_void + fixed_missing} files")
    print("=" * 80)

if __name__ == "__main__":
    main()
