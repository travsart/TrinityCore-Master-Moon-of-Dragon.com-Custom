/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "FireSpecialization.h"
#include "Player.h"
#include "Unit.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "ObjectAccessor.h"

namespace Playerbot
{

FireSpecialization::FireSpecialization(Player* bot)
    : MageSpecialization(bot)
    , _hasHotStreak(false)
    , _hasHeatingUp(false)
    , _lastCritTime(0)
    , _combustionEndTime(0)
    , _inCombustion(false)
    , _lastDoTCheck(0)
    , _lastAoECheck(0)
    , _lastBuffCheck(0)
    , _lastRotationUpdate(0)
    , _lastEnemyScan(0)
{
}

void FireSpecialization::UpdateRotation(::Unit* target)
{
    if (!target || !_bot->IsAlive() || !target->IsAlive())
        return;

    uint32 currentTime = getMSTime();
    if (currentTime - _lastRotationUpdate < 100) // 100ms throttle
        return;
    _lastRotationUpdate = currentTime;

    // Update proc states
    UpdateHotStreak();
    UpdateHeatingUp();
    UpdateCombustion();

    // Check for AoE situations
    if (ShouldUseAoE())
    {
        HandleAoERotation();
        return;
    }

    // Use Combustion during Hot Streak or high damage windows
    if (ShouldUseCombustion())
    {
        CastCombustion();
        return;
    }

    // Hot Streak rotation (instant Pyroblast)
    if (HasHotStreak() && ShouldCastPyroblast())
    {
        CastPyroblast();
        return;
    }

    // Heating Up - use Fire Blast to fish for Hot Streak
    if (HasHeatingUp() && ShouldCastFireBlast())
    {
        CastFireBlast();
        return;
    }

    // Phoenix Flames for Heating Up
    if (HasHeatingUp() && ShouldCastPhoenixFlames())
    {
        CastPhoenixFlames();
        return;
    }

    // Moving - use Scorch
    if (_bot->isMoving() && CanUseAbility(SCORCH))
    {
        CastScorch();
        return;
    }

    // Standard rotation - Fireball
    if (ShouldCastFireball())
    {
        CastFireball();
        return;
    }

    // Backup - Fire Blast
    if (ShouldCastFireBlast())
    {
        CastFireBlast();
    }
}

void FireSpecialization::UpdateBuffs()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastBuffCheck < 5000) // 5 second throttle
        return;
    _lastBuffCheck = currentTime;

    // Arcane Intellect
    if (!_bot->HasAura(ARCANE_INTELLECT))
    {
        if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(ARCANE_INTELLECT))
        {
            _bot->CastSpell(_bot, ARCANE_INTELLECT, false);
        }
    }

    CheckFireBuffs();
    UpdateDoTs();
}

void FireSpecialization::UpdateCooldowns(uint32 diff)
{
    // Update all cooldown timers
    for (auto& cooldown : _cooldowns)
    {
        if (cooldown.second > diff)
            cooldown.second -= diff;
        else
            cooldown.second = 0;
    }

    // Update DoT timers
    for (auto& igniteTimer : _igniteTimers)
    {
        if (igniteTimer.second > diff)
            igniteTimer.second -= diff;
        else
            igniteTimer.second = 0;
    }

    for (auto& bombTimer : _livingBombTimers)
    {
        if (bombTimer.second > diff)
            bombTimer.second -= diff;
        else
            bombTimer.second = 0;
    }

    UpdateFireCooldowns(diff);
}

bool FireSpecialization::CanUseAbility(uint32 spellId)
{
    if (!HasEnoughResource(spellId))
        return false;

    // Check cooldown
    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end() && it->second > 0)
        return false;

    return true;
}

void FireSpecialization::OnCombatStart(::Unit* target)
{
    _hasHotStreak = false;
    _hasHeatingUp = false;
    _lastCritTime = 0;
    _inCombustion = false;
    _combustionEndTime = 0;
    _igniteTimers.clear();
    _livingBombTimers.clear();
}

void FireSpecialization::OnCombatEnd()
{
    _hasHotStreak = false;
    _hasHeatingUp = false;
    _inCombustion = false;
    _cooldowns.clear();
    _igniteTimers.clear();
    _livingBombTimers.clear();
    _nearbyEnemies.clear();
}

bool FireSpecialization::HasEnoughResource(uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return false;

    uint32 manaCost = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask());
    return GetMana() >= manaCost;
}

void FireSpecialization::ConsumeResource(uint32 spellId)
{
    // Mana is consumed automatically by spell casting
}

Position FireSpecialization::GetOptimalPosition(::Unit* target)
{
    if (!target)
        return _bot->GetPosition();

    float distance = GetOptimalRange(target);
    float angle = _bot->GetAngle(target);

    Position pos;
    target->GetNearPosition(pos, distance, angle + M_PI);
    return pos;
}

float FireSpecialization::GetOptimalRange(::Unit* target)
{
    return OPTIMAL_CASTING_RANGE;
}

// Private methods

void FireSpecialization::UpdateHotStreak()
{
    _hasHotStreak = _bot->HasAura(HOT_STREAK);
}

