/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "ShamanSpecialization.h"
#include <vector>
#include <map>
#include <unordered_map>
#include <queue>

namespace Playerbot
{

// Healing priority levels for Shaman
enum class ShamanHealPriority : uint8
{
    EMERGENCY = 0,      // <20% health, imminent death
    CRITICAL = 1,       // 20-40% health, needs immediate attention
    MODERATE = 2,       // 40-70% health, should heal soon
    MAINTENANCE = 3,    // 70-90% health, top off when convenient
    FULL = 4           // >90% health, no healing needed
};

// Heal target info for shaman priority queue
struct ShamanHealTarget
{
    ::Unit* target;
    ShamanHealPriority priority;
    float healthPercent;
    uint32 missingHealth;
    bool inCombat;
    uint32 timestamp;

    ShamanHealTarget() : target(nullptr), priority(ShamanHealPriority::FULL), healthPercent(100.0f),
                         missingHealth(0), inCombat(false), timestamp(0) {}

    ShamanHealTarget(::Unit* t, ShamanHealPriority p, float hp, uint32 missing)
        : target(t), priority(p), healthPercent(hp), missingHealth(missing),
          inCombat(t ? t->IsInCombat() : false), timestamp(getMSTime()) {}

    bool operator<(const ShamanHealTarget& other) const
    {
        if (priority != other.priority)
            return priority > other.priority;
        if (healthPercent != other.healthPercent)
            return healthPercent > other.healthPercent;
        return timestamp > other.timestamp;
    }
};

class TC_GAME_API RestorationSpecialization : public ShamanSpecialization
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

    // Totem management
    void UpdateTotemManagement() override;
    void DeployOptimalTotems() override;
    uint32 GetOptimalFireTotem() override;
    uint32 GetOptimalEarthTotem() override;
    uint32 GetOptimalWaterTotem() override;
    uint32 GetOptimalAirTotem() override;

    // Shock rotation
    void UpdateShockRotation(::Unit* target) override;
    uint32 GetNextShockSpell(::Unit* target) override;

    // Specialization info
    ShamanSpec GetSpecialization() const override { return ShamanSpec::RESTORATION; }
    const char* GetSpecializationName() const override { return "Restoration"; }

private:
    // Restoration-specific mechanics
    void UpdateHealing();
    void UpdateRiptide();
    void UpdateEarthShield();
    void UpdateTidalWave();
    void UpdateNatureSwiftness();
    bool ShouldCastHealingWave(::Unit* target);
    bool ShouldCastLesserHealingWave(::Unit* target);
    bool ShouldCastChainHeal(::Unit* target);
    bool ShouldCastRiptide(::Unit* target);
    bool ShouldUseNatureSwiftness();

    // Healing optimization
    ::Unit* GetBestHealTarget();
    void HealTarget(::Unit* target);
    void PrioritizeHealing();
    uint32 GetOptimalHealSpell(const ShamanHealTarget& healTarget);
    void PerformTriage();

    // Earth Shield management
    void UpdateEarthShieldManagement();
    void CastEarthShield(::Unit* target);
    ::Unit* GetBestEarthShieldTarget();
    bool HasEarthShield(::Unit* target);

    // Riptide management
    void UpdateRiptideManagement();
    void CastRiptide(::Unit* target);
    bool HasRiptide(::Unit* target);
    uint32 GetRiptideRemainingTime(::Unit* target);

    // Chain heal mechanics
    void CastChainHeal(::Unit* target);
    std::vector<::Unit*> GetChainHealTargets(::Unit* primary);
    bool ShouldUseChainHeal();

    // Tidal wave mechanics
    void ManageTidalWave();
    void TriggerTidalWave();
    bool HasTidalWave();
    uint32 GetTidalWaveStacks();

    // Nature's swiftness
    void UseNatureSwiftness();
    bool IsNatureSwiftnessReady();
    void CastInstantHealingWave(::Unit* target);

    // Group healing
    void UpdateGroupHealing();
    bool ShouldUseGroupHeals();
    void HandleEmergencyHealing();
    void UseEmergencyHeals(::Unit* target);
    bool IsEmergencyHealing();

    // Cleansing totems
    void UpdateCleansingTotems();
    void DeployCleansingTotem();
    bool ShouldUsePoisonCleansing();
    bool ShouldUseDiseaseCleansing();

    // Mana management for healers
    void ManageMana();
    void UseWaterShield();
    void UseManaSpringTotem();
    bool ShouldConserveMana();

    // Healing totem management
    void ManageHealingTotems();
    void DeployHealingStreamTotem();
    bool ShouldUseHealingStreamTotem();

    // Restoration spell IDs
    enum RestorationSpells
    {
        HEALING_WAVE = 331,
        LESSER_HEALING_WAVE = 8004,
        CHAIN_HEAL = 1064,
        RIPTIDE = 61295,
        EARTH_SHIELD = 974,
        WATER_SHIELD = 52127,
        NATURE_SWIFTNESS = 16188,
        TIDAL_WAVE = 51564,
        ANCESTRAL_SPIRIT = 2008,
        CURE_POISON = 526,
        CURE_DISEASE = 2870
    };

    // State tracking
    uint32 _earthShieldCharges;
    uint32 _tidalWaveStacks;
    uint32 _natureSwiftnessReady;
    uint32 _lastNatureSwiftness;
    uint32 _lastEarthShield;
    uint32 _lastChainHeal;
    bool _hasWaterShield;
    bool _hasTidalWave;

    // Riptide tracking
    std::unordered_map<ObjectGuid, uint32> _riptideTimers;
    std::unordered_map<ObjectGuid, uint32> _earthShieldTargets;

    // Healing tracking
    std::priority_queue<ShamanHealTarget> _healQueue;

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;

    // Performance optimization
    uint32 _lastHealCheck;
    uint32 _lastEarthShieldCheck;
    uint32 _lastRiptideCheck;
    uint32 _lastTotemCheck;
    uint32 _lastGroupScan;

    // Group member tracking
    std::vector<::Unit*> _groupMembers;

    // Emergency state
    bool _emergencyMode;
    uint32 _emergencyStartTime;

    // Performance tracking
    uint32 _totalHealingDone;
    uint32 _manaSpent;
    uint32 _overhealingDone;

    // Constants
    static const uint32 EARTH_SHIELD_DURATION = 600000; // 10 minutes
    static const uint32 RIPTIDE_DURATION = 18000; // 18 seconds
    static const uint32 NATURE_SWIFTNESS_COOLDOWN = 120000; // 2 minutes
    static const uint32 CHAIN_HEAL_COOLDOWN = 2500; // 2.5 seconds
    // static const float EMERGENCY_HEALTH_THRESHOLD;
    // static const float LESSER_HEALING_WAVE_THRESHOLD;
    // static const float HEALING_WAVE_THRESHOLD;
    static const uint32 MAX_TIDAL_WAVE_STACKS = 2;
    // static const float OPTIMAL_HEALING_RANGE;
    static const uint32 CHAIN_HEAL_MIN_TARGETS = 3;
    // static const float MANA_CONSERVATION_THRESHOLD;
};

} // namespace Playerbot