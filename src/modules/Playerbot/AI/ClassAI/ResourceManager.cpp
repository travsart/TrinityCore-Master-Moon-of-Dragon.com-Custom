/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "ResourceManager.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "DB2Structure.h"
#include "Log.h"
#include <algorithm>

namespace Playerbot
{

ResourceManager::ResourceManager(Player* bot) : _bot(bot), _runicPower(0)
{
    // Initialize runes to available state
    for (uint32 i = 0; i < MAX_RUNES; ++i)
    {
        _runes[i].available = true;
        _runes[i].cooldownRemaining = 0;
        _runes[i].type = (i < 2) ? 0 : ((i < 4) ? 1 : 2); // 2 Blood, 2 Frost, 2 Unholy
    }

    TC_LOG_DEBUG("playerbot.resource", "ResourceManager initialized for bot {}", _bot ? _bot->GetName() : "unknown");
}

void ResourceManager::Update(uint32 diff)
{
    _updateCount.fetch_add(1);

    // Sync with player state
    SyncWithPlayer();

    // Update resource regeneration
    for (auto& pair : _resources)
    {
        UpdateResourceRegeneration(pair.second, diff);
    }

    // Update runes for Death Knights
    if (_bot && _bot->GetClass() == CLASS_DEATH_KNIGHT)
    {
        UpdateRunes(diff);
    }

    // Performance monitoring
    uint32 currentTime = getMSTime();
    if (currentTime - _lastPerformanceCheck > PERFORMANCE_CHECK_INTERVAL)
    {
        _lastPerformanceCheck = currentTime;
        // Could add performance metrics here
    }
}

void ResourceManager::Initialize()
{
    if (!_bot)
        return;

    InitializeClassResources();
    LoadSpellResourceCosts();
    SyncWithPlayer();

    TC_LOG_DEBUG("playerbot.resource", "ResourceManager initialized for class {}",
                 static_cast<uint32>(_bot->GetClass()));
}

bool ResourceManager::HasEnoughResource(uint32 spellId)
{
    if (!spellId || !_bot)
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return false;

    // Check spell power costs using modern API
    std::vector<SpellPowerCost> costs = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask());
    if (costs.empty())
        return true; // No resource cost

    // Check if we have enough of each required resource
    for (const SpellPowerCost& powerCost : costs)
    {
        if (powerCost.Amount > 0)
        {
            ResourceType resourceType = GetResourceTypeForPower(powerCost.Power);
            if (!HasEnoughResource(resourceType, powerCost.Amount))
                return false;
        }
    }

    return true;
}

bool ResourceManager::HasEnoughResource(ResourceType type, uint32 amount)
{
    auto it = _resources.find(type);
    if (it != _resources.end())
    {
        return it->second.HasEnough(amount);
    }

    // Fallback to player power if not tracked
    Powers powerType = GetPowerTypeForResource(type);
    return _bot && _bot->GetPower(powerType) >= amount;
}

uint32 ResourceManager::GetResource(ResourceType type)
{
    auto it = _resources.find(type);
    if (it != _resources.end())
    {
        return it->second.current;
    }

    // Fallback to player power
    Powers powerType = GetPowerTypeForResource(type);
    return _bot ? _bot->GetPower(powerType) : 0;
}

uint32 ResourceManager::GetMaxResource(ResourceType type)
{
    auto it = _resources.find(type);
    if (it != _resources.end())
    {
        return it->second.maximum;
    }

    // Fallback to player power
    Powers powerType = GetPowerTypeForResource(type);
    return _bot ? _bot->GetMaxPower(powerType) : 0;
}

float ResourceManager::GetResourcePercent(ResourceType type)
{
    auto it = _resources.find(type);
    if (it != _resources.end())
    {
        return it->second.GetPercent();
    }

    // Fallback to player power
    Powers powerType = GetPowerTypeForResource(type);
    if (_bot)
    {
        uint32 max = _bot->GetMaxPower(powerType);
        return max > 0 ? static_cast<float>(_bot->GetPower(powerType)) / max : 0.0f;
    }

    return 0.0f;
}

