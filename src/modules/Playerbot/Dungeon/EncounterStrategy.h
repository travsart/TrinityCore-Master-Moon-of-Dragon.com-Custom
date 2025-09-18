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
#include "DungeonBehavior.h"
#include "Player.h"
#include "Group.h"
#include "Unit.h"
#include <unordered_map>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>

namespace Playerbot
{

/**
 * @brief Comprehensive encounter strategy system for dungeon boss fights
 *
 * This system provides detailed strategies for specific dungeon encounters,
 * including mechanics handling, positioning, and role-specific instructions.
 */
class TC_GAME_API EncounterStrategy
{
public:
    static EncounterStrategy* instance();

    // Core strategy management
    void ExecuteEncounterStrategy(Group* group, uint32 encounterId);
    void UpdateEncounterExecution(Group* group, uint32 encounterId, uint32 diff);
    void HandleEncounterMechanic(Group* group, uint32 encounterId, const std::string& mechanic);
    void AdaptStrategyToGroupComposition(Group* group, uint32 encounterId);

    // Phase-based encounter management
    void HandleEncounterPhaseTransition(Group* group, uint32 encounterId, uint32 newPhase);
    void ExecutePhaseStrategy(Group* group, uint32 encounterId, uint32 phase);
    void PrepareForPhaseTransition(Group* group, uint32 encounterId, uint32 upcomingPhase);

    // Mechanic-specific handlers
    void HandleTankSwapMechanic(Group* group, Player* currentTank, Player* newTank);
    void HandleStackingDebuffMechanic(Group* group, Player* affectedPlayer);
    void HandleAoEDamageMechanic(Group* group, const Position& dangerZone, float radius);
    void HandleAddSpawnMechanic(Group* group, const std::vector<Unit*>& adds);
    void HandleChanneledSpellMechanic(Group* group, Unit* caster, uint32 spellId);
    void HandleEnrageMechanic(Group* group, Unit* boss, uint32 timeRemaining);

    // Role-specific strategy execution
    struct TankStrategy
    {
        std::function<void(Player*, Group*, const DungeonEncounter&)> positioningStrategy;
        std::function<void(Player*, Group*, Unit*)> threatManagementStrategy;
        std::function<void(Player*, Group*, const std::string&)> mechanicResponseStrategy;
        std::function<void(Player*, Group*)> cooldownUsageStrategy;

        std::vector<uint32> priorityCooldowns;
        std::vector<std::string> keyMechanics;
        Position optimalPosition;
        float threatThreshold;
        bool requiresMovement;
    };

    struct HealerStrategy
    {
        std::function<void(Player*, Group*, const DungeonEncounter&)> healingPriorityStrategy;
        std::function<void(Player*, Group*)> manaManagementStrategy;
        std::function<void(Player*, Group*, const std::string&)> mechanicResponseStrategy;
        std::function<void(Player*, Group*)> dispelStrategy;

        std::vector<uint32> emergencyCooldowns;
        std::vector<uint32> dispelPriorities;
        Position safePosition;
        float healingThreshold;
        bool requiresMovement;
    };

    struct DpsStrategy
    {
        std::function<void(Player*, Group*, const std::vector<Unit*>&)> targetPriorityStrategy;
        std::function<void(Player*, Group*, const DungeonEncounter&)> damageOptimizationStrategy;
        std::function<void(Player*, Group*, const std::string&)> mechanicResponseStrategy;
        std::function<void(Player*, Group*)> cooldownRotationStrategy;

        std::vector<uint32> burstCooldowns;
        std::vector<uint32> targetPriorities;
        Position optimalPosition;
        float threatLimit;
        bool canMoveDuringCast;
    };

    TankStrategy GetTankStrategy(uint32 encounterId, Player* tank);
    HealerStrategy GetHealerStrategy(uint32 encounterId, Player* healer);
    DpsStrategy GetDpsStrategy(uint32 encounterId, Player* dps);

