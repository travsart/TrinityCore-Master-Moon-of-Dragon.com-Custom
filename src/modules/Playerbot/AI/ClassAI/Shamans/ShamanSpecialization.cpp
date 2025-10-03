/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "ShamanSpecialization.h"
#include "Player.h"
#include "Unit.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "ObjectAccessor.h"
#include "GameTime.h"
#include "SharedDefines.h"
#include "Creature.h"
#include "TemporarySummon.h"

namespace Playerbot
{

void ShamanSpecialization::DeployTotem(TotemType type, uint32 spellId)
{
    if (!_bot || !spellId)
        return;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return;

    // Check mana cost
    auto powerCosts = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask());
    uint32 manaCost = 0;
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA)
        {
            manaCost = cost.Amount;
            break;
        }
    }

    if (_bot->GetPower(POWER_MANA) < manaCost)
        return;

    // Check if totem of this type is already active
    uint32 typeIndex = static_cast<uint32>(type);
    if (typeIndex < 4u && _activeTotems[typeIndex].isActive)
    {
        // Don't replace same totem
        if (_activeTotems[typeIndex].spellId == spellId)
            return;

        // Recall existing totem of this type
        RecallTotem(type);
    }

    // Cast the totem
    if (_bot->CastSpell(_bot, spellId, false) == SPELL_CAST_OK)
    {
        if (typeIndex < 4u)
        {
            _activeTotems[typeIndex] = TotemInfo(spellId, type, 120000, 30.0f); // Default 2 min duration, 30y range
            _activeTotems[typeIndex].isActive = true;
            _activeTotems[typeIndex].position = _bot->GetPosition();
        }

        // Set cooldown for totem type
        _totemCooldowns[type] = getMSTime() + 1000; // 1 second cooldown between totem casts
    }
}

void ShamanSpecialization::RecallTotem(TotemType type)
{
    uint32 typeIndex = static_cast<uint32>(type);
    if (typeIndex < 4u && _activeTotems[typeIndex].isActive)
    {
        // Find and destroy the totem
        if (_activeTotems[typeIndex].totem && _activeTotems[typeIndex].totem->IsAlive())
        {
            // Simply kill the totem creature
            _activeTotems[typeIndex].totem->setDeathState(JUST_DIED);
        }

        _activeTotems[typeIndex] = TotemInfo(); // Reset
        _activeTotems[typeIndex].isActive = false;
    }
}

bool ShamanSpecialization::IsTotemActive(TotemType type)
{
    uint32 typeIndex = static_cast<uint32>(type);
    if (typeIndex >= 4u)
        return false;

    return _activeTotems[typeIndex].isActive &&
           _activeTotems[typeIndex].remainingTime > 0;
}

uint32 ShamanSpecialization::GetTotemRemainingTime(TotemType type)
{
    uint32 typeIndex = static_cast<uint32>(type);
    if (typeIndex >= 4u || !_activeTotems[typeIndex].isActive)
        return 0;

    uint32 currentTime = getMSTime();
    if (_activeTotems[typeIndex].remainingTime > currentTime)
        return _activeTotems[typeIndex].remainingTime - currentTime;

    return 0;
}

Position ShamanSpecialization::GetOptimalTotemPosition(TotemType type)
{
    // For most totems, place near the shaman's current position
    // This can be overridden by specific specializations for tactical positioning
    return _bot->GetPosition();
}

void ShamanSpecialization::CastEarthShock(::Unit* target)
{
    if (!target || !_bot)
        return;

    uint32 spellId = EARTH_SHOCK;
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return;

    // Check mana cost
    auto powerCosts = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask());
    uint32 manaCost = 0;
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA)
        {
            manaCost = cost.Amount;
            break;
        }
    }

    if (_bot->GetPower(POWER_MANA) < manaCost)
        return;

    // Check shock cooldown
    if (IsShockOnCooldown())
        return;

    // Check range
    float distance = _bot->GetDistance(target);
    if (distance > spellInfo->GetMaxRange())
        return;

    // Check line of sight
    if (!_bot->IsWithinLOSInMap(target))
        return;

    // Cast spell
    if (_bot->CastSpell(target, spellId, false) == SPELL_CAST_OK)
    {
        _lastEarthShock = getMSTime();
        _shockCooldown = getMSTime() + 6000; // 6 second shock cooldown
    }
}

void ShamanSpecialization::CastFlameShock(::Unit* target)
{
    if (!target || !_bot)
        return;

    uint32 spellId = FLAME_SHOCK;
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return;

    // Check mana cost
    auto powerCosts = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask());
    uint32 manaCost = 0;
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA)
        {
            manaCost = cost.Amount;
            break;
        }
    }

    if (_bot->GetPower(POWER_MANA) < manaCost)
        return;

    // Check shock cooldown
    if (IsShockOnCooldown())
        return;

    // Don't reapply if target already has Flame Shock
    if (target->HasAura(FLAME_SHOCK))
        return;

    // Check range
    float distance = _bot->GetDistance(target);
    if (distance > spellInfo->GetMaxRange())
        return;

    // Check line of sight
    if (!_bot->IsWithinLOSInMap(target))
        return;

    // Cast spell
    if (_bot->CastSpell(target, spellId, false) == SPELL_CAST_OK)
    {
        _lastFlameShock = getMSTime();
        _shockCooldown = getMSTime() + 6000; // 6 second shock cooldown
    }
}

void ShamanSpecialization::CastFrostShock(::Unit* target)
{
    if (!target || !_bot)
        return;

    uint32 spellId = FROST_SHOCK;
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return;

    // Check mana cost
    auto powerCosts = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask());
    uint32 manaCost = 0;
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA)
        {
            manaCost = cost.Amount;
            break;
        }
    }

    if (_bot->GetPower(POWER_MANA) < manaCost)
        return;

    // Check shock cooldown
    if (IsShockOnCooldown())
        return;

    // Check range
    float distance = _bot->GetDistance(target);
    if (distance > spellInfo->GetMaxRange())
        return;

    // Check line of sight
    if (!_bot->IsWithinLOSInMap(target))
        return;

    // Cast spell - Frost Shock is good for slowing fleeing enemies
    if (_bot->CastSpell(target, spellId, false) == SPELL_CAST_OK)
    {
        _lastFrostShock = getMSTime();
        _shockCooldown = getMSTime() + 6000; // 6 second shock cooldown
    }
}

bool ShamanSpecialization::IsShockOnCooldown()
{
    uint32 currentTime = getMSTime();
    return _shockCooldown > currentTime;
}

} // namespace Playerbot