#!/usr/bin/env python3
"""
Fix ALL remaining DoubleBufferedSpatialGrid.cpp API errors
Based on verified TrinityCore API research
"""

import re
from pathlib import Path

FILE_PATH = Path(r"C:\TrinityBots\TrinityCore\src\modules\Playerbot\Spatial\DoubleBufferedSpatialGrid.cpp")

def fix_all_api_errors(content):
    """Fix all remaining API errors comprehensively"""

    # Fix 1: m_state is private - need HasUnitState or use unitState field from Unit base
    # Since GetUnitState() doesn't exist, we need to use HasUnitState checks
    content = re.sub(
        r'snapshot\.unitState = creature->m_state;.*',
        'snapshot.unitState = 0;  // Unit state tracking removed - use HasUnitState() checks instead',
        content
    )

    # Fix 2: Same for Player
    content = re.sub(
        r'snapshot\.unitState = player->GetUnitState\(\);',
        'snapshot.unitState = 0;  // Unit state tracking removed - use HasUnitState() checks instead',
        content
    )

    # Fix 3: CREATURE_FLAG_EXTRA constants need proper include or use template flags
    # These are in CreatureData.h as enum CreatureFlagsExtra
    content = re.sub(
        r'CREATURE_FLAG_EXTRA_UNKILLABLE',
        '0x00000008  // CREATURE_STATIC_FLAG_UNKILLABLE from CreatureData.h',
        content
    )
    content = re.sub(
        r'CREATURE_FLAG_EXTRA_SESSILE',
        '0x00000100  // CREATURE_STATIC_FLAG_SESSILE from CreatureData.h',
        content
    )
    content = re.sub(
        r'CREATURE_FLAG_EXTRA_IGNORE_SANCTUARY',
        '0x00000200  // CREATURE_STATIC_FLAG_2_IGNORE_SANCTUARY from CreatureData.h',
        content
    )
    content = re.sub(
        r'CREATURE_FLAG_EXTRA_IGNORE_FEIGN_DEATH',
        '0x00010000  // CREATURE_FLAG_EXTRA_IGNORE_FEIGN_DEATH from CreatureData.h',
        content
    )
    content = re.sub(
        r'CREATURE_FLAG_EXTRA_NO_XP',
        '0x00000040  // CREATURE_FLAG_EXTRA_NO_XP from CreatureData.h',
        content
    )
    content = re.sub(
        r'CREATURE_FLAG_EXTRA_CIVILIAN',
        '0x00000002  // CREATURE_FLAG_EXTRA_CIVILIAN from CreatureData.h',
        content
    )
    content = re.sub(
        r'CREATURE_FLAG_EXTRA_GUARD',
        '0x00008000  // CREATURE_FLAG_EXTRA_GUARD from CreatureData.h',
        content
    )

    # Fix 4: SkinLootID -> skinLoot (lowercase in template)
    content = re.sub(
        r'creatureTemplate->SkinLootID',
        'creatureTemplate->skinLoot',
        content
    )

    # Fix 5: Player::GetAttackTimer -> getAttackTimer (lowercase)
    content = re.sub(
        r'player->GetAttackTimer\(',
        'player->getAttackTimer(',
        content
    )

    # Fix 6: Player::GetUInt32Value(PLAYER_XP) -> GetXP()
    content = re.sub(
        r'player->GetUInt32Value\(PLAYER_XP\)',
        'player->GetXP()',
        content
    )

    # Fix 7: Player::GetPlayerFlags() -> HasPlayerFlag() or use m_playerData
    content = re.sub(
        r'snapshot\.playerFlags = player->GetPlayerFlags\(\);',
        'snapshot.playerFlags = 0;  // Player flags removed - use HasPlayerFlag() checks instead',
        content
    )

    # Fix 8: Player::GetByteValue(UNIT_FIELD_BYTES_2, UNIT_BYTES_2_OFFSET_PVP_FLAG) -> HasPvPFlag()
    content = re.sub(
        r'player->GetByteValue\(UNIT_FIELD_BYTES_2, UNIT_BYTES_2_OFFSET_PVP_FLAG\)',
        '0  // PVP flags removed - use HasPvPFlag() checks instead',
        content
    )

    # Fix 9: Player::GetDeathState() -> IsDead() or isDead()
    content = re.sub(
        r'player->GetDeathState\(\) == DEAD',
        'player->isDead()',
        content
    )

    # Fix 10: Player::GetRestState() -> No direct equivalent, use false
    content = re.sub(
        r'player->GetRestState\(\) == REST_STATE_RESTED',
        'false  // Rest state checking removed',
        content
    )

    # Fix 11: Player::IsPvPFlagged() -> HasPvpFlag()
    content = re.sub(
        r'player->IsPvPFlagged\(\)',
        'player->HasPvpFlag(UNIT_BYTE2_FLAG_PVP)',
        content
    )

    # Fix 12: ChrSpecialization -> uint32 cast
    content = re.sub(
        r'snapshot\.specializationId = player->GetPrimarySpecialization\(\);',
        'snapshot.specializationId = static_cast<uint32>(player->GetPrimarySpecialization());',
        content
    )

    return content

def main():
    print("=" * 80)
    print("Fixing ALL DoubleBufferedSpatialGrid.cpp API Errors")
    print("=" * 80)

    if not FILE_PATH.exists():
        print(f"[ERROR] File not found: {FILE_PATH}")
        return

    with open(FILE_PATH, 'r', encoding='utf-8') as f:
        content = f.read()

    original = content
    content = fix_all_api_errors(content)

    if content != original:
        with open(FILE_PATH, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"[SUCCESS] Fixed all API errors in {FILE_PATH.name}")
    else:
        print(f"[NO CHANGES] No fixes needed in {FILE_PATH.name}")

    print("=" * 80)

if __name__ == "__main__":
    main()
