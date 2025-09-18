/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "FrostSpecialization.h"
#include "Player.h"
#include "Unit.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "ObjectAccessor.h"

namespace Playerbot
{

FrostSpecialization::FrostSpecialization(Player* bot)
    : MageSpecialization(bot)
    , _icicleCount(0)
    , _hasFingersOfFrost(false)
    , _hasBrainFreeze(false)
    , _lastShatterTime(0)
    , _icyVeinsEndTime(0)
    , _inIcyVeins(false)
    , _waterElementalGuid(0)
    , _lastElementalCommand(0)
    , _lastFrozenCheck(0)
    , _lastKitingCheck(0)
    , _lastElementalCheck(0)
    , _lastDefensiveCheck(0)
    , _lastRotationUpdate(0)
    , _lastKiteTime(0)
    , _isKiting(false)
{
}

void FrostSpecialization::UpdateRotation(::Unit* target)
{
    if (!target || !_bot->IsAlive() || !target->IsAlive())
        return;

    uint32 currentTime = getMSTime();
    if (currentTime - _lastRotationUpdate < 100) // 100ms throttle
        return;
    _lastRotationUpdate = currentTime;

    // Update proc states
    UpdateIcicles();
    UpdateFrozenTargets();
    UpdateShatter();

    // Handle kiting if needed
    if (NeedsToKite(target))
    {
        HandleKiting(target);
    }

    // Check for AoE situations
    if (ShouldUseAoE())
    {
        HandleAoERotation();
        return;
    }

    // Use Icy Veins during optimal windows
    if (ShouldUseIcyVeins())
    {
        CastIcyVeins();
        return;
    }

    // Brain Freeze proc - cast Flurry
    if (HasBrainFreeze() && ShouldCastFlurry())
    {
        CastFlurry();
        return;
    }

    // Fingers of Frost proc - cast Ice Lance
    if (HasFingersOfFrost() && ShouldCastIceLance())
    {
        CastIceLance();
        return;
    }

    // Shatter combo on frozen targets
    if (IsTargetFrozen(target) && CanShatter(target))
    {
        ExecuteShatterCombo(target);
        return;
    }

    // Glacial Spike with max icicles
    if (HasMaxIcicles() && ShouldCastGlacialSpike())
    {
        CastGlacialSpike();
        return;
    }

    // Standard Frostbolt rotation
    if (ShouldCastFrostbolt())
    {
        CastFrostbolt();
        return;
    }

    // Backup - Ice Lance
    if (ShouldCastIceLance())
    {
        CastIceLance();
    }
}

void FrostSpecialization::UpdateBuffs()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastDefensiveCheck < 5000) // 5 second throttle
        return;
    _lastDefensiveCheck = currentTime;

    // Arcane Intellect
    if (!_bot->HasAura(ARCANE_INTELLECT))
    {
        if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(ARCANE_INTELLECT))
        {
            _bot->CastSpell(_bot, ARCANE_INTELLECT, false);
        }
    }

    // Ice Barrier
    if (ShouldUseIceBarrier())
    {
        CastIceBarrier();
    }

    // Water Elemental management
    UpdateWaterElemental();

    CheckFrostBuffs();
    UpdateDefensiveSpells();
}

void FrostSpecialization::UpdateCooldowns(uint32 diff)
{
    // Update all cooldown timers
    for (auto& cooldown : _cooldowns)
    {
        if (cooldown.second > diff)
            cooldown.second -= diff;
        else
            cooldown.second = 0;
    }

    // Update frozen target timers
    for (auto& frozenTimer : _frozenTargets)
    {
        if (frozenTimer.second > diff)
            frozenTimer.second -= diff;
        else
            frozenTimer.second = 0;
    }

    // Update slowed target timers
    for (auto& slowTimer : _slowedTargets)
    {
        if (slowTimer.second > diff)
            slowTimer.second -= diff;
        else
            slowTimer.second = 0;
    }

    UpdateFrostCooldowns(diff);
}

