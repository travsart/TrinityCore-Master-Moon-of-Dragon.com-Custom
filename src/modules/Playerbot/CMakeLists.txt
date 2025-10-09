# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

# TrinityCore Playerbot Module - Enterprise Implementation
# High-performance AI-controlled player bots with Intel TBB, parallel hashmap, and boost optimizations

if(NOT BUILD_PLAYERBOT)
    message(STATUS "Playerbot module: DISABLED (use -DBUILD_PLAYERBOT=1 to enable)")
    return()
endif()

message(STATUS "=== Building TrinityCore Playerbot Enterprise Module ===")

# Include enterprise dependency validation
include(${CMAKE_SOURCE_DIR}/cmake/modules/FindPlayerbotDependencies.cmake)

# Verify all enterprise dependencies are available
# Note: Dependencies are validated in FindPlayerbotDependencies.cmake
# The validation script will fail fast if any critical dependencies are missing
message(STATUS "Enterprise dependencies validated by FindPlayerbotDependencies.cmake")

message(STATUS "✅ All enterprise dependencies validated")
message(STATUS "Playerbot module: ENABLED with enterprise optimizations")

# Set source files - Enterprise Phase 1 Implementation
set(PLAYERBOT_SOURCES
    # Module entry point
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerbotModule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerbotModule.h
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerbotModuleAdapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerbotModuleAdapter.h

    # Configuration system (basic)
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotConfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotTradeConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotTradeConfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotLog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotLog.h
    # Temporarily disabled: DependencyValidator (needs MySQL headers fix)
    # ${CMAKE_CURRENT_SOURCE_DIR}/Config/DependencyValidator.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/Config/DependencyValidator.h

    # Session management (essential for bot functionality)
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSession.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSession.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSessionMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSessionMgr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotWorldSessionMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotWorldSessionMgr.h

    # Database pool (basic implementation)
    # Temporarily disabled: BotDatabasePool (needs API updates)
    # ${CMAKE_CURRENT_SOURCE_DIR}/Database/BotDatabasePool.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/Database/BotDatabasePool.h

    # AI system (AI Framework implementation)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BotAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BotAI.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BotAIFactory.cpp

    # Phase 2.1: BehaviorManager Base Class - Manager Pattern Foundation
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BehaviorManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BehaviorManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ExampleManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ExampleManager.h

    # Phase 2.1 Refactoring: BehaviorPriorityManager - Priority-based Behavior Coordination (Fixes Issues #2 & #3)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BehaviorPriorityManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BehaviorPriorityManager.h

    # AI Strategy System (minimal implementation)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/Strategy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/Strategy.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/GroupCombatStrategy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/GroupCombatStrategy.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/IdleStrategy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/IdleStrategy.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/QuestStrategy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/QuestStrategy.h

    # Phase 2.2: CombatMovementStrategy - Role-based Combat Positioning
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/CombatMovementStrategy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/CombatMovementStrategy.h

    # AI Action System (basic)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/Action.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/Action.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/CommonActions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/CommonActions.h

    # Core References System - SafeObjectReference (Issue #4 Fix)
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/References/SafeObjectReference.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/References/SafeObjectReference.h

    # Phase 7.1: Manager System Architecture - Observer-Manager Integration Layer
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Managers/IManagerBase.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Managers/ManagerRegistry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Managers/ManagerRegistry.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Events/EventDispatcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Events/EventDispatcher.h

    # Phase 7.3: Event System - Streamlined Architecture (Legacy code eliminated)
    ${CMAKE_CURRENT_SOURCE_DIR}/Events/BotEventData.h
    # BotEventSystem.cpp/h - DELETED (legacy singleton removed)
    # BotEventHooks.cpp/h - DELETED (1113 lines redundant code eliminated)
    # Observers (15 files) - DELETED (5000+ lines dead code eliminated)
    # Event flow now: TrinityCore ScriptMgr → PlayerbotEventScripts → EventDispatcher → Managers

    # Phase 4.1: Script System Integration - Non-Invasive Event Hooks
    ${CMAKE_CURRENT_SOURCE_DIR}/Scripts/PlayerbotEventScripts.cpp

    # AI Trigger System
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Triggers/Trigger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Triggers/Trigger.h

    # AI Value System
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Values/Value.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Values/Value.h

    # AI Utility Systems
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ResourceManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ResourceManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/CooldownManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/CooldownManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/InterruptManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/InterruptManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ObjectCache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ObjectCache.h

    # ClassAI System - Base and implementations
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/ClassAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/ClassAI.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/ActionPriority.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/ActionPriority.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/CooldownManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/CooldownManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/ResourceManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/ResourceManager.h

    # ClassAI Template Architecture (Phase 3: Code Duplication Elimination)
    # Note: .cpp removed due to old SPEC_* constants, but .h still needed for template definitions
    # ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/CombatSpecializationTemplates.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/CombatSpecializationTemplates.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/ResourceTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/RoleSpecializations.h

    # Baseline Rotation Manager (Levels 1-9 unspecialized bots)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/BaselineRotationManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/BaselineRotationManager.h

    # Death Knight AI
    # DeathKnightAI.cpp has BaselineRotationManager integration - ENABLED
    # DeathKnightAI_Specialization.cpp disabled to prevent duplicate symbols
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/DeathKnights/DeathKnightAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/DeathKnights/DeathKnightAI.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/DeathKnights/DiseaseManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/DeathKnights/DiseaseManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/DeathKnights/RuneManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/DeathKnights/RuneManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/DeathKnights/DeathKnightTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/DeathKnights/UnholySpecialization.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/DeathKnights/BloodSpecialization.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/DeathKnights/FrostSpecialization.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/DeathKnights/DeathKnightAI_Specialization.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/DeathKnights/DeathKnightSpecialization.cpp

    # Death Knight Refactored Specializations (Template-based)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/DeathKnights/BloodDeathKnightRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/DeathKnights/FrostDeathKnightRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/DeathKnights/UnholyDeathKnightRefactored.h

    # Demon Hunter AI
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/DemonHunters/DemonHunterAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/DemonHunters/DemonHunterAI.h

    # Druid AI
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Druids/DruidAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Druids/DruidAI.h

    # Druid Refactored Specializations (Template-based)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Druids/BalanceDruidRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Druids/FeralDruidRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Druids/GuardianDruidRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Druids/RestorationDruidRefactored.h

    # Evoker AI
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Evokers/EvokerAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Evokers/EvokerAI.h

    # Evoker Refactored Specializations (Template-based)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Evokers/DevastationEvokerRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Evokers/PreservationEvokerRefactored.h

    # Hunter AI
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Hunters/HunterAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Hunters/HunterAI.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Hunters/HunterSpecializationBase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Hunters/HunterSpecialization.h
    # Note: Old HunterSpecialization.cpp excluded - uses deprecated APIs
    # HunterSpecializationBase.cpp provides only the base class constructor and helpers
    # Using refactored template-based specializations (BeastMastery/Marksmanship/SurvivalHunterRefactored)

    # Hunter Refactored Specializations (Template-based)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Hunters/BeastMasteryHunterRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Hunters/MarksmanshipHunterRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Hunters/SurvivalHunterRefactored.h

    # Demon Hunter Refactored Specializations (Template-based)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/DemonHunters/HavocDemonHunterRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/DemonHunters/VengeanceDemonHunterRefactored.h

    # Mage AI
    # MageAI.cpp has BaselineRotationManager integration - ENABLED
    # MageAI_Specialization.cpp disabled to prevent duplicate symbols
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Mages/MageAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Mages/MageAI.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Mages/FireSpecialization.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Mages/ArcaneSpecialization.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Mages/FrostSpecialization.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Mages/MageAI_Specialization.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Mages/MageSpecialization.cpp

    # Mage Refactored Specializations (Template-based)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Mages/ArcaneMageRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Mages/FireMageRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Mages/FrostMageRefactored.h

    # Priest Refactored Specializations (Template-based)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Priests/DisciplinePriestRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Priests/HolyPriestRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Priests/ShadowPriestRefactored.h

    # Monk AI
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Monks/MonkAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Monks/MonkAI.h

    # Monk Refactored Specializations (Template-based)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Monks/BrewmasterMonkRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Monks/MistweaverMonkRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Monks/WindwalkerMonkRefactored.h

    # Paladin AI
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Paladins/PaladinAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Paladins/PaladinAI.h

    # Paladin Refactored Specializations (Template-based)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Paladins/HolyPaladinRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Paladins/ProtectionPaladinRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Paladins/RetributionSpecializationRefactored.h

    # Priest AI
    # PriestAI.cpp has BaselineRotationManager integration - ENABLED
    # PriestAI_Specialization.cpp disabled to prevent duplicate symbols
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Priests/PriestAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Priests/PriestAI.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Priests/PriestSpecialization.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Priests/PriestSpecialization.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Priests/DisciplineSpecialization.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Priests/HolySpecialization.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Priests/ShadowSpecialization.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Priests/PriestAI_Specialization.cpp

    # Rogue AI - NOW USING TEMPLATE-BASED REFACTORED ARCHITECTURE
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Rogues/RogueAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Rogues/RogueAI.h
    # LEGACY FILES REMOVED (14 files, 123KB deleted):
    # ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Rogues/RogueCombatPositioning.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Rogues/EnergyManager.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Rogues/EnergyManager.h
    # ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Rogues/AssassinationSpecialization.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Rogues/SubtletySpecialization.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Rogues/CombatSpecialization.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Rogues/RogueAI_Specialization.cpp

    # Rogue Refactored Specializations (Template-based)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Rogues/AssassinationRogueRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Rogues/OutlawRogueRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Rogues/SubtletyRogueRefactored.h
    # ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Rogues/RogueSpecialization.cpp  # REMOVED - Legacy class deleted

    # Shaman AI
    # ShamanAI.cpp has BaselineRotationManager integration - ENABLED (added in this commit)
    # ShamanAI_Specialization.cpp disabled to prevent duplicate symbols
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Shamans/ShamanAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Shamans/ShamanAI.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Shamans/ShamanSpecialization.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Shamans/ShamanSpecialization.h

    # Shaman Refactored Specializations (Template-based)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Shamans/ElementalShamanRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Shamans/EnhancementShamanRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Shamans/RestorationShamanRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Shamans/RestorationSpecialization.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Shamans/EnhancementSpecialization.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Shamans/ElementalSpecialization.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Shamans/ShamanAI_Specialization.cpp

    # Warlock AI - Using enterprise WarlockAI.cpp with BaselineRotationManager
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Warlocks/WarlockAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Warlocks/WarlockAI.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Warlocks/AfflictionSpecialization.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Warlocks/DestructionSpecialization.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Warlocks/DemonologySpecialization.cpp
    # WarlockAI_Specialization.cpp disabled - using full WarlockAI.cpp implementation
    # ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Warlocks/WarlockAI_Specialization.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Warlocks/WarlockSpecialization.cpp

    # Warlock Refactored Specializations (Template-based)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Warlocks/AfflictionWarlockRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Warlocks/DemonologyWarlockRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Warlocks/DestructionWarlockRefactored.h

    # Warrior AI
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Warriors/WarriorAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Warriors/WarriorAI.h

    # Warrior Refactored Specializations (Template-based)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Warriors/ArmsWarriorRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Warriors/FuryWarriorRefactored.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ClassAI/Warriors/ProtectionWarriorRefactored.h

    # Group Management System
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/GroupInvitationHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/GroupInvitationHandler.h

    # Movement System
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/BotMovementUtil.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/BotMovementUtil.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/LeaderFollowBehavior.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/LeaderFollowBehavior.h

    # Phase 2: Account Management System
    ${CMAKE_CURRENT_SOURCE_DIR}/Account/BotAccountMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Account/BotAccountMgr.h

    # Phase 2: Character Management System
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotNameMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotNameMgr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotCharacterDistribution.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotCharacterDistribution.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotCustomizationGenerator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotCustomizationGenerator.h

    # Phase 2: Bot Lifecycle Management System
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawner.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotCharacterCreator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotCharacterCreator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotScheduler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotScheduler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotLifecycleMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotLifecycleMgr.h

    # WoW 11.2 Character Creation Data (header-only)
    ${CMAKE_CURRENT_SOURCE_DIR}/Data/WoW112CharacterCreation.h

    # Phase 2: Refactored Architecture Components
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawnOrchestrator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawnOrchestrator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotResourcePool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotResourcePool.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotPerformanceMonitor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotPerformanceMonitor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawnEventBus.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawnEventBus.h

    # Performance Optimization System (Phase 5)
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ObjectPool.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/PacketPoolManager.h

    # Phase 2: Enhanced Session Management
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSessionFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSessionFactory.h

    # AI Combat System - Group Combat Coordination
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/GroupCombatTrigger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/GroupCombatTrigger.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/RoleBasedCombatPositioning.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/RoleBasedCombatPositioning.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/PositionManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/PositionManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/MechanicAwareness.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/MechanicAwareness.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/InterruptCoordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/InterruptCoordinator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/InterruptAwareness.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/InterruptAwareness.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/InterruptManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/InterruptManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/TargetSelector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/TargetSelector.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/TargetScanner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/TargetScanner.h

    # Integration Tests
    ${CMAKE_CURRENT_SOURCE_DIR}/Tests/TargetScannerTest.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/TargetAssistAction.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/TargetAssistAction.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/SpellInterruptAction.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/SpellInterruptAction.h
    # ThreatManager - resolved naming conflict by renaming to BotThreatManager
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/BotThreatManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/BotThreatManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/ThreatAbilities.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/ThreatAbilities.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/ThreatCoordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/ThreatCoordinator.h
# Phase 2 Day 14: Combat AI Integration    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/CombatAIIntegrator.cpp    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/CombatAIIntegrator.h    ${CMAKE_CURRENT_SOURCE_DIR}/AI/EnhancedBotAI.cpp    ${CMAKE_CURRENT_SOURCE_DIR}/AI/EnhancedBotAI.h

    # Phase 2: Interface Definitions (header-only)
    ${CMAKE_CURRENT_SOURCE_DIR}/Interfaces/IBotSpawner.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Interfaces/IBotSession.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Interfaces/IBotPerformance.h

    # Phase 2: Adapter Pattern Implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/Adapters/BotSpawnerAdapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Adapters/BotSpawnerAdapter.h

    # Database systems
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotCharacterDBInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotCharacterDBInterface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabase.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabaseConnection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabaseConnection.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotResultSet.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotResultSet.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabaseStatements.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabaseStatements.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotMigrationMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotMigrationMgr.h

    # Script integration
    ${CMAKE_CURRENT_SOURCE_DIR}/Scripts/PlayerbotWorldScript.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Scripts/PlayerbotWorldScript.h

    # Social Systems - WoW 11.2 Economy Integration
    # Phase 2.4: Replaced by TradeManager and AuctionManager (BehaviorManager pattern)
    # TradeManager enabled - now inherits from BehaviorManager
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/TradeManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/TradeManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/TradeManager_Events.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/MarketAnalysis.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/MarketAnalysis.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/GuildIntegration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/GuildIntegration.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/LootDistribution.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/LootDistribution.h

    # Equipment Management - Complete Auto-Equip + Junk Selling + Consumables
    ${CMAKE_CURRENT_SOURCE_DIR}/Equipment/EquipmentManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Equipment/EquipmentManager.h

    # Profession System - Complete Crafting + Gathering Automation (Phase 1, 2, 3)
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/ProfessionManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/ProfessionManager.h
    # Phase 2.4: Replaced by GatheringManager (BehaviorManager pattern)
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/GatheringManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/GatheringManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/FarmingCoordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/FarmingCoordinator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/ProfessionAuctionBridge.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/ProfessionAuctionBridge.h

    # Economy Systems - Advanced Auction House Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/Economy/AuctionManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Economy/AuctionManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Economy/AuctionManager_Events.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Economy/PlayerbotAuctionConfig.h
    # BotAI_Auction_Integration no longer needed - AuctionManager is managed by BotAI
    # ${CMAKE_CURRENT_SOURCE_DIR}/Economy/BotAI_Auction_Integration.cpp

    # Quest Systems - Dynamic Quest Management
    # Phase 2.4: Replaced by QuestManager (BehaviorManager pattern)
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestPickup.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestPickup.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/DynamicQuestSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/DynamicQuestSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/ObjectiveTracker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/ObjectiveTracker.h

    # Game Systems - Integrated Quest, Inventory, Trade Management
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/QuestManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/QuestManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/QuestManager_Events.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/NPCInteractionManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/NPCInteractionManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/InventoryManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/InventoryManager.h
    # Config files commented out - incomplete implementations
    # ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotInventoryConfig.h
    # ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotQuestConfig.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotQuestConfig.h

    # Advanced Features - Group/Raid, Economy, Social, Advanced Behaviors
    ${CMAKE_CURRENT_SOURCE_DIR}/Advanced/GroupCoordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Advanced/GroupCoordinator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Advanced/EconomyManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Advanced/EconomyManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Advanced/SocialManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Advanced/SocialManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Advanced/AdvancedBehaviorManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Advanced/AdvancedBehaviorManager.h

    # Performance Optimization (Phase 5) - ThreadPool, MemoryPool, QueryOptimizer, Profiler
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ThreadPool/ThreadPool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ThreadPool/ThreadPool.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/MemoryPool/MemoryPool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/MemoryPool/MemoryPool.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/QueryOptimizer/QueryOptimizer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/QueryOptimizer/QueryOptimizer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/Profiler/Profiler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/Profiler/Profiler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/PerformanceManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/PerformanceManager.h

    # Phase 3: Game System Integration - Movement System (Strategy-Based Architecture)
    # MovementManager REMOVED - Movement is handled by strategies using BotMovementUtil
    # See MOVEMENT_ARCHITECTURE_FINAL.md for rationale
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Core/MovementTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Core/MovementValidator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Core/MovementValidator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Pathfinding/PathfindingAdapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Pathfinding/PathfindingAdapter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Pathfinding/PathOptimizer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Pathfinding/PathOptimizer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Pathfinding/NavMeshInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Pathfinding/NavMeshInterface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/LeaderFollowBehavior.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/LeaderFollowBehavior.h

    # Phase 3: Game System Integration - NPC Interaction System
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/Core/InteractionManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/Core/InteractionManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/Core/InteractionTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/Core/GossipHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/Core/GossipHandler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/Core/InteractionValidator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/Core/InteractionValidator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/Vendors/VendorInteraction.h

    # Phase 3: Game System Integration - Quest System Completion
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestCompletion.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestCompletion.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestTurnIn.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestTurnIn.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestValidation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestValidation.h

    # Phase 3: Core Combat Behavior System (State Analysis & Adaptive Behavior)
    # Foundation for intelligent combat decision-making
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/CombatStateAnalyzer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/CombatStateAnalyzer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/AdaptiveBehaviorManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/AdaptiveBehaviorManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/CombatBehaviorIntegration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/CombatBehaviorIntegration.h

    # Phase 4: Advanced Combat Utilities (Enterprise-Grade Combat Coordination)
    # Implements missing combat coordination utilities for all 119 ClassAI implementations
    # Performance: <0.1ms total per bot, scales to 5000+ bots
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/CombatBehaviors/DefensiveBehaviorManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/CombatBehaviors/DefensiveBehaviorManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/CombatBehaviors/InterruptRotationManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/CombatBehaviors/InterruptRotationManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/CombatBehaviors/DispelCoordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/CombatBehaviors/DispelCoordinator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/CombatBehaviors/AoEDecisionManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/CombatBehaviors/AoEDecisionManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/CombatBehaviors/CooldownStackingOptimizer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/CombatBehaviors/CooldownStackingOptimizer.h

    # Commands - Chat command system for bot management
    ${CMAKE_CURRENT_SOURCE_DIR}/Commands/PlayerbotCommands.cpp
)

