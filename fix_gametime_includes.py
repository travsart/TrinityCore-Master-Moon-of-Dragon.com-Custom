#!/usr/bin/env python3
"""
Fix missing GameTime.h includes for Phase 7A
Fixes 540 C3861 errors: "GetGameTimeMS": Identifier not found
"""

import os
import re
from pathlib import Path

# List of files from error log that need GameTime.h include
FILES = [
    "src/modules/Playerbot/AI/BehaviorTree/Nodes/CombatNodes.h",
    "src/modules/Playerbot/AI/BehaviorTree/Nodes/HealingNodes.h",
    "src/modules/Playerbot/AI/ClassAI/ActionPriority.h",
    "src/modules/Playerbot/AI/ClassAI/BaselineRotationManager.cpp",
    "src/modules/Playerbot/AI/ClassAI/ClassAI.h",
    "src/modules/Playerbot/AI/ClassAI/Common/StatusEffectTracker.h",
    "src/modules/Playerbot/AI/ClassAI/DeathKnights/DiseaseManager.cpp",
    "src/modules/Playerbot/AI/ClassAI/DeathKnights/DiseaseManager.h",
    "src/modules/Playerbot/AI/ClassAI/DemonHunters/DemonHunterAI.cpp",
    "src/modules/Playerbot/AI/ClassAI/Druids/BalanceDruidRefactored.h",
    "src/modules/Playerbot/AI/ClassAI/Evokers/EvokerAI.cpp",
    "src/modules/Playerbot/AI/ClassAI/Evokers/EvokerAI.h",
    "src/modules/Playerbot/AI/ClassAI/Hunters/BeastMasteryHunterRefactored.h",
    "src/modules/Playerbot/AI/ClassAI/Hunters/HunterAI.h",
    "src/modules/Playerbot/AI/ClassAI/Paladins/PaladinAI.cpp",
    "src/modules/Playerbot/AI/ClassAI/Priests/PriestAI.cpp",
    "src/modules/Playerbot/AI/ClassAI/ResourceManager.cpp",
    "src/modules/Playerbot/AI/ClassAI/ResourceManager.h",
    "src/modules/Playerbot/AI/ClassAI/ResourceTypes.h",
    "src/modules/Playerbot/AI/ClassAI/Shamans/ShamanAI.cpp",
    "src/modules/Playerbot/AI/Combat/CrowdControlManager.h",
    "src/modules/Playerbot/AI/Combat/DefensiveManager.h",
    "src/modules/Playerbot/AI/Combat/GroupCombatTrigger.cpp",
    "src/modules/Playerbot/AI/Combat/InterruptCoordinator.cpp",
    "src/modules/Playerbot/AI/Combat/MechanicAwareness.cpp",
    "src/modules/Playerbot/AI/Combat/MovementIntegration.h",
    "src/modules/Playerbot/AI/Coordination/RaidOrchestrator.cpp",
    "src/modules/Playerbot/AI/Coordination/RoleCoordinator.cpp",
    "src/modules/Playerbot/AI/Strategy/GroupCombatStrategy.cpp",
    "src/modules/Playerbot/AI/Strategy/LootStrategy.cpp",
    "src/modules/Playerbot/AI/Strategy/QuestStrategy.cpp",
    "src/modules/Playerbot/AI/Strategy/RestStrategy.cpp",
    "src/modules/Playerbot/Advanced/AdvancedBehaviorManager.cpp",
    "src/modules/Playerbot/Advanced/SocialManager.cpp",
    "src/modules/Playerbot/Character/BotCharacterDistribution.cpp",
    "src/modules/Playerbot/Chat/BotChatCommandHandler.cpp",
    "src/modules/Playerbot/Game/InventoryManager.cpp",
    "src/modules/Playerbot/Game/NPCInteractionManager.cpp",
    "src/modules/Playerbot/Game/QuestAcceptanceManager.cpp",
    "src/modules/Playerbot/Group/RoleAssignment.h",
    "src/modules/Playerbot/Movement/Arbiter/MovementArbiter.cpp",
    "src/modules/Playerbot/Movement/Arbiter/MovementRequest.cpp",
    "src/modules/Playerbot/Movement/LeaderFollowBehavior.cpp",
    "src/modules/Playerbot/Movement/QuestPathfinder.cpp",
    "src/modules/Playerbot/Professions/ProfessionAuctionBridge.cpp",
    "src/modules/Playerbot/Professions/ProfessionManager.cpp",
    "src/modules/Playerbot/Quest/DynamicQuestSystem.cpp",
    "src/modules/Playerbot/Quest/DynamicQuestSystem.h",
    "src/modules/Playerbot/Quest/ObjectiveTracker.cpp",
    "src/modules/Playerbot/Quest/ObjectiveTracker.h",
    "src/modules/Playerbot/Quest/QuestCompletion.h",
    "src/modules/Playerbot/Quest/QuestPickup.cpp",
    "src/modules/Playerbot/Quest/QuestPickup.h",
    "src/modules/Playerbot/Quest/UnifiedQuestManager.cpp",
    "src/modules/Playerbot/Session/BotPacketRelay.cpp",
    "src/modules/Playerbot/Session/BotPacketSimulator.cpp",
    "src/modules/Playerbot/Session/BotPriorityManager.cpp",
    "src/modules/Playerbot/Session/BotWorldSessionMgr.cpp",
    "src/modules/Playerbot/Social/AuctionHouse.h",
    "src/modules/Playerbot/Social/GuildIntegration.cpp",
    "src/modules/Playerbot/Social/GuildIntegration.h",
    "src/modules/Playerbot/Social/LootAnalysis.h",
    "src/modules/Playerbot/Social/LootCoordination.h",
    "src/modules/Playerbot/Social/LootDistribution.cpp",
    "src/modules/Playerbot/Social/LootDistribution.h",
    "src/modules/Playerbot/Social/MarketAnalysis.cpp",
    "src/modules/Playerbot/Social/MarketAnalysis.h",
    "src/modules/Playerbot/Social/UnifiedLootManager.cpp",
    "src/modules/Playerbot/Social/UnifiedLootManager.h",
]

