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
#include "ShamanSpecialization.h"
#include "Position.h"
#include <unordered_map>
#include <memory>

// Forward declarations
class ElementalSpecialization;
class EnhancementSpecialization;
class RestorationSpecialization;

namespace Playerbot
{

// Shaman AI implementation with specialization pattern
class TC_GAME_API ShamanAI : public ClassAI
{
public:
    explicit ShamanAI(Player* bot);
    ~ShamanAI() = default;

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
    ShamanSpec _currentSpec;
    std::unique_ptr<ShamanSpecialization> _specialization;

    // Performance tracking
    uint32 _manaSpent;
    uint32 _damageDealt;
    uint32 _healingDone;
    uint32 _totemsDeploy;
    uint32 _shocksUsed;
    std::unordered_map<uint32, uint32> _abilityUsage;

    // Specialization management
    void InitializeSpecialization();
    ShamanSpec DetectCurrentSpecialization();
    void SwitchSpecialization(ShamanSpec newSpec);

    // Delegation to specialization
    void DelegateToSpecialization(::Unit* target);

    // Shared shaman utilities
    void UpdateShamanBuffs();
    void UpdateTotemCheck();
    void UpdateShockRotation();

    // Private helper methods
    void UpdateWeaponImbues();
    void UpdateUtilityBuffs();
    void DeployInitialTotems(::Unit* target);
    void RecallCombatTotems();
    void ApplyCombatBuffs();
    void LogCombatMetrics();
    bool IsShockSpell(uint32 spellId) const;
    bool IsTotemSpell(uint32 spellId) const;
    uint32 GetOptimalWeaponImbue(bool mainHand) const;
    bool HasWeaponImbue(bool mainHand) const;
    bool NearWater() const;
    bool ShouldUseBloodlust() const;
    Player* FindGroupTank(Group* group) const;
    uint32 CalculateDamageDealt(::Unit* target) const;
    uint32 CalculateHealingDone() const;
    uint32 CalculateManaUsage() const;
};

} // namespace Playerbot