void ResourceManager::ConsumeResource(uint32 spellId)
{
    if (!spellId || !_bot)
        return;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return;

    // Get power costs using modern API
    std::vector<SpellPowerCost> costs = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask());

    for (const SpellPowerCost& powerCost : costs)
    {
        if (powerCost.Amount > 0)
        {
            ResourceType resourceType = GetResourceTypeForPower(powerCost.Power);
            ConsumeResource(resourceType, powerCost.Amount);

            // Record usage for tracking
            RecordResourceUsage(resourceType, powerCost.Amount, spellId);
        }
    }
}

void ResourceManager::ConsumeResource(ResourceType type, uint32 amount)
{
    auto it = _resources.find(type);
    if (it != _resources.end())
    {
        uint32 consumed = it->second.Consume(amount);
        _totalConsumed[type] += consumed;

        TC_LOG_DEBUG("playerbot.resource", "Consumed {} {} ({}%)",
                     consumed, static_cast<uint32>(type), it->second.GetPercent() * 100.0f);
    }
}

void ResourceManager::AddResource(ResourceType type, uint32 amount)
{
    auto it = _resources.find(type);
    if (it != _resources.end())
    {
        uint32 added = it->second.Add(amount);
        _totalGenerated[type] += added;

        TC_LOG_DEBUG("playerbot.resource", "Added {} {} ({}%)",
                     added, static_cast<uint32>(type), it->second.GetPercent() * 100.0f);
    }
}

void ResourceManager::SetResource(ResourceType type, uint32 amount)
{
    auto it = _resources.find(type);
    if (it != _resources.end())
    {
        it->second.current = std::min(amount, it->second.maximum);
    }
}

void ResourceManager::SetMaxResource(ResourceType type, uint32 amount)
{
    auto it = _resources.find(type);
    if (it != _resources.end())
    {
        it->second.maximum = amount;
        it->second.current = std::min(it->second.current, amount);
    }
}

uint32 ResourceManager::GetResourceIn(ResourceType type, uint32 timeMs)
{
    auto it = _resources.find(type);
    if (it != _resources.end() && it->second.isRegenerated)
    {
        float regenAmount = it->second.regenRate * (timeMs / 1000.0f);
        uint32 futureAmount = it->second.current + static_cast<uint32>(regenAmount);
        return std::min(futureAmount, it->second.maximum);
    }

    return GetResource(type);
}

bool ResourceManager::WillHaveEnoughIn(ResourceType type, uint32 amount, uint32 timeMs)
{
    return GetResourceIn(type, timeMs) >= amount;
}

uint32 ResourceManager::GetTimeToResource(ResourceType type, uint32 amount)
{
    auto it = _resources.find(type);
    if (it != _resources.end() && it->second.isRegenerated && it->second.regenRate > 0.0f)
    {
        if (it->second.current >= amount)
            return 0;

        uint32 needed = amount - it->second.current;
        return static_cast<uint32>((needed / it->second.regenRate) * 1000.0f);
    }

    return UINT32_MAX; // Never
}

uint32 ResourceManager::GetComboPoints()
{
    return GetResource(ResourceType::COMBO_POINTS);
}

void ResourceManager::ConsumeComboPoints()
{
    ConsumeResource(ResourceType::COMBO_POINTS, GetComboPoints());
}

void ResourceManager::AddComboPoints(uint32 points)
{
    AddResource(ResourceType::COMBO_POINTS, points);
}

uint32 ResourceManager::GetHolyPower()
{
    return GetResource(ResourceType::HOLY_POWER);
}

void ResourceManager::ConsumeHolyPower()
{
    ConsumeResource(ResourceType::HOLY_POWER, GetHolyPower());
}

void ResourceManager::AddHolyPower(uint32 power)
{
    AddResource(ResourceType::HOLY_POWER, power);
}

uint32 ResourceManager::GetChi()
{
    return GetResource(ResourceType::CHI);
}

void ResourceManager::ConsumeChi(uint32 amount)
{
    ConsumeResource(ResourceType::CHI, amount);
}

void ResourceManager::AddChi(uint32 amount)
{
    AddResource(ResourceType::CHI, amount);
}

