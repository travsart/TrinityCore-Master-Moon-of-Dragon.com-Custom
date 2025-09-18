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

class TC_GAME_API FurySpecialization : public WarriorSpecialization
{
public:
    explicit FurySpecialization(Player* bot);

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
    WarriorSpec GetSpecialization() const override { return WarriorSpec::FURY; }
    const char* GetSpecializationName() const override { return "Fury"; }

private:
    // Fury-specific mechanics
    void UpdateEnrage();
    void UpdateFlurry();
    void UpdateBerserkerRage();
    void UpdateDualWield();
    bool ShouldCastBloodthirst(::Unit* target);
    bool ShouldCastWhirlwind();
    bool ShouldCastRampage(::Unit* target);
    bool ShouldCastExecute(::Unit* target);
    bool ShouldUseBerserkerRage();

    // Dual wield optimization
    void OptimizeDualWield();
    void UpdateOffhandAttacks();
    bool HasDualWieldWeapons();
    void CastFlurry();

    // Enrage mechanics
    void TriggerEnrage();
    void ManageEnrage();
    bool IsEnraged();
    uint32 GetEnrageTimeRemaining();
    void ExtendEnrage();

    // Fury rotation spells
    void CastBloodthirst(::Unit* target);
    void CastRampage(::Unit* target);
    void CastRagingBlow(::Unit* target);
    void CastFuriousSlash(::Unit* target);
    void CastExecute(::Unit* target);
    void CastWhirlwind();

    // Berserker mechanics
    void CastBerserkerRage();
    void UpdateBerserkerStance();
    bool ShouldStayInBerserkerStance();

    // Rage generation optimization
    void OptimizeRageGeneration();
    void BuildRage();
    bool ShouldConserveRage();
    void SpendRageEfficiently();

    // Cooldown management
    void UpdateFuryCooldowns(uint32 diff);
    void UseRecklessness();
    void UseEnragedRegeneration();
    bool ShouldUseRecklessness();
    bool ShouldUseEnragedRegeneration();

    // Execute phase management
    void HandleExecutePhase(::Unit* target);
    bool IsInExecutePhase(::Unit* target);
    void OptimizeExecuteRotation(::Unit* target);

    // Flurry mechanics
    void UpdateFlurryStacks();
    uint32 GetFlurryStacks();
    bool HasFlurryProc();
    void ConsumeFlurry();

    // Fury spell IDs
    enum FurySpells
    {
        BLOODTHIRST = 23881,
        RAMPAGE = 184367,
        RAGING_BLOW = 85288,
        FURIOUS_SLASH = 100130,
        EXECUTE = 5308,
        WHIRLWIND = 1680,
        BERSERKER_RAGE = 18499,
        ENRAGE = 184361,
        FLURRY = 12319,
        RECKLESSNESS = 1719,
        ENRAGED_REGENERATION = 55694,
        DUAL_WIELD = 674,
        TITANS_GRIP = 46917
    };

    // State tracking
    bool _isEnraged;
    uint32 _enrageEndTime;
    uint32 _flurryStacks;
    bool _flurryProc;
    uint32 _lastBerserkerRage;
    uint32 _lastBloodthirst;

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;

    // Performance optimization
    uint32 _lastEnrageCheck;
    uint32 _lastFlurryCheck;
    uint32 _lastDualWieldCheck;
    uint32 _lastRotationUpdate;

    // Execute phase tracking
    bool _inExecutePhase;
    uint32 _executePhaseStartTime;

    // Rage optimization
    uint32 _lastRageOptimization;
    float _averageRageGeneration;

    // Constants
    static constexpr uint32 ENRAGE_DURATION = 8000; // 8 seconds
    static constexpr uint32 FLURRY_DURATION = 15000; // 15 seconds
    static constexpr uint32 MAX_FLURRY_STACKS = 3;
    static constexpr float EXECUTE_HEALTH_THRESHOLD = 20.0f;
    static constexpr uint32 BLOODTHIRST_RAGE_COST = 30;
    static constexpr uint32 RAMPAGE_RAGE_COST = 85;
    static constexpr uint32 RAGING_BLOW_RAGE_COST = 25;
    static constexpr float OPTIMAL_RAGE_THRESHOLD = 60.0f;
    static constexpr float RAGE_DUMP_THRESHOLD = 90.0f;
};

} // namespace Playerbot