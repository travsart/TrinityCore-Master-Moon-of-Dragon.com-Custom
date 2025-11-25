#!/usr/bin/env python3
"""
Add all missing source files to CMakeLists.txt for linker error resolution.
"""

import os
import re

cmake_path = 'C:/TrinityBots/TrinityCore/src/modules/Playerbot/CMakeLists.txt'

with open(cmake_path, 'r', encoding='utf-8') as f:
    content = f.read()

# List of files to add with their section markers
files_to_add = [
    # RoleAssignment.cpp after RoleAssignment.h in Group section
    {
        'file': '${CMAKE_CURRENT_SOURCE_DIR}/Group/RoleAssignment.cpp',
        'after': '${CMAKE_CURRENT_SOURCE_DIR}/Group/RoleAssignment.h',
        'check': 'Group/RoleAssignment.cpp'
    },
]

# Add PvP files - need to add entire section
pvp_files = '''
    # Phase 2: PvP AI Systems
    ${CMAKE_CURRENT_SOURCE_DIR}/PvP/ArenaAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PvP/ArenaAI.h
    ${CMAKE_CURRENT_SOURCE_DIR}/PvP/PvPCombatAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PvP/PvPCombatAI.h
'''

# Add AI Decision files
ai_decision_files = '''
    # Phase 5: AI Decision Systems
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Decision/DecisionFusionSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Decision/DecisionFusionSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Decision/ActionPriorityQueue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Decision/ActionPriorityQueue.h
'''

# Add AI Services files
ai_services_files = '''
    # AI Services - Combat Support Systems
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Services/ThreatAssistant.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Services/ThreatAssistant.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Services/HealingTargetSelector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Services/HealingTargetSelector.h
'''

changes_made = 0

# 1. Add RoleAssignment.cpp after RoleAssignment.h
if 'Group/RoleAssignment.cpp' not in content:
    print("1. Adding RoleAssignment.cpp...")
    content = content.replace(
        '${CMAKE_CURRENT_SOURCE_DIR}/Group/RoleAssignment.h\n',
        '${CMAKE_CURRENT_SOURCE_DIR}/Group/RoleAssignment.cpp\n    ${CMAKE_CURRENT_SOURCE_DIR}/Group/RoleAssignment.h\n'
    )
    changes_made += 1

# 2. Add PvP files section
if 'PvP/ArenaAI.cpp' not in content:
    print("2. Adding PvP AI files...")
    # Add before Quest section or after Group section
    if '# Phase 3: Quest System' in content:
        content = content.replace(
            '# Phase 3: Quest System',
            pvp_files + '\n    # Phase 3: Quest System'
        )
    elif '# Network Packet Sniffer' in content:
        content = content.replace(
            '# Network Packet Sniffer',
            pvp_files + '\n    # Network Packet Sniffer'
        )
    changes_made += 1

# 3. Add AI Decision files
if 'AI/Decision/DecisionFusionSystem.cpp' not in content:
    print("3. Adding AI Decision files...")
    # Find a good place after AI section starts
    if '# Phase 5: AI Intelligence Framework' in content:
        content = content.replace(
            '# Phase 5: AI Intelligence Framework',
            '# Phase 5: AI Intelligence Framework' + ai_decision_files
        )
    elif '${CMAKE_CURRENT_SOURCE_DIR}/AI/BotAI.cpp' in content:
        content = content.replace(
            '${CMAKE_CURRENT_SOURCE_DIR}/AI/BotAI.cpp',
            '${CMAKE_CURRENT_SOURCE_DIR}/AI/BotAI.cpp' + ai_decision_files
        )
    changes_made += 1

# 4. Add AI Services files
if 'AI/Services/ThreatAssistant.cpp' not in content:
    print("4. Adding AI Services files...")
    # Add after AI Decision files or somewhere in AI section
    if 'AI/Decision/ActionPriorityQueue.h' in content:
        content = content.replace(
            '${CMAKE_CURRENT_SOURCE_DIR}/AI/Decision/ActionPriorityQueue.h',
            '${CMAKE_CURRENT_SOURCE_DIR}/AI/Decision/ActionPriorityQueue.h' + ai_services_files
        )
    elif '${CMAKE_CURRENT_SOURCE_DIR}/AI/BotAI.cpp' in content:
        content = content.replace(
            '${CMAKE_CURRENT_SOURCE_DIR}/AI/BotAI.cpp',
            '${CMAKE_CURRENT_SOURCE_DIR}/AI/BotAI.cpp' + ai_services_files
        )
    changes_made += 1

with open(cmake_path, 'w', encoding='utf-8') as f:
    f.write(content)

print(f"\nDone! Made {changes_made} changes to CMakeLists.txt.")
print("Run cmake configure to regenerate build files.")