# Group sources for IDE organization
source_group("" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerbotModule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerbotModule.h
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerbotModuleAdapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerbotModuleAdapter.h
)

source_group("Config" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotConfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotTradeConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotTradeConfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotLog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotLog.h
)


source_group("Database" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotCharacterDBInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotCharacterDBInterface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabase.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabaseConnection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabaseConnection.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotResultSet.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotResultSet.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabaseStatements.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabaseStatements.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotMigrationMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotMigrationMgr.h
)

source_group("AI" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BotAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BotAI.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BotAIFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BehaviorManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BehaviorManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ExampleManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ExampleManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BehaviorPriorityManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BehaviorPriorityManager.h
)

source_group("AI\\Strategy" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/Strategy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/Strategy.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/GroupCombatStrategy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/GroupCombatStrategy.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/IdleStrategy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/IdleStrategy.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/CombatMovementStrategy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/CombatMovementStrategy.h
)

source_group("AI\\Actions" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/Action.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/Action.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/CommonActions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/CommonActions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/TargetAssistAction.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/TargetAssistAction.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/SpellInterruptAction.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/SpellInterruptAction.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/RestAction.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/RestAction.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/BuffAction.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/BuffAction.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/LootAction.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/LootAction.h
)

source_group("AI\\Triggers" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Triggers/Trigger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Triggers/Trigger.h
)

