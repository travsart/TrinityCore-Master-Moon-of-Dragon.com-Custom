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

namespace Playerbot
{

LineOfSightManager::LineOfSightManager(Player* bot)
    : _bot(bot), _cacheDuration(DEFAULT_CACHE_DURATION), _lastCacheCleanup(0),
      _maxRange(DEFAULT_MAX_RANGE), _heightTolerance(DEFAULT_HEIGHT_TOLERANCE),
      _angleTolerance(M_PI/3), _enableCaching(true), _profilingEnabled(false),
      _lastObstructionUpdate(0)
{
    if (!_bot)
    {
        TC_LOG_ERROR("playerbot", "LineOfSightManager: Bot player is null!");
        return;
    }

    TC_LOG_DEBUG("playerbot.los", "LineOfSightManager initialized for bot {}", _bot->GetName());
}

LoSResult LineOfSightManager::CheckLineOfSight(const LoSContext& context)
{
    auto startTime = std::chrono::steady_clock::now();
    LoSResult result;

    std::shared_lock<std::shared_mutex> lock(_mutex);

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
            LoSCacheEntry* cacheEntry = FindCacheEntry(sourceGuid, targetGuid, context.checkType);
            if (cacheEntry && cacheEntry->IsValid(getMSTime()))
            {
                _metrics.cacheHits++;
                auto endTime = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
                TrackPerformance(duration, true, cacheEntry->result.hasLineOfSight);
                return cacheEntry->result;
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
            cacheEntry.timestamp = getMSTime();
            cacheEntry.expirationTime = cacheEntry.timestamp + _cacheDuration;
            cacheEntry.checkType = context.checkType;

            AddCacheEntry(cacheEntry);
        }

        if (result.hasLineOfSight)
            _metrics.successfulChecks++;
        else
            _metrics.failedChecks++;

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        TrackPerformance(duration, false, result.hasLineOfSight);
    }
    catch (const std::exception& e)
    {
        result.hasLineOfSight = false;
        result.failureReason = "Exception during LoS check: " + std::string(e.what());
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

    if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId))
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

bool LineOfSightManager::CanCastSpell(Unit* target, uint32 spellId)
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

