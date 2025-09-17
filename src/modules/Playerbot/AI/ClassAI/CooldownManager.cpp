/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "CooldownManager.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Player.h"
#include "Log.h"
#include <algorithm>

namespace Playerbot
{

CooldownManager::CooldownManager()
{
    TC_LOG_DEBUG("playerbot.cooldown", "CooldownManager initialized");
}

void CooldownManager::Update(uint32 diff)
{
    std::lock_guard<std::mutex> lock(_cooldownMutex);

    _totalUpdates.fetch_add(1);

    // Update global cooldown
    uint32 currentGCD = _globalCooldown.load();
    if (currentGCD > 0)
    {
        uint32 newGCD = (currentGCD > diff) ? currentGCD - diff : 0;
        _globalCooldown.store(newGCD);
    }

    // Update category cooldowns
    {
        std::lock_guard<std::mutex> categoryLock(_categoryMutex);
        for (auto& pair : _categoryCooldowns)
        {
            if (pair.second > diff)
                pair.second -= diff;
            else
                pair.second = 0;
        }
    }

    // Update individual spell cooldowns
    uint32 activeCooldowns = 0;
    for (auto& pair : _cooldowns)
    {
        UpdateCooldown(pair.second, diff);
        if (pair.second.remainingMs > 0 || pair.second.isChanneling)
            activeCooldowns++;
    }

    _activeCooldowns.store(activeCooldowns);

    // Periodic cleanup
    if ((_totalUpdates.load() % (CLEANUP_INTERVAL_MS / 100)) == 0)
    {
        CleanupExpiredCooldowns();
    }
}

void CooldownManager::StartCooldown(uint32 spellId, uint32 cooldownMs)
{
    StartCooldown(spellId, cooldownMs, true);
}

void CooldownManager::StartCooldown(uint32 spellId, uint32 cooldownMs, bool triggersGCD)
{
    if (!spellId)
        return;

    std::lock_guard<std::mutex> lock(_cooldownMutex);

    // Apply cooldown multiplier
    uint32 adjustedCooldown = ApplyCooldownMultiplier(cooldownMs);

    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end())
    {
        it->second.cooldownMs = adjustedCooldown;
        it->second.remainingMs = adjustedCooldown;
        it->second.onGCD = triggersGCD;
    }
    else
    {
        CooldownInfo info(spellId, adjustedCooldown, triggersGCD);
        _cooldowns[spellId] = info;
    }

    TC_LOG_DEBUG("playerbot.cooldown", "Started cooldown for spell {}: {}ms", spellId, adjustedCooldown);
}

void CooldownManager::ResetCooldown(uint32 spellId)
{
    if (!spellId)
        return;

    std::lock_guard<std::mutex> lock(_cooldownMutex);

    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end())
    {
        it->second.remainingMs = 0;
        it->second.isChanneling = false;
        TC_LOG_DEBUG("playerbot.cooldown", "Reset cooldown for spell {}", spellId);
    }
}

void CooldownManager::ReduceCooldown(uint32 spellId, uint32 reductionMs)
{
    if (!spellId || !reductionMs)
        return;

    std::lock_guard<std::mutex> lock(_cooldownMutex);

    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end())
    {
        if (it->second.remainingMs > reductionMs)
            it->second.remainingMs -= reductionMs;
        else
            it->second.remainingMs = 0;

        TC_LOG_DEBUG("playerbot.cooldown", "Reduced cooldown for spell {} by {}ms", spellId, reductionMs);
    }
}

bool CooldownManager::IsReady(uint32 spellId) const
{
    if (!spellId)
        return false;

    std::lock_guard<std::mutex> lock(_cooldownMutex);

    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end())
    {
        return it->second.IsReady();
    }

    return true; // Spell not tracked = ready
}

uint32 CooldownManager::GetRemaining(uint32 spellId) const
{
    if (!spellId)
        return 0;

    std::lock_guard<std::mutex> lock(_cooldownMutex);

    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end())
    {
        return it->second.remainingMs;
    }

    return 0;
}

