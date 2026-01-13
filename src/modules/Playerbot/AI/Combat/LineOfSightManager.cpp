/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "LineOfSightManager.h"
#include "Player.h"
#include "Unit.h"
#include "Map.h"
#include "Log.h"
#include "GameObject.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "G3D/Vector3.h"
#include "../../Spatial/SpatialGridManager.h"
#include "ObjectAccessor.h"
#include "PhaseShift.h"
#include "SharedDefines.h"
#include "GameTime.h"

namespace Playerbot
{

LineOfSightManager::LineOfSightManager(Player* bot)
    : _bot(bot), _maxRange(DEFAULT_MAX_RANGE), _heightTolerance(DEFAULT_HEIGHT_TOLERANCE),
      _angleTolerance(static_cast<float>(M_PI) / 3.0f), _enableCaching(true), _profilingEnabled(false),
      _lastObstructionUpdate(0)
{
    TC_LOG_DEBUG("playerbot.los", "LineOfSightManager initialized for bot {}", _bot->GetName());
}

LoSResult LineOfSightManager::CheckLineOfSight(const LoSContext& context)
{
    auto startTime = ::std::chrono::steady_clock::now();
    LoSResult result;

    // No lock needed - line of sight cache is per-bot instance data

    try
    {
        _metrics.totalChecks++;

        if (!context.source || !context.target)
        {
            result.failureReason = "Invalid source or target";
            return result;
        }

        ObjectGuid sourceGuid = context.source->GetGUID();
        ObjectGuid targetGuid = context.target->GetGUID();
        if (_enableCaching)
        {
            LoSCacheKey key(sourceGuid, targetGuid, context.checkType);
            auto cachedEntry = _losCache.Get(key);
            if (cachedEntry.has_value())
            {
                // LRUCache handles TTL automatically - entry is valid if returned
                _metrics.cacheHits++;
                auto endTime = ::std::chrono::steady_clock::now();
                auto duration = ::std::chrono::duration_cast<::std::chrono::microseconds>(endTime - startTime);
                TrackPerformance(duration, true, cachedEntry->result.hasLineOfSight);
                return cachedEntry->result;
            }
        }

        _metrics.cacheMisses++;

        result = PerformLineOfSightCheck(context);

        if (_enableCaching)
        {
            LoSCacheEntry cacheEntry;
            cacheEntry.sourceGuid = sourceGuid;
            cacheEntry.targetGuid = targetGuid;
            cacheEntry.result = result;
            cacheEntry.timestamp = GameTime::GetGameTimeMS();
            // expirationTime is set for backward compatibility but LRUCache handles TTL automatically
            cacheEntry.expirationTime = cacheEntry.timestamp + DEFAULT_CACHE_DURATION;
            cacheEntry.checkType = context.checkType;

            AddCacheEntry(cacheEntry);
        }

        if (result.hasLineOfSight)
            _metrics.successfulChecks++;
        else
            _metrics.failedChecks++;

        auto endTime = ::std::chrono::steady_clock::now();
        auto duration = ::std::chrono::duration_cast<::std::chrono::microseconds>(endTime - startTime);
        TrackPerformance(duration, false, result.hasLineOfSight);
    }
    catch (const ::std::exception& e)
    {
        result.hasLineOfSight = false;
        result.failureReason = ::std::string("Exception during LoS check: ") + e.what();
        TC_LOG_ERROR("playerbot.los", "Exception in CheckLineOfSight for bot {}: {}", _bot->GetName(), e.what());
    }

    return result;
}

LoSResult LineOfSightManager::CheckLineOfSight(Unit* target, LoSCheckType checkType)
{
    LoSContext context;
    context.bot = _bot;
    context.source = _bot;
    context.target = target;
    context.sourcePos = _bot->GetPosition();
    context.targetPos = target->GetPosition();
    context.checkType = checkType;
    context.maxRange = _maxRange;
    context.maxHeightDiff = _heightTolerance;

    switch (checkType)
    {
        case LoSCheckType::SPELL_CASTING:
            context.validationFlags = LoSValidation::SPELL_LOS;
            break;
        case LoSCheckType::RANGED_COMBAT:
            context.validationFlags = LoSValidation::COMBAT_LOS;
            break;
        case LoSCheckType::MOVEMENT:
            context.validationFlags = LoSValidation::MOVEMENT_LOS;
            break;
        default:
            context.validationFlags = LoSValidation::BASIC_LOS;
            break;
    }

    return CheckLineOfSight(context);
}

LoSResult LineOfSightManager::CheckLineOfSight(const Position& targetPos, LoSCheckType checkType)
{
    LoSContext context;
    context.bot = _bot;
    context.source = _bot;
    context.target = nullptr;
    context.sourcePos = _bot->GetPosition();
    context.targetPos = targetPos;
    context.checkType = checkType;
    context.validationFlags = LoSValidation::BASIC_LOS;
    context.maxRange = _maxRange;
    context.maxHeightDiff = _heightTolerance;

    return CheckLineOfSight(context);
}

LoSResult LineOfSightManager::CheckSpellLineOfSight(Unit* target, uint32 spellId)
{
    LoSContext context;
    context.bot = _bot;
    context.source = _bot;
    context.target = target;
    context.sourcePos = _bot->GetPosition();
    context.targetPos = target->GetPosition();
    context.checkType = LoSCheckType::SPELL_CASTING;
    context.validationFlags = LoSValidation::SPELL_LOS;
    context.spellId = spellId;

    if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE))
    {
        context.maxRange = spellInfo->GetMaxRange();
    }

    return CheckLineOfSight(context);
}