source_group("AI\\Combat" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/GroupCombatTrigger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/GroupCombatTrigger.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/RoleBasedCombatPositioning.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/RoleBasedCombatPositioning.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/PositionManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/PositionManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/MechanicAwareness.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/MechanicAwareness.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/InterruptCoordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/InterruptCoordinator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/InterruptAwareness.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/InterruptAwareness.h
    # ThreatManager - resolved naming conflict by renaming to BotThreatManager
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/BotThreatManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/BotThreatManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/ThreatAbilities.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/ThreatAbilities.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/ThreatCoordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/ThreatCoordinator.h
# Phase 2 Day 14: Combat AI Integration    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/CombatAIIntegrator.cpp    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/CombatAIIntegrator.h    ${CMAKE_CURRENT_SOURCE_DIR}/AI/EnhancedBotAI.cpp    ${CMAKE_CURRENT_SOURCE_DIR}/AI/EnhancedBotAI.h
)

source_group("AI\\Values" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Values/Value.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Values/Value.h
)

source_group("AI\\Utilities" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ResourceManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/ResourceManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/CooldownManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/CooldownManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/InterruptManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/InterruptManager.h
)

source_group("Group" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/GroupInvitationHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/GroupInvitationHandler.h
)

source_group("Movement" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/LeaderFollowBehavior.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/LeaderFollowBehavior.h
)

