#!/usr/bin/env python3
import re
import sys

# Mapping of class names to their required base specialization
BASE_CLASS_MAP = {
    'BloodDeathKnightRefactored': 'DeathKnightSpecialization',
    'FrostDeathKnightRefactored': 'DeathKnightSpecialization',
    'UnholyDeathKnightRefactored': 'DeathKnightSpecialization',

    'ArcaneMageRefactored': 'MageSpecialization',
    'FireMageRefactored': 'MageSpecialization',
    'FrostMageRefactored': 'MageSpecialization',

    'DisciplinePriestRefactored': 'PriestSpecialization',
    'HolyPriestRefactored': 'PriestSpecialization',
    'ShadowPriestRefactored': 'PriestSpecialization',

    'AssassinationRogueRefactored': 'RogueSpecialization',
    'OutlawRogueRefactored': 'RogueSpecialization',
    'SubtletyRogueRefactored': 'RogueSpecialization',

    'ElementalShamanRefactored': 'ShamanSpecialization',
    'EnhancementShamanRefactored': 'ShamanSpecialization',
    'RestorationShamanRefactored': 'ShamanSpecialization',

    'AfflictionWarlockRefactored': 'WarlockSpecialization',
    'DemonologyWarlockRefactored': 'WarlockSpecialization',
    'DestructionWarlockRefactored': 'WarlockSpecialization',

    'ArmsWarriorRefactored': 'WarriorSpecialization',
    'FuryWarriorRefactored': 'WarriorSpecialization',
    'ProtectionWarriorRefactored': 'WarriorSpecialization',

    'HolyPaladinRefactored': 'PaladinSpecialization',
    'ProtectionPaladinRefactored': 'PaladinSpecialization',
    'RetributionPaladinRefactored': 'PaladinSpecialization',

    'BrewmasterMonkRefactored': 'MonkSpecialization',
    'MistweaverMonkRefactored': 'MonkSpecialization',
    'WindwalkerMonkRefactored': 'MonkSpecialization',

    'BalanceDruidRefactored': 'DruidSpecialization',
    'FeralDruidRefactored': 'DruidSpecialization',
    'GuardianDruidRefactored': 'DruidSpecialization',
    'RestorationDruidRefactored': 'DruidSpecialization',

    'HavocDemonHunterRefactored': 'DemonHunterSpecialization',
    'VengeanceDemonHunterRefactored': 'DemonHunterSpecialization',

    'AugmentationEvokerRefactored': 'EvokerSpecialization',
    'DevastationEvokerRefactored': 'EvokerSpecialization',
    'PreservationEvokerRefactored': 'EvokerSpecialization',

    'BeastMasteryHunterRefactored': 'HunterSpecialization',
    'MarksmanshipHunterRefactored': 'HunterSpecialization',
    'SurvivalHunterRefactored': 'HunterSpecialization',
}

def add_multiple_inheritance(file_path):
    """Add class-specific base class to refactored specialization inheritance."""

    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    modified = False
    for class_name, base_spec in BASE_CLASS_MAP.items():
        # Pattern: class ClassName : public TemplateBase<...>
        # We want: class ClassName : public TemplateBase<...>, public BaseSpec
        pattern = rf'^(class\s+{class_name}\s*:\s*public\s+\w+Specialization<[^>]+>)$'

        def replacement(match):
            return f'{match.group(1)}, public {base_spec}'

        new_content, count = re.subn(pattern, replacement, content, flags=re.MULTILINE)

        if count > 0:
            content = new_content
            modified = True
            print(f"Added {base_spec} to {class_name} in {file_path}")

    if modified:
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(content)
        return True

    return False

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python add_multiple_inheritance.py <file1> <file2> ...")
        sys.exit(1)

    files_modified = 0
    for file_path in sys.argv[1:]:
        try:
            if add_multiple_inheritance(file_path):
                files_modified += 1
        except Exception as e:
            print(f"Error processing {file_path}: {e}", file=sys.stderr)

    print(f"\nTotal files modified: {files_modified}")
