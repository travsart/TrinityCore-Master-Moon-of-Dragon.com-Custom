#!/usr/bin/env python3
"""
Fix Debug errors by parsing actual build log output
Handles both C2562 (void returns value) and C2561 (missing return)
"""

import re
from pathlib import Path
from collections import defaultdict

BUILD_LOG = Path(r"C:\TrinityBots\TrinityCore\debug_rebuild_fresh.log")
BASE_DIR = Path(r"C:\TrinityBots\TrinityCore")

def parse_errors():
    """Parse build log and extract error information"""
    void_returns = []  # [(file_path, line_num, func_name)]
    missing_returns = []  # [(file_path, line_num, func_name)]

    with open(BUILD_LOG, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()

    # Pattern 1: C2562 - void function returns value
    # Example: C:\...\PathOptimizer.cpp(435,13): error C2562: "Playerbot::PathOptimizer::ApplyCatmullRomSmoothing": "void"-Funktion gibt einen Wert zurück
    for match in re.finditer(r'([^(]+\.(cpp|h))\((\d+),\d+\):\s+error C2562:\s+"([^"]+)":\s+"void"', content):
        file_path = match.group(1)
        line_num = int(match.group(3))
        func_sig = match.group(4)
        void_returns.append((file_path, line_num, func_sig))

    # Pattern 2: C2561 - function must return value
    # Example: C:\...\AfflictionSpecialization.cpp(400,9): error C2561: "Playerbot::AfflictionSpecialization::ShouldCastSeedOfCorruption": Funktion muss einen Wert zurückgeben
    for match in re.finditer(r'([^(]+\.(cpp|h))\((\d+),\d+\):\s+error C2561:\s+"([^"]+)":', content):
        file_path = match.group(1)
        line_num = int(match.group(3))
        func_sig = match.group(4)
        missing_returns.append((file_path, line_num, func_sig))

    return void_returns, missing_returns

def fix_void_return(file_path: str, line_num: int, func_name: str) -> bool:
    """Fix a single void function return error"""
    try:
        path = Path(file_path)
        if not path.exists():
            return False

        with open(path, 'r', encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()

        if line_num - 1 >= len(lines):
            return False

        line = lines[line_num - 1]

        # Fix: return <value>; → return;
        if re.search(r'\breturn\s+(false|true|nullptr|\{\}|0|Position\(\))\s*;', line):
            lines[line_num - 1] = re.sub(
                r'\breturn\s+(false|true|nullptr|\{\}|0|Position\(\))\s*;',
                'return;',
                line
            )

            with open(path, 'w', encoding='utf-8') as f:
                f.writelines(lines)
            return True

        return False
    except Exception as e:
        print(f"  [ERR] {path.name}:{line_num}: {e}")
        return False

def fix_missing_return(file_path: str, line_num: int, func_name: str) -> bool:
    """Fix a missing return statement"""
    try:
        path = Path(file_path)
        if not path.exists():
            return False

        with open(path, 'r', encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()

        if line_num - 1 >= len(lines):
            return False

        # Determine return type by looking backwards for function signature
        return_type = None
        for i in range(max(0, line_num - 50), line_num):
            if i < len(lines):
                # Check for function return type
                if re.search(r'\bbool\b.*::.*\(', lines[i]):
                    return_type = 'bool'
                    break
                elif re.search(r'std::vector\s*<', lines[i]):
                    return_type = 'vector'
                    break
                elif re.search(r'(Unit|Creature|Player|GameObject)\s*\*.*::', lines[i]):
                    return_type = 'pointer'
                    break
                elif re.search(r'\buint32\b.*::', lines[i]):
                    return_type = 'uint32'
                    break
                elif re.search(r'Position\s+.*::', lines[i]):
                    return_type = 'Position'
                    break

        # Get indentation from error line
        indent = len(lines[line_num - 1]) - len(lines[line_num - 1].lstrip())

        # Insert appropriate return before the error line
        if return_type == 'bool':
            lines.insert(line_num - 1, ' ' * indent + 'return false; // Auto-fixed\n')
        elif return_type == 'vector':
            lines.insert(line_num - 1, ' ' * indent + 'return {}; // Auto-fixed\n')
        elif return_type == 'pointer':
            lines.insert(line_num - 1, ' ' * indent + 'return nullptr; // Auto-fixed\n')
        elif return_type == 'uint32':
            lines.insert(line_num - 1, ' ' * indent + 'return 0; // Auto-fixed\n')
        elif return_type == 'Position':
            lines.insert(line_num - 1, ' ' * indent + 'return Position(); // Auto-fixed\n')
        else:
            return False  # Can't determine type

        with open(path, 'w', encoding='utf-8') as f:
            f.writelines(lines)
        return True

    except Exception as e:
        print(f"  [ERR] {path.name}:{line_num}: {e}")
        return False

def main():
    print("=" * 80)
    print("DEBUG ERROR FIX - BUILD LOG PARSER")
    print("=" * 80)

    print("\n[1/4] Parsing build log...")
    void_returns, missing_returns = parse_errors()

    print(f"  - C2562 (void returns value): {len(void_returns)} errors")
    print(f"  - C2561 (missing return): {len(missing_returns)} errors")

    print("\n[2/4] Fixing void function returns...")
    fixed_void = 0
    failed_void = 0

    # Group by file for better output
    by_file = defaultdict(list)
    for file_path, line_num, func_name in void_returns:
        by_file[file_path].append((line_num, func_name))

    for file_path in sorted(by_file.keys()):
        errors = by_file[file_path]
        file_fixed = 0
        for line_num, func_name in errors:
            if fix_void_return(file_path, line_num, func_name):
                file_fixed += 1
                fixed_void += 1
            else:
                failed_void += 1

        if file_fixed > 0:
            rel_path = Path(file_path).relative_to(BASE_DIR)
            print(f"  [OK] {rel_path} ({file_fixed} fixes)")

    print(f"\n  [DONE] Fixed {fixed_void} void returns, {failed_void} failed")

    print("\n[3/4] Fixing missing returns...")
    fixed_missing = 0
    failed_missing = 0

    # Group by file
    by_file = defaultdict(list)
    for file_path, line_num, func_name in missing_returns:
        by_file[file_path].append((line_num, func_name))

    for file_path in sorted(by_file.keys()):
        errors = by_file[file_path]
        file_fixed = 0
        for line_num, func_name in errors:
            if fix_missing_return(file_path, line_num, func_name):
                file_fixed += 1
                fixed_missing += 1
            else:
                failed_missing += 1

        if file_fixed > 0:
            rel_path = Path(file_path).relative_to(BASE_DIR)
            print(f"  [OK] {rel_path} ({file_fixed} fixes)")

    print(f"\n  [DONE] Fixed {fixed_missing} missing returns, {failed_missing} failed")

    print("\n[4/4] Summary")
    print("=" * 80)
    print(f"TOTAL FIXED: {fixed_void + fixed_missing} errors")
    print(f"  - Void returns: {fixed_void}")
    print(f"  - Missing returns: {fixed_missing}")
    print(f"FAILED: {failed_void + failed_missing} errors")
    print("=" * 80)

if __name__ == "__main__":
    main()
