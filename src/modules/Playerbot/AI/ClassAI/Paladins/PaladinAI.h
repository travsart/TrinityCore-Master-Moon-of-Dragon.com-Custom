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
#include "PaladinSpecialization.h"
#include "Position.h"
#include <unordered_map>
#include <memory>

// Forward declarations
class HolySpecialization;
class ProtectionSpecialization;
class RetributionSpecialization;

namespace Playerbot
{

// Paladin AI implementation with specialization pattern
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
    // Specialization system
    PaladinSpec _currentSpec;
    std::unique_ptr<PaladinSpecialization> _specialization;

    // Performance tracking
    uint32 _manaSpent;
    uint32 _healingDone;
    uint32 _damageDealt;

    // Shared utility tracking
    uint32 _lastBlessings;
    uint32 _lastAura;
    std::unordered_map<uint32, uint32> _abilityUsage;

    // Specialization management
    void InitializeSpecialization();
    void UpdateSpecialization();
    PaladinSpec DetectCurrentSpecialization();
    void SwitchSpecialization(PaladinSpec newSpec);

    // Delegation to specialization
    void DelegateToSpecialization(::Unit* target);

    // Shared paladin utilities
    void UpdatePaladinBuffs();
    void CastBlessings();
    void UpdateAuras();
};

} // namespace Playerbot