std::vector<Position> LineOfSightManager::FindLineOfSightPositions(Unit* target, float radius)
{
    std::vector<Position> losPositions;

    if (!target)
        return losPositions;

    Position targetPos = target->GetPosition();
    Position botPos = _bot->GetPosition();

    for (float angle = 0.0f; angle < 2.0f * M_PI; angle += M_PI / 8.0f)
    {
        for (float distance = radius * 0.5f; distance <= radius; distance += radius * 0.25f)
        {
            Position candidatePos;
            candidatePos.m_positionX = targetPos.GetPositionX() + distance * std::cos(angle);
            candidatePos.m_positionY = targetPos.GetPositionY() + distance * std::sin(angle);
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

    std::vector<Position> candidates = FindLineOfSightPositions(target, preferredRange > 0.0f ? preferredRange : 20.0f);

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
            float rangeDiff = std::abs(pos.GetExactDist(target) - preferredRange);
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

std::vector<Unit*> LineOfSightManager::GetVisibleEnemies(float maxRange)
{
    std::vector<Unit*> visibleEnemies;

    std::list<Unit*> nearbyUnits;
    Trinity::AnyUnitInObjectRangeCheck check(_bot, maxRange);
    Trinity::UnitSearcher<Trinity::AnyUnitInObjectRangeCheck> searcher(_bot, nearbyUnits, check);
    Cell::VisitAllObjects(_bot, searcher, maxRange);

    for (Unit* unit : nearbyUnits)
    {
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

std::vector<Unit*> LineOfSightManager::GetVisibleAllies(float maxRange)
{
    std::vector<Unit*> visibleAllies;

    std::list<Unit*> nearbyUnits;
    Trinity::AnyUnitInObjectRangeCheck check(_bot, maxRange);
    Trinity::UnitSearcher<Trinity::AnyUnitInObjectRangeCheck> searcher(_bot, nearbyUnits, check);
    Cell::VisitAllObjects(_bot, searcher, maxRange);

    for (Unit* unit : nearbyUnits)
    {
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

Unit* LineOfSightManager::GetBestVisibleTarget(const std::vector<Unit*>& candidates)
{
    Unit* bestTarget = nullptr;
    float bestScore = 0.0f;

    for (Unit* candidate : candidates)
    {
        if (!candidate || !CanSeeTarget(candidate))
            continue;

        float distance = _bot->GetDistance(candidate);
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

    float heightDiff = std::abs(_bot->GetPositionZ() - target->GetPositionZ());
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
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _losCache.clear();
    TC_LOG_DEBUG("playerbot.los", "LoS cache cleared for bot {}", _bot->GetName());
}

void LineOfSightManager::ClearExpiredCacheEntries()
{
    std::unique_lock<std::shared_mutex> lock(_mutex);

    uint32 currentTime = getMSTime();
    if (currentTime - _lastCacheCleanup < CACHE_CLEANUP_INTERVAL)
        return;

    auto it = _losCache.begin();
    while (it != _losCache.end())
    {
        if (it->second.IsExpired(currentTime))
            it = _losCache.erase(it);
        else
            ++it;
    }

    _lastCacheCleanup = currentTime;
}

LoSResult LineOfSightManager::PerformLineOfSightCheck(const LoSContext& context)
{
    LoSResult result;
    result.checkType = context.checkType;
    result.checkTime = getMSTime();

    Position from = context.sourcePos;
    Position to = context.targetPos;

    result.distance = CalculateDistance3D(from, to);
    result.heightDifference = std::abs(to.GetPositionZ() - from.GetPositionZ());

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

    if ((context.validationFlags & LoSValidation::TERRAIN) && CheckTerrainBlocking(from, to))
    {
        result.blockedByTerrain = true;
        result.failureReason = "Blocked by terrain";
        return result;
    }

    if ((context.validationFlags & LoSValidation::BUILDINGS) && CheckBuildingBlocking(from, to))
    {
        result.blockedByBuilding = true;
        result.failureReason = "Blocked by building";
        return result;
    }

    if ((context.validationFlags & LoSValidation::OBJECTS) && CheckObjectBlocking(from, to))
    {
        result.blockedByObject = true;
        result.failureReason = "Blocked by object";
        return result;
    }

    if ((context.validationFlags & LoSValidation::UNITS) && !context.ignoreUnits && CheckUnitBlocking(from, to, context.target))
    {
        result.blockedByUnit = true;
        result.failureReason = "Blocked by unit";
        return result;
    }

    if ((context.validationFlags & LoSValidation::WATER) && CheckWaterBlocking(from, to))
    {
        result.blockedByWater = true;
        result.failureReason = "Blocked by water";
        return result;
    }

    if ((context.validationFlags & LoSValidation::ANGLE_CHECK) && !IsAngleAcceptable(from, to, context.viewAngleTolerance))
    {
        result.blockedByAngle = true;
        result.failureReason = "Outside viewing angle";
        return result;
    }

    if ((context.validationFlags & LoSValidation::SPELL_SPECIFIC) && context.spellId > 0)
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

    return !map->isInLineOfSight(from.GetPositionX(), from.GetPositionY(), from.GetPositionZ() + 2.0f,
                                to.GetPositionX(), to.GetPositionY(), to.GetPositionZ() + 2.0f);
}

bool LineOfSightManager::CheckBuildingBlocking(const Position& from, const Position& to)
{
    Map* map = _bot->GetMap();
    if (!map)
        return false;

    return !map->IsInLineOfSight(from.GetPositionX(), from.GetPositionY(), from.GetPositionZ() + 2.0f,
                               to.GetPositionX(), to.GetPositionY(), to.GetPositionZ() + 2.0f, LINEOFSIGHT_CHECK_VMAP);
}

bool LineOfSightManager::CheckObjectBlocking(const Position& from, const Position& to)
{
    std::list<GameObject*> nearbyObjects;
    Trinity::AllGameObjectsInRange check(_bot, std::max(from.GetExactDist(&to), 30.0f));
    Trinity::GameObjectSearcher<Trinity::AllGameObjectsInRange> searcher(_bot, nearbyObjects, check);
    Cell::VisitAllObjects(_bot, searcher, std::max(from.GetExactDist(&to), 30.0f));

    for (GameObject* obj : nearbyObjects)
    {
        if (!obj || !obj->IsInWorld())
            continue;

        if (obj->GetGoType() == GAMEOBJECT_TYPE_DOOR && obj->GetGoState() == GO_STATE_ACTIVE)
            continue;

        float objDistance = obj->GetDistance(from);
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
    std::list<Unit*> nearbyUnits;
    Trinity::AnyUnitInObjectRangeCheck check(_bot, from.GetExactDist(&to));
    Trinity::UnitSearcher<Trinity::AnyUnitInObjectRangeCheck> searcher(_bot, nearbyUnits, check);
    Cell::VisitAllObjects(_bot, searcher, from.GetExactDist(&to));

    for (Unit* unit : nearbyUnits)
    {
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

    bool fromInWater = map->IsInWater(from.GetPositionX(), from.GetPositionY(), from.GetPositionZ());
    bool toInWater = map->IsInWater(to.GetPositionX(), to.GetPositionY(), to.GetPositionZ());

    return fromInWater != toInWater;
}

LoSCacheEntry* LineOfSightManager::FindCacheEntry(ObjectGuid sourceGuid, ObjectGuid targetGuid, LoSCheckType checkType)
{
    std::string key = GenerateCacheKey(sourceGuid, targetGuid, checkType);
    auto it = _losCache.find(key);
    return (it != _losCache.end()) ? &it->second : nullptr;
}

void LineOfSightManager::AddCacheEntry(const LoSCacheEntry& entry)
{
    if (_losCache.size() >= MAX_CACHE_SIZE)
    {
        ClearExpiredCacheEntries();
    }

    std::string key = GenerateCacheKey(entry.sourceGuid, entry.targetGuid, entry.checkType);
    _losCache[key] = entry;
}

std::string LineOfSightManager::GenerateCacheKey(ObjectGuid sourceGuid, ObjectGuid targetGuid, LoSCheckType checkType)
{
    return std::to_string(sourceGuid.GetRawValue()) + "_" +
           std::to_string(targetGuid.GetRawValue()) + "_" +
           std::to_string(static_cast<uint8>(checkType));
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
    return std::abs(to.GetPositionZ() - from.GetPositionZ()) <= maxDiff;
}

bool LineOfSightManager::CheckSpellSpecificRequirements(Unit* target, uint32 spellId)
{
    if (!target || !spellId)
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return false;

    if (spellInfo->HasAttribute(SPELL_ATTR2_NOT_NEED_FACING))
        return true;

    if (spellInfo->HasAttribute(SPELL_ATTR5_DONT_TURN_DURING_CAST))
        return true;

    float angle = _bot->GetRelativeAngle(target);
    return std::abs(angle) <= M_PI/3;
}

bool LineOfSightManager::CheckInterruptLineOfSight(Unit* target)
{
    if (!target)
        return false;

    return target->HasUnitState(UNIT_STATE_CASTING) && _bot->GetDistance(target) <= 30.0f;
}

bool LineOfSightManager::CheckHealingLineOfSight(Unit* target)
{
    if (!target)
        return false;

    return !_bot->IsHostileTo(target) && _bot->GetDistance(target) <= 40.0f;
}

void LineOfSightManager::TrackPerformance(std::chrono::microseconds duration, bool cacheHit, bool successful)
{
    if (duration > _metrics.maxCheckTime)
        _metrics.maxCheckTime = duration;

    auto currentTime = std::chrono::steady_clock::now();
    auto timeSinceLastUpdate = std::chrono::duration_cast<std::chrono::seconds>(currentTime - _metrics.lastUpdate);

    if (timeSinceLastUpdate.count() >= 1)
    {
        uint32 totalChecks = _metrics.totalChecks.load();
        if (totalChecks > 0)
        {
            _metrics.averageCheckTime = std::chrono::microseconds(
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

    return map->isInLineOfSight(from.GetPositionX(), from.GetPositionY(), from.GetPositionZ(),
                              to.GetPositionX(), to.GetPositionY(), to.GetPositionZ());
}

float LoSUtils::GetLoSDistance(Player* source, Unit* target)
{
    if (!source || !target)
        return 0.0f;

    return source->GetDistance(target);
}

bool LoSUtils::CanCastSpellAtTarget(Player* caster, Unit* target, uint32 spellId)
{
    if (!caster || !target || !spellId)
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return false;

    if (caster->GetDistance(target) > spellInfo->GetMaxRange())
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