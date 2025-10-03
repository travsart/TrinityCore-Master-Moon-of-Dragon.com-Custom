/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "Timer.h"

// Forward declarations
class Player;

namespace Playerbot
{

class TC_GAME_API EnergyManager
{
public:
    explicit EnergyManager(Player* bot) : _bot(bot), _lastTickTime(getMSTime()) {}

    bool ShouldPoolEnergy(uint32 targetEnergy = 60);
    bool HasEnoughEnergy(uint32 required);
    void ConsumeEnergy(uint32 amount);
    uint32 GetCurrentEnergy();
    float GetEnergyPercent();
    uint32 GetTimeToEnergy(uint32 targetEnergy);
    void Update(uint32 diff);
    void UpdateEnergyTracking();

private:
    Player* _bot;
    uint32 _lastTickTime;

    // Energy regeneration constants
    static constexpr uint32 ENERGY_REGEN_RATE = 20; // Energy per second
    static constexpr uint32 MAX_ENERGY = 100;
    static constexpr uint32 ENERGY_TICK_INTERVAL = 2000; // 2 seconds per tick
    static constexpr uint32 ENERGY_PER_TICK = 40; // 20 energy per second * 2 seconds
};

} // namespace Playerbot