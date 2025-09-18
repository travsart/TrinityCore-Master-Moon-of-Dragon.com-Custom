/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "ArmsSpecialization.h"
#include "Player.h"
#include "Unit.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "ObjectAccessor.h"

namespace Playerbot
{

ArmsSpecialization::ArmsSpecialization(Player* bot)
    : WarriorSpecialization(bot)
    , _preferredStance(WarriorStance::BATTLE)
    , _lastMortalStrike(0)
    , _lastColossusSmash(0)
    , _lastOverpower(0)
    , _overpowerReady(false)
    , _suddenDeathProc(false)
    , _lastStanceCheck(0)
    , _lastWeaponCheck(0)
    , _lastRotationUpdate(0)
    , _inExecutePhase(false)
    , _executePhaseStartTime(0)
{
}

void ArmsSpecialization::UpdateRotation(::Unit* target)
{
    if (!target || !_bot->IsAlive() || !target->IsAlive())
        return;

    uint32 currentTime = getMSTime();
    if (currentTime - _lastRotationUpdate < 100) // 100ms throttle
        return;
    _lastRotationUpdate = currentTime;

    // Update mechanics
    UpdateMortalStrike();
    UpdateOverpower();
    UpdateDeepWounds();

    // Stance management
    UpdateStance();

    // Check for execute phase
    if (IsInExecutePhase(target))
    {
        HandleExecutePhase(target);
        return;
    }

    // Use major cooldowns when appropriate
    if (ShouldUseBladestorm())
    {
        UseBladestorm();
        return;
    }

    if (ShouldUseAvatar())
    {
        UseAvatar();
        return;
    }

    // Colossus Smash for debuff
    if (ShouldCastColossusSmash(target))
    {
        CastColossusSmash(target);
        return;
    }

    // Mortal Strike as priority
    if (ShouldCastMortalStrike(target))
    {
        CastMortalStrike(target);
        return;
    }

    // Overpower when available
    if (ShouldCastOverpower(target))
    {
        CastOverpower(target);
        return;
    }

    // War Breaker for AoE situations
    if (ShouldCastWarBreaker(target))
    {
        CastWarBreaker(target);
        return;
    }

    // Whirlwind for AoE
    if (CanUseAbility(WHIRLWIND))
    {
        CastWhirlwind();
        return;
    }

    // Rend as filler if low rage
    if (GetRagePercent() < 50.0f && CanUseAbility(REND))
    {
        CastRend(target);
        return;
    }

    // Heroic Strike as rage dump
    if (GetRagePercent() > RAGE_DUMP_THRESHOLD && CanUseAbility(HEROIC_STRIKE))
    {
        // Heroic Strike implementation would be handled by base class
    }
}

void ArmsSpecialization::UpdateBuffs()
{
    // Battle Shout
    if (!_bot->HasAura(BATTLE_SHOUT) && !_bot->HasAura(COMMANDING_SHOUT))
    {
        if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(BATTLE_SHOUT))
        {
            _bot->CastSpell(_bot, BATTLE_SHOUT, false);
        }
    }

    // Sweeping Strikes for AoE situations
    std::vector<::Unit*> nearbyEnemies; // Would be populated by enemy detection
    if (nearbyEnemies.size() > 1 && !_bot->HasAura(SWEEPING_STRIKES))
    {
        CastSweepingStrikes();
    }

    OptimizeTwoHandedWeapon();
}

void ArmsSpecialization::UpdateCooldowns(uint32 diff)
{
    // Update all cooldown timers
    for (auto& cooldown : _cooldowns)
    {
        if (cooldown.second > diff)
            cooldown.second -= diff;
        else
            cooldown.second = 0;
    }

    // Update Deep Wounds timers
    for (auto& timer : _deepWoundsTimers)
    {
        if (timer.second > diff)
            timer.second -= diff;
        else
            timer.second = 0;
    }

    UpdateArmsCooldowns(diff);
}

