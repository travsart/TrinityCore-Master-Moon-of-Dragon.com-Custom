/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "HunterSpecialization.h"
#include "Player.h"
#include "Pet.h"
#include <unordered_map>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>

namespace Playerbot
{

enum class BeastMasteryPhase : uint8
{
    OPENING         = 0,  // Pet summoning and initial setup
    BURST_WINDOW    = 1,  // Bestial Wrath + cooldown stacking
    STEADY_ROTATION = 2,  // Standard DPS rotation
    PET_FOCUS      = 3,  // Pet-centric damage phase
    UTILITY_PHASE  = 4,  // Crowd control and utility
    EMERGENCY      = 5   // Low health or critical situations
};

enum class PetBehaviorMode : uint8
{
    AGGRESSIVE      = 0,  // Maximum DPS engagement
    DEFENSIVE       = 1,  // Protect hunter mode
    PASSIVE         = 2,  // No autonomous action
    ASSIST          = 3,  // Attack hunter's target only
    TANK_MODE       = 4,  // Pet tanking for group
    KITE_SUPPORT    = 5,  // Support hunter's kiting
    CROWD_CONTROL   = 6   // Focus on CC abilities
};

enum class PetFamily : uint8
{
    FEROCITY        = 0,  // DPS pets (Cat, Wolf, etc.)
    TENACITY        = 1,  // Tank pets (Bear, Turtle, etc.)
    CUNNING         = 2   // Utility pets (Spider, Serpent, etc.)
};

struct PetAnalytics
{
    uint32 damageDealt;
    uint32 damageTaken;
    uint32 healingReceived;
    uint32 abilitiesUsed;
    uint32 timeInCombat;
    float dpsContribution;
    float survivalRate;
    uint32 lastUpdateTime;

    PetAnalytics() : damageDealt(0), damageTaken(0), healingReceived(0)
        , abilitiesUsed(0), timeInCombat(0), dpsContribution(0.0f)
        , survivalRate(1.0f), lastUpdateTime(getMSTime()) {}
};

/**
 * @brief Enhanced Beast Mastery specialization with advanced pet management
 *
 * Focuses on maximizing pet potential through intelligent pet behavior,
 * advanced shot weaving, and burst window optimization.
 */
class TC_GAME_API BeastMasterySpecialization_Enhanced : public HunterSpecialization
{
public:
    explicit BeastMasterySpecialization_Enhanced(Player* bot);
    ~BeastMasterySpecialization_Enhanced() override = default;

    // Core rotation interface
    void UpdateRotation(::Unit* target) override;
    void UpdateBuffs() override;
    void UpdateCooldowns(uint32 diff) override;
    bool CanUseAbility(uint32 spellId) override;
    void OnCombatStart(::Unit* target) override;
    void OnCombatEnd() override;
    bool HasEnoughResource(uint32 spellId) override;
    void ConsumeResource(uint32 spellId) override;
    Position GetOptimalPosition(::Unit* target) override;
    float GetOptimalRange(::Unit* target) override;

    // Advanced pet management
    void UpdateAdvancedPetManagement();
    void OptimizePetBehavior(::Unit* target);
    void AnalyzePetPerformance();
    void HandlePetEmergencies();
    void CoordinatePetAndHunterActions(::Unit* target);

    // Burst window optimization
    void ExecuteBurstSequence(::Unit* target);
    void PrepareBurstWindow();
    bool IsBurstWindowActive() const;
    void OptimizeBurstTiming(::Unit* target);
    float CalculateBurstPotential(::Unit* target) const;

    // Advanced shot rotation
    void ExecuteOptimalShotRotation(::Unit* target);
    void WeaveAutoShotsOptimally(::Unit* target);
    void HandleSteadyShotClipping();
    void OptimizeGlobalCooldownUsage();

    // Pet behavior intelligence
    void SetPetBehaviorMode(PetBehaviorMode mode);
    void AdaptPetBehaviorToSituation(::Unit* target);
    void HandlePetPositioning(::Unit* target);
    void OptimizePetAbilityUsage(::Unit* target);

