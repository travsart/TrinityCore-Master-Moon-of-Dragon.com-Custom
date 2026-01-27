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
#include <functional>

class Group;
class Player;
class Unit;

namespace Playerbot
{

// Forward declarations
struct DungeonEncounter;
enum class DungeonRole : uint8;

/**
 * @brief Role-specific strategy for tanks
 */
struct TankStrategy
{
    ::std::function<void(Player*, Group*, const DungeonEncounter&)> positioningStrategy;
    ::std::function<void(Player*, Group*, Unit*)> threatManagementStrategy;
    ::std::function<void(Player*, Group*, const ::std::string&)> mechanicResponseStrategy;
    ::std::function<void(Player*, Group*)> cooldownUsageStrategy;

    ::std::vector<uint32> priorityCooldowns;
    ::std::vector<::std::string> keyMechanics;
    Position optimalPosition;
    float threatThreshold = 1.1f;
    bool requiresMovement = false;
};

/**
 * @brief Role-specific strategy for healers
 */
struct HealerStrategy
{
    ::std::function<void(Player*, Group*, const DungeonEncounter&)> healingPriorityStrategy;
    ::std::function<void(Player*, Group*)> manaManagementStrategy;
    ::std::function<void(Player*, Group*, const ::std::string&)> mechanicResponseStrategy;
    ::std::function<void(Player*, Group*)> dispelStrategy;

    ::std::vector<uint32> emergencyCooldowns;
    ::std::vector<uint32> dispelPriorities;
    Position safePosition;
    float healingThreshold = 0.7f;
    bool requiresMovement = false;
};

/**
 * @brief Role-specific strategy for DPS
 */
struct DpsStrategy
{
    ::std::function<void(Player*, Group*, const ::std::vector<Unit*>&)> targetPriorityStrategy;
    ::std::function<void(Player*, Group*, const DungeonEncounter&)> damageOptimizationStrategy;
    ::std::function<void(Player*, Group*, const ::std::string&)> mechanicResponseStrategy;
    ::std::function<void(Player*, Group*)> cooldownRotationStrategy;

    ::std::vector<uint32> burstCooldowns;
    ::std::vector<uint32> targetPriorities;
    Position optimalPosition;
    float threatLimit = 0.9f;
    bool canMoveDuringCast = false;
};

/**
 * @brief Strategy execution metrics (snapshot - non-atomic for copyability)
 *
 * This is a copyable snapshot of metrics. The internal implementation may use
 * atomic storage but returns this copyable snapshot for external consumers.
 */
struct StrategyMetrics
{
    uint32 strategiesExecuted = 0;
    uint32 strategiesSuccessful = 0;
    uint32 mechanicsHandled = 0;
    uint32 mechanicsSuccessful = 0;
    float averageExecutionTime = 300000.0f;  // 5 minutes
    float strategySuccessRate = 0.85f;
    float mechanicSuccessRate = 0.9f;
    uint32 adaptationsPerformed = 0;

    void Reset()
    {
        strategiesExecuted = 0;
        strategiesSuccessful = 0;
        mechanicsHandled = 0;
        mechanicsSuccessful = 0;
        averageExecutionTime = 300000.0f;
        strategySuccessRate = 0.85f;
        mechanicSuccessRate = 0.9f;
        adaptationsPerformed = 0;
    }
};

/**
 * @brief Interface for encounter strategy management in dungeons
 *
 * Provides comprehensive encounter strategy execution, phase management,
 * mechanic handling, and role-specific strategies for dungeon bosses.
 */
class TC_GAME_API IEncounterStrategy
{
public:
    virtual ~IEncounterStrategy() = default;

    // Core strategy management
    virtual void ExecuteEncounterStrategy(Group* group, uint32 encounterId) = 0;
    virtual void UpdateEncounterExecution(Group* group, uint32 encounterId, uint32 diff) = 0;
    virtual void HandleEncounterMechanic(Group* group, uint32 encounterId, const ::std::string& mechanic) = 0;
    virtual void AdaptStrategyToGroupComposition(Group* group, uint32 encounterId) = 0;