std::vector<ResourceManager::RuneInfo> ResourceManager::GetRunes()
{
    std::vector<RuneInfo> result;
    for (uint32 i = 0; i < MAX_RUNES; ++i)
    {
        result.push_back(_runes[i]);
    }
    return result;
}

uint32 ResourceManager::GetAvailableRunes(uint8 runeType)
{
    uint32 count = 0;
    for (uint32 i = 0; i < MAX_RUNES; ++i)
    {
        if (_runes[i].available && (runeType == 255 || _runes[i].type == runeType))
        {
            count++;
        }
    }
    return count;
}

bool ResourceManager::HasRunesAvailable(uint32 bloodRunes, uint32 frostRunes, uint32 unholyRunes)
{
    return GetAvailableRunes(0) >= bloodRunes &&
           GetAvailableRunes(1) >= frostRunes &&
           GetAvailableRunes(2) >= unholyRunes;
}

void ResourceManager::ConsumeRunes(uint32 bloodRunes, uint32 frostRunes, uint32 unholyRunes)
{
    auto consumeRuneType = [this](uint8 runeType, uint32 count) {
        uint32 consumed = 0;
        for (uint32 i = 0; i < MAX_RUNES && consumed < count; ++i)
        {
            if (_runes[i].available && _runes[i].type == runeType)
            {
                _runes[i].available = false;
                _runes[i].cooldownRemaining = RUNE_COOLDOWN_MS;
                consumed++;
            }
        }
    };

    consumeRuneType(0, bloodRunes);
    consumeRuneType(1, frostRunes);
    consumeRuneType(2, unholyRunes);

    TC_LOG_DEBUG("playerbot.resource", "Consumed runes: {} blood, {} frost, {} unholy",
                 bloodRunes, frostRunes, unholyRunes);
}

void ResourceManager::RecordResourceUsage(ResourceType type, uint32 amount, uint32 spellId)
{
    _spellResourceCost[spellId] = amount;
    _spellUsageCount[spellId]++;

    // Report to monitor
    if (_bot)
    {
        ResourceMonitor::Instance().RecordResourceUsage(_bot->GetGUID().GetCounter(), type, amount);
    }
}

float ResourceManager::GetResourceEfficiency(ResourceType type)
{
    auto totalGenerated = _totalGenerated.find(type);
    auto totalConsumed = _totalConsumed.find(type);

    if (totalGenerated != _totalGenerated.end() && totalConsumed != _totalConsumed.end())
    {
        uint32 generated = totalGenerated->second;
        uint32 consumed = totalConsumed->second;

        if (generated > 0)
        {
            return static_cast<float>(consumed) / generated;
        }
    }

    return 1.0f; // Perfect efficiency if no data
}

float ResourceManager::GetSpellResourceEfficiency(uint32 spellId)
{
    auto usageIt = _spellUsageCount.find(spellId);
    auto costIt = _spellResourceCost.find(spellId);

    if (usageIt != _spellUsageCount.end() && costIt != _spellResourceCost.end())
    {
        // Simple efficiency metric: usage frequency vs cost
        return static_cast<float>(usageIt->second) / costIt->second;
    }

    return 1.0f;
}

bool ResourceManager::CanAffordSpellSequence(const std::vector<uint32>& spellIds)
{
    std::unordered_map<ResourceType, uint32> totalCosts;

    for (uint32 spellId : spellIds)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
        if (spellInfo)
        {
            // Get power costs for this spell
            std::vector<SpellPowerCost> costs = spellInfo->CalcPowerCost(_bot, spellInfo->GetSchoolMask());
            for (const SpellPowerCost& powerCost : costs)
            {
                ResourceType type = GetResourceTypeForPower(powerCost.Power);
                uint32 cost = powerCost.Amount;
                totalCosts[type] += cost;
            }
        }
    }

    for (const auto& pair : totalCosts)
    {
        if (!HasEnoughResource(pair.first, pair.second))
            return false;
    }

    return true;
}

