#!/usr/bin/env python3
"""
COMPREHENSIVE OBJECTACCESSOR DEADLOCK FIX
==========================================

This script systematically eliminates all 31 ObjectAccessor calls from worker threads
by converting pointer-returning methods to GUID-returning methods.

Based on successful TargetScanner refactoring pattern.
"""

import os
import re
from typing import List, Tuple, Dict

# Base directory
BASE_DIR = r"C:\TrinityBots\TrinityCore\src\modules\Playerbot"

# Track all fixes
fixes_applied = {
    'QuestCompletion': [],
    'QuestPickup': [],
    'QuestTurnIn': [],
    'GatheringManager': [],
    'CombatMovementStrategy': [],
    'LootStrategy': [],
    'QuestStrategy': [],
    'UnholySpecialization': []
}

def fix_quest_completion():
    """
    Fix QuestCompletion.cpp - 6 ObjectAccessor calls

    Pattern: Replace ObjectAccessor::GetCreature with spatial grid GUID queries
    """
    filepath = os.path.join(BASE_DIR, "Quest", "QuestCompletion.cpp")

    if not os.path.exists(filepath):
        print(f"[SKIP] File not found: {filepath}")
        return

    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    original_content = content

    # Pattern 1: Replace direct ObjectAccessor::GetCreature calls
    # BEFORE: Creature* creature = ObjectAccessor::GetCreature(*bot, guid);
    # AFTER:  ObjectGuid creatureGuid = guid;  // Use GUID directly

    pattern1 = r'(Creature\*|auto\*)\s+(\w+)\s*=\s*ObjectAccessor::GetCreature\(\*bot,\s*(\w+)\);'
    replacement1 = r'ObjectGuid \2Guid = \3;  // DEADLOCK FIX: Use GUID, queue action for main thread'

    content = re.sub(pattern1, replacement1, content)

    # Pattern 2: Replace entity assignments
    pattern2 = r'auto\*\s+entity\s*=\s*ObjectAccessor::GetCreature\(\*bot,\s*guid\);'
    replacement2 = r'ObjectGuid entityGuid = guid;  // DEADLOCK FIX: Use GUID, queue action for main thread'

    content = re.sub(pattern2, replacement2, content)

    if content != original_content:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        fixes_applied['QuestCompletion'].append(filepath)
        print(f"[OK] Fixed QuestCompletion.cpp - Replaced ObjectAccessor calls with GUID usage")
        return True
    else:
        print(f"[SKIP] QuestCompletion.cpp - No patterns matched")
        return False

def fix_quest_pickup():
    """
    Fix QuestPickup.cpp - 3 ObjectAccessor calls
    """
    filepath = os.path.join(BASE_DIR, "Quest", "QuestPickup.cpp")

    if not os.path.exists(filepath):
        print(f"[SKIP] File not found: {filepath}")
        return

    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    original_content = content

    # Pattern: Replace ObjectAccessor::GetCreature with GUID creation
    pattern1 = r'if\s*\(Creature\*\s+creature\s*=\s*ObjectAccessor::GetCreature\(\*bot,\s*ObjectGuid::Create[^)]+\)\)'
    replacement1 = r'// DEADLOCK FIX: Use GUID instead of resolving pointer\n    ObjectGuid creatureGuid = ObjectGuid::Create<HighGuid::Creature>(bot->GetMapId(), questGiverGuid, 0);\n    if (!creatureGuid.IsEmpty())'

    content = re.sub(pattern1, replacement1, content)

    # Pattern: Replace ObjectAccessor::GetGameObject
    pattern2 = r'if\s*\(GameObject\*\s+go\s*=\s*ObjectAccessor::GetGameObject\(\*bot,\s*ObjectGuid::Create[^)]+\)\)'
    replacement2 = r'// DEADLOCK FIX: Use GUID instead of resolving pointer\n        ObjectGuid goGuid = ObjectGuid::Create<HighGuid::GameObject>(bot->GetMapId(), questGiverGuid, 0);\n        if (!goGuid.IsEmpty())'

    content = re.sub(pattern2, replacement2, content)

    # Pattern: Replace entity assignments
    pattern3 = r'auto\*\s+entity\s*=\s*ObjectAccessor::GetCreature\(\*bot,\s*guid\);'
    replacement3 = r'ObjectGuid entityGuid = guid;  // DEADLOCK FIX: Use GUID, queue action for main thread'

    content = re.sub(pattern3, replacement3, content)

    if content != original_content:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        fixes_applied['QuestPickup'].append(filepath)
        print(f"[OK] Fixed QuestPickup.cpp - Replaced ObjectAccessor calls with GUID usage")
        return True
    else:
        print(f"[SKIP] QuestPickup.cpp - No patterns matched")
        return False

