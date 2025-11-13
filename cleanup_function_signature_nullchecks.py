#!/usr/bin/env python3
"""
Remove null checks inserted between function signatures and opening braces.
"""

import re
import sys
from pathlib import Path

def remove_signature_null_checks(file_path):
    """Remove null checks between function signatures and opening braces."""
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    original_content = content
    changes = 0

    # Pattern: Function signature followed by null check before opening brace
    # Matches: )\nif (!ptr)\n{\n    TC_LOG_ERROR(...)\n    return...\n}\n{
    pattern = re.compile(
        r'(\)\s*\n)'  # Closing paren of function signature
        r'if\s*\([^)]+\)\s*\n'  # if (!something)
        r'\s*\{\s*\n'  # {
        r'\s*TC_LOG_ERROR\("playerbot\.nullcheck"[^\n]+\n'  # TC_LOG_ERROR line
        r'(?:\s*return[^;]*;\s*\n)?'  # optional return statement
        r'\s*\}\s*\n'  # }
        r'(\{\s*\n)',  # Opening brace of actual function body
        re.MULTILINE
    )

    # Replace with just the function signature parts
    new_content = pattern.sub(r'\1\2', content)
    changes = content.count('\n') - new_content.count('\n')

    # Also handle standalone orphaned checks that return nullptr in bool/void functions
    pattern2 = re.compile(
        r'^\s+if\s*\([^)]+\)\s*\n'
        r'\s*\{\s*\n'
        r'\s*TC_LOG_ERROR\("playerbot\.nullcheck"[^\n]+\n'
        r'\s*return\s+nullptr;\s*\n'
        r'\s*\}\s*\n',
        re.MULTILINE
    )
    new_content = pattern2.sub('', new_content)
    changes += content.count('\n') - new_content.count('\n')

    # Handle TC_LOG_MESSAGE_BODY variant
    pattern3 = re.compile(
        r'^\s+if\s*\([^)]+\)\s*\n'
        r'\s*\{\s*\n'
        r'\s*TC_LOG_MESSAGE_BODY\([^\n]+\n'
        r'(?:\s*return[^;]*;\s*\n)?'
        r'\s*\}\s*\n',
        re.MULTILINE
    )
    new_content = pattern3.sub('', new_content)
    changes += new_content.count('\n') - content.count('\n')

    # Only write if changes were made
    if new_content != original_content:
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(new_content)
        return True, changes

    return False, 0

def main():
    if len(sys.argv) < 2:
        print("Usage: python cleanup_function_signature_nullchecks.py <file>")
        sys.exit(1)

    file_path = Path(sys.argv[1])
    if not file_path.exists():
        print(f"Error: File {file_path} does not exist")
        sys.exit(1)

    print(f"Cleaning {file_path}...")
    modified, changes = remove_signature_null_checks(file_path)

    if modified:
        print(f"  Fixed: {changes} lines removed")
    else:
        print("  No changes needed")

if __name__ == "__main__":
    main()
