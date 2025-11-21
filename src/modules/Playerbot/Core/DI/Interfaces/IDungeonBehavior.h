/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "Define.h"
#include "Position.h"
#include <string>
#include <vector>
#include <atomic>
#include <chrono>

// Forward declarations
class Player;
class Group;
class Unit;

namespace Playerbot
{

// Forward declarations for nested types
struct DungeonData;
struct DungeonEncounter;
enum class DungeonRole : uint8;
enum class EncounterStrategy : uint8;
enum class ThreatManagement : uint8;

// DungeonMetrics definition (needs full definition for return by value)
struct DungeonMetrics
{
    ::std::atomic<uint32> dungeonsCompleted{0};
    ::std::atomic<uint32> dungeonsAttempted{0};
    ::std::atomic<uint32> encountersCompleted{0};
    ::std::atomic<uint32> encounterWipes{0};
    ::std::atomic<float> averageCompletionTime{2700000.0f}; // 45 minutes
    ::std::atomic<float> successRate{0.85f};
    ::std::atomic<float> encounterSuccessRate{0.9f};
    ::std::atomic<uint32> totalDamageDealt{0};
    ::std::atomic<uint32> totalHealingDone{0};
    ::std::chrono::steady_clock::time_point lastUpdate;

    void Reset() {
        dungeonsCompleted = 0; dungeonsAttempted = 0; encountersCompleted = 0;
        encounterWipes = 0; averageCompletionTime = 2700000.0f; successRate = 0.85f;
        encounterSuccessRate = 0.9f; totalDamageDealt = 0; totalHealingDone = 0;
        lastUpdate = ::std::chrono::steady_clock::now();
    }

    float GetCompletionRate() const {
        uint32 attempted = dungeonsAttempted.load();
        uint32 completed = dungeonsCompleted.load();
        return attempted > 0 ? (float)completed / attempted : 0.0f;
    }
};

/**
 * @brief Interface for comprehensive dungeon behavior automation
 *
 * Defines the contract for automated dungeon navigation, encounter management,
 * role coordination, and performance optimization for group content.
 */
class TC_GAME_API IDungeonBehavior
{
public:
    virtual ~IDungeonBehavior() = default;

    // Core dungeon management
    virtual bool EnterDungeon(Group* group, uint32 dungeonId) = 0;
    virtual void UpdateDungeonProgress(Group* group) = 0;
    virtual void HandleDungeonCompletion(Group* group) = 0;
    virtual void HandleDungeonWipe(Group* group) = 0;

    // Encounter management
    virtual void StartEncounter(Group* group, uint32 encounterId) = 0;
    virtual void UpdateEncounter(Group* group, uint32 encounterId) = 0;
    virtual void CompleteEncounter(Group* group, uint32 encounterId) = 0;
    virtual void HandleEncounterWipe(Group* group, uint32 encounterId) = 0;

    // Role-specific behavior coordination
    virtual void CoordinateTankBehavior(Player* tank, const DungeonEncounter& encounter) = 0;
    virtual void CoordinateHealerBehavior(Player* healer, const DungeonEncounter& encounter) = 0;
    virtual void CoordinateDpsBehavior(Player* dps, const DungeonEncounter& encounter) = 0;
    virtual void CoordinateCrowdControlBehavior(Player* cc, const DungeonEncounter& encounter) = 0;

    // Movement and positioning
    virtual void UpdateGroupPositioning(Group* group, const DungeonEncounter& encounter) = 0;
    virtual void HandleSpecialPositioning(Group* group, uint32 encounterId) = 0;
    virtual Position GetOptimalPosition(Player* player, DungeonRole role, const DungeonEncounter& encounter) = 0;
    virtual void AvoidDangerousAreas(Player* player, const ::std::vector<Position>& dangerousAreas) = 0;