bool LineOfSightManager::CanSeeTarget(Unit* target)
{
    if (!target)
        return false;

    LoSResult result = CheckLineOfSight(target, LoSCheckType::BASIC);
    return result.hasLineOfSight;
}

bool LineOfSightManager::CanCastSpell(uint32 spellId, Unit* target)
{
    if (!target || !spellId)
        return false;

    LoSResult result = CheckSpellLineOfSight(target, spellId);
    return result.hasLineOfSight && CheckSpellSpecificRequirements(target, spellId);
}

bool LineOfSightManager::CanAttackTarget(Unit* target)
{
    if (!target)
        return false;

    LoSResult result = CheckLineOfSight(target, LoSCheckType::RANGED_COMBAT);
    return result.hasLineOfSight;
}

bool LineOfSightManager::CanHealTarget(Unit* target)
{
    if (!target)
        return false;

    LoSResult result = CheckLineOfSight(target, LoSCheckType::HEALING);
    return result.hasLineOfSight && CheckHealingLineOfSight(target);
}

bool LineOfSightManager::CanInterruptTarget(Unit* target)
{
    if (!target)
        return false;

    LoSResult result = CheckLineOfSight(target, LoSCheckType::INTERRUPT);
    return result.hasLineOfSight && CheckInterruptLineOfSight(target);
}

bool LineOfSightManager::CanMoveToPosition(const Position& pos)
{
    LoSResult result = CheckLineOfSight(pos, LoSCheckType::MOVEMENT);
    return result.hasLineOfSight;
}

::std::vector<Position> LineOfSightManager::FindLineOfSightPositions(Unit* target, float radius)
{
    ::std::vector<Position> losPositions;

    if (!target)
        return losPositions;

    Position targetPos = target->GetPosition();
    Position botPos = _bot->GetPosition();
    for (float angle = 0.0f; angle < 2.0f * static_cast<float>(M_PI); angle += static_cast<float>(M_PI) / 8.0f)
    {
        for (float distance = radius * 0.5f; distance <= radius; distance += radius * 0.25f)
        {
            Position candidatePos;
            candidatePos.m_positionX = targetPos.GetPositionX() + distance * ::std::cos(angle);
            candidatePos.m_positionY = targetPos.GetPositionY() + distance * ::std::sin(angle);
            candidatePos.m_positionZ = targetPos.GetPositionZ();

            if (HasLineOfSightFromPosition(candidatePos, target))
            {
                losPositions.push_back(candidatePos);
            }
        }
    }

    return losPositions;
}