float CooldownManager::GetRemainingPercent(uint32 spellId) const
{
    if (!spellId)
        return 0.0f;

    std::lock_guard<std::mutex> lock(_cooldownMutex);

    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end())
    {
        return it->second.GetCooldownPercent();
    }

    return 0.0f;
}

uint32 CooldownManager::GetTotalCooldown(uint32 spellId) const
{
    if (!spellId)
        return 0;

    std::lock_guard<std::mutex> lock(_cooldownMutex);

    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end())
    {
        return it->second.cooldownMs;
    }

    return 0;
}

void CooldownManager::TriggerGCD(uint32 durationMs)
{
    _globalCooldown.store(durationMs);
    TC_LOG_DEBUG("playerbot.cooldown", "Triggered GCD: {}ms", durationMs);
}

bool CooldownManager::IsGCDReady() const
{
    return _globalCooldown.load() == 0;
}

uint32 CooldownManager::GetGCDRemaining() const
{
    return _globalCooldown.load();
}

void CooldownManager::SetCharges(uint32 spellId, uint32 current, uint32 maximum, uint32 rechargeTimeMs)
{
    if (!spellId)
        return;

    std::lock_guard<std::mutex> lock(_cooldownMutex);

    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end())
    {
        it->second.charges = current;
        it->second.maxCharges = maximum;
        it->second.chargeRechargeMs = rechargeTimeMs;
    }
    else
    {
        CooldownInfo info;
        info.spellId = spellId;
        info.charges = current;
        info.maxCharges = maximum;
        info.chargeRechargeMs = rechargeTimeMs;
        _cooldowns[spellId] = info;
    }

    TC_LOG_DEBUG("playerbot.cooldown", "Set charges for spell {}: {}/{}", spellId, current, maximum);
}

uint32 CooldownManager::GetCharges(uint32 spellId) const
{
    if (!spellId)
        return 0;

    std::lock_guard<std::mutex> lock(_cooldownMutex);

    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end())
    {
        return it->second.charges;
    }

    return 1; // Default to 1 charge if not tracked
}

uint32 CooldownManager::GetMaxCharges(uint32 spellId) const
{
    if (!spellId)
        return 0;

    std::lock_guard<std::mutex> lock(_cooldownMutex);

    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end())
    {
        return it->second.maxCharges;
    }

    return 1; // Default to 1 charge if not tracked
}

void CooldownManager::ConsumeCharge(uint32 spellId)
{
    if (!spellId)
        return;

    std::lock_guard<std::mutex> lock(_cooldownMutex);

    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end() && it->second.charges > 0)
    {
        it->second.charges--;

        // Start recharge timer if not at max charges
        if (it->second.charges < it->second.maxCharges && it->second.chargeRechargeMs > 0)
        {
            it->second.nextChargeTime = it->second.chargeRechargeMs;
        }

        TC_LOG_DEBUG("playerbot.cooldown", "Consumed charge for spell {}: {} remaining", spellId, it->second.charges);
    }
}

void CooldownManager::AddCharge(uint32 spellId)
{
    if (!spellId)
        return;

    std::lock_guard<std::mutex> lock(_cooldownMutex);

    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end())
    {
        if (it->second.charges < it->second.maxCharges)
        {
            it->second.charges++;
            TC_LOG_DEBUG("playerbot.cooldown", "Added charge for spell {}: {}/{}",
                         spellId, it->second.charges, it->second.maxCharges);
        }
    }
}

uint32 CooldownManager::GetNextChargeTime(uint32 spellId) const
{
    if (!spellId)
        return 0;

    std::lock_guard<std::mutex> lock(_cooldownMutex);

    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end())
    {
        return it->second.nextChargeTime;
    }

    return 0;
}

void CooldownManager::StartChanneling(uint32 spellId, uint32 channelDurationMs)
{
    if (!spellId)
        return;

    std::lock_guard<std::mutex> lock(_cooldownMutex);

    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end())
    {
        it->second.isChanneling = true;
        it->second.channelDuration = channelDurationMs;
    }
    else
    {
        CooldownInfo info;
        info.spellId = spellId;
        info.isChanneling = true;
        info.channelDuration = channelDurationMs;
        _cooldowns[spellId] = info;
    }

    TC_LOG_DEBUG("playerbot.cooldown", "Started channeling spell {}: {}ms", spellId, channelDurationMs);
}

