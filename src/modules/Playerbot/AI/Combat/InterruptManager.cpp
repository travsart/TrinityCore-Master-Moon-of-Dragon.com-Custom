/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "InterruptManager.h"
#include "Player.h"
#include "Unit.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Log.h"
#include "Group.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include <algorithm>
#include <cmath>

namespace Playerbot
{

InterruptManager::InterruptManager(Player* bot)
    : _bot(bot), _isInterrupting(false), _currentTarget(nullptr),
      _reactionTime(DEFAULT_REACTION_TIME), _maxInterruptRange(DEFAULT_MAX_RANGE),
      _scanInterval(DEFAULT_SCAN_INTERVAL), _predictiveInterrupts(true), _emergencyMode(false),
      _timingAccuracyTarget(TIMING_ACCURACY_TARGET), _lastScan(0), _lastInterruptAttempt(0),
      _lastCoordinationUpdate(0)
{
    if (!_bot)
    {
        TC_LOG_ERROR("playerbot", "InterruptManager: Bot player is null!");
        return;
    }

    InitializeInterruptCapabilities();
    TC_LOG_DEBUG("playerbot.interrupt", "InterruptManager initialized for bot {} with {} capabilities",
               _bot->GetName(), _interruptCapabilities.size());
}

void InterruptManager::UpdateInterruptSystem(uint32 diff)
{
    std::unique_lock<std::shared_mutex> lock(_mutex);

    uint32 currentTime = getMSTime();
    if (currentTime - _lastScan < _scanInterval && !_emergencyMode)
        return;

    _lastScan = currentTime;

    try
    {
        ScanNearbyUnitsForCasts();
        UpdateInterruptCapabilities();

        if (!_trackedTargets.empty())
        {
            ProcessInterruptOpportunities();

            if (_predictiveInterrupts)
            {
                HandleMultipleInterruptTargets();
            }
        }

        if (currentTime - _lastCoordinationUpdate >= COORDINATION_UPDATE_INTERVAL)
        {
            if (Group* group = _bot->GetGroup())
            {
                std::vector<Player*> groupMembers;
                for (GroupReference const& ref : group->GetMembers())
                {
                    if (Player* member = ref.GetSource())
                        groupMembers.push_back(member);
                }
                CoordinateInterruptsWithGroup(groupMembers);
            }
            _lastCoordinationUpdate = currentTime;
        }

        auto it = _trackedTargets.begin();
        while (it != _trackedTargets.end())
        {
            if (currentTime - it->detectedTime > TARGET_TRACKING_DURATION)
                it = _trackedTargets.erase(it);
            else
                ++it;
        }
    }
    catch (const std::exception& e)
    {
        TC_LOG_ERROR("playerbot.interrupt", "Exception in UpdateInterruptSystem for bot {}: {}", _bot->GetName(), e.what());
    }
}

std::vector<InterruptTarget> InterruptManager::ScanForInterruptTargets()
{
    std::vector<InterruptTarget> targets;

    std::list<Unit*> nearbyUnits;
    Trinity::AnyUnitInObjectRangeCheck check(_bot, _maxInterruptRange);
    Trinity::UnitSearcher<Trinity::AnyUnitInObjectRangeCheck> searcher(_bot, nearbyUnits, check);
    Cell::VisitAllObjects(_bot, searcher, _maxInterruptRange);

    for (Unit* unit : nearbyUnits)
    {
        if (!IsValidInterruptTarget(unit))
            continue;

        if (!unit->HasUnitState(UNIT_STATE_CASTING))
            continue;

        Spell* currentSpell = unit->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (!currentSpell)
            continue;

        const SpellInfo* spellInfo = currentSpell->GetSpellInfo();
        if (!spellInfo || !IsSpellInterruptWorthy(spellInfo->Id, unit))
            continue;

        InterruptTarget target;
        target.guid = unit->GetGUID();
        target.unit = unit;
        target.position = unit->GetPosition();
        target.currentSpell = currentSpell;
        target.spellInfo = spellInfo;
        target.spellId = spellInfo->Id;
        target.priority = AssessInterruptPriority(spellInfo, unit);
        target.type = ClassifyInterruptType(spellInfo);
        target.totalCastTime = static_cast<float>(spellInfo->CalcCastTime());
        target.remainingCastTime = static_cast<float>(currentSpell->GetCastTime() - currentSpell->GetCastedTime());
        target.castProgress = (target.totalCastTime - target.remainingCastTime) / target.totalCastTime;
        target.detectedTime = getMSTime();
        target.isChanneled = spellInfo->IsChanneled();
        target.isInterruptible = !spellInfo->HasAttribute(SPELL_ATTR4_NOT_INTERRUPTIBLE);
        target.requiresLoS = !spellInfo->HasAttribute(SPELL_ATTR2_NOT_NEED_FACING);
        target.spellName = spellInfo->SpellName[0];
        target.targetName = unit->GetName();

        if (target.isInterruptible && target.priority != InterruptPriority::IGNORE)
        {
            targets.push_back(target);
        }
    }

    std::sort(targets.begin(), targets.end(),
        [](const InterruptTarget& a, const InterruptTarget& b) {
            if (a.priority != b.priority)
                return a.priority < b.priority;
            return a.remainingCastTime < b.remainingCastTime;
        });

    return targets;
}

InterruptResult InterruptManager::AttemptInterrupt(const InterruptTarget& target)
{
    auto startTime = std::chrono::steady_clock::now();
    InterruptResult result;
    result.originalTarget = target;

    std::unique_lock<std::shared_mutex> lock(_mutex);

    try
    {
        if (!target.unit || !target.unit->IsAlive())
        {
            result.failureReason = "Target is no longer valid";
            return result;
        }

        if (!target.unit->HasUnitState(UNIT_STATE_CASTING))
        {
            result.failureReason = "Target is no longer casting";
            return result;
        }

        InterruptCapability* capability = GetBestInterruptForTarget(target);
        if (!capability || !capability->isAvailable)
        {
            result.failureReason = "No available interrupt capability";
            return result;
        }

        InterruptPlan plan = CreateInterruptPlan(target);
        if (!IsInterruptExecutable(plan))
        {
            result.failureReason = "Interrupt plan not executable: " + plan.reasoning;
            return result;
        }

        bool executionSuccess = ExecuteInterruptPlan(plan);
        result.attemptMade = true;
        result.success = executionSuccess;
        result.usedMethod = plan.method;
        result.usedSpell = capability->spellId;

        auto endTime = std::chrono::steady_clock::now();
        auto reactionTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        result.executionTime = static_cast<uint32>(reactionTime.count());

        float expectedTime = target.remainingCastTime;
        float actualTime = static_cast<float>(reactionTime.count()) / 1000.0f;
        result.timingAccuracy = 1.0f - std::abs(expectedTime - actualTime) / expectedTime;

        _metrics.interruptAttempts++;
        if (result.success)
        {
            _metrics.successfulInterrupts++;
            if (target.priority == InterruptPriority::CRITICAL)
                _metrics.criticalInterrupts++;
        }
        else
        {
            _metrics.failedInterrupts++;
        }

        UpdateReactionTimeMetrics(reactionTime);
        UpdateTimingAccuracy(result);

        TC_LOG_DEBUG("playerbot.interrupt", "Bot {} attempted interrupt on {} ({}): {}",
                   _bot->GetName(), target.targetName, target.spellName,
                   result.success ? "SUCCESS" : result.failureReason);
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.failureReason = "Exception during interrupt: " + std::string(e.what());
        TC_LOG_ERROR("playerbot.interrupt", "Exception in AttemptInterrupt for bot {}: {}", _bot->GetName(), e.what());
    }

    return result;
}

void InterruptManager::ProcessInterruptOpportunities()
{
    std::vector<InterruptTarget> targets = ScanForInterruptTargets();
    _trackedTargets = targets;

    if (targets.empty())
        return;

    std::vector<InterruptPlan> plans = GenerateInterruptPlans(targets);
    if (plans.empty())
        return;

    std::sort(plans.begin(), plans.end());

    for (const InterruptPlan& plan : plans)
    {
        if (!plan.target || plan.target->priority == InterruptPriority::IGNORE)
            continue;

        if (ShouldLetOthersInterrupt(*plan.target))
            continue;

        float urgency = CalculateInterruptUrgency(*plan.target);
        if (urgency < 0.5f && plan.target->priority > InterruptPriority::HIGH)
            continue;

        if (ExecuteInterruptPlan(plan))
        {
            RegisterInterruptAttempt(*plan.target);
            break;
        }
    }
}

InterruptPriority InterruptManager::AssessInterruptPriority(const SpellInfo* spellInfo, Unit* caster)
{
    if (!spellInfo || !caster)
        return InterruptPriority::IGNORE;

    uint32 spellId = spellInfo->Id;

    InterruptPriority configuredPriority = InterruptUtils::GetSpellInterruptPriority(spellId);
    if (configuredPriority != InterruptPriority::IGNORE)
        return configuredPriority;

    if (ShouldInterruptHealing(spellInfo, caster))
        return AssessHealingPriority(spellInfo, caster);

    if (ShouldInterruptCrowdControl(spellInfo, caster))
        return AssessCrowdControlPriority(spellInfo, caster);

    if (ShouldInterruptDamage(spellInfo, caster))
        return AssessDamagePriority(spellInfo, caster);

    if (ShouldInterruptBuff(spellInfo, caster))
        return AssessBuffPriority(spellInfo, caster);

    return InterruptPriority::LOW;
}

InterruptType InterruptManager::ClassifyInterruptType(const SpellInfo* spellInfo)
{
    if (!spellInfo)
        return InterruptType::DAMAGE_PREVENTION;

    if (spellInfo->HasEffect(SPELL_EFFECT_HEAL) || spellInfo->HasEffect(SPELL_EFFECT_HEAL_PCT))
        return InterruptType::HEALING_DENIAL;

    if (spellInfo->HasEffect(SPELL_EFFECT_APPLY_AURA))
    {
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        {
            if (spellInfo->Effects[i].ApplyAuraName == SPELL_AURA_MOD_STUN ||
                spellInfo->Effects[i].ApplyAuraName == SPELL_AURA_MOD_FEAR ||
                spellInfo->Effects[i].ApplyAuraName == SPELL_AURA_MOD_CHARM ||
                spellInfo->Effects[i].ApplyAuraName == SPELL_AURA_MOD_CONFUSE)
            {
                return InterruptType::CROWD_CONTROL;
            }
        }

        if (spellInfo->IsPositive())
            return InterruptType::BUFF_DENIAL;
        else
            return InterruptType::DEBUFF_PREVENTION;
    }

    if (spellInfo->IsChanneled())
        return InterruptType::CHANNEL_BREAK;

    return InterruptType::DAMAGE_PREVENTION;
}

bool InterruptManager::IsSpellInterruptWorthy(uint32 spellId, Unit* caster)
{
    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return false;

    if (spellInfo->HasAttribute(SPELL_ATTR4_NOT_INTERRUPTIBLE))
        return false;

    InterruptPriority priority = AssessInterruptPriority(spellInfo, caster);
    return priority != InterruptPriority::IGNORE;
}

void InterruptManager::InitializeInterruptCapabilities()
{
    _interruptCapabilities.clear();

    uint8 botClass = _bot->GetClass();
    std::vector<uint32> classInterrupts = InterruptUtils::GetClassInterruptSpells(botClass);

    for (uint32 spellId : classInterrupts)
    {
        const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
            continue;

        InterruptCapability capability;
        capability.spellId = spellId;
        capability.spellName = spellInfo->SpellName[0];
        capability.range = spellInfo->GetMaxRange();
        capability.cooldown = static_cast<float>(spellInfo->RecoveryTime);
        capability.manaCost = spellInfo->ManaCost;
        capability.castTime = static_cast<float>(spellInfo->CalcCastTime());
        capability.requiresLoS = !spellInfo->HasAttribute(SPELL_ATTR2_NOT_NEED_FACING);
        capability.requiresFacing = !spellInfo->HasAttribute(SPELL_ATTR5_DONT_TURN_DURING_CAST);

        if (spellInfo->HasEffect(SPELL_EFFECT_INTERRUPT_CAST))
            capability.method = InterruptMethod::SPELL_INTERRUPT;
        else if (spellInfo->HasEffect(SPELL_EFFECT_APPLY_AURA))
        {
            for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
            {
                if (spellInfo->Effects[i].ApplyAuraName == SPELL_AURA_MOD_STUN)
                    capability.method = InterruptMethod::STUN;
                else if (spellInfo->Effects[i].ApplyAuraName == SPELL_AURA_MOD_SILENCE)
                    capability.method = InterruptMethod::SILENCE;
                else if (spellInfo->Effects[i].ApplyAuraName == SPELL_AURA_MOD_FEAR)
                    capability.method = InterruptMethod::FEAR;
            }
        }
        else if (spellInfo->HasEffect(SPELL_EFFECT_KNOCK_BACK))
            capability.method = InterruptMethod::KNOCKBACK;

        capability.minPriority = InterruptPriority::MODERATE;
        capability.lastUsed = 0;

        _interruptCapabilities.push_back(capability);
    }

    TC_LOG_DEBUG("playerbot.interrupt", "Initialized {} interrupt capabilities for bot {}",
               _interruptCapabilities.size(), _bot->GetName());
}

void InterruptManager::UpdateInterruptCapabilities()
{
    for (InterruptCapability& capability : _interruptCapabilities)
    {
        uint32 currentTime = getMSTime();
        capability.isAvailable = _bot->HasSpell(capability.spellId) &&
                                !_bot->HasSpellCooldown(capability.spellId) &&
                                (currentTime - capability.lastUsed >= static_cast<uint32>(capability.cooldown));

        if (capability.manaCost > 0)
        {
            capability.isAvailable = capability.isAvailable &&
                                   _bot->GetPower(POWER_MANA) >= capability.manaCost;
        }
    }
}

InterruptCapability* InterruptManager::GetBestInterruptForTarget(const InterruptTarget& target)
{
    InterruptCapability* bestCapability = nullptr;
    float bestEffectiveness = 0.0f;

    for (InterruptCapability& capability : _interruptCapabilities)
    {
        if (!capability.isAvailable)
            continue;

        if (capability.minPriority > target.priority)
            continue;

        if (capability.range < _bot->GetDistance(target.unit))
            continue;

        float effectiveness = CalculateInterruptEffectiveness(capability, target);
        if (effectiveness > bestEffectiveness)
        {
            bestEffectiveness = effectiveness;
            bestCapability = &capability;
        }
    }

    return bestCapability;
}

InterruptPlan InterruptManager::CreateInterruptPlan(const InterruptTarget& target)
{
    InterruptPlan plan;
    plan.target = const_cast<InterruptTarget*>(&target);

    InterruptCapability* capability = GetBestInterruptForTarget(target);
    if (!capability)
    {
        plan.reasoning = "No available interrupt capability";
        return plan;
    }

    plan.capability = capability;
    plan.method = capability->method;
    plan.priority = static_cast<uint32>(target.priority);

    float reactionDelay = CalculateReactionDelay();
    float executionTime = CalculateExecutionTime(capability->method);
    plan.executionTime = executionTime;
    plan.reactionTime = reactionDelay;

    if (target.remainingCastTime < executionTime + reactionDelay)
    {
        plan.successProbability = 0.0f;
        plan.reasoning = "Insufficient time to execute interrupt";
        return plan;
    }

    plan.successProbability = std::min(1.0f, (target.remainingCastTime - reactionDelay) / executionTime);

    if (capability->requiresLoS && !HasLineOfSightToTarget(target.unit))
    {
        plan.requiresMovement = true;
        plan.executionPosition = CalculateOptimalInterruptPosition(target.unit);
    }

    plan.reasoning = "Interrupt plan generated successfully";
    return plan;
}

std::vector<InterruptPlan> InterruptManager::GenerateInterruptPlans(const std::vector<InterruptTarget>& targets)
{
    std::vector<InterruptPlan> plans;
    plans.reserve(targets.size());

    for (const InterruptTarget& target : targets)
    {
        InterruptPlan plan = CreateInterruptPlan(target);
        if (plan.successProbability > 0.0f)
        {
            plans.push_back(plan);
        }
    }

    return plans;
}

bool InterruptManager::ExecuteInterruptPlan(const InterruptPlan& plan)
{
    if (!plan.capability || !plan.target || !plan.target->unit)
        return false;

    try
    {
        _isInterrupting = true;
        _currentTarget = plan.target;

        if (plan.requiresMovement)
        {
            _bot->GetMotionMaster()->MovePoint(0, plan.executionPosition.GetPositionX(),
                                             plan.executionPosition.GetPositionY(),
                                             plan.executionPosition.GetPositionZ());
            return false;
        }

        bool success = false;
        switch (plan.method)
        {
            case InterruptMethod::SPELL_INTERRUPT:
            case InterruptMethod::STUN:
            case InterruptMethod::SILENCE:
            case InterruptMethod::FEAR:
            case InterruptMethod::KNOCKBACK:
                success = CastInterruptSpell(plan.capability->spellId, plan.target->unit);
                break;

            case InterruptMethod::LINE_OF_SIGHT:
                success = AttemptLoSInterrupt(plan.target->unit);
                break;

            case InterruptMethod::MOVEMENT:
                success = AttemptMovementInterrupt(plan.target->unit);
                break;

            default:
                success = CastInterruptSpell(plan.capability->spellId, plan.target->unit);
                break;
        }

        if (success)
        {
            plan.capability->lastUsed = getMSTime();
            _lastInterruptAttempt = getMSTime();
        }

        _isInterrupting = false;
        _currentTarget = nullptr;

        return success;
    }
    catch (const std::exception& e)
    {
        TC_LOG_ERROR("playerbot.interrupt", "Exception executing interrupt plan for bot {}: {}", _bot->GetName(), e.what());
        _isInterrupting = false;
        _currentTarget = nullptr;
        return false;
    }
}

bool InterruptManager::ShouldInterruptHealing(const SpellInfo* spellInfo, Unit* caster)
{
    if (!spellInfo || !caster)
        return false;

    return spellInfo->HasEffect(SPELL_EFFECT_HEAL) || spellInfo->HasEffect(SPELL_EFFECT_HEAL_PCT);
}

bool InterruptManager::ShouldInterruptCrowdControl(const SpellInfo* spellInfo, Unit* caster)
{
    if (!spellInfo || !caster)
        return false;

    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (spellInfo->Effects[i].ApplyAuraName == SPELL_AURA_MOD_STUN ||
            spellInfo->Effects[i].ApplyAuraName == SPELL_AURA_MOD_FEAR ||
            spellInfo->Effects[i].ApplyAuraName == SPELL_AURA_MOD_CHARM ||
            spellInfo->Effects[i].ApplyAuraName == SPELL_AURA_MOD_CONFUSE)
        {
            return true;
        }
    }