    // Pet family optimization
    PetFamily GetOptimalPetFamily(uint32 contentType) const;
    void RecommendPetSwitch();
    void AnalyzePetFamilyEffectiveness();

    // Multi-target management
    void HandleMultiTargetScenarios();
    void PrioritizeTargetsForPet();
    void CoordinateMultiTargetDPS();
    void OptimizePetTargetSwitching();

    // Crowd control and utility
    void HandleCrowdControlSituations();
    void UsePetUtilityAbilities();
    void ExecuteEmergencyTactics();
    void HandlePetRevive();

    // Performance analytics
    struct BeastMasteryMetrics
    {
        std::atomic<uint32> petDamageDealt{0};
        std::atomic<uint32> hunterDamageDealt{0};
        std::atomic<uint32> bestialWrathUsages{0};
        std::atomic<uint32> petDeaths{0};
        std::atomic<uint32> petRevives{0};
        std::atomic<float> petDpsContribution{0.6f};
        std::atomic<float> burstWindowEfficiency{0.8f};
        std::atomic<float> petSurvivalRate{0.9f};
        std::atomic<uint32> steadyShotsCast{0};
        std::atomic<uint32> autoShotsMissed{0};
        std::chrono::steady_clock::time_point lastUpdate;

        void Reset() {
            petDamageDealt = 0; hunterDamageDealt = 0; bestialWrathUsages = 0;
            petDeaths = 0; petRevives = 0; petDpsContribution = 0.6f;
            burstWindowEfficiency = 0.8f; petSurvivalRate = 0.9f;
            steadyShotsCast = 0; autoShotsMissed = 0;
            lastUpdate = std::chrono::steady_clock::now();
        }
    };

    BeastMasteryMetrics GetSpecializationMetrics() const { return _metrics; }

    // Talent optimization
    void AnalyzeTalentEffectiveness();
    void RecommendTalentChanges();
    void OptimizeForContent(uint32 contentType);

    // Advanced positioning
    void OptimizeHunterPositioning(::Unit* target);
    void CoordinateHunterPetPositioning(::Unit* target);
    void HandleDeadZoneOptimally(::Unit* target);
    void ExecuteKitingStrategy(::Unit* target);

private:
    // Enhanced rotation phases
    void ExecuteOpeningSequence(::Unit* target);
    void ExecuteBurstPhase(::Unit* target);
    void ExecuteSteadyPhase(::Unit* target);
    void ExecutePetFocusPhase(::Unit* target);
    void ExecuteUtilityPhase(::Unit* target);
    void ExecuteEmergencyPhase(::Unit* target);

    // Shot optimization
    bool ShouldUseSteadyShot(::Unit* target) const;
    bool ShouldUseArcaneShot(::Unit* target) const;
    bool ShouldUseMultiShot(::Unit* target) const;
    bool ShouldUseKillShot(::Unit* target) const;
    bool ShouldUseConcussiveShot(::Unit* target) const;
    bool ShouldUseSerpratingShot(::Unit* target) const;

    // Advanced shot execution
    void ExecuteSteadyShot(::Unit* target);
    void ExecuteArcaneShot(::Unit* target);
    void ExecuteMultiShot(::Unit* target);
    void ExecuteKillShot(::Unit* target);
    void ExecuteConcussiveShot(::Unit* target);
    void ExecuteSerpentSting(::Unit* target);

    // Cooldown management
    bool ShouldUseBestialWrath() const;
    bool ShouldUseIntimidation(::Unit* target) const;
    bool ShouldUseCallOfTheWild() const;
    bool ShouldUseSilencingShot(::Unit* target) const;
    bool ShouldUseMastersCall() const;

    // Advanced cooldown execution
    void ExecuteBestialWrath();
    void ExecuteIntimidation(::Unit* target);
    void ExecuteCallOfTheWild();
    void ExecuteSilencingShot(::Unit* target);
    void ExecuteMastersCall();

    // Pet command optimization
    void OptimizePetCommands(::Unit* target);
    void HandlePetSpecialAbilities(::Unit* target);
    void ManagePetResourcesOptimally();
    void UpdatePetThreatManagement();