    // Trash mob handling
    virtual void HandleTrashMobs(Group* group, const ::std::vector<uint32>& trashMobIds) = 0;
    virtual void PullTrashGroup(Group* group, const ::std::vector<Unit*>& trashMobs) = 0;
    virtual void AssignTrashTargets(Group* group, const ::std::vector<Unit*>& trashMobs) = 0;
    virtual void ExecuteTrashStrategy(Group* group, const ::std::vector<Unit*>& trashMobs) = 0;

    // Boss encounter strategies
    virtual void ExecuteBossStrategy(Group* group, const DungeonEncounter& encounter) = 0;
    virtual void HandleBossMechanics(Group* group, uint32 encounterId, const ::std::string& mechanic) = 0;
    virtual void AdaptToEncounterPhase(Group* group, uint32 encounterId, uint32 phase) = 0;
    virtual void HandleEnrageTimer(Group* group, const DungeonEncounter& encounter) = 0;

    // Threat and aggro management
    virtual void ManageGroupThreat(Group* group, const DungeonEncounter& encounter) = 0;
    virtual void HandleTankSwap(Group* group, Player* currentTank, Player* newTank) = 0;
    virtual void ManageThreatMeters(Group* group) = 0;
    virtual void HandleThreatEmergency(Group* group, Player* player) = 0;

    // Healing and damage coordination
    virtual void CoordinateGroupHealing(Group* group, const DungeonEncounter& encounter) = 0;
    virtual void CoordinateGroupDamage(Group* group, const DungeonEncounter& encounter) = 0;
    virtual void HandleHealingEmergency(Group* group) = 0;
    virtual void OptimizeDamageOutput(Group* group, const DungeonEncounter& encounter) = 0;

    // Crowd control and utility
    virtual void CoordinateCrowdControl(Group* group, const ::std::vector<Unit*>& targets) = 0;
    virtual void HandleCrowdControlBreaks(Group* group, Unit* target) = 0;
    virtual void ManageGroupUtilities(Group* group, const DungeonEncounter& encounter) = 0;
    virtual void HandleSpecialAbilities(Group* group, uint32 encounterId) = 0;

    // Loot and rewards management
    virtual void HandleEncounterLoot(Group* group, uint32 encounterId) = 0;
    virtual void DistributeLoot(Group* group, const ::std::vector<uint32>& lootItems) = 0;
    virtual void HandleNeedGreedPass(Group* group, uint32 itemId, Player* player) = 0;
    virtual void OptimizeLootDistribution(Group* group) = 0;

    // Performance monitoring
    virtual DungeonMetrics GetGroupDungeonMetrics(uint32 groupId) = 0;
    virtual DungeonMetrics GetGlobalDungeonMetrics() = 0;

    // Dungeon-specific strategies
    virtual void LoadDungeonData() = 0;
    virtual DungeonData GetDungeonData(uint32 dungeonId) = 0;
    virtual DungeonEncounter GetEncounterData(uint32 encounterId) = 0;
    virtual void UpdateDungeonStrategy(Group* group, EncounterStrategy strategy) = 0;

    // Error handling and recovery
    virtual void HandleDungeonError(Group* group, const ::std::string& error) = 0;
    virtual void RecoverFromWipe(Group* group) = 0;
    virtual void HandlePlayerDisconnection(Group* group, Player* disconnectedPlayer) = 0;
    virtual void HandleGroupDisbandInDungeon(Group* group) = 0;

    // Configuration and settings
    virtual void SetEncounterStrategy(uint32 groupId, EncounterStrategy strategy) = 0;
    virtual EncounterStrategy GetEncounterStrategy(uint32 groupId) = 0;
    virtual void SetThreatManagement(uint32 groupId, ThreatManagement management) = 0;
    virtual void EnableAdaptiveBehavior(uint32 groupId, bool enable) = 0;

    // Update and maintenance
    virtual void Update(uint32 diff) = 0;
    virtual void UpdateGroupDungeon(Group* group, uint32 diff) = 0;
    virtual void CleanupInactiveDungeons() = 0;
};

} // namespace Playerbot
