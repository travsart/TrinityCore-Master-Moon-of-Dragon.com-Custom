/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "ResourceManager.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Map.h"

namespace Playerbot
{

ResourceManager::ResourceManager(Player* bot) : _bot(bot)
{
}

uint32 ResourceManager::GetMana() const
{
    if (!_bot)
        return 0;
    return _bot->GetPower(POWER_MANA);
}

uint32 ResourceManager::GetMaxMana() const
{
    if (!_bot)
        return 0;
    return _bot->GetMaxPower(POWER_MANA);
}

float ResourceManager::GetManaPercent() const
{
    if (!_bot)
        return 0.0f;
    return _bot->GetPowerPct(POWER_MANA);
}

uint32 ResourceManager::GetEnergy() const
{
    if (!_bot)
        return 0;
    return _bot->GetPower(POWER_ENERGY);
}

uint32 ResourceManager::GetMaxEnergy() const
{
    if (!_bot)
        return 0;
    return _bot->GetMaxPower(POWER_ENERGY);
}

float ResourceManager::GetEnergyPercent() const
{
    if (!_bot)
        return 0.0f;
    return _bot->GetPowerPct(POWER_ENERGY);
}

uint32 ResourceManager::GetRage() const
{
    if (!_bot)
        return 0;
    return _bot->GetPower(POWER_RAGE);
}

uint32 ResourceManager::GetMaxRage() const
{
    if (!_bot)
        return 0;
    return _bot->GetMaxPower(POWER_RAGE);
}

float ResourceManager::GetRagePercent() const
{
    if (!_bot)
        return 0.0f;
    return _bot->GetPowerPct(POWER_RAGE);
}

bool ResourceManager::HasEnoughMana(uint32 spellId)
{
    if (!_bot)
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, _bot->GetMap()->GetDifficultyID());
    if (!spellInfo)
        return false;

    auto powerCosts = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask());
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA && _bot->GetPower(POWER_MANA) < int32(cost.Amount))
            return false;
    }

    return true;
}

bool ResourceManager::HasEnoughEnergy(uint32 spellId)
{
    if (!_bot)
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, _bot->GetMap()->GetDifficultyID());
    if (!spellInfo)
        return false;

    auto powerCosts = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask());
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_ENERGY && _bot->GetPower(POWER_ENERGY) < int32(cost.Amount))
            return false;
    }

    return true;
}

bool ResourceManager::HasEnoughRage(uint32 spellId)
{
    if (!_bot)
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, _bot->GetMap()->GetDifficultyID());
    if (!spellInfo)
        return false;

    auto powerCosts = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask());
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_RAGE && _bot->GetPower(POWER_RAGE) < int32(cost.Amount))
            return false;
    }

    return true;
}

void ResourceManager::ConsumeResource(uint32 spellId)
{
    if (!_bot)
        return;

    // Track resource consumption for performance metrics
    // This is handled by the spell system automatically
}

void ResourceManager::UpdateResourceTracking()
{
    if (!_bot)
        return;

    // Track resource consumption for performance metrics
}

} // namespace Playerbot