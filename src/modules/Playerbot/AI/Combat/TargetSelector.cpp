/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "TargetSelector.h"
#include "Player.h"
#include "Unit.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Log.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Group.h"
#include "SharedDefines.h"
#include "Pet.h"
#include "Creature.h"
#include "ObjectAccessor.h"
#include "SpellHistory.h"
#include "../../Spatial/SpatialGridManager.h"
#include "../../Spatial/SpatialGridQueryHelpers.h" // Thread-safe spatial grid queries

namespace Playerbot
{

TargetSelector::TargetSelector(Player* bot, BotThreatManager* threatManager)
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return nullptr;
    }
    : _bot(bot), _threatManager(threatManager), _groupTarget(nullptr), _emergencyMode(false),
      _maxTargetsToEvaluate(DEFAULT_MAX_TARGETS), _selectionCacheDuration(CACHE_DURATION_MS),
      _defaultMaxRange(DEFAULT_MAX_RANGE), _cacheTimestamp(0), _cacheDirty(true)
{
    if (!_bot)
    {
        TC_LOG_ERROR("playerbot", "TargetSelector: Bot player is null!");
        return;
    }

    if (!_threatManager)
    {
        TC_LOG_ERROR("playerbot", "TargetSelector: ThreatManager is null for bot {}", _bot->GetName());
        return;
    }

    TC_LOG_DEBUG("playerbot.target", "TargetSelector initialized for bot {}", _bot->GetName());
}

SelectionResult TargetSelector::SelectBestTarget(const SelectionContext& context)
{
    auto startTime = std::chrono::steady_clock::now();
    SelectionResult result;

    std::lock_guard<std::recursive_mutex> lock(_mutex);

    try
    {
        result.candidatesEvaluated = 0;

        if (!context.bot)
        {
            result.failureReason = "Invalid bot in context";
            UpdateMetrics(result);
            return result;
        }

        std::vector<Unit*> candidates = GetAllTargetCandidates(context);
        if (candidates.empty())
        {
            result.failureReason = "No valid target candidates found";
            UpdateMetrics(result);
            return result;
        }

        std::vector<TargetInfo> evaluatedTargets;
        evaluatedTargets.reserve(std::min(candidates.size(), static_cast<size_t>(_maxTargetsToEvaluate)));

        for (Unit* candidate : candidates)
        {
            if (result.candidatesEvaluated >= _maxTargetsToEvaluate)
                break;

            if (!IsValidTarget(candidate, context.validationFlags))
                continue;

            TargetInfo targetInfo;
            targetInfo.guid = candidate->GetGUID();
            targetInfo.unit = candidate;
            targetInfo.distance = std::sqrt(_bot->GetExactDistSq(candidate)); // Calculate once from squared distance
            targetInfo.healthPercent = candidate->GetHealthPct();
            targetInfo.threatLevel = _threatManager ? _threatManager->GetThreat(candidate) : 0.0f;
            targetInfo.isInterruptTarget = IsInterruptible(candidate);
            targetInfo.isGroupFocus = (candidate == context.groupTarget);
            targetInfo.isVulnerable = IsVulnerable(candidate);
            targetInfo.lastUpdate = getMSTime();

            targetInfo.priority = DetermineTargetPriority(candidate, context);
            targetInfo.score = CalculateTargetScore(candidate, context);

            evaluatedTargets.push_back(targetInfo);
            result.candidatesEvaluated++;
        }

        if (evaluatedTargets.empty())
        {
            result.failureReason = "No valid targets after evaluation";
            UpdateMetrics(result);
            return result;
        }

        std::sort(evaluatedTargets.begin(), evaluatedTargets.end(), std::greater<TargetInfo>());

        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return;
        }
        result.target = evaluatedTargets[0].unit;
        result.info = evaluatedTargets[0];
        result.success = true;

        for (size_t i = 1; i < std::min(evaluatedTargets.size(), size_t(5)); ++i)
        {
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return;
            }
            result.alternativeTargets.push_back(evaluatedTargets[i]);
        }

        TC_LOG_DEBUG("playerbot.target", "Selected target {} for bot {} with score {:.2f} (priority {})",
                   if (!bot)
                   {
                       TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                       return nullptr;
                   }
                   result.target->GetName(), _bot->GetName(), result.info.score, uint32(result.info.priority));
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.failureReason = std::string("Exception during target selection: ") + e.what();
        if (!bot)
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsInCombat");
            return nullptr;
        }
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return;
        }
        TC_LOG_ERROR("playerbot.target", "Exception in SelectBestTarget for bot {}: {}", _bot->GetName(), e.what());
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
        return;
    }
    }

    auto endTime = std::chrono::steady_clock::now();
    result.selectionTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    UpdateMetrics(result);
    return result;
}