    return false;
}

bool InterruptManager::ShouldInterruptDamage(const SpellInfo* spellInfo, Unit* caster)
{
    if (!spellInfo || !caster)
        return false;

    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (spellInfo->Effects[i].Effect == SPELL_EFFECT_SCHOOL_DAMAGE ||
            spellInfo->Effects[i].Effect == SPELL_EFFECT_WEAPON_DAMAGE ||
            spellInfo->Effects[i].Effect == SPELL_EFFECT_WEAPON_PERCENT_DAMAGE)
        {
            return spellInfo->Effects[i].CalcValue() > 1000;
        }
    }

    return false;
}

float InterruptManager::CalculateInterruptUrgency(const InterruptTarget& target)
{
    float urgency = 0.0f;

    switch (target.priority)
    {
        case InterruptPriority::CRITICAL:
            urgency += 1.0f;
            break;
        case InterruptPriority::HIGH:
            urgency += 0.8f;
            break;
        case InterruptPriority::MODERATE:
            urgency += 0.6f;
            break;
        case InterruptPriority::LOW:
            urgency += 0.3f;
            break;
        default:
            urgency = 0.0f;
            break;
    }

    float timeUrgency = 1.0f - (target.remainingCastTime / target.totalCastTime);
    urgency += timeUrgency * 0.5f;

    return std::min(1.0f, urgency);
}

