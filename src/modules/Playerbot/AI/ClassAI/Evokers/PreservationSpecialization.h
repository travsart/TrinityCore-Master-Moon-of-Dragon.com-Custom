/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef PRESERVATION_SPECIALIZATION_H
#define PRESERVATION_SPECIALIZATION_H

#include "EvokerSpecialization.h"
#include <vector>
#include <unordered_set>
#include <queue>

namespace Playerbot
{

enum class PreservationRotationPhase : uint8
{
    ASSESSMENT = 0,
    EMERGENCY_HEALING = 1,
    ECHO_MANAGEMENT = 2,
    EMPOWERED_HEALING = 3,
    SUSTAIN_HEALING = 4,
    TEMPORAL_ABILITIES = 5,
    DREAM_FLIGHT_SETUP = 6,
    GROUP_HEALING = 7,
    DAMAGE_CONTRIBUTION = 8,
    RESOURCE_RECOVERY = 9
};

enum class HealingPriority : uint8
{
    EMERGENCY_HEAL = 0,
    ECHO_REFRESH = 1,
    EMPOWERED_HEALS = 2,
    GROUP_HEALING = 3,
    SUSTAIN_HEALING = 4,
    TEMPORAL_ABILITIES = 5,
    DAMAGE_CONTRIBUTION = 6,
    UTILITY = 7
};

struct EchoTracker
{
    ::Unit* target;
    uint32 remainingHeals;
    uint32 healAmount;
    uint32 lastHeal;
    uint32 creationTime;
    bool isActive;

    EchoTracker() : target(nullptr), remainingHeals(0), healAmount(0), lastHeal(0), creationTime(0), isActive(false) {}
    EchoTracker(::Unit* tgt, uint32 heals, uint32 amount) : target(tgt), remainingHeals(heals), healAmount(amount),
                                                            lastHeal(getMSTime()), creationTime(getMSTime()), isActive(true) {}
};

struct TemporalInfo
{
    uint8 compressionStacks;
    uint32 compressionTimeRemaining;
    uint32 lastTemporalAnomaly;
    bool anomalyActive;
    uint32 stasisTargets;
    uint32 lastStasis;

    TemporalInfo() : compressionStacks(0), compressionTimeRemaining(0), lastTemporalAnomaly(0),
                    anomalyActive(false), stasisTargets(0), lastStasis(0) {}
};

struct CallOfYseraInfo
{
    bool isActive;
    uint8 stacks;
    uint32 timeRemaining;
    uint32 lastProc;

    CallOfYseraInfo() : isActive(false), stacks(0), timeRemaining(0), lastProc(0) {}
};

struct PreservationMetrics
{
    uint32 emeraldBlossomCasts;
    uint32 verdantEmbraceCasts;
    uint32 dreamBreathCasts;
    uint32 spiritBloomCasts;
    uint32 temporalAnomalyCasts;
    uint32 renewingBlazeCasts;
    uint32 echoesCreated;
    uint32 echoHealsPerformed;
    uint32 dreamFlightActivations;
    uint32 reversion Casts;
    uint32 totalHealingDone;
    uint32 overhealing;
    float echoUptime;
    float temporalCompressionUptime;
    float callOfYseraUptime;
    float averageHealingPerSecond;
    float healingEfficiency;

    PreservationMetrics() : emeraldBlossomCasts(0), verdantEmbraceCasts(0), dreamBreathCasts(0), spiritBloomCasts(0),
                          temporalAnomalyCasts(0), renewingBlazeCasts(0), echoesCreated(0), echoHealsPerformed(0),
                          dreamFlightActivations(0), reversionCasts(0), totalHealingDone(0), overhealing(0),
                          echoUptime(0.0f), temporalCompressionUptime(0.0f), callOfYseraUptime(0.0f),
                          averageHealingPerSecond(0.0f), healingEfficiency(0.0f) {}
};

class PreservationSpecialization : public EvokerSpecialization
{
public:
    explicit PreservationSpecialization(Player* bot);
    ~PreservationSpecialization() override = default;

