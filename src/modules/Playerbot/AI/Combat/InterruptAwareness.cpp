/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

// Combat/ThreatManager.h removed - not used in this file
#include "InterruptAwareness.h"
#include "Player.h"
#include "Unit.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "World.h"
#include "Creature.h"
#include "CellImpl.h"
#include "GridNotifiersImpl.h"
#include "../BotAI.h"
#include "InterruptCoordinator.h"
#include <algorithm>
#include <thread>
#include <chrono>

namespace Playerbot
{

InterruptAwareness::InterruptAwareness(Player* observer)
    : _observer(observer)
    , _lastUpdate(std::chrono::steady_clock::now())
{
    // Set default configuration
    _config.maxDetectionRange = 40.0f;
    _config.detectionIntervalMs = 100;
    _config.detectFriendlySpells = false;
    _config.detectChanneledSpells = true;
    _config.detectInstantCasts = false;
    _config.minCastTime = 500;
    _config.requireLineOfSight = true;

    TC_LOG_DEBUG("playerbot", "InterruptAwareness: Initialized for observer %s",
                 observer ? observer->GetName().c_str() : "nullptr");
}

SpellScanResult InterruptAwareness::Update(uint32 diff)
{
    SpellScanResult result;

    if (!_active.load() || !_observer)
        return result;

    auto updateStart = std::chrono::steady_clock::now();

    // Check if it's time for a scan
    auto timeSinceLastUpdate = std::chrono::duration_cast<std::chrono::milliseconds>(updateStart - _lastUpdate);
    if (timeSinceLastUpdate.count() < _config.detectionIntervalMs)
        return result;

    // Skip scan if performance optimization suggests it
    if (ShouldSkipScan())
        return result;

    // Update active casts (remove expired ones)
    UpdateActiveCasts();

    // Perform the main scan
    result = ScanNearbyUnits();

    // Process completed casts
    ProcessCompletedCasts(result);

    // Update performance metrics
    auto scanTime = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - updateStart);

    {
        std::lock_guard<std::mutex> lock(_metricsMutex);
        _metrics.totalScans.fetch_add(1);
        _metrics.unitsScanned.fetch_add(result.totalUnitsScanned);
        _metrics.spellsDetected.fetch_add(result.totalSpellsDetected);

        // Update timing metrics
        result.scanTime = scanTime;
        if (scanTime > _metrics.maxScanTime)
            _metrics.maxScanTime = scanTime;

        // Rolling average calculation
        auto currentAvg = _metrics.averageScanTime;
        _metrics.averageScanTime = std::chrono::microseconds(
            (currentAvg.count() * 9 + scanTime.count()) / 10
        );
    }

    _lastUpdate = updateStart;
    _lastScanTimeMs = static_cast<uint32>(scanTime.count() / 1000);
    _scanCount++;

    // Periodic optimization
    if (_scanCount % SCAN_OPTIMIZATION_INTERVAL == 0)
    {
        OptimizeForPerformance();
    }

    return result;
}

void InterruptAwareness::SetObserver(Player* observer)
{
    std::lock_guard<std::mutex> lock(_observerMutex);
    _observer = observer;

    TC_LOG_DEBUG("playerbot", "InterruptAwareness: Observer set to %s",
                 observer ? observer->GetName().c_str() : "nullptr");
}

std::vector<DetectedSpellCast> InterruptAwareness::GetActiveCasts() const
{
    std::shared_lock<std::shared_mutex> lock(_castMutex);
    std::vector<DetectedSpellCast> allCasts;

    for (const auto& [casterGuid, casts] : _activeCasts)
    {
        for (const auto& cast : casts)
        {
            if (cast.IsValid())
                allCasts.push_back(cast);
        }
    }

    return allCasts;
}

