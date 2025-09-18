/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "PaladinSpecialization.h"
#include <map>
#include <queue>

namespace Playerbot
{

// Healing priority levels for Paladin
enum class PaladinHealPriority : uint8
{
    EMERGENCY = 0,      // <20% health, imminent death
    CRITICAL = 1,       // 20-40% health, needs immediate attention
    MODERATE = 2,       // 40-70% health, should heal soon
    MAINTENANCE = 3,    // 70-90% health, top off when convenient
    FULL = 4           // >90% health, no healing needed
};

// Heal target info for paladin priority queue
struct PaladinHealTarget
{
    ::Unit* target;
    PaladinHealPriority priority;
    float healthPercent;
    uint32 missingHealth;
    bool inCombat;
    uint32 timestamp;

    PaladinHealTarget() : target(nullptr), priority(PaladinHealPriority::FULL), healthPercent(100.0f),
                         missingHealth(0), inCombat(false), timestamp(0) {}

    PaladinHealTarget(::Unit* t, PaladinHealPriority p, float hp, uint32 missing)
        : target(t), priority(p), healthPercent(hp), missingHealth(missing),
          inCombat(t ? t->IsInCombat() : false), timestamp(getMSTime()) {}

    bool operator<(const PaladinHealTarget& other) const
    {
        if (priority != other.priority)
            return priority > other.priority;
        if (healthPercent != other.healthPercent)
            return healthPercent > other.healthPercent;
        return timestamp > other.timestamp;
    }
};

class TC_GAME_API HolySpecialization : public PaladinSpecialization
{
public:
    explicit HolySpecialization(Player* bot);

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

    // Aura management
    void UpdateAura() override;
    PaladinAura GetOptimalAura() override;
    void SwitchAura(PaladinAura aura) override;

    // Specialization info
    PaladinSpec GetSpecialization() const override { return PaladinSpec::HOLY; }
    const char* GetSpecializationName() const override { return "Holy"; }

private:
    // Holy-specific mechanics
    void UpdateHealing();
    void UpdateDivineIllumination();
    void UpdateSealOfLight();
    bool ShouldCastHolyLight(::Unit* target);
    bool ShouldCastFlashOfLight(::Unit* target);
    bool ShouldCastHolyShock(::Unit* target);
    bool ShouldCastLayOnHands(::Unit* target);
    bool ShouldUseDivineFavor();

    // Healing optimization
    ::Unit* GetBestHealTarget();
    void HealTarget(::Unit* target);
    void PrioritizeHealing();
    uint32 GetOptimalHealSpell(const PaladinHealTarget& healTarget);
    void PerformTriage();

    // Holy Power mechanics (if applicable)
    void UpdateHolyPower();
    uint32 GetHolyPower();
    void SpendHolyPower();
    bool ShouldCastWordOfGlory(::Unit* target);
    bool ShouldCastLightOfDawn();

    // Beacon of Light management
    void UpdateBeaconOfLight();
    void CastBeaconOfLight(::Unit* target);
    ::Unit* GetBestBeaconTarget();
    bool HasBeaconOfLight(::Unit* target);

    // Holy shock mechanics
    void CastHolyShock(::Unit* target);
    bool CanHolyShockHeal();
    bool CanHolyShockDamage();

    // Divine favor and illumination
    void ManageDivineFavor();
    void TriggerDivineIllumination();
    bool HasDivineIllumination();

    // Group healing
    void UpdateGroupHealing();
    bool ShouldUseGroupHeals();
    void CastLightOfDawn();
    void CastWordOfGlory(::Unit* target);

    // Emergency healing
    void HandleEmergencyHealing();
    void UseEmergencyHeals(::Unit* target);
    bool IsEmergencyHealing();

    // Judgement for mana return
    void UpdateJudgementForMana();
    void CastJudgementOfWisdom(::Unit* target);
    bool ShouldJudgeForMana();

    // Holy spell IDs
    enum HolySpells
    {
        HOLY_SHOCK = 20473,
        BEACON_OF_LIGHT = 53563,
        DIVINE_ILLUMINATION = 31842,
        WORD_OF_GLORY = 85673,
        LIGHT_OF_DAWN = 85222,
        JUDGEMENT_OF_LIGHT = 20185,
        JUDGEMENT_OF_WISDOM = 53408,
        DIVINE_FAVOR = 20216,
        INFUSION_OF_LIGHT = 53576,
        SPEED_OF_LIGHT = 85499
    };

    // State tracking
    PaladinAura _currentAura;
    uint32 _holyPower;
    uint32 _lastDivineFavor;
    uint32 _lastLayOnHands;
    bool _hasDivineIllumination;
    bool _hasInfusionOfLight;

    // Healing tracking
    std::priority_queue<PaladinHealTarget> _healQueue;
    std::map<uint64, uint32> _beaconTargets;

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;

    // Performance optimization
    uint32 _lastHealCheck;
    uint32 _lastBeaconCheck;
    uint32 _lastAuraCheck;
    uint32 _lastRotationUpdate;

    // Group member tracking
    std::vector<::Unit*> _groupMembers;
    uint32 _lastGroupScan;

    // Emergency state
    bool _emergencyMode;
    uint32 _emergencyStartTime;

    // Constants
    static constexpr uint32 BEACON_OF_LIGHT_DURATION = 300000; // 5 minutes
    static constexpr uint32 DIVINE_FAVOR_COOLDOWN = 120000; // 2 minutes
    static constexpr uint32 LAY_ON_HANDS_COOLDOWN = 600000; // 10 minutes
    static constexpr uint32 HOLY_SHOCK_COOLDOWN = 6000; // 6 seconds
    static constexpr float EMERGENCY_HEALTH_THRESHOLD = 25.0f;
    static constexpr float FLASH_OF_LIGHT_THRESHOLD = 50.0f;
    static constexpr float HOLY_LIGHT_THRESHOLD = 70.0f;
    static constexpr uint32 MAX_HOLY_POWER = 3;
};

} // namespace Playerbot