SelectionResult TargetSelector::SelectAttackTarget(float maxRange)
{
    SelectionContext context;
    context.bot = _bot;
    context.botRole = _threatManager ? _threatManager->GetBotRole() : ThreatRole::DPS;
    context.maxRange = maxRange > 0.0f ? maxRange : _defaultMaxRange;
    context.inCombat = _bot->IsInCombat();
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsInCombat");
        return;
    }
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsInCombat");
        return;
    }
    context.emergencyMode = _emergencyMode;
    context.validationFlags = TargetValidation::COMBAT;
    context.selectionReason = "Attack target selection";

    if (Group* group = _bot->GetGroup())
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
        return nullptr;
    }
    {
        for (GroupReference const& ref : group->GetMembers())
        {
            if (Player* member = ref.GetSource())
                context.groupMembers.push_back(member);
        }
    }

    context.groupTarget = _groupTarget;
    context.currentTarget = _bot->GetVictim();
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsInCombat");
        return nullptr;
    }

    return SelectBestTarget(context);
}

SelectionResult TargetSelector::SelectSpellTarget(uint32 spellId, float maxRange)
{
    SelectionContext context;
    context.bot = _bot;
    context.botRole = _threatManager ? _threatManager->GetBotRole() : ThreatRole::DPS;
    context.spellId = spellId;
    context.inCombat = _bot->IsInCombat();
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsInCombat");
        return;
    }
    context.emergencyMode = _emergencyMode;
    context.validationFlags = TargetValidation::SPELL_TARGET;
    context.selectionReason = "Spell target selection";

    if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE))
    {
        context.maxRange = maxRange > 0.0f ? maxRange : spellInfo->GetMaxRange();
    }
    else
    {
        context.maxRange = maxRange > 0.0f ? maxRange : _defaultMaxRange;
    }

    context.weights.interruptWeight = 2.0f;
    context.weights.vulnerabilityWeight = 1.0f;

    return SelectBestTarget(context);
}

SelectionResult TargetSelector::SelectHealTarget(bool emergencyOnly)
{
    SelectionContext context;
    context.bot = _bot;
    context.botRole = ThreatRole::HEALER;
    context.maxRange = 40.0f;
    context.inCombat = _bot->IsInCombat();
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsInCombat");
        return;
    }
    context.emergencyMode = emergencyOnly;
    context.validationFlags = static_cast<TargetValidation>(static_cast<uint32>(TargetValidation::ALIVE) |
                                                           static_cast<uint32>(TargetValidation::IN_RANGE));
    context.selectionReason = "Heal target selection";

    context.weights.healthWeight = 3.0f;
    context.weights.distanceWeight = 1.0f;
    context.weights.roleWeight = 2.0f;
    context.weights.tankPriority = 2.5f;
    context.weights.healerPriority = 2.0f;

    std::vector<Unit*> candidates = GetNearbyAllies(context.maxRange);
    Unit* bestTarget = nullptr;
    float bestScore = 0.0f;

    for (Unit* ally : candidates)
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsInCombat");
        return nullptr;
    }
    {
        if (!ally || ally->GetHealthPct() >= 95.0f)
            continue;

        if (emergencyOnly && ally->GetHealthPct() > 30.0f)
            continue;

        if (!IsValidTarget(ally, context.validationFlags))
            continue;

        float score = CalculateTargetScore(ally, context);
        if (score > bestScore)
        {
            bestScore = score;
            bestTarget = ally;
        }
    }

    SelectionResult result;
    if (bestTarget)
    {
        result.target = bestTarget;
        result.success = true;
        result.info.unit = bestTarget;
        result.info.score = bestScore;
        result.info.priority = emergencyOnly ? TargetPriority::EMERGENCY : TargetPriority::CRITICAL;
    }
    else
    {
        result.failureReason = emergencyOnly ? "No emergency heal targets found" : "No heal targets found";
    }

    return result;
}

