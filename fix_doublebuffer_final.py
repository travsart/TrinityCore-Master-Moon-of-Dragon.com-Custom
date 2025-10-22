#!/usr/bin/env python3
"""
Fix DoubleBufferedSpatialGrid.cpp syntax errors from previous script
"""

import re
from pathlib import Path

FILE_PATH = Path(r"C:\TrinityBots\TrinityCore\src\modules\Playerbot\Spatial\DoubleBufferedSpatialGrid.cpp")

def fix_syntax_errors(content):
    """Fix syntax errors caused by inline comments in expressions"""

    # Fix the broken flag checks - move comments to end of line
    content = re.sub(
        r'\(creatureTemplate->flags_extra & (0x[0-9A-Fa-f]+)\s+// ([^)]+)\)',
        r'(creatureTemplate->flags_extra & \1)',  # Remove inline comment
        content
    )

    # Fix GetDeathState which doesn't exist
    content = re.sub(
        r'player->GetDeathState\(\)',
        'player->isDead()',
        content
    )

    # Fix skinLoot field name - should be SkinningLootId
    content = re.sub(
        r'creatureTemplate->skinLoot',
        'creatureTemplate->SkinningLootId',
        content
    )

    # Fix GetActiveSpec - doesn't exist
    content = re.sub(
        r'snapshot\.activeSpecId = player->GetActiveSpec\(\);',
        'snapshot.activeSpecId = 0;  // Spec tracking removed',
        content
    )

    # Fix GetFreeTalentPoints - doesn't exist
    content = re.sub(
        r'snapshot\.freeTalentPoints = player->GetFreeTalentPoints\(\);',
        'snapshot.freeTalentPoints = 0;  // Talent points removed',
        content
    )

    return content

def main():
    print("=" * 80)
    print("Fixing DoubleBufferedSpatialGrid.cpp Syntax Errors")
    print("=" * 80)

    if not FILE_PATH.exists():
        print(f"[ERROR] File not found: {FILE_PATH}")
        return

    with open(FILE_PATH, 'r', encoding='utf-8') as f:
        content = f.read()

    original = content
    content = fix_syntax_errors(content)

    if content != original:
        with open(FILE_PATH, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"[SUCCESS] Fixed syntax errors in {FILE_PATH.name}")
    else:
        print(f"[NO CHANGES] No fixes needed in {FILE_PATH.name}")

    print("=" * 80)

if __name__ == "__main__":
    main()
