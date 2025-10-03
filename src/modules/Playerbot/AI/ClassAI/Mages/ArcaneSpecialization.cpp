/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "ArcaneSpecialization.h"
#include "Player.h"
#include "Unit.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "SharedDefines.h"
#include "SpellAuras.h"
#include "ObjectAccessor.h"

namespace Playerbot
{

ArcaneSpecialization::ArcaneSpecialization(Player* bot)
    : MageSpecialization(bot)
    , _arcaneBlastStacks(0)
    , _lastArcaneSpellTime(0)
    , _inBurnPhase(false)
    , _inConservePhase(true)
    , _burnPhaseStartTime(0)
    , _conservePhaseStartTime(0)
    , _lastManaCheck(0)
    , _lastBuffCheck(0)
    , _lastRotationUpdate(0)
{
}

void ArcaneSpecialization::UpdateRotation(::Unit* target)
{
    if (!target || !_bot->IsAlive() || !target->IsAlive())
        return;

    uint32 currentTime = getMSTime();
    if (currentTime - _lastRotationUpdate < 100) // 100ms throttle
        return;
    _lastRotationUpdate = currentTime;

    // Update phases based on mana
    if (_inConservePhase && GetManaPercent() > BURN_ENTRY_THRESHOLD)
    {
        EnterBurnPhase();
    }
    else if (_inBurnPhase && GetManaPercent() < CONSERVE_EXIT_THRESHOLD)
    {
        EnterConservePhase();
    }

    // Handle burn phase rotation
    if (_inBurnPhase)
    {
        // Use cooldowns during burn phase
        if (!HasArcanePower() && CanUseAbility(ARCANE_POWER))
            CastArcanePower();

        if (!HasPresenceOfMind() && CanUseAbility(PRESENCE_OF_MIND))
            CastPresenceOfMind();

        // Arcane Blast stacking
        if (ShouldCastArcaneBlast())
        {
            CastArcaneBlast();
            return;
        }

        // Arcane Missiles on Clearcasting
        if (HasClearcastingProc() && ShouldCastArcaneMissiles())
        {
            CastArcaneMissiles();
            return;
        }

        // Arcane Barrage to finish combo
        if (ShouldCastArcaneBarrage())
        {
            CastArcaneBarrage();
            return;
        }
    }
    else // Conserve phase
    {
        // Use mana gems if needed
        if (ShouldUseManaGem())
        {
            UseManaGem();
            return;
        }

        // Conservative rotation
        if (HasClearcastingProc())
        {
            if (GetArcaneBlastStacks() < 2)
                CastArcaneBlast();
            else
                CastArcaneMissiles();
            return;
        }

        // Build to 2 stacks then barrage
        if (GetArcaneBlastStacks() < 2)
        {
            CastArcaneBlast();
        }
        else
        {
            CastArcaneBarrage();
        }
    }
}

void ArcaneSpecialization::UpdateBuffs()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastBuffCheck < 5000) // 5 second throttle
        return;
    _lastBuffCheck = currentTime;

    // Arcane Intellect
    if (!_bot->HasAura(ARCANE_INTELLECT))
    {
        if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(ARCANE_INTELLECT, DIFFICULTY_NONE))
        {
            _bot->CastSpell(_bot, ARCANE_INTELLECT, TRIGGERED_NONE);
        }
    }

    // Mana Shield if low health and high mana
    if (_bot->GetHealthPct() < 30.0f && GetManaPercent() > 50.0f)
    {
        if (!_bot->HasAura(MANA_SHIELD))
            CastManaShield();
    }

    CheckArcaneBuffs();
}

void ArcaneSpecialization::UpdateCooldowns(uint32 diff)
{
    // Update all cooldown timers
    for (auto& cooldown : _cooldowns)
    {
        if (cooldown.second > diff)
            cooldown.second -= diff;
        else
            cooldown.second = 0;
    }

    UpdateArcaneCooldowns(diff);
}

bool ArcaneSpecialization::CanUseAbility(uint32 spellId)
{
    if (!HasEnoughResource(spellId))
        return false;

    // Check cooldown
    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end() && it->second > 0)
        return false;

    return true;
}

void ArcaneSpecialization::OnCombatStart(::Unit* target)
{
    _arcaneBlastStacks = 0;
    _lastArcaneSpellTime = getMSTime();

    // Start in conserve phase
    EnterConservePhase();
}