bool FrostSpecialization::CanUseAbility(uint32 spellId)
{
    if (!HasEnoughResource(spellId))
        return false;

    // Check cooldown
    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end() && it->second > 0)
        return false;

    return true;
}

void FrostSpecialization::OnCombatStart(::Unit* target)
{
    _icicleCount = 0;
    _hasFingersOfFrost = false;
    _hasBrainFreeze = false;
    _lastShatterTime = 0;
    _inIcyVeins = false;
    _isKiting = false;
    _frozenTargets.clear();
    _slowedTargets.clear();

    // Summon Water Elemental
    SummonWaterElementalIfNeeded();
}

void FrostSpecialization::OnCombatEnd()
{
    _icicleCount = 0;
    _hasFingersOfFrost = false;
    _hasBrainFreeze = false;
    _inIcyVeins = false;
    _isKiting = false;
    _cooldowns.clear();
    _frozenTargets.clear();
    _slowedTargets.clear();
}

bool FrostSpecialization::HasEnoughResource(uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return false;

    uint32 manaCost = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask());
    return GetMana() >= manaCost;
}

void FrostSpecialization::ConsumeResource(uint32 spellId)
{
    // Mana is consumed automatically by spell casting
}

Position FrostSpecialization::GetOptimalPosition(::Unit* target)
{
    if (!target)
        return _bot->GetPosition();

    float distance = GetOptimalRange(target);

    // If kiting, maintain greater distance
    if (_isKiting || NeedsToKite(target))
    {
        distance = KITING_DISTANCE;
    }

    float angle = _bot->GetAngle(target);
    Position pos;
    target->GetNearPosition(pos, distance, angle + M_PI);
    return pos;
}

float FrostSpecialization::GetOptimalRange(::Unit* target)
{
    if (NeedsToKite(target))
        return KITING_DISTANCE;

    return OPTIMAL_CASTING_RANGE;
}

// Private methods

void FrostSpecialization::UpdateIcicles()
{
    if (Aura* icicles = _bot->GetAura(ICICLES))
    {
        _icicleCount = icicles->GetStackAmount();
    }
    else
    {
        _icicleCount = 0;
    }
}

void FrostSpecialization::UpdateFrozenTargets()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastFrozenCheck < 1000) // 1 second throttle
        return;
    _lastFrozenCheck = currentTime;

    // Update frozen states - implementation depends on debuff checking
    ::Unit* target = _bot->GetVictim();
    if (target)
    {
        // Check if target has freeze effects
        if (target->HasAuraType(SPELL_AURA_MOD_ROOT) ||
            target->HasAuraType(SPELL_AURA_MOD_STUN))
        {
            _frozenTargets[target->GetGUID().GetCounter()] = currentTime + 8000; // 8 second duration
        }
    }
}

void FrostSpecialization::UpdateShatter()
{
    _hasFingersOfFrost = _bot->HasAura(FINGERS_OF_FROST);
    _hasBrainFreeze = _bot->HasAura(BRAIN_FREEZE);
}

bool FrostSpecialization::HasFingersOfFrost()
{
    return _hasFingersOfFrost;
}

bool FrostSpecialization::HasBrainFreeze()
{
    return _hasBrainFreeze;
}

bool FrostSpecialization::IsTargetFrozen(::Unit* target)
{
    if (!target)
        return false;

    auto it = _frozenTargets.find(target->GetGUID().GetCounter());
    return it != _frozenTargets.end() && it->second > getMSTime();
}

bool FrostSpecialization::CanShatter(::Unit* target)
{
    if (!target)
        return false;

    uint32 currentTime = getMSTime();

    // Shatter window check
    if (currentTime - _lastShatterTime < SHATTER_WINDOW)
        return false;

    return IsTargetFrozen(target) || HasFingersOfFrost();
}

uint32 FrostSpecialization::GetIcicleCount()
{
    return _icicleCount;
}

bool FrostSpecialization::ShouldCastFrostbolt()
{
    // Don't cast if we have procs to use
    if (HasFingersOfFrost() || HasBrainFreeze())
        return false;

    // Don't cast if moving and we have instant options
    if (_bot->isMoving() && (HasFingersOfFrost() || HasBrainFreeze()))
        return false;

    return CanUseAbility(FROSTBOLT);
}

