#!/usr/bin/env python3
"""
Add #include "Log.h" to all refactored specialization files that use TC_LOG_DEBUG
"""

import os
import re

# List of files that need Log.h (from grep results)
files_needing_log = [
    r"src\modules\Playerbot\AI\ClassAI\DeathKnights\BloodDeathKnightRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\Warlocks\AfflictionWarlockRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\Shamans\EnhancementShamanRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\Shamans\ElementalShamanRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\Priests\ShadowPriestRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\Mages\FrostMageRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\Mages\FireMageRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\Evokers\DevastationEvokerRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\Druids\GuardianDruidRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\Druids\FeralDruidRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\Warlocks\DestructionWarlockRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\Warlocks\DemonologyWarlockRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\Shamans\RestorationShamanRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\Rogues\SubtletyRogueRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\Rogues\OutlawRogueRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\Rogues\AssassinationRogueRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\Priests\HolyPriestRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\Priests\DisciplinePriestRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\Paladins\ProtectionPaladinRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\Paladins\HolyPaladinRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\Monks\WindwalkerMonkRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\Monks\MistweaverMonkRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\Monks\BrewmasterMonkRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\Evokers\PreservationEvokerRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\Druids\RestorationDruidRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\Druids\BalanceDruidRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\DemonHunters\VengeanceDemonHunterRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\DeathKnights\UnholyDeathKnightRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\DeathKnights\FrostDeathKnightRefactored.h",
]

def add_log_include(filepath):
    """Add #include "Log.h" to the file if not already present"""
    if not os.path.exists(filepath):
        print(f"WARNING: File not found: {filepath}")
        return False

    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    # Check if Log.h is already included
    if '#include "Log.h"' in content:
        print(f"SKIP: {filepath} already has Log.h")
        return False

    # Find the last #include statement before the first blank line or comment after includes
    # Pattern: Look for the include section and add Log.h at the end
    lines = content.split('\n')
    include_end_idx = -1

    for i, line in enumerate(lines):
        if line.strip().startswith('#include'):
            include_end_idx = i

    if include_end_idx == -1:
        print(f"WARNING: No includes found in {filepath}")
        return False

    # Insert Log.h after the last include
    lines.insert(include_end_idx + 1, '#include "Log.h"')

    # Write back
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))

    print(f"SUCCESS: Added Log.h to {filepath}")
    return True

def main():
    base_dir = os.path.dirname(os.path.abspath(__file__))
    success_count = 0
    skip_count = 0
    error_count = 0

    for rel_path in files_needing_log:
        filepath = os.path.join(base_dir, rel_path)

        try:
            result = add_log_include(filepath)
            if result:
                success_count += 1
            else:
                skip_count += 1
        except Exception as e:
            print(f"ERROR processing {filepath}: {e}")
            error_count += 1

    print(f"\n=== SUMMARY ===")
    print(f"Successfully added Log.h: {success_count}")
    print(f"Skipped (already present): {skip_count}")
    print(f"Errors: {error_count}")
    print(f"Total files processed: {len(files_needing_log)}")

if __name__ == "__main__":
    main()
