#!/usr/bin/env python3
"""
Universal fix for ALL remaining void function return errors
"""
import re
from pathlib import Path

BASE_DIR = Path(r"C:\TrinityBots\TrinityCore\src\modules\Playerbot")

def fix_file(file_path: Path) -> int:
    """Fix all void function return errors in a file"""
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()

        result = []
        fixes = 0
        current_return_type = None
        brace_depth = 0

        for i, line in enumerate(lines):
            # Detect function signature and return type
            func_match = re.match(r'^\s*(void|bool|int|uint32|float|std::\w+|Unit\*|Creature\*|Player\*|Position)\s+\w+::\w+\s*\(', line)
            if func_match:
                current_return_type = func_match.group(1)
                brace_depth = 0

            # Track brace depth
            brace_depth += line.count('{') - line.count('}')

            # Reset on function end
            if brace_depth == 0 and current_return_type:
                current_return_type = None

            # Fix void function returns
            if current_return_type == 'void':
                if re.search(r'\breturn\s+(false|true|nullptr|\{\}|\d+|Position\(\))\s*;', line):
                    line = re.sub(r'\breturn\s+(false|true|nullptr|\{\}|\d+|Position\(\))\s*;', 'return;', line)
                    fixes += 1

            result.append(line)

        if fixes > 0:
            with open(file_path, 'w', encoding='utf-8') as f:
                f.writelines(result)

        return fixes

    except Exception as e:
        print(f"  [ERR] {file_path.name}: {e}")
        return 0

def main():
    print("=" * 80)
    print("UNIVERSAL VOID RETURN FIX - ALL REMAINING FILES")
    print("=" * 80)

    total_fixes = 0
    files_fixed = 0

    for cpp_file in BASE_DIR.rglob("*.cpp"):
        if 'backup' in str(cpp_file) or 'test' in str(cpp_file).lower():
            continue

        fixes = fix_file(cpp_file)
        if fixes > 0:
            files_fixed += 1
            total_fixes += fixes
            print(f"  [OK] {cpp_file.relative_to(BASE_DIR)} ({fixes} fixes)")

    print("\n" + "=" * 80)
    print(f"TOTAL: {total_fixes} fixes in {files_fixed} files")
    print("=" * 80)

if __name__ == "__main__":
    main()