bool FrostSpecialization::ShouldCastIceLance()
{
    // Cast on Fingers of Frost proc
    if (HasFingersOfFrost())
        return true;

    // Cast on frozen targets
    ::Unit* target = _bot->GetVictim();
    if (target && IsTargetFrozen(target))
        return true;

    // Cast while moving
    if (_bot->isMoving())
        return true;

    return false;
}

bool FrostSpecialization::ShouldCastFlurry()
{
    return HasBrainFreeze() && CanUseAbility(FLURRY);
}

bool FrostSpecialization::ShouldCastGlacialSpike()
{
    return HasMaxIcicles() && CanUseAbility(GLACIAL_SPIKE);
}

bool FrostSpecialization::ShouldCastConeOfCold()
{
    ::Unit* target = _bot->GetVictim();
    if (!target)
        return false;

    float distance = _bot->GetDistance(target);
    return distance <= CONE_OF_COLD_RANGE && CanUseAbility(CONE_OF_COLD);
}

bool FrostSpecialization::ShouldCastBlizzard()
{
    return ShouldUseAoE() && CanUseAbility(BLIZZARD);
}

bool FrostSpecialization::ShouldUseIcyVeins()
{
    if (!CanUseAbility(ICY_VEINS))
        return false;

    // Use during burst windows
    return GetManaPercent() > 70.0f || HasMaxIcicles();
}

void FrostSpecialization::CastFrostbolt()
{
    if (CanUseAbility(FROSTBOLT))
    {
        _bot->CastSpell(_bot->GetVictim(), FROSTBOLT, false);
        BuildIcicles();
    }
}

void FrostSpecialization::CastIceLance()
{
    if (CanUseAbility(ICE_LANCE))
    {
        _bot->CastSpell(_bot->GetVictim(), ICE_LANCE, false);
        LaunchIcicles();
    }
}

void FrostSpecialization::CastFlurry()
{
    if (CanUseAbility(FLURRY))
    {
        _bot->CastSpell(_bot->GetVictim(), FLURRY, false);

        // Flurry applies Winter's Chill (frozen effect)
        ::Unit* target = _bot->GetVictim();
        if (target)
        {
            _frozenTargets[target->GetGUID().GetCounter()] = getMSTime() + 2000; // 2 second freeze
        }
    }
}

void FrostSpecialization::CastGlacialSpike()
{
    if (CanUseAbility(GLACIAL_SPIKE))
    {
        _bot->CastSpell(_bot->GetVictim(), GLACIAL_SPIKE, false);
        _icicleCount = 0; // Consumes all icicles
    }
}

void FrostSpecialization::CastConeOfCold()
{
    if (CanUseAbility(CONE_OF_COLD))
    {
        _bot->CastSpell(_bot, CONE_OF_COLD, false);
        _cooldowns[CONE_OF_COLD] = 12000; // 12 second cooldown
    }
}

void FrostSpecialization::CastBlizzard()
{
    if (CanUseAbility(BLIZZARD))
    {
        ::Unit* target = _bot->GetVictim();
        if (target)
        {
            _bot->CastSpell(target, BLIZZARD, false);
        }
    }
}

void FrostSpecialization::CastFrostNova()
{
    if (CanUseAbility(FROST_NOVA))
    {
        _bot->CastSpell(_bot, FROST_NOVA, false);
        _cooldowns[FROST_NOVA] = 25000; // 25 second cooldown
    }
}

void FrostSpecialization::CastIceBarrier()
{
    if (CanUseAbility(ICE_BARRIER))
    {
        _bot->CastSpell(_bot, ICE_BARRIER, false);
        _cooldowns[ICE_BARRIER] = 25000; // 25 second cooldown
    }
}

void FrostSpecialization::CastIcyVeins()
{
    if (CanUseAbility(ICY_VEINS))
    {
        _bot->CastSpell(_bot, ICY_VEINS, false);
        _cooldowns[ICY_VEINS] = 180000; // 3 minute cooldown
        _inIcyVeins = true;
        _icyVeinsEndTime = getMSTime() + ICY_VEINS_DURATION;
    }
}