source_group("Account" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Account/BotAccountMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Account/BotAccountMgr.h
)

source_group("Character" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotNameMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotNameMgr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotCharacterDistribution.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotCharacterDistribution.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotCustomizationGenerator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotCustomizationGenerator.h
)

source_group("Lifecycle" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawner.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotLifecycleMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotLifecycleMgr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotScheduler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotScheduler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawnOrchestrator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawnOrchestrator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotResourcePool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotResourcePool.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotPerformanceMonitor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotPerformanceMonitor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawnEventBus.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawnEventBus.h
)

source_group("Session" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSession.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSession.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSessionMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSessionMgr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotWorldSessionMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotWorldSessionMgr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSessionFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSessionFactory.h
)

source_group("Interfaces" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Interfaces/IBotSpawner.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Interfaces/IBotSession.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Interfaces/IBotPerformance.h
)

source_group("Performance" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ObjectPool.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/PacketPoolManager.h
)

source_group("Adapters" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Adapters/BotSpawnerAdapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Adapters/BotSpawnerAdapter.h
)

source_group("Scripts" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Scripts/PlayerbotWorldScript.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Scripts/PlayerbotWorldScript.h
)

source_group("Social" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/TradeManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/TradeManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/TradeManager_Events.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/MarketAnalysis.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/MarketAnalysis.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/GuildIntegration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/GuildIntegration.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/LootDistribution.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Social/LootDistribution.h
)

