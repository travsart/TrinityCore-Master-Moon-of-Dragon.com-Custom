#!/usr/bin/env python3
"""
Fix compilation errors in Cell::Visit replacements
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
print("FIXING CELL::VISIT REPLACEMENT COMPILATION ERRORS")
print("=" * 80)

for file_path in files:
    path = Path(file_path)
    print(f"\nProcessing {path.name}...")

    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()

    original = content

    # Fix 1: Instance()->GetOrCreateGrid → Instance().GetOrCreateGrid
    content = re.sub(
        r'Playerbot::SpatialGridManager::Instance\(\)->GetOrCreateGrid',
        'Playerbot::SpatialGridManager::Instance().GetOrCreateGrid',
        content
    )

    # Fix 2: ObjectGuid const& guid : guids → ObjectGuid guid : guids
    content = re.sub(
        r'for \(ObjectGuid const& (guid) : (guids)\)',
        r'for (ObjectGuid \1 : \2)',
        content
    )

    if content != original:
        with open(path, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"  [OK] Fixed Instance() reference and for-loop syntax")
    else:
        print(f"  [SKIP] No changes needed")

print("\n" + "=" * 80)
print("Compilation errors fixed - ready to rebuild")
print("=" * 80)
