#!/usr/bin/env python3
"""
Phase 7C: Remove orphaned null check corruption patterns
Fixes C2059 syntax errors caused by orphaned if blocks interrupting code
"""

import re
import os
from pathlib import Path

# Pattern to find orphaned null checks
ORPHANED_PATTERN = re.compile(
    r'(\s+)if\s*\(\s*!\s*\w+\s*\)\s*\n\s*{\s*\n\s*TC_LOG_ERROR\([^)]+\);\s*\n\s*return[^;]*;\s*\n\s*}',
    re.MULTILINE
)

def fix_file(filepath):
    """Remove orphaned null checks from a file."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()

        original_content = content
        matches = list(ORPHANED_PATTERN.finditer(content))

        if not matches:
            return False, "no orphaned patterns found"

        # Remove all orphaned patterns
        content = ORPHANED_PATTERN.sub('', content)

        # Write back only if changed
        if content != original_content:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(content)
            return True, f"removed {len(matches)} orphaned null check(s)"

        return False, "no changes after pattern removal"

    except Exception as e:
        return False, f"error: {e}"

def main():
    print("=== Phase 7C: Removing orphaned null check corruption ===")
    print()

    # Find all C++ files in Playerbot module
    playerbot_dir = Path("src/modules/Playerbot")
    cpp_files = list(playerbot_dir.rglob("*.cpp"))
    h_files = list(playerbot_dir.rglob("*.h"))
    all_files = cpp_files + h_files

    print(f"Scanning {len(all_files)} files...")
    print()

    fixed = 0
    skipped = 0
    total_patterns = 0

    for filepath in sorted(all_files):
        success, message = fix_file(filepath)

        if success:
            # Extract count from message
            count = int(message.split()[1])
            total_patterns += count
            print(f"FIXED: {filepath.relative_to('src/modules/Playerbot')} ({message})")
            fixed += 1
        else:
            if "no orphaned patterns" not in message:
                print(f"SKIP: {filepath.relative_to('src/modules/Playerbot')} ({message})")
            skipped += 1

    print()
    print("=== Summary ===")
    print(f"Files fixed: {fixed}")
    print(f"Files skipped: {skipped}")
    print(f"Total orphaned patterns removed: {total_patterns}")
    print(f"Total files scanned: {len(all_files)}")
    print()
    print("Expected error reduction: ~349 C2059 errors eliminated")

if __name__ == "__main__":
    main()
