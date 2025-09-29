/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "DemonHunterSpecialization.h"
#include "Player.h"
#include <unordered_map>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>

namespace Playerbot
{

enum class VengeancePhase : uint8
{
    OPENING         = 0,  // Initial threat establishment
    PAIN_BUILDING   = 1,  // Building pain resources
    SOUL_CLEAVING   = 2,  // Soul Cleave spending phase
    SIGIL_CONTROL   = 3,  // Sigil management phase
    DEFENSIVE_BURST = 4,  // Emergency defensive phase
    METAMORPHOSIS   = 5,  // Metamorphosis tanking burst
    EMERGENCY       = 6   // Critical survival situations
};

enum class SoulFragmentState : uint8
{
    NONE            = 0,  // No soul fragments available
    ACCUMULATING    = 1,  // Building soul fragments
    OPTIMAL         = 2,  // Optimal fragment count
    CONSUMING       = 3,  // Actively consuming fragments
    EMERGENCY       = 4   // Emergency consumption needed
};

enum class SigilState : uint8
{
    READY           = 0,  // Sigils available for use
    PLANNING        = 1,  // Planning sigil placement
    ACTIVE          = 2,  // Sigils currently active
    COOLDOWN        = 3,  // Sigils on cooldown
    COORDINATING    = 4   // Coordinating multiple sigils
};

struct VengeanceTarget
{
    ObjectGuid targetGuid;
    bool hasFieryBrand;
    uint32 fieryBrandTimeRemaining;
    uint32 lastShearTime;
    uint32 lastInfernalStrikeTime;
    float threatLevel;
    bool isHighThreatTarget;
    uint32 painGeneratedFrom;
    bool isOptimalForSigils;
    uint32 lastTauntTime;

    VengeanceTarget() : hasFieryBrand(false), fieryBrandTimeRemaining(0)
        , lastShearTime(0), lastInfernalStrikeTime(0), threatLevel(0.0f)
        , isHighThreatTarget(false), painGeneratedFrom(0)
        , isOptimalForSigils(false), lastTauntTime(0) {}
};

/**
 * @brief Enhanced Vengeance specialization with advanced soul fragment mastery and sigil coordination
 *
 * Focuses on sophisticated pain management, soul fragment optimization,
 * and intelligent sigil placement for maximum tanking effectiveness and survivability.
 */
class TC_GAME_API VengeanceSpecialization_Enhanced : public DemonHunterSpecialization
{
public:
    explicit VengeanceSpecialization_Enhanced(Player* bot);
    ~VengeanceSpecialization_Enhanced() override = default;

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

    // Metamorphosis management
    void UpdateMetamorphosis() override;
    bool ShouldUseMetamorphosis() override;
    void TriggerMetamorphosis() override;
    MetamorphosisState GetMetamorphosisState() const override;

    // Soul fragment management
    void UpdateSoulFragments() override;
    void ConsumeSoulFragments() override;
    bool ShouldConsumeSoulFragments() override;
    uint32 GetAvailableSoulFragments() const override;

    // Specialization info
    DemonHunterSpec GetSpecialization() const override;
    const char* GetSpecializationName() const override;

    // Advanced pain mastery
    void ManagePainOptimally();
    void OptimizePainGeneration();
    void HandlePainSpendingEfficiency();
    void CoordinatePainResources();
    void MaximizePainUtilization();

    // Soul fragment mastery
    void ManageSoulFragmentsOptimally();
    void OptimizeSoulFragmentGeneration();
    void HandleSoulFragmentHealing();
    void CoordinateSoulFragmentConsumption();
    void MaximizeSoulFragmentEfficiency();

    // Sigil coordination mastery
    void ManageSigilsOptimally();
    void OptimizeSigilPlacement();
    void HandleSigilTiming();
    void CoordinateSigilEffects();
    void MaximizeSigilEffectiveness();

    // Threat management mastery
    void ManageThreatOptimally();
    void OptimizeThreatGeneration(::Unit* target);
    void HandleMultiTargetThreat();
    void CoordinateThreatRotation();
    void MaximizeThreatEfficiency();

    // Defensive cooldown mastery
    void ManageDefensiveCooldownsOptimally();
    void OptimizeDefensiveTiming();
    void HandleEmergencyDefensives();
    void CoordinateDefensiveRotation();
    void MaximizeDefensiveValue();

