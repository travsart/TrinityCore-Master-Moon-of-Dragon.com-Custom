/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license
 * Copyright (C) 2008-2016 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 */

#include "DefensiveManager.h"
#include "Player.h"
#include "SpellAuras.h"
#include "ThreatManager.h"
#include "Log.h"
#include <algorithm>

namespace Playerbot
{

DefensiveManager::DefensiveManager(Player* bot)
    : _bot(bot)
    , _recentDamage(0.0f)
    , _lastUpdate(0)
{
}

void DefensiveManager::Update(uint32 diff, const CombatMetrics& metrics)
{
    if (!_bot)
        return;

    _lastUpdate += diff;

    if (_lastUpdate < UPDATE_INTERVAL)
        return;

    _lastUpdate = 0;

    // Update damage tracking
    UpdateDamageTracking(metrics);
}

void DefensiveManager::Reset()
{
    _cooldownTracker.clear();
    _recentDamage = 0.0f;
    _lastUpdate = 0;
}

bool DefensiveManager::NeedsDefensive()
{
    if (!_bot)
        return false;

    float healthPct = _bot->GetHealthPct();
    float incomingDamage = EstimateIncomingDamage();

    return ShouldUseDefensive(healthPct, incomingDamage);
}

bool DefensiveManager::NeedsEmergencyDefensive()
{
    if (!_bot)
        return false;

    float healthPct = _bot->GetHealthPct();

    // Emergency threshold: < 20% HP
    return healthPct < 20.0f;
}

uint32 DefensiveManager::GetRecommendedDefensive()
{
    if (!_bot)
        return 0;

    float healthPct = _bot->GetHealthPct();
    float incomingDamage = EstimateIncomingDamage();

    return GetBestDefensive(healthPct, incomingDamage);
}

uint32 DefensiveManager::UseEmergencyDefensive()
{
    if (!_bot)
        return 0;

    // Get emergency defensives only
    std::vector<DefensiveCooldown*> emergencies;

    for (auto& defensive : _availableDefensives)
    {
        if (defensive.isEmergency && defensive.IsAvailable() && !IsOnCooldown(defensive.spellId))
            emergencies.push_back(&defensive);
    }

    if (emergencies.empty())
        return 0;

    // Sort by damage reduction (highest first)
    std::sort(emergencies.begin(), emergencies.end(),
        [](const DefensiveCooldown* a, const DefensiveCooldown* b) {
            return a->damageReduction > b->damageReduction;
        });

    // Use best emergency defensive
    DefensiveCooldown* best = emergencies.front();
    best->MarkUsed();
    _cooldownTracker[best->spellId] = getMSTime();

    TC_LOG_DEBUG("playerbot", "DefensiveManager: {} using EMERGENCY defensive {}",
        _bot->GetName(), best->spellId);

    return best->spellId;
}

void DefensiveManager::RegisterDefensive(const DefensiveCooldown& cooldown)
{
    _availableDefensives.push_back(cooldown);
}

void DefensiveManager::UseDefensiveCooldown(uint32 spellId)
{
    DefensiveCooldown* defensive = FindDefensive(spellId);
    if (!defensive)
        return;

    defensive->MarkUsed();
    _cooldownTracker[spellId] = getMSTime();

    TC_LOG_DEBUG("playerbot", "DefensiveManager: {} used defensive {}",
        _bot ? _bot->GetName() : "unknown", spellId);
}

bool DefensiveManager::ShouldUseDefensive(float healthPercent, float incomingDamage)
{
    if (!_bot)
        return false;

    // Check if any defensive is available
    if (GetAvailableDefensives(DefensivePriority::EMERGENCY).empty())
        return false;

    // Already has active defensive? Don't stack
    if (HasActiveDefensive())
        return false;

    // Determine threshold based on role
    float threshold = 60.0f;  // Default
    // TODO: Adjust based on role (tank = 80%, healer = 70%, DPS = 50%)

    // Need defensive if below threshold or high incoming damage
    return (healthPercent < threshold) || (incomingDamage > (_bot->GetMaxHealth() * 0.3f));
}

uint32 DefensiveManager::GetBestDefensive(float healthPercent, float incomingDamage)
{
    if (!_bot)
        return 0;

    // Determine priority threshold based on health
    DefensivePriority minPriority = DefensivePriority::OPTIONAL;

    if (healthPercent < 20.0f)
        minPriority = DefensivePriority::EMERGENCY;
    else if (healthPercent < 40.0f)
        minPriority = DefensivePriority::HIGH;
    else if (healthPercent < 60.0f)
        minPriority = DefensivePriority::MEDIUM;
    else if (healthPercent < 80.0f)
        minPriority = DefensivePriority::LOW;

    // Get available defensives
    std::vector<DefensiveCooldown*> available = GetAvailableDefensives(minPriority);

    if (available.empty())
        return 0;

    // Sort by priority (emergency first, then damage reduction)
    std::sort(available.begin(), available.end(),
        [](const DefensiveCooldown* a, const DefensiveCooldown* b) {
            if (a->priority != b->priority)
                return a->priority < b->priority;  // Lower enum value = higher priority
            return a->damageReduction > b->damageReduction;
        });

    return available.front()->spellId;
}

bool DefensiveManager::IsOnCooldown(uint32 spellId) const
{
    auto it = _cooldownTracker.find(spellId);
    if (it == _cooldownTracker.end())
        return false;

    DefensiveCooldown* defensive = const_cast<DefensiveManager*>(this)->FindDefensive(spellId);
    if (!defensive)
        return false;

    uint32 now = getMSTime();
    return (now - it->second) < defensive->cooldown;
}

uint32 DefensiveManager::GetRemainingCooldown(uint32 spellId) const
{
    auto it = _cooldownTracker.find(spellId);
    if (it == _cooldownTracker.end())
        return 0;

    DefensiveCooldown* defensive = const_cast<DefensiveManager*>(this)->FindDefensive(spellId);
    if (!defensive)
        return 0;

    uint32 now = getMSTime();
    uint32 elapsed = now - it->second;

    if (elapsed >= defensive->cooldown)
        return 0;

    return defensive->cooldown - elapsed;
}

float DefensiveManager::EstimateIncomingDamage() const
{
    if (!_bot)
        return 0.0f;

    // Base estimate on recent damage
    float estimate = _recentDamage;

    // Multiply by enemy count
    ThreatManager& threatMgr = _bot->GetThreatManager();
    std::list<HostileReference*> const& threatList = threatMgr.GetThreatList();

    uint32 enemyCount = 0;
    for (HostileReference* ref : threatList)
    {
        if (ref && ref->GetOwner() && !ref->GetOwner()->isDead())
            ++enemyCount;
    }

    if (enemyCount > 0)
        estimate *= std::min(enemyCount, 5u);  // Cap at 5× multiplier

    return estimate;
}

// Private helper functions

DefensiveCooldown* DefensiveManager::FindDefensive(uint32 spellId)
{
    for (auto& defensive : _availableDefensives)
    {
        if (defensive.spellId == spellId)
            return &defensive;
    }
    return nullptr;
}

std::vector<DefensiveCooldown*> DefensiveManager::GetAvailableDefensives(DefensivePriority minPriority)
{
    std::vector<DefensiveCooldown*> available;

    for (auto& defensive : _availableDefensives)
    {
        // Check priority
        if (defensive.priority > minPriority)
            continue;

        // Check availability
        if (!defensive.IsAvailable())
            continue;

        // Check cooldown
        if (IsOnCooldown(defensive.spellId))
            continue;

        available.push_back(&defensive);
    }

    return available;
}

bool DefensiveManager::HasActiveDefensive() const
{
    if (!_bot)
        return false;

    // Check for active major defensive auras
    // TODO: Implement comprehensive defensive aura detection
    // For now, check for common DR auras

    return _bot->HasAuraType(SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN);
}

float DefensiveManager::GetHealthDeficit() const
{
    if (!_bot)
        return 0.0f;

    return 100.0f - _bot->GetHealthPct();
}

void DefensiveManager::UpdateDamageTracking(const CombatMetrics& metrics)
{
    // TODO: Implement damage tracking from combat metrics
    // This requires integration with combat metrics system
    // For now, use simple heuristic

    if (!_bot)
        return;

    // Estimate damage as inverse of health change
    // This is a placeholder - should be replaced with actual metrics
    _recentDamage = 0.0f;
}

} // namespace Playerbot
