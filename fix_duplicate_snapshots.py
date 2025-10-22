#!/usr/bin/env python3
"""
Fix duplicate snapshot code blocks created by malformed script output
Pattern: } snapshot_X = ... should be }\\nif (snapshot_X)
"""

import re
from pathlib import Path

BASE_DIR = Path(r"C:\TrinityBots\TrinityCore\src\modules\Playerbot")

def fix_duplicates(content):
    """Remove duplicate snapshot blocks"""
    # Pattern matches: closing brace immediately followed by snapshot reassignment
    # Example: "        }auto snapshot_target = SpatialGridQueryHelpers::..."
    pattern = re.compile(
        r'(\s*)\}(auto|::Unit\*|Creature\*|Unit\*)\s+(snapshot_\w+)\s*=\s*SpatialGridQueryHelpers::(Find\w+)\(([^)]+)\);\s*\n'
        r'\s*\3\s*=\s*nullptr;\s*\n'
        r'\s*if\s*\(\s*\3\s*\)\s*\n'
        r'\s*\{\s*\n'
        r'\s*\w+\s*=\s*ObjectAccessor::(Get\w+)\([^)]+\);\s*\n'
        r'\s*\}',
        re.MULTILINE
    )

    def replace(match):
        # Just keep the closing brace with proper spacing
        indent = match.group(1)
        return f'{indent}}}'

    return pattern.sub(replace, content)

def fix_whitespace_after_comments(content):
    """Clean up excessive newlines after PHASE 5F comments"""
    # Pattern: PHASE 5F comment followed by multiple blank lines
    pattern = re.compile(
        r'(// PHASE 5F: Thread-safe spatial grid validation)\s*\n\s*\n(\s*auto snapshot_)',
        re.MULTILINE
    )
    return pattern.sub(r'\1\n\2', content)

def fix_whitespace_in_blocks(content):
    """Remove excessive blank lines within snapshot validation blocks"""
    # Pattern: blank lines within if blocks
    pattern1 = re.compile(
        r'(\s+auto snapshot_\w+ = SpatialGridQueryHelpers::Find\w+\([^)]+\);)\s*\n\s*\n(\s+\w+\*?\s+\w+ = nullptr;)',
        re.MULTILINE
    )
    content = pattern1.sub(r'\1\n\2', content)

    pattern2 = re.compile(
        r'(\s+\w+\*?\s+\w+ = nullptr;)\s*\n\s*\n(\s+if \(snapshot_)',
        re.MULTILINE
    )
    content = pattern2.sub(r'\1\n\2', content)

    pattern3 = re.compile(
        r'(if \(snapshot_\w+\))\s*\n\s*\n(\s+\{)',
        re.MULTILINE
    )
    content = pattern3.sub(r'\1\n\2', content)

    pattern4 = re.compile(
        r'(\s+\{)\s*\n\s*\n(\s+\w+ = ObjectAccessor::)',
        re.MULTILINE
    )
    content = pattern4.sub(r'\1\n\2', content)

    return content

def process_file(filepath):
    """Process a single file"""
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    original = content

    # Apply fixes
    content = fix_duplicates(content)
    content = fix_whitespace_after_comments(content)
    content = fix_whitespace_in_blocks(content)

    if content != original:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        return True

    return False

def main():
    """Main execution"""
    print("=" * 80)
    print("Fixing Duplicate Snapshot Blocks in ClassAI")
    print("=" * 80)

    # Get all .cpp files
    cpp_files = sorted(BASE_DIR.rglob("*.cpp"))

    fixed = 0
    for filepath in cpp_files:
        if process_file(filepath):
            print(f"[FIXED] {filepath.relative_to(BASE_DIR)}")
            fixed += 1

    print("=" * 80)
    print(f"Complete: {fixed} files fixed")
    print("=" * 80)

if __name__ == "__main__":
    main()