    // Performance analytics
    struct VengeanceMetrics
    {
        std::atomic<uint32> shearCasts{0};
        std::atomic<uint32> soulCleaveCasts{0};
        std::atomic<uint32> demonSpikesCasts{0};
        std::atomic<uint32> fieryBrandCasts{0};
        std::atomic<uint32> sigilsCast{0};
        std::atomic<uint32> infernalStrikeCasts{0};
        std::atomic<uint32> metamorphosisActivations{0};
        std::atomic<uint32> soulFragmentsGenerated{0};
        std::atomic<uint32> soulFragmentsConsumed{0};
        std::atomic<float> painEfficiency{0.85f};
        std::atomic<float> threatControlEfficiency{0.95f};
        std::atomic<float> soulFragmentEfficiency{0.9f};
        std::atomic<float> sigilEffectiveness{0.8f};
        std::atomic<float> damageReductionPercentage{0.45f};
        std::atomic<uint32> emergencyDefensivesUsed{0};
        std::chrono::steady_clock::time_point lastUpdate;

        void Reset() {
            shearCasts = 0; soulCleaveCasts = 0; demonSpikesCasts = 0;
            fieryBrandCasts = 0; sigilsCast = 0; infernalStrikeCasts = 0;
            metamorphosisActivations = 0; soulFragmentsGenerated = 0; soulFragmentsConsumed = 0;
            painEfficiency = 0.85f; threatControlEfficiency = 0.95f; soulFragmentEfficiency = 0.9f;
            sigilEffectiveness = 0.8f; damageReductionPercentage = 0.45f; emergencyDefensivesUsed = 0;
            lastUpdate = std::chrono::steady_clock::now();
        }

        // Copy constructor
        VengeanceMetrics(const VengeanceMetrics& other)
            : shearCasts(other.shearCasts.load())
            , soulCleaveCasts(other.soulCleaveCasts.load())
            , demonSpikesCasts(other.demonSpikesCasts.load())
            , fieryBrandCasts(other.fieryBrandCasts.load())
            , sigilsCast(other.sigilsCast.load())
            , infernalStrikeCasts(other.infernalStrikeCasts.load())
            , metamorphosisActivations(other.metamorphosisActivations.load())
            , soulFragmentsGenerated(other.soulFragmentsGenerated.load())
            , soulFragmentsConsumed(other.soulFragmentsConsumed.load())
            , painEfficiency(other.painEfficiency.load())
            , threatControlEfficiency(other.threatControlEfficiency.load())
            , soulFragmentEfficiency(other.soulFragmentEfficiency.load())
            , sigilEffectiveness(other.sigilEffectiveness.load())
            , damageReductionPercentage(other.damageReductionPercentage.load())
            , emergencyDefensivesUsed(other.emergencyDefensivesUsed.load())
            , lastUpdate(other.lastUpdate)
        {}

        // Move constructor
        VengeanceMetrics(VengeanceMetrics&& other) noexcept
            : shearCasts(other.shearCasts.load())
            , soulCleaveCasts(other.soulCleaveCasts.load())
            , demonSpikesCasts(other.demonSpikesCasts.load())
            , fieryBrandCasts(other.fieryBrandCasts.load())
            , sigilsCast(other.sigilsCast.load())
            , infernalStrikeCasts(other.infernalStrikeCasts.load())
            , metamorphosisActivations(other.metamorphosisActivations.load())
            , soulFragmentsGenerated(other.soulFragmentsGenerated.load())
            , soulFragmentsConsumed(other.soulFragmentsConsumed.load())
            , painEfficiency(other.painEfficiency.load())
            , threatControlEfficiency(other.threatControlEfficiency.load())
            , soulFragmentEfficiency(other.soulFragmentEfficiency.load())
            , sigilEffectiveness(other.sigilEffectiveness.load())
            , damageReductionPercentage(other.damageReductionPercentage.load())
            , emergencyDefensivesUsed(other.emergencyDefensivesUsed.load())
            , lastUpdate(std::move(other.lastUpdate))
        {}

        // Copy assignment operator
        VengeanceMetrics& operator=(const VengeanceMetrics& other) {
            if (this != &other) {
                shearCasts = other.shearCasts.load();
                soulCleaveCasts = other.soulCleaveCasts.load();
                demonSpikesCasts = other.demonSpikesCasts.load();
                fieryBrandCasts = other.fieryBrandCasts.load();
                sigilsCast = other.sigilsCast.load();
                infernalStrikeCasts = other.infernalStrikeCasts.load();
                metamorphosisActivations = other.metamorphosisActivations.load();
                soulFragmentsGenerated = other.soulFragmentsGenerated.load();
                soulFragmentsConsumed = other.soulFragmentsConsumed.load();
                painEfficiency = other.painEfficiency.load();
                threatControlEfficiency = other.threatControlEfficiency.load();
                soulFragmentEfficiency = other.soulFragmentEfficiency.load();
                sigilEffectiveness = other.sigilEffectiveness.load();
                damageReductionPercentage = other.damageReductionPercentage.load();
                emergencyDefensivesUsed = other.emergencyDefensivesUsed.load();
                lastUpdate = other.lastUpdate;
            }
            return *this;
        }

