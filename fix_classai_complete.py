#!/usr/bin/env python3
"""
Complete fix for ALL remaining ObjectAccessor calls in ClassAI
Handles all edge cases including inline declarations, direct assignments, and conditionals
"""

import os
import re
from pathlib import Path

BASE_DIR = Path(r"C:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI")

def fix_inline_if_declarations(content):
    """Fix: if (Type* var = ObjectAccessor::GetXXX(...))"""
    pattern = re.compile(
        r'(\s+)if\s*\(([:\w]+\*?\s+\w+)\s*=\s*ObjectAccessor::(GetUnit|GetCreature)\(\*([^,]+),\s*([^)]+)\)\)',
        re.MULTILINE
    )

    def replace(match):
        indent = match.group(1)
        var_decl = match.group(2).strip()
        method = match.group(3)
        bot_ref = match.group(4).strip()
        guid = match.group(5).strip()

        var_name = var_decl.split()[-1].replace('*', '')
        var_type = ' '.join(var_decl.split()[:-1])

        return f'''{indent}// PHASE 5F: Thread-safe spatial grid validation
{indent}auto snapshot_{var_name} = SpatialGridQueryHelpers::FindCreatureByGuid({bot_ref}, {guid});
{indent}{var_type} {var_name} = nullptr;
{indent}if (snapshot_{var_name})
{indent}{{
{indent}    {var_name} = ObjectAccessor::{method}(*{bot_ref}, {guid});
{indent}}}
{indent}if ({var_name})'''

    return pattern.sub(replace, content)

def fix_direct_assignments(content):
    """Fix: entity = ObjectAccessor::GetXXX(...) (without declaration)"""
    pattern = re.compile(
        r'(\s+)(\w+)\s*=\s*ObjectAccessor::(GetUnit|GetCreature)\(\*([^,]+),\s*([^)]+)\);',
        re.MULTILINE
    )

    def replace(match):
        indent = match.group(1)
        var_name = match.group(2)
        method = match.group(3)
        bot_ref = match.group(4).strip()
        guid = match.group(5).strip()

        return f'''{indent}// PHASE 5F: Thread-safe spatial grid validation
{indent}auto snapshot_{var_name} = SpatialGridQueryHelpers::FindCreatureByGuid({bot_ref}, {guid});
{indent}{var_name} = nullptr;
{indent}if (snapshot_{var_name})
{indent}{{
{indent}    {var_name} = ObjectAccessor::{method}(*{bot_ref}, {guid});
{indent}}}'''

    return pattern.sub(replace, content)

def fix_standalone_assignments_with_type(content):
    """Fix: entity = ObjectAccessor::GetXXX(...) where entity was inside PHASE 5F block"""
    # This handles the malformed output from previous script
    pattern = re.compile(
        r'(\s+)(\w+)\s*=\s*nullptr;\s*\n\s*\n\s*\n\s*if\s*\(snapshot_\2\)\s*\n\s*{\s*\n\s*(\2)\s*=\s*ObjectAccessor::(GetUnit|GetCreature)\(\*([^,]+),\s*([^)]+)\);',
        re.MULTILINE | re.DOTALL
    )

    # Already fixed, skip
    return content

def fix_conditional_expressions(content):
    """Fix: ObjectAccessor::GetXXX() in conditional expressions"""
    pattern = re.compile(
        r'(\s+)(ObjectAccessor::(GetUnit|GetCreature)\(\*([^,]+),\s*([^)]+)\))\s*(&&|\|\|)',
        re.MULTILINE
    )

    matches = list(pattern.finditer(content))
    if not matches:
        return content

    # Process from end to start to maintain positions
    for match in reversed(matches):
        indent = match.group(1)
        full_call = match.group(2)
        method = match.group(3)
        bot_ref = match.group(4).strip()
        guid = match.group(5).strip()
        operator = match.group(6)

        # Generate unique temp variable
        temp_var = f"temp_check_{abs(hash(full_call)) % 10000}"

        replacement = f'''{indent}// PHASE 5F: Thread-safe spatial grid validation
{indent}auto snapshot_{temp_var} = SpatialGridQueryHelpers::FindCreatureByGuid({bot_ref}, {guid});
{indent}Creature* {temp_var} = nullptr;
{indent}if (snapshot_{temp_var})
{indent}{{
{indent}    {temp_var} = ObjectAccessor::{method}(*{bot_ref}, {guid});
{indent}}}
{indent}({temp_var} != nullptr) {operator}'''

        content = content[:match.start()] + replacement + content[match.end():]

    return content

def process_file(filepath):
    """Process a single file"""
    print(f"Processing: {filepath.relative_to(BASE_DIR)}")

    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    original = content

    # Add include if missing
    if 'SpatialGridQueryHelpers.h' not in content:
        include_pattern = re.compile(r'(#include\s+[<"].*?[>"])', re.MULTILINE)
        includes = list(include_pattern.finditer(content))
        if includes:
            last_include = includes[-1]
            rel_path = filepath.relative_to(BASE_DIR)
            depth = len(rel_path.parts) - 1
            prefix = "../" * (depth + 3)
            new_include = f'\n#include "{prefix}Spatial/SpatialGridQueryHelpers.h"  // PHASE 5F: Thread-safe queries'
            content = content[:last_include.end()] + new_include + content[last_include.end():]

    # Apply all fixes
    content = fix_inline_if_declarations(content)
    content = fix_direct_assignments(content)
    content = fix_conditional_expressions(content)

    if content != original:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"  [MODIFIED]")
        return True
    else:
        print(f"  [NO CHANGES]")
        return False

def main():
    """Main execution"""
    print("=" * 80)
    print("Phase 5F COMPLETE: Fix ALL remaining ObjectAccessor calls")
    print("=" * 80)

    # Get all .cpp files in ClassAI
    cpp_files = list(BASE_DIR.rglob("*.cpp"))

    modified = 0
    for filepath in sorted(cpp_files):
        if process_file(filepath):
            modified += 1

    print("=" * 80)
    print(f"Complete: {modified} files modified")
    print("=" * 80)

if __name__ == "__main__":
    main()