void ArcaneSpecialization::OnCombatEnd()
{
    _arcaneBlastStacks = 0;
    _inBurnPhase = false;
    _inConservePhase = true;
    _cooldowns.clear();
}

bool ArcaneSpecialization::HasEnoughResource(uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return false;

    auto powerCosts = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask()); uint32 manaCost = 0; for (auto const& cost : powerCosts) { if (cost.Power == POWER_MANA) { manaCost = cost.Amount; break; } }

    // Apply Arcane Blast mana cost multiplier
    if (spellId == ARCANE_BLAST)
    {
        uint32 stacks = GetArcaneBlastStacks();
        manaCost += manaCost * stacks * 0.75f; // 175% per stack
    }

    return GetMana() >= manaCost;
}

void ArcaneSpecialization::ConsumeResource(uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return;

    auto powerCosts = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask()); uint32 manaCost = 0; for (auto const& cost : powerCosts) { if (cost.Power == POWER_MANA) { manaCost = cost.Amount; break; } }

    if (spellId == ARCANE_BLAST)
    {
        uint32 stacks = GetArcaneBlastStacks();
        manaCost += manaCost * stacks * 0.75f;
    }

    // Mana is consumed automatically by spell casting
}

Position ArcaneSpecialization::GetOptimalPosition(::Unit* target)
{
    if (!target)
        return _bot->GetPosition();

    float distance = GetOptimalRange(target);
    float angle = target->GetAbsoluteAngle(_bot);

    Position pos = target->GetNearPosition(distance, angle + M_PI); // Behind the target
    return pos;
}

float ArcaneSpecialization::GetOptimalRange(::Unit* target)
{
    return OPTIMAL_CASTING_RANGE;
}

// Private methods

void ArcaneSpecialization::UpdateArcaneCharges()
{
    // Update Arcane Blast stacks based on buff
    if (Aura* arcaneCharges = _bot->GetAura(ARCANE_CHARGES))
    {
        _arcaneBlastStacks = arcaneCharges->GetStackAmount();
    }
    else
    {
        _arcaneBlastStacks = 0;
    }
}

void ArcaneSpecialization::UpdateManaGems()
{
    // Check if we have mana gems in inventory
    // Implementation depends on inventory system
}

bool ArcaneSpecialization::ShouldConserveMana()
{
    return GetManaPercent() < MANA_CONSERVATION_THRESHOLD;
}

bool ArcaneSpecialization::ShouldUseManaGem()
{
    return GetManaPercent() < MANA_GEM_THRESHOLD && _inConservePhase;
}

uint32 ArcaneSpecialization::GetArcaneCharges()
{
    UpdateArcaneCharges();
    return _arcaneBlastStacks;
}

void ArcaneSpecialization::CastArcaneMissiles()
{
    if (CanUseAbility(ARCANE_MISSILES))
    {
        _bot->CastSpell(_bot->GetVictim(), ARCANE_MISSILES, TRIGGERED_NONE);
        _lastArcaneSpellTime = getMSTime();
    }
}

void ArcaneSpecialization::CastArcaneBlast()
{
    if (CanUseAbility(ARCANE_BLAST))
    {
        _bot->CastSpell(_bot->GetVictim(), ARCANE_BLAST, TRIGGERED_NONE);
        _lastArcaneSpellTime = getMSTime();

        // Increment stack count
        if (_arcaneBlastStacks < ARCANE_BLAST_MAX_STACKS)
            ++_arcaneBlastStacks;
    }
}

void ArcaneSpecialization::CastArcaneOrb()
{
    if (CanUseAbility(ARCANE_ORB))
    {
        _bot->CastSpell(_bot->GetVictim(), ARCANE_ORB, TRIGGERED_NONE);
        _lastArcaneSpellTime = getMSTime();
    }
}

void ArcaneSpecialization::CastArcaneBarrage()
{
    if (CanUseAbility(ARCANE_BARRAGE))
    {
        _bot->CastSpell(_bot->GetVictim(), ARCANE_BARRAGE, TRIGGERED_NONE);
        _lastArcaneSpellTime = getMSTime();
        _arcaneBlastStacks = 0; // Arcane Barrage consumes all charges
    }
}