std::vector<DetectedSpellCast> InterruptAwareness::GetCastsFromUnit(ObjectGuid casterGuid) const
{
    std::shared_lock<std::shared_mutex> lock(_castMutex);
    auto it = _activeCasts.find(casterGuid);
    if (it != _activeCasts.end())
    {
        std::vector<DetectedSpellCast> validCasts;
        for (const auto& cast : it->second)
        {
            if (cast.IsValid())
                validCasts.push_back(cast);
        }
        return validCasts;
    }
    return {};
}

std::vector<DetectedSpellCast> InterruptAwareness::GetInterruptibleCasts(float maxRange) const
{
    std::shared_lock<std::shared_mutex> lock(_castMutex);
    std::vector<DetectedSpellCast> interruptibleCasts;

    for (const auto& [casterGuid, casts] : _activeCasts)
    {
        for (const auto& cast : casts)
        {
            if (cast.IsValid() && cast.isInterruptible)
            {
                // Check range if specified
                if (maxRange > 0.0f && cast.detectionRange > maxRange)
                    continue;

                interruptibleCasts.push_back(cast);
            }
        }
    }

    // Sort by remaining cast time (most urgent first)
    std::sort(interruptibleCasts.begin(), interruptibleCasts.end(),
              [](const DetectedSpellCast& a, const DetectedSpellCast& b) {
                  return a.GetRemainingTime() < b.GetRemainingTime();
              });

    return interruptibleCasts;
}

std::vector<DetectedSpellCast> InterruptAwareness::GetHostileCasts() const
{
    std::shared_lock<std::shared_mutex> lock(_castMutex);
    std::vector<DetectedSpellCast> hostileCasts;

    for (const auto& [casterGuid, casts] : _activeCasts)
    {
        for (const auto& cast : casts)
        {
            if (cast.IsValid() && cast.isHostile)
            {
                hostileCasts.push_back(cast);
            }
        }
    }

    return hostileCasts;
}

bool InterruptAwareness::IsUnitCasting(ObjectGuid unitGuid) const
{
    std::shared_lock<std::shared_mutex> lock(_castMutex);
    auto it = _activeCasts.find(unitGuid);
    if (it != _activeCasts.end())
    {
        for (const auto& cast : it->second)
        {
            if (cast.IsValid())
                return true;
        }
    }
    return false;
}

DetectedSpellCast const* InterruptAwareness::GetSpellCast(ObjectGuid casterGuid, uint32 spellId) const
{
    std::shared_lock<std::shared_mutex> lock(_castMutex);
    auto it = _activeCasts.find(casterGuid);
    if (it != _activeCasts.end())
    {
        for (const auto& cast : it->second)
        {
            if (cast.IsValid() && cast.spellId == spellId)
                return &cast;
        }
    }
    return nullptr;
}

void InterruptAwareness::SetInterruptCoordinator(std::shared_ptr<InterruptCoordinator> coordinator)
{
    _coordinator = coordinator;
    TC_LOG_DEBUG("playerbot", "InterruptAwareness: Interrupt coordinator set");
}

void InterruptAwareness::RegisterSpellCastCallback(std::function<void(DetectedSpellCast const&)> callback)
{
    std::lock_guard<std::mutex> lock(_callbackMutex);
    _spellCastCallbacks.push_back(callback);
}

void InterruptAwareness::RegisterSpellCompleteCallback(std::function<void(ObjectGuid, uint32, bool)> callback)
{
    std::lock_guard<std::mutex> lock(_callbackMutex);
    _spellCompleteCallbacks.push_back(callback);
}

size_t InterruptAwareness::GetActiveCastCount() const
{
    std::shared_lock<std::shared_mutex> lock(_castMutex);
    size_t count = 0;
    for (const auto& [casterGuid, casts] : _activeCasts)
    {
        for (const auto& cast : casts)
        {
            if (cast.IsValid())
                count++;
        }
    }
    return count;
}