Position LineOfSightManager::FindBestLineOfSightPosition(Unit* target, float preferredRange)
{
    if (!target)
        return _bot->GetPosition();

    ::std::vector<Position> candidates = FindLineOfSightPositions(target, preferredRange > 0.0f ? preferredRange : 20.0f);
    if (candidates.empty())
        return _bot->GetPosition();

    Position botPos = _bot->GetPosition();
    Position bestPos = candidates[0];
    float bestScore = 0.0f;

    for (const Position& pos : candidates)
    {
        float distance = pos.GetExactDist(&botPos);
        float score = 100.0f - distance;

        if (preferredRange > 0.0f)
        {
            float rangeDiff = ::std::abs(pos.GetExactDist(target) - preferredRange);
            score -= rangeDiff * 2.0f;
        }

        if (score > bestScore)
        {
            bestScore = score;
            bestPos = pos;
        }
    }

    return bestPos;
}

bool LineOfSightManager::HasLineOfSightFromPosition(const Position& fromPos, Unit* target)
{
    if (!target)
        return false;

    LoSContext context;
    context.bot = _bot;
    context.source = _bot;
    context.target = target;
    context.sourcePos = fromPos;
    context.targetPos = target->GetPosition();
    context.checkType = LoSCheckType::BASIC;
    context.validationFlags = LoSValidation::BASIC_LOS;

    LoSResult result = CheckLineOfSight(context);
    return result.hasLineOfSight;
}

::std::vector<Unit*> LineOfSightManager::GetVisibleEnemies(float maxRange)
{
    ::std::vector<Unit*> visibleEnemies;

    // DEADLOCK FIX: Use lock-free spatial grid instead of Cell::VisitGridObjects
    Map* map = _bot->GetMap();
    if (!map)
        return visibleEnemies;

    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (!spatialGrid)
    {
        // Grid not yet created for this map - create it on demand
        sSpatialGridManager.CreateGrid(map);
        spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
            return visibleEnemies;
    }

    // Query nearby creature GUIDs (lock-free!)
    ::std::vector<ObjectGuid> nearbyGuids = spatialGrid->QueryNearbyCreatureGuids(
        _bot->GetPosition(), maxRange);
    // Resolve GUIDs to Unit pointers and filter visible enemies
    for (ObjectGuid guid : nearbyGuids)
    {
        // SPATIAL GRID MIGRATION COMPLETE (2025-11-26):
        // ObjectAccessor is intentionally retained - Live Unit* needed for:
        // 1. Return type is std::vector<Unit*> - callers require live Unit*
        // 2. IsAlive() verification requires real-time state check
        // 3. IsHostileTo() faction check requires live relationship data
        // The spatial grid pre-filters candidates to reduce ObjectAccessor calls.
        ::Unit* unit = ObjectAccessor::GetUnit(*_bot, guid);
        if (!unit || !unit->IsAlive())
            continue;

        if (!_bot->IsHostileTo(unit))
            continue;

        if (CanSeeTarget(unit))
        {
            visibleEnemies.push_back(unit);
        }
    }

    return visibleEnemies;
}