        // Move assignment operator
        VengeanceMetrics& operator=(VengeanceMetrics&& other) noexcept {
            if (this != &other) {
                shearCasts = other.shearCasts.load();
                soulCleaveCasts = other.soulCleaveCasts.load();
                demonSpikesCasts = other.demonSpikesCasts.load();
                fieryBrandCasts = other.fieryBrandCasts.load();
                sigilsCast = other.sigilsCast.load();
                infernalStrikeCasts = other.infernalStrikeCasts.load();
                metamorphosisActivations = other.metamorphosisActivations.load();
                soulFragmentsGenerated = other.soulFragmentsGenerated.load();
                soulFragmentsConsumed = other.soulFragmentsConsumed.load();
                painEfficiency = other.painEfficiency.load();
                threatControlEfficiency = other.threatControlEfficiency.load();
                soulFragmentEfficiency = other.soulFragmentEfficiency.load();
                sigilEffectiveness = other.sigilEffectiveness.load();
                damageReductionPercentage = other.damageReductionPercentage.load();
                emergencyDefensivesUsed = other.emergencyDefensivesUsed.load();
                lastUpdate = std::move(other.lastUpdate);
            }
            return *this;
        }
    };

    VengeanceMetrics GetSpecializationMetrics() const { return _metrics; }

    // Immolation Aura optimization
    void ManageImmolationAuraOptimally();
    void OptimizeImmolationAuraTiming();
    void HandleImmolationAuraPositioning();
    void CoordinateImmolationAuraWithRotation();

    // Demon Spikes optimization
    void ManageDemonSpikesOptimally();
    void OptimizeDemonSpikesCharges();
    void HandleDemonSpikesEfficiency();
    void CoordinateDemonSpikesWithDamage();

    // Fiery Brand mastery
    void ManageFieryBrandOptimally();
    void OptimizeFieryBrandTargeting();
    void HandleFieryBrandSpreading();
    void CoordinateFieryBrandWithDefensives();

private:
    // Enhanced rotation phases
    void ExecuteOpeningSequence(::Unit* target);
    void ExecutePainBuildingPhase(::Unit* target);
    void ExecuteSoulCleavingPhase(::Unit* target);
    void ExecuteSigilControlPhase(::Unit* target);
    void ExecuteDefensiveBurstPhase(::Unit* target);
    void ExecuteMetamorphosisPhase(::Unit* target);
    void ExecuteEmergencyPhase(::Unit* target);

    // Spell execution optimization
    bool ShouldCastShear(::Unit* target) const;
    bool ShouldCastSoulCleave() const;
    bool ShouldCastDemonSpikes() const;
    bool ShouldCastFieryBrand(::Unit* target) const;
    bool ShouldCastSigil(uint32 sigilType) const;

    // Advanced spell execution
    void ExecuteShear(::Unit* target);
    void ExecuteSoulCleave();
    void ExecuteDemonSpikes();
    void ExecuteFieryBrand(::Unit* target);
    void ExecuteSigil(uint32 sigilType, Position targetPos);

    // Cooldown management
    bool ShouldUseSoulBarrier() const;
    bool ShouldUseInfernalStrike(::Unit* target) const;
    bool ShouldUseImmolationAura() const;

    // Advanced cooldown execution
    void ExecuteMetamorphosis();
    void ExecuteSoulBarrier();
    void ExecuteInfernalStrike(::Unit* target);
    void ExecuteImmolationAura();

    // Pain management implementations
    void UpdatePainTracking();
    void OptimizePainSpending();
    void HandlePainGeneration();
    void CalculateOptimalPainUsage();

    // Soul fragment implementations
    void UpdateSoulFragmentTracking();
    void GenerateSoulFragments();
    void ConsumeSoulFragmentsForHealing();
    void OptimizeSoulFragmentPositioning();

    // Sigil implementations
    void UpdateSigilTracking();
    void PlaceSigilOptimally(uint32 sigilType, ::Unit* target);
    void CoordinateMultipleSigils();
    void OptimizeSigilTiming();

    // Threat implementations
    void UpdateThreatTracking();
    void OptimizeThreatRotation(::Unit* target);
    void HandleThreatEmergency();
    void CalculateThreatPriorities();

    // Defensive implementations
    void UpdateDefensiveTracking();
    void OptimizeDefensiveSequence();
    void HandleDefensiveEmergencies();
    void CoordinateDefensiveCooldowns();

    // Target analysis for vengeance
    void AnalyzeTargetForVengeance(::Unit* target);
    void AssessThreatRequirements(::Unit* target);
    void PredictIncomingDamage(::Unit* target);
    void OptimizeTargetRotation(::Unit* target);

    // Multi-target tanking
    void HandleMultiTargetVengeance();
    void OptimizeAoEThreatGeneration();
    void CoordinateMultiTargetDefenses();
    void ManageAoESoulFragments();

