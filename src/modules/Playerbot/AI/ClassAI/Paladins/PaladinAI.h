/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "../ClassAI.h"
#include "PaladinSpecialization.h"
#include "Position.h"
#include <memory>

namespace Playerbot
{

// Forward declarations
class PaladinSpecialization;
class HolyPaladinRefactored;
class ProtectionPaladinRefactored;
class RetributionPaladinRefactored;

// Minimal Paladin AI implementation - stub for compilation
class TC_GAME_API PaladinAI : public ClassAI
{
public:
    explicit PaladinAI(Player* bot);
    ~PaladinAI() = default;

    // ClassAI interface implementation
    void UpdateRotation(::Unit* target) override;
    void UpdateBuffs() override;
    void UpdateCooldowns(uint32 diff) override;
    bool CanUseAbility(uint32 spellId) override;

    // Combat state callbacks
    void OnCombatStart(::Unit* target) override;
    void OnCombatEnd() override;

protected:
    // Resource management
    bool HasEnoughResource(uint32 spellId) override;
    void ConsumeResource(uint32 spellId) override;

    // Positioning
    Position GetOptimalPosition(::Unit* target) override;
    float GetOptimalRange(::Unit* target) override;

private:
    // Specialization management
    void DetectSpecialization();
    void InitializeSpecialization();
    PaladinSpec _currentSpec;
    std::unique_ptr<PaladinSpecialization> _specialization;
    void SwitchSpecialization(PaladinSpec newSpec);
    void DelegateToSpecialization(::Unit* target);
};

} // namespace Playerbot