source_group("Equipment" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Equipment/EquipmentManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Equipment/EquipmentManager.h
)

source_group("Professions" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/ProfessionManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/ProfessionManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/GatheringManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/GatheringManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/FarmingCoordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/FarmingCoordinator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/ProfessionAuctionBridge.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Professions/ProfessionAuctionBridge.h
)

source_group("Economy" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Economy/AuctionManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Economy/AuctionManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Economy/AuctionManager_Events.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Economy/PlayerbotAuctionConfig.h
    # ${CMAKE_CURRENT_SOURCE_DIR}/Economy/BotAI_Auction_Integration.cpp
)

source_group("Quest" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/DynamicQuestSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/DynamicQuestSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/ObjectiveTracker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/ObjectiveTracker.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestPickup.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestPickup.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestCompletion.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestCompletion.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestTurnIn.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestTurnIn.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestValidation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Quest/QuestValidation.h
)

source_group("Game" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/QuestManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/QuestManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/QuestManager_Events.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/NPCInteractionManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/NPCInteractionManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/InventoryManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/InventoryManager.h
)

source_group("Advanced" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Advanced/GroupCoordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Advanced/GroupCoordinator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Advanced/EconomyManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Advanced/EconomyManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Advanced/SocialManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Advanced/SocialManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Advanced/AdvancedBehaviorManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Advanced/AdvancedBehaviorManager.h
)

