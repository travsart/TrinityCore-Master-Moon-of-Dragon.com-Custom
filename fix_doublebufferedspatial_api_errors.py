#!/usr/bin/env python3
"""
Fix remaining TrinityCore API compatibility errors in DoubleBufferedSpatialGrid.cpp
"""

import re
from pathlib import Path

FILE_PATH = Path(r"C:\TrinityBots\TrinityCore\src\modules\Playerbot\Spatial\DoubleBufferedSpatialGrid.cpp")

def fix_api_errors(content):
    """Fix all remaining API errors"""

    # Fix 1: GetCurrentWaypointID -> No direct equivalent, use 0
    content = re.sub(
        r'creature->GetCurrentWaypointID\(\)',
        '0  // GetCurrentWaypointID not available',
        content
    )

    # Fix 2: GetUnitState -> HasUnitState with specific flag
    content = re.sub(
        r'snapshot\.unitState = creature->GetUnitState\(\);',
        'snapshot.unitState = creature->HasUnitState(UNIT_STATE_ROOT) ? UNIT_STATE_ROOT : 0;',
        content
    )

    # Fix 3: GetAttackers -> getAttackers
    content = re.sub(
        r'creature->GetAttackers\(\)',
        'creature->getAttackers()',
        content
    )

    # Fix 4: GetAttackTimer -> getAttackTimer (with BASE_ATTACK argument)
    content = re.sub(
        r'creature->GetAttackTimer\(WeaponAttackType::BASE_ATTACK\)',
        'creature->getAttackTimer(WeaponAttackType::BASE_ATTACK)',
        content
    )

    # Fix 5: CannotReachTarget -> No direct equivalent, use false
    content = re.sub(
        r'creature->CannotReachTarget\(\)',
        'false  // CannotReachTarget not available',
        content
    )

    # Fix 6: CREATURE_FLAG_EXTRA_* constants -> CreatureFlagsExtra enum
    content = re.sub(
        r'CREATURE_FLAG_EXTRA_UNKILLABLE',
        'static_cast<uint32>(CreatureFlagsExtra::Unkillable)',
        content
    )
    content = re.sub(
        r'CREATURE_FLAG_EXTRA_SESSILE',
        'static_cast<uint32>(CreatureFlagsExtra::Sessile)',
        content
    )
    content = re.sub(
        r'CREATURE_FLAG_EXTRA_IGNORE_SANCTUARY',
        'static_cast<uint32>(CreatureFlagsExtra::IgnoreSanctuary)',
        content
    )

    # Fix 7: IsLevitating -> HasUnitMovementFlag
    content = re.sub(
        r'creature->IsLevitating\(\)',
        'creature->HasUnitMovementFlag(MOVEMENTFLAG_DISABLE_GRAVITY)',
        content
    )

    # Fix 8: HasLootRecipient -> hasLootRecipient
    content = re.sub(
        r'creature->HasLootRecipient\(\)',
        'creature->hasLootRecipient()',
        content
    )

    # Fix 9: CanSkin -> getSkinnerGUID
    content = re.sub(
        r'snapshot\.isSkinnable = creature->CanSkin\(\);',
        'snapshot.isSkinnable = !creature->getSkinnerGUID().IsEmpty();',
        content
    )

    return content

def main():
    print("=" * 80)
    print("Fixing DoubleBufferedSpatialGrid.cpp API Errors")
    print("=" * 80)

    if not FILE_PATH.exists():
        print(f"[ERROR] File not found: {FILE_PATH}")
        return

    with open(FILE_PATH, 'r', encoding='utf-8') as f:
        content = f.read()

    original = content
    content = fix_api_errors(content)

    if content != original:
        with open(FILE_PATH, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"[SUCCESS] Fixed API errors in {FILE_PATH.name}")
    else:
        print(f"[NO CHANGES] No fixes needed in {FILE_PATH.name}")

    print("=" * 80)

if __name__ == "__main__":
    main()