def add_gametime_include(filepath):
    """Add #include "GameTime.h" to a file if needed."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            lines = f.readlines()

        # Check if already has GameTime.h include
        for line in lines:
            if re.search(r'#include.*GameTime\.h', line):
                return False, "already has GameTime.h"

        # Check if file uses GameTime::GetGameTimeMS
        content = ''.join(lines)
        if 'GameTime::GetGameTimeMS' not in content:
            return False, "doesn't use GameTime::GetGameTimeMS"

        # Find last #include line
        last_include_idx = -1
        for i, line in enumerate(lines):
            if line.strip().startswith('#include'):
                last_include_idx = i

        if last_include_idx == -1:
            return False, "no #include lines found"

        # Insert #include "GameTime.h" after last include
        lines.insert(last_include_idx + 1, '#include "GameTime.h"\n')

        # Write back
        with open(filepath, 'w', encoding='utf-8') as f:
            f.writelines(lines)

        return True, f"added GameTime.h after line {last_include_idx + 1}"

    except Exception as e:
        return False, f"error: {e}"

def main():
    print("=== Phase 7A: Adding GameTime.h includes ===")
    print()

    fixed = 0
    skipped = 0

    for file in FILES:
        filepath = Path(file)

        if not filepath.exists():
            print(f"SKIP: File not found: {file}")
            skipped += 1
            continue

        success, message = add_gametime_include(filepath)

        if success:
            print(f"FIXED: {file} ({message})")
            fixed += 1
        else:
            print(f"SKIP: {file} ({message})")
            skipped += 1

    print()
    print("=== Summary ===")
    print(f"Files fixed: {fixed}")
    print(f"Files skipped: {skipped}")
    print(f"Total files processed: {len(FILES)}")
    print()
    print("Expected error reduction: ~540 errors (C3861: GetGameTimeMS)")

if __name__ == "__main__":
    main()
