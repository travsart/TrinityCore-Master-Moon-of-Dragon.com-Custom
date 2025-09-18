/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef MISTWEAVER_SPECIALIZATION_H
#define MISTWEAVER_SPECIALIZATION_H

#include "MonkSpecialization.h"
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <queue>

namespace Playerbot
{

enum class MistweaverRotationPhase : uint8
{
    ASSESSMENT = 0,
    EMERGENCY_HEALING = 1,
    FISTWEAVING = 2,
    HOT_MANAGEMENT = 3,
    GROUP_HEALING = 4,
    MANA_MANAGEMENT = 5,
    DAMAGE_CONTRIBUTION = 6,
    UTILITY_SUPPORT = 7,
    COOLDOWN_MANAGEMENT = 8
};

enum class HealingPriority : uint8
{
    EMERGENCY_HEAL = 0,
    CRITICAL_HEAL = 1,
    SUSTAIN_HEAL = 2,
    GROUP_HEAL = 3,
    HOT_REFRESH = 4,
    FISTWEAVING = 5,
    DAMAGE_CONTRIBUTION = 6
};

struct MistweaverMetrics
{
    uint32 vivifyCasts;
    uint32 envelopingMistCasts;
    uint32 renewingMistCasts;
    uint32 essenceFontCasts;
    uint32 soothingMistChannels;
    uint32 lifeCocoonCasts;
    uint32 fistweavingHealing;
    uint32 directHealing;
    uint32 totalHealingDone;
    uint32 overhealing;
    uint32 manaSpent;
    float healingEfficiency;
    float fistweavingUptime;
    float manaEfficiency;
    float averageGroupHealth;

    MistweaverMetrics() : vivifyCasts(0), envelopingMistCasts(0), renewingMistCasts(0), essenceFontCasts(0),
                         soothingMistChannels(0), lifeCocoonCasts(0), fistweavingHealing(0), directHealing(0),
                         totalHealingDone(0), overhealing(0), manaSpent(0), healingEfficiency(0.0f),
                         fistweavingUptime(0.0f), manaEfficiency(0.0f), averageGroupHealth(0.0f) {}
};

class MistweaverSpecialization : public MonkSpecialization
{
public:
    explicit MistweaverSpecialization(Player* bot);
    ~MistweaverSpecialization() override = default;

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

    // Target Selection Implementation
    ::Unit* GetBestTarget() override;

private:
    // Mistweaver-specific systems
    void UpdateHealingAssessment();
    void UpdateFistweavingSystem();
    void UpdateHoTManagement();
    void UpdateManaManagement();
    void UpdateCooldownManagement();

    // Rotation phases
    void ExecuteAssessmentPhase(::Unit* target);
    void ExecuteEmergencyHealing(::Unit* target);
    void ExecuteFistweaving(::Unit* target);
    void ExecuteHoTManagement(::Unit* target);
    void ExecuteGroupHealing(::Unit* target);
    void ExecuteManaManagement(::Unit* target);
    void ExecuteDamageContribution(::Unit* target);
    void ExecuteUtilitySupport(::Unit* target);
    void ExecuteCooldownManagement(::Unit* target);

    // Core healing abilities
    void CastVivify(::Unit* target);
    void CastEnvelopingMist(::Unit* target);
    void CastRenewingMist(::Unit* target);
    void CastEssenceFont();
    void CastSoothingMist(::Unit* target);
    void CastLifeCocoon(::Unit* target);

    // Special abilities
    void CastThunderFocusTea();
    void CastManaTea();
    void CastRevival();
    void CastLifeCycles();

    // Fistweaving abilities
    void CastTigerPalm(::Unit* target);
    void CastBlackoutKick(::Unit* target);
    void CastRisingSunKick(::Unit* target);
    void CastTeachingsOfTheMonastery();

    // Utility abilities
    void CastParalysis(::Unit* target);
    void CastRingOfPeace();
    void CastLegSweep();

    // Healing target management
    void ScanForHealTargets();
    void PrioritizeHealTargets();
    void UpdateHealingQueue();
    ::Unit* GetBestHealTarget();
    ::Unit* GetMostCriticalTarget();
    ::Unit* GetBestGroupHealTarget();
    std::vector<::Unit*> GetAlliesNeedingHealing(float healthThreshold = 0.8f);

    // Fistweaving management
    void EvaluateFistweavingMode();
    void ToggleFistweaving();
    bool ShouldFistweave();
    bool ShouldSwitchToDirectHealing();
    ::Unit* GetBestFistweavingTarget();
    void ProcessFistweavingHealing();

    // HoT management
    void UpdateRenewingMistTracking();
    void UpdateEnvelopingMistTracking();
    void RefreshExpiredHoTs();
    bool NeedsRenewingMist(::Unit* target);
    bool NeedsEnvelopingMist(::Unit* target);
    uint32 GetActiveHoTCount();

    // Mana optimization
    void OptimizeManaUsage();
    void ConserveMana();
    bool ShouldConserveMana();
    void UseManaRecoveryAbilities();
    float GetManaPercent();
    uint32 GetManaRequired(uint32 spellId);

    // Group healing optimization
    void AnalyzeGroupHealth();
    bool ShouldUseGroupHealing();
    bool ShouldUseEssenceFont();
    uint32 GetInjuredAllyCount();
    float GetAverageGroupHealth();

    // Positioning for healing
    void OptimizeHealerPositioning();
    bool IsAtOptimalHealingRange();
    void MaintainGroupProximity();
    Position GetCentralPosition();

    // Emergency healing
    void HandleEmergencyHealing();
    void UseMajorCooldowns();
    void PrioritizeEmergencyTargets();
    bool IsEmergencyHealing();

    // Damage contribution
    void ContributeDamageAsFistweaver(::Unit* target);
    void ContributeDamageAsHealer(::Unit* target);
    bool ShouldContributeDamage();

    // Cooldown optimization
    void ManageHealingCooldowns();
    void PrioritizeCooldownUsage();
    bool ShouldUseLifeCocoon(::Unit* target);
    bool ShouldUseRevival();

    // Performance analysis
    void UpdateMistweaverMetrics();
    void AnalyzeHealingEfficiency();
    void AnalyzeFistweavingEfficiency();
    void LogMistweaverDecision(const std::string& decision, const std::string& reason);

    // State variables
    MistweaverRotationPhase _mistweaverPhase;
    FistweavingInfo _fistweaving;
    std::priority_queue<MistweaverTarget> _healingTargets;
    MistweaverMetrics _metrics;

    // HoT tracking
    std::unordered_map<ObjectGuid, uint32> _renewingMistTimers;
    std::unordered_map<ObjectGuid, uint32> _envelopingMistTimers;
    std::unordered_map<ObjectGuid, uint32> _soothingMistTimers;

    // Timing variables
    uint32 _lastVivifyTime;
    uint32 _lastEnvelopingMistTime;
    uint32 _lastRenewingMistTime;
    uint32 _lastEssenceFontTime;
    uint32 _lastSoothingMistTime;
    uint32 _lastLifeCocoonTime;
    uint32 _lastHealingScanTime;
    uint32 _lastManaCheckTime;
    uint32 _lastFistweavingEval;

    // Configuration constants
    static constexpr float EMERGENCY_HEALTH_THRESHOLD = 0.25f;  // 25% health
    static constexpr float CRITICAL_HEALTH_THRESHOLD = 0.5f;    // 50% health
    static constexpr float LOW_HEALTH_THRESHOLD = 0.7f;         // 70% health
    static constexpr float GROUP_HEAL_THRESHOLD = 0.6f;         // 60% health
    static constexpr uint32 GROUP_HEAL_COUNT_THRESHOLD = 3;     // 3+ injured for group heals
    static constexpr float FISTWEAVING_SWITCH_THRESHOLD = 0.8f; // 80% group health
    static constexpr float MANA_CONSERVATION_THRESHOLD = 0.3f;  // 30% mana
    static constexpr uint32 HOT_REFRESH_THRESHOLD = 5000;       // 5 seconds before expiry
    static constexpr uint32 HEALING_SCAN_INTERVAL = 1000;       // 1 second
    static constexpr float HEALING_RANGE = 40.0f;               // Healing range
    static constexpr float FISTWEAVING_EFFICIENCY_THRESHOLD = 0.8f; // 80% efficiency

    // Ability priorities
    std::vector<uint32> _emergencyHeals;
    std::vector<uint32> _sustainHeals;
    std::vector<uint32> _groupHeals;
    std::vector<uint32> _hotAbilities;
    std::vector<uint32> _fistweavingAbilities;

    // Optimization settings
    bool _prioritizeFistweaving;
    bool _conserveManaAggressively;
    bool _useGroupHealingOptimization;
    uint32 _maxHoTTargets;
    float _healingEfficiencyTarget;
};

} // namespace Playerbot

#endif // MISTWEAVER_SPECIALIZATION_H