SelectionResult TargetSelector::SelectInterruptTarget(float maxRange)
{
    SelectionContext context;
    context.bot = _bot;
    context.maxRange = maxRange > 0.0f ? maxRange : 30.0f;
    context.inCombat = _bot->IsInCombat();
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsInCombat");
    return;
}
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsInCombat");
        return;
    }
    context.validationFlags = TargetValidation::COMBAT;
    context.selectionReason = "Interrupt target selection";

    context.weights.interruptWeight = 5.0f;
    context.weights.threatWeight = 2.0f;
    context.weights.distanceWeight = 1.5f;

    std::vector<Unit*> candidates = GetNearbyEnemies(context.maxRange);
    Unit* bestTarget = nullptr;
    float bestScore = 0.0f;

    for (Unit* enemy : candidates)
    {
        if (!enemy || !IsInterruptible(enemy))
            continue;

        if (!IsValidTarget(enemy, context.validationFlags))
            continue;

        float score = CalculateInterruptScore(enemy, context);
        if (score > bestScore)
        {
            bestScore = score;
            bestTarget = enemy;
        if (!target)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: target in method IsAlive");
            return;
        }
        }
    }

    SelectionResult result;
    if (bestTarget)
    {
        result.target = bestTarget;
        result.success = true;
        result.info.unit = bestTarget;
        result.info.score = bestScore;
        result.info.priority = TargetPriority::INTERRUPT;
        result.info.isInterruptTarget = true;
    }
    else
    {
        result.failureReason = "No interruptible targets found";
        if (!target)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: target in method ToCreature");
            return nullptr;
        }
    if (!target)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: target in method IsInCombat");
        return;
    }
    }

    return result;
}

SelectionResult TargetSelector::SelectTankTarget()
{
    SelectionContext context;
    context.bot = _bot;
    context.botRole = ThreatRole::TANK;
    context.maxRange = 10.0f;
    context.inCombat = _bot->IsInCombat();
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsInCombat");
        return;
    }
    context.validationFlags = static_cast<TargetValidation>(static_cast<uint32>(TargetValidation::COMBAT) |
                                                           static_cast<uint32>(TargetValidation::MELEE_RANGE_CHECK));
    context.selectionReason = "Tank target selection";

    context.weights.threatWeight = 3.0f;
    context.weights.distanceWeight = 2.0f;
    context.weights.healthWeight = 1.0f;
    context.weights.roleWeight = 1.5f;

    return SelectBestTarget(context);
}