uint32 ResourceManager::GetOptimalResourceThreshold(ResourceType type)
{
    uint32 maxResource = GetMaxResource(type);

    // Different thresholds for different resource types
    switch (type)
    {
        case ResourceType::MANA:
            return maxResource * 30 / 100; // 30% for mana
        case ResourceType::ENERGY:
            return maxResource * 40 / 100; // 40% for energy
        case ResourceType::RAGE:
            return maxResource * 20 / 100; // 20% for rage
        default:
            return maxResource * 25 / 100; // 25% default
    }
}

bool ResourceManager::ShouldConserveResource(ResourceType type)
{
    float currentPercent = GetResourcePercent(type);
    return currentPercent < CONSERVATION_THRESHOLD;
}

bool ResourceManager::IsResourceCritical(ResourceType type)
{
    float currentPercent = GetResourcePercent(type);
    return currentPercent < CRITICAL_RESOURCE_THRESHOLD;
}

bool ResourceManager::NeedsResourceEmergency()
{
    ResourceType primaryResource = GetPrimaryResourceType();
    return IsResourceCritical(primaryResource);
}

std::vector<uint32> ResourceManager::GetResourceEmergencySpells()
{
    std::vector<uint32> emergencySpells;

    if (!_bot)
        return emergencySpells;

    // Class-specific emergency resource spells
    switch (_bot->GetClass())
    {
        case CLASS_WARRIOR:
            // Berserker Rage, etc.
            break;
        case CLASS_ROGUE:
            // Adrenaline Rush, etc.
            break;
        case CLASS_MAGE:
            // Evocation, Mana Gem, etc.
            break;
        // Add other classes as needed
    }

    return emergencySpells;
}

uint32 ResourceManager::GetTotalResourceGenerated(ResourceType type)
{
    auto it = _totalGenerated.find(type);
    return (it != _totalGenerated.end()) ? it->second : 0;
}

uint32 ResourceManager::GetTotalResourceConsumed(ResourceType type)
{
    auto it = _totalConsumed.find(type);
    return (it != _totalConsumed.end()) ? it->second : 0;
}

float ResourceManager::GetAverageResourceUsage(ResourceType type)
{
    uint32 consumed = GetTotalResourceConsumed(type);
    uint32 updates = _updateCount.load();

    return updates > 0 ? static_cast<float>(consumed) / updates : 0.0f;
}

void ResourceManager::DumpResourceState()
{
    if (!_bot)
        return;

    TC_LOG_DEBUG("playerbot.resource", "=== Resource Manager Dump for {} ===", _bot->GetName());

    for (const auto& pair : _resources)
    {
        const ResourceInfo& info = pair.second;
        TC_LOG_DEBUG("playerbot.resource", "{}: {}/{} ({}%) - Regen: {}/sec",
                     static_cast<uint32>(pair.first), info.current, info.maximum,
                     static_cast<uint32>(info.GetPercent() * 100), info.regenRate);
    }

    if (_bot->GetClass() == CLASS_DEATH_KNIGHT)
    {
        TC_LOG_DEBUG("playerbot.resource", "Runic Power: {}", _runicPower);
        for (uint32 i = 0; i < MAX_RUNES; ++i)
        {
            TC_LOG_DEBUG("playerbot.resource", "Rune {}: {} (cooldown: {}ms)",
                         i, _runes[i].available ? "Available" : "On cooldown", _runes[i].cooldownRemaining);
        }
    }
}

ResourceInfo ResourceManager::GetResourceInfo(ResourceType type)
{
    auto it = _resources.find(type);
    if (it != _resources.end())
    {
        return it->second;
    }
    return ResourceInfo();
}

void ResourceManager::UpdateResourceRegeneration(ResourceInfo& resource, uint32 diff)
{
    if (!resource.isRegenerated || resource.regenRate <= 0.0f)
        return;

    if (resource.current >= resource.maximum)
        return;

    float regenAmount = resource.regenRate * (diff / 1000.0f);
    uint32 regenInt = static_cast<uint32>(regenAmount);

    if (regenInt > 0)
    {
        resource.Add(regenInt);
        resource.lastUpdate = getMSTime();
    }
}

