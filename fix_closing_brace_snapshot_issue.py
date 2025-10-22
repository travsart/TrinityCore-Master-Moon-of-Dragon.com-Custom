#!/usr/bin/env python3
"""
Fix the specific pattern where closing brace is joined with snapshot declaration:
    } auto snapshot_entity = ...

Should be:
    }
(and remove the duplicate snapshot block that follows)
"""

import re
from pathlib import Path

BASE_DIR = Path(r"C:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI")

def fix_joined_brace_snapshot(content):
    """Fix }snapshot or } snapshot patterns"""

    # Pattern 1: } auto snapshot_... (joined)
    # Remove the entire duplicate block that starts with this
    pattern1 = re.compile(
        r'(\s*\}\s*)(auto|const\s+auto\*?)\s+snapshot_(\w+)\s*=\s*SpatialGridQueryHelpers::(Find\w+)\([^;]+;[^}]*\}',
        re.MULTILINE | re.DOTALL
    )
    content = pattern1.sub(r'\1', content)

    # Pattern 2: Clean up any remaining } followed immediately by entity/target = nullptr
    pattern2 = re.compile(
        r'\}\s*(entity|target|snapshot_\w+)\s*=\s*nullptr;',
        re.MULTILINE
    )
    content = pattern2.sub(r'}', content)

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
    content = fix_joined_brace_snapshot(content)

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
    print("Fixing Joined Brace + Snapshot Issues")
    print("=" * 80)

    # Find all .cpp files in ClassAI directory
    cpp_files = list(BASE_DIR.rglob("*.cpp"))

    fixed = 0
    for filepath in cpp_files:
        rel_path = filepath.relative_to(BASE_DIR.parent)
        if process_file(filepath):
            print(f"[FIXED] {rel_path}")
            fixed += 1

    print("=" * 80)
    print(f"Complete: {fixed} files fixed out of {len(cpp_files)} total files")
    print("=" * 80)

if __name__ == "__main__":
    main()
