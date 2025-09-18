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
#include "Pet.h"
#include "Creature.h"
#include "ObjectAccessor.h"

namespace Playerbot
{

TargetSelector::TargetSelector(Player* bot, ThreatManager* threatManager)
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

    std::unique_lock<std::shared_mutex> lock(_mutex);

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
            targetInfo.distance = _bot->GetDistance(candidate);
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

        result.target = evaluatedTargets[0].unit;
        result.info = evaluatedTargets[0];
        result.success = true;

        for (size_t i = 1; i < std::min(evaluatedTargets.size(), size_t(5)); ++i)
        {
            result.alternativeTargets.push_back(evaluatedTargets[i]);
        }

        TC_LOG_DEBUG("playerbot.target", "Selected target {} for bot {} with score {:.2f} (priority {})",
                   result.target->GetName(), _bot->GetName(), result.info.score, uint32(result.info.priority));
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.failureReason = "Exception during target selection: " + std::string(e.what());
        TC_LOG_ERROR("playerbot.target", "Exception in SelectBestTarget for bot {}: {}", _bot->GetName(), e.what());
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
    context.emergencyMode = _emergencyMode;
    context.validationFlags = TargetValidation::COMBAT;
    context.selectionReason = "Attack target selection";

    if (Group* group = _bot->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            if (Player* member = ref->GetSource())
                context.groupMembers.push_back(member);
        }
    }

    context.groupTarget = _groupTarget;
    context.currentTarget = _bot->GetVictim();

    return SelectBestTarget(context);
}

SelectionResult TargetSelector::SelectSpellTarget(uint32 spellId, float maxRange)
{
    SelectionContext context;
    context.bot = _bot;
    context.botRole = _threatManager ? _threatManager->GetBotRole() : ThreatRole::DPS;
    context.spellId = spellId;
    context.inCombat = _bot->IsInCombat();
    context.emergencyMode = _emergencyMode;
    context.validationFlags = TargetValidation::SPELL_TARGET;
    context.selectionReason = "Spell target selection";

    if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId))
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
    context.emergencyMode = emergencyOnly;
    context.validationFlags = TargetValidation::ALIVE | TargetValidation::NOT_HOSTILE | TargetValidation::IN_RANGE;
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
    context.validationFlags = TargetValidation::COMBAT | TargetValidation::MELEE_RANGE;
    context.selectionReason = "Tank target selection";

    context.weights.threatWeight = 3.0f;
    context.weights.distanceWeight = 2.0f;
    context.weights.healthWeight = 1.0f;
    context.weights.roleWeight = 1.5f;

    return SelectBestTarget(context);
}

bool TargetSelector::IsValidTarget(Unit* target, TargetValidation validation) const
{
    if (!target)
        return false;

    if ((validation & TargetValidation::ALIVE) && !target->IsAlive())
        return false;

    if ((validation & TargetValidation::HOSTILE) && !_bot->IsHostileTo(target))
        return false;

    if ((validation & TargetValidation::NOT_FRIENDLY) && _bot->IsFriendlyTo(target))
        return false;

    if ((validation & TargetValidation::IN_RANGE) && _bot->GetDistance(target) > _defaultMaxRange)
        return false;

    if ((validation & TargetValidation::LINE_OF_SIGHT) && !_bot->IsWithinLOSInMap(target))
        return false;

    if ((validation & TargetValidation::NOT_IMMUNE) && target->IsImmunedToDamage())
        return false;

    if ((validation & TargetValidation::NOT_EVADING) && target->IsInEvadeMode())
        return false;

    if ((validation & TargetValidation::NOT_CONFUSED) && target->HasUnitState(UNIT_STATE_CONFUSED))
        return false;

    if ((validation & TargetValidation::IN_COMBAT) && !target->IsInCombat())
        return false;

    if ((validation & TargetValidation::NOT_CROWD_CONTROLLED))
    {
        if (target->HasUnitState(UNIT_STATE_STUNNED | UNIT_STATE_ROOT | UNIT_STATE_CONFUSED | UNIT_STATE_FLEEING))
            return false;
    }

    if ((validation & TargetValidation::THREAT_REQUIRED) && _threatManager)
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

    return _bot->GetDistance(target) <= range;
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

    return _bot->CanAttack(target) && !target->HasUnitFlag(UNIT_FLAG_IMMUNE_TO_PC);
}