void FrostSpecialization::CastSummonWaterElemental()
{
    if (CanUseAbility(SUMMON_WATER_ELEMENTAL))
    {
        _bot->CastSpell(_bot, SUMMON_WATER_ELEMENTAL, false);
        _cooldowns[SUMMON_WATER_ELEMENTAL] = 30000; // 30 second cooldown
    }
}

void FrostSpecialization::LaunchIcicles()
{
    // Icicles are launched automatically with Ice Lance
    // This method tracks the mechanic
    if (_icicleCount > 0)
    {
        // Simulate icicle damage
        _icicleCount = std::max(0, static_cast<int>(_icicleCount) - 1);
    }
}

void FrostSpecialization::BuildIcicles()
{
    // Icicles are built automatically with Frostbolt
    if (_icicleCount < MAX_ICICLES)
    {
        ++_icicleCount;
    }
}

bool FrostSpecialization::HasMaxIcicles()
{
    return _icicleCount >= MAX_ICICLES;
}

void FrostSpecialization::HandleKiting(::Unit* target)
{
    if (!target)
        return;

    uint32 currentTime = getMSTime();
    if (currentTime - _lastKitingCheck < 500) // 500ms throttle
        return;
    _lastKitingCheck = currentTime;

    float distance = _bot->GetDistance(target);

    // If too close, move away
    if (distance < KITING_DISTANCE)
    {
        Position kitePos = GetOptimalPosition(target);
        _bot->GetMotionMaster()->MovePoint(0, kitePos);
        _isKiting = true;
        _lastKiteTime = currentTime;

        // Cast Frost Nova to freeze pursuers
        if (distance < MELEE_RANGE && CanUseAbility(FROST_NOVA))
        {
            CastFrostNova();
        }
    }
    else
    {
        _isKiting = false;
    }

    // Apply slowing effects
    ApplySlows(target);
}

void FrostSpecialization::ApplySlows(::Unit* target)
{
    if (!target)
        return;

    // Cast slowing spells
    CastSlowingSpells(target);
}

bool FrostSpecialization::NeedsToKite(::Unit* target)
{
    if (!target)
        return false;

    // Check if target is in melee range and we're squishy
    float distance = _bot->GetDistance(target);
    return distance < KITING_DISTANCE && _bot->GetHealthPct() < 80.0f;
}

float FrostSpecialization::GetKitingDistance(::Unit* target)
{
    return KITING_DISTANCE;
}

void FrostSpecialization::CastSlowingSpells(::Unit* target)
{
    if (!target)
        return;

    // Frostbolt naturally slows
    // Cone of Cold for close enemies
    if (_bot->GetDistance(target) <= CONE_OF_COLD_RANGE && CanUseAbility(CONE_OF_COLD))
    {
        CastConeOfCold();
    }
}

void FrostSpecialization::HandleAoERotation()
{
    std::vector<::Unit*> enemies = GetFrozenEnemies();

    if (enemies.size() >= 3)
    {
        // Use Blizzard for multiple enemies
        if (CanUseAbility(BLIZZARD))
        {
            CastBlizzard();
            return;
        }

        // Frozen Orb for AoE
        if (CanUseAbility(FROZEN_ORB))
        {
            CastFrozenOrb();
            return;
        }

        // Cone of Cold for close enemies
        if (CanUseAbility(CONE_OF_COLD))
        {
            CastConeOfCold();
            return;
        }
    }

    // Fall back to single target
    UpdateRotation(_bot->GetVictim());
}

std::vector<::Unit*> FrostSpecialization::GetFrozenEnemies()
{
    std::vector<::Unit*> enemies;
    // Implementation depends on TrinityCore's enemy detection system
    return enemies;
}

bool FrostSpecialization::ShouldUseAoE()
{
    std::vector<::Unit*> enemies = GetFrozenEnemies();
    return enemies.size() >= 3;
}

