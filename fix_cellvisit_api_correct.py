#!/usr/bin/env python3
"""
Fix Cell::Visit replacements with correct SpatialGridManager API
"""

from pathlib import Path
import re

files = [
    r'C:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI\CombatSpecializationTemplates.h',
    r'C:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI\DemonHunters\HavocDemonHunterRefactored.h',
    r'C:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI\Hunters\SurvivalHunterRefactored.h',
    r'C:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI\RoleSpecializations.h'
]

print("=" * 80)
print("FIXING SPATIALGRIDMANAGER API CALLS")
print("=" * 80)

for file_path in files:
    path = Path(file_path)
    print(f"\nProcessing {path.name}...")

    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()

    original = content

    # Fix: GetOrCreateGrid → GetGrid
    content = re.sub(
        r'\.GetOrCreateGrid\(',
        '.GetGrid(',
        content
    )

    if content != original:
        with open(path, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"  [OK] Fixed GetOrCreateGrid → GetGrid")
    else:
        print(f"  [SKIP] No changes needed")

print("\n" + "=" * 80)
print("API calls fixed - ready to rebuild")
print("=" * 80)
