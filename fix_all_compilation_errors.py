#!/usr/bin/env python3
"""
Fix all compilation errors in Playerbot module:
1. Replace std::lock_guard<std::recursive_mutex> with std::lock_guard
2. Replace getMSTime() with GameTime::GetGameTimeMS()
"""

import os
import re
from pathlib import Path

def fix_file(file_path):
    """Fix compilation errors in a single file."""
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    original = content

    # Fix lock_guard type
    content = re.sub(
        r'std::lock_guard<std::recursive_mutex>',
        'std::lock_guard',
        content
    )

    # Fix getMSTime()
    content = re.sub(
        r'getMSTime\(\)',
        'GameTime::GetGameTimeMS()',
        content
    )

    if content != original:
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(content)
        return True
    return False

def main():
    playerbot_dir = Path('src/modules/Playerbot')

    count = 0
    for cpp_file in playerbot_dir.rglob('*.cpp'):
        if fix_file(cpp_file):
            print(f"Fixed: {cpp_file.relative_to(playerbot_dir)}")
            count += 1

    for h_file in playerbot_dir.rglob('*.h'):
        if fix_file(h_file):
            print(f"Fixed: {h_file.relative_to(playerbot_dir)}")
            count += 1

    print(f"\nTotal files fixed: {count}")

if __name__ == "__main__":
    main()
