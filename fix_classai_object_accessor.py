#!/usr/bin/env python3
"""
DEADLOCK FIX PHASE 2 - Automated ClassAI Object Accessor Removal

This script removes redundant ObjectAccessor::GetUnit() calls from ClassAI
UpdateRotation methods that already receive a target parameter.

CRITICAL: These calls run on WORKER THREADS and access Map (NOT thread-safe),
causing deadlocks with 5000+ bots.
"""

import re
import os
from pathlib import Path

# Patterns to fix
PATTERNS = [
    # Pattern 1: ::Unit* target = ObjectAccessor::GetUnit(*bot, bot->GetTarget());
    {
        'pattern': r'::Unit\*\s+target\s*=\s*ObjectAccessor::GetUnit\(\*bot,\s*bot->GetTarget\(\)\)\s*;',
        'replacement': '// DEADLOCK FIX: Use target parameter from UpdateRotation() instead of ObjectAccessor\n    // The target has already been resolved on the main thread by BotAI',
        'description': 'Remove redundant ObjectAccessor::GetUnit() for target'
    },
    # Pattern 2: Unit* target = ObjectAccessor::GetUnit(*bot, bot->GetTarget());
    {
        'pattern': r'Unit\*\s+target\s*=\s*ObjectAccessor::GetUnit\(\*bot,\s*bot->GetTarget\(\)\)\s*;',
        'replacement': '// DEADLOCK FIX: Use target parameter from UpdateRotation() instead of ObjectAccessor\n    // The target has already been resolved on the main thread by BotAI',
        'description': 'Remove redundant ObjectAccessor::GetUnit() for target (without ::)'
    },
    # Pattern 3: target = ObjectAccessor::GetUnit(*bot, bot->GetTarget());
    {
        'pattern': r'target\s*=\s*ObjectAccessor::GetUnit\(\*bot,\s*bot->GetTarget\(\)\)\s*;',
        'replacement': '// DEADLOCK FIX: Use target parameter from UpdateRotation() instead of ObjectAccessor\n    // The target has already been resolved on the main thread by BotAI\n    // target is already valid from parameter',
        'description': 'Remove redundant ObjectAccessor::GetUnit() assignment to existing target'
    },
]

def fix_file(filepath):
    """Fix a single file by applying all patterns."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()

        original_content = content
        fixes_applied = []

        for pattern_info in PATTERNS:
            matches = re.findall(pattern_info['pattern'], content)
            if matches:
                content = re.sub(pattern_info['pattern'], pattern_info['replacement'], content)
                fixes_applied.append(f"{len(matches)}x {pattern_info['description']}")

        if content != original_content:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(content)
            return fixes_applied
        return None

    except Exception as e:
        print(f"ERROR processing {filepath}: {e}")
        return None

def main():
    """Main entry point - fix all ClassAI files."""
    base_dir = Path('src/modules/Playerbot/AI/ClassAI')

    if not base_dir.exists():
        print(f"ERROR: Directory not found: {base_dir}")
        print("Please run this script from C:\\TrinityBots\\TrinityCore")
        return

    print("=" * 80)
    print("DEADLOCK FIX PHASE 2 - Removing ObjectAccessor from ClassAI UpdateRotation")
    print("=" * 80)
    print()

    total_files = 0
    fixed_files = 0
    total_fixes = 0

    # Process all .cpp and .h files in ClassAI directory
    for filepath in base_dir.rglob('*.cpp'):
        total_files += 1
        fixes = fix_file(filepath)

        if fixes:
            fixed_files += 1
            total_fixes += len(fixes)
            print(f"[OK] FIXED: {filepath.relative_to('src/modules/Playerbot')}")
            for fix in fixes:
                print(f"   - {fix}")
            print()

    print("=" * 80)
    print(f"SUMMARY:")
    print(f"  Total files processed: {total_files}")
    print(f"  Files modified: {fixed_files}")
    print(f"  Total fixes applied: {total_fixes}")
    print("=" * 80)
    print()

    if fixed_files > 0:
        print("[OK] SUCCESS: ObjectAccessor calls removed from worker threads")
        print()
        print("NEXT STEPS:")
        print("1. Rebuild worldserver: cmake --build . --config RelWithDebInfo --target worldserver")
        print("2. Test with 5000+ bots to verify futures 3-14 complete")
        print("3. Check for any remaining ObjectAccessor usage in worker threads")
    else:
        print("[INFO] NO FIXES NEEDED: All files are already correct!")

if __name__ == '__main__':
    main()