void ResourceManager::UpdateRunes(uint32 diff)
{
    for (uint32 i = 0; i < MAX_RUNES; ++i)
    {
        if (!_runes[i].available && _runes[i].cooldownRemaining > 0)
        {
            if (_runes[i].cooldownRemaining > diff)
            {
                _runes[i].cooldownRemaining -= diff;
            }
            else
            {
                _runes[i].cooldownRemaining = 0;
                _runes[i].available = true;
            }
        }
    }
}

void ResourceManager::SyncWithPlayer()
{
    if (!_bot)
        return;

    // Sync primary resource
    ResourceType primaryType = GetPrimaryResourceType();
    Powers primaryPower = GetPowerTypeForResource(primaryType);

    auto it = _resources.find(primaryType);
    if (it != _resources.end())
    {
        it->second.current = _bot->GetPower(primaryPower);
        it->second.maximum = _bot->GetMaxPower(primaryPower);
    }
    else
    {
        // Create resource info if it doesn't exist
        _resources[primaryType] = ResourceInfo(primaryType,
                                               _bot->GetPower(primaryPower),
                                               _bot->GetMaxPower(primaryPower));
    }

    // Sync class-specific resources
    switch (_bot->GetClass())
    {
        case CLASS_ROGUE:
        case CLASS_DRUID: // In cat form
            // Combo points
            if (_bot->GetPower(POWER_COMBO_POINTS) != GetComboPoints())
            {
                SetResource(ResourceType::COMBO_POINTS, _bot->GetPower(POWER_COMBO_POINTS));
            }
            break;

        case CLASS_PALADIN:
            // Holy Power
            if (_bot->GetPower(POWER_HOLY_POWER) != GetHolyPower())
            {
                SetResource(ResourceType::HOLY_POWER, _bot->GetPower(POWER_HOLY_POWER));
            }
            break;

        case CLASS_MONK:
            // Chi
            if (_bot->GetPower(POWER_CHI) != GetChi())
            {
                SetResource(ResourceType::CHI, _bot->GetPower(POWER_CHI));
            }
            break;

        case CLASS_DEATH_KNIGHT:
            // Runic Power
            _runicPower = _bot->GetPower(POWER_RUNIC_POWER);
            break;
    }
}

void ResourceManager::InitializeClassResources()
{
    if (!_bot)
        return;

    ResourceType primaryType = GetPrimaryResourceType();
    Powers primaryPower = GetPowerTypeForResource(primaryType);

    // Initialize primary resource
    float regenRate = 0.0f;
    switch (primaryType)
    {
        case ResourceType::MANA:
            regenRate = ResourceCalculator::CalculateManaRegen(_bot);
            break;
        case ResourceType::ENERGY:
            regenRate = ResourceCalculator::CalculateEnergyRegen(_bot);
            break;
        case ResourceType::RAGE:
            regenRate = -ResourceCalculator::CalculateRageDecay(_bot); // Decay is negative regen
            break;
    }

    _resources[primaryType] = ResourceInfo(primaryType,
                                           _bot->GetPower(primaryPower),
                                           _bot->GetMaxPower(primaryPower),
                                           regenRate);

    // Initialize class-specific secondary resources
    switch (_bot->GetClass())
    {
        case CLASS_ROGUE:
        case CLASS_DRUID:
            _resources[ResourceType::COMBO_POINTS] = ResourceInfo(ResourceType::COMBO_POINTS, 0, 5);
            break;

        case CLASS_PALADIN:
            _resources[ResourceType::HOLY_POWER] = ResourceInfo(ResourceType::HOLY_POWER, 0, 3);
            break;

        case CLASS_MONK:
            _resources[ResourceType::CHI] = ResourceInfo(ResourceType::CHI, 0, 4);
            break;

        case CLASS_WARLOCK:
            _resources[ResourceType::SOUL_SHARDS] = ResourceInfo(ResourceType::SOUL_SHARDS, 0, 3);
            break;
    }
}

void ResourceManager::LoadSpellResourceCosts()
{
    // This could be enhanced to load spell costs from a database or cache
    // For now, we'll calculate them on-demand
}