    // Core Interface Implementation
    void UpdateRotation(::Unit* target) override;
    void UpdateBuffs() override;
    void UpdateCooldowns(uint32 diff) override;
    bool CanUseAbility(uint32 spellId) override;
    void OnCombatStart(::Unit* target) override;
    void OnCombatEnd() override;

    // Resource Management Implementation
    bool HasEnoughResource(uint32 spellId) override;
    void ConsumeResource(uint32 spellId) override;

    // Positioning Implementation
    Position GetOptimalPosition(::Unit* target) override;
    float GetOptimalRange(::Unit* target) override;

    // Essence Management Implementation
    void UpdateEssenceManagement() override;
    bool HasEssence(uint32 required = 1) override;
    uint32 GetEssence() override;
    void SpendEssence(uint32 amount) override;
    void GenerateEssence(uint32 amount) override;
    bool ShouldConserveEssence() override;

    // Empowerment Management Implementation
    void UpdateEmpowermentSystem() override;
    void StartEmpoweredSpell(uint32 spellId, EmpowermentLevel targetLevel, ::Unit* target) override;
    void UpdateEmpoweredChanneling() override;
    void ReleaseEmpoweredSpell() override;
    EmpowermentLevel CalculateOptimalEmpowermentLevel(uint32 spellId, ::Unit* target) override;
    bool ShouldEmpowerSpell(uint32 spellId) override;

    // Aspect Management Implementation
    void UpdateAspectManagement() override;
    void ShiftToAspect(EvokerAspect aspect) override;
    EvokerAspect GetOptimalAspect() override;
    bool CanShiftAspect() override;

    // Combat Phase Management Implementation
    void UpdateCombatPhase() override;
    CombatPhase GetCurrentPhase() override;
    bool ShouldExecuteBurstRotation() override;

    // Target Selection Implementation
    ::Unit* GetBestTarget() override;
    std::vector<::Unit*> GetEmpoweredSpellTargets(uint32 spellId) override;

private:
    // Preservation-specific systems
    void UpdateEchoManagement();
    void UpdateTemporalManagement();
    void UpdateCallOfYseraTracking();
    void UpdateHealingPriorities();
    void UpdateGroupHealthAssessment();

    // Rotation phases
    void ExecuteAssessmentPhase(::Unit* target);
    void ExecuteEmergencyHealing(::Unit* target);
    void ExecuteEchoManagement(::Unit* target);
    void ExecuteEmpoweredHealing(::Unit* target);
    void ExecuteSustainHealing(::Unit* target);
    void ExecuteTemporalAbilities(::Unit* target);
    void ExecuteDreamFlightSetup(::Unit* target);
    void ExecuteGroupHealing(::Unit* target);
    void ExecuteDamageContribution(::Unit* target);
    void ExecuteResourceRecovery(::Unit* target);

    // Core healing abilities
    void CastEmeraldBlossom();
    void CastVerdantEmbrace(::Unit* target);
    void CastReversion(::Unit* target);
    void CastRenewingBlaze(::Unit* target);
    void CastLifebind(::Unit* target);

    // Empowered healing abilities
    void CastEmpoweredDreamBreath(::Unit* target, EmpowermentLevel level);
    void CastEmpoweredSpiritBloom(::Unit* target, EmpowermentLevel level);

    // Temporal abilities
    void CastTemporalAnomaly();
    void CastStasis(::Unit* target);
    void CastTimeDilation(::Unit* target);

    // Major cooldowns
    void CastDreamFlight();
    void CastFieldOfDreams();

    // Echo system management
    void CreateEcho(::Unit* target, uint32 healAmount, uint32 numHeals = 3);
    void ProcessEchoHealing();
    void RemoveExpiredEchoes();
    void OptimizeEchoPlacement();
    bool ShouldCreateEcho(::Unit* target);
    uint32 GetActiveEchoCount();
    ::Unit* GetBestEchoTarget();