void FireSpecialization::UpdateHeatingUp()
{
    _hasHeatingUp = _bot->HasAura(HEATING_UP);
}

void FireSpecialization::UpdateCombustion()
{
    _inCombustion = _bot->HasAura(COMBUSTION);
    if (_inCombustion && _combustionEndTime == 0)
    {
        _combustionEndTime = getMSTime() + COMBUSTION_DURATION;
    }
    else if (!_inCombustion)
    {
        _combustionEndTime = 0;
    }
}

bool FireSpecialization::HasHotStreak()
{
    return _hasHotStreak;
}

bool FireSpecialization::HasHeatingUp()
{
    return _hasHeatingUp;
}

bool FireSpecialization::HasCombustion()
{
    return _inCombustion;
}

bool FireSpecialization::ShouldCastPyroblast()
{
    // Cast instant Pyroblast on Hot Streak
    return HasHotStreak();
}

bool FireSpecialization::ShouldCastFireball()
{
    // Don't cast Fireball if we have Hot Streak
    if (HasHotStreak())
        return false;

    // Don't cast if moving
    if (_bot->isMoving())
        return false;

    return CanUseAbility(FIREBALL);
}

bool FireSpecialization::ShouldCastFireBlast()
{
    // Use Fire Blast when we have Heating Up to fish for Hot Streak
    if (HasHeatingUp())
        return CanUseAbility(FIRE_BLAST);

    // Use as filler if other spells not available
    return CanUseAbility(FIRE_BLAST);
}

bool FireSpecialization::ShouldCastPhoenixFlames()
{
    // Use Phoenix Flames when we have Heating Up
    return HasHeatingUp() && CanUseAbility(PHOENIX_FLAMES);
}

bool FireSpecialization::ShouldUseCombustion()
{
    // Use Combustion when we have Hot Streak for maximum damage
    if (!CanUseAbility(COMBUSTION))
        return false;

    // Use during Hot Streak or when at high mana
    return HasHotStreak() || GetManaPercent() > 80.0f;
}

void FireSpecialization::CastFireball()
{
    if (CanUseAbility(FIREBALL))
    {
        _bot->CastSpell(_bot->GetVictim(), FIREBALL, false);
    }
}

void FireSpecialization::CastPyroblast()
{
    if (CanUseAbility(PYROBLAST))
    {
        _bot->CastSpell(_bot->GetVictim(), PYROBLAST, false);
    }
}

void FireSpecialization::CastFireBlast()
{
    if (CanUseAbility(FIRE_BLAST))
    {
        _bot->CastSpell(_bot->GetVictim(), FIRE_BLAST, false);
        _cooldowns[FIRE_BLAST] = 12000; // 12 second cooldown
    }
}

void FireSpecialization::CastPhoenixFlames()
{
    if (CanUseAbility(PHOENIX_FLAMES))
    {
        _bot->CastSpell(_bot->GetVictim(), PHOENIX_FLAMES, false);
        _cooldowns[PHOENIX_FLAMES] = 25000; // 25 second cooldown
    }
}

void FireSpecialization::CastScorch()
{
    if (CanUseAbility(SCORCH))
    {
        _bot->CastSpell(_bot->GetVictim(), SCORCH, false);
    }
}

void FireSpecialization::CastFlamestrike()
{
    if (CanUseAbility(FLAMESTRIKE))
    {
        ::Unit* target = _bot->GetVictim();
        if (target)
        {
            _bot->CastSpell(target, FLAMESTRIKE, false);
        }
    }
}

void FireSpecialization::CastDragonsBreath()
{
    if (CanUseAbility(DRAGONS_BREATH))
    {
        _bot->CastSpell(_bot, DRAGONS_BREATH, false);
        _cooldowns[DRAGONS_BREATH] = 20000; // 20 second cooldown
    }
}

void FireSpecialization::CastCombustion()
{
    if (CanUseAbility(COMBUSTION))
    {
        _bot->CastSpell(_bot, COMBUSTION, false);
        _cooldowns[COMBUSTION] = 120000; // 2 minute cooldown
        _inCombustion = true;
        _combustionEndTime = getMSTime() + COMBUSTION_DURATION;
    }
}

void FireSpecialization::CastMirrorImage()
{
    if (CanUseAbility(MIRROR_IMAGE))
    {
        _bot->CastSpell(_bot, MIRROR_IMAGE, false);
        _cooldowns[MIRROR_IMAGE] = 120000; // 2 minute cooldown
    }
}

void FireSpecialization::UpdateDoTs()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastDoTCheck < 2000) // 2 second throttle
        return;
    _lastDoTCheck = currentTime;

    ::Unit* target = _bot->GetVictim();
    if (!target)
        return;

    // Apply/refresh Ignite
    if (!HasIgnite(target) || GetIgniteTimeRemaining(target) < 3000)
    {
        ApplyIgnite(target);
    }

    // Apply Living Bomb if not present
    if (!HasLivingBomb(target))
    {
        CastLivingBomb(target);
    }
}

void FireSpecialization::ApplyIgnite(::Unit* target)
{
    if (target && target->IsAlive())
    {
        // Ignite is applied automatically by fire damage crits
        _igniteTimers[target->GetGUID().GetCounter()] = getMSTime() + IGNITE_DURATION;
    }
}