void FrostSpecialization::CastFrozenOrb()
{
    if (CanUseAbility(FROZEN_ORB))
    {
        ::Unit* target = _bot->GetVictim();
        if (target)
        {
            _bot->CastSpell(target, FROZEN_ORB, false);
            _cooldowns[FROZEN_ORB] = 60000; // 60 second cooldown
        }
    }
}

void FrostSpecialization::CastIceNova()
{
    if (CanUseAbility(ICE_NOVA))
    {
        ::Unit* target = _bot->GetVictim();
        if (target)
        {
            _bot->CastSpell(target, ICE_NOVA, false);
            _cooldowns[ICE_NOVA] = 25000; // 25 second cooldown
        }
    }
}

void FrostSpecialization::UpdateWaterElemental()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastElementalCheck < 5000) // 5 second throttle
        return;
    _lastElementalCheck = currentTime;

    if (!HasWaterElemental())
    {
        SummonWaterElementalIfNeeded();
    }
    else
    {
        CommandWaterElemental();
    }
}

void FrostSpecialization::CommandWaterElemental()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastElementalCommand < 2000) // 2 second throttle
        return;
    _lastElementalCommand = currentTime;

    // Command water elemental to use abilities
    // Implementation depends on pet command system
}

bool FrostSpecialization::HasWaterElemental()
{
    // Check if water elemental is active
    // Implementation depends on pet system
    return false; // Placeholder
}

void FrostSpecialization::SummonWaterElementalIfNeeded()
{
    if (!HasWaterElemental() && CanUseAbility(SUMMON_WATER_ELEMENTAL))
    {
        CastSummonWaterElemental();
    }
}

void FrostSpecialization::UpdateDefensiveSpells()
{
    // Ice Barrier when taking damage
    if (_bot->GetHealthPct() < 70.0f && ShouldUseIceBarrier())
    {
        CastIceBarrier();
    }

    // Ice Block in emergencies
    if (_bot->GetHealthPct() < 20.0f && ShouldUseIceBlock())
    {
        UseIceBlock();
    }
}

void FrostSpecialization::CastDefensiveSpells()
{
    UpdateDefensiveSpells();
}

bool FrostSpecialization::ShouldUseIceBlock()
{
    return _bot->GetHealthPct() < 20.0f && CanUseAbility(ICE_BLOCK);
}

bool FrostSpecialization::ShouldUseIceBarrier()
{
    return _bot->GetHealthPct() < 70.0f && CanUseAbility(ICE_BARRIER) && !_bot->HasAura(ICE_BARRIER);
}

void FrostSpecialization::UpdateFrostCooldowns(uint32 diff)
{
    // Update Icy Veins state
    if (_inIcyVeins && getMSTime() >= _icyVeinsEndTime)
    {
        _inIcyVeins = false;
        _icyVeinsEndTime = 0;
    }
}

void FrostSpecialization::CheckFrostBuffs()
{
    UpdateIcicles();
    UpdateShatter();
}

void FrostSpecialization::UseCooldowns()
{
    // Use Icy Veins offensively
    if (ShouldUseIcyVeins())
    {
        CastIcyVeins();
    }
}

void FrostSpecialization::ExecuteShatterCombo(::Unit* target)
{
    if (!target)
        return;

    _lastShatterTime = getMSTime();

    // Cast Ice Lance for shatter damage
    if (CanShatter(target) && ShouldCastIceLance())
    {
        CastIceLance();
    }
}

void FrostSpecialization::SetupShatter(::Unit* target)
{
    if (!target)
        return;

    // Use Flurry to freeze target
    if (HasBrainFreeze() && ShouldCastFlurry())
    {
        CastFlurry();
    }

    // Use Frost Nova for area freeze
    if (_bot->GetDistance(target) <= MELEE_RANGE && CanUseAbility(FROST_NOVA))
    {
        CastFrostNova();
    }
}

bool FrostSpecialization::IsShatterReady(::Unit* target)
{
    return IsTargetFrozen(target) || HasFingersOfFrost();
}

} // namespace Playerbot