bool TargetSelector::IsValidTarget(Unit* target, TargetValidation validation) const
    if (!target)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: target in method IsAlive");
        return nullptr;
    }
{
    if (!target)
        return false;

    if (HasFlag(validation & TargetValidation::ALIVE) && !target->IsAlive())
        return false;

    if (HasFlag(validation & TargetValidation::HOSTILE) && !_bot->IsHostileTo(target))
        return false;

    if (HasFlag(validation & TargetValidation::NOT_FRIENDLY) && _bot->IsFriendlyTo(target))
        return false;

    if (HasFlag(validation & TargetValidation::IN_RANGE) && _bot->GetExactDistSq(target) > (_defaultMaxRange * _defaultMaxRange))
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method HasSpell");
        return nullptr;
    }
        if (!target)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: target in method ToCreature");
            return nullptr;
        }
    if (!target)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: target in method IsInCombat");
        return nullptr;
    }
        return false;

    if (HasFlag(validation & TargetValidation::LINE_OF_SIGHT) && !_bot->IsWithinLOSInMap(target))
        return false;

    if (HasFlag(validation & TargetValidation::NOT_IMMUNE) && target->IsImmunedToDamage(SPELL_SCHOOL_MASK_NORMAL))
        return false;

    if (HasFlag(validation & TargetValidation::NOT_EVADING) && target->ToCreature() && target->ToCreature()->IsInEvadeMode())
        return false;

    if (HasFlag(validation & TargetValidation::NOT_CONFUSED) && target->HasUnitState(UNIT_STATE_CONFUSED))
        return false;

    if (HasFlag(validation & TargetValidation::IN_COMBAT) && !target->IsInCombat())
        return false;

    if (HasFlag(validation & TargetValidation::NOT_CROWD_CONTROLLED))
    {
        if (target->HasUnitState(UNIT_STATE_STUNNED | UNIT_STATE_ROOT | UNIT_STATE_CONFUSED | UNIT_STATE_FLEEING))
            return false;
    }

    if (HasFlag(validation & TargetValidation::THREAT_REQUIRED) && _threatManager)
    {
        if (!_threatManager->HasThreat(target))
            return false;
    }

    return true;
}

bool TargetSelector::IsInRange(Unit* target, float range) const
{
    if (!target)
        return false;

    return _bot->GetExactDistSq(target) <= (range * range);
}

bool TargetSelector::HasLineOfSight(Unit* target) const
{
    if (!target)
        return false;

    return _bot->IsWithinLOSInMap(target);
}

bool TargetSelector::CanAttack(Unit* target) const
{
    if (!target)
        return false;

    return _bot->IsValidAttackTarget(target) && !target->HasUnitFlag(UNIT_FLAG_IMMUNE_TO_PC);
}

bool TargetSelector::CanCast(Unit* target, uint32 spellId) const
{
    if (!target || !spellId)
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return false;

    float maxRange = spellInfo->GetMaxRange();
    if (_bot->GetExactDistSq(target) > (maxRange * maxRange))
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method HasSpell");
        return nullptr;
    }
        return false;

    if (!_bot->IsWithinLOSInMap(target))
        return false;

    // Check basic casting requirements
    if (!_bot->HasSpell(spellId))
        if (!unit)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: unit in method IsAlive");
            return nullptr;
        }
        return false;

    if (!_bot->IsWithinLOSInMap(target))
        return false;

    return _bot->GetSpellHistory()->IsReady(spellInfo);
}

float TargetSelector::CalculateTargetScore(Unit* target, const SelectionContext& context)
{
    if (!target)
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
            return nullptr;
        }
        return 0.0f;

    float totalScore = 0.0f;

    totalScore += CalculateThreatScore(target, context) * context.weights.threatWeight;
    totalScore += CalculateHealthScore(target, context) * context.weights.healthWeight;
    totalScore += CalculateDistanceScore(target, context) * context.weights.distanceWeight;
    totalScore += CalculateRoleScore(target, context) * context.weights.roleWeight;
    totalScore += CalculateVulnerabilityScore(target, context) * context.weights.vulnerabilityWeight;
    totalScore += CalculateInterruptScore(target, context) * context.weights.interruptWeight;
    totalScore += CalculateGroupFocusScore(target, context) * context.weights.groupFocusWeight;

    return std::max(0.0f, totalScore);
}
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetMapId");
    return;
}

TargetPriority TargetSelector::DetermineTargetPriority(Unit* target, const SelectionContext& context)
{
    if (!target)
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetPosition");
            return nullptr;
        }
        return TargetPriority::IGNORE;

    if (context.emergencyMode && target->GetHealthPct() < 25.0f)
        return TargetPriority::EMERGENCY;

    if (IsInterruptible(target))
        return TargetPriority::INTERRUPT;

    if (IsHealer(target))
        return TargetPriority::CRITICAL;

    if (IsCaster(target))
        return TargetPriority::CRITICAL;

    if (target == context.groupTarget)
        return TargetPriority::PRIMARY;

    if (target->GetHealthPct() < 30.0f)
        return TargetPriority::PRIMARY;

    if (_bot->GetExactDistSq(target) <= (8.0f * 8.0f)) // 64.0f
        return TargetPriority::SECONDARY;

    return TargetPriority::SECONDARY;
}

