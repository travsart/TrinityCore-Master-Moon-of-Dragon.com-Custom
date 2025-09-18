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
#include "DemonHunterSpecialization.h"
#include <memory>

namespace Playerbot
{

class DemonHunterSpecialization;

class TC_GAME_API DemonHunterAI : public ClassAI
{
public:
    explicit DemonHunterAI(Player* bot);
    ~DemonHunterAI() = default;

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
    DemonHunterSpec GetCurrentSpecialization() const;

    // Specialization delegation
    std::unique_ptr<DemonHunterSpecialization> _specialization;
    DemonHunterSpec _detectedSpec;
};

} // namespace Playerbot