ResourceType ResourceManager::GetPrimaryResourceType()
{
    if (!_bot)
        return ResourceType::MANA;

    switch (_bot->GetClass())
    {
        case CLASS_WARRIOR:
            return ResourceType::RAGE;
        case CLASS_PALADIN:
        case CLASS_PRIEST:
        case CLASS_MAGE:
        case CLASS_WARLOCK:
            return ResourceType::MANA;
        case CLASS_HUNTER:
            return ResourceType::FOCUS;
        case CLASS_ROGUE:
        case CLASS_MONK:
            return ResourceType::ENERGY;
        case CLASS_DEATH_KNIGHT:
            return ResourceType::RUNIC_POWER;
        case CLASS_SHAMAN:
        case CLASS_DRUID:
            return ResourceType::MANA;
        case CLASS_DEMON_HUNTER:
            return ResourceType::FURY;
        case CLASS_EVOKER:
            return ResourceType::ESSENCE;
        default:
            return ResourceType::MANA;
    }
}

Powers ResourceManager::GetPowerTypeForResource(ResourceType type)
{
    switch (type)
    {
        case ResourceType::MANA: return POWER_MANA;
        case ResourceType::RAGE: return POWER_RAGE;
        case ResourceType::FOCUS: return POWER_FOCUS;
        case ResourceType::ENERGY: return POWER_ENERGY;
        case ResourceType::COMBO_POINTS: return POWER_COMBO_POINTS;
        case ResourceType::RUNES: return POWER_RUNES;
        case ResourceType::RUNIC_POWER: return POWER_RUNIC_POWER;
        case ResourceType::HOLY_POWER: return POWER_HOLY_POWER;
        case ResourceType::CHI: return POWER_CHI;
        default: return POWER_MANA;
    }
}

ResourceType ResourceManager::GetResourceTypeForPower(Powers power)
{
    switch (power)
    {
        case POWER_MANA: return ResourceType::MANA;
        case POWER_RAGE: return ResourceType::RAGE;
        case POWER_FOCUS: return ResourceType::FOCUS;
        case POWER_ENERGY: return ResourceType::ENERGY;
        case POWER_COMBO_POINTS: return ResourceType::COMBO_POINTS;
        case POWER_RUNES: return ResourceType::RUNES;
        case POWER_RUNIC_POWER: return ResourceType::RUNIC_POWER;
        case POWER_HOLY_POWER: return ResourceType::HOLY_POWER;
        case POWER_CHI: return ResourceType::CHI;
        default: return ResourceType::MANA;
    }
}

uint32 ResourceManager::CalculateSpellResourceCost(uint32 spellId, ResourceType type)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo || !_bot)
        return 0;

    Powers requiredPower = GetPowerTypeForResource(type);

    // Check if spell uses the required power type and calculate cost
    for (SpellPowerEntry const* powerEntry : spellInfo->PowerCosts)
    {
        if (!powerEntry)
            continue;

        if (powerEntry->PowerType == requiredPower)
        {
            uint32 baseCost = powerEntry->ManaCost;
            uint32 percentCost = 0;

            // Calculate percentage-based cost
            if (powerEntry->PowerCostPct > 0.0f)
            {
                percentCost = uint32(powerEntry->PowerCostPct * _bot->GetMaxPower(requiredPower) / 100.0f);
            }

            return baseCost + percentCost;
        }
    }

    return 0;
}

bool ResourceManager::IsResourceTypeUsedBySpell(uint32 spellId, ResourceType type)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return false;

    Powers requiredPower = GetPowerTypeForResource(type);

    // Check if any of the spell's power costs match the required type
    for (SpellPowerEntry const* powerEntry : spellInfo->PowerCosts)
    {
        if (powerEntry && powerEntry->PowerType == requiredPower)
            return true;
    }

    return false;
}

// ResourceCalculator implementation

std::unordered_map<uint32, uint32> ResourceCalculator::_manaCostCache;
std::unordered_map<uint32, uint32> ResourceCalculator::_rageCostCache;
std::unordered_map<uint32, uint32> ResourceCalculator::_energyCostCache;
std::mutex ResourceCalculator::_cacheMutex;

uint32 ResourceCalculator::CalculateManaCost(uint32 spellId, Player* caster)
{
    if (!spellId || !caster)
        return 0;

    std::lock_guard<std::mutex> lock(_cacheMutex);
    auto it = _manaCostCache.find(spellId);
    if (it != _manaCostCache.end())
        return it->second;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return 0;

    // Calculate mana cost using modern API
    uint32 cost = 0;
    for (SpellPowerEntry const* powerEntry : spellInfo->PowerCosts)
    {
        if (!powerEntry || powerEntry->PowerType != POWER_MANA)
            continue;

        cost = powerEntry->ManaCost;
        if (powerEntry->PowerCostPct > 0.0f)
        {
            cost += uint32(powerEntry->PowerCostPct * caster->GetMaxPower(POWER_MANA) / 100.0f);
        }
        break; // Use first mana cost found
    }

    if (cost == 0)
        return 0;
    _manaCostCache[spellId] = cost;
    return cost;
}

uint32 ResourceCalculator::CalculateRageCost(uint32 spellId, Player* caster)
{
    if (!spellId || !caster)
        return 0;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return 0;

    // Find rage cost in power costs array
    for (SpellPowerEntry const* powerEntry : spellInfo->PowerCosts)
    {
        if (powerEntry && powerEntry->PowerType == POWER_RAGE)
        {
            return powerEntry->ManaCost;
        }
    }

    return 0;
}

uint32 ResourceCalculator::CalculateEnergyCost(uint32 spellId, Player* caster)
{
    if (!spellId || !caster)
        return 0;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return 0;

    // Find energy cost in power costs array
    for (SpellPowerEntry const* powerEntry : spellInfo->PowerCosts)
    {
        if (powerEntry && powerEntry->PowerType == POWER_ENERGY)
        {
            return powerEntry->ManaCost;
        }
    }

    return 0;
}

uint32 ResourceCalculator::CalculateFocusCost(uint32 spellId, Player* caster)
{
    if (!spellId || !caster)
        return 0;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return 0;

    // Find focus cost in power costs array
    for (SpellPowerEntry const* powerEntry : spellInfo->PowerCosts)
    {
        if (powerEntry && powerEntry->PowerType == POWER_FOCUS)
        {
            return powerEntry->ManaCost;
        }
    }

    return 0;
}

float ResourceCalculator::CalculateManaRegen(Player* player)
{
    if (!player)
        return 0.0f;

    // Base mana regeneration calculation
    // This would need to be enhanced with proper stats calculation
    return 10.0f; // Placeholder
}

float ResourceCalculator::CalculateEnergyRegen(Player* player)
{
    if (!player)
        return 0.0f;

    // Energy regenerates at 10 per second base
    return 10.0f;
}

float ResourceCalculator::CalculateRageDecay(Player* player)
{
    if (!player)
        return 0.0f;

    // Rage decays at varying rates
    return 2.0f; // Placeholder
}

bool ResourceCalculator::IsResourceEfficientSpell(uint32 spellId, Player* caster)
{
    float efficiency = CalculateResourceEfficiency(spellId, caster);
    return efficiency > 1.0f; // Arbitrary threshold
}

float ResourceCalculator::CalculateResourceEfficiency(uint32 spellId, Player* caster)
{
    if (!spellId || !caster)
        return 0.0f;

    // This would calculate damage/healing per resource point
    // Placeholder implementation
    return 1.0f;
}

uint32 ResourceCalculator::GetOptimalResourceLevel(ResourceType type, Player* player)
{
    if (!player)
        return 0;

    // Class and resource-specific optimal levels
    return 50; // Placeholder
}

uint32 ResourceCalculator::GetComboPointsFromSpell(uint32 spellId)
{
    // This would check spell effects for combo point generation
    return 1; // Most combo point spells generate 1 point
}

uint32 ResourceCalculator::GetHolyPowerFromSpell(uint32 spellId)
{
    // This would check spell effects for holy power generation
    return 1;
}

uint32 ResourceCalculator::GetChiFromSpell(uint32 spellId)
{
    // This would check spell effects for chi generation
    return 1;
}

void ResourceCalculator::CacheSpellResourceCost(uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return;

    std::lock_guard<std::mutex> lock(_cacheMutex);

    // Cache costs for each power type the spell uses
    for (SpellPowerEntry const* powerEntry : spellInfo->PowerCosts)
    {
        if (!powerEntry)
            continue;

        switch (powerEntry->PowerType)
        {
            case POWER_MANA:
                _manaCostCache[spellId] = powerEntry->ManaCost;
                break;
            case POWER_RAGE:
                _rageCostCache[spellId] = powerEntry->ManaCost;
                break;
            case POWER_ENERGY:
                _energyCostCache[spellId] = powerEntry->ManaCost;
                break;
            default:
                break;
        }
    }
}

// ResourceMonitor implementation

ResourceMonitor& ResourceMonitor::Instance()
{
    static ResourceMonitor instance;
    return instance;
}

void ResourceMonitor::RecordResourceUsage(uint32 botGuid, ResourceType type, uint32 amount)
{
    std::lock_guard<std::mutex> lock(_dataMutex);
    _botResourceData[botGuid][type].totalUsed += amount;
    _botResourceData[botGuid][type].sampleCount++;
}

void ResourceMonitor::RecordResourceWaste(uint32 botGuid, ResourceType type, uint32 amount)
{
    std::lock_guard<std::mutex> lock(_dataMutex);
    _botResourceData[botGuid][type].totalWasted += amount;
}

void ResourceMonitor::RecordResourceStarvation(uint32 botGuid, ResourceType type, uint32 duration)
{
    std::lock_guard<std::mutex> lock(_dataMutex);
    _botResourceData[botGuid][type].starvationTime += duration;
}

float ResourceMonitor::GetAverageResourceUsage(ResourceType type)
{
    std::lock_guard<std::mutex> lock(_dataMutex);

    uint32 totalUsed = 0;
    uint32 totalSamples = 0;

    for (const auto& botPair : _botResourceData)
    {
        auto typeIt = botPair.second.find(type);
        if (typeIt != botPair.second.end())
        {
            totalUsed += typeIt->second.totalUsed;
            totalSamples += typeIt->second.sampleCount;
        }
    }

    return totalSamples > 0 ? static_cast<float>(totalUsed) / totalSamples : 0.0f;
}

float ResourceMonitor::GetResourceWasteRate(ResourceType type)
{
    std::lock_guard<std::mutex> lock(_dataMutex);

    uint32 totalUsed = 0;
    uint32 totalWasted = 0;

    for (const auto& botPair : _botResourceData)
    {
        auto typeIt = botPair.second.find(type);
        if (typeIt != botPair.second.end())
        {
            totalUsed += typeIt->second.totalUsed;
            totalWasted += typeIt->second.totalWasted;
        }
    }

    uint32 total = totalUsed + totalWasted;
    return total > 0 ? static_cast<float>(totalWasted) / total : 0.0f;
}

uint32 ResourceMonitor::GetResourceStarvationTime(ResourceType type)
{
    std::lock_guard<std::mutex> lock(_dataMutex);

    uint32 totalStarvation = 0;
    for (const auto& botPair : _botResourceData)
    {
        auto typeIt = botPair.second.find(type);
        if (typeIt != botPair.second.end())
        {
            totalStarvation += typeIt->second.starvationTime;
        }
    }

    return totalStarvation;
}

std::vector<std::string> ResourceMonitor::GetResourceOptimizationSuggestions(uint32 botGuid)
{
    std::vector<std::string> suggestions;

    std::lock_guard<std::mutex> lock(_dataMutex);
    auto botIt = _botResourceData.find(botGuid);
    if (botIt == _botResourceData.end())
        return suggestions;

    for (const auto& resourcePair : botIt->second)
    {
        const ResourceUsageData& data = resourcePair.second;

        if (data.totalWasted > data.totalUsed * 0.2f) // 20% waste threshold
        {
            suggestions.push_back("Reduce resource waste for " + std::to_string(static_cast<uint32>(resourcePair.first)));
        }

        if (data.starvationTime > 10000) // 10 seconds of starvation
        {
            suggestions.push_back("Improve resource management for " + std::to_string(static_cast<uint32>(resourcePair.first)));
        }
    }

    return suggestions;
}

} // namespace Playerbot