std::vector<Unit*> TargetSelector::GetNearbyEnemies(float range) const
{
    std::vector<Unit*> enemies;

    // PHASE 5B: Thread-safe spatial grid query (replaces QueryNearbyCreatureGuids + ObjectAccessor)
    auto hostileSnapshots = SpatialGridQueryHelpers::FindHostileCreaturesInRange(_bot, range, true);

    // Convert snapshots to Unit* for return (needed by callers)
    for (auto const* snapshot : hostileSnapshots)
    {
        if (!snapshot)
            continue;

        // Get Unit* for callers that need it
        /* MIGRATION TODO: Convert to BotActionQueue or spatial grid */ Unit* unit = ObjectAccessor::GetUnit(*_bot, snapshot->guid);
        if (!unit)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: unit in method IsAlive");
            return;
        }
        if (unit && unit->IsAlive())
            enemies.push_back(unit);
    }

    return enemies;
}

std::vector<Unit*> TargetSelector::GetNearbyAllies(float range) const
{
    std::vector<Unit*> allies;

    if (Group* group = _bot->GetGroup())
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
        return nullptr;
    }
    {
        float rangeSq = range * range;
        for (GroupReference const& ref : group->GetMembers())
        {
            if (Player* member = ref.GetSource())
            {
                if (member != _bot && _bot->GetExactDistSq(member) <= rangeSq)
                    allies.push_back(member);
            }
        }
    }

    // PHASE 5B: Thread-safe spatial grid query for pets (replaces QueryNearbyCreatureGuids + ObjectAccessor)
    auto spatialGrid = sSpatialGridManager.GetGrid(_bot->GetMapId());
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetMapId");
        return nullptr;
    }
    if (!spatialGrid)
        return allies;

    auto creatureSnapshots = spatialGrid->QueryNearbyCreatures(_bot->GetPosition(), range);
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetPosition");
        return;
    }

    // Filter for friendly pets
    for (auto const& snapshot : creatureSnapshots)
    {
        if (!snapshot.IsAlive() || snapshot.isHostile)
            continue;

        // Get Unit* to check if pet and owner (need actual object)
        /* MIGRATION TODO: Convert to BotActionQueue or spatial grid */ Unit* unit = ObjectAccessor::GetUnit(*_bot, snapshot.guid);
        if (!unit)
            continue;

        if (Pet* pet = unit->ToPet())
        {
            if (Player* owner = pet->GetOwner())
            {
                if (_bot->IsFriendlyTo(owner))
                    allies.push_back(unit);
            }
        }
    }

    return allies;
}

std::vector<Unit*> TargetSelector::GetAllTargetCandidates(const SelectionContext& context) const
{
    if (context.botRole == ThreatRole::HEALER)
        return GetNearbyAllies(context.maxRange);
    else
        return GetNearbyEnemies(context.maxRange);
}

float TargetSelector::CalculateThreatScore(Unit* target, const SelectionContext& context)
{
    if (!_threatManager || !target)
        return 0.0f;
if (!currentSpell)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: currentSpell in method GetSpellInfo");
    return nullptr;
}

    float threat = _threatManager->GetThreat(target);
    float maxThreat = 100.0f;

    return (threat / maxThreat) * 100.0f;
}

float TargetSelector::CalculateHealthScore(Unit* target, const SelectionContext& context)
{
    if (!target)
        return 0.0f;

    float healthPct = target->GetHealthPct();

    if (context.botRole == ThreatRole::HEALER)
    {
        return 100.0f - healthPct;
    }
    else
    {
        if (healthPct < 30.0f)
            return 150.0f - healthPct;
        return 50.0f;
    }
}