bool ArmsSpecialization::CanUseAbility(uint32 spellId)
{
    if (!HasEnoughResource(spellId))
        return false;

    // Check cooldown
    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end() && it->second > 0)
        return false;

    // Stance requirements
    WarriorStance currentStance = GetCurrentStance();
    switch (spellId)
    {
        case MORTAL_STRIKE:
        case OVERPOWER:
        case EXECUTE:
            return currentStance == WarriorStance::BATTLE || currentStance == WarriorStance::BERSERKER;
        case DEFENSIVE_STANCE:
            return currentStance != WarriorStance::DEFENSIVE;
        default:
            break;
    }

    return true;
}

void ArmsSpecialization::OnCombatStart(::Unit* target)
{
    _overpowerReady = false;
    _suddenDeathProc = false;
    _inExecutePhase = false;
    _deepWoundsTimers.clear();

    // Switch to optimal stance
    WarriorStance optimalStance = GetOptimalStance(target);
    if (GetCurrentStance() != optimalStance)
    {
        SwitchStance(optimalStance);
    }
}

void ArmsSpecialization::OnCombatEnd()
{
    _overpowerReady = false;
    _suddenDeathProc = false;
    _inExecutePhase = false;
    _cooldowns.clear();
    _deepWoundsTimers.clear();
}

bool ArmsSpecialization::HasEnoughResource(uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return false;

    uint32 rageCost = 0;
    switch (spellId)
    {
        case MORTAL_STRIKE:
            rageCost = MORTAL_STRIKE_RAGE_COST;
            break;
        case EXECUTE:
            rageCost = EXECUTE_RAGE_COST;
            break;
        case COLOSSUS_SMASH:
            rageCost = 20;
            break;
        case OVERPOWER:
            rageCost = 5;
            break;
        case WHIRLWIND:
            rageCost = 25;
            break;
        default:
            rageCost = 10; // Default rage cost
            break;
    }

    return GetRage() >= rageCost;
}

void ArmsSpecialization::ConsumeResource(uint32 spellId)
{
    // Rage is consumed automatically by spell casting
}

Position ArmsSpecialization::GetOptimalPosition(::Unit* target)
{
    if (!target)
        return _bot->GetPosition();

    // Stay in melee range
    float distance = OPTIMAL_MELEE_RANGE;
    Position pos = target->GetPosition();
    pos.m_positionX += distance;
    return pos;
}

float ArmsSpecialization::GetOptimalRange(::Unit* target)
{
    return OPTIMAL_MELEE_RANGE;
}

void ArmsSpecialization::UpdateStance()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastStanceCheck < 2000) // 2 second throttle
        return;
    _lastStanceCheck = currentTime;

    WarriorStance currentStance = GetCurrentStance();
    WarriorStance optimalStance = GetOptimalStance(_bot->GetVictim());

    if (currentStance != optimalStance)
    {
        SwitchStance(optimalStance);
    }
}

WarriorStance ArmsSpecialization::GetOptimalStance(::Unit* target)
{
    if (!target)
        return WarriorStance::BATTLE;

    // Use defensive stance if low health
    if (_bot->GetHealthPct() < 30.0f)
        return WarriorStance::DEFENSIVE;

    // Use berserker stance for execute phase
    if (IsInExecutePhase(target))
        return WarriorStance::BERSERKER;

    // Default to battle stance for Arms
    return WarriorStance::BATTLE;
}

void ArmsSpecialization::SwitchStance(WarriorStance stance)
{
    WarriorStance currentStance = GetCurrentStance();
    if (currentStance == stance)
        return;

    uint32 stanceSpellId = 0;
    switch (stance)
    {
        case WarriorStance::BATTLE:
            stanceSpellId = BATTLE_STANCE;
            break;
        case WarriorStance::DEFENSIVE:
            stanceSpellId = DEFENSIVE_STANCE;
            break;
        case WarriorStance::BERSERKER:
            stanceSpellId = BERSERKER_STANCE;
            break;
        default:
            return;
    }

    if (CanUseAbility(stanceSpellId))
    {
        _bot->CastSpell(_bot, stanceSpellId, false);
    }
}

// Private methods

void ArmsSpecialization::UpdateMortalStrike()
{
    // Mortal Strike tracking and optimization
}

