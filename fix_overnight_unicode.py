#!/usr/bin/env python3
"""
Fix Unicode encoding in overnight_autonomous_mode.py subprocess calls.
Adds encoding='utf-8', errors='replace' to all subprocess.run() calls.
"""

import re

file_path = r'C:\TrinityBots\TrinityCore\.claude\scripts\overnight_autonomous_mode.py'

# Read the file
with open(file_path, 'r', encoding='utf-8') as f:
    content = f.read()

# Pattern 1: subprocess.run() with text=True but no encoding
# Replace:   text=True
# With:      text=True, encoding='utf-8', errors='replace'
content = re.sub(
    r'(subprocess\.run\([^)]*?)(\s*text=True)([^)]*?\))',
    r'\1\2, encoding="utf-8", errors="replace"\3',
    content
)

# Pattern 2: subprocess.run() with capture_output=True but no text/encoding
# Add text=True, encoding='utf-8', errors='replace' before the closing paren
content = re.sub(
    r'(subprocess\.run\([^)]*?capture_output=True)([^)]*?)(\))',
    lambda m: m.group(1) + ', text=True, encoding="utf-8", errors="replace"' + m.group(2) + m.group(3) if 'text=' not in m.group(0) else m.group(0),
    content
)

# Pattern 3: subprocess.Popen() - add encoding parameters if stdout/stderr are specified
content = re.sub(
    r'(subprocess\.Popen\([^)]*?(?:stdout|stderr)=[^)]*?)(\))',
    lambda m: m.group(1) + ', encoding="utf-8", errors="replace"' + m.group(2) if 'encoding=' not in m.group(0) else m.group(0),
    content
)

# Write back
with open(file_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("✅ Fixed Unicode encoding in overnight_autonomous_mode.py")
print("   Added encoding='utf-8', errors='replace' to subprocess calls")
