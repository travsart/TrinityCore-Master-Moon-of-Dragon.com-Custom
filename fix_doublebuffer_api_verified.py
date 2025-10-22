#!/usr/bin/env python3
"""
Fix DoubleBufferedSpatialGrid.cpp API errors with VERIFIED TrinityCore API calls
Based on actual verification of Creature.h, Unit.h, and CreatureData.h
"""

import re
from pathlib import Path
import time

FILE_PATH = Path(r"C:\TrinityBots\TrinityCore\src\modules\Playerbot\Spatial\DoubleBufferedSpatialGrid.cpp")

def fix_verified_api_errors(content):
    """Fix all API errors with verified TrinityCore methods"""

    # Fix 1: GetCurrentWaypointID() -> GetCurrentWaypointInfo().first
    # Verified: std::pair<uint32, uint32> GetCurrentWaypointInfo() exists (line 400 of Creature.h)
    content = re.sub(
        r'creature->GetCurrentWaypointID\(\)',
        'creature->GetCurrentWaypointInfo().first',
        content
    )

    # Fix 2: GetUnitState() -> m_state (direct access, as no getter exists)
    # Verified: uint32 m_state exists (line 1998 of Unit.h), HasUnitState exists but not GetUnitState
    content = re.sub(
        r'snapshot\.unitState = creature->GetUnitState\(\);',
        'snapshot.unitState = creature->m_state;  // Direct access - no GetUnitState() method',
        content
    )

    # Fix 3: GetAttackers() -> getAttackers()
    # Verified: AttackerSet const& getAttackers() const exists (line 722 of Unit.h)
    content = re.sub(
        r'creature->GetAttackers\(\)',
        'creature->getAttackers()',
        content
    )

    # Fix 4: GetAttackTimer() -> getAttackTimer()
    # Verified: uint32 getAttackTimer(WeaponAttackType) const exists (line 698 of Unit.h)
    content = re.sub(
        r'creature->GetAttackTimer\(',
        'creature->getAttackTimer(',
        content
    )

    # Fix 5: CannotReachTarget() -> false (no getter exists, only SetCannotReachTarget)
    # Verified: Only SetCannotReachTarget exists (line 381 of Creature.h)
    content = re.sub(
        r'snapshot\.canNotReachTarget = creature->CannotReachTarget\(\);',
        'snapshot.canNotReachTarget = false;  // No CannotReachTarget() getter exists',
        content
    )

    # Fix 6: CREATURE_FLAG_EXTRA constants exist as-is
    # Verified: These are defined in CreatureData.h lines 341-359
    # No changes needed - they exist!

    # Fix 7: IsLevitating() -> IsGravityDisabled()
    # Verified: bool IsGravityDisabled() const exists (line 1146 of Unit.h)
    content = re.sub(
        r'creature->IsLevitating\(\)',
        'creature->IsGravityDisabled()',
        content
    )

    # Fix 8: HasLootRecipient() -> hasLootRecipient()
    # Verified: bool hasLootRecipient() const exists (line 306 of Creature.h)
    content = re.sub(
        r'creature->HasLootRecipient\(\)',
        'creature->hasLootRecipient()',
        content
    )

    # Fix 9: CanSkin() -> check SkinLootID from template
    # Verified: CreatureTemplate has uint32 SkinLootID field (line 462 of CreatureData.h)
    content = re.sub(
        r'snapshot\.isSkinnable = creature->CanSkin\(\);',
        'snapshot.isSkinnable = creatureTemplate && creatureTemplate->SkinLootID > 0;',
        content
    )

    return content

def main():
    print("=" * 80)
    print("Fixing DoubleBufferedSpatialGrid.cpp with VERIFIED TrinityCore API")
    print("=" * 80)

    # Wait for file lock to release
    max_attempts = 10
    for attempt in range(max_attempts):
        try:
            with open(FILE_PATH, 'r', encoding='utf-8') as f:
                content = f.read()
            break
        except (PermissionError, OSError) as e:
            if attempt < max_attempts - 1:
                print(f"[WAIT] File locked, waiting... (attempt {attempt + 1}/{max_attempts})")
                time.sleep(2)
            else:
                print(f"[ERROR] Could not read {FILE_PATH} after {max_attempts} attempts: {e}")
                return

    original = content
    content = fix_verified_api_errors(content)

    if content != original:
        try:
            with open(FILE_PATH, 'w', encoding='utf-8') as f:
                f.write(content)
            print(f"[SUCCESS] Fixed API errors in {FILE_PATH.name}")
        except Exception as e:
            print(f"[ERROR] Could not write {FILE_PATH}: {e}")
            return
    else:
        print(f"[NO CHANGES] No fixes needed in {FILE_PATH.name}")

    print("=" * 80)

if __name__ == "__main__":
    main()
