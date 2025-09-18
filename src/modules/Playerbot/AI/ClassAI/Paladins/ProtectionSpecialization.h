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
#include <vector>

namespace Playerbot
{

class TC_GAME_API ProtectionSpecialization : public PaladinSpecialization
{
public:
    explicit ProtectionSpecialization(Player* bot);

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
    PaladinSpec GetSpecialization() const override { return PaladinSpec::PROTECTION; }
    const char* GetSpecializationName() const override { return "Protection"; }

private:
    // Protection-specific mechanics
    void UpdateThreat();
    void UpdateShieldBlock();
    void UpdateAvengersShield();
    void UpdateConsecration();
    bool ShouldCastAvengersShield(::Unit* target);
    bool ShouldCastHammerOfWrath(::Unit* target);
    bool ShouldCastShieldOfRighteousness(::Unit* target);
    bool ShouldCastConsecration();

    // Threat management
    void GenerateThreat();
    void MaintainThreat();
    std::vector<::Unit*> GetThreatTargets();
    bool NeedsThreat(::Unit* target);

    // Shield abilities
    void CastAvengersShield(::Unit* target);
    void CastShieldOfRighteousness(::Unit* target);
    void CastHolyShield();

    // Area control
    void CastConsecration();
    void CastHammerOfWrath(::Unit* target);

    // Protection spell IDs
    enum ProtectionSpells
    {
        AVENGERS_SHIELD = 31935,
        SHIELD_OF_RIGHTEOUSNESS = 53600,
        HAMMER_OF_WRATH = 24275,
        CONSECRATION = 26573,
        HOLY_SHIELD = 20925,
        RIGHTEOUS_FURY = 25780
    };

    // State tracking
    uint32 _lastConsecration;
    uint32 _lastAvengersShield;
    std::map<uint32, uint32> _cooldowns;
};

} // namespace Playerbot