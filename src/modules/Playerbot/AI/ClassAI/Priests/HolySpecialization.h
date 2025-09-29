/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "PriestSpecialization.h"
#include <map>
#include <set>

namespace Playerbot
{

class TC_GAME_API HolySpecialization : public PriestSpecialization
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

    // Healing interface
    void UpdateHealing() override;
    bool ShouldHeal() override;
    ::Unit* GetBestHealTarget() override;
    void HealTarget(::Unit* target) override;

    // Role management
    PriestRole GetCurrentRole() override;
    void SetRole(PriestRole role) override;

    // Specialization info
    PriestSpec GetSpecialization() const override { return PriestSpec::HOLY; }
    const char* GetSpecializationName() const override { return "Holy"; }

private:
    // Holy-specific mechanics
    void UpdateHolyWordCooldowns();
    void UpdateSerendipity();
    void UpdateSpiritOfRedemption();
    void UpdateEmpoweredRenew();
    bool ShouldCastHolyWordSerenity(::Unit* target);
    bool ShouldCastHolyWordSanctify();
    bool ShouldCastDivineHymn();
    bool ShouldCastGuardianSpirit(::Unit* target);
    bool ShouldCastEmpoweredRenew(::Unit* target);

    // Holy Word abilities
    void CastHolyWordSerenity(::Unit* target);
    void CastHolyWordSanctify();
    void CastHolyWordChastise(::Unit* target);
    void CastDivineHymn();
    void CastGuardianSpirit(::Unit* target);

    // Enhanced healing spells
    void CastGreaterHeal(::Unit* target);
    void CastFlashHeal(::Unit* target);
    void CastHeal(::Unit* target);
    void CastRenew(::Unit* target);
    void CastPrayerOfHealing();
    void CastCircleOfHealing();
    void CastPrayerOfMending(::Unit* target);

    // Serendipity mechanics
    void UpdateSerendipityStacks();
    uint32 GetSerendipityStacks();
    bool ShouldUseSerendipity();
    void ConsumeSerendipity();

    // HoT management
    void UpdateHoTs();
    void ManageRenews();
    bool ShouldRefreshRenew(::Unit* target);
    uint32 GetRenewTimeRemaining(::Unit* target);
    void OptimizeHoTCoverage();

    // AoE healing optimization
    void UpdateAoEHealing();
    bool ShouldUseAoEHealing();
    void CastOptimalAoEHeal();
    std::vector<::Unit*> GetClusteredInjuredMembers();
    Position GetOptimalAoEHealPosition();

    // Spirit of Redemption
    void HandleSpiritOfRedemption();
    bool IsInSpiritOfRedemption();
    void MaximizeSpiritHealing();

    // Chakra system (if applicable)
    void UpdateChakra();
    void EnterChakraSerenity();
    void EnterChakraSanctuary();
    bool IsInChakraSerenity();
    bool IsInChakraSanctuary();

    // Emergency healing protocols
    void HandleEmergencyHealing();
    void UseEmergencyHeals(::Unit* target);
    void ActivateEmergencyCooldowns();

    // Healing optimization
    void OptimizeHealingRotation();
    void PrioritizeHealingTargets();
    ::Unit* GetMostEfficientHealTarget();
    uint32 CalculateHealEfficiency(::Unit* target, uint32 spellId);

    // Mana efficiency
    void OptimizeManaUsage();
    bool ShouldUseFastHeals();
    bool ShouldUseEfficientHeals();
    void ConserveManaIfNeeded();

    // Holy spell IDs
    enum HolySpells
    {
        HOLY_WORD_SERENITY = 2050,
        HOLY_WORD_SANCTIFY = 34861,
        HOLY_WORD_CHASTISE = 88625,
        DIVINE_HYMN = 64843,
        GUARDIAN_SPIRIT = 47788,
        SERENDIPITY = 63730,
        SPIRIT_OF_REDEMPTION = 20711,
        EMPOWERED_RENEW = 63534,
        CHAKRA_SERENITY = 81208,
        CHAKRA_SANCTUARY = 81206,
        APOTHEOSIS = 200183,
        BENEDICTION = 193157
    };

    // State tracking
    PriestRole _currentRole;
    uint32 _serendipityStacks;
    bool _inSpiritOfRedemption;
    bool _inChakraSerenity;
    bool _inChakraSanctuary;
    uint32 _spiritActivationTime;

    // HoT tracking per target
    std::map<uint64, uint32> _renewTimers;
    std::map<uint64, uint32> _prayerOfMendingBounces;

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;

    // Priority queue for healing
    std::priority_queue<Playerbot::HealTarget> _healQueue;

    // Performance optimization
    uint32 _lastHealCheck;
    uint32 _lastHoTCheck;
    uint32 _lastAoECheck;
    uint32 _lastSerendipityCheck;
    uint32 _lastRotationUpdate;

    // AoE healing tracking
    std::vector<uint64> _clusteredMembers;
    uint32 _lastClusterScan;
    Position _lastAoEPosition;

    // Emergency state
    bool _emergencyMode;
    uint32 _emergencyStartTime;

    // Constants
    static constexpr uint32 RENEW_DURATION = 15000; // 15 seconds
    static constexpr uint32 PRAYER_OF_MENDING_BOUNCES = 5;
    static constexpr uint32 MAX_SERENDIPITY_STACKS = 2;
    static constexpr uint32 SERENDIPITY_DURATION = 20000; // 20 seconds
    static constexpr uint32 SPIRIT_OF_REDEMPTION_DURATION = 15000; // 15 seconds
    static constexpr float AOE_HEAL_THRESHOLD = 3.0f; // Use AoE with 3+ injured
    static constexpr float EMERGENCY_HEALTH_THRESHOLD = 25.0f;
    static constexpr float RENEW_REFRESH_THRESHOLD = 3000; // 3 seconds remaining
    static constexpr float MANA_EFFICIENT_THRESHOLD = 0.6f;
    static constexpr float CLUSTER_DISTANCE = 15.0f; // Distance for AoE healing
};

} // namespace Playerbot