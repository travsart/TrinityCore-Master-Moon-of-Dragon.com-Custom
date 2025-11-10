#!/usr/bin/env python3
"""
Fix CombatStateAnalyzer.cpp by removing all orphaned null checks
"""

import re

def fix_file():
    file_path = "src/modules/Playerbot/AI/Combat/CombatStateAnalyzer.cpp"

    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    original = content

    # Pattern for orphaned null checks (both inline and block style)
    # This removes any if statement with TC_LOG_ERROR for nullcheck
    patterns = [
        # Block style: if (!ptr)\n{\n    TC_LOG_ERROR...\n    return...\n}
        r'if\s*\(![\w]+\)\s*\n\s*\{\s*\n\s*TC_LOG_ERROR\("playerbot\.nullcheck"[^\}]+\}\s*\n',
        # Inline style without newline after condition
        r'if\s*\(![\w]+\)\s*\{\s*TC_LOG_ERROR\("playerbot\.nullcheck"[^\}]+\}\s*',
    ]

    for pattern in patterns:
        content = re.sub(pattern, '', content, flags=re.MULTILINE)

    # Also fix return nullptr in bool functions
    content = re.sub(r'return\s+nullptr;', 'return false;', content)

    # Write back
    with open(file_path, 'w', encoding='utf-8') as f:
        f.write(content)

    lines_removed = original.count('\n') - content.count('\n')
    print(f"Removed {lines_removed} lines from {file_path}")

if __name__ == "__main__":
    fix_file()