    // Position optimization
    void OptimizeVengeancePositioning(::Unit* target);
    void MaintainOptimalTankPosition(::Unit* target);
    void HandlePositionalSigils();
    void ExecuteTankingRepositioning();

    // Performance tracking
    void TrackVengeancePerformance();
    void AnalyzePainEfficiency();
    void UpdateThreatMetrics();
    void OptimizeBasedOnVengeanceMetrics();

    // Emergency handling
    void HandleLowHealthVengeanceEmergency();
    void HandleHighDamageEmergency();
    void ExecuteEmergencyDefensives();
    void HandleThreatLossEmergency();

    // State tracking
    VengeancePhase _currentPhase;
    SoulFragmentState _soulFragmentState;
    SigilState _sigilState;

    // Target tracking
    std::unordered_map<ObjectGuid, VengeanceTarget> _vengeanceTargets;
    ObjectGuid _primaryThreatTarget;
    std::vector<ObjectGuid> _aoeTargets;

    // Pain tracking
    uint32 _currentPain;
    uint32 _painGenerated;
    uint32 _painSpent;
    float _painEfficiencyRatio;

    // Soul fragment tracking
    uint32 _availableSoulFragments;
    uint32 _soulFragmentsGenerated;
    uint32 _soulFragmentsConsumed;
    uint32 _lastSoulFragmentGeneration;

    // Sigil tracking
    uint32 _lastSigilOfFlameTime;
    uint32 _lastSigilOfSilenceTime;
    uint32 _lastSigilOfMiseryTime;
    uint32 _lastSigilOfChainsTime;
    std::unordered_map<uint32, uint32> _sigilCooldowns;

    // Threat tracking
    uint32 _currentThreatLevel;
    uint32 _lastThreatCheck;
    uint32 _threatGenerationRate;
    bool _hasSufficientThreat;

    // Defensive tracking
    uint32 _demonSpikesCharges;
    uint32 _lastDemonSpikesTime;
    uint32 _lastFieryBrandTime;
    uint32 _lastSoulBarrierTime;
    uint32 _defensiveCooldownsActive;

    // Metamorphosis tracking
    uint32 _metamorphosisTimeRemaining;
    uint32 _lastMetamorphosisActivation;
    bool _metamorphosisActive;
    uint32 _metamorphosisCooldown;

    // Immolation Aura tracking
    uint32 _lastImmolationAuraTime;
    bool _immolationAuraActive;
    uint32 _immolationAuraTimeRemaining;

    // Combat analysis
    uint32 _combatStartTime;
    uint32 _totalVengeanceDamage;
    uint32 _totalDamageMitigated;
    uint32 _totalThreatGenerated;
    float _averageVengeanceDps;

    // Performance metrics
    VengeanceMetrics _metrics;
    mutable std::mutex _metricsMutex;

    // Configuration
    std::atomic<float> _painEfficiencyThreshold{0.8f};
    std::atomic<uint32> _optimalSoulFragments{5};
    std::atomic<float> _threatMarginTarget{1.5f};
    std::atomic<bool> _enableAdvancedSigils{true};
    std::atomic<bool> _enableOptimalSoulFragments{true};

    // Constants
    static constexpr uint32 MAX_PAIN = 100;
    static constexpr uint32 SOUL_FRAGMENT_HEAL_AMOUNT = 6; // 6% max health
    static constexpr uint32 DEMON_SPIKES_DURATION = 6000; // 6 seconds
    static constexpr uint32 DEMON_SPIKES_COOLDOWN = 20000; // 20 seconds
    static constexpr uint32 FIERY_BRAND_DURATION = 8000; // 8 seconds
    static constexpr uint32 FIERY_BRAND_COOLDOWN = 30000; // 30 seconds
    static constexpr uint32 SOUL_BARRIER_DURATION = 12000; // 12 seconds
    static constexpr uint32 SOUL_BARRIER_COOLDOWN = 30000; // 30 seconds
    static constexpr uint32 SIGIL_DELAY = 2000; // 2 seconds activation delay
    static constexpr uint32 SIGIL_COOLDOWN = 30000; // 30 seconds
    static constexpr uint32 METAMORPHOSIS_DURATION = 15000; // 15 seconds
    static constexpr uint32 IMMOLATION_AURA_DURATION = 6000; // 6 seconds
    static constexpr float EMERGENCY_HEALTH_THRESHOLD = 0.25f; // 25% health
    static constexpr uint8 OPTIMAL_PAIN_FOR_SOUL_CLEAVE = 30;
    static constexpr uint8 OPTIMAL_SOUL_FRAGMENTS_FOR_CONSUMPTION = 5;
    static constexpr float OPTIMAL_VENGEANCE_RANGE = 8.0f;
};

} // namespace Playerbot