    // Situational analysis
    void AnalyzeCombatSituation(::Unit* target);
    void AssessTargetThreat(::Unit* target);
    void EvaluateGroupDynamics();
    void DetermineOptimalStrategy(::Unit* target);

    // Performance optimization
    void OptimizeRotationTiming();
    void AnalyzeActionPriorities();
    void UpdateDamageMetrics();
    void TrackCooldownEfficiency();

    // Pet AI enhancement
    void EnhancePetAI();
    void UpdatePetDecisionMaking();
    void OptimizePetReactionTime();
    void HandlePetTargetSelection();

    // Resource management
    void OptimizeManaUsage();
    void ManagePetFocus();
    void HandleResourceStarvation();
    void PredictResourceNeeds();

    // Utility and emergency functions
    void HandleInterrupts(::Unit* target);
    void ExecuteEmergencyHealing();
    void HandleMovementRequirements();
    void ManageAggro();

    // State tracking
    BeastMasteryPhase _currentPhase;
    PetBehaviorMode _petBehaviorMode;
    PetFamily _currentPetFamily;

    // Timing and cooldowns
    uint32 _bestialWrathCooldown;
    uint32 _intimidationCooldown;
    uint32 _callOfTheWildCooldown;
    uint32 _silencingShotCooldown;
    uint32 _mastersCallCooldown;
    uint32 _lastSteadyShot;
    uint32 _lastAutoShot;
    uint32 _lastPetCommand;
    uint32 _burstWindowStart;
    uint32 _phaseTransitionTime;

    // Pet state tracking
    PetAnalytics _petAnalytics;
    uint32 _petLastDamage;
    uint32 _petLastHealth;
    uint32 _petLastPosition;
    bool _petInCombat;
    bool _petNeedsHealing;
    bool _petNeedsFeeding;
    float _petHappiness;
    uint32 _petReviveAttempts;

    // Combat analysis
    std::vector<uint32> _recentDamageEvents;
    uint32 _combatStartTime;
    uint32 _totalDamageDealt;
    uint32 _totalHealingDone;
    float _averageDps;
    uint32 _targetSwitches;
    uint32 _emergencyActions;

    // Multi-target tracking
    std::vector<ObjectGuid> _multiTargets;
    std::unordered_map<ObjectGuid, uint32> _targetPriorities;
    std::unordered_map<ObjectGuid, uint32> _targetThreatLevels;
    uint32 _primaryTargetGuid;

    // Positioning data
    Position _optimalPosition;
    Position _petOptimalPosition;
    bool _isKiting;
    bool _inDeadZone;
    uint32 _lastPositionUpdate;

    // Performance metrics
    BeastMasteryMetrics _metrics;
    mutable std::mutex _metricsMutex;

    // Configuration
    std::atomic<float> _petDpsWeight{0.6f};
    std::atomic<float> _burstThreshold{0.8f};
    std::atomic<uint32> _emergencyHealthThreshold{30};
    std::atomic<bool> _enableAdvancedPetAI{true};
    std::atomic<bool> _enableBurstOptimization{true};

    // Constants
    static constexpr uint32 BESTIAL_WRATH_DURATION = 18000; // 18 seconds
    static constexpr uint32 INTIMIDATION_COOLDOWN = 60000; // 1 minute
    static constexpr uint32 CALL_OF_THE_WILD_COOLDOWN = 300000; // 5 minutes
    static constexpr float OPTIMAL_PET_RANGE = 5.0f;
    static constexpr float DEAD_ZONE_MIN = 5.0f;
    static constexpr float DEAD_ZONE_MAX = 8.0f;
    static constexpr uint32 BURST_WINDOW_DURATION = 20000; // 20 seconds
    static constexpr float PET_HEALTH_EMERGENCY_THRESHOLD = 0.3f;
    static constexpr uint32 PET_COMMAND_COOLDOWN = 1500; // 1.5 seconds
    static constexpr uint32 STEADY_SHOT_CAST_TIME = 1500; // 1.5 seconds
    static constexpr float MULTI_TARGET_THRESHOLD = 3.0f;
    static constexpr uint32 PHASE_TRANSITION_COOLDOWN = 2000; // 2 seconds
};

} // namespace Playerbot