void CooldownManager::StopChanneling(uint32 spellId)
{
    if (!spellId)
        return;

    std::lock_guard<std::mutex> lock(_cooldownMutex);

    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end())
    {
        it->second.isChanneling = false;
        it->second.channelDuration = 0;
        TC_LOG_DEBUG("playerbot.cooldown", "Stopped channeling spell {}", spellId);
    }
}

bool CooldownManager::IsChanneling(uint32 spellId) const
{
    if (!spellId)
        return false;

    std::lock_guard<std::mutex> lock(_cooldownMutex);

    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end())
    {
        return it->second.isChanneling;
    }

    return false;
}

bool CooldownManager::IsChannelingAny() const
{
    std::lock_guard<std::mutex> lock(_cooldownMutex);

    for (const auto& pair : _cooldowns)
    {
        if (pair.second.isChanneling)
            return true;
    }

    return false;
}

uint32 CooldownManager::GetChannelRemaining(uint32 spellId) const
{
    if (!spellId)
        return 0;

    std::lock_guard<std::mutex> lock(_cooldownMutex);

    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end() && it->second.isChanneling)
    {
        return it->second.channelDuration;
    }

    return 0;
}

void CooldownManager::ResetAllCooldowns()
{
    std::lock_guard<std::mutex> lock(_cooldownMutex);

    for (auto& pair : _cooldowns)
    {
        pair.second.remainingMs = 0;
        pair.second.isChanneling = false;
    }

    _globalCooldown.store(0);

    TC_LOG_DEBUG("playerbot.cooldown", "Reset all cooldowns");
}

void CooldownManager::ReduceAllCooldowns(uint32 reductionMs)
{
    if (!reductionMs)
        return;

    std::lock_guard<std::mutex> lock(_cooldownMutex);

    for (auto& pair : _cooldowns)
    {
        if (pair.second.remainingMs > reductionMs)
            pair.second.remainingMs -= reductionMs;
        else
            pair.second.remainingMs = 0;
    }

    TC_LOG_DEBUG("playerbot.cooldown", "Reduced all cooldowns by {}ms", reductionMs);
}

std::vector<uint32> CooldownManager::GetSpellsOnCooldown() const
{
    std::lock_guard<std::mutex> lock(_cooldownMutex);

    std::vector<uint32> result;
    for (const auto& pair : _cooldowns)
    {
        if (pair.second.remainingMs > 0)
        {
            result.push_back(pair.first);
        }
    }

    return result;
}

std::vector<uint32> CooldownManager::GetReadySpells(const std::vector<uint32>& spellIds) const
{
    std::lock_guard<std::mutex> lock(_cooldownMutex);

    std::vector<uint32> result;
    for (uint32 spellId : spellIds)
    {
        auto it = _cooldowns.find(spellId);
        if (it == _cooldowns.end() || it->second.IsReady())
        {
            result.push_back(spellId);
        }
    }

    return result;
}

void CooldownManager::SetCooldownCategory(uint32 spellId, uint32 categoryId)
{
    if (!spellId || !categoryId)
        return;

    std::lock_guard<std::mutex> lock(_categoryMutex);
    _spellCategories[spellId] = categoryId;

    TC_LOG_DEBUG("playerbot.cooldown", "Set spell {} to category {}", spellId, categoryId);
}

void CooldownManager::StartCategoryCooldown(uint32 categoryId, uint32 cooldownMs)
{
    if (!categoryId)
        return;

    std::lock_guard<std::mutex> lock(_categoryMutex);
    _categoryCooldowns[categoryId] = ApplyCooldownMultiplier(cooldownMs);

    TC_LOG_DEBUG("playerbot.cooldown", "Started category {} cooldown: {}ms", categoryId, cooldownMs);
}

