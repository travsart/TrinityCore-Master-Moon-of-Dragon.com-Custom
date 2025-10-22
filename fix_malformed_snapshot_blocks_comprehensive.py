#!/usr/bin/env python3
"""
Comprehensive fix for all malformed snapshot blocks.
Issues to fix:
1. }bot_target = ... (missing newline before bot_target)
2. Duplicate snapshot_entity declarations
3. bot_target references (should be snapshot_target or target)
"""

import re
from pathlib import Path

BASE_DIR = Path(r"C:\TrinityBots\TrinityCore\src\modules\Playerbot")

def fix_malformed_code(content):
    """Fix malformed snapshot validation blocks"""
    original = content

    # Pattern 1: Fix "}bot_target" (missing newline)
    content = re.sub(r'\}bot_target\s*=', '}\\n        const auto* snapshot_target =', content)
    content = re.sub(r'\}snapshot_', '}\\n        const auto* snapshot_', content)

    # Pattern 2: Replace bot_target with snapshot_target consistently
    # Only if not preceded by dot or arrow (to avoid replacing obj.bot_target)
    content = re.sub(r'(?<![.\->])bot_target\b', 'snapshot_target', content)

    # Pattern 3: Remove duplicate consecutive snapshot declarations
    # This removes lines like:
    #     const auto* snapshot_entity = ...;
    #     Creature* target = nullptr;
    #     if (snapshot_entity) { ... }
    #     const auto* snapshot_entity = ...;  <-- DUPLICATE
    pattern_dup = re.compile(
        r'(const\s+auto\*\s+snapshot_(\w+)\s*=\s*SpatialGridQueryHelpers::Find\w+[^;]+;)\s*\n'
        r'\s*Creature\*\s+\w+\s*=\s*nullptr;\s*\n'
        r'\s*if\s*\(\s*snapshot_\2\s*\)\s*\n'
        r'\s*\{[^}]+\}\s*\n'
        r'\s*(const\s+auto\*\s+snapshot_\2\s*=\s*SpatialGridQueryHelpers::Find\w+[^;]+;)',
        re.MULTILINE | re.DOTALL
    )
    content = pattern_dup.sub(r'\1', content)

    # Pattern 4: Fix duplicate target = nullptr / if (snapshot) blocks
    pattern_dup2 = re.compile(
        r'(\s+)Creature\*\s+target\s*=\s*nullptr;\s*\n'
        r'\s*if\s*\(\s*snapshot_\w+\s*\)\s*\n'
        r'\s*\{\s*\n'
        r'\s*target\s*=\s*ObjectAccessor::GetUnit[^}]+\}\s*\n'
        r'\s*Creature\*\s+target\s*=\s*nullptr;\s*\n'  # <-- DUPLICATE
        r'\s*if\s*\(\s*snapshot_\w+\s*\)\s*\n'
        r'\s*\{\s*\n'
        r'\s*target\s*=\s*ObjectAccessor::GetUnit[^}]+\}',
        re.MULTILINE | re.DOTALL
    )

    def keep_first_target_block(match):
        # Keep only the first block
        lines = match.group(0).split('\n')
        indent = match.group(1)
        # Find where duplicate starts (second "Creature* target")
        for i, line in enumerate(lines):
            if i > 0 and 'Creature* target' in line:
                # Return only up to this point
                return '\n'.join(lines[:i])
        return match.group(0)

    content = pattern_dup2.sub(keep_first_target_block, content)

    return content

def process_file(filepath):
    """Fix one file"""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
    except Exception as e:
        print(f"[ERROR] Could not read {filepath}: {e}")
        return False

    original = content
    content = fix_malformed_code(content)

    if content != original:
        try:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(content)
            return True
        except Exception as e:
            print(f"[ERROR] Could not write {filepath}: {e}")
            return False
    return False

def main():
    """Main execution"""
    print("=" * 80)
    print("Comprehensive Fix for Malformed Snapshot Blocks")
    print("=" * 80)

    # Find all .cpp files in ClassAI directory
    classai_dir = BASE_DIR / "AI" / "ClassAI"
    cpp_files = list(classai_dir.rglob("*.cpp"))

    fixed = 0
    for filepath in cpp_files:
        rel_path = filepath.relative_to(BASE_DIR)
        if process_file(filepath):
            print(f"[FIXED] {rel_path}")
            fixed += 1

    print("=" * 80)
    print(f"Complete: {fixed} files fixed out of {len(cpp_files)} total files")
    print("=" * 80)

if __name__ == "__main__":
    main()
