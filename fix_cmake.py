#!/usr/bin/env python3
"""
Add all missing source files to CMakeLists.txt to resolve linker errors.
"""

import os

cmake_path = 'C:/TrinityBots/TrinityCore/src/modules/Playerbot/CMakeLists.txt'

with open(cmake_path, 'r', encoding='utf-8') as f:
    content = f.read()

# 1. Uncomment MountManager and BattlePetManager
print("1. Uncommenting MountManager and BattlePetManager...")
content = content.replace(
    '# ${CMAKE_CURRENT_SOURCE_DIR}/Companion/MountManager.cpp',
    '${CMAKE_CURRENT_SOURCE_DIR}/Companion/MountManager.cpp'
)
content = content.replace(
    '# ${CMAKE_CURRENT_SOURCE_DIR}/Companion/MountManager.h',
    '${CMAKE_CURRENT_SOURCE_DIR}/Companion/MountManager.h'
)
content = content.replace(
    '# ${CMAKE_CURRENT_SOURCE_DIR}/Companion/BattlePetManager.cpp',
    '${CMAKE_CURRENT_SOURCE_DIR}/Companion/BattlePetManager.cpp'
)
content = content.replace(
    '# ${CMAKE_CURRENT_SOURCE_DIR}/Companion/BattlePetManager.h',
    '${CMAKE_CURRENT_SOURCE_DIR}/Companion/BattlePetManager.h'
)

# 2. Add BotLifecycleManager if not present
if 'Lifecycle/BotLifecycleManager.cpp' not in content:
    print("2. Adding BotLifecycleManager...")
    content = content.replace(
        '${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawner.cpp',
        '${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotLifecycleManager.cpp\n    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotLifecycleManager.h\n    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawner.cpp'
    )

# 3. Add RoleAssignment if not present
if 'Group/RoleAssignment.cpp' not in content:
    print("3. Adding RoleAssignment...")
    # Find a good place in the Group section
    if '${CMAKE_CURRENT_SOURCE_DIR}/Group/' in content:
        # Find last Group entry and add after
        lines = content.split('\n')
        new_lines = []
        added = False
        for i, line in enumerate(lines):
            new_lines.append(line)
            if 'Group/' in line and '.cpp' in line and not added:
                # Check if next line doesn't have Group - we're at the end of group section
                if i + 1 < len(lines) and 'Group/' not in lines[i+1]:
                    new_lines.append('    ${CMAKE_CURRENT_SOURCE_DIR}/Group/RoleAssignment.cpp')
                    new_lines.append('    ${CMAKE_CURRENT_SOURCE_DIR}/Group/RoleAssignment.h')
                    added = True
        if added:
            content = '\n'.join(new_lines)
        else:
            # Fallback: add near GroupFormation
            content = content.replace(
                '${CMAKE_CURRENT_SOURCE_DIR}/Group/GroupFormation.cpp',
                '${CMAKE_CURRENT_SOURCE_DIR}/Group/GroupFormation.cpp\n    ${CMAKE_CURRENT_SOURCE_DIR}/Group/RoleAssignment.cpp\n    ${CMAKE_CURRENT_SOURCE_DIR}/Group/RoleAssignment.h'
            )

# 4. Add PvP files if not present
if 'PvP/ArenaAI.cpp' not in content:
    print("4. Adding PvP files (ArenaAI, PvPCombatAI)...")
    # Find PvP section or add one
    if '${CMAKE_CURRENT_SOURCE_DIR}/PvP/' in content:
        content = content.replace(
            '${CMAKE_CURRENT_SOURCE_DIR}/PvP/',
            '${CMAKE_CURRENT_SOURCE_DIR}/PvP/ArenaAI.cpp\n    ${CMAKE_CURRENT_SOURCE_DIR}/PvP/ArenaAI.h\n    ${CMAKE_CURRENT_SOURCE_DIR}/PvP/PvPCombatAI.cpp\n    ${CMAKE_CURRENT_SOURCE_DIR}/PvP/PvPCombatAI.h\n    ${CMAKE_CURRENT_SOURCE_DIR}/PvP/',
            1  # Replace only first occurrence
        )
    else:
        # Add PvP section before Quest section or after Economy
        content = content.replace(
            '# Phase 3: Quest System',
            '''# Phase 2: PvP AI Systems
    ${CMAKE_CURRENT_SOURCE_DIR}/PvP/ArenaAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PvP/ArenaAI.h
    ${CMAKE_CURRENT_SOURCE_DIR}/PvP/PvPCombatAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PvP/PvPCombatAI.h

    # Phase 3: Quest System'''
        )

# 5. Add AI Decision files if not present
if 'AI/Decision/DecisionFusionSystem.cpp' not in content:
    print("5. Adding AI Decision files...")
    # Find a good place in AI section
    if 'AI/Decision/' in content:
        # Already has Decision folder, just add the files
        content = content.replace(
            '${CMAKE_CURRENT_SOURCE_DIR}/AI/Decision/',
            '${CMAKE_CURRENT_SOURCE_DIR}/AI/Decision/DecisionFusionSystem.cpp\n    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Decision/DecisionFusionSystem.h\n    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Decision/ActionPriorityQueue.cpp\n    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Decision/ActionPriorityQueue.h\n    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Decision/BehaviorTree.cpp\n    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Decision/BehaviorTree.h\n    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Decision/',
            1
        )
    else:
        # Add after BehaviorTree section or AI Common
        content = content.replace(
            '# AI Common',
            '''# AI Decision System - Phase 5 Enhancement
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Decision/DecisionFusionSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Decision/DecisionFusionSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Decision/ActionPriorityQueue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Decision/ActionPriorityQueue.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Decision/BehaviorTree.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Decision/BehaviorTree.h

    # AI Common'''
        )

# 6. Add AI Services files if not present
if 'AI/Services/ThreatAssistant.cpp' not in content:
    print("6. Adding AI Services files...")
    if 'AI/Services/' in content:
        content = content.replace(
            '${CMAKE_CURRENT_SOURCE_DIR}/AI/Services/',
            '${CMAKE_CURRENT_SOURCE_DIR}/AI/Services/ThreatAssistant.cpp\n    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Services/ThreatAssistant.h\n    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Services/HealingTargetSelector.cpp\n    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Services/HealingTargetSelector.h\n    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Services/',
            1
        )
    else:
        # Add after AI Common
        content = content.replace(
            '# AI Common',
            '''# AI Services - Combat Support Systems
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Services/ThreatAssistant.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Services/ThreatAssistant.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Services/HealingTargetSelector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Services/HealingTargetSelector.h

    # AI Common'''
        )

with open(cmake_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("\nDone! CMakeLists.txt updated.")
print("You need to re-run cmake configure before building.")