::std::vector<Unit*> LineOfSightManager::GetVisibleAllies(float maxRange)
{
    ::std::vector<Unit*> visibleAllies;

    // DEADLOCK FIX: Use lock-free spatial grid instead of Cell::VisitGridObjects
    Map* map = _bot->GetMap();
    if (!map)
        return visibleAllies;

    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (!spatialGrid)
    {
        // Grid not yet created for this map - create it on demand
        sSpatialGridManager.CreateGrid(map);
        spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
            return visibleAllies;
    }
    // Query nearby creature GUIDs (lock-free!)
    ::std::vector<ObjectGuid> nearbyGuids = spatialGrid->QueryNearbyCreatureGuids(
        _bot->GetPosition(), maxRange);

    // Resolve GUIDs to Unit pointers and filter visible allies
    for (ObjectGuid guid : nearbyGuids)
    {
        // SPATIAL GRID MIGRATION COMPLETE (2025-11-26):
        // ObjectAccessor is intentionally retained - Live Unit* needed for:
        // 1. Return type is std::vector<Unit*> - callers require live Unit*
        // 2. IsAlive() verification requires real-time state check
        // 3. IsHostileTo() faction check requires live relationship data
        // The spatial grid pre-filters candidates to reduce ObjectAccessor calls.
        ::Unit* unit = ObjectAccessor::GetUnit(*_bot, guid);
        if (!unit || !unit->IsAlive())
            continue;

        if (_bot->IsHostileTo(unit))
            continue;

        if (CanSeeTarget(unit))
        {
            visibleAllies.push_back(unit);
        }
    }

    return visibleAllies;
}

Unit* LineOfSightManager::GetBestVisibleTarget(const ::std::vector<Unit*>& candidates)
{
    Unit* bestTarget = nullptr;
    float bestScore = 0.0f;

    for (Unit* candidate : candidates)
    {
        if (!candidate || !CanSeeTarget(candidate))
            continue;

        float distance = ::std::sqrt(_bot->GetExactDistSq(candidate)); // Calculate once from squared distance
        float healthPct = candidate->GetHealthPct();

        float score = 100.0f - (distance * 2.0f);
        if (healthPct < 50.0f)
            score += (50.0f - healthPct);

        if (score > bestScore)
        {
            bestScore = score;
            bestTarget = candidate;
        }
    }

    return bestTarget;
}

bool LineOfSightManager::IsHeightDifferenceBlocking(Unit* target)
{
    if (!target)
        return false;

    float heightDiff = ::std::abs(_bot->GetPositionZ() - target->GetPositionZ());
    return heightDiff > _heightTolerance;
}

float LineOfSightManager::CalculateHeightAdvantage(Unit* target)
{
    if (!target)
        return 0.0f;

    return _bot->GetPositionZ() - target->GetPositionZ();
}

bool LineOfSightManager::HasElevationAdvantage(Unit* target)
{
    return CalculateHeightAdvantage(target) > 3.0f;
}

void LineOfSightManager::ClearCache()
{
    // No lock needed - line of sight cache is per-bot instance data
    _losCache.Clear();
    TC_LOG_DEBUG("playerbot.los", "LoS cache cleared for bot {}", _bot->GetName());
}

void LineOfSightManager::ClearExpiredCacheEntries()
{
    // LRUCache handles TTL-based expiration automatically on access
    // This method provides explicit cleanup for maintenance purposes
    size_t removed = _losCache.RemoveExpired();
    if (removed > 0)
    {
        TC_LOG_DEBUG("playerbot.los", "LoS cache cleanup: {} expired entries removed for bot {}",
                    removed, _bot->GetName());
    }
}

