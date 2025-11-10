#!/usr/bin/env python3
"""
Comprehensive cleanup of all orphaned null check patterns
"""

import re
from pathlib import Path

def cleanup_file(file_path):
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    original = content

    # Pattern 1: Standard orphaned null check blocks
    content = re.sub(
        r'\s*if\s*\(![\w]+\)\s*\n?\s*\{\s*\n?\s*TC_LOG_ERROR\("playerbot\.nullcheck"[^\}]+\}\s*\n?',
        '',
        content,
        flags=re.MULTILINE
    )

    # Pattern 2: Orphaned return statements (return false/nullptr/0; outside functions)
    content = re.sub(
        r'\s*return\s+(false|nullptr|0)\s*;\s*\n\s*\}\s*\n',
        '',
        content,
        flags=re.MULTILINE
    )

    # Pattern 3: Just orphaned "return;" statements
    content = re.sub(
        r'\s*if\s*\(![\w]+\)\s*\n?\s*\{\s*\n?\s*TC_LOG_ERROR[^\}]+\}\s*\n?',
        '',
        content,
        flags=re.MULTILINE
    )

    if content != original:
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(content)
        return True
    return False

def main():
    playerbot_dir = Path('src/modules/Playerbot')

    count = 0
    for cpp_file in list(playerbot_dir.rglob('*.cpp')) + list(playerbot_dir.rglob('*.h')):
        if cleanup_file(cpp_file):
            print(f"Cleaned: {cpp_file.relative_to(playerbot_dir)}")
            count += 1

    print(f"\nTotal files cleaned: {count}")

if __name__ == "__main__":
    main()
