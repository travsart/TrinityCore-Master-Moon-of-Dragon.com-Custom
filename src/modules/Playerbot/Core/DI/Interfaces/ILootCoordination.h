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
#include "Player.h"
#include "Group.h"
#include "LootMgr.h"
#include <vector>
#include <string>

namespace Playerbot
{

// Forward declarations
struct LootItem;
enum class LootRollType : uint8;

class TC_GAME_API ILootCoordination
{
public:
    virtual ~ILootCoordination() = default;

    // Core loot coordination workflow
    virtual void InitiateLootSession(Group* group, Loot* loot) = 0;
    virtual void ProcessLootSession(Group* group, uint32 lootSessionId) = 0;
    virtual void CompleteLootSession(uint32 lootSessionId) = 0;
    virtual void HandleLootSessionTimeout(uint32 lootSessionId) = 0;

    // Intelligent loot distribution orchestration
    virtual void OrchestrateLootDistribution(Group* group, const ::std::vector<LootItem>& items) = 0;
    virtual void PrioritizeLootDistribution(Group* group, ::std::vector<LootItem>& items) = 0;
    virtual void OptimizeLootSequence(Group* group, ::std::vector<LootItem>& items) = 0;
    virtual void HandleSimultaneousLooting(Group* group, const ::std::vector<LootItem>& items) = 0;

    // Group consensus and communication
    virtual void FacilitateGroupLootDiscussion(Group* group, const LootItem& item) = 0;
    virtual void HandleLootConflictResolution(Group* group, const LootItem& item) = 0;
    virtual void BroadcastLootRecommendations(Group* group, const LootItem& item) = 0;
    virtual void CoordinateGroupLootDecisions(Group* group, uint32 rollId) = 0;

    // Loot efficiency and optimization
    virtual void OptimizeLootEfficiency(Group* group) = 0;
    virtual void MinimizeLootTime(Group* group, uint32 sessionId) = 0;
    virtual void MaximizeLootFairness(Group* group, uint32 sessionId) = 0;
    virtual void BalanceLootSpeedAndFairness(Group* group, uint32 sessionId) = 0;

    // Conflict resolution and mediation
    virtual void MediateLootDispute(Group* group, const LootItem& item, const ::std::vector<uint32>& disputingPlayers) = 0;
    virtual void HandleLootGrievances(Group* group, uint32 complainingPlayer, const ::std::string& grievance) = 0;
    virtual void ResolveRollTies(Group* group, uint32 rollId) = 0;
    virtual void HandleLootNinja(Group* group, uint32 suspectedPlayer) = 0;

    // Configuration and customization
    virtual void SetCoordinationStyle(uint32 groupId, const ::std::string& style) = 0;
    virtual void SetConflictResolutionMethod(uint32 groupId, const ::std::string& method) = 0;
    virtual void EnableAdvancedCoordination(uint32 groupId, bool enable) = 0;
    virtual void SetLootCoordinationTimeout(uint32 groupId, uint32 timeoutMs) = 0;

    // Error handling and recovery
    virtual void HandleCoordinationError(uint32 sessionId, const ::std::string& error) = 0;
    virtual void RecoverFromCoordinationFailure(uint32 sessionId) = 0;
    virtual void HandleCorruptedLootState(uint32 sessionId) = 0;
    virtual void EmergencyLootDistribution(Group* group, const LootItem& item) = 0;

    // Update and maintenance
    virtual void Update(uint32 diff) = 0;
    virtual void UpdateLootSessions() = 0;
    virtual void CleanupExpiredSessions() = 0;
    virtual void ValidateCoordinationStates() = 0;
};

} // namespace Playerbot