void ArmsSpecialization::UpdateOverpower()
{
    // Check for Overpower procs (after dodges/parries)
    // This would be handled by combat log events in real implementation
}

void ArmsSpecialization::UpdateDeepWounds()
{
    // Deep Wounds DoT tracking
}

void ArmsSpecialization::UpdateTacticalMastery()
{
    // Tactical Mastery allows retaining rage when switching stances
}

bool ArmsSpecialization::ShouldCastMortalStrike(::Unit* target)
{
    return target && CanUseAbility(MORTAL_STRIKE);
}

bool ArmsSpecialization::ShouldCastOverpower(::Unit* target)
{
    return target && _overpowerReady && CanUseAbility(OVERPOWER);
}

bool ArmsSpecialization::ShouldCastExecute(::Unit* target)
{
    return target && IsInExecutePhase(target) && CanUseAbility(EXECUTE);
}

bool ArmsSpecialization::ShouldCastColossusSmash(::Unit* target)
{
    return target && CanUseAbility(COLOSSUS_SMASH);
}

bool ArmsSpecialization::ShouldCastWarBreaker(::Unit* target)
{
    // Use War Breaker in AoE situations or for debuff
    return target && CanUseAbility(WAR_BREAKER);
}

void ArmsSpecialization::OptimizeTwoHandedWeapon()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastWeaponCheck < 5000) // 5 second throttle
        return;
    _lastWeaponCheck = currentTime;

    // Check if using optimal two-handed weapon
    // Implementation would check equipped weapon type
}

void ArmsSpecialization::UpdateWeaponMastery()
{
    // Two-handed weapon specialization bonuses
}

bool ArmsSpecialization::HasTwoHandedWeapon()
{
    // Check if player has two-handed weapon equipped
    return true; // Placeholder
}

void ArmsSpecialization::CastSweepingStrikes()
{
    if (CanUseAbility(SWEEPING_STRIKES))
    {
        _bot->CastSpell(_bot, SWEEPING_STRIKES, false);
        _cooldowns[SWEEPING_STRIKES] = 30000; // 30 second cooldown
    }
}

void ArmsSpecialization::CastMortalStrike(::Unit* target)
{
    if (target && CanUseAbility(MORTAL_STRIKE))
    {
        _bot->CastSpell(target, MORTAL_STRIKE, false);
        _lastMortalStrike = getMSTime();
        _cooldowns[MORTAL_STRIKE] = 6000; // 6 second cooldown

        // Apply Deep Wounds
        ApplyDeepWounds(target);
    }
}

void ArmsSpecialization::CastColossusSmash(::Unit* target)
{
    if (target && CanUseAbility(COLOSSUS_SMASH))
    {
        _bot->CastSpell(target, COLOSSUS_SMASH, false);
        _lastColossusSmash = getMSTime();
        _cooldowns[COLOSSUS_SMASH] = 20000; // 20 second cooldown
    }
}

void ArmsSpecialization::CastOverpower(::Unit* target)
{
    if (target && CanUseAbility(OVERPOWER))
    {
        _bot->CastSpell(target, OVERPOWER, false);
        _lastOverpower = getMSTime();
        _overpowerReady = false;
    }
}

void ArmsSpecialization::CastExecute(::Unit* target)
{
    if (target && CanUseAbility(EXECUTE))
    {
        _bot->CastSpell(target, EXECUTE, false);
    }
}

void ArmsSpecialization::CastWarBreaker(::Unit* target)
{
    if (target && CanUseAbility(WAR_BREAKER))
    {
        _bot->CastSpell(target, WAR_BREAKER, false);
        _cooldowns[WAR_BREAKER] = 45000; // 45 second cooldown
    }
}

void ArmsSpecialization::CastWhirlwind()
{
    if (CanUseAbility(WHIRLWIND))
    {
        _bot->CastSpell(_bot, WHIRLWIND, false);
    }
}

void ArmsSpecialization::CastCleave(::Unit* target)
{
    if (target && CanUseAbility(CLEAVE))
    {
        _bot->CastSpell(target, CLEAVE, false);
    }
}

