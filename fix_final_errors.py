#!/usr/bin/env python3
"""
Fix all remaining compilation errors from Phase 5F
1. Remove duplicate snapshot blocks
2. Fix auto* = nullptr (change to Creature* = nullptr)
3. Fix typos (ot_target -> bot_target, hot_target -> bot_target, etc.)
"""

import re
from pathlib import Path

BASE_DIR = Path(r"C:\TrinityBots\TrinityCore\src\modules\Playerbot")

# Files with known errors
PROBLEM_FILES = [
    "AI/ClassAI/Shamans/ElementalSpecialization.cpp",
    "AI/ClassAI/DeathKnights/UnholySpecialization.cpp",
    "AI/ClassAI/Shamans/ShamanAI.cpp",
]

def fix_auto_pointer_nullptr(content):
    """Fix: auto* entity = nullptr; -> Creature* entity = nullptr;"""
    pattern = re.compile(r'(\s+)auto\*\s+(\w+)\s*=\s*nullptr;', re.MULTILINE)
    return pattern.sub(r'\1Creature* \2 = nullptr;', content)

def fix_typos(content):
    """Fix variable name typos"""
    # ot_target -> bot_target
    content = re.sub(r'\bot_target\b', 'bot_target', content)
    # hot_target -> bot_target
    content = re.sub(r'\bhot_target\b', 'bot_target', content)
    return content

def fix_malformed_snapshot_blocks(content):
    """Remove duplicate/malformed snapshot blocks like '} auto snapshot_entity ='"""
    # Pattern: closing brace followed by snapshot reassignment
    pattern = re.compile(
        r'(\s*)\}\s*(auto|const\s+\w+\*)\s+(snapshot_\w+)\s*=\s*SpatialGridQueryHelpers::(Find\w+)\([^)]+\);\s*\n'
        r'[^\n]*\s*=\s*nullptr;\s*\n'
        r'\s*if\s*\(\s*\3\s*\)\s*\n'
        r'\s*\{\s*\n'
        r'[^\}]+\}\s*\n?',
        re.MULTILINE | re.DOTALL
    )
    content = pattern.sub(r'\1}\n', content)

    # Also fix: } o snapshot_entity (typo with 'o' instead of proper code)
    content = re.sub(r'\}\s*o\s+snapshot_', '}\n        // PHASE 5F: Thread-safe spatial grid validation\n        auto snapshot_', content)

    return content

def process_file(filepath):
    """Fix one file"""
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    original = content

    # Apply all fixes
    content = fix_auto_pointer_nullptr(content)
    content = fix_typos(content)
    content = fix_malformed_snapshot_blocks(content)

    if content != original:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        return True
    return False

def main():
    """Main execution"""
    print("=" * 80)
    print("Fixing Phase 5F Compilation Errors")
    print("=" * 80)

    fixed = 0
    for rel_path in PROBLEM_FILES:
        filepath = BASE_DIR / rel_path
        if not filepath.exists():
            print(f"[SKIP] {rel_path} - not found")
            continue

        if process_file(filepath):
            print(f"[FIXED] {rel_path}")
            fixed += 1
        else:
            print(f"[NO CHANGE] {rel_path}")

    print("=" * 80)
    print(f"Complete: {fixed} files fixed")
    print("=" * 80)

if __name__ == "__main__":
    main()