float TargetSelector::CalculateDistanceScore(Unit* target, const SelectionContext& context)
{
    if (!target)
        return 0.0f;

    float distance = std::sqrt(_bot->GetExactDistSq(target)); // Calculate once from squared distance
    if (!target)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: target in method ToPlayer");
        return nullptr;
    }
    float maxRange = context.maxRange;

    if (distance > maxRange)
        return 0.0f;

    return ((maxRange - distance) / maxRange) * 50.0f;
}

float TargetSelector::CalculateRoleScore(Unit* target, const SelectionContext& context)
{
    if (!target)
        return 0.0f;

    float score = 50.0f;

    if (IsHealer(target))
        score *= context.weights.healerPriority;
    else if (IsCaster(target))
        score *= context.weights.casterPriority;
    else if (IsTank(target))
        score *= context.weights.tankPriority;
    else
        score *= context.weights.dpsPriority;

    return score;
}

float TargetSelector::CalculateVulnerabilityScore(Unit* target, const SelectionContext& context)
    if (!target)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: target in method ToCreature");
        return nullptr;
    }
        if (!target)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: target in method ToPlayer");
            return nullptr;
        }
{
    if (!target)
        return 0.0f;

    if (!creature)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: creature in method GetCreatureTemplate");
        return nullptr;
    }
    float score = 0.0f;

    if (IsVulnerable(target))
        score += 30.0f;

    if (target->HasUnitState(UNIT_STATE_STUNNED))
        score += 20.0f;

    if (target->HasUnitState(UNIT_STATE_ROOT))
        score += 15.0f;

    if (!player)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetClass");
        return nullptr;
    }
    if (!target->CanFreeMove())
        score += 10.0f;

    return score;
}

float TargetSelector::CalculateInterruptScore(Unit* target, const SelectionContext& context)
{
    if (!target || !IsInterruptible(target))
        return 0.0f;

    float score = 100.0f;

    if (Spell* currentSpell = target->GetCurrentSpell(CURRENT_GENERIC_SPELL))
    if (!target)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: target in method ToPlayer");
        return nullptr;
    }
    {
        if (SpellInfo const* spellInfo = currentSpell->GetSpellInfo())
        if (!currentSpell)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: currentSpell in method GetSpellInfo");
            return nullptr;
        }
        {
            if (spellInfo->HasEffect(SPELL_EFFECT_HEAL))
                score += 50.0f;
            if (spellInfo->HasEffect(SPELL_EFFECT_APPLY_AURA))
                score += 30.0f;
            if (spellInfo->HasAura(SPELL_AURA_PERIODIC_DAMAGE))
                score += 40.0f;
        }
    }

    return score;
}

float TargetSelector::CalculateGroupFocusScore(Unit* target, const SelectionContext& context)
{
    if (!target)
        return 0.0f;

    if (target == context.groupTarget)
        return 75.0f;

    uint32 focusCount = 0;
    for (Player* member : context.groupMembers)
    {
        if (member && member->GetVictim() == target)
            focusCount++;
    }

    return focusCount * 15.0f;
}

bool TargetSelector::IsHealer(Unit* target) const
{
    if (!target)
        return false;

    if (Player* player = target->ToPlayer())
    if (!target)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: target in method ToPlayer");
        if (!currentSpell)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: currentSpell in method GetSpellInfo");
            return nullptr;
        }
        return nullptr;
    }
    {
        ChrSpecialization spec = player->GetPrimarySpecialization();
        switch (spec)
        {
            case ChrSpecialization::PriestDiscipline:
            case ChrSpecialization::PriestHoly:
            case ChrSpecialization::PaladinHoly:
            case ChrSpecialization::DruidRestoration:
            case ChrSpecialization::ShamanRestoration:
            case ChrSpecialization::MonkMistweaver:
            case ChrSpecialization::EvokerPreservation:
                return true;
            default:
                return false;
        }
    }

    return false;
}