bool TargetSelector::CanCast(Unit* target, uint32 spellId) const
{
    if (!target || !spellId)
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return false;

    if (_bot->GetDistance(target) > spellInfo->GetMaxRange())
        return false;

    if (!_bot->IsWithinLOSInMap(target))
        return false;

    return _bot->CanCast(target, spellInfo, false);
}

float TargetSelector::CalculateTargetScore(Unit* target, const SelectionContext& context)
{
    if (!target)
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

TargetPriority TargetSelector::DetermineTargetPriority(Unit* target, const SelectionContext& context)
{
    if (!target)
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

    if (_bot->GetDistance(target) <= 8.0f)
        return TargetPriority::SECONDARY;

    return TargetPriority::SECONDARY;
}

std::vector<Unit*> TargetSelector::GetNearbyEnemies(float range) const
{
    std::vector<Unit*> enemies;

    auto check = [&](Unit* unit) -> bool
    {
        if (!unit || !unit->IsAlive())
            return false;

        if (!_bot->IsHostileTo(unit))
            return false;

        if (_bot->GetDistance(unit) > range)
            return false;

        enemies.push_back(unit);
        return false;
    };

    Trinity::AnyUnitInObjectRangeCheck checker(_bot, range);
    Trinity::UnitSearcher<Trinity::AnyUnitInObjectRangeCheck> searcher(_bot, enemies, checker);

    Cell::VisitAllObjects(_bot, searcher, range);

    return enemies;
}

std::vector<Unit*> TargetSelector::GetNearbyAllies(float range) const
{
    std::vector<Unit*> allies;

    if (Group* group = _bot->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            if (Player* member = ref->GetSource())
            {
                if (member != _bot && _bot->GetDistance(member) <= range)
                    allies.push_back(member);
            }
        }
    }

    auto check = [&](Unit* unit) -> bool
    {
        if (!unit || !unit->IsAlive())
            return false;

        if (_bot->IsHostileTo(unit))
            return false;

        if (_bot->GetDistance(unit) > range)
            return false;

        if (Pet* pet = unit->ToPet())
        {
            if (Player* owner = pet->GetOwner())
            {
                if (_bot->IsFriendlyTo(owner))
                    allies.push_back(unit);
            }
        }

        return false;
    };

    Trinity::AnyUnitInObjectRangeCheck checker(_bot, range);
    Trinity::UnitSearcher<Trinity::AnyUnitInObjectRangeCheck> searcher(_bot, allies, checker);

    Cell::VisitAllObjects(_bot, searcher, range);

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

    float distance = _bot->GetDistance(target);
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
{
    if (!target)
        return 0.0f;

    float score = 0.0f;

    if (IsVulnerable(target))
        score += 30.0f;

    if (target->HasUnitState(UNIT_STATE_STUNNED))
        score += 20.0f;

    if (target->HasUnitState(UNIT_STATE_ROOT))
        score += 15.0f;

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
    {
        if (SpellInfo const* spellInfo = currentSpell->GetSpellInfo())
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
    {
        uint8 spec = player->GetUInt32Value(PLAYER_FIELD_CURRENT_SPEC_ID);
        switch (player->getClass())
        {
            case CLASS_PRIEST:
                return spec == TALENT_SPEC_PRIEST_DISCIPLINE || spec == TALENT_SPEC_PRIEST_HOLY;
            case CLASS_PALADIN:
                return spec == TALENT_SPEC_PALADIN_HOLY;
            case CLASS_SHAMAN:
                return spec == TALENT_SPEC_SHAMAN_RESTORATION;
            case CLASS_DRUID:
                return spec == TALENT_SPEC_DRUID_RESTORATION;
            case CLASS_MONK:
                return spec == TALENT_SPEC_MONK_MISTWEAVER;
            case CLASS_EVOKER:
                return spec == TALENT_SPEC_EVOKER_PRESERVATION;
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
    {
        return creature->GetCreatureTemplate()->type_flags & CREATURE_TYPE_FLAG_SPELL_CASTER;
    }

    if (Player* player = target->ToPlayer())
    {
        uint8 playerClass = player->getClass();
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
    {
        uint8 spec = player->GetUInt32Value(PLAYER_FIELD_CURRENT_SPEC_ID);
        switch (player->getClass())
        {
            case CLASS_WARRIOR:
                return spec == TALENT_SPEC_WARRIOR_PROTECTION;
            case CLASS_PALADIN:
                return spec == TALENT_SPEC_PALADIN_PROTECTION;
            case CLASS_DEATH_KNIGHT:
                return spec == TALENT_SPEC_DEATHKNIGHT_BLOOD;
            case CLASS_DRUID:
                return spec == TALENT_SPEC_DRUID_BEAR;
            case CLASS_MONK:
                return spec == TALENT_SPEC_MONK_BREWMASTER;
            case CLASS_DEMON_HUNTER:
                return spec == TALENT_SPEC_DEMON_HUNTER_VENGEANCE;
            default:
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
    {
        if (SpellInfo const* spellInfo = currentSpell->GetSpellInfo())
        {
            if (spellInfo->HasAttribute(SPELL_ATTR0_UNAFFECTED_BY_INVULNERABILITY))
                return false;

            return !spellInfo->HasAttribute(SPELL_ATTR4_NOT_INTERRUPTIBLE);
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
    float nearestDistance = maxRange;

    auto check = [&](Unit* unit) -> bool
    {
        if (!unit || !unit->IsAlive() || !bot->IsHostileTo(unit))
            return false;

        float distance = bot->GetDistance(unit);
        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            nearestEnemy = unit;
        }
        return false;
    };

    std::list<Unit*> units;
    Trinity::AnyUnitInObjectRangeCheck checker(bot, maxRange);
    Trinity::UnitSearcher<Trinity::AnyUnitInObjectRangeCheck> searcher(bot, units, checker);
    Cell::VisitAllObjects(bot, searcher, maxRange);

    return nearestEnemy;
}

Unit* TargetSelectionUtils::GetWeakestEnemy(Player* bot, float maxRange)
{
    if (!bot)
        return nullptr;

    Unit* weakestEnemy = nullptr;
    float lowestHealth = 100.0f;

    std::list<Unit*> units;
    Trinity::AnyUnitInObjectRangeCheck checker(bot, maxRange);
    Trinity::UnitSearcher<Trinity::AnyUnitInObjectRangeCheck> searcher(bot, units, checker);
    Cell::VisitAllObjects(bot, searcher, maxRange);

    for (Unit* unit : units)
    {
        if (!unit || !unit->IsAlive() || !bot->IsHostileTo(unit))
            continue;

        float healthPct = unit->GetHealthPct();
        if (healthPct < lowestHealth)
        {
            lowestHealth = healthPct;
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
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            if (Player* member = ref->GetSource())
            {
                if (member != bot && bot->GetDistance(member) <= maxRange)
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
        return false;

    if (healer->IsHostileTo(target))
        return false;

    if (healer->GetDistance(target) > 40.0f)
        return false;

    return true;
}

bool TargetSelectionUtils::IsGoodInterruptTarget(Unit* target, Player* interrupter)
{
    if (!target || !interrupter)
        return false;

    if (!target->HasUnitState(UNIT_STATE_CASTING))
        return false;

    if (interrupter->GetDistance(target) > 30.0f)
        return false;

    if (!interrupter->IsWithinLOSInMap(target))
        return false;

    return true;
}

} // namespace Playerbot