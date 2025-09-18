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

namespace Playerbot
{

class TC_GAME_API RetributionSpecialization : public PaladinSpecialization
{
public:
    explicit RetributionSpecialization(Player* bot);

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
    PaladinSpec GetSpecialization() const override { return PaladinSpec::RETRIBUTION; }
    const char* GetSpecializationName() const override { return "Retribution"; }

private:
    // Retribution-specific mechanics
    void UpdateSealTwisting();
    void UpdateArtOfWar();
    void UpdateDivineStorm();
    bool ShouldCastCrusaderStrike(::Unit* target);
    bool ShouldCastDivineStorm();
    bool ShouldCastTemplarsVerdict(::Unit* target);
    bool ShouldCastExorcism(::Unit* target);

    // Two-handed weapon optimization
    void OptimizeTwoHandedWeapon();
    bool HasTwoHandedWeapon();

    // DPS rotation
    void CastCrusaderStrike(::Unit* target);
    void CastTemplarsVerdict(::Unit* target);
    void CastDivineStorm();
    void CastExorcism(::Unit* target);
    void CastHammerOfWrath(::Unit* target);

    // Retribution spell IDs
    enum RetributionSpells
    {
        CRUSADER_STRIKE = 35395,
        TEMPLARS_VERDICT = 85256,
        DIVINE_STORM = 53385,
        EXORCISM = 879,
        HAMMER_OF_WRATH = 24275,
        ART_OF_WAR = 53489,
        SEAL_OF_COMMAND = 20375
    };

    // State tracking
    uint32 _holyPower;
    bool _hasArtOfWar;
    std::map<uint32, uint32> _cooldowns;
};

} // namespace Playerbot