void ArmsSpecialization::ApplyDeepWounds(::Unit* target)
{
    if (target)
    {
        _deepWoundsTimers[target->GetGUID().GetCounter()] = getMSTime() + DEEP_WOUNDS_DURATION;
    }
}

bool ArmsSpecialization::HasDeepWounds(::Unit* target)
{
    if (!target)
        return false;

    auto it = _deepWoundsTimers.find(target->GetGUID().GetCounter());
    return it != _deepWoundsTimers.end() && it->second > getMSTime();
}

uint32 ArmsSpecialization::GetDeepWoundsTimeRemaining(::Unit* target)
{
    if (!target)
        return 0;

    auto it = _deepWoundsTimers.find(target->GetGUID().GetCounter());
    if (it != _deepWoundsTimers.end() && it->second > getMSTime())
    {
        return it->second - getMSTime();
    }
    return 0;
}

void ArmsSpecialization::ManageStanceDancing()
{
    // Tactical mastery allows stance dancing while retaining rage
    if (_bot->HasSpell(TACTICAL_MASTERY))
    {
        // Can switch stances more freely
    }
}

bool ArmsSpecialization::ShouldSwitchToDefensive()
{
    return _bot->GetHealthPct() < 30.0f;
}

bool ArmsSpecialization::ShouldSwitchToBerserker()
{
    ::Unit* target = _bot->GetVictim();
    return target && IsInExecutePhase(target);
}

uint32 ArmsSpecialization::GetTacticalMasteryRage()
{
    return _bot->HasSpell(TACTICAL_MASTERY) ? TACTICAL_MASTERY_RAGE : 0;
}

void ArmsSpecialization::UpdateArmsCooldowns(uint32 diff)
{
    // Update execute phase
    if (_inExecutePhase)
    {
        ::Unit* target = _bot->GetVictim();
        if (!target || !IsInExecutePhase(target))
        {
            _inExecutePhase = false;
            _executePhaseStartTime = 0;
        }
    }
}

void ArmsSpecialization::UseBladestorm()
{
    if (CanUseAbility(BLADESTORM))
    {
        _bot->CastSpell(_bot, BLADESTORM, false);
        _cooldowns[BLADESTORM] = 90000; // 90 second cooldown
    }
}

void ArmsSpecialization::UseAvatar()
{
    if (CanUseAbility(AVATAR))
    {
        _bot->CastSpell(_bot, AVATAR, false);
        _cooldowns[AVATAR] = 90000; // 90 second cooldown
    }
}

void ArmsSpecialization::UseRecklessness()
{
    if (CanUseAbility(RECKLESSNESS))
    {
        _bot->CastSpell(_bot, RECKLESSNESS, false);
        _cooldowns[RECKLESSNESS] = 90000; // 90 second cooldown
    }
}

bool ArmsSpecialization::ShouldUseBladestorm()
{
    // Use Bladestorm for AoE situations or when surrounded
    return CanUseAbility(BLADESTORM) && GetRagePercent() > 50.0f;
}

bool ArmsSpecialization::ShouldUseAvatar()
{
    // Use Avatar for burst damage
    return CanUseAbility(AVATAR) && _bot->GetVictim();
}

void ArmsSpecialization::HandleExecutePhase(::Unit* target)
{
    if (!_inExecutePhase)
    {
        _inExecutePhase = true;
        _executePhaseStartTime = getMSTime();
    }

    OptimizeExecuteRotation(target);
}

bool ArmsSpecialization::IsInExecutePhase(::Unit* target)
{
    return target && target->GetHealthPct() <= EXECUTE_HEALTH_THRESHOLD;
}

void ArmsSpecialization::OptimizeExecuteRotation(::Unit* target)
{
    if (!target)
        return;

    // Switch to berserker stance for execute
    if (GetCurrentStance() != WarriorStance::BERSERKER)
    {
        SwitchStance(WarriorStance::BERSERKER);
    }

    // Execute has priority in execute phase
    if (ShouldCastExecute(target))
    {
        CastExecute(target);
    }
}

} // namespace Playerbot