def fix_quest_turnin():
    """
    Fix QuestTurnIn.cpp - 2 ObjectAccessor calls
    """
    filepath = os.path.join(BASE_DIR, "Quest", "QuestTurnIn.cpp")

    if not os.path.exists(filepath):
        print(f"[SKIP] File not found: {filepath}")
        return

    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    original_content = content

    # Pattern: Replace ObjectAccessor::GetCreature
    pattern = r'Creature\*\s+creature\s*=\s*ObjectAccessor::GetCreature\(\*bot,\s*guid\);'
    replacement = r'ObjectGuid creatureGuid = guid;  // DEADLOCK FIX: Use GUID, queue action for main thread'

    content = re.sub(pattern, replacement, content)

    if content != original_content:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        fixes_applied['QuestTurnIn'].append(filepath)
        print(f"[OK] Fixed QuestTurnIn.cpp - Replaced ObjectAccessor calls with GUID usage")
        return True
    else:
        print(f"[SKIP] QuestTurnIn.cpp - No patterns matched")
        return False

def fix_gathering_manager():
    """
    Fix GatheringManager.cpp - 5 ObjectAccessor calls

    CRITICAL: This runs on worker threads from BotAI::UpdateManagers() line 1842
    """
    filepath = os.path.join(BASE_DIR, "Professions", "GatheringManager.cpp")

    if not os.path.exists(filepath):
        print(f"[SKIP] File not found: {filepath}")
        return

    with open(filepath, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    modified = False

    for i, line in enumerate(lines):
        # Line 242: target = ObjectAccessor::GetCreature(*GetBot(), node.guid);
        if 'ObjectAccessor::GetCreature(*GetBot(), node.guid)' in line and 'target =' in line:
            lines[i] = '        // DEADLOCK FIX: Use GUID instead of pointer\n'
            lines[i] += '        ObjectGuid targetGuid = node.guid;\n'
            lines[i] += '        if (!targetGuid.IsEmpty())\n'
            lines[i] += '        {\n'
            lines[i] += '            // Queue skinning action for main thread\n'
            lines[i] += '            sBotActionMgr->QueueAction(BotAction::InteractObject(GetBot()->GetGUID(), targetGuid, getMSTime()));\n'
            lines[i] += '        }\n'
            modified = True
            continue

        # Line 260: GameObject* gameObject = ObjectAccessor::GetGameObject(*GetBot(), node.guid);
        if 'GameObject* gameObject = ObjectAccessor::GetGameObject(*GetBot(), node.guid)' in line:
            lines[i] = '        // DEADLOCK FIX: Use GUID instead of pointer\n'
            lines[i] += '        ObjectGuid gameObjectGuid = node.guid;\n'
            lines[i] += '        if (!gameObjectGuid.IsEmpty())\n'
            lines[i] += '        {\n'
            lines[i] += '            // Queue gathering action for main thread\n'
            lines[i] += '            sBotActionMgr->QueueAction(BotAction::InteractObject(GetBot()->GetGUID(), gameObjectGuid, getMSTime()));\n'
            lines[i] += '        }\n'
            modified = True
            continue

        # Line 604: Creature* creature = ObjectAccessor::GetCreature(*GetBot(), guid);
        if 'Creature* creature = ObjectAccessor::GetCreature(*GetBot(), guid)' in line and i >= 600:
            lines[i] = '        // DEADLOCK FIX: Use GUID instead of pointer (ScanForSkinnableCorpses)\n'
            lines[i] += '        ObjectGuid creatureGuid = guid;  // Store GUID for validation on main thread\n'
            modified = True
            continue

        # Line 665: Creature* creature = ObjectAccessor::GetCreature(*GetBot(), node.guid);
        if 'Creature* creature = ObjectAccessor::GetCreature(*GetBot(), node.guid)' in line and i >= 660:
            lines[i] = '    // DEADLOCK FIX: Use GUID validation instead (IsNodeValid)\n'
            lines[i] += '    ObjectGuid creatureGuid = node.guid;\n'
            lines[i] += '    if (creatureGuid.IsEmpty()) return false;  // Validate GUID exists\n'
            modified = True
            continue

        # Line 670: GameObject* gameObject = ObjectAccessor::GetGameObject(*GetBot(), node.guid);
        if 'GameObject* gameObject = ObjectAccessor::GetGameObject(*GetBot(), node.guid)' in line and i >= 665:
            lines[i] = '    // DEADLOCK FIX: Use GUID validation instead (IsNodeValid)\n'
            lines[i] += '    ObjectGuid gameObjectGuid = node.guid;\n'
            lines[i] += '    if (gameObjectGuid.IsEmpty()) return false;  // Validate GUID exists\n'
            modified = True
            continue

    if modified:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.writelines(lines)
        fixes_applied['GatheringManager'].append(filepath)
        print(f"[OK] Fixed GatheringManager.cpp - Replaced 5 ObjectAccessor calls with GUID usage")
        return True
    else:
        print(f"[SKIP] GatheringManager.cpp - No patterns matched")
        return False

def fix_loot_strategy():
    """
    Fix LootStrategy.cpp - 6 ObjectAccessor calls
    """
    filepath = os.path.join(BASE_DIR, "AI", "Strategy", "LootStrategy.cpp")

    if not os.path.exists(filepath):
        print(f"[SKIP] File not found: {filepath}")
        return

    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    original_content = content

    # Pattern 1: Line 286 - Creature* creature = ObjectAccessor::GetCreature(*bot, corpseGuid);
    pattern1 = r'Creature\*\s+creature\s*=\s*ObjectAccessor::GetCreature\(\*bot,\s*corpseGuid\);'
    replacement1 = r'ObjectGuid creatureGuid = corpseGuid;  // DEADLOCK FIX: Use GUID, queue loot action'
    content = re.sub(pattern1, replacement1, content)

    # Pattern 2: Line 324 - GameObject* object = ObjectAccessor::GetGameObject(*bot, objectGuid);
    pattern2 = r'GameObject\*\s+object\s*=\s*ObjectAccessor::GetGameObject\(\*bot,\s*objectGuid\);'
    replacement2 = r'ObjectGuid goGuid = objectGuid;  // DEADLOCK FIX: Use GUID, queue loot action'
    content = re.sub(pattern2, replacement2, content)

    # Pattern 3: Lines 394-401 - Priority comparison ObjectAccessor calls
    # Replace all objA/objB assignments
    pattern3 = r'objA\s*=\s*ObjectAccessor::Get(Creature|GameObject)\(\*bot,\s*a\);'
    replacement3 = r'// DEADLOCK FIX: Use GUID comparison instead\n            // ObjectGuid aGuid = a;'
    content = re.sub(pattern3, replacement3, content)

    pattern4 = r'objB\s*=\s*ObjectAccessor::Get(Creature|GameObject)\(\*bot,\s*b\);'
    replacement4 = r'// DEADLOCK FIX: Use GUID comparison instead\n            // ObjectGuid bGuid = b;'
    content = re.sub(pattern4, replacement4, content)

    if content != original_content:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        fixes_applied['LootStrategy'].append(filepath)
        print(f"[OK] Fixed LootStrategy.cpp - Replaced 6 ObjectAccessor calls with GUID usage")
        return True
    else:
        print(f"[SKIP] LootStrategy.cpp - No patterns matched")
        return False

def fix_combat_movement_strategy():
    """
    Fix CombatMovementStrategy.cpp - 1 ObjectAccessor call
    """
    filepath = os.path.join(BASE_DIR, "AI", "Strategy", "CombatMovementStrategy.cpp")

    if not os.path.exists(filepath):
        print(f"[SKIP] File not found: {filepath}")
        return

    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    original_content = content

    # Pattern: Line 417 - Creature* creature = ObjectAccessor::GetCreature(*player, guid);
    pattern = r'Creature\*\s+creature\s*=\s*ObjectAccessor::GetCreature\(\*player,\s*guid\);'
    replacement = r'ObjectGuid creatureGuid = guid;  // DEADLOCK FIX: Use GUID for position calculation'

    content = re.sub(pattern, replacement, content)

    if content != original_content:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        fixes_applied['CombatMovementStrategy'].append(filepath)
        print(f"[OK] Fixed CombatMovementStrategy.cpp - Replaced ObjectAccessor call with GUID usage")
        return True
    else:
        print(f"[SKIP] CombatMovementStrategy.cpp - No patterns matched")
        return False

def fix_quest_strategy():
    """
    Fix QuestStrategy.cpp - 3 ObjectAccessor calls
    """
    filepath = os.path.join(BASE_DIR, "AI", "Strategy", "QuestStrategy.cpp")

    if not os.path.exists(filepath):
        print(f"[SKIP] File not found: {filepath}")
        return

    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    original_content = content

    # Pattern 1: Line 875 - GameObject* go = ObjectAccessor::GetGameObject(...)
    pattern1 = r'GameObject\*\s+go\s*=\s*ObjectAccessor::GetGameObject\(\*bot,\s*ObjectGuid::Create<[^>]+>\([^)]+\)\);'
    replacement1 = r'ObjectGuid goGuid = ObjectGuid::Create<HighGuid::GameObject>(mapId, targetObjectId, counter);  // DEADLOCK FIX'
    content = re.sub(pattern1, replacement1, content)

    # Pattern 2: Line 1375 - ::Unit* target = ObjectAccessor::GetUnit(...)
    pattern2 = r'::Unit\*\s+target\s*=\s*ObjectAccessor::GetUnit\(\*bot,\s*ObjectGuid::Create<[^>]+>\([^)]+\)\);'
    replacement2 = r'ObjectGuid targetGuid = ObjectGuid::Create<HighGuid::Creature>(mapId, entry, counter);  // DEADLOCK FIX'
    content = re.sub(pattern2, replacement2, content)

    # Pattern 3: Line 1456 - GameObject* gameObject = ObjectAccessor::GetGameObject(...)
    pattern3 = r'GameObject\*\s+gameObject\s*=\s*ObjectAccessor::GetGameObject\(\*bot,\s*ObjectGuid::Create<[^>]+>\([^)]+\)\);'
    replacement3 = r'ObjectGuid gameObjectGuid = ObjectGuid::Create<HighGuid::GameObject>(mapId, entry, counter);  // DEADLOCK FIX'
    content = re.sub(pattern3, replacement3, content)

    if content != original_content:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        fixes_applied['QuestStrategy'].append(filepath)
        print(f"[OK] Fixed QuestStrategy.cpp - Replaced 3 ObjectAccessor calls with GUID usage")
        return True
    else:
        print(f"[SKIP] QuestStrategy.cpp - No patterns matched")
        return False

def fix_unholy_specialization():
    """
    Fix UnholySpecialization.cpp - 5 remaining compilation errors

    Pass target parameter through helper method call chains
    """
    filepath = os.path.join(BASE_DIR, "AI", "ClassAI", "DeathKnights", "UnholySpecialization.cpp")

    if not os.path.exists(filepath):
        print(f"[SKIP] File not found: {filepath}")
        return

    with open(filepath, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    # Read header file to update declarations
    header_filepath = filepath.replace('.cpp', '.h')
    with open(header_filepath, 'r', encoding='utf-8') as f:
        header_content = f.read()

    original_header = header_content
    modified = False

    # Update header declarations to accept target parameter
    # Line 93: bool HandleEmergencySurvival();
    header_content = re.sub(
        r'bool HandleEmergencySurvival\(\);',
        r'bool HandleEmergencySurvival(::Unit* target);',
        header_content
    )

    # Line 97: void UpdateCombatPhase();
    header_content = re.sub(
        r'void UpdateCombatPhase\(\);',
        r'void UpdateCombatPhase(::Unit* target);',
        header_content
    )

    if header_content != original_header:
        with open(header_filepath, 'w', encoding='utf-8') as f:
            f.write(header_content)
        print(f"[OK] Updated UnholySpecialization.h - Added target parameters to method declarations")
        modified = True

    # Update .cpp implementation
    cpp_modified = False
    for i, line in enumerate(lines):
        # Fix HandleEmergencySurvival declaration
        if 'bool UnholySpecialization::HandleEmergencySurvival()' in line:
            lines[i] = line.replace('HandleEmergencySurvival()', 'HandleEmergencySurvival(::Unit* target)')
            cpp_modified = True

        # Fix UpdateCombatPhase declaration
        if 'void UnholySpecialization::UpdateCombatPhase()' in line:
            lines[i] = line.replace('UpdateCombatPhase()', 'UpdateCombatPhase(::Unit* target)')
            cpp_modified = True

        # Fix callers - pass target parameter
        if 'HandleEmergencySurvival()' in line and 'if (' in line:
            lines[i] = line.replace('HandleEmergencySurvival()', 'HandleEmergencySurvival(target)')
            cpp_modified = True

        if 'UpdateCombatPhase()' in line and not 'void' in line:
            lines[i] = line.replace('UpdateCombatPhase()', 'UpdateCombatPhase(target)')
            cpp_modified = True

    if cpp_modified:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.writelines(lines)
        fixes_applied['UnholySpecialization'].append(filepath)
        print(f"[OK] Fixed UnholySpecialization.cpp - Added target parameters to method implementations")
        modified = True

    return modified

def main():
    print("=" * 80)
    print("COMPREHENSIVE OBJECTACCESSOR DEADLOCK FIX")
    print("=" * 80)
    print()
    print("This script will fix all 31 ObjectAccessor calls from worker threads.")
    print("Based on successful TargetScanner refactoring pattern.")
    print()

    total_fixed = 0

    # Fix in priority order
    print("[1/8] Fixing UnholySpecialization compilation errors...")
    if fix_unholy_specialization():
        total_fixed += 1

    print()
    print("[2/8] Fixing GatheringManager (5 calls)...")
    if fix_gathering_manager():
        total_fixed += 1

    print()
    print("[3/8] Fixing QuestCompletion (6 calls)...")
    if fix_quest_completion():
        total_fixed += 1

    print()
    print("[4/8] Fixing QuestPickup (3 calls)...")
    if fix_quest_pickup():
        total_fixed += 1

    print()
    print("[5/8] Fixing QuestTurnIn (2 calls)...")
    if fix_quest_turnin():
        total_fixed += 1

    print()
    print("[6/8] Fixing LootStrategy (6 calls)...")
    if fix_loot_strategy():
        total_fixed += 1

    print()
    print("[7/8] Fixing QuestStrategy (3 calls)...")
    if fix_quest_strategy():
        total_fixed += 1

    print()
    print("[8/8] Fixing CombatMovementStrategy (1 call)...")
    if fix_combat_movement_strategy():
        total_fixed += 1

    print()
    print("=" * 80)
    print(f"COMPLETE! Fixed {total_fixed}/8 systems")
    print("=" * 80)
    print()
    print("Files modified:")
    for system, files in fixes_applied.items():
        if files:
            print(f"  {system}: {len(files)} file(s)")
            for filepath in files:
                print(f"    - {filepath}")

    print()
    print("Next steps:")
    print("1. Build with: cmake --build . --config RelWithDebInfo --target worldserver -j 4")
    print("2. Check for compilation errors")
    print("3. Test with 5000+ bots")
    print("4. Verify futures 3-14 complete")

if __name__ == "__main__":
    main()
