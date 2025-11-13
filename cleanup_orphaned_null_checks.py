#!/usr/bin/env python3
"""
Remove orphaned null safety checks from TrinityCore Playerbot module.
These are if statements that appear outside function bodies, causing syntax errors.
"""

import re
import sys
from pathlib import Path

def remove_orphaned_null_checks(file_path):
    """Remove orphaned null check blocks from a C++ file."""
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    original_content = content
    changes = 0

    # Pattern 1: Orphaned if block with TC_LOG_ERROR (most common pattern)
    # Matches: if (!ptr)\n{\n    TC_LOG_ERROR(...)\n    return...\n}
    pattern1 = re.compile(
        r'^\s+if\s*\([^)]+\)\s*\n'  # if (!something)
        r'\s*\{\s*\n'  # {
        r'\s*TC_LOG_ERROR\("playerbot\.nullcheck"[^\n]+\n'  # TC_LOG_ERROR line
        r'(?:\s*return[^;]*;\s*\n)?'  # optional return statement
        r'\s*\}\s*\n',  # }
        re.MULTILINE
    )

    # Pattern 2: Orphaned if with TC_LOG_MESSAGE_BODY
    pattern2 = re.compile(
        r'^\s+if\s*\([^)]+\)\s*\n'
        r'\s*\{\s*\n'
        r'\s*TC_LOG_MESSAGE_BODY\([^\n]+\n'
        r'(?:\s*return[^;]*;\s*\n)?'
        r'\s*\}\s*\n',
        re.MULTILINE
    )

    # Remove Pattern 1 occurrences
    new_content = pattern1.sub('', content)
    changes += content.count('\n') - new_content.count('\n')
    content = new_content

    # Remove Pattern 2 occurrences
    new_content = pattern2.sub('', content)
    changes += content.count('\n') - new_content.count('\n')
    content = new_content

    # Only write if changes were made
    if content != original_content:
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(content)
        return changes

    return 0

def main():
    if len(sys.argv) < 2:
        print("Usage: python cleanup_orphaned_null_checks.py <directory>")
        sys.exit(1)

    root_dir = Path(sys.argv[1])
    if not root_dir.exists():
        print(f"Error: Directory {root_dir} does not exist")
        sys.exit(1)

    print(f"Scanning {root_dir} for C++ files...")

    total_files = 0
    total_changes = 0

    for cpp_file in root_dir.rglob("*.cpp"):
        changes = remove_orphaned_null_checks(cpp_file)
        if changes > 0:
            total_files += 1
            total_changes += changes
            print(f"  Fixed {cpp_file.name}: {changes} lines removed")

    print(f"\nSummary:")
    print(f"  Files modified: {total_files}")
    print(f"  Total lines removed: {total_changes}")

if __name__ == "__main__":
    main()