bool TargetSelector::IsCaster(Unit* target) const
{
    if (!target)
        return false;

    if (target->HasUnitState(UNIT_STATE_CASTING))
        return true;

    if (Creature* creature = target->ToCreature())
    if (!target)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: target in method ToCreature");
        return nullptr;
    }
        if (!creature)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: creature in method GetCreatureTemplate");
            return nullptr;
        }
    {
        // TODO: Fix type_flags API - currently not available
        // return creature->GetCreatureTemplate()->type_flags & CREATURE_TYPE_FLAG_SPELL_CASTER;
        return true; // Stub implementation
    }

    if (Player* player = target->ToPlayer())
    if (!target)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: target in method ToPlayer");
        return nullptr;
    }
    {
        uint8 playerClass = player->GetClass();
        if (!player)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: player in method GetClass");
            return nullptr;
        }
        if (!unit)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: unit in method IsAlive");
            return;
        }
        return (playerClass == CLASS_MAGE || playerClass == CLASS_WARLOCK ||
                playerClass == CLASS_PRIEST || playerClass == CLASS_SHAMAN ||
                playerClass == CLASS_EVOKER);
    }

    return false;
}

bool TargetSelector::IsTank(Unit* target) const
{
    if (!target)
        return false;

    if (Player* player = target->ToPlayer())
    if (!target)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: target in method ToPlayer");
        return nullptr;
    }
    {
        ChrSpecialization spec = player->GetPrimarySpecialization();
        switch (spec)
        {
            case ChrSpecialization::WarriorProtection:
            case ChrSpecialization::PaladinProtection:
            case ChrSpecialization::DeathKnightBlood:
            case ChrSpecialization::DruidGuardian:
            case ChrSpecialization::MonkBrewmaster:
            case ChrSpecialization::DemonHunterVengeance:
                return true;
            default:
                if (!unit)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: unit in method IsAlive");
                    return nullptr;
                }
                return false;
        }
    }

    return false;
}

bool TargetSelector::IsVulnerable(Unit* target) const
{
    if (!target)
        return false;

    if (target->HasUnitState(UNIT_STATE_STUNNED | UNIT_STATE_ROOT | UNIT_STATE_CONFUSED))
        return true;

    if (target->GetHealthPct() < 30.0f)
        return true;

    if (target->HasAuraType(SPELL_AURA_MOD_DAMAGE_TAKEN))
        return true;

    return false;
}

bool TargetSelector::IsInterruptible(Unit* target) const
{
    if (!target)
        return false;

    if (!target->HasUnitState(UNIT_STATE_CASTING))
        return false;

    if (Spell* currentSpell = target->GetCurrentSpell(CURRENT_GENERIC_SPELL))
    if (!target)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: target in method IsAlive");
        return nullptr;
    }
    {
        if (SpellInfo const* spellInfo = currentSpell->GetSpellInfo())
        if (!currentSpell)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: currentSpell in method GetSpellInfo");
            return nullptr;
        }
        {
            // Check if spell can be interrupted - use basic cast time check
            return spellInfo->CastTimeEntry && spellInfo->CastTimeEntry->Base > 0;
        }
    }

    return false;
}

void TargetSelector::UpdateMetrics(const SelectionResult& result)
{
    _metrics.totalSelections++;

    if (result.success)
        _metrics.successfulSelections++;
    else
        _metrics.failedSelections++;

    _metrics.targetsEvaluated += result.candidatesEvaluated;

    if (result.selectionTime > _metrics.maxSelectionTime)
        _metrics.maxSelectionTime = result.selectionTime;

    auto currentTime = std::chrono::steady_clock::now();
    auto timeSinceLastUpdate = std::chrono::duration_cast<std::chrono::seconds>(currentTime - _metrics.lastUpdate);

    if (timeSinceLastUpdate.count() >= 1)
    {
        _metrics.averageSelectionTime = _metrics.totalSelections > 0 ?
            std::chrono::microseconds(_metrics.totalSelections.load() / _metrics.totalSelections.load()) :
            std::chrono::microseconds{0};
        _metrics.lastUpdate = currentTime;
    }
}