void InterruptAwareness::ResetMetrics()
{
    std::lock_guard<std::mutex> lock(_metricsMutex);
    _metrics.totalScans.store(0);
    _metrics.unitsScanned.store(0);
    _metrics.spellsDetected.store(0);
    _metrics.interruptibleSpells.store(0);
    _metrics.hostileSpells.store(0);
    _metrics.averageScanTime = std::chrono::microseconds{0};
    _metrics.maxScanTime = std::chrono::microseconds{0};
}

void InterruptAwareness::AddSpellPattern(uint32 npcId, std::vector<uint32> const& spellSequence)
{
    if (spellSequence.empty())
        return;

    std::unique_lock<std::shared_mutex> lock(_patternMutex);
    SpellPattern pattern;
    pattern.npcId = npcId;
    pattern.spellSequence = spellSequence;
    pattern.lastMatch = std::chrono::steady_clock::now();
    pattern.currentIndex = 0;

    _spellPatterns[npcId] = pattern;

    TC_LOG_DEBUG("playerbot", "InterruptAwareness: Added spell pattern for NPC %u with %u spells",
                 npcId, static_cast<uint32>(spellSequence.size()));
}

void InterruptAwareness::ClearSpellPatterns()
{
    std::unique_lock<std::shared_mutex> lock(_patternMutex);
    _spellPatterns.clear();
}

void InterruptAwareness::SetScanPriority(CreatureType creatureType, uint32 priority)
{
    std::unique_lock<std::shared_mutex> lock(_priorityMutex);
    _creatureTypePriorities[creatureType] = priority;
}

void InterruptAwareness::SetScanPriority(uint32 npcId, uint32 priority)
{
    std::unique_lock<std::shared_mutex> lock(_priorityMutex);
    _npcPriorities[npcId] = priority;
}

std::vector<uint32> InterruptAwareness::PredictUpcomingSpells(ObjectGuid casterGuid) const
{
    if (!_enablePatterns)
        return {};

    // This would implement predictive spell detection based on known patterns
    // For now, return empty vector
    return {};
}

SpellScanResult InterruptAwareness::ScanNearbyUnits()
{
    SpellScanResult result;

    if (!_observer)
        return result;

    // Get nearby units to scan
    std::vector<Unit*> units = GetNearbyUnits();
    result.totalUnitsScanned = static_cast<uint32>(units.size());

    // Limit scanning to prevent performance issues
    if (units.size() > MAX_SCAN_UNITS)
    {
        // Sort by priority and distance, then take the top ones
        std::sort(units.begin(), units.end(),
                 [this](Unit* a, Unit* b) {
                     uint32 priorityA = GetUnitScanPriority(a);
                     uint32 priorityB = GetUnitScanPriority(b);
                     if (priorityA != priorityB)
                         return priorityA > priorityB;

                     // If priorities are equal, prefer closer units
                     float distA = _observer->GetDistance(a);
                     float distB = _observer->GetDistance(b);
                     return distA < distB;
                 });

        units.resize(MAX_SCAN_UNITS);
    }

    // Process each unit
    for (Unit* unit : units)
    {
        if (ShouldScanUnit(unit))
        {
            ProcessUnit(unit, result);
        }
    }

    return result;
}

