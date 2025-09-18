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
#include "WarlockSpecialization.h"
#include "Position.h"
#include <unordered_map>
#include <memory>

// Forward declarations
class AfflictionSpecialization;
class DemonologySpecialization;
class DestructionSpecialization;

namespace Playerbot
{

// Warlock AI implementation with specialization pattern
class TC_GAME_API WarlockAI : public ClassAI
{
public:
    explicit WarlockAI(Player* bot);
    ~WarlockAI() = default;

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
    // Specialization system
    WarlockSpec _currentSpec;
    std::unique_ptr<WarlockSpecialization> _specialization;

    // Performance tracking
    uint32 _manaSpent;
    uint32 _damageDealt;
    uint32 _dotDamage;
    uint32 _petDamage;
    std::unordered_map<uint32, uint32> _abilityUsage;

    // Specialization management
    void InitializeSpecialization();
    WarlockSpec DetectCurrentSpecialization();
    void SwitchSpecialization(WarlockSpec newSpec);

    // Delegation to specialization
    void DelegateToSpecialization(::Unit* target);

    // Shared warlock utilities
    void UpdateWarlockBuffs();
    void UpdatePetCheck();
    void UpdateSoulShardCheck();
};

} // namespace Playerbot