    // Positioning and movement strategies
    void UpdateEncounterPositioning(Group* group, uint32 encounterId);
    void HandleMovementMechanic(Group* group, uint32 encounterId, const std::string& mechanic);
    Position CalculateOptimalPosition(Player* player, uint32 encounterId, DungeonRole role);
    void AvoidMechanicAreas(Group* group, const std::vector<Position>& dangerAreas);

    // Cooldown and resource management
    void CoordinateGroupCooldowns(Group* group, uint32 encounterId);
    void PlanCooldownUsage(Group* group, const DungeonEncounter& encounter);
    void HandleEmergencyCooldowns(Group* group);
    void OptimizeResourceUsage(Group* group, uint32 encounterId);

    // Adaptive strategy system
    void AnalyzeEncounterPerformance(Group* group, uint32 encounterId);
    void AdaptStrategyBasedOnFailures(Group* group, uint32 encounterId);
    void LearnFromSuccessfulEncounters(Group* group, uint32 encounterId);
    void AdjustDifficultyRating(uint32 encounterId, float performanceRating);

    // Encounter-specific strategy implementations
    void ExecuteDeadminesStrategies(Group* group, uint32 encounterId);
    void ExecuteWailingCavernsStrategies(Group* group, uint32 encounterId);
    void ExecuteShadowfangKeepStrategies(Group* group, uint32 encounterId);
    void ExecuteStockadeStrategies(Group* group, uint32 encounterId);
    void ExecuteRazorfenKraulStrategies(Group* group, uint32 encounterId);

    // Performance monitoring
    struct StrategyMetrics
    {
        std::atomic<uint32> strategiesExecuted{0};
        std::atomic<uint32> strategiesSuccessful{0};
        std::atomic<uint32> mechanicsHandled{0};
        std::atomic<uint32> mechanicsSuccessful{0};
        std::atomic<float> averageExecutionTime{300000.0f}; // 5 minutes
        std::atomic<float> strategySuccessRate{0.85f};
        std::atomic<float> mechanicSuccessRate{0.9f};
        std::atomic<uint32> adaptationsPerformed{0};

        void Reset() {
            strategiesExecuted = 0; strategiesSuccessful = 0; mechanicsHandled = 0;
            mechanicsSuccessful = 0; averageExecutionTime = 300000.0f;
            strategySuccessRate = 0.85f; mechanicSuccessRate = 0.9f;
            adaptationsPerformed = 0;
        }
    };

    StrategyMetrics GetStrategyMetrics(uint32 encounterId);
    StrategyMetrics GetGlobalStrategyMetrics();

    // Configuration and settings
    void SetStrategyComplexity(uint32 encounterId, float complexity); // 0.0 = simple, 1.0 = complex
    void EnableAdaptiveStrategies(bool enable) { _adaptiveStrategiesEnabled = enable; }
    void SetMechanicResponseTime(uint32 responseTimeMs) { _mechanicResponseTime = responseTimeMs; }

private:
    EncounterStrategy();
    ~EncounterStrategy() = default;

    // Strategy database
    std::unordered_map<uint32, TankStrategy> _tankStrategies; // encounterId -> strategy
    std::unordered_map<uint32, HealerStrategy> _healerStrategies;
    std::unordered_map<uint32, DpsStrategy> _dpsStrategies;
    std::unordered_map<uint32, StrategyMetrics> _encounterMetrics;
    mutable std::mutex _strategyMutex;

    // Encounter mechanics database
    struct EncounterMechanic
    {
        std::string mechanicName;
        std::string description;
        uint32 triggerCondition;
        uint32 duration;
        float dangerLevel;
        std::vector<std::string> counterMeasures;
        std::function<void(Group*, const EncounterMechanic&)> handler;

        EncounterMechanic(const std::string& name, const std::string& desc)
            : mechanicName(name), description(desc), triggerCondition(0)
            , duration(0), dangerLevel(5.0f) {}
    };

    std::unordered_map<uint32, std::vector<EncounterMechanic>> _encounterMechanics; // encounterId -> mechanics

