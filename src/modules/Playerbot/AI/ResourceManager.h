/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "Define.h"
#include "Player.h"
#include <unordered_map>

namespace Playerbot
{

class TC_GAME_API ResourceManager
{
public:
    explicit ResourceManager(Player* bot);
    ~ResourceManager() = default;

    // Resource monitoring
    uint32 GetMana() const;
    uint32 GetMaxMana() const;
    float GetManaPercent() const;

    uint32 GetEnergy() const;
    uint32 GetMaxEnergy() const;
    float GetEnergyPercent() const;

    uint32 GetRage() const;
    uint32 GetMaxRage() const;
    float GetRagePercent() const;

    // Resource management
    bool HasEnoughMana(uint32 spellId);
    bool HasEnoughEnergy(uint32 spellId);
    bool HasEnoughRage(uint32 spellId);

    void ConsumeResource(uint32 spellId);
    void UpdateResourceTracking();

private:
    Player* _bot;
    std::unordered_map<uint32, uint32> _resourceCosts;
};

} // namespace Playerbot