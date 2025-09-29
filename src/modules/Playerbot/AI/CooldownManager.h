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

class TC_GAME_API CooldownManager
{
public:
    explicit CooldownManager(Player* bot);
    ~CooldownManager() = default;

    // Cooldown tracking
    void Update(uint32 diff);
    void UpdateCooldowns(uint32 diff);

    bool IsReady(uint32 spellId) const;
    bool IsGCDReady() const;
    uint32 GetRemainingCooldown(uint32 spellId) const;

    void StartCooldown(uint32 spellId, uint32 duration);
    void TriggerGCD();

    void AddCooldown(uint32 spellId, uint32 duration);
    void RemoveCooldown(uint32 spellId);
    void ClearAllCooldowns();

    // Priority cooldown management
    void SetPriorityCooldown(uint32 spellId, bool isPriority);
    bool IsPriorityCooldown(uint32 spellId) const;

private:
    Player* _bot;
    std::unordered_map<uint32, uint32> _cooldowns;
    std::unordered_map<uint32, bool> _priorityCooldowns;
    uint32 _lastUpdate;
};

} // namespace Playerbot