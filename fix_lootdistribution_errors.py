#!/usr/bin/env python3
"""Fix LootDistribution.cpp compilation errors"""

import re

def fix_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    original = content

    # Fix 1: CanEnter -> CannotEnter (inverted logic)
    # The API is CannotEnter which returns TransferAbortParams
    # If it's truthy, player cannot enter
    content = content.replace(
        'if (!instance->CanEnter(_bot))',
        'if (instance->CannotEnter(_bot))'
    )

    # Fix 2: ITEM_SUBCLASS_WEAPON_WARGLAIVE -> ITEM_SUBCLASS_WEAPON_WARGLAIVES (with S)
    content = content.replace('ITEM_SUBCLASS_WEAPON_WARGLAIVE', 'ITEM_SUBCLASS_WEAPON_WARGLAIVES')

    # Fix 3: ITEM_MOD_INTELLECT_PCT -> ITEM_MOD_INTELLECT
    content = content.replace('ITEM_MOD_INTELLECT_PCT', 'ITEM_MOD_INTELLECT')

    # Fix 4: ITEM_MOD_AGILITY_PCT -> ITEM_MOD_AGILITY
    content = content.replace('ITEM_MOD_AGILITY_PCT', 'ITEM_MOD_AGILITY')

    # Fix 5: ITEM_MOD_STRENGTH_PCT -> ITEM_MOD_STRENGTH
    content = content.replace('ITEM_MOD_STRENGTH_PCT', 'ITEM_MOD_STRENGTH')

    # Fix 6: ITEM_MOD_STAMINA_PCT -> ITEM_MOD_STAMINA
    content = content.replace('ITEM_MOD_STAMINA_PCT', 'ITEM_MOD_STAMINA')

    if content != original:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"Fixed {filepath}")

        # Count fixes
        fixes = 0
        if 'CannotEnter' in content and 'CanEnter' in original:
            fixes += 1
            print("  - Fixed CanEnter -> CannotEnter")
        if 'WARGLAIVES' in content:
            fixes += 1
            print("  - Fixed ITEM_SUBCLASS_WEAPON_WARGLAIVE -> ITEM_SUBCLASS_WEAPON_WARGLAIVES")
        if 'ITEM_MOD_INTELLECT' in content and 'ITEM_MOD_INTELLECT_PCT' not in content:
            fixes += 1
            print("  - Fixed ITEM_MOD_*_PCT -> ITEM_MOD_*")

        return fixes
    else:
        print(f"No changes needed for {filepath}")
        return 0

if __name__ == '__main__':
    fix_file('C:/TrinityBots/TrinityCore/src/modules/Playerbot/Social/LootDistribution.cpp')