void InterruptManager::ScanNearbyUnitsForCasts()
{
    std::vector<InterruptTarget> newTargets = ScanForInterruptTargets();

    for (const InterruptTarget& newTarget : newTargets)
    {
        auto it = std::find_if(_trackedTargets.begin(), _trackedTargets.end(),
            [&newTarget](const InterruptTarget& existing) {
                return existing.guid == newTarget.guid && existing.spellId == newTarget.spellId;
            });

        if (it == _trackedTargets.end())
        {
            auto& firstDetected = _targetFirstDetected[newTarget.guid];
            if (firstDetected == 0)
                firstDetected = getMSTime();

            _trackedTargets.push_back(newTarget);
        }
        else
        {
            UpdateTargetInformation(*it);
        }
    }
}

bool InterruptManager::IsValidInterruptTarget(Unit* unit)
{
    if (!unit || !unit->IsAlive())
        return false;

    if (!_bot->IsHostileTo(unit))
        return false;

    if (unit->IsImmunedToSpellEffect(sSpellMgr->GetSpellInfo(SPELL_EFFECT_INTERRUPT_CAST), SpellEffIndex::EFFECT_0))
        return false;

    if (_bot->GetDistance(unit) > _maxInterruptRange)
        return false;

    return true;
}

bool InterruptManager::CastInterruptSpell(uint32 spellId, Unit* target)
{
    if (!target || !_bot->HasSpell(spellId))
        return false;

    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return false;

    if (_bot->HasSpellCooldown(spellId))
        return false;

    if (spellInfo->ManaCost > 0 && _bot->GetPower(POWER_MANA) < spellInfo->ManaCost)
        return false;

    if (!_bot->IsWithinLOSInMap(target))
        return false;

    if (_bot->GetDistance(target) > spellInfo->GetMaxRange())
        return false;

    _bot->CastSpell(target, spellId, false);
    return true;
}

