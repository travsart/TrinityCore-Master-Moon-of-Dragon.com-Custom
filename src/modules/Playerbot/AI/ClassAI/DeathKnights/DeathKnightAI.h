/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "ClassAI.h"
#include "DeathKnightSpecialization.h"
#include <memory>

namespace Playerbot
{

class DeathKnightSpecialization;

class TC_GAME_API DeathKnightAI : public ClassAI
{
public:
    explicit DeathKnightAI(Player* bot);
    ~DeathKnightAI() = default;

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
    DeathKnightSpec GetCurrentSpecialization() const;

    // Specialization delegation
    std::unique_ptr<DeathKnightSpecialization> _specialization;
    DeathKnightSpec _detectedSpec;
};

} // namespace Playerbot