    // Healing target selection
    ::Unit* GetBestHealTarget();
    ::Unit* GetMostInjuredAlly();
    ::Unit* GetBestGroupHealTarget();
    std::vector<::Unit*> GetAlliesNeedingHealing(float healthThreshold = 0.8f);
    std::vector<::Unit*> GetGroupHealTargets(::Unit* center, float range = 30.0f);

    // Healing optimization
    void OptimizeHealingRotation();
    void PrioritizeHealingTargets();
    bool ShouldUseGroupHealing();
    bool ShouldUseEmergencyHealing();
    uint32 CalculateHealingNeeded(::Unit* target);
    float CalculateHealingEfficiency(uint32 spellId, ::Unit* target);

    // Temporal magic optimization
    void OptimizeTemporalAbilities();
    bool ShouldUseTemporalAnomaly();
    bool ShouldUseStasis(::Unit* target);
    ::Unit* GetBestStasisTarget();
    void UpdateTemporalCompression();

    // Damage contribution
    void ContributeDamage(::Unit* target);
    bool ShouldContributeDamage();
    ::Unit* GetBestDamageTarget();

    // Positioning and range management
    void OptimizeHealerPositioning();
    bool IsAtOptimalHealingRange();
    void MaintainGroupProximity();

    // Resource management
    void OptimizeEssenceForHealing();
    void PlanHealingRotation();
    bool ShouldConserveForEmergency();

    // Defensive abilities
    void HandleHealerDefense();
    void UseHealerEmergencyAbilities();

    // Metrics and analysis
    void UpdatePreservationMetrics();
    void AnalyzeHealingEfficiency();
    void LogPreservationDecision(const std::string& decision, const std::string& reason);

    // State variables
    PreservationRotationPhase _preservationPhase;
    std::vector<EchoTracker> _activeEchoes;
    TemporalInfo _temporal;
    CallOfYseraInfo _callOfYsera;
    PreservationMetrics _metrics;

    // Timing variables
    uint32 _lastEmeraldBlossomTime;
    uint32 _lastVerdantEmbraceTime;
    uint32 _lastDreamBreathTime;
    uint32 _lastSpiritBloomTime;
    uint32 _lastTemporalAnomalyTime;
    uint32 _lastRenewingBlazeTime;
    uint32 _lastDreamFlightTime;
    uint32 _lastReversionTime;
    uint32 _lastEchoUpdate;

    // Configuration constants
    static constexpr uint32 ECHO_MAX_COUNT = 8;
    static constexpr uint32 ECHO_HEAL_INTERVAL = 2000;      // 2 seconds
    static constexpr uint32 ECHO_DURATION = 30000;         // 30 seconds
    static constexpr uint32 TEMPORAL_ANOMALY_DURATION = 8000; // 8 seconds
    static constexpr uint32 TEMPORAL_COMPRESSION_DURATION = 10000; // 10 seconds
    static constexpr uint32 CALL_OF_YSERA_DURATION = 15000; // 15 seconds
    static constexpr float EMERGENCY_HEALTH_THRESHOLD = 0.3f; // 30% health
    static constexpr float LOW_HEALTH_THRESHOLD = 0.6f;     // 60% health
    static constexpr float GROUP_HEAL_THRESHOLD = 0.7f;     // 70% health
    static constexpr uint32 GROUP_HEAL_COUNT_THRESHOLD = 3; // 3+ injured for group heals
    static constexpr float HEALING_EFFICIENCY_THRESHOLD = 0.8f; // 80% efficiency minimum

    // Ability priorities
    std::vector<uint32> _emergencyHeals;
    std::vector<uint32> _sustainHeals;
    std::vector<uint32> _groupHeals;
    std::vector<uint32> _empoweredHeals;
    std::vector<uint32> _temporalAbilities;

    // Optimization settings
    bool _prioritizeEchoes;
    bool _conserveEssenceForEmergencies;
    bool _useGroupHealingOptimization;
    uint32 _maxEchoes;
    float _healingEfficiencyTarget;
};

} // namespace Playerbot

#endif // PRESERVATION_SPECIALIZATION_H