float InterruptManager::CalculateReactionDelay()
{
    return static_cast<float>(_reactionTime) / 1000.0f;
}

float InterruptManager::CalculateExecutionTime(InterruptMethod method)
{
    switch (method)
    {
        case InterruptMethod::SPELL_INTERRUPT:
        case InterruptMethod::STUN:
        case InterruptMethod::SILENCE:
        case InterruptMethod::FEAR:
            return 0.5f;

        case InterruptMethod::KNOCKBACK:
            return 0.3f;

        case InterruptMethod::LINE_OF_SIGHT:
        case InterruptMethod::MOVEMENT:
            return 1.0f;

        default:
            return 0.5f;
    }
}

void InterruptManager::UpdateTargetInformation(InterruptTarget& target)
{
    if (!target.unit || !target.unit->IsAlive())
        return;

    if (!target.unit->HasUnitState(UNIT_STATE_CASTING))
        return;

    Spell* currentSpell = target.unit->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    if (!currentSpell)
        return;

    target.remainingCastTime = static_cast<float>(currentSpell->GetCastTime() - currentSpell->GetCastedTime());
    target.castProgress = (target.totalCastTime - target.remainingCastTime) / target.totalCastTime;
    target.position = target.unit->GetPosition();
}

bool InterruptManager::HasLineOfSightToTarget(Unit* target)
{
    return target && _bot->IsWithinLOSInMap(target);
}

