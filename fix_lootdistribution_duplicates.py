#!/usr/bin/env python3
"""Fix duplicate case statements in LootDistribution.cpp"""

import re

def fix_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    original = content

    # Fix duplicate cases - Remove duplicate ITEM_MOD_X lines that have the same name twice
    # Pattern: case ITEM_MOD_X:\n            case ITEM_MOD_X: -> case ITEM_MOD_X:

    # Fix around line 1451-1463 - first switch block
    content = content.replace(
        '''case ITEM_MOD_INTELLECT:
            case ITEM_MOD_INTELLECT:
                hasIntellect = true;''',
        '''case ITEM_MOD_INTELLECT:
                hasIntellect = true;'''
    )

    content = content.replace(
        '''case ITEM_MOD_AGILITY:
            case ITEM_MOD_AGILITY:
                hasAgility = true;''',
        '''case ITEM_MOD_AGILITY:
                hasAgility = true;'''
    )

    content = content.replace(
        '''case ITEM_MOD_STRENGTH:
            case ITEM_MOD_STRENGTH:
                hasStrength = true;''',
        '''case ITEM_MOD_STRENGTH:
                hasStrength = true;'''
    )

    # Fix around line 2723-2760 - second switch block
    content = content.replace(
        '''case ITEM_MOD_STRENGTH:
        case ITEM_MOD_STRENGTH:
            if (role == ChrSpecializationRole::Tank''',
        '''case ITEM_MOD_STRENGTH:
            if (role == ChrSpecializationRole::Tank'''
    )

    content = content.replace(
        '''case ITEM_MOD_AGILITY:
        case ITEM_MOD_AGILITY:
            if (_bot->GetClass()''',
        '''case ITEM_MOD_AGILITY:
            if (_bot->GetClass()'''
    )

    content = content.replace(
        '''case ITEM_MOD_INTELLECT:
        case ITEM_MOD_INTELLECT:
            if (role == ChrSpecializationRole::Healer)''',
        '''case ITEM_MOD_INTELLECT:
            if (role == ChrSpecializationRole::Healer)'''
    )

    content = content.replace(
        '''case ITEM_MOD_STAMINA:
        case ITEM_MOD_STAMINA:
            if (role == ChrSpecializationRole::Tank)''',
        '''case ITEM_MOD_STAMINA:
            if (role == ChrSpecializationRole::Tank)'''
    )

    if content != original:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"Fixed {filepath}")
        return True
    else:
        print(f"No changes needed for {filepath}")
        return False

if __name__ == '__main__':
    fix_file('C:/TrinityBots/TrinityCore/src/modules/Playerbot/Social/LootDistribution.cpp')
