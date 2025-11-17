/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license
 * Copyright (C) 2008-2016 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 */

#include "TargetManager.h"
#include "Player.h"
#include "Unit.h"
#include "ThreatManager.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "Log.h"
#include <algorithm>

namespace Playerbot
{

// TMTargetInfo implementation
float TMTargetInfo::CalculateScore() const
{
    if (!target || target->isDead())
        return 0.0f;

    // Start with priority base score
    float score = 100.0f;

    switch (priority)
    {
        case TMTargetPriority::CRITICAL:
            score = 1000.0f;
            break;
        case TMTargetPriority::HIGH:
            score = 500.0f;
            break;
        case TMTargetPriority::MEDIUM:
            score = 250.0f;
            break;
        case TMTargetPriority::LOW:
            score = 100.0f;
            break;
        case TMTargetPriority::IGNORE:
            return 0.0f;
    }

    // Execute range bonus (low HP = kill priority)
    if (healthPercent < 20.0f)
        score *= 2.0f;
    else if (healthPercent < 35.0f)
        score *= 1.5f;

    // Distance penalty (prefer closer targets)
    float distanceFactor = 1.0f / (1.0f + (distance / 10.0f));
    score *= distanceFactor;

    // Threat bonus (high threat = protect group)
    score += threatLevel * 50.0f;

    // Recent damage bonus (prioritize active threats)
    score += damageDealt * 0.5f;

    // Target switching penalty (reduce thrashing)
    if (timeSinceLastSwitch < 5000)
        score *= 0.8f;

    return score;
}

// TargetManager implementation
TargetManager::TargetManager(Player* bot)
    : _bot(bot)
    , _currentTarget()
    , _lastUpdate(0)
    , _lastSwitchTime(0)
{
}

void TargetManager::Update(uint32 diff, const CombatMetrics& metrics)
{
    if (!_bot)
        return;

    _lastUpdate += diff;

    // Throttle updates
    if (_lastUpdate < UPDATE_INTERVAL)
        return;

    _lastUpdate = 0;

    // Update target cache
    UpdateTargetCache(metrics);
}

void TargetManager::Reset()
{
    _currentTarget = ObjectGuid::Empty;
    _lastUpdate = 0;
    _lastSwitchTime = 0;
    _targetCache.clear();
}

Unit* TargetManager::GetPriorityTarget()
{
    if (!_bot)
        return nullptr;

    ::std::vector<Unit*> targets = GetCombatTargets();

    if (targets.empty())
        return nullptr;

    // Score all targets
    ::std::vector<::std::pair<Unit*, float>> scoredTargets;

    for (Unit* target : targets)
    {
        TMTargetInfo info = AssessTarget(target);
        float score = info.CalculateScore();

        if (score > 0.0f)
            scoredTargets.emplace_back(target, score);
    }

    if (scoredTargets.empty())
        return nullptr;

    // Sort by score (highest first)
    ::std::sort(scoredTargets.begin(), scoredTargets.end(),
        [](const auto& a, const auto& b) {
            return a.second > b.second;
        });

    return scoredTargets.front().first;
}

bool TargetManager::ShouldSwitchTarget(float switchThreshold)
{
    if (!_bot)
        return false;

    // Don't switch too frequently
    uint32 now = GameTime::GetGameTimeMS();
    if (now - _lastSwitchTime < MIN_SWITCH_INTERVAL)
        return false;

    Unit* currentTarget = GetCurrentTarget();
    if (!currentTarget || currentTarget->isDead())
        return true;

    Unit* bestTarget = GetPriorityTarget();
    if (!bestTarget)
        return false;

    // Same target = no switch
    if (currentTarget == bestTarget)
        return false;

    // Calculate scores
    TMTargetInfo currentInfo = AssessTarget(currentTarget);
    TMTargetInfo bestInfo = AssessTarget(bestTarget);

    float currentScore = currentInfo.CalculateScore();
    float bestScore = bestInfo.CalculateScore();

    // Switch if new target significantly better
    return (bestScore - currentScore) >= (currentScore * switchThreshold);
}

TMTargetPriority TargetManager::ClassifyTarget(Unit* target)
{
    if (!target || target->isDead())
        return TMTargetPriority::IGNORE;

    // Friendly = ignore
    if (target->IsFriendlyTo(_bot))
        return TMTargetPriority::IGNORE;

    // Crowd controlled = ignore
    if (IsCrowdControlled(target))
        return TMTargetPriority::IGNORE;

    // Immune = ignore
    if (IsImmune(target))
        return TMTargetPriority::IGNORE;

    // Healer = critical
    if (IsHealer(target))
        return TMTargetPriority::CRITICAL;

    // Execute range = critical
    if (target->GetHealthPct() < 20.0f)
        return TMTargetPriority::CRITICAL;

    // Caster = high
    if (IsCaster(target))
        return TMTargetPriority::HIGH;

    // High threat = high
    float threat = CalculateThreatLevel(target);
    if (threat > 0.7f)
        return TMTargetPriority::HIGH;

    // Elite/boss = high
    if (target->IsElite())
        return TMTargetPriority::HIGH;

    // Tank = low
    // TODO: Detect tank role properly

    // Default = medium
    return TMTargetPriority::MEDIUM;
}

bool TargetManager::IsHighPriorityTarget(Unit* target)
{
    TMTargetPriority priority = ClassifyTarget(target);
    return priority == TMTargetPriority::CRITICAL || priority == TMTargetPriority::HIGH;
}

::std::vector<Unit*> TargetManager::GetCombatTargets()
{
    ::std::vector<Unit*> targets;

    if (!_bot)
        return targets;

    // Get all enemies from threat list
    ThreatManager& threatMgr = _bot->GetThreatManager();
    ::std::list<HostileReference*> const& threatList = threatMgr.GetThreatList();

    for (HostileReference* ref : threatList)
    {
        if (!ref)
            continue;

        Unit* enemy = ref->GetOwner();
        if (enemy && !enemy->isDead() && _bot->IsValidAttackTarget(enemy))
            targets.push_back(enemy);
    }

    return targets;
}

TMTargetInfo TargetManager::AssessTarget(Unit* target)
{
    TMTargetInfo info;

    if (!target || !_bot)
        return info;

    info.target = target;
    info.priority = ClassifyTarget(target);
    info.healthPercent = target->GetHealthPct();
    info.distance = _bot->GetDistance(target);
    info.isCaster = IsCaster(target);
    info.isHealer = IsHealer(target);
    info.isCrowdControlled = IsCrowdControlled(target);
    info.isImmune = IsImmune(target);
    info.threatLevel = CalculateThreatLevel(target);
    info.damageDealt = 0.0f;  // TODO: Get from metrics
    info.timeSinceLastSwitch = 0;  // TODO: Track switch history

    return info;
}

void TargetManager::SetCurrentTarget(Unit* target)
{
    if (!target)
    {
        _currentTarget = ObjectGuid::Empty;
        return;
    }

    ObjectGuid newGuid = target->GetGUID();

    if (newGuid != _currentTarget)
    {
        _lastSwitchTime = GameTime::GetGameTimeMS();
        _currentTarget = newGuid;

        TC_LOG_DEBUG("playerbot", "TargetManager: Bot {} switched to target {}",
            _bot ? _bot->GetName() : "unknown",
            target->GetName());
    }
}

Unit* TargetManager::GetCurrentTarget() const
{
    if (_currentTarget.IsEmpty())
        return nullptr;

    if (!_bot)
        return nullptr;

    return ObjectAccessor::GetUnit(*_bot, _currentTarget);
}

// Private helper functions

bool TargetManager::IsHealer(Unit* target) const
{
    if (!target)
        return false;

    // Check if target is a healing class/spec
    if (target->IsPlayer())
    {
        Player* player = target->ToPlayer();
        switch (player->GetClass())
        {
            case CLASS_PRIEST:
            case CLASS_PALADIN:
            case CLASS_SHAMAN:
            case CLASS_DRUID:
            case CLASS_MONK:
                // TODO: Check spec for healer role
                return true;
            default:
                return false;
        }
    }

    // For NPCs, check for healing abilities
    // TODO: Implement NPC healer detection
    return false;
}

bool TargetManager::IsCaster(Unit* target) const
{
    if (!target)
        return false;

    // Check power type (mana users are likely casters)
    return target->GetPowerType() == POWER_MANA;
}

bool TargetManager::IsCrowdControlled(Unit* target) const
{
    if (!target)
        return false;

    // Check for CC auras
    return target->HasAuraType(SPELL_AURA_MOD_STUN) ||
           target->HasAuraType(SPELL_AURA_MOD_FEAR) ||
           target->HasAuraType(SPELL_AURA_MOD_CONFUSE) ||
           target->HasAuraType(SPELL_AURA_MOD_ROOT) ||
           target->HasAuraType(SPELL_AURA_TRANSFORM);
}

bool TargetManager::IsImmune(Unit* target) const
{
    if (!target)
        return false;

    // Check for damage immunity
    return target->HasAuraType(SPELL_AURA_SCHOOL_IMMUNITY) ||
           target->IsImmunedToDamage(SPELL_SCHOOL_MASK_ALL);
}

float TargetManager::CalculateThreatLevel(Unit* target) const
{
    if (!target || !_bot)
        return 0.0f;

    // Get threat from target's threat manager
    ThreatManager& threatMgr = target->GetThreatManager();

    // Get highest threat on target
    Unit* topThreat = target->GetVictim();
    if (!topThreat)
        return 0.0f;

    float maxThreat = threatMgr.GetThreat(topThreat);
    if (maxThreat <= 0.0f)
        return 0.0f;

    // Get bot's threat
    float botThreat = threatMgr.GetThreat(_bot);

    // Calculate relative threat (0.0-1.0)
    return ::std::min(botThreat / maxThreat, 1.0f);
}

float TargetManager::GetRecentDamage(Unit* target, const CombatMetrics& metrics) const
{
    // TODO: Implement damage tracking from combat metrics
    // This requires integration with combat metrics system
    return 0.0f;
}

void TargetManager::UpdateTargetCache(const CombatMetrics& metrics)
{
    ::std::vector<Unit*> targets = GetCombatTargets();

    // Clear stale entries
    ::std::unordered_set<ObjectGuid> activeGuids;
    for (Unit* target : targets)
    {
        if (target)
            activeGuids.insert(target->GetGUID());
    }

    for (auto it = _targetCache.begin(); it != _targetCache.end();)
    {
        if (activeGuids.find(it->first) == activeGuids.end())
            it = _targetCache.erase(it);
        else
            ++it;
    }

    // Update cache
    for (Unit* target : targets)
    {
        if (!target)
            continue;

        _targetCache[target->GetGUID()] = AssessTarget(target);
    }
}

} // namespace Playerbot