// TargetSelectionUtils implementation
Unit* TargetSelectionUtils::GetNearestEnemy(Player* bot, float maxRange)
{
    if (!bot)
        return nullptr;

    Unit* nearestEnemy = nullptr;
    float nearestDistSq = maxRange * maxRange;

    // PHASE 5B: Thread-safe spatial grid query (replaces QueryNearbyCreatureGuids + ObjectAccessor)
    auto hostileSnapshots = SpatialGridQueryHelpers::FindHostileCreaturesInRange(bot, maxRange, true);

    // Find nearest hostile using squared distances
    for (auto const* snapshot : hostileSnapshots)
    {
        if (!snapshot)
            continue;

        float distSq = bot->GetExactDistSq(snapshot->position);
        if (distSq < nearestDistSq)
        {
            nearestDistSq = distSq;
            // Get Unit* for return (needed by caller)
            /* MIGRATION TODO: Convert to BotActionQueue or spatial grid */ Unit* unit = ObjectAccessor::GetUnit(*bot, snapshot->guid);
            if (!unit)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: unit in method IsAlive");
                return;
            }
            if (unit && unit->IsAlive())
                nearestEnemy = unit;
        }
    }

    return nearestEnemy;
}

Unit* TargetSelectionUtils::GetWeakestEnemy(Player* bot, float maxRange)
{
    if (!bot)
        return nullptr;

    Unit* weakestEnemy = nullptr;
    float lowestHealth = 100.0f;

    // PHASE 5B: Thread-safe spatial grid query (replaces QueryNearbyCreatureGuids + ObjectAccessor)
    auto hostileSnapshots = SpatialGridQueryHelpers::FindHostileCreaturesInRange(bot, maxRange, true);

    // Find weakest hostile (lowest health %)
    for (auto const* snapshot : hostileSnapshots)
    {
        if (!snapshot || snapshot->maxHealth == 0)
            continue;

        float healthPct = (float(snapshot->health) / float(snapshot->maxHealth)) * 100.0f;
        if (healthPct < lowestHealth)
        {
            lowestHealth = healthPct;
            // Get Unit* for return (needed by caller)
            /* MIGRATION TODO: Convert to BotActionQueue or spatial grid */ Unit* unit = ObjectAccessor::GetUnit(*bot, snapshot->guid);
            if (!unit)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: unit in method IsAlive");
                return;
            }
            if (unit && unit->IsAlive())
                weakestEnemy = unit;
        }
    }

    return weakestEnemy;
}

Unit* TargetSelectionUtils::GetMostWoundedAlly(Player* bot, float maxRange)
{
    if (!bot)
        return nullptr;

    Unit* woundedAlly = nullptr;
    float lowestHealth = 100.0f;

    if (Group* group = bot->GetGroup())
    {
        float maxRangeSq = maxRange * maxRange;
        for (GroupReference const& ref : group->GetMembers())
        {
            if (Player* member = ref.GetSource())
            {
                if (member != bot && bot->GetExactDistSq(member) <= maxRangeSq)
                {
                    float healthPct = member->GetHealthPct();
                    if (healthPct < lowestHealth && healthPct < 95.0f)
                    {
                        lowestHealth = healthPct;
                        woundedAlly = member;
                    }
                }
            }
        }
    }

    return woundedAlly;
}

bool TargetSelectionUtils::IsGoodHealTarget(Unit* target, Player* healer)
{
    if (!target || !healer)
        return false;

    if (!target->IsAlive() || target->GetHealthPct() >= 95.0f)
    if (!target)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: target in method IsAlive");
        return nullptr;
    }
        return false;

    if (healer->IsHostileTo(target))
        return false;

    if (healer->GetExactDistSq(target) > (40.0f * 40.0f)) // 1600.0f
        return false;

    return true;
}

bool TargetSelectionUtils::IsGoodInterruptTarget(Unit* target, Player* interrupter)
{
    if (!target || !interrupter)
        return false;

    if (!target->HasUnitState(UNIT_STATE_CASTING))
        return false;

    if (interrupter->GetExactDistSq(target) > (30.0f * 30.0f)) // 900.0f
        return false;

    if (!interrupter->IsWithinLOSInMap(target))
        return false;

    return true;
}

} // namespace Playerbot