    // Adaptive learning system
    struct StrategyLearningData
    {
        std::unordered_map<uint32, uint32> mechanicFailures; // mechanicHash -> failure count
        std::unordered_map<uint32, uint32> mechanicSuccesses; // mechanicHash -> success count
        std::unordered_map<uint32, float> strategyEffectiveness; // strategyHash -> effectiveness
        uint32 totalEncountersAttempted;
        uint32 totalEncountersSuccessful;
        uint32 lastLearningUpdate;

        StrategyLearningData() : totalEncountersAttempted(0), totalEncountersSuccessful(0)
            , lastLearningUpdate(getMSTime()) {}
    };

    std::unordered_map<uint32, StrategyLearningData> _learningData; // encounterId -> learning data

    // Configuration
    std::atomic<bool> _adaptiveStrategiesEnabled{true};
    std::atomic<uint32> _mechanicResponseTime{2000}; // 2 seconds
    std::atomic<float> _strategyComplexity{0.7f}; // 70% complexity

    // Global metrics
    StrategyMetrics _globalMetrics;

    // Strategy initialization
    void InitializeStrategyDatabase();
    void LoadTankStrategies();
    void LoadHealerStrategies();
    void LoadDpsStrategies();
    void LoadEncounterMechanics();

    // Strategy execution helpers
    void ExecuteRoleStrategy(Player* player, uint32 encounterId, DungeonRole role);
    void HandleSpecificMechanic(Group* group, const EncounterMechanic& mechanic);
    void CoordinateGroupResponse(Group* group, const std::string& mechanic);
    void ValidateStrategyExecution(Group* group, uint32 encounterId);

    // Positioning algorithms
    Position CalculateTankPosition(uint32 encounterId, Group* group);
    Position CalculateHealerPosition(uint32 encounterId, Group* group);
    Position CalculateDpsPosition(uint32 encounterId, Group* group, bool isMelee);
    void UpdateGroupFormation(Group* group, uint32 encounterId);

    // Learning and adaptation
    void UpdateLearningData(uint32 encounterId, const std::string& mechanic, bool wasSuccessful);
    void AdaptStrategyComplexity(uint32 encounterId);
    void OptimizeStrategyBasedOnLearning(uint32 encounterId);
    uint32 GenerateMechanicHash(const std::string& mechanic);

    // Performance analysis
    void AnalyzeGroupPerformance(Group* group, uint32 encounterId);
    void IdentifyPerformanceBottlenecks(Group* group, uint32 encounterId);
    void RecommendStrategyAdjustments(Group* group, uint32 encounterId);

    // Constants
    static constexpr uint32 STRATEGY_UPDATE_INTERVAL = 1000; // 1 second
    static constexpr uint32 DEFAULT_MECHANIC_RESPONSE_TIME = 2000; // 2 seconds
    static constexpr float MECHANIC_SUCCESS_THRESHOLD = 0.8f; // 80% success rate
    static constexpr uint32 LEARNING_UPDATE_INTERVAL = 300000; // 5 minutes
    static constexpr float MIN_STRATEGY_EFFECTIVENESS = 0.3f;
    static constexpr uint32 MAX_STRATEGY_ADAPTATIONS = 10;
    static constexpr float POSITIONING_TOLERANCE = 3.0f; // 3 yards
    static constexpr uint32 COOLDOWN_COORDINATION_WINDOW = 5000; // 5 seconds
};

/**
 * ENCOUNTER STRATEGY EXAMPLES:
 *
 * DEADMINES - VANCLEEF:
 * - Phase 1: Tank positioning, add management
 * - Phase 2: Movement coordination, ground effects
 * - Mechanics: Add spawns, ground fire, knockback
 *
 * WAILING CAVERNS - MUTANUS:
 * - Single phase encounter
 * - Mechanics: Sleep effects, positioning
 * - Strategy: Dispel priority, movement coordination
 *
 * SHADOWFANG KEEP - ARUGAL:
 * - Phase-based encounter with teleports
 * - Mechanics: Add spawns, teleportation, magic damage
 * - Strategy: Add control, positioning adaptation
 *
 * STORMWIND STOCKADE - HOGGER:
 * - Simple tank and spank with fear mechanic
 * - Mechanics: Fear effects, enrage
 * - Strategy: Fear resistance, threat management
 */

} // namespace Playerbot