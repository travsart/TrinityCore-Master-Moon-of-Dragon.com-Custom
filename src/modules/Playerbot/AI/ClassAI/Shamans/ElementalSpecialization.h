/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "ShamanSpecialization.h"
#include <vector>
#include <map>

namespace Playerbot
{

class TC_GAME_API ElementalSpecialization : public ShamanSpecialization
{
public:
    explicit ElementalSpecialization(Player* bot);

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

    // Totem management
    void UpdateTotemManagement() override;
    void DeployOptimalTotems() override;
    uint32 GetOptimalFireTotem() override;
    uint32 GetOptimalEarthTotem() override;
    uint32 GetOptimalWaterTotem() override;
    uint32 GetOptimalAirTotem() override;

    // Shock rotation
    void UpdateShockRotation(::Unit* target) override;
    uint32 GetNextShockSpell(::Unit* target) override;

    // Specialization info
    ShamanSpec GetSpecialization() const override { return ShamanSpec::ELEMENTAL; }
    const char* GetSpecializationName() const override { return "Elemental"; }

private:
    // Elemental-specific mechanics
    void UpdateElementalFocus();
    void UpdateLavaBurst();
    void UpdateThunderstorm();
    void UpdateElementalMastery();
    bool ShouldCastLightningBolt(::Unit* target);
    bool ShouldCastChainLightning(const std::vector<::Unit*>& enemies);
    bool ShouldCastLavaBurst(::Unit* target);
    bool ShouldCastElementalBlast(::Unit* target);
    bool ShouldCastThunderstorm();

    // Spell rotation
    void CastLightningBolt(::Unit* target);
    void CastChainLightning(const std::vector<::Unit*>& enemies);
    void CastLavaBurst(::Unit* target);
    void CastElementalBlast(::Unit* target);
    void CastThunderstorm();

    // Target selection
    ::Unit* GetBestElementalTarget();
    std::vector<::Unit*> GetChainLightningTargets(::Unit* primary);
    ::Unit* GetBestShockTarget();

    // Mana management for casters
    void ManageMana();
    void UseTotemOfWrath();
    void UseManaSpringTotem();
    bool ShouldConserveMana();

    // Elemental focus and clearcast management
    void ManageElementalFocus();
    void TriggerClearcastingProc();
    bool HasClearcasting();

    // Lightning shield management
    void UpdateLightningShield();
    void RefreshLightningShield();
    uint32 GetLightningShieldCharges();

    // Positioning for elemental
    void UpdateElementalPositioning();
    bool IsAtOptimalCastingRange(::Unit* target);
    Position GetSafeCastingPosition(::Unit* target);

    // Elemental spell IDs
    enum ElementalSpells
    {
        LIGHTNING_BOLT = 403,
        CHAIN_LIGHTNING = 421,
        LAVA_BURST = 51505,
        ELEMENTAL_BLAST = 117014,
        THUNDERSTORM = 51490,
        ELEMENTAL_MASTERY = 16166,
        LAVA_LASH = 60103,
        CLEARCASTING = 16246
    };

    // State tracking
    uint32 _lightningShieldCharges;
    uint32 _elementalFocusStacks;
    uint32 _clearcastingProcs;
    uint32 _lastLightningBolt;
    uint32 _lastChainLightning;
    uint32 _lastLavaBurst;
    uint32 _lastElementalBlast;
    uint32 _lastThunderstorm;

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;

    // Target tracking
    ::Unit* _primaryTarget;
    std::vector<::Unit*> _chainTargets;

    // Performance tracking
    uint32 _totalDamageDealt;
    uint32 _manaSpent;
    uint32 _spellsCast;

    // Constants
    static constexpr float OPTIMAL_CASTING_RANGE = 30.0f;
    static constexpr float CHAIN_LIGHTNING_RANGE = 25.0f;
    static constexpr float THUNDERSTORM_RANGE = 10.0f;
    static constexpr uint32 ELEMENTAL_FOCUS_MAX_STACKS = 3;
    static constexpr uint32 LIGHTNING_SHIELD_MAX_CHARGES = 3;
    static constexpr float MANA_CONSERVATION_THRESHOLD = 0.3f;
    static constexpr uint32 CHAIN_LIGHTNING_MIN_TARGETS = 3;
};

} // namespace Playerbot