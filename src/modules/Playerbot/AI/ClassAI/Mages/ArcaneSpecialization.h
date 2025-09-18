/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "MageSpecialization.h"
#include <map>

namespace Playerbot
{

class TC_GAME_API ArcaneSpecialization : public MageSpecialization
{
public:
    explicit ArcaneSpecialization(Player* bot);

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

    // Specialization info
    MageSpec GetSpecialization() const override { return MageSpec::ARCANE; }
    const char* GetSpecializationName() const override { return "Arcane"; }

private:
    // Arcane-specific mechanics
    void UpdateArcaneCharges();
    void UpdateManaGems();
    bool ShouldConserveMana();
    bool ShouldUseManaGem();
    uint32 GetArcaneCharges();
    void CastArcaneMissiles();
    void CastArcaneBlast();
    void CastArcaneOrb();
    void CastArcaneBarrage();
    void CastPresenceOfMind();
    void CastArcanePower();
    void CastManaShield();
    void UseManaGem();

    // Arcane blast stacking mechanics
    uint32 GetArcaneBlastStacks();
    bool ShouldCastArcaneBlast();
    bool ShouldCastArcaneBarrage();
    bool ShouldCastArcaneMissiles();

    // Cooldown and buff management
    void UpdateArcaneCooldowns(uint32 diff);
    void CheckArcaneBuffs();
    bool HasClearcastingProc();
    bool HasPresenceOfMind();
    bool HasArcanePower();

    // Mana management specific to Arcane
    void OptimizeManaUsage();
    bool IsInBurnPhase();
    bool IsInConservePhase();
    void EnterBurnPhase();
    void EnterConservePhase();

    // Arcane spell IDs
    enum ArcaneSpells
    {
        ARCANE_MISSILES = 5143,
        ARCANE_BLAST = 30451,
        ARCANE_BARRAGE = 44425,
        ARCANE_ORB = 153626,
        PRESENCE_OF_MIND = 12043,
        ARCANE_POWER = 12042,
        MANA_SHIELD = 1463,
        MANA_GEM = 759,
        CLEARCASTING = 12536,
        ARCANE_CHARGES = 36032
    };

    // State tracking
    uint32 _arcaneBlastStacks;
    uint32 _lastArcaneSpellTime;
    bool _inBurnPhase;
    bool _inConservePhase;
    uint32 _burnPhaseStartTime;
    uint32 _conservePhaseStartTime;

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;

    // Performance optimization
    uint32 _lastManaCheck;
    uint32 _lastBuffCheck;
    uint32 _lastRotationUpdate;

    // Constants
    static constexpr uint32 ARCANE_BLAST_MAX_STACKS = 4;
    static constexpr uint32 BURN_PHASE_DURATION = 15000; // 15 seconds
    static constexpr uint32 CONSERVE_PHASE_DURATION = 30000; // 30 seconds
    static constexpr float BURN_PHASE_MANA_THRESHOLD = 0.8f;
    static constexpr float CONSERVE_PHASE_MANA_THRESHOLD = 0.4f;
    static constexpr float MANA_GEM_THRESHOLD = 0.2f;
};

} // namespace Playerbot