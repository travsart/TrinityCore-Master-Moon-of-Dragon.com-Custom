#!/usr/bin/env python3
"""
Fix *Refactored.h Files - Remove Old Base Class Inheritance
===========================================================

Removes inheritance from old *Specialization base classes in all *Refactored.h files.

The *Refactored.h files use template-based architecture and should NOT inherit from
the old polymorphic *Specialization base classes which have been deleted.

Pattern to fix:
    class FooRefactored : public TemplateBase<Resource>, public FooSpecialization

Should become:
    class FooRefactored : public TemplateBase<Resource>

Created: 2025-10-23
Purpose: Fix compilation errors from undefined base classes
"""

import re
from pathlib import Path
from typing import List, Tuple

# Mappings of class names to their old base class names
OLD_BASE_CLASSES = {
    # Death Knight
    'BloodDeathKnightRefactored': 'DeathKnightSpecialization',
    'FrostDeathKnightRefactored': 'DeathKnightSpecialization',
    'UnholyDeathKnightRefactored': 'DeathKnightSpecialization',

    # Demon Hunter
    'HavocDemonHunterRefactored': 'DemonHunterSpecialization',
    'VengeanceDemonHunterRefactored': 'DemonHunterSpecialization',

    # Druid
    'BalanceDruidRefactored': 'DruidSpecialization',
    'FeralDruidRefactored': 'DruidSpecialization',
    'GuardianDruidRefactored': 'DruidSpecialization',
    'RestorationDruidRefactored': 'DruidSpecialization',

    # Evoker
    'DevastationEvokerRefactored': 'EvokerSpecialization',
    'PreservationEvokerRefactored': 'EvokerSpecialization',
    'AugmentationEvokerRefactored': 'EvokerSpecialization',

    # Hunter
    'BeastMasteryHunterRefactored': 'HunterSpecialization',
    'MarksmanshipHunterRefactored': 'HunterSpecialization',
    'SurvivalHunterRefactored': 'HunterSpecialization',

    # Mage
    'ArcaneMageRefactored': 'MageSpecialization',
    'FireMageRefactored': 'MageSpecialization',
    'FrostMageRefactored': 'MageSpecialization',

    # Monk
    'BrewmasterMonkRefactored': 'MonkSpecialization',
    'MistweaverMonkRefactored': 'MonkSpecialization',
    'WindwalkerMonkRefactored': 'MonkSpecialization',

    # Paladin
    'HolyPaladinRefactored': 'PaladinSpecialization',
    'ProtectionPaladinRefactored': 'PaladinSpecialization',
    'RetributionPaladinRefactored': 'PaladinSpecialization',

    # Priest
    'DisciplinePriestRefactored': 'PriestSpecialization',
    'HolyPriestRefactored': 'PriestSpecialization',
    'ShadowPriestRefactored': 'PriestSpecialization',

    # Rogue
    'AssassinationRogueRefactored': 'RogueSpecialization',
    'OutlawRogueRefactored': 'RogueSpecialization',
    'SubtletyRogueRefactored': 'RogueSpecialization',

    # Shaman
    'ElementalShamanRefactored': 'ShamanSpecialization',
    'EnhancementShamanRefactored': 'ShamanSpecialization',
    'RestorationShamanRefactored': 'ShamanSpecialization',

    # Warlock
    'AfflictionWarlockRefactored': 'WarlockSpecialization',
    'DemonologyWarlockRefactored': 'WarlockSpecialization',
    'DestructionWarlockRefactored': 'WarlockSpecialization',

    # Warrior
    'ArmsWarriorRefactored': 'WarriorSpecialization',
    'FuryWarriorRefactored': 'WarriorSpecialization',
    'ProtectionWarriorRefactored': 'WarriorSpecialization',
}

def fix_class_inheritance(content: str, class_name: str, old_base: str) -> Tuple[str, List[str]]:
    """
    Remove inheritance from old base class in class declaration and constructor.

    Returns: (new_content, list_of_changes)
    """
    changes = []

    # Pattern 1: Class declaration with multiple inheritance
    # Example: class Foo : public Base1<T>, public OldBase
    # Should become: class Foo : public Base1<T>

    pattern1 = rf'(class\s+{class_name}\s*:\s*public\s+[^,]+),\s*public\s+{old_base}'
    match1 = re.search(pattern1, content)

    if match1:
        new_content = re.sub(pattern1, r'\1', content)
        changes.append(f"Removed '{old_base}' from class inheritance")
        content = new_content

    # Pattern 2: Constructor initialization list
    # Example: FooRefactored(...) : Base(...), OldBase(bot)
    # Should become: FooRefactored(...) : Base(...)

    pattern2 = rf'({class_name}\([^)]+\)\s*:\s*[^,]+(?:,\s*[^,]+)*?),\s*{old_base}\([^)]+\)'
    match2 = re.search(pattern2, content, re.MULTILINE | re.DOTALL)

    if match2:
        new_content = re.sub(pattern2, r'\1', content, flags=re.MULTILINE | re.DOTALL)
        changes.append(f"Removed '{old_base}(bot)' from constructor initialization list")
        content = new_content

    return content, changes

def process_file(file_path: Path) -> Tuple[bool, List[str]]:
    """Process a single *Refactored.h file"""

    # Extract class name from filename
    filename = file_path.stem  # e.g., "BloodDeathKnightRefactored"

    if filename not in OLD_BASE_CLASSES:
        return False, [f"No base class mapping found for {filename}"]

    old_base = OLD_BASE_CLASSES[filename]

    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()
        original_content = content

    content, changes = fix_class_inheritance(content, filename, old_base)

    was_modified = (content != original_content)

    if was_modified:
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(content)

    return was_modified, changes

def main():
    """Main execution"""

    print("=" * 80)
    print("Fix *Refactored.h Files - Remove Old Base Class Inheritance")
    print("=" * 80)
    print()

    base_path = Path(r"c:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI")

    # Find all *Refactored.h files
    refactored_files = list(base_path.rglob("*Refactored.h"))

    files_processed = 0
    files_modified = 0
    total_changes = 0

    for file_path in sorted(refactored_files):
        was_modified, changes = process_file(file_path)

        if changes:
            print(f"{file_path.name}:")
            if was_modified:
                for change in changes:
                    print(f"  [OK] {change}")
                files_modified += 1
                total_changes += len(changes)
            else:
                for change in changes:
                    print(f"  [SKIP] {change}")
            print()

        files_processed += 1

    print("=" * 80)
    print(f"Files Processed: {files_processed}")
    print(f"Files Modified: {files_modified}")
    print(f"Total Changes: {total_changes}")
    print("=" * 80)

if __name__ == "__main__":
    main()
