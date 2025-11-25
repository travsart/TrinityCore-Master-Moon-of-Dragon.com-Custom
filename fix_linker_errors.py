#!/usr/bin/env python3
"""
Fix all compilation issues causing linker errors in the Playerbot module.
These issues are causing .cpp files to fail compilation, preventing their symbols
from being exported to playerbot.lib.
"""

import os
import re

def replace_in_file(path, old_str, new_str, description=""):
    """Replace a string in a file."""
    if not os.path.exists(path):
        print(f"ERROR: File not found: {path}")
        return False

    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()

    if old_str not in content:
        print(f"  NOT FOUND: {description or old_str[:50]}")
        return False

    content = content.replace(old_str, new_str)

    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)

    print(f"  FIXED: {description or old_str[:50]}")
    return True

def replace_all_in_file(path, pattern, replacement, description=""):
    """Replace all occurrences of a pattern in a file using regex."""
    if not os.path.exists(path):
        print(f"ERROR: File not found: {path}")
        return 0

    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()

    original = content
    content, count = re.subn(pattern, replacement, content)

    if count == 0:
        print(f"  NOT FOUND: {description}")
        return 0

    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)

    print(f"  FIXED: {count} occurrences of {description}")
    return count

print("=" * 70)
print("FIXING COMPILATION ERRORS IN PLAYERBOT MODULE")
print("=" * 70)

base_path = 'C:/TrinityBots/TrinityCore/src/modules/Playerbot'

# =============================================================================
# 1. Fix PvPCombatAI.cpp - replace `player` with `_bot`
# =============================================================================
print("\n1. Fixing PvPCombatAI.cpp (player -> _bot)...")
pvp_path = f'{base_path}/PvP/PvPCombatAI.cpp'
replace_all_in_file(pvp_path, r'if \(!player\)', 'if (!_bot)', 'if (!player)')
replace_all_in_file(pvp_path, r'if \(!player \|\|', 'if (!_bot ||', 'if (!player ||')

# =============================================================================
# 2. Fix ArenaAI.cpp - replace `player` with `_bot`
# =============================================================================
print("\n2. Fixing ArenaAI.cpp (player -> _bot)...")
arena_path = f'{base_path}/PvP/ArenaAI.cpp'
replace_all_in_file(arena_path, r'if \(!player\)', 'if (!_bot)', 'if (!player)')
replace_all_in_file(arena_path, r'if \(!player \|\|', 'if (!_bot ||', 'if (!player ||')

# =============================================================================
# 3. Fix RoleAssignment.cpp - replace `player` with `_bot`
# =============================================================================
print("\n3. Fixing RoleAssignment.cpp (player -> _bot)...")
role_path = f'{base_path}/Group/RoleAssignment.cpp'
replace_all_in_file(role_path, r'if \(!player\)', 'if (!_bot)', 'if (!player)')
replace_all_in_file(role_path, r'if \(!player \|\|', 'if (!_bot ||', 'if (!player ||')

# =============================================================================
# 4. Fix DecisionFusionSystem.cpp - extra parenthesis
# =============================================================================
print("\n4. Fixing DecisionFusionSystem.cpp (extra parenthesis)...")
decision_path = f'{base_path}/AI/Decision/DecisionFusionSystem.cpp'
replace_in_file(decision_path,
    'uint32 spec = bot->GetPrimarySpecialization());',
    'uint32 spec = static_cast<uint32>(bot->GetPrimarySpecialization());',
    'GetPrimarySpecialization extra paren + cast')

# =============================================================================
# 5. Fix MountManager.cpp - check for issues
# =============================================================================
print("\n5. Checking MountManager.cpp...")
mount_path = f'{base_path}/Companion/MountManager.cpp'
# The playerMountsItr issue needs proper context - let's read the file
if os.path.exists(mount_path):
    with open(mount_path, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()
    if 'playerMountsItr' in content and 'auto playerMountsItr' not in content:
        print("  WARNING: playerMountsItr may be undeclared - needs manual review")
    else:
        print("  OK: MountManager.cpp appears correct")

# =============================================================================
# 6. Fix BattlePetManager.cpp - check for issues
# =============================================================================
print("\n6. Checking BattlePetManager.cpp...")
battlepet_path = f'{base_path}/Companion/BattlePetManager.cpp'
# No known issues - implementations exist

# =============================================================================
# 7. Fix BotLifecycleManager.cpp - check for Log.h include
# =============================================================================
print("\n7. Checking BotLifecycleManager.cpp...")
lifecycle_path = f'{base_path}/Lifecycle/BotLifecycleManager.cpp'
if os.path.exists(lifecycle_path):
    with open(lifecycle_path, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()
    if 'TC_LOG_ERROR' in content and '#include "Log.h"' not in content:
        # Add Log.h include
        content = content.replace(
            '#include "BotLifecycleManager.h"',
            '#include "BotLifecycleManager.h"\n#include "Log.h"'
        )
        with open(lifecycle_path, 'w', encoding='utf-8') as f:
            f.write(content)
        print("  FIXED: Added Log.h include")
    else:
        print("  OK: BotLifecycleManager.cpp appears correct")

print("\n" + "=" * 70)
print("DONE! Please rebuild to verify fixes.")
print("=" * 70)