void InterruptAwareness::ProcessUnit(Unit* unit, SpellScanResult& result)
{
    if (!unit)
        return;

    ObjectGuid unitGuid = unit->GetGUID();

    // Check for current spell casting
    if (unit->HasUnitState(UNIT_STATE_CASTING))
    {
        // Check generic spell
        if (Spell* currentSpell = unit->GetCurrentSpell(CURRENT_GENERIC_SPELL))
        {
            if (ShouldDetectSpell(unit, currentSpell))
            {
                DetectedSpellCast cast = AnalyzeSpellCast(unit, currentSpell);
                if (cast.IsValid())
                {
                    // Check if this is a new cast or an update
                    bool isNewCast = true;
                    {
                        std::shared_lock<std::shared_mutex> lock(_castMutex);
                        auto it = _activeCasts.find(unitGuid);
                        if (it != _activeCasts.end())
                        {
                            for (const auto& existingCast : it->second)
                            {
                                if (existingCast.spellId == cast.spellId)
                                {
                                    isNewCast = false;
                                    break;
                                }
                            }
                        }
                    }

                    if (isNewCast)
                    {
                        AddDetectedCast(cast);
                        result.newCasts.push_back(cast);
                        result.totalSpellsDetected++;

                        // Update metrics
                        if (cast.isInterruptible)
                            _metrics.interruptibleSpells.fetch_add(1);
                        if (cast.isHostile)
                            _metrics.hostileSpells.fetch_add(1);

                        // Notify callbacks and coordinator
                        NotifySpellDetected(cast);
                    }
                }
            }
        }

        // Check channeled spell
        if (Spell* channeledSpell = unit->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
        {
            if (_config.detectChanneledSpells && ShouldDetectSpell(unit, channeledSpell))
            {
                DetectedSpellCast cast = AnalyzeSpellCast(unit, channeledSpell);
                if (cast.IsValid())
                {
                    cast.isChanneled = true;

                    // Similar new cast check as above
                    bool isNewCast = true;
                    {
                        std::shared_lock<std::shared_mutex> lock(_castMutex);
                        auto it = _activeCasts.find(unitGuid);
                        if (it != _activeCasts.end())
                        {
                            for (const auto& existingCast : it->second)
                            {
                                if (existingCast.spellId == cast.spellId && existingCast.isChanneled)
                                {
                                    isNewCast = false;
                                    break;
                                }
                            }
                        }
                    }

                    if (isNewCast)
                    {
                        AddDetectedCast(cast);
                        result.newCasts.push_back(cast);
                        result.totalSpellsDetected++;

                        NotifySpellDetected(cast);
                    }
                }
            }
        }
    }

    // Update pattern recognition if enabled
    if (_enablePatterns && !result.newCasts.empty())
    {
        for (const auto& cast : result.newCasts)
        {
            if (cast.casterGuid == unitGuid)
            {
                UpdateSpellPatterns(cast);
            }
        }
    }
}

DetectedSpellCast InterruptAwareness::AnalyzeSpellCast(Unit* caster, Spell const* spell) const
{
    DetectedSpellCast cast;

    if (!caster || !spell)
        return cast;

    const SpellInfo* spellInfo = spell->GetSpellInfo();
    if (!spellInfo)
        return cast;

    cast.casterGuid = caster->GetGUID();
    cast.spellId = spellInfo->Id;
    cast.detectionTime = std::chrono::steady_clock::now();
    cast.casterPosition = Position(caster->GetPositionX(), caster->GetPositionY(), caster->GetPositionZ());
    cast.isChanneled = spellInfo->IsChanneled();
    cast.isInterruptible = IsSpellInterruptible(spell);
    cast.isHostile = IsSpellHostile(caster, spell);
    cast.schoolMask = spellInfo->SchoolMask;

    if (_observer)
        cast.detectionRange = _observer->GetDistance(caster);

    // Calculate cast timing
    if (cast.isChanneled)
    {
        cast.castTimeMs = spellInfo->GetDuration();
    }
    else
    {
        cast.castTimeMs = spellInfo->CastTimeEntry->Base;
    }

    // Calculate remaining time based on spell progress
    uint32 elapsedTime = spell->GetTimer(); // Time since cast started
    if (elapsedTime < cast.castTimeMs)
    {
        cast.remainingTimeMs = cast.castTimeMs - elapsedTime;
        cast.estimatedCastEnd = cast.detectionTime + std::chrono::milliseconds(cast.remainingTimeMs);
    }
    else
    {
        // Spell is nearly complete or we detected it very late
        cast.remainingTimeMs = 100; // Give a small buffer
        cast.estimatedCastEnd = cast.detectionTime + std::chrono::milliseconds(100);
    }

    return cast;
}

bool InterruptAwareness::ShouldDetectSpell(Unit* caster, Spell const* spell) const
{
    if (!caster || !spell)
        return false;

    const SpellInfo* spellInfo = spell->GetSpellInfo();
    if (!spellInfo)
        return false;

    // Check cast time minimum
    if (spellInfo->CastTimeEntry->Base < _config.minCastTime && !spellInfo->IsChanneled())
        return false;

    // Check if we should detect instant casts
    if (!_config.detectInstantCasts && spellInfo->CastTimeEntry->Base == 0 && !spellInfo->IsChanneled())
        return false;

    // Check range
    if (_observer)
    {
        float distance = _observer->GetDistance(caster);
        if (distance > _config.maxDetectionRange)
            return false;

        // Check line of sight if required
        if (_config.requireLineOfSight && !_observer->IsWithinLOSInMap(caster))
            return false;
    }

    // Check friendly vs hostile
    if (!_config.detectFriendlySpells && _observer)
    {
        if (caster->IsFriendlyTo(_observer))
            return false;
    }

    return true;
}

bool InterruptAwareness::IsSpellInterruptible(Spell const* spell) const
{
    if (!spell)
        return false;

    const SpellInfo* spellInfo = spell->GetSpellInfo();
    if (!spellInfo)
        return false;

    // Check for uninterruptible attributes
    // Note: SPELL_ATTR0_UNAFFECTED_BY_INVULNERABILITY not available in this TrinityCore version

    if (spellInfo->HasAttribute(SPELL_ATTR4_CANNOT_BE_STOLEN))
        return false;

    // Most spells are interruptible by default
    return true;
}

bool InterruptAwareness::IsSpellHostile(Unit* caster, Spell const* spell) const
{
    if (!caster || !spell || !_observer)
        return false;

    // Check faction relationship
    if (caster->IsHostileTo(_observer))
        return true;

    // Check spell effects for hostile nature
    const SpellInfo* spellInfo = spell->GetSpellInfo();
    if (!spellInfo)
        return false;

    // Check for damage or harmful effects
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        switch (spellInfo->GetEffects()[i].Effect)
        {
            case SPELL_EFFECT_SCHOOL_DAMAGE:
            case SPELL_EFFECT_WEAPON_DAMAGE:
            case SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL:
            case SPELL_EFFECT_NORMALIZED_WEAPON_DMG:
            case SPELL_EFFECT_WEAPON_PERCENT_DAMAGE:
                return true;

            case SPELL_EFFECT_APPLY_AURA:
                switch (spellInfo->GetEffects()[i].ApplyAuraName)
                {
                    case SPELL_AURA_MOD_FEAR:
                    case SPELL_AURA_MOD_STUN:
                    case SPELL_AURA_MOD_CHARM:
                    case SPELL_AURA_TRANSFORM:
                    case SPELL_AURA_MOD_SILENCE:
                    case SPELL_AURA_PERIODIC_DAMAGE:
                        return true;
                    default:
                        break;
                }
                break;

            default:
                break;
        }
    }

    return false;
}