Position InterruptManager::CalculateOptimalInterruptPosition(Unit* target)
{
    if (!target)
        return _bot->GetPosition();

    Position targetPos = target->GetPosition();
    Position botPos = _bot->GetPosition();

    float angle = std::atan2(targetPos.GetPositionY() - botPos.GetPositionY(),
                           targetPos.GetPositionX() - botPos.GetPositionX());

    Position optimalPos;
    optimalPos.m_positionX = targetPos.GetPositionX() - 15.0f * std::cos(angle);
    optimalPos.m_positionY = targetPos.GetPositionY() - 15.0f * std::sin(angle);
    optimalPos.m_positionZ = targetPos.GetPositionZ();

    return optimalPos;
}

void InterruptManager::UpdateReactionTimeMetrics(std::chrono::microseconds reactionTime)
{
    if (reactionTime < _metrics.minReactionTime)
        _metrics.minReactionTime = reactionTime;

    if (reactionTime > _metrics.maxReactionTime)
        _metrics.maxReactionTime = reactionTime;

    _metrics.averageReactionTime = std::chrono::microseconds(
        static_cast<uint64_t>(_metrics.averageReactionTime.count() * 0.9 + reactionTime.count() * 0.1)
    );
}

void InterruptManager::UpdateTimingAccuracy(const InterruptResult& result)
{
    _metrics.averageTimingAccuracy = _metrics.averageTimingAccuracy * 0.9f + result.timingAccuracy * 0.1f;
}

