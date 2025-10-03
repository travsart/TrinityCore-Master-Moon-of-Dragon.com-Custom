/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "EnergyManager.h"
#include "Player.h"
#include "SharedDefines.h"
#include <algorithm>

namespace Playerbot
{

bool EnergyManager::ShouldPoolEnergy(uint32 targetEnergy)
{
    if (!_bot)
        return false;

    uint32 currentEnergy = _bot->GetPower(POWER_ENERGY);
    return currentEnergy < targetEnergy;
}

bool EnergyManager::HasEnoughEnergy(uint32 required)
{
    if (!_bot)
        return false;

    return _bot->GetPower(POWER_ENERGY) >= required;
}

void EnergyManager::ConsumeEnergy(uint32 amount)
{
    if (!_bot)
        return;

    uint32 currentEnergy = _bot->GetPower(POWER_ENERGY);
    uint32 newEnergy = currentEnergy >= amount ? currentEnergy - amount : 0;
    _bot->SetPower(POWER_ENERGY, newEnergy);
}

uint32 EnergyManager::GetCurrentEnergy()
{
    if (!_bot)
        return 0;

    return _bot->GetPower(POWER_ENERGY);
}

float EnergyManager::GetEnergyPercent()
{
    if (!_bot)
        return 0.0f;

    uint32 currentEnergy = _bot->GetPower(POWER_ENERGY);
    return (static_cast<float>(currentEnergy) / MAX_ENERGY) * 100.0f;
}

uint32 EnergyManager::GetTimeToEnergy(uint32 targetEnergy)
{
    uint32 currentEnergy = GetCurrentEnergy();
    if (currentEnergy >= targetEnergy)
        return 0;

    // Base regen is 20 energy per second
    float regenRate = ENERGY_REGEN_RATE;

    // Check for Combat Potency talent (Combat spec)
    if (_bot && _bot->HasSpell(35551))
        regenRate *= 1.2f;

    // Check for Vigor talent
    if (_bot && _bot->HasSpell(14983))
        regenRate *= 1.1f;

    uint32 energyNeeded = targetEnergy - currentEnergy;
    return static_cast<uint32>((energyNeeded / regenRate) * 1000);
}

void EnergyManager::Update(uint32 diff)
{
    _lastTickTime += diff;
}

void EnergyManager::UpdateEnergyTracking()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastTickTime >= 100) // Update every 100ms
    {
        _lastTickTime = currentTime;
    }
}

} // namespace Playerbot