bool CooldownManager::IsCategoryReady(uint32 categoryId) const
{
    if (!categoryId)
        return true;

    std::lock_guard<std::mutex> lock(_categoryMutex);

    auto it = _categoryCooldowns.find(categoryId);
    return (it == _categoryCooldowns.end() || it->second == 0);
}

uint32 CooldownManager::GetTimeUntilReady(uint32 spellId) const
{
    if (!spellId)
        return 0;

    std::lock_guard<std::mutex> lock(_cooldownMutex);

    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end())
    {
        uint32 cooldownTime = it->second.remainingMs;
        uint32 chargeTime = (it->second.charges == 0) ? it->second.nextChargeTime : 0;
        return std::max(cooldownTime, chargeTime);
    }

    return 0;
}

bool CooldownManager::WillBeReadyIn(uint32 spellId, uint32 timeMs) const
{
    return GetTimeUntilReady(spellId) <= timeMs;
}

uint32 CooldownManager::GetTotalSpellsTracked() const
{
    std::lock_guard<std::mutex> lock(_cooldownMutex);
    return static_cast<uint32>(_cooldowns.size());
}

uint32 CooldownManager::GetSpellsOnCooldownCount() const
{
    return _activeCooldowns.load();
}

uint32 CooldownManager::GetAverageActiveCooldowns() const
{
    // Simple implementation - could be enhanced with rolling average
    return _activeCooldowns.load();
}

void CooldownManager::DumpCooldowns() const
{
    std::lock_guard<std::mutex> lock(_cooldownMutex);

    TC_LOG_DEBUG("playerbot.cooldown", "=== Cooldown Manager Dump ===");
    TC_LOG_DEBUG("playerbot.cooldown", "GCD Remaining: {}ms", _globalCooldown.load());
    TC_LOG_DEBUG("playerbot.cooldown", "Total Spells Tracked: {}", _cooldowns.size());
    TC_LOG_DEBUG("playerbot.cooldown", "Active Cooldowns: {}", _activeCooldowns.load());

    for (const auto& pair : _cooldowns)
    {
        const CooldownInfo& info = pair.second;
        TC_LOG_DEBUG("playerbot.cooldown", "Spell {}: {}ms remaining, {}/{} charges, channeling: {}",
                     pair.first, info.remainingMs, info.charges, info.maxCharges, info.isChanneling);
    }
}

CooldownInfo CooldownManager::GetCooldownInfo(uint32 spellId) const
{
    if (!spellId)
        return CooldownInfo();

    std::lock_guard<std::mutex> lock(_cooldownMutex);

    auto it = _cooldowns.find(spellId);
    if (it != _cooldowns.end())
    {
        return it->second;
    }

    return CooldownInfo();
}

void CooldownManager::UpdateCooldown(CooldownInfo& cooldown, uint32 diff)
{
    // Update main cooldown
    if (cooldown.remainingMs > 0)
    {
        if (cooldown.remainingMs > diff)
            cooldown.remainingMs -= diff;
        else
            cooldown.remainingMs = 0;
    }

    // Update channel duration
    if (cooldown.isChanneling && cooldown.channelDuration > 0)
    {
        if (cooldown.channelDuration > diff)
            cooldown.channelDuration -= diff;
        else
        {
            cooldown.channelDuration = 0;
            cooldown.isChanneling = false;
        }
    }

    // Update charge regeneration
    UpdateCharges(cooldown, diff);
}

void CooldownManager::UpdateCharges(CooldownInfo& cooldown, uint32 diff)
{
    if (cooldown.charges < cooldown.maxCharges && cooldown.nextChargeTime > 0)
    {
        if (cooldown.nextChargeTime > diff)
        {
            cooldown.nextChargeTime -= diff;
        }
        else
        {
            // Add a charge
            cooldown.charges++;

            // Reset timer for next charge if still not at max
            if (cooldown.charges < cooldown.maxCharges)
                cooldown.nextChargeTime = cooldown.chargeRechargeMs;
            else
                cooldown.nextChargeTime = 0;
        }
    }
}