source_group("Performance" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ThreadPool/ThreadPool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/ThreadPool/ThreadPool.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/MemoryPool/MemoryPool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/MemoryPool/MemoryPool.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/QueryOptimizer/QueryOptimizer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/QueryOptimizer/QueryOptimizer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/Profiler/Profiler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/Profiler/Profiler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/PerformanceManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Performance/PerformanceManager.h
)

source_group("Movement" FILES
    # MovementManager removed - strategy-based architecture
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Core/MovementTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Core/MovementValidator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Core/MovementValidator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Pathfinding/PathfindingAdapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Pathfinding/PathfindingAdapter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Pathfinding/PathOptimizer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Pathfinding/PathOptimizer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Pathfinding/NavMeshInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/Pathfinding/NavMeshInterface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/LeaderFollowBehavior.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/LeaderFollowBehavior.h
)

source_group("Interaction" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/Core/InteractionManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/Core/InteractionManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/Core/InteractionTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/Core/GossipHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/Core/GossipHandler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/Core/InteractionValidator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/Core/InteractionValidator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Interaction/Vendors/VendorInteraction.h
)

source_group("Commands" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Commands/PlayerbotCommands.cpp
)

# Create the playerbot library
add_library(playerbot ${PLAYERBOT_SOURCES})