void ArcaneSpecialization::CastPresenceOfMind()
{
    if (CanUseAbility(PRESENCE_OF_MIND))
    {
        _bot->CastSpell(_bot, PRESENCE_OF_MIND, TRIGGERED_NONE);
        _cooldowns[PRESENCE_OF_MIND] = 84000; // 84 second cooldown
    }
}

void ArcaneSpecialization::CastArcanePower()
{
    if (CanUseAbility(ARCANE_POWER))
    {
        _bot->CastSpell(_bot, ARCANE_POWER, TRIGGERED_NONE);
        _cooldowns[ARCANE_POWER] = 120000; // 2 minute cooldown
    }
}

void ArcaneSpecialization::CastManaShield()
{
    if (CanUseAbility(MANA_SHIELD))
    {
        _bot->CastSpell(_bot, MANA_SHIELD, TRIGGERED_NONE);
    }
}

void ArcaneSpecialization::UseManaGem()
{
    // Implementation depends on item usage system
    // For now, simulate mana restoration
    if (GetManaPercent() < 80.0f)
    {
        // Simulate mana gem usage
        uint32 manaRestore = GetMaxMana() * 0.25f; // 25% mana restoration
        // _bot->ModifyPower(POWER_MANA, manaRestore); // Would need proper API
    }
}

uint32 ArcaneSpecialization::GetArcaneBlastStacks()
{
    UpdateArcaneCharges();
    return _arcaneBlastStacks;
}

bool ArcaneSpecialization::ShouldCastArcaneBlast()
{
    uint32 stacks = GetArcaneBlastStacks();

    // Don't stack to 4 in conserve phase
    if (_inConservePhase && stacks >= 2)
        return false;

    // Always stack in burn phase
    if (_inBurnPhase && stacks < ARCANE_BLAST_MAX_STACKS)
        return true;

    return stacks < 2;
}

bool ArcaneSpecialization::ShouldCastArcaneBarrage()
{
    uint32 stacks = GetArcaneBlastStacks();

    // Use at max stacks or when transitioning
    if (stacks >= ARCANE_BLAST_MAX_STACKS)
        return true;

    // Use at 2+ stacks in conserve phase
    if (_inConservePhase && stacks >= 2)
        return true;

    return false;
}

bool ArcaneSpecialization::ShouldCastArcaneMissiles()
{
    return HasClearcastingProc() && GetArcaneBlastStacks() > 0;
}

void ArcaneSpecialization::UpdateArcaneCooldowns(uint32 diff)
{
    // Track phase durations
    if (_inBurnPhase)
    {
        _burnPhaseStartTime += diff;
        if (_burnPhaseStartTime >= OPTIMAL_BURN_DURATION)
        {
            EnterConservePhase();
        }
    }
    else if (_inConservePhase)
    {
        _conservePhaseStartTime += diff;
    }
}

void ArcaneSpecialization::CheckArcaneBuffs()
{
    // Check for beneficial auras
    UpdateArcaneCharges();
}

bool ArcaneSpecialization::HasClearcastingProc()
{
    return _bot->HasAura(CLEARCASTING);
}

bool ArcaneSpecialization::HasPresenceOfMind()
{
    return _bot->HasAura(PRESENCE_OF_MIND);
}

bool ArcaneSpecialization::HasArcanePower()
{
    return _bot->HasAura(ARCANE_POWER);
}

void ArcaneSpecialization::OptimizeManaUsage()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastManaCheck < 1000) // 1 second throttle
        return;
    _lastManaCheck = currentTime;

    float manaPercent = GetManaPercent();

    // Determine optimal phase based on mana
    if (manaPercent > BURN_ENTRY_THRESHOLD && _inConservePhase)
    {
        EnterBurnPhase();
    }
    else if (manaPercent < CONSERVE_EXIT_THRESHOLD && _inBurnPhase)
    {
        EnterConservePhase();
    }
}

bool ArcaneSpecialization::IsInBurnPhase()
{
    return _inBurnPhase;
}

bool ArcaneSpecialization::IsInConservePhase()
{
    return _inConservePhase;
}

void ArcaneSpecialization::EnterBurnPhase()
{
    _inBurnPhase = true;
    _inConservePhase = false;
    _burnPhaseStartTime = 0;
}

void ArcaneSpecialization::EnterConservePhase()
{
    _inBurnPhase = false;
    _inConservePhase = true;
    _conservePhaseStartTime = 0;
    _arcaneBlastStacks = 0; // Reset stacks when entering conserve
}

} // namespace Playerbot