void InterruptAwareness::UpdateActiveCasts()
{
    RemoveExpiredCasts();

    // Update remaining times for active casts
    std::unique_lock<std::shared_mutex> lock(_castMutex);
    for (auto& [casterGuid, casts] : _activeCasts)
    {
        for (auto& cast : casts)
        {
            if (cast.IsValid())
            {
                cast.remainingTimeMs = cast.GetRemainingTime();
            }
        }
    }
}

void InterruptAwareness::AddDetectedCast(DetectedSpellCast const& cast)
{
    std::unique_lock<std::shared_mutex> lock(_castMutex);

    // Limit total active casts to prevent memory issues
    if (GetActiveCastCount() >= MAX_ACTIVE_CASTS)
    {
        // Remove oldest casts
        auto now = std::chrono::steady_clock::now();
        for (auto& [casterGuid, casts] : _activeCasts)
        {
            casts.erase(
                std::remove_if(casts.begin(), casts.end(),
                              [now](const DetectedSpellCast& c) {
                                  auto age = std::chrono::duration_cast<std::chrono::seconds>(now - c.detectionTime);
                                  return age.count() > 30; // Remove casts older than 30 seconds
                              }),
                casts.end()
            );
        }
    }

    _activeCasts[cast.casterGuid].push_back(cast);
}

