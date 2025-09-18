/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef AUGMENTATION_SPECIALIZATION_H
#define AUGMENTATION_SPECIALIZATION_H

#include "EvokerSpecialization.h"
#include <vector>
#include <unordered_set>
#include <unordered_map>

namespace Playerbot
{

enum class AugmentationRotationPhase : uint8
{
    BUFF_APPLICATION = 0,
    EBON_MIGHT_MANAGEMENT = 1,
    PRESCIENCE_DISTRIBUTION = 2,
    BREATH_OF_EONS_SETUP = 3,
    DAMAGE_CONTRIBUTION = 4,
    BUFF_REFRESH = 5,
    UTILITY_SUPPORT = 6,
    EMERGENCY_SUPPORT = 7
};

struct AugmentationBuffInfo
{
    ::Unit* target;
    uint32 spellId;
    uint32 timeRemaining;
    uint32 lastApplication;
    uint8 stacks;
    bool isActive;

    AugmentationBuffInfo() : target(nullptr), spellId(0), timeRemaining(0), lastApplication(0), stacks(0), isActive(false) {}
    AugmentationBuffInfo(::Unit* tgt, uint32 spell) : target(tgt), spellId(spell), timeRemaining(0),
                                                     lastApplication(getMSTime()), stacks(1), isActive(true) {}
};

struct AugmentationMetrics
{
    uint32 ebonMightApplications;
    uint32 prescienceApplications;
    uint32 breathOfEonsCasts;
    uint32 blisteryScalesApplications;
    uint32 totalBuffsApplied;
    uint32 totalDamageContributed;
    float ebonMightUptime;
    float prescienceUptime;
    float averageBuffsActive;
    float damageAmplificationProvided;

    AugmentationMetrics() : ebonMightApplications(0), prescienceApplications(0), breathOfEonsCasts(0),
                          blisteryScalesApplications(0), totalBuffsApplied(0), totalDamageContributed(0),
                          ebonMightUptime(0.0f), prescienceUptime(0.0f), averageBuffsActive(0.0f),
                          damageAmplificationProvided(0.0f) {}
};

class AugmentationSpecialization : public EvokerSpecialization
{
public:
    explicit AugmentationSpecialization(Player* bot);
    ~AugmentationSpecialization() override = default;

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
    // Augmentation-specific systems
    void UpdateBuffManagement();
    void UpdateEbonMightTracking();
    void UpdatePrescienceTracking();
    void UpdateBuffPriorities();

    // Rotation phases
    void ExecuteBuffApplication(::Unit* target);
    void ExecuteEbonMightManagement(::Unit* target);
    void ExecutePrescienceDistribution(::Unit* target);
    void ExecuteBreathOfEonsSetup(::Unit* target);
    void ExecuteDamageContribution(::Unit* target);
    void ExecuteBuffRefresh(::Unit* target);
    void ExecuteUtilitySupport(::Unit* target);
    void ExecuteEmergencySupport(::Unit* target);

    // Core buff abilities
    void CastEbonMight(::Unit* target);
    void CastPrescience(::Unit* target);
    void CastBlisteryScales(::Unit* target);
    void CastReactiveHide(::Unit* target);

    // Empowered abilities
    void CastEmpoweredBreathOfEons(::Unit* target, EmpowermentLevel level);

    // Buff management
    void ApplyOptimalBuffs();
    void RefreshExpiredBuffs();
    void DistributeBuffsOptimally();
    bool NeedsEbonMight(::Unit* target);
    bool NeedsPrescience(::Unit* target);
    bool NeedsBlisteryScales(::Unit* target);

    // Target selection for buffs
    ::Unit* GetBestEbonMightTarget();
    ::Unit* GetBestPrescienceTarget();
    ::Unit* GetBestBlisteryScalesTarget();
    std::vector<::Unit*> GetBuffTargets();
    std::vector<::Unit*> GetDamageDealer();

    // Buff optimization
    void OptimizeBuffDistribution();
    void PrioritizeBuffTargets();
    uint32 CalculateBuffValue(::Unit* target, uint32 spellId);
    bool ShouldReplaceExistingBuff(::Unit* target, uint32 spellId);

    // Damage contribution
    void ContributeDamageAsAugmentation(::Unit* target);
    bool ShouldContributeDamage();

    // Utility and support
    void ProvideUtilitySupport();
    void HandleEmergencySupport();

    // Metrics and analysis
    void UpdateAugmentationMetrics();
    void AnalyzeBuffEfficiency();
    void LogAugmentationDecision(const std::string& decision, const std::string& reason);

    // State variables
    AugmentationRotationPhase _augmentationPhase;
    std::vector<AugmentationBuffInfo> _activeBuffs;
    AugmentationMetrics _metrics;

    // Timing variables
    uint32 _lastEbonMightTime;
    uint32 _lastPrescienceTime;
    uint32 _lastBreathOfEonsTime;
    uint32 _lastBlisteryScalesTime;
    uint32 _lastBuffRefreshTime;

    // Configuration constants
    static constexpr uint32 EBON_MIGHT_DURATION = 30000;        // 30 seconds
    static constexpr uint32 PRESCIENCE_DURATION = 18000;        // 18 seconds
    static constexpr uint32 BLISTERY_SCALES_DURATION = 600000;  // 10 minutes
    static constexpr uint32 BUFF_REFRESH_INTERVAL = 5000;       // 5 seconds
    static constexpr uint32 MAX_EBON_MIGHT_TARGETS = 4;         // Maximum targets
    static constexpr uint32 MAX_PRESCIENCE_TARGETS = 2;         // Maximum targets
    static constexpr float BUFF_REFRESH_THRESHOLD = 0.3f;       // Refresh at 30% duration

    // Ability priorities
    std::vector<uint32> _buffAbilities;
    std::vector<uint32> _empoweredAbilities;
    std::vector<uint32> _damageAbilities;
    std::vector<uint32> _utilityAbilities;

    // Optimization settings
    bool _prioritizeBuffs;
    bool _optimizeBuffDistribution;
    uint32 _maxBuffTargets;
    float _buffEfficiencyThreshold;
};

} // namespace Playerbot

#endif // AUGMENTATION_SPECIALIZATION_H