# Set target properties with enterprise C++20 standards
set_target_properties(playerbot PROPERTIES
    OUTPUT_NAME "playerbot"
    FOLDER "modules"
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Include directories with enterprise dependencies
target_include_directories(playerbot
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/Config
        ${CMAKE_CURRENT_SOURCE_DIR}/Core
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/References
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/Managers
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/Events
        ${CMAKE_CURRENT_SOURCE_DIR}/Events
        ${CMAKE_CURRENT_SOURCE_DIR}/Session
        ${CMAKE_CURRENT_SOURCE_DIR}/Database
        ${CMAKE_CURRENT_SOURCE_DIR}/AI
        ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy
        ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions
        ${CMAKE_CURRENT_SOURCE_DIR}/AI/Triggers
        ${CMAKE_CURRENT_SOURCE_DIR}/AI/Values
        ${CMAKE_CURRENT_SOURCE_DIR}/Interfaces
        ${CMAKE_CURRENT_SOURCE_DIR}/Adapters
        ${CMAKE_CURRENT_SOURCE_DIR}/Economy
        ${CMAKE_SOURCE_DIR}/src/modules/Common
        ${MYSQL_INCLUDE_DIR}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Account
        ${CMAKE_CURRENT_SOURCE_DIR}/Character
        ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle
        ${CMAKE_CURRENT_SOURCE_DIR}/Scripts
        ${CMAKE_CURRENT_BINARY_DIR}
        ${PLAYERBOT_INCLUDE_DIRS}
)

# Link against TrinityCore and enterprise dependencies
target_link_libraries(playerbot
    PUBLIC
        # TrinityCore core libraries
        game-interface
        shared
        database
        common
        # Shared module libraries
        modules-common
    PRIVATE
        # Enterprise dependencies
        ${PLAYERBOT_LIBRARIES}
        playerbot-dependencies
        # MySQL libraries for database connection
        ${MYSQL_LIBRARY}
)

# Platform-specific libraries for enterprise dependencies
if(WIN32)
    target_link_libraries(playerbot PRIVATE ws2_32 wsock32 mswsock)
elseif(UNIX)
    target_link_libraries(playerbot PRIVATE pthread dl)
endif()

# Enterprise preprocessor definitions
target_compile_definitions(playerbot
    PRIVATE
        BUILD_PLAYERBOT=1
        TRINITY_API_EXPORT_PLAYERBOT
        $<$<CONFIG:Debug>:PLAYERBOT_DEBUG=1>
        $<$<CONFIG:Release>:PLAYERBOT_RELEASE=1>
)

# Platform-specific definitions
if(WIN32)
    target_compile_definitions(playerbot PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# Enterprise optimization flags - only for Release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Configuring enterprise performance optimizations")

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(playerbot PRIVATE
            $<$<CONFIG:Release>:-O3>                    # Maximum optimization
            $<$<CONFIG:Release>:-march=native>          # Target current CPU
            $<$<CONFIG:Release>:-mtune=native>          # Tune for current CPU
            $<$<CONFIG:Release>:-flto>                  # Link-time optimization
            $<$<CONFIG:Release>:-ffast-math>            # Fast math for SIMD
            $<$<CONFIG:Release>:-funroll-loops>         # Loop unrolling
            $<$<CONFIG:Release>:-ftree-vectorize>       # Auto-vectorization
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(playerbot PRIVATE
            $<$<CONFIG:Release>:/O2>                    # Maximum optimization (Release only)
            $<$<CONFIG:Release>:/GL>                    # Whole program optimization (Release only)
            $<$<CONFIG:Release>:/fp:fast>               # Fast floating point (Release only)
            $<$<CONFIG:Release>:/arch:AVX2>             # Enable AVX2 if available (Release only)
        )
        # Apply link-time optimizations only for Release builds
        target_link_options(playerbot PRIVATE $<$<CONFIG:Release>:/LTCG>)
    endif()
endif()

# Install configuration file
if(CONF_DIR)
    install(FILES conf/playerbots.conf.dist DESTINATION ${CONF_DIR}/modules)
endif()

# Install SQL migration files and structure
if(CONF_DIR)
    # Create sql/migrations directory in installation
    install(DIRECTORY DESTINATION ${CONF_DIR}/sql)
    install(DIRECTORY DESTINATION ${CONF_DIR}/sql/migrations)

    # Install all migration files
    install(FILES
        sql/migrations/001_initial_schema.sql
        sql/migrations/002_account_management.sql
        sql/migrations/003_lifecycle_management.sql
        sql/migrations/004_character_distribution.sql
        sql/migrations/005_initial_data.sql
        sql/migrations/006_bot_names.sql
        DESTINATION ${CONF_DIR}/sql/migrations
    )

    message(STATUS "Playerbot migration files will be installed to: ${CONF_DIR}/sql/migrations/")
endif()

# Legacy SQL installation (deprecated - keeping for compatibility)
if(SQL_DIR)
    install(DIRECTORY sql/ DESTINATION ${SQL_DIR}/playerbot)
endif()

# Add to game library dependencies
if(TARGET game)
    target_link_libraries(game PRIVATE playerbot)
    target_compile_definitions(game PRIVATE
        BUILD_PLAYERBOT=1
    )
endif()

# Enterprise dependency verification test (temporarily disabled)
# add_executable(playerbot-dependency-test
#     Config/DependencyValidator.cpp
#     Config/DependencyValidator.h
# )
#
# target_link_libraries(playerbot-dependency-test
#     PRIVATE
#         playerbot-dependencies
#         shared  # For Trinity logging
# )
#
# target_include_directories(playerbot-dependency-test
#     PRIVATE
#         ${CMAKE_CURRENT_SOURCE_DIR}/Config
# )
#
# # Run dependency validation as part of build
# add_custom_command(
#     TARGET playerbot-dependency-test POST_BUILD
#     COMMAND playerbot-dependency-test
#     COMMENT "Validating Playerbot enterprise dependencies"
#     VERBATIM
# )

# Unit tests (temporarily disabled)
# if(BUILD_TESTING)
#     # Performance benchmarks
#     add_executable(playerbot-benchmark
#         Tests/BenchmarkTests.cpp
#         Session/BotSession.cpp
#         Session/BotSessionMgr.cpp
#     )
#
#     target_link_libraries(playerbot-benchmark
#         PRIVATE
#             playerbot-dependencies
#             shared
#     )
#
#     target_include_directories(playerbot-benchmark
#         PRIVATE
#             ${CMAKE_CURRENT_SOURCE_DIR}
#     )
#
#     set_target_properties(playerbot-benchmark PROPERTIES
#         FOLDER "tests"
#         OUTPUT_NAME "playerbot-benchmark"
#     )
# endif()

# Enterprise quality validation
message(STATUS "=== Playerbot Enterprise Configuration Summary ===")
message(STATUS "  -> Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  -> C++ Standard: C++20")
message(STATUS "  -> Intel TBB: ${PLAYERBOT_TBB_FOUND}")
message(STATUS "  -> Parallel Hashmap: ${PLAYERBOT_PHMAP_FOUND}")
message(STATUS "  -> Boost: ${PLAYERBOT_BOOST_FOUND}")
message(STATUS "  -> MySQL: ${PLAYERBOT_MYSQL_FOUND}")
message(STATUS "  -> Performance Optimizations: $<$<CONFIG:Release>:Enabled>")
message(STATUS "  -> Source files: ${PLAYERBOT_SOURCES}")
message(STATUS "🚀 TrinityCore Playerbot enterprise module ready for compilation")
