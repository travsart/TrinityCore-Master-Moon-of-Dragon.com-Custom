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
#include <chrono>
#include <array>

// Forward declarations
class ElementalSpecialization;
class EnhancementSpecialization;
class RestorationSpecialization;

namespace Playerbot
{

// Totem types for management
enum class TotemType : uint8
{
    FIRE = 0,
    EARTH = 1,
    WATER = 2,
    AIR = 3,
    MAX = 4
};

// Totem tracking structure
struct TotemInfo
{
    uint32 spellId;
    uint32 deployTime;
    Position position;
    bool isActive;

    TotemInfo() : spellId(0), deployTime(0), isActive(false) {}
};

// Shaman AI implementation with specialization pattern and CombatBehaviorIntegration
class TC_GAME_API ShamanAI : public ClassAI
{
public:
    explicit ShamanAI(Player* bot);
    ~ShamanAI(); // Implemented in cpp file

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

    // Totem management system
    std::array<TotemInfo, static_cast<size_t>(TotemType::MAX)> _activeTotems;
    uint32 _lastTotemUpdate;
    uint32 _lastTotemCheck;

    // Shock management
    uint32 _lastShockTime;
    uint32 _flameshockTarget;
    uint32 _flameshockExpiry;

    // Maelstrom/resource tracking
    uint32 _maelstromWeaponStacks;
    uint32 _lastMaelstromCheck;
    uint32 _elementalMaelstrom;

    // Cooldown tracking
    uint32 _lastWindShear;
    uint32 _lastBloodlust;
    uint32 _lastElementalMastery;
    uint32 _lastAscendance;
    uint32 _lastFireElemental;
    uint32 _lastEarthElemental;
    uint32 _lastSpiritWalk;
    uint32 _lastShamanisticRage;

    // Combat state
    bool _hasFlameShockUp;
    uint32 _lavaBurstCharges;
    bool _hasLavaSurgeProc;
    uint32 _healingStreamTotemTime;
    uint32 _chainHealBounceCount;

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

    // CombatBehaviorIntegration rotation priorities
    bool HandleInterrupts(::Unit* target);
    bool HandleDefensives();
    bool HandlePositioning(::Unit* target);
    bool HandleTotemManagement(::Unit* target);
    bool HandleTargetSwitching(::Unit* target);
    bool HandlePurgeDispel(::Unit* target);
    bool HandleAoEDecisions(::Unit* target);
    bool HandleOffensiveCooldowns(::Unit* target);
    bool HandleResourceManagement();
    bool HandleNormalRotation(::Unit* target);

    // Totem management helpers
    bool NeedsTotemRefresh(TotemType type) const;
    uint32 GetOptimalTotem(TotemType type, ::Unit* target) const;
    bool DeployTotem(uint32 spellId, TotemType type);
    void UpdateTotemPositions();
    bool IsTotemInRange(TotemType type, ::Unit* target) const;
    void RecallTotem(TotemType type);

    // Elemental-specific helpers
    bool UpdateElementalRotation(::Unit* target);
    bool HandleLavaBurst(::Unit* target);
    bool HandleFlameShock(::Unit* target);
    bool HandleChainLightning(::Unit* target);
    bool HandleEarthquake();
    bool HandleElementalBlast(::Unit* target);
    bool CheckLavaSurgeProc();
    uint32 GetElementalMaelstrom() const;

    // Enhancement-specific helpers
    bool UpdateEnhancementRotation(::Unit* target);
    bool HandleStormstrike(::Unit* target);
    bool HandleLavaLash(::Unit* target);
    bool HandleCrashLightning();
    bool HandleWindstrike(::Unit* target);
    bool HandleMaelstromWeapon();
    uint32 GetMaelstromWeaponStacks() const;
    bool ShouldUseInstantLightningBolt() const;

    // Restoration-specific helpers
    bool UpdateRestorationRotation(::Unit* target);
    bool HandleChainHeal();
    bool HandleRiptide(Player* target);
    bool HandleHealingWave(Player* target);
    bool HandleHealingRain();
    bool HandleHealingStreamTotem();
    bool HandleSpiritLink();
    Player* GetLowestHealthGroupMember() const;
    uint32 CountInjuredGroupMembers(float healthThreshold = 80.0f) const;

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
    bool IsInMeleeRange(::Unit* target) const;
    bool HasFlameShockOnTarget(::Unit* target) const;
    void RefreshFlameShock(::Unit* target);
    bool ShouldUseAscendance() const;
    bool ShouldUseElementalMastery() const;
};

} // namespace Playerbot