uint32 CooldownManager::ApplyCooldownMultiplier(uint32 cooldownMs) const
{
    float multiplier = _cooldownMultiplier.load();
    return static_cast<uint32>(cooldownMs * multiplier);
}

void CooldownManager::EnsureSpellData(uint32 spellId)
{
    // This could load spell data from DBC if needed for more accurate cooldown tracking
    // For now, we rely on the caller to provide correct cooldown values
}

void CooldownManager::CleanupExpiredCooldowns()
{
    auto it = _cooldowns.begin();
    while (it != _cooldowns.end())
    {
        if (it->second.remainingMs == 0 &&
            !it->second.isChanneling &&
            it->second.charges >= it->second.maxCharges)
        {
            it = _cooldowns.erase(it);
        }
        else
        {
            ++it;
        }
    }

    TC_LOG_DEBUG("playerbot.cooldown", "Cleaned up expired cooldowns, {} active", _cooldowns.size());
}

// CooldownCalculator implementation

std::unordered_map<uint32, uint32> CooldownCalculator::_cooldownCache;
std::unordered_map<uint32, bool> CooldownCalculator::_gcdCache;
std::mutex CooldownCalculator::_cacheMutex;

uint32 CooldownCalculator::CalculateSpellCooldown(uint32 spellId, Player* caster)
{
    if (!spellId || !caster)
        return 0;

    // Check cache first
    {
        std::lock_guard<std::mutex> lock(_cacheMutex);
        auto it = _cooldownCache.find(spellId);
        if (it != _cooldownCache.end())
            return it->second;
    }

    // Get spell info
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return 0;

    uint32 cooldown = spellInfo->RecoveryTime;

    // Cache the result
    {
        std::lock_guard<std::mutex> lock(_cacheMutex);
        _cooldownCache[spellId] = cooldown;
    }

    return cooldown;
}

uint32 CooldownCalculator::CalculateGCD(uint32 spellId, Player* caster)
{
    if (!spellId || !caster)
        return 1500; // Default GCD

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return 1500;

    // Most spells use the standard GCD
    // TODO: Apply haste calculations and spell-specific GCD modifications
    return 1500;
}

bool CooldownCalculator::TriggersGCD(uint32 spellId)
{
    if (!spellId)
        return false;

    // Check cache first
    {
        std::lock_guard<std::mutex> lock(_cacheMutex);
        auto it = _gcdCache.find(spellId);
        if (it != _gcdCache.end())
            return it->second;
    }

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return false;

    // Most spells trigger GCD, exceptions are rare
    bool triggersGCD = true; // Default assumption

    // Cache the result
    {
        std::lock_guard<std::mutex> lock(_cacheMutex);
        _gcdCache[spellId] = triggersGCD;
    }

    return triggersGCD;
}

uint32 CooldownCalculator::CalculateChargeRechargeTime(uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return 0;

    // For charge-based spells, recharge time is typically the base cooldown
    return spellInfo->RecoveryTime;
}

uint32 CooldownCalculator::GetSpellCharges(uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return 1;

    // Most spells have 1 charge, specific spells may have more
    // TODO: Implement charge detection from spell data
    return 1;
}

uint32 CooldownCalculator::ApplyHaste(uint32 cooldownMs, float hastePercent)
{
    if (hastePercent <= 0.0f)
        return cooldownMs;

    float reduction = 1.0f / (1.0f + hastePercent / 100.0f);
    return static_cast<uint32>(cooldownMs * reduction);
}

uint32 CooldownCalculator::ApplyCooldownReduction(uint32 cooldownMs, Player* caster, uint32 spellId)
{
    if (!caster || !spellId)
        return cooldownMs;

    // TODO: Apply various cooldown reduction effects
    // - Talents that reduce cooldowns
    // - Gear effects
    // - Temporary buffs

    return cooldownMs;
}

void CooldownCalculator::CacheSpellData(uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return;

    std::lock_guard<std::mutex> lock(_cacheMutex);
    _cooldownCache[spellId] = spellInfo->RecoveryTime;
    _gcdCache[spellId] = true; // Default assumption
}

} // namespace Playerbot