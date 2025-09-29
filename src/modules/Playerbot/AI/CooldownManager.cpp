/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "CooldownManager.h"
#include "Player.h"
#include "GameTime.h"
#include "SpellHistory.h"
#include "SpellMgr.h"
#include "Map.h"

namespace Playerbot
{

CooldownManager::CooldownManager(Player* bot) : _bot(bot), _lastUpdate(0)
{
}

void CooldownManager::Update(uint32 diff)
{
    if (!_bot)
        return;

    uint32 currentTime = getMSTime();

    // Update our internal cooldown tracking
    for (auto it = _cooldowns.begin(); it != _cooldowns.end();)
    {
        if (it->second <= currentTime)
        {
            it = _cooldowns.erase(it);
        }
        else
        {
            ++it;
        }
    }

    _lastUpdate = currentTime;
}

void CooldownManager::UpdateCooldowns(uint32 diff)
{
    Update(diff);
}

bool CooldownManager::IsReady(uint32 spellId) const
{
    if (!_bot)
        return false;

    // Check TrinityCore's spell cooldown system first
    if (_bot->GetSpellHistory()->HasCooldown(spellId))
        return false;

    // Check our internal tracking
    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end())
    {
        return getMSTime() >= it->second;
    }

    return true;
}

bool CooldownManager::IsGCDReady() const
{
    if (!_bot)
        return false;

    // Check if GCD is active - simplified check
    return !_bot->IsNonMeleeSpellCast(false, false, true);
}

uint32 CooldownManager::GetRemainingCooldown(uint32 spellId) const
{
    if (!_bot)
        return 0;

    // Check TrinityCore's spell cooldown system
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, _bot->GetMap()->GetDifficultyID());
    if (spellInfo && _bot->GetSpellHistory()->HasCooldown(spellId))
    {
        return _bot->GetSpellHistory()->GetRemainingCooldown(spellInfo).count();
    }

    // Check our internal tracking
    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end())
    {
        uint32 currentTime = getMSTime();
        if (it->second > currentTime)
            return it->second - currentTime;
    }

    return 0;
}

void CooldownManager::StartCooldown(uint32 spellId, uint32 duration)
{
    if (!_bot)
        return;

    uint32 endTime = getMSTime() + duration;
    _cooldowns[spellId] = endTime;
}

void CooldownManager::TriggerGCD()
{
    if (!_bot)
        return;

    // This is handled by TrinityCore's spell system
    // when spells are cast normally
}

void CooldownManager::AddCooldown(uint32 spellId, uint32 duration)
{
    StartCooldown(spellId, duration);
}

void CooldownManager::RemoveCooldown(uint32 spellId)
{
    _cooldowns.erase(spellId);
}

void CooldownManager::ClearAllCooldowns()
{
    _cooldowns.clear();
}

void CooldownManager::SetPriorityCooldown(uint32 spellId, bool isPriority)
{
    _priorityCooldowns[spellId] = isPriority;
}

bool CooldownManager::IsPriorityCooldown(uint32 spellId) const
{
    auto it = _priorityCooldowns.find(spellId);
    return it != _priorityCooldowns.end() && it->second;
}

} // namespace Playerbot