void FireSpecialization::CastLivingBomb(::Unit* target)
{
    if (target && CanUseAbility(LIVING_BOMB))
    {
        _bot->CastSpell(target, LIVING_BOMB, false);
        _livingBombTimers[target->GetGUID().GetCounter()] = getMSTime() + LIVING_BOMB_DURATION;
    }
}

bool FireSpecialization::HasIgnite(::Unit* target)
{
    if (!target)
        return false;

    auto it = _igniteTimers.find(target->GetGUID().GetCounter());
    return it != _igniteTimers.end() && it->second > getMSTime();
}

bool FireSpecialization::HasLivingBomb(::Unit* target)
{
    if (!target)
        return false;

    auto it = _livingBombTimers.find(target->GetGUID().GetCounter());
    return it != _livingBombTimers.end() && it->second > getMSTime();
}

uint32 FireSpecialization::GetIgniteTimeRemaining(::Unit* target)
{
    if (!target)
        return 0;

    auto it = _igniteTimers.find(target->GetGUID().GetCounter());
    if (it != _igniteTimers.end() && it->second > getMSTime())
    {
        return it->second - getMSTime();
    }
    return 0;
}

void FireSpecialization::HandleAoERotation()
{
    std::vector<::Unit*> enemies = GetNearbyEnemies();

    if (enemies.size() >= AOE_THRESHOLD)
    {
        // Use Flamestrike for AoE
        if (CanUseAbility(FLAMESTRIKE))
        {
            CastFlamestrike();
            return;
        }

        // Dragon's Breath for close enemies
        if (CanUseAbility(DRAGONS_BREATH))
        {
            CastDragonsBreath();
            return;
        }

        // Meteor for high damage AoE
        if (CanUseAbility(METEOR))
        {
            CastMeteor();
            return;
        }
    }

    // Fall back to single target
    UpdateRotation(_bot->GetVictim());
}

std::vector<::Unit*> FireSpecialization::GetNearbyEnemies(float range)
{
    std::vector<::Unit*> enemies;
    uint32 currentTime = getMSTime();

    if (currentTime - _lastEnemyScan < 1000) // 1 second cache
    {
        // Return cached results converted from GUIDs
        // Implementation would need proper enemy caching
        return enemies;
    }

    _lastEnemyScan = currentTime;
    _nearbyEnemies.clear();

    // Scan for nearby enemies
    // Implementation depends on TrinityCore's enemy detection system

    return enemies;
}

bool FireSpecialization::ShouldUseAoE()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastAoECheck < 1000) // 1 second throttle
        return false;
    _lastAoECheck = currentTime;

    std::vector<::Unit*> enemies = GetNearbyEnemies();
    return enemies.size() >= AOE_THRESHOLD;
}

void FireSpecialization::CastMeteor()
{
    if (CanUseAbility(METEOR))
    {
        ::Unit* target = _bot->GetVictim();
        if (target)
        {
            _bot->CastSpell(target, METEOR, false);
            _cooldowns[METEOR] = 45000; // 45 second cooldown
        }
    }
}

void FireSpecialization::CastBlastWave()
{
    if (CanUseAbility(BLAST_WAVE))
    {
        _bot->CastSpell(_bot, BLAST_WAVE, false);
        _cooldowns[BLAST_WAVE] = 30000; // 30 second cooldown
    }
}

void FireSpecialization::UpdateFireCooldowns(uint32 diff)
{
    // Update Combustion state
    if (_inCombustion && getMSTime() >= _combustionEndTime)
    {
        _inCombustion = false;
        _combustionEndTime = 0;
    }
}

void FireSpecialization::CheckFireBuffs()
{
    UpdateHotStreak();
    UpdateHeatingUp();
    UpdateCombustion();
}

bool FireSpecialization::HasCriticalMass()
{
    return _bot->HasAura(CRITICAL_MASS);
}

void FireSpecialization::UseCooldowns()
{
    // Use Mirror Image defensively
    if (_bot->GetHealthPct() < 30.0f && CanUseAbility(MIRROR_IMAGE))
    {
        CastMirrorImage();
    }
}

void FireSpecialization::ProcessHotStreak()
{
    // Hot Streak processing is handled by UpdateHotStreak()
    if (HasHotStreak())
    {
        // Priority: use instant Pyroblast
        if (ShouldCastPyroblast())
        {
            CastPyroblast();
        }
    }
}

void FireSpecialization::ProcessHeatingUp()
{
    // Heating Up processing
    if (HasHeatingUp())
    {
        // Use Fire Blast or Phoenix Flames to fish for Hot Streak
        if (ShouldCastFireBlast())
        {
            CastFireBlast();
        }
        else if (ShouldCastPhoenixFlames())
        {
            CastPhoenixFlames();
        }
    }
}

void FireSpecialization::CheckForInstantPyroblast()
{
    // Check if we should use instant Pyroblast
    if (HasHotStreak() && CanUseAbility(PYROBLAST))
    {
        CastPyroblast();
    }
}

} // namespace Playerbot