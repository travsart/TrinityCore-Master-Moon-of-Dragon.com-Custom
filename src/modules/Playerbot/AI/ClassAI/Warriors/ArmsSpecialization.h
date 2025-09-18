/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "WarriorSpecialization.h"
#include <map>

namespace Playerbot
{

class TC_GAME_API ArmsSpecialization : public WarriorSpecialization
{
public:
    explicit ArmsSpecialization(Player* bot);

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

    // Stance management
    void UpdateStance() override;
    WarriorStance GetOptimalStance(::Unit* target) override;
    void SwitchStance(WarriorStance stance) override;

    // Specialization info
    WarriorSpec GetSpecialization() const override { return WarriorSpec::ARMS; }
    const char* GetSpecializationName() const override { return "Arms"; }

private:
    // Arms-specific mechanics
    void UpdateMortalStrike();
    void UpdateOverpower();
    void UpdateDeepWounds();
    void UpdateTacticalMastery();
    bool ShouldCastMortalStrike(::Unit* target);
    bool ShouldCastOverpower(::Unit* target);
    bool ShouldCastExecute(::Unit* target);
    bool ShouldCastColossusSmash(::Unit* target);
    bool ShouldCastWarBreaker(::Unit* target);

    // Two-handed weapon optimization
    void OptimizeTwoHandedWeapon();
    void UpdateWeaponMastery();
    bool HasTwoHandedWeapon();
    void CastSweepingStrikes();

    // Arms rotation spells
    void CastMortalStrike(::Unit* target);
    void CastColossusSmash(::Unit* target);
    void CastOverpower(::Unit* target);
    void CastExecute(::Unit* target);
    void CastWarBreaker(::Unit* target);
    void CastWhirlwind();
    void CastCleave(::Unit* target);

    // Deep Wounds management
    void ApplyDeepWounds(::Unit* target);
    bool HasDeepWounds(::Unit* target);
    uint32 GetDeepWoundsTimeRemaining(::Unit* target);

    // Tactical mastery and stance dancing
    void ManageStanceDancing();
    bool ShouldSwitchToDefensive();
    bool ShouldSwitchToBerserker();
    uint32 GetTacticalMasteryRage();

    // Cooldown management
    void UpdateArmsCooldowns(uint32 diff);
    void UseBladestorm();
    void UseAvatar();
    void UseRecklessness();
    bool ShouldUseBladestorm();
    bool ShouldUseAvatar();

    // Execute phase management
    void HandleExecutePhase(::Unit* target);
    bool IsInExecutePhase(::Unit* target);
    void OptimizeExecuteRotation(::Unit* target);

    // Arms spell IDs
    enum ArmsSpells
    {
        MORTAL_STRIKE = 12294,
        COLOSSUS_SMASH = 86346,
        OVERPOWER = 7384,
        EXECUTE = 5308,
        WAR_BREAKER = 262161,
        WHIRLWIND = 1680,
        SWEEPING_STRIKES = 260708,
        BLADESTORM = 227847,
        AVATAR = 107574,
        RECKLESSNESS = 1719,
        DEEP_WOUNDS = 115767,
        TACTICAL_MASTERY = 12295,
        SUDDEN_DEATH = 29723
    };

    // State tracking
    WarriorStance _preferredStance;
    uint32 _lastMortalStrike;
    uint32 _lastColossusSmash;
    uint32 _lastOverpower;
    bool _overpowerReady;
    bool _suddenDeathProc;

    // Deep Wounds tracking per target
    std::map<uint64, uint32> _deepWoundsTimers;

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;

    // Performance optimization
    uint32 _lastStanceCheck;
    uint32 _lastWeaponCheck;
    uint32 _lastRotationUpdate;

    // Execute phase tracking
    bool _inExecutePhase;
    uint32 _executePhaseStartTime;

    // Constants
    static constexpr uint32 DEEP_WOUNDS_DURATION = 21000; // 21 seconds
    static constexpr uint32 COLOSSUS_SMASH_DURATION = 10000; // 10 seconds
    static constexpr uint32 OVERPOWER_WINDOW = 5000; // 5 seconds
    static constexpr float EXECUTE_HEALTH_THRESHOLD = 20.0f;
    static constexpr uint32 MORTAL_STRIKE_RAGE_COST = 30;
    static constexpr uint32 EXECUTE_RAGE_COST = 15;
    static constexpr uint32 TACTICAL_MASTERY_RAGE = 25;
};

} // namespace Playerbot