// InterruptUtils implementation
InterruptPriority InterruptUtils::GetSpellInterruptPriority(uint32 spellId)
{
    switch (spellId)
    {
        // Critical interrupts - Healing
        case 2061:   // Flash Heal
        case 596:    // Prayer of Healing
        case 25314:  // Greater Heal
            return InterruptPriority::CRITICAL;

        // Critical interrupts - CC
        case 118:    // Polymorph
        case 5782:   // Fear
        case 6770:   // Sap
            return InterruptPriority::CRITICAL;

        // High priority - Major damage spells
        case 133:    // Fireball
        case 5676:   // Searing Pain
        case 172:    // Corruption
            return InterruptPriority::HIGH;

        default:
            return InterruptPriority::MODERATE;
    }
}

std::vector<uint32> InterruptUtils::GetClassInterruptSpells(uint8 playerClass)
{
    std::vector<uint32> interrupts;

    switch (playerClass)
    {
        case CLASS_WARRIOR:
            interrupts.push_back(6552);  // Pummel
            interrupts.push_back(72);    // Shield Bash
            break;

        case CLASS_PALADIN:
            interrupts.push_back(96231); // Rebuke
            break;

        case CLASS_HUNTER:
            interrupts.push_back(147362); // Counter Shot
            interrupts.push_back(19577);  // Intimidation
            break;

        case CLASS_ROGUE:
            interrupts.push_back(1766);  // Kick
            interrupts.push_back(408);   // Kidney Shot
            break;

        case CLASS_PRIEST:
            interrupts.push_back(15487); // Silence
            interrupts.push_back(8122);  // Psychic Scream
            break;

        case CLASS_DEATH_KNIGHT:
            interrupts.push_back(47528); // Mind Freeze
            interrupts.push_back(47476); // Strangulate
            break;

        case CLASS_SHAMAN:
            interrupts.push_back(57994); // Wind Shear
            break;

        case CLASS_MAGE:
            interrupts.push_back(2139);  // Counterspell
            break;

        case CLASS_WARLOCK:
            interrupts.push_back(19647); // Spell Lock
            interrupts.push_back(6789);  // Death Coil
            break;

        case CLASS_MONK:
            interrupts.push_back(116705); // Spear Hand Strike
            break;

        case CLASS_DRUID:
            interrupts.push_back(78675);  // Solar Beam
            interrupts.push_back(16979);  // Feral Charge - Bear
            break;

        case CLASS_DEMON_HUNTER:
            interrupts.push_back(183752); // Disrupt
            break;

        case CLASS_EVOKER:
            interrupts.push_back(351338); // Quell
            break;
    }

    return interrupts;
}

bool InterruptUtils::CanClassInterrupt(uint8 playerClass)
{
    std::vector<uint32> interrupts = GetClassInterruptSpells(playerClass);
    return !interrupts.empty();
}

float InterruptUtils::GetClassInterruptRange(uint8 playerClass)
{
    switch (playerClass)
    {
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
        case CLASS_ROGUE:
        case CLASS_DEATH_KNIGHT:
        case CLASS_MONK:
        case CLASS_DEMON_HUNTER:
            return 5.0f;  // Melee range

        case CLASS_HUNTER:
        case CLASS_MAGE:
        case CLASS_WARLOCK:
        case CLASS_SHAMAN:
        case CLASS_EVOKER:
            return 30.0f; // Ranged

        case CLASS_PRIEST:
        case CLASS_DRUID:
            return 20.0f; // Medium range

        default:
            return 15.0f;
    }
}

} // namespace Playerbot