void InterruptAwareness::RemoveExpiredCasts()
{
    std::unique_lock<std::shared_mutex> lock(_castMutex);

    for (auto it = _activeCasts.begin(); it != _activeCasts.end();)
    {
        auto& casts = it->second;

        // Remove expired casts
        casts.erase(
            std::remove_if(casts.begin(), casts.end(),
                          [](const DetectedSpellCast& cast) {
                              return cast.IsExpired();
                          }),
            casts.end()
        );

        // Remove empty entries
        if (casts.empty())
            it = _activeCasts.erase(it);
        else
            ++it;
    }
}

void InterruptAwareness::ProcessCompletedCasts(SpellScanResult& result)
{
    // This would check for spells that were interrupted or completed
    // Implementation would depend on how TrinityCore reports spell completion
    // For now, we rely on the expiration system
}

std::vector<Unit*> InterruptAwareness::GetNearbyUnits() const
{
    std::vector<Unit*> units;

    if (!_observer)
        return units;

    // Use TrinityCore's ObjectAccessor for proper grid-based unit search
    std::list<Unit*> nearbyUnits;
    Trinity::AnyUnitInObjectRangeCheck check(_observer, _config.maxDetectionRange);
    Trinity::UnitListSearcher<Trinity::AnyUnitInObjectRangeCheck> searcher(_observer, nearbyUnits, check);
    Cell::VisitAllObjects(_observer, searcher, _config.maxDetectionRange);

    // Convert to vector and filter appropriate units
    for (Unit* unit : nearbyUnits)
    {
        if (!unit || unit == _observer)
            continue;

        // Filter for casting or combat units to reduce processing load
        if (unit->HasUnitState(UNIT_STATE_CASTING) || unit->IsInCombat())
        {
            units.push_back(unit);
        }
        // Also include hostile units that might cast soon
        else if (unit->IsHostileTo(_observer) && unit->IsAlive())
        {
            units.push_back(unit);
        }
    }

    // Fallback: Add current target if grid search didn't find it
    if (Unit* target = ObjectAccessor::GetUnit(*_observer, _observer->GetTarget()))
    {
        if (_observer->GetDistance(target) <= _config.maxDetectionRange)
        {
            // Check if target is already in the list
            auto it = std::find(units.begin(), units.end(), target);
            if (it == units.end())
            {
                units.push_back(target);
            }
        }
    }

    return units;
}

bool InterruptAwareness::ShouldScanUnit(Unit* unit) const
{
    if (!unit || !unit->IsAlive())
        return false;

    // Don't scan the observer
    if (unit == _observer)
        return false;

    // Check if unit is in casting state or might cast soon
    if (!unit->HasUnitState(UNIT_STATE_CASTING) && !unit->IsInCombat())
        return false;

    return true;
}

uint32 InterruptAwareness::GetUnitScanPriority(Unit* unit) const
{
    if (!unit)
        return 0;

    uint32 priority = 100; // Base priority

    // Higher priority for creatures in combat
    if (unit->IsInCombat())
        priority += 50;

    // Higher priority for creatures that are casting
    if (unit->HasUnitState(UNIT_STATE_CASTING))
        priority += 100;

    // Simplified implementation - avoid complex Creature API calls
    // This will be properly implemented when TrinityCore integration is resolved

    return priority;
}

void InterruptAwareness::UpdateSpellPatterns(DetectedSpellCast const& cast)
{
    if (!_enablePatterns)
        return;

    // Simplified pattern recognition - avoid complex Creature API calls
    // This will be properly implemented when TrinityCore integration is resolved

    // For now, just log the detected spell for future pattern analysis
    TC_LOG_DEBUG("playerbot", "InterruptAwareness: Spell pattern detection deferred - spell %u from caster %s",
                 cast.spellId, cast.casterGuid.ToString().c_str());
}

