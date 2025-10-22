#!/usr/bin/env python3
"""
FINAL COMPLETE FIX: All ClassAI ObjectAccessor calls
One-time comprehensive replacement handling ALL edge cases
"""

import re
from pathlib import Path

BASE_DIR = Path(r"C:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI")

def add_include(content, filepath):
    """Add SpatialGridQueryHelpers include if missing"""
    if 'SpatialGridQueryHelpers.h' in content:
        return content

    pattern = re.compile(r'(#include\s+[<"].*?[>"])', re.MULTILINE)
    matches = list(pattern.finditer(content))
    if not matches:
        return content

    last_match = matches[-1]
    rel_path = filepath.relative_to(BASE_DIR)
    depth = len(rel_path.parts) - 1
    prefix = "../" * (depth + 3)
    include = f'\n#include "{prefix}Spatial/SpatialGridQueryHelpers.h"  // PHASE 5F: Thread-safe queries'

    return content[:last_match.end()] + include + content[last_match.end():]

def fix_all_objectaccessor(content):
    """Fix ALL ObjectAccessor::GetUnit/GetCreature calls in ONE pass"""

    replacements = []

    # Pattern 1: if (Type* var = ObjectAccessor::GetXXX(...))
    pattern1 = re.compile(
        r'(\s+)if\s*\(([:\w]+\*?\s+\w+)\s*=\s*ObjectAccessor::(GetUnit|GetCreature)\(\*([^,]+),\s*([^)]+)\)\)',
        re.MULTILINE
    )
    for match in pattern1.finditer(content):
        indent = match.group(1)
        var_decl = match.group(2).strip()
        method = match.group(3)
        bot = match.group(4).strip()
        guid = match.group(5).strip()

        var_name = var_decl.split()[-1].replace('*', '')
        var_type = ' '.join(var_decl.split()[:-1])

        replacement = f'''{indent}// PHASE 5F: Thread-safe spatial grid validation
{indent}auto snapshot_{var_name} = SpatialGridQueryHelpers::FindCreatureByGuid({bot}, {guid});
{indent}{var_type} {var_name} = nullptr;
{indent}if (snapshot_{var_name})
{indent}{{
{indent}    {var_name} = ObjectAccessor::{method}(*{bot}, {guid});
{indent}}}
{indent}if ({var_name})'''

        replacements.append((match.start(), match.end(), replacement))

    # Pattern 2: var = ObjectAccessor::GetXXX(...); (direct assignment)
    pattern2 = re.compile(
        r'(\s+)(\w+)\s*=\s*ObjectAccessor::(GetUnit|GetCreature)\(\*([^,]+),\s*([^)]+)\);',
        re.MULTILINE
    )
    for match in pattern2.finditer(content):
        # Skip if already processed by pattern1
        skip = False
        for start, end, _ in replacements:
            if start <= match.start() < end:
                skip = True
                break
        if skip:
            continue

        indent = match.group(1)
        var_name = match.group(2)
        method = match.group(3)
        bot = match.group(4).strip()
        guid = match.group(5).strip()

        replacement = f'''{indent}// PHASE 5F: Thread-safe spatial grid validation
{indent}auto snapshot_{var_name} = SpatialGridQueryHelpers::FindCreatureByGuid({bot}, {guid});
{indent}{var_name} = nullptr;
{indent}if (snapshot_{var_name})
{indent}{{
{indent}    {var_name} = ObjectAccessor::{method}(*{bot}, {guid});
{indent}}}'''

        replacements.append((match.start(), match.end(), replacement))

    # Pattern 3: Type* var = ObjectAccessor::GetXXX(...); (declaration with assignment)
    pattern3 = re.compile(
        r'(\s+)([:\w]+\*?\s+\w+)\s*=\s*ObjectAccessor::(GetUnit|GetCreature)\(\*([^,]+),\s*([^)]+)\);',
        re.MULTILINE
    )
    for match in pattern3.finditer(content):
        # Skip if already processed
        skip = False
        for start, end, _ in replacements:
            if start <= match.start() < end:
                skip = True
                break
        if skip:
            continue

        indent = match.group(1)
        var_decl = match.group(2).strip()
        method = match.group(3)
        bot = match.group(4).strip()
        guid = match.group(5).strip()

        var_name = var_decl.split()[-1].replace('*', '')
        var_type = ' '.join(var_decl.split()[:-1])

        replacement = f'''{indent}// PHASE 5F: Thread-safe spatial grid validation
{indent}auto snapshot_{var_name} = SpatialGridQueryHelpers::FindCreatureByGuid({bot}, {guid});
{indent}{var_type} {var_name} = nullptr;
{indent}if (snapshot_{var_name})
{indent}{{
{indent}    {var_name} = ObjectAccessor::{method}(*{bot}, {guid});
{indent}}}'''

        replacements.append((match.start(), match.end(), replacement))

    # Apply replacements in reverse order to preserve positions
    replacements.sort(key=lambda x: x[0], reverse=True)

    for start, end, replacement in replacements:
        content = content[:start] + replacement + content[end:]

    return content

def process_file(filepath):
    """Process one file"""
    with open(filepath, 'r', encoding='utf-8') as f:
        original = f.read()

    content = add_include(original, filepath)
    content = fix_all_objectaccessor(content)

    if content != original:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        return True
    return False

def main():
    """Main"""
    print("=" * 80)
    print("FINAL COMPLETE ClassAI ObjectAccessor Fix")
    print("=" * 80)

    cpp_files = sorted(BASE_DIR.rglob("*.cpp"))
    modified = 0

    for filepath in cpp_files:
        if process_file(filepath):
            print(f"[FIXED] {filepath.relative_to(BASE_DIR)}")
            modified += 1

    print("=" * 80)
    print(f"COMPLETE: {modified} files modified")
    print("=" * 80)

if __name__ == "__main__":
    main()