    // Phase-based encounter management
    virtual void HandleEncounterPhaseTransition(Group* group, uint32 encounterId, uint32 newPhase) = 0;
    virtual void ExecutePhaseStrategy(Group* group, uint32 encounterId, uint32 phase) = 0;
    virtual void PrepareForPhaseTransition(Group* group, uint32 encounterId, uint32 upcomingPhase) = 0;

    // Mechanic-specific handlers
    virtual void HandleTankSwapMechanic(Group* group, Player* currentTank, Player* newTank) = 0;
    virtual void HandleStackingDebuffMechanic(Group* group, Player* affectedPlayer) = 0;
    virtual void HandleAoEDamageMechanic(Group* group, const Position& dangerZone, float radius) = 0;
    virtual void HandleAddSpawnMechanic(Group* group, const ::std::vector<Unit*>& adds) = 0;
    virtual void HandleChanneledSpellMechanic(Group* group, Unit* caster, uint32 spellId) = 0;
    virtual void HandleEnrageMechanic(Group* group, Unit* boss, uint32 timeRemaining) = 0;

    // Role-specific strategy getters
    virtual TankStrategy GetTankStrategy(uint32 encounterId, Player* tank) = 0;
    virtual HealerStrategy GetHealerStrategy(uint32 encounterId, Player* healer) = 0;
    virtual DpsStrategy GetDpsStrategy(uint32 encounterId, Player* dps) = 0;

    // Positioning and movement strategies
    virtual void UpdateEncounterPositioning(Group* group, uint32 encounterId) = 0;
    virtual void HandleMovementMechanic(Group* group, uint32 encounterId, const ::std::string& mechanic) = 0;
    virtual Position CalculateOptimalPosition(Player* player, uint32 encounterId, DungeonRole role) = 0;
    virtual void AvoidMechanicAreas(Group* group, const ::std::vector<Position>& dangerAreas) = 0;

    // Cooldown and resource management
    virtual void CoordinateGroupCooldowns(Group* group, uint32 encounterId) = 0;
    virtual void PlanCooldownUsage(Group* group, const DungeonEncounter& encounter) = 0;
    virtual void HandleEmergencyCooldowns(Group* group) = 0;
    virtual void OptimizeResourceUsage(Group* group, uint32 encounterId) = 0;

    // Adaptive strategy system
    virtual void AnalyzeEncounterPerformance(Group* group, uint32 encounterId) = 0;
    virtual void AdaptStrategyBasedOnFailures(Group* group, uint32 encounterId) = 0;
    virtual void LearnFromSuccessfulEncounters(Group* group, uint32 encounterId) = 0;
    virtual void AdjustDifficultyRating(uint32 encounterId, float performanceRating) = 0;

    // Encounter-specific strategy implementations
    virtual void ExecuteDeadminesStrategies(Group* group, uint32 encounterId) = 0;
    virtual void ExecuteWailingCavernsStrategies(Group* group, uint32 encounterId) = 0;
    virtual void ExecuteShadowfangKeepStrategies(Group* group, uint32 encounterId) = 0;
    virtual void ExecuteStockadeStrategies(Group* group, uint32 encounterId) = 0;
    virtual void ExecuteRazorfenKraulStrategies(Group* group, uint32 encounterId) = 0;

    // Performance monitoring
    virtual StrategyMetrics GetStrategyMetrics(uint32 encounterId) = 0;
    virtual StrategyMetrics GetGlobalStrategyMetrics() = 0;

    // Configuration and settings
    virtual void SetStrategyComplexity(uint32 encounterId, float complexity) = 0;
    virtual void EnableAdaptiveStrategies(bool enable) = 0;
    virtual void SetMechanicResponseTime(uint32 responseTimeMs) = 0;
};

} // namespace Playerbot