bool InterruptAwareness::MatchesKnownPattern(ObjectGuid casterGuid, uint32 spellId) const
{
    if (!_enablePatterns)
        return false;

    // Implementation would check if this spell matches expected patterns
    // This is a placeholder for future pattern matching logic
    return false;
}

void InterruptAwareness::OptimizeForPerformance()
{
    // Clean up old data, optimize data structures
    RemoveExpiredCasts();

    // Reset metrics if they get too large
    {
        std::lock_guard<std::mutex> lock(_metricsMutex);
        if (_metrics.totalScans.load() > 100000)
        {
            // Scale down metrics to prevent overflow
            _metrics.totalScans.store(_metrics.totalScans.load() / 2);
            _metrics.unitsScanned.store(_metrics.unitsScanned.load() / 2);
            _metrics.spellsDetected.store(_metrics.spellsDetected.load() / 2);
            _metrics.interruptibleSpells.store(_metrics.interruptibleSpells.load() / 2);
            _metrics.hostileSpells.store(_metrics.hostileSpells.load() / 2);
        }
    }

    TC_LOG_DEBUG("playerbot", "InterruptAwareness: Performance optimization complete - %u active casts, %u scans performed",
                 static_cast<uint32>(GetActiveCastCount()), _scanCount);
}

bool InterruptAwareness::ShouldSkipScan() const
{
    // Skip scan if we're not in combat and have no active casts
    if (!_observer || !_observer->IsInCombat())
    {
        if (GetActiveCastCount() == 0)
            return true;
    }

    // Skip scan if performance is degraded
    if (_lastScanTimeMs > 10) // If last scan took more than 10ms
        return true;

    return false;
}

void InterruptAwareness::NotifySpellDetected(DetectedSpellCast const& cast)
{
    // Notify interrupt coordinator
    if (auto coordinator = _coordinator.lock())
    {
        // Find the spell object for the coordinator
        Unit* caster = ObjectAccessor::GetUnit(*_observer, cast.casterGuid);
        if (caster)
        {
            if (Spell* spell = caster->GetCurrentSpell(CURRENT_GENERIC_SPELL))
            {
                if (spell->GetSpellInfo()->Id == cast.spellId)
                {
                    coordinator->OnSpellCastStart(caster, spell);
                }
            }
            else if (Spell* channeled = caster->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
            {
                if (channeled->GetSpellInfo()->Id == cast.spellId)
                {
                    coordinator->OnSpellCastStart(caster, channeled);
                }
            }
        }
    }

    // Notify registered callbacks
    std::lock_guard<std::mutex> lock(_callbackMutex);
    for (const auto& callback : _spellCastCallbacks)
    {
        try
        {
            callback(cast);
        }
        catch (...)
        {
            TC_LOG_ERROR("playerbot", "InterruptAwareness: Exception in spell cast callback");
        }
    }
}

void InterruptAwareness::NotifySpellCompleted(ObjectGuid casterGuid, uint32 spellId, bool interrupted)
{
    // Notify interrupt coordinator
    if (auto coordinator = _coordinator.lock())
    {
        Unit* caster = ObjectAccessor::GetUnit(*_observer, casterGuid);
        if (caster)
        {
            coordinator->OnSpellCastFinish(caster, spellId, interrupted);
        }
    }

    // Notify registered callbacks
    std::lock_guard<std::mutex> lock(_callbackMutex);
    for (const auto& callback : _spellCompleteCallbacks)
    {
        try
        {
            callback(casterGuid, spellId, interrupted);
        }
        catch (...)
        {
            TC_LOG_ERROR("playerbot", "InterruptAwareness: Exception in spell complete callback");
        }
    }
}

} // namespace Playerbot