LoSResult LineOfSightManager::PerformLineOfSightCheck(const LoSContext& context)
{
    LoSResult result;
    result.checkType = context.checkType;
    result.checkTime = GameTime::GetGameTimeMS();

    Position from = context.sourcePos;
    Position to = context.targetPos;
    result.distance = CalculateDistance3D(from, to);
    result.heightDifference = ::std::abs(to.GetPositionZ() - from.GetPositionZ());

    if (!IsWithinRange(from, to, context.maxRange))
    {
        result.blockedByRange = true;
        result.failureReason = "Target out of range";
        return result;
    }
    if (!IsHeightDifferenceAcceptable(from, to, context.maxHeightDiff))
    {
        result.blockedByHeight = true;
        result.failureReason = "Height difference too great";
        return result;
    }

    if ((context.validationFlags & LoSValidation::TERRAIN) != LoSValidation::NONE && CheckTerrainBlocking(from, to))
    {
        result.blockedByTerrain = true;
        result.failureReason = "Blocked by terrain";
        return result;
    }

    if ((context.validationFlags & LoSValidation::BUILDINGS) != LoSValidation::NONE && CheckBuildingBlocking(from, to))
    {
        result.blockedByBuilding = true;
        result.failureReason = "Blocked by building";
        return result;
    }

    if ((context.validationFlags & LoSValidation::OBJECTS) != LoSValidation::NONE && CheckObjectBlocking(from, to))
    {
        result.blockedByObject = true;
        result.failureReason = "Blocked by object";
        return result;
    }
    if ((context.validationFlags & LoSValidation::UNITS) != LoSValidation::NONE && !context.ignoreUnits && CheckUnitBlocking(from, to, context.target))
    {
        result.blockedByUnit = true;
        result.failureReason = "Blocked by unit";
        return result;
    }

    if ((context.validationFlags & LoSValidation::WATER) != LoSValidation::NONE && CheckWaterBlocking(from, to))
    {
        result.blockedByWater = true;
        result.failureReason = "Blocked by water";
        return result;
    }

    if ((context.validationFlags & LoSValidation::ANGLE_CHECK) != LoSValidation::NONE && !IsAngleAcceptable(from, to, context.viewAngleTolerance))
    {
        result.blockedByAngle = true;
        result.failureReason = "Outside viewing angle";
        return result;
    }

    if ((context.validationFlags & LoSValidation::SPELL_SPECIFIC) != LoSValidation::NONE && context.spellId > 0)
    {
        if (!CheckSpellSpecificRequirements(context.target, context.spellId))
        {
            result.failureReason = "Spell-specific requirements not met";
            return result;
        }
    }

    result.hasLineOfSight = true;
    return result;
}

bool LineOfSightManager::CheckTerrainBlocking(const Position& from, const Position& to)
{
    Map* map = _bot->GetMap();
    if (!map)
        return true;

    return !map->isInLineOfSight(_bot->GetPhaseShift(), from.GetPositionX(), from.GetPositionY(), from.GetPositionZ() + 2.0f,
                                to.GetPositionX(), to.GetPositionY(), to.GetPositionZ() + 2.0f,
                                LINEOFSIGHT_ALL_CHECKS, VMAP::ModelIgnoreFlags::Nothing);
}

bool LineOfSightManager::CheckBuildingBlocking(const Position& from, const Position& to)
{
    Map* map = _bot->GetMap();
    if (!map)
        return false;
    return !map->isInLineOfSight(_bot->GetPhaseShift(), from.GetPositionX(), from.GetPositionY(), from.GetPositionZ() + 2.0f,
                               to.GetPositionX(), to.GetPositionY(), to.GetPositionZ() + 2.0f, LINEOFSIGHT_CHECK_VMAP, VMAP::ModelIgnoreFlags::Nothing);
}

bool LineOfSightManager::CheckObjectBlocking(const Position& from, const Position& to)
{
    // DEADLOCK FIX: Use lock-free spatial grid instead of Cell::VisitGridObjects
    float searchRange = ::std::max(from.GetExactDist(&to), 30.0f);
    Map* map = _bot->GetMap();
    if (!map)
        return false;

    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (!spatialGrid)
    {
        // Grid not yet created for this map - create it on demand
        sSpatialGridManager.CreateGrid(map);
        spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
            return false;
    }

    // Query nearby GameObject GUIDs (lock-free!)
    ::std::vector<ObjectGuid> nearbyGuids = spatialGrid->QueryNearbyGameObjectGuids(
        _bot->GetPosition(), searchRange);

    // Resolve GUIDs to GameObject pointers and check for blocking
    for (ObjectGuid guid : nearbyGuids)
    {
        GameObject* obj = _bot->GetMap()->GetGameObject(guid);
        if (!obj || !obj->IsInWorld())
            continue;

        if (obj->GetGoType() == GAMEOBJECT_TYPE_DOOR && obj->GetGoState() == GO_STATE_ACTIVE)
            continue;

        float objDistance = ::std::sqrt(obj->GetExactDistSq(from)); // Calculate once from squared distance
        float totalDistance = from.GetExactDist(&to);

        if (objDistance < totalDistance && objDistance > 1.0f)
        {
            Position objPos = obj->GetPosition();
            if (LoSUtils::DoLinesIntersect(from, to, objPos, objPos))
            {
                return true;
            }
        }
    }

    return false;
}

bool LineOfSightManager::CheckUnitBlocking(const Position& from, const Position& to, Unit* ignoreUnit)
{
    // DEADLOCK FIX: Use lock-free spatial grid instead of Cell::VisitGridObjects
    float searchRange = from.GetExactDist(&to);
    Map* map = _bot->GetMap();
    if (!map)
        return false;

    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (!spatialGrid)
    {
        // Grid not yet created for this map - create it on demand
        sSpatialGridManager.CreateGrid(map);
        spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
            return false;
    }

    // Query nearby creature GUIDs (lock-free!)
    ::std::vector<ObjectGuid> nearbyGuids = spatialGrid->QueryNearbyCreatureGuids(
        _bot->GetPosition(), searchRange);

    // Resolve GUIDs to Unit pointers and check for blocking
    for (ObjectGuid guid : nearbyGuids)
    {
        // SPATIAL GRID MIGRATION COMPLETE (2025-11-26):
        // ObjectAccessor is intentionally retained - Live Unit* needed for:
        // 1. GetPosition() requires current position from live object
        // 2. IsAlive() verification requires real-time state check
        // 3. Equality comparison (unit == ignoreUnit) requires live references
        // The spatial grid pre-filters candidates to reduce ObjectAccessor calls.
        ::Unit* unit = ObjectAccessor::GetUnit(*_bot, guid);
        if (!unit || unit == _bot || unit == ignoreUnit)
            continue;

        if (!unit->IsAlive())
            continue;

        Position unitPos = unit->GetPosition();
        float distanceToLine = 0.0f;

        if (distanceToLine < 1.5f)
        {
            return true;
        }
    }

    return false;
}

bool LineOfSightManager::CheckWaterBlocking(const Position& from, const Position& to)
{
    Map* map = _bot->GetMap();
    if (!map)
        return false;

    bool fromInWater = map->IsInWater(_bot->GetPhaseShift(), from.GetPositionX(), from.GetPositionY(), from.GetPositionZ());
    bool toInWater = map->IsInWater(_bot->GetPhaseShift(), to.GetPositionX(), to.GetPositionY(), to.GetPositionZ());

    return fromInWater != toInWater;
}

void LineOfSightManager::AddCacheEntry(const LoSCacheEntry& entry)
{
    // LRUCache handles capacity management automatically - no need to check size
    // The cache will evict LRU entries when full
    LoSCacheKey key(entry.sourceGuid, entry.targetGuid, entry.checkType);
    _losCache.Put(key, entry);
}

float LineOfSightManager::CalculateDistance3D(const Position& from, const Position& to)
{
    return from.GetExactDist(&to);
}

bool LineOfSightManager::IsWithinRange(const Position& from, const Position& to, float maxRange)
{
    return from.GetExactDist(&to) <= maxRange;
}

bool LineOfSightManager::IsHeightDifferenceAcceptable(const Position& from, const Position& to, float maxDiff)
{
    return ::std::abs(to.GetPositionZ() - from.GetPositionZ()) <= maxDiff;
}

bool LineOfSightManager::CheckSpellSpecificRequirements(Unit* target, uint32 spellId)
{
    if (!target || !spellId)
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return false;

    // Check if spell ignores line of sight
    if (spellInfo->HasAttribute(SPELL_ATTR2_IGNORE_LINE_OF_SIGHT))
        return true;

    // Check if AI doesn't need to face target
    if (spellInfo->HasAttribute(SPELL_ATTR5_AI_DOESNT_FACE_TARGET))
        return true;

    float angle = _bot->GetRelativeAngle(target);
    return ::std::abs(angle) <= static_cast<float>(M_PI) / 3.0f;
}

bool LineOfSightManager::CheckInterruptLineOfSight(Unit* target)
{
    if (!target)
        return false;

    return target->HasUnitState(UNIT_STATE_CASTING) && _bot->GetExactDistSq(target) <= (30.0f * 30.0f); // 900.0f
}

bool LineOfSightManager::CheckHealingLineOfSight(Unit* target)
{
    if (!target)
        return false;

    return !_bot->IsHostileTo(target) && _bot->GetExactDistSq(target) <= (40.0f * 40.0f); // 1600.0f
}

void LineOfSightManager::TrackPerformance(::std::chrono::microseconds duration, bool cacheHit, bool successful)
{
    if (duration > _metrics.maxCheckTime)
        _metrics.maxCheckTime = duration;

    auto currentTime = ::std::chrono::steady_clock::now();
    auto timeSinceLastUpdate = ::std::chrono::duration_cast<::std::chrono::seconds>(currentTime - _metrics.lastUpdate);

    if (timeSinceLastUpdate.count() >= 1)
    {
        uint32 totalChecks = _metrics.totalChecks.load();
        if (totalChecks > 0)
        {
            _metrics.averageCheckTime = ::std::chrono::microseconds(
                static_cast<uint64_t>(_metrics.averageCheckTime.count() * 0.9 + duration.count() * 0.1)
            );
        }
        _metrics.lastUpdate = currentTime;
    }
}

// LoSUtils implementation
bool LoSUtils::HasLoS(Player* source, Unit* target)
{
    if (!source || !target)
        return false;

    return source->IsWithinLOSInMap(target);
}

bool LoSUtils::HasLoS(const Position& from, const Position& to, Map* map)
{
    if (!map)
        return false;

    // Use empty PhaseShift for static position checks
    PhaseShift emptyPhaseShift;
    return map->isInLineOfSight(emptyPhaseShift, from.GetPositionX(), from.GetPositionY(), from.GetPositionZ(),
                              to.GetPositionX(), to.GetPositionY(), to.GetPositionZ(),
                              LINEOFSIGHT_ALL_CHECKS, VMAP::ModelIgnoreFlags::Nothing);
}

float LoSUtils::GetLoSDistance(Player* source, Unit* target)
{
    if (!source || !target)
        return 0.0f;

    return ::std::sqrt(source->GetExactDistSq(target)); // Calculate actual distance
}

bool LoSUtils::CanCastSpellAtTarget(Player* caster, Unit* target, uint32 spellId)
{
    if (!caster || !target || !spellId)
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return false;

    float maxRange = spellInfo->GetMaxRange();
    if (caster->GetExactDistSq(target) > (maxRange * maxRange))
        return false;

    return HasLoS(caster, target);
}

bool LoSUtils::IsDirectPathClear(const Position& from, const Position& to, Map* map)
{
    if (!map)
        return false;

    return HasLoS(from, to, map);
}

Position LoSUtils::GetLastVisiblePoint(const Position& from, const Position& to, Map* map)
{
    if (!map)
        return from;

    Position current = from;
    Position step;

    float distance = from.GetExactDist(&to);
    uint32 steps = static_cast<uint32>(distance / 2.0f);

    step.m_positionX = (to.GetPositionX() - from.GetPositionX()) / steps;
    step.m_positionY = (to.GetPositionY() - from.GetPositionY()) / steps;
    step.m_positionZ = (to.GetPositionZ() - from.GetPositionZ()) / steps;

    for (uint32 i = 0; i < steps; ++i)
    {
        Position next = current;
        next.m_positionX += step.m_positionX;
        next.m_positionY += step.m_positionY;
        next.m_positionZ += step.m_positionZ;

        if (!HasLoS(from, next, map))
            break;

        current = next;
    }

    return current;
}

} // namespace Playerbot