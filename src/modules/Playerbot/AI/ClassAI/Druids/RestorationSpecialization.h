/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "DruidSpecialization.h"
#include <map>
#include <vector>
#include <queue>

namespace Playerbot
{

// Healing priority levels for Druid
enum class DruidHealPriority : uint8
{
    EMERGENCY = 0,      // <20% health, imminent death
    CRITICAL = 1,       // 20-40% health, needs immediate attention
    MODERATE = 2,       // 40-70% health, should heal soon
    MAINTENANCE = 3,    // 70-90% health, top off when convenient
    FULL = 4           // >90% health, no healing needed
};

// Heal target info for druid priority queue
struct DruidHealTarget
{
    ::Unit* target;
    DruidHealPriority priority;
    float healthPercent;
    uint32 missingHealth;
    bool inCombat;
    uint32 timestamp;

    DruidHealTarget() : target(nullptr), priority(DruidHealPriority::FULL), healthPercent(100.0f),
                        missingHealth(0), inCombat(false), timestamp(0) {}

    DruidHealTarget(::Unit* t, DruidHealPriority p, float hp, uint32 missing)
        : target(t), priority(p), healthPercent(hp), missingHealth(missing),
          inCombat(t ? t->IsInCombat() : false), timestamp(getMSTime()) {}

    bool operator<(const DruidHealTarget& other) const
    {
        if (priority != other.priority)
            return priority > other.priority;
        if (healthPercent != other.healthPercent)
            return healthPercent > other.healthPercent;
        return timestamp > other.timestamp;
    }
};

class TC_GAME_API RestorationSpecialization : public DruidSpecialization
{
public:
    explicit RestorationSpecialization(Player* bot);

    // Core specialization interface
    void UpdateRotation(::Unit* target) override;
    void UpdateBuffs() override;
    void UpdateCooldowns(uint32 diff) override;
    bool CanUseAbility(uint32 spellId) override;

    // Combat callbacks
    void OnCombatStart(::Unit* target) override;
    void OnCombatEnd() override;

    // Resource management
    bool HasEnoughResource(uint32 spellId) override;
    void ConsumeResource(uint32 spellId) override;

    // Positioning
    Position GetOptimalPosition(::Unit* target) override;
    float GetOptimalRange(::Unit* target) override;

    // Form management
    void UpdateFormManagement() override;
    DruidForm GetOptimalFormForSituation() override;
    bool ShouldShiftToForm(DruidForm form) override;
    void ShiftToForm(DruidForm form) override;

    // DoT/HoT management
    void UpdateDotHotManagement() override;
    bool ShouldApplyDoT(::Unit* target, uint32 spellId) override;
    bool ShouldApplyHoT(::Unit* target, uint32 spellId) override;

    // Specialization info
    DruidSpec GetSpecialization() const override { return DruidSpec::RESTORATION; }
    const char* GetSpecializationName() const override { return "Restoration"; }

private:
    // Restoration-specific mechanics
    void UpdateHealing();
    void UpdateHealOverTimeManagement();
    void UpdateNaturesSwiftness();
    void UpdateTranquility();
    bool ShouldCastHealingTouch(::Unit* target);
    bool ShouldCastRegrowth(::Unit* target);
    bool ShouldCastRejuvenation(::Unit* target);
    bool ShouldCastLifebloom(::Unit* target);
    bool ShouldCastSwiftmend(::Unit* target);
    bool ShouldCastTranquility();
    bool ShouldUseNaturesSwiftness();

    // Healing optimization
    ::Unit* GetBestHealTarget();
    void HealTarget(::Unit* target);
    void PrioritizeHealing();
    uint32 GetOptimalHealSpell(const DruidHealTarget& healTarget);
    void PerformTriage();

    // HoT management
    void ApplyHealingOverTime(::Unit* target, uint32 spellId);
    void RefreshExpiringHoTs();
    uint32 GetHoTRemainingTime(::Unit* target, uint32 spellId);
    void ManageLifebloomStack(::Unit* target);

    // Group healing
    void UpdateGroupHealing();
    bool ShouldUseGroupHeals();
    void HandleEmergencyHealing();
    void UseEmergencyHeals(::Unit* target);
    bool IsEmergencyHealing();

    // Restoration abilities
    void CastHealingTouch(::Unit* target);
    void CastRegrowth(::Unit* target);
    void CastRejuvenation(::Unit* target);
    void CastLifebloom(::Unit* target);
    void CastSwiftmend(::Unit* target);
    void CastTranquility();
    void CastInnervate(::Unit* target);

    // Tree of Life form management
    void EnterTreeOfLifeForm();
    bool ShouldUseTreeForm();
    void ManageTreeForm();

    // Nature's Swiftness management
    void UseNaturesSwiftness();
    bool IsNaturesSwiftnessReady();
    void CastInstantHealingTouch(::Unit* target);

    // Mana management for healers
    void ManageMana();
    void CastInnervateOptimal();
    bool ShouldConserveMana();

    // Restoration spell IDs
    enum RestorationSpells
    {
        HEALING_TOUCH = 5185,
        REGROWTH = 8936,
        SWIFTMEND = 18562,
        TRANQUILITY = 740,
        INNERVATE = 29166,
        NATURES_SWIFTNESS = 17116,
        REMOVE_CURSE = 2782,
        ABOLISH_POISON = 2893
    };

    // Healing tracking
    std::priority_queue<DruidHealTarget> _healQueue;
    std::unordered_map<ObjectGuid, std::vector<HealOverTimeInfo>> _activeHoTs;

    // HoT tracking
    std::unordered_map<ObjectGuid, uint32> _regrowthTimers;
    std::unordered_map<ObjectGuid, uint32> _lifebloomStacks;

    // Tree of Life form tracking
    uint32 _treeOfLifeRemaining;
    bool _inTreeForm;
    uint32 _lastTreeFormShift;

    // Nature's Swiftness tracking
    uint32 _naturesSwiftnessReady;
    uint32 _lastNaturesSwiftness;

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;
    uint32 _tranquilityReady;
    uint32 _lastTranquility;

    // Performance optimization
    uint32 _lastHealCheck;
    uint32 _lastHotCheck;
    uint32 _lastGroupScan;

    // Group member tracking
    std::vector<::Unit*> _groupMembers;

    // Emergency state
    bool _emergencyMode;
    uint32 _emergencyStartTime;

    // Performance tracking
    uint32 _totalHealingDone;
    uint32 _overhealingDone;
    uint32 _manaSpent;

    // Constants
    static constexpr float OPTIMAL_HEALING_RANGE = 40.0f;
    static constexpr uint32 TREE_OF_LIFE_DURATION = 25000; // 25 seconds
    static constexpr uint32 NATURES_SWIFTNESS_COOLDOWN = 60000; // 1 minute
    static constexpr uint32 TRANQUILITY_COOLDOWN = 480000; // 8 minutes
    static constexpr float EMERGENCY_HEALTH_THRESHOLD = 25.0f;
    static constexpr float REGROWTH_THRESHOLD = 50.0f;
    static constexpr float HEALING_TOUCH_THRESHOLD = 70.0f;
    static constexpr uint32 LIFEBLOOM_MAX_STACKS = 3;
    static constexpr uint32 REJUVENATION_DURATION = 12000; // 12 seconds
    static constexpr uint32 REGROWTH_DURATION = 21000; // 21 seconds
    static constexpr uint32 LIFEBLOOM_DURATION = 7000; // 7 seconds
    static constexpr float MANA_CONSERVATION_THRESHOLD = 0.3f;
};

} // namespace Playerbot