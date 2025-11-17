/*
 * Copyright (C) 2025 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "AdaptiveBehaviorManager.h"
#include "Decision/DecisionFusionSystem.h"
#include "../Common/ActionScoringEngine.h"
#include "Player.h"
#include "Group.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellHistory.h"
#include "SpellAuras.h"
#include "SpellAuraEffects.h"
#include "Timer.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Log.h"
#include <algorithm>

namespace Playerbot
{

AdaptiveBehaviorManager::AdaptiveBehaviorManager(::Player* bot) :
    _bot(bot),
    _activeStrategies(STRATEGY_NONE),
    _activeProfile(nullptr),
    _lastProfileSwitch(0),
    _profileSwitchCount(0),
    _lastStrategyUpdate(0),
    _strategySwitchCount(0),
    _updateTimer(0),
    _lastUpdateTime(0),
    _totalUpdateTime(0),
    _updateCount(0),
    _compositionCacheTime(0),
    _roleCacheTime(0)
{
    InitializeDefaultProfiles();
    AssignRoles();
}

AdaptiveBehaviorManager::~AdaptiveBehaviorManager() = default;

void AdaptiveBehaviorManager::InitializeDefaultProfiles()
{
    CreateEmergencyTankProfile();
    CreateAOEProfile();
    CreateSurvivalProfile();
    CreateBurstProfile();
    CreateResourceConservationProfile();
}

void AdaptiveBehaviorManager::CreateEmergencyTankProfile()
{
    BehaviorProfile profile;
    profile.name = "EmergencyTank";
    profile.priority = BehaviorPriority::EMERGENCY;
    profile.strategyFlags = STRATEGY_EMERGENCY_TANK | STRATEGY_DEFENSIVE | STRATEGY_USE_COOLDOWNS;
    profile.minDuration = 5000;
    profile.maxDuration = 30000;
    profile.cooldown = 60000;

    profile.condition = [](const CombatMetrics& metrics, CombatSituation situation) {
        return situation == CombatSituation::TANK_DEAD ||
               (!metrics.tankAlive && (metrics.eliteCount > 0 || metrics.bossCount > 0));
    };

    profile.applyFunction = [this](::Player* bot, uint32 flags) {
        // Emergency tank activation logic
        TC_LOG_DEBUG("bot.playerbot", "Bot {} activating emergency tank mode", bot->GetName());
        ActivateStrategy(flags);
    };

    RegisterProfile(profile);
}

void AdaptiveBehaviorManager::CreateAOEProfile()
{
    BehaviorProfile profile;
    profile.name = "AOEMode";
    profile.priority = BehaviorPriority::HIGH;
    profile.strategyFlags = STRATEGY_AOE_FOCUS | STRATEGY_AGGRESSIVE;
    profile.minDuration = 3000;
    profile.maxDuration = 20000;
    profile.cooldown = 5000;

    profile.condition = [](const CombatMetrics& metrics, CombatSituation situation) {
        return situation == CombatSituation::AOE_HEAVY ||
               metrics.enemyCount >= 4 ||
               (metrics.enemyCount >= 3 && metrics.nearestEnemyDistance <= 8.0f);
    };

    profile.applyFunction = [this](::Player* bot, uint32 flags) {
        TC_LOG_DEBUG("bot.playerbot", "Bot {} activating AOE mode", bot->GetName());
        ActivateStrategy(flags);
    };

    RegisterProfile(profile);
}

void AdaptiveBehaviorManager::CreateSurvivalProfile()
{
    BehaviorProfile profile;
    profile.name = "Survival";
    profile.priority = BehaviorPriority::CRITICAL;
    profile.strategyFlags = STRATEGY_SURVIVAL | STRATEGY_DEFENSIVE | STRATEGY_USE_CONSUMABLES | STRATEGY_USE_COOLDOWNS;
    profile.minDuration = 5000;
    profile.maxDuration = 15000;
    profile.cooldown = 30000;

    profile.condition = [](const CombatMetrics& metrics, CombatSituation situation) {
        return situation == CombatSituation::DEFENSIVE ||
               situation == CombatSituation::WIPE_IMMINENT ||
               metrics.personalHealthPercent < 30.0f ||
               (metrics.averageGroupHealth < 40.0f && !metrics.healerAlive);
    };

    profile.applyFunction = [this](::Player* bot, uint32 flags) {
        TC_LOG_DEBUG("bot.playerbot", "Bot {} activating survival mode", bot->GetName());
        ActivateStrategy(flags);
    };

    RegisterProfile(profile);
}

void AdaptiveBehaviorManager::CreateBurstProfile()
{
    BehaviorProfile profile;
    profile.name = "BurstPhase";
    profile.priority = BehaviorPriority::HIGH;
    profile.strategyFlags = STRATEGY_BURST_DAMAGE | STRATEGY_AGGRESSIVE | STRATEGY_USE_COOLDOWNS | STRATEGY_USE_CONSUMABLES;
    profile.minDuration = 10000;
    profile.maxDuration = 30000;
    profile.cooldown = 120000;

    profile.condition = [](const CombatMetrics& metrics, CombatSituation situation) {
        return situation == CombatSituation::BURST_NEEDED ||
               (metrics.bossCount > 0 && metrics.enrageTimer > 0 && metrics.enrageTimer < 30000);
    };

    profile.applyFunction = [this](::Player* bot, uint32 flags) {
        TC_LOG_DEBUG("bot.playerbot", "Bot {} activating burst phase", bot->GetName());
        ActivateStrategy(flags);
    };

    RegisterProfile(profile);
}

void AdaptiveBehaviorManager::CreateResourceConservationProfile()
{
    BehaviorProfile profile;
    profile.name = "ResourceConservation";
    profile.priority = BehaviorPriority::NORMAL;
    profile.strategyFlags = STRATEGY_CONSERVE_MANA | STRATEGY_SAVE_COOLDOWNS;
    profile.minDuration = 10000;
    profile.maxDuration = 60000;
    profile.cooldown = 20000;

    profile.condition = [](const CombatMetrics& metrics, CombatSituation situation) {
        return metrics.manaPercent < 30.0f ||
               (metrics.combatDuration < 30000 && metrics.bossCount > 0); // Save resources early in boss fight
    };

    profile.applyFunction = [this](::Player* bot, uint32 flags) {
        TC_LOG_DEBUG("bot.playerbot", "Bot {} activating resource conservation", bot->GetName());
        ActivateStrategy(flags);
    };

    RegisterProfile(profile);
}

void AdaptiveBehaviorManager::Update(uint32 diff, const CombatMetrics& metrics, CombatSituation situation)
{
    uint32 startTime = GameTime::GetGameTimeMS();

    _updateTimer += diff;

    // Update every 200ms for responsiveness
    if (_updateTimer >= 200)
    {
        UpdateBehavior(metrics, situation);
        UpdateProfiles(diff, metrics, situation);
        AdaptToComposition();

        // Apply emergency strategies if needed
        ApplyEmergencyStrategies(metrics);

        // Apply positioning strategies based on situation
        ApplyPositioningStrategies(situation);

        // Apply resource management strategies
        ApplyResourceStrategies(metrics);

        _updateTimer = 0;
    }

    // Track performance
    _lastUpdateTime = GameTime::GetGameTimeMS() - startTime;
    _totalUpdateTime += _lastUpdateTime;
    _updateCount++;
}

void AdaptiveBehaviorManager::UpdateBehavior(const CombatMetrics& metrics, CombatSituation situation)
{
    // Clear expired strategies
    uint32 newStrategies = _activeStrategies;

    // Apply offensive or defensive strategies based on metrics
    if (metrics.averageGroupHealth > 70.0f && metrics.enemyCount > 0)
    {
        ApplyOffensiveStrategies(metrics);
    }
    else if (metrics.averageGroupHealth < 50.0f)
    {
        ApplyDefensiveStrategies(metrics);
    }

    // Track strategy changes
    if (newStrategies != _activeStrategies)
    {
        _strategySwitchCount++;
        _lastStrategyUpdate = GameTime::GetGameTimeMS();
    }
}

void AdaptiveBehaviorManager::UpdateProfiles(uint32 diff, const CombatMetrics& metrics, CombatSituation situation)
{
    BehaviorProfile* highestPriorityProfile = nullptr;
    BehaviorPriority highestPriority = BehaviorPriority::LOW;

    for (BehaviorProfile& profile : _profiles)
    {
        // Update active time
    if (profile.isActive)
        {
            profile.activeTime += diff;

            // Check if profile should deactivate (exceeded max duration)
    if (profile.activeTime >= profile.maxDuration)
            {
                RemoveProfile(profile);
                continue;
            }

            // Check if profile should remain active (minimum duration not met)
    if (profile.activeTime < profile.minDuration)
                continue;
        }

        // Check cooldown
    if (profile.lastActivated > 0 && profile.cooldown > 0)
        {
            if (GameTime::GetGameTimeMS() - profile.lastActivated < profile.cooldown)
                continue;
        }

        // Evaluate activation condition
        EvaluateProfileActivation(profile, metrics, situation);

        // Track highest priority profile
    if (profile.condition && profile.condition(metrics, situation))
        {
            if (profile.priority > highestPriority)
            {
                highestPriority = profile.priority;
                highestPriorityProfile = &profile;
            }
        }
    }

    // Switch to highest priority profile if different from current
    if (highestPriorityProfile && highestPriorityProfile != _activeProfile)
    {
        if (_activeProfile)
            RemoveProfile(*_activeProfile);

        ApplyProfile(*highestPriorityProfile);
        _activeProfile = highestPriorityProfile;
        _lastProfileSwitch = GameTime::GetGameTimeMS();
        _profileSwitchCount++;
    }
}

void AdaptiveBehaviorManager::EvaluateProfileActivation(BehaviorProfile& profile, const CombatMetrics& metrics, CombatSituation situation)
{
    if (!profile.condition)
        return;

    bool shouldActivate = profile.condition(metrics, situation);

    if (shouldActivate && !profile.isActive)
    {
        // Check cooldown
    if (profile.lastActivated > 0 && profile.cooldown > 0)
        {
            if (GameTime::GetGameTimeMS() - profile.lastActivated < profile.cooldown)
                return;
        }

        ApplyProfile(profile);
    }
    else if (!shouldActivate && profile.isActive && profile.activeTime >= profile.minDuration)
    {
        RemoveProfile(profile);
    }
}

void AdaptiveBehaviorManager::ApplyProfile(BehaviorProfile& profile)
{
    if (profile.applyFunction)
        profile.applyFunction(_bot, profile.strategyFlags);

    profile.isActive = true;
    profile.lastActivated = GameTime::GetGameTimeMS();
    profile.activeTime = 0;
    TC_LOG_DEBUG("bot.playerbot", "Bot {} activated behavior profile: {}", _bot->GetName(), profile.name);
}

void AdaptiveBehaviorManager::RemoveProfile(BehaviorProfile& profile)
{
    DeactivateStrategy(profile.strategyFlags);
    profile.isActive = false;
    profile.activeTime = 0;
    TC_LOG_DEBUG("bot.playerbot", "Bot {} deactivated behavior profile: {}", _bot->GetName(), profile.name);
}

void AdaptiveBehaviorManager::AdaptToComposition()
{
    // Update composition if cache is old
    if (GameTime::GetGameTimeMS() - _compositionCacheTime > 5000)
    {
        UpdateGroupComposition();
        _compositionCacheTime = GameTime::GetGameTimeMS();
    }

    // Adapt behavior based on composition
    if (!IsOptimalComposition())
    {
        // Missing tank - someone may need to emergency tank
    if (_groupComposition.tanks == 0 && CanPerformRole(BotRole::TANK))
        {
            ActivateStrategy(STRATEGY_EMERGENCY_TANK);
        }

        // Missing healer - activate self-preservation
    if (_groupComposition.healers == 0)
        {
            ActivateStrategy(STRATEGY_SURVIVAL | STRATEGY_USE_CONSUMABLES);
        }

        // Too many melee - some should stay ranged
    if (_groupComposition.meleeDPS > _groupComposition.rangedDPS + 2)
        {
            if (GetPrimaryRole() == BotRole::MELEE_DPS && CanPerformRole(BotRole::RANGED_DPS))
            {
                ActivateStrategy(STRATEGY_STAY_RANGED);
            }
        }
    }
}

void AdaptiveBehaviorManager::AssignRoles()
{
    BotRole primary = DetermineOptimalRole();
    BotRole secondary = DetermineSecondaryRole();

    _roleAssignment.primaryRole = primary;
    _roleAssignment.secondaryRole = secondary;
    _roleAssignment.roleEffectiveness = CalculateRoleScore(primary);
    _roleAssignment.rolePriority = GetRolePriority(primary);
    _roleAssignment.assignedTime = GameTime::GetGameTimeMS();
    _roleAssignment.isTemporary = false;

    TC_LOG_DEBUG("bot.playerbot", "Bot {} assigned roles - Primary: {}, Secondary: {}",
        _bot->GetName(), GetRoleName(primary), GetRoleName(secondary));
}

void AdaptiveBehaviorManager::ActivateStrategy(uint32 flags)
{
    uint32 oldStrategies = _activeStrategies;
    _activeStrategies |= flags;

    if (oldStrategies != _activeStrategies)
    {
        _strategySwitchCount++;

        // Track activation time for each strategy
    for (uint32 i = 0; i < 32; ++i)
        {
            uint32 flag = 1 << i;
            if ((flags & flag) && !(oldStrategies & flag))
            {
                _strategyActiveTimes[flag] = GameTime::GetGameTimeMS();
            }
        }
    }
}

void AdaptiveBehaviorManager::DeactivateStrategy(uint32 flags)
{
    uint32 oldStrategies = _activeStrategies;
    _activeStrategies &= ~flags;

    if (oldStrategies != _activeStrategies)
    {
        _strategySwitchCount++;
    }
}

void AdaptiveBehaviorManager::RegisterProfile(const BehaviorProfile& profile)
{
    _profiles.push_back(profile);
}

void AdaptiveBehaviorManager::ActivateProfile(const ::std::string& name)
{
    for (BehaviorProfile& profile : _profiles)
    {
        if (profile.name == name)
        {
            ApplyProfile(profile);
            return;
        }
    }
}

void AdaptiveBehaviorManager::DeactivateProfile(const ::std::string& name)
{
    for (BehaviorProfile& profile : _profiles)
    {
        if (profile.name == name && profile.isActive)
        {
            RemoveProfile(profile);
            return;
        }
    }
}

bool AdaptiveBehaviorManager::IsProfileActive(const ::std::string& name) const
{
    for (const BehaviorProfile& profile : _profiles)
    {
        if (profile.name == name)
            return profile.isActive;
    }
    return false;
}

const BehaviorProfile* AdaptiveBehaviorManager::GetActiveProfile() const
{
    return _activeProfile;
}

::std::vector<::std::string> AdaptiveBehaviorManager::GetActiveProfileNames() const
{
    ::std::vector<::std::string> names;
    for (const BehaviorProfile& profile : _profiles)
    {
        if (profile.isActive)
            names.push_back(profile.name);
    }
    return names;
}

bool AdaptiveBehaviorManager::CanPerformRole(BotRole role) const
{
    Classes botClass = GetBotClass();
    switch (role)
    {
        case BotRole::TANK:
            return botClass == CLASS_WARRIOR || botClass == CLASS_PALADIN ||
                   botClass == CLASS_DEATH_KNIGHT || botClass == CLASS_DRUID;

        case BotRole::HEALER:
            return botClass == CLASS_PRIEST || botClass == CLASS_DRUID ||
                   botClass == CLASS_SHAMAN || botClass == CLASS_PALADIN;
        case BotRole::MELEE_DPS:
            return botClass == CLASS_WARRIOR || botClass == CLASS_ROGUE ||
                   botClass == CLASS_DEATH_KNIGHT || botClass == CLASS_PALADIN ||
                   botClass == CLASS_SHAMAN || botClass == CLASS_DRUID;

        case BotRole::RANGED_DPS:
            return botClass == CLASS_HUNTER || botClass == CLASS_MAGE ||
                   botClass == CLASS_WARLOCK || botClass == CLASS_PRIEST ||
                   botClass == CLASS_SHAMAN || botClass == CLASS_DRUID;

        case BotRole::CROWD_CONTROL:
            return botClass == CLASS_MAGE || botClass == CLASS_ROGUE ||
                   botClass == CLASS_HUNTER || botClass == CLASS_WARLOCK;
        default:
            return true;
    }
}

float AdaptiveBehaviorManager::GetRoleEffectiveness(BotRole role) const
{
    return CalculateRoleScore(role);
}

void AdaptiveBehaviorManager::ForceRole(BotRole role, bool temporary)
{
    _roleAssignment.primaryRole = role;
    _roleAssignment.isTemporary = temporary;
    _roleAssignment.assignedTime = GameTime::GetGameTimeMS();

    TC_LOG_DEBUG("bot.playerbot", "Bot {} forced to role: {} (temporary: {})",
        _bot->GetName(), GetRoleName(role), temporary);
}

void AdaptiveBehaviorManager::UpdateGroupComposition()
{
    _groupComposition.Reset();

    ::Group* group = _bot->GetGroup();
    if (!group)
    {
        _groupComposition.totalMembers = 1;
        _groupComposition.alive = _bot->IsAlive() ? 1 : 0;
        return;
    }

    for (GroupReference const& groupRef : group->GetMembers())
    {
        ::Player* member = groupRef.GetSource();
        if (!member)
            continue;

        _groupComposition.totalMembers++;

        if (member->IsAlive())
            _groupComposition.alive++;
        else
            _groupComposition.dead++;

        // Classify member role based on class/spec
        Classes memberClass = static_cast<Classes>(member->GetClass());
        switch (memberClass)
        {
            case CLASS_WARRIOR:
                _groupComposition.tanks++;
                break;

            case CLASS_DEATH_KNIGHT:
                _groupComposition.tanks++;
                break;

            case CLASS_PALADIN:
                // Paladin can be tank, healer, or DPS
                _groupComposition.tanks++;
                _groupComposition.healers++;
                break;

            case CLASS_PRIEST:
                _groupComposition.healers++;
                break;

            case CLASS_DRUID:
                // Druid is hybrid - count as both tank and healer potential
                _groupComposition.tanks++;
                _groupComposition.healers++;
                break;

            case CLASS_SHAMAN:
                _groupComposition.healers++;
                _groupComposition.meleeDPS++;
                break;

            case CLASS_ROGUE:
                _groupComposition.meleeDPS++;
                break;

            case CLASS_HUNTER:
                _groupComposition.rangedDPS++;
                break;

            case CLASS_MAGE:
                _groupComposition.rangedDPS++;
                break;

            case CLASS_WARLOCK:
                _groupComposition.rangedDPS++;
                break;

            default:
                break;
        }

        // Check for special abilities
    if (memberClass == CLASS_SHAMAN || memberClass == CLASS_MAGE)
            _groupComposition.hasBloodlust = true;

        if (memberClass == CLASS_DRUID)
            _groupComposition.hasBattleRes = true;
    }
}

bool AdaptiveBehaviorManager::IsOptimalComposition() const
{
    // Optimal composition has at least 1 tank, 1 healer, and DPS
    return _groupComposition.tanks >= 1 &&
           _groupComposition.healers >= 1 &&
           (_groupComposition.meleeDPS + _groupComposition.rangedDPS) >= 1;
}

bool AdaptiveBehaviorManager::NeedsRoleSwitch() const
{
    // Check if role switch would benefit group
    if (!IsOptimalComposition())
    {
        if (_groupComposition.tanks == 0 && CanPerformRole(BotRole::TANK))
            return true;

        if (_groupComposition.healers == 0 && CanPerformRole(BotRole::HEALER))
            return true;
    }

    return false;
}

bool AdaptiveBehaviorManager::ShouldEmergencyTank() const
{
    return IsStrategyActive(STRATEGY_EMERGENCY_TANK) ||
           (!_groupComposition.tanks && CanPerformRole(BotRole::TANK));
}

bool AdaptiveBehaviorManager::ShouldEmergencyHeal() const
{
    return IsStrategyActive(STRATEGY_EMERGENCY_HEAL) ||
           (!_groupComposition.healers && CanPerformRole(BotRole::HEALER));
}

bool AdaptiveBehaviorManager::ShouldUseDefensiveCooldowns() const
{
    return IsStrategyActive(STRATEGY_DEFENSIVE) ||
           IsStrategyActive(STRATEGY_SURVIVAL);
}

bool AdaptiveBehaviorManager::ShouldUseOffensiveCooldowns() const
{
    return IsStrategyActive(STRATEGY_BURST_DAMAGE) ||
           (IsStrategyActive(STRATEGY_USE_COOLDOWNS) && !IsStrategyActive(STRATEGY_SAVE_COOLDOWNS));
}

bool AdaptiveBehaviorManager::ShouldSaveResources() const
{
    return IsStrategyActive(STRATEGY_CONSERVE_MANA) ||
           IsStrategyActive(STRATEGY_SAVE_COOLDOWNS);
}

uint32 AdaptiveBehaviorManager::GetAverageUpdateTime() const
{
    if (_updateCount == 0)
        return 0;
    return _totalUpdateTime / _updateCount;
}

void AdaptiveBehaviorManager::RecordDecisionOutcome(const ::std::string& decision, bool success)
{
    DecisionOutcome& outcome = _decisionHistory[decision];
    if (success)
        outcome.successCount++;
    else
        outcome.failureCount++;

    uint32 total = outcome.successCount + outcome.failureCount;
    if (total > 0)
        outcome.successRate = static_cast<float>(outcome.successCount) / total * 100.0f;
}

float AdaptiveBehaviorManager::GetDecisionSuccessRate(const ::std::string& decision) const
{
    auto it = _decisionHistory.find(decision);
    if (it != _decisionHistory.end())
        return it->second.successRate;
    return 0.0f;
}

void AdaptiveBehaviorManager::AdjustBehaviorWeights()
{
    // Analyze decision history and adjust profile priorities
    for (BehaviorProfile& profile : _profiles)
    {
        float successRate = GetDecisionSuccessRate(profile.name);

        // Adjust priority based on success rate
    if (successRate > 80.0f && profile.priority < BehaviorPriority::CRITICAL)
        {
            profile.priority = static_cast<BehaviorPriority>(static_cast<uint8>(profile.priority) + 1);
        }
        else if (successRate < 40.0f && profile.priority > BehaviorPriority::LOW)
        {
            profile.priority = static_cast<BehaviorPriority>(static_cast<uint8>(profile.priority) - 1);
        }
    }
}

void AdaptiveBehaviorManager::UpdateRoleAssignment()
{
    if (GameTime::GetGameTimeMS() - _roleAssignment.assignedTime < 30000)
        return; // Don't switch roles too frequently
    if (NeedsRoleSwitch())
    {
        AssignRoles();
    }
}

BotRole AdaptiveBehaviorManager::DetermineOptimalRole() const
{
    Classes botClass = GetBotClass();

    // Check group needs first
    if (_groupComposition.tanks == 0 && CanPerformRole(BotRole::TANK))
        return BotRole::TANK;

    if (_groupComposition.healers == 0 && CanPerformRole(BotRole::HEALER))
        return BotRole::HEALER;

    // Default role based on class
    switch (botClass)
    {
        case CLASS_WARRIOR:
            return HasTankSpec() ? BotRole::TANK : BotRole::MELEE_DPS;
        case CLASS_PALADIN:
            if (HasTankSpec()) return BotRole::TANK;
            if (HasHealSpec()) return BotRole::HEALER;
            return BotRole::MELEE_DPS;
        case CLASS_PRIEST:
            return BotRole::HEALER;
        case CLASS_ROGUE:
            return BotRole::MELEE_DPS;
        case CLASS_HUNTER:
            return BotRole::RANGED_DPS;
        case CLASS_SHAMAN:
            return HasHealSpec() ? BotRole::HEALER : BotRole::MELEE_DPS;
        case CLASS_MAGE:
        case CLASS_WARLOCK:
            return BotRole::RANGED_DPS;
        case CLASS_DRUID:
            if (HasTankSpec()) return BotRole::TANK;
            if (HasHealSpec()) return BotRole::HEALER;
            return BotRole::MELEE_DPS;
        case CLASS_DEATH_KNIGHT:
            return HasTankSpec() ? BotRole::TANK : BotRole::MELEE_DPS;
        default:
            return BotRole::MELEE_DPS;
    }
}

BotRole AdaptiveBehaviorManager::DetermineSecondaryRole() const
{
    BotRole primary = _roleAssignment.primaryRole;
    Classes botClass = GetBotClass();

    // Determine secondary role based on class flexibility
    switch (botClass)
    {
        case CLASS_PALADIN:
            if (primary == BotRole::TANK) return BotRole::OFF_HEALER;
            if (primary == BotRole::HEALER) return BotRole::OFF_TANK;
            return BotRole::SUPPORT;

        case CLASS_DRUID:
            if (primary == BotRole::TANK) return BotRole::OFF_HEALER;
            if (primary == BotRole::HEALER) return BotRole::RANGED_DPS;
            return BotRole::HYBRID;

        case CLASS_SHAMAN:
            if (primary == BotRole::HEALER) return BotRole::RANGED_DPS;
            return BotRole::OFF_HEALER;

        case CLASS_PRIEST:
            if (primary == BotRole::HEALER) return BotRole::RANGED_DPS;
            return BotRole::SUPPORT;

        case CLASS_WARRIOR:
            if (primary == BotRole::TANK) return BotRole::MELEE_DPS;
            return BotRole::OFF_TANK;

        case CLASS_DEATH_KNIGHT:
            if (primary == BotRole::TANK) return BotRole::MELEE_DPS;
            return BotRole::OFF_TANK;

        default:
            return BotRole::NONE;
    }
}

float AdaptiveBehaviorManager::CalculateRoleScore(BotRole role) const
{
    float score = 50.0f; // Base score

    Classes botClass = GetBotClass();

    // Class-specific bonuses
    switch (role)
    {
        case BotRole::TANK:
            if (botClass == CLASS_WARRIOR || botClass == CLASS_PALADIN ||
                botClass == CLASS_DEATH_KNIGHT)
                score += 30.0f;
            if (HasTankSpec())
                score += 20.0f;
            break;

        case BotRole::HEALER:
            if (botClass == CLASS_PRIEST || botClass == CLASS_DRUID ||
                botClass == CLASS_SHAMAN || botClass == CLASS_PALADIN)
                score += 30.0f;
            if (HasHealSpec())
                score += 20.0f;
            break;

        case BotRole::MELEE_DPS:
            if (botClass == CLASS_ROGUE || botClass == CLASS_WARRIOR ||
                botClass == CLASS_DEATH_KNIGHT)
                score += 30.0f;
            break;

        case BotRole::RANGED_DPS:
            if (botClass == CLASS_HUNTER || botClass == CLASS_MAGE ||
                botClass == CLASS_WARLOCK)
                score += 30.0f;
            break;

        default:
            break;
    }

    // Gear score bonus (simplified)
    score += GetGearScore() / 100.0f;

    return ::std::min(100.0f, score);
}

bool AdaptiveBehaviorManager::IsRoleNeeded(BotRole role) const
{
    switch (role)
    {
        case BotRole::TANK:
            return _groupComposition.tanks == 0;
        case BotRole::HEALER:
            return _groupComposition.healers == 0;
        case BotRole::MELEE_DPS:
            return _groupComposition.meleeDPS < 3;
        case BotRole::RANGED_DPS:
            return _groupComposition.rangedDPS < 3;
        default:
            return false;
    }
}

uint32 AdaptiveBehaviorManager::GetRolePriority(BotRole role) const
{
    // Priority: Tank > Healer > DPS
    switch (role)
    {
        case BotRole::TANK: return 1;
        case BotRole::HEALER: return 2;
        case BotRole::OFF_TANK: return 3;
        case BotRole::OFF_HEALER: return 4;
        case BotRole::MELEE_DPS:
        case BotRole::RANGED_DPS: return 5;
        default: return 999;
    }
}

void AdaptiveBehaviorManager::ApplyEmergencyStrategies(const CombatMetrics& metrics)
{
    // Emergency tank if needed
    if (metrics.tankAlive == false && CanPerformRole(BotRole::TANK))
    {
        ActivateStrategy(STRATEGY_EMERGENCY_TANK | STRATEGY_DEFENSIVE);
    }

    // Emergency healing if needed
    if (metrics.averageGroupHealth < 40.0f && CanPerformRole(BotRole::HEALER))
    {
        ActivateStrategy(STRATEGY_EMERGENCY_HEAL);
    }

    // Survival mode if health critical
    if (metrics.personalHealthPercent < 30.0f)
    {
        ActivateStrategy(STRATEGY_SURVIVAL | STRATEGY_USE_COOLDOWNS | STRATEGY_USE_CONSUMABLES);
    }
}

void AdaptiveBehaviorManager::ApplyOffensiveStrategies(const CombatMetrics& metrics)
{
    // Burst if boss is low or enraging
    if (metrics.bossCount > 0 && (metrics.enrageTimer < 30000 || metrics.lowestGroupHealth > 70.0f))
    {
        ActivateStrategy(STRATEGY_BURST_DAMAGE | STRATEGY_USE_COOLDOWNS);
    }

    // AOE if many enemies
    if (metrics.enemyCount >= 3)
    {
        ActivateStrategy(STRATEGY_AOE_FOCUS);
    }
    else
    {
        ActivateStrategy(STRATEGY_SINGLE_TARGET);
    }

    // Aggressive if safe
    if (metrics.averageGroupHealth > 80.0f && metrics.tankAlive && metrics.healerAlive)
    {
        ActivateStrategy(STRATEGY_AGGRESSIVE);
    }
}

void AdaptiveBehaviorManager::ApplyDefensiveStrategies(const CombatMetrics& metrics)
{
    // Defensive if taking heavy damage
    if (metrics.personalHealthPercent < 50.0f || metrics.incomingDPS > 0)
    {
        ActivateStrategy(STRATEGY_DEFENSIVE);
    }

    // Save cooldowns if early in fight
    if (metrics.combatDuration < 30000 && metrics.bossCount > 0)
    {
        ActivateStrategy(STRATEGY_SAVE_COOLDOWNS);
    }

    // Conserve mana if low
    if (metrics.manaPercent < 40.0f)
    {
        ActivateStrategy(STRATEGY_CONSERVE_MANA);
    }
}

void AdaptiveBehaviorManager::ApplyPositioningStrategies(CombatSituation situation)
{
    switch (situation)
    {
        case CombatSituation::SPREAD:
            ActivateStrategy(STRATEGY_SPREAD | STRATEGY_MOBILITY);
            DeactivateStrategy(STRATEGY_STACK);
            break;

        case CombatSituation::STACK:
            ActivateStrategy(STRATEGY_STACK);
            DeactivateStrategy(STRATEGY_SPREAD);
            break;
        case CombatSituation::KITE:
            ActivateStrategy(STRATEGY_KITE | STRATEGY_MOBILITY | STRATEGY_STAY_RANGED);
            DeactivateStrategy(STRATEGY_STAY_MELEE);
            break;

        default:
            // Normal positioning based on role
    if (IsDPSRole(GetPrimaryRole()))
            {
                if (GetPrimaryRole() == BotRole::MELEE_DPS)
                    ActivateStrategy(STRATEGY_STAY_MELEE);
                else
                    ActivateStrategy(STRATEGY_STAY_RANGED);
            }
            break;
    }
}

void AdaptiveBehaviorManager::ApplyResourceStrategies(const CombatMetrics& metrics)
{
    // Use consumables in critical situations
    if (metrics.personalHealthPercent < 40.0f ||
        (metrics.bossCount > 0 && metrics.enrageTimer < 60000))
    {
        ActivateStrategy(STRATEGY_USE_CONSUMABLES);
    }

    // Manage cooldowns based on combat phase
    if (metrics.bossCount > 0)
    {
        if (metrics.combatDuration < 30000)
        {
            // Early in fight, save cooldowns
            ActivateStrategy(STRATEGY_SAVE_COOLDOWNS);
            DeactivateStrategy(STRATEGY_USE_COOLDOWNS);
        }
        else if (metrics.enrageTimer < 120000)
        {
            // Use cooldowns when needed
            ActivateStrategy(STRATEGY_USE_COOLDOWNS);
            DeactivateStrategy(STRATEGY_SAVE_COOLDOWNS);
        }
    }
    else
    {
        // Normal enemies, use cooldowns freely
    if (metrics.enemyCount >= 3 || metrics.eliteCount > 0)
        {
            ActivateStrategy(STRATEGY_USE_COOLDOWNS);
        }
    }
}

Classes AdaptiveBehaviorManager::GetBotClass() const
{
    return static_cast<Classes>(_bot->GetClass());
}

uint32 AdaptiveBehaviorManager::GetBotSpec() const
{
    // Simplified spec detection - would need talent inspection in production
    return 0;
}

bool AdaptiveBehaviorManager::HasTankSpec() const
{
    // Simplified - would check actual spec/talents
    Classes botClass = GetBotClass();
    return botClass == CLASS_WARRIOR || botClass == CLASS_PALADIN ||
           botClass == CLASS_DEATH_KNIGHT || botClass == CLASS_DRUID;
}

bool AdaptiveBehaviorManager::HasHealSpec() const
{
    // Simplified - would check actual spec/talents
    Classes botClass = GetBotClass();
    return botClass == CLASS_PRIEST || botClass == CLASS_PALADIN ||
           botClass == CLASS_DRUID || botClass == CLASS_SHAMAN;
}

bool AdaptiveBehaviorManager::HasCrowdControl() const
{
    Classes botClass = GetBotClass();
    return botClass == CLASS_MAGE || botClass == CLASS_ROGUE ||
           botClass == CLASS_HUNTER || botClass == CLASS_WARLOCK ||
           botClass == CLASS_DRUID || botClass == CLASS_SHAMAN;
}

float AdaptiveBehaviorManager::GetGearScore() const
{
    // Simplified gear score calculation
    // In production would calculate from actual equipped items
    return 3000.0f; // Placeholder
}

void AdaptiveBehaviorManager::Reset()
{
    _activeStrategies = STRATEGY_NONE;
    _activeProfile = nullptr;
    _profileSwitchCount = 0;
    _strategySwitchCount = 0;
    _strategyActiveTimes.clear();
    _decisionHistory.clear();
    _updateTimer = 0;

    for (BehaviorProfile& profile : _profiles)
    {
        profile.isActive = false;
        profile.activeTime = 0;
        profile.lastActivated = 0;
    }

    AssignRoles();
}

void AdaptiveBehaviorManager::ClearProfiles()
{
    _profiles.clear();
    _activeProfile = nullptr;
}

void AdaptiveBehaviorManager::ResetStrategies()
{
    _activeStrategies = STRATEGY_NONE;
    _strategyActiveTimes.clear();
}

// ============================================================================
// DECISION FUSION INTEGRATION
// ============================================================================

::bot::ai::DecisionVote AdaptiveBehaviorManager::GetRecommendedAction(Unit* target, ::bot::ai::CombatContext context) const
{
    // TODO: DecisionVote and DecisionSource not fully defined (only forward-declared)
    // TODO: Implement when DecisionFusionSystem.h provides full definitions
    (void)target; (void)context; // Suppress unused warnings
    return ::bot::ai::DecisionVote{}; // Return default-constructed vote

    /* COMMENTED OUT until DecisionVote is fully defined:
    using namespace ::bot::ai;
    using ::Playerbot::BotRole;  // Disambiguate from bot::ai::BotRole

    DecisionVote vote;
    vote.source = DecisionSource::ADAPTIVE_BEHAVIOR;
    vote.target = target;

    // No recommendation if bot is invalid
    if (!this->_bot)
        return vote;

    // ========================================================================
    // 1. DETERMINE ACTION PRIORITY BASED ON ROLE
    // ========================================================================

    // Emergency healing/tanking takes absolute priority
    if (this->ShouldEmergencyHeal())
    {
        vote.actionId = 0; // TODO: Get emergency heal spell from ClassAI
        vote.confidence = 1.0f; // Maximum confidence for emergency
        vote.urgency = 1.0f;    // Maximum urgency
        vote.reasoning = "AdaptiveBehavior: Emergency healing required";
        return vote;
    }

    if (this->ShouldEmergencyTank())
    {
        vote.actionId = 0; // TODO: Get emergency defensive from ClassAI
        vote.confidence = 1.0f;
        vote.urgency = 1.0f;
        vote.reasoning = "AdaptiveBehavior: Emergency tanking required";
        return vote;
    }

    // ========================================================================
    // 2. ROLE-BASED ACTION RECOMMENDATION
    // ========================================================================

    BotRole primaryRole = this->GetPrimaryRole();
    float roleEffectiveness = this->_roleAssignment.roleEffectiveness / 100.0f; // Normalize to 0-1

    // Base confidence on role effectiveness
    vote.confidence = ::std::min(::std::max(roleEffectiveness, 0.3f), 0.8f); // Clamp 0.3-0.8

    // Base urgency on context and active strategies
    vote.urgency = 0.5f; // Default medium urgency

    // ========================================================================
    // 3. CONTEXT-BASED URGENCY ADJUSTMENTS
    // ========================================================================
    // COMMENTED OUT: CombatContext enum values not defined (only forward-declared)
    // TODO: Define CombatContext enum with RAID_NORMAL, RAID_HEROIC, etc.
    /*
    switch (context)
    {
        case CombatContext::RAID_NORMAL:
        case CombatContext::RAID_HEROIC:
            vote.urgency += 0.2f; // Higher urgency in raid content
            break;
        case CombatContext::DUNGEON_BOSS:
            vote.urgency += 0.15f;
            break;
        case CombatContext::PVP_ARENA:
            vote.urgency += 0.25f; // Highest urgency in arena
            break;
        case CombatContext::PVP_BG:
            vote.urgency += 0.15f;
            break;
        default:
            break;
    }
    */

    // ========================================================================
    // 4. STRATEGY-BASED URGENCY ADJUSTMENTS
    // ========================================================================
    if (this->IsStrategyActive(STRATEGY_BURST_DAMAGE))
        vote.urgency += 0.2f;  // Increase urgency during burst windows
    if (this->IsStrategyActive(STRATEGY_SURVIVAL))
        vote.urgency += 0.3f;  // High urgency in survival mode
    if (this->IsStrategyActive(STRATEGY_EMERGENCY_TANK) || this->IsStrategyActive(STRATEGY_EMERGENCY_HEAL))
        vote.urgency += 0.4f;  // Very high urgency for emergency strategies
    if (this->IsStrategyActive(STRATEGY_SAVE_COOLDOWNS))
        vote.urgency -= 0.1f;  // Reduce urgency when saving cooldowns

    // ========================================================================
    // 5. ROLE-SPECIFIC ACTION REASONING
    // ========================================================================

    ::std::string reasoning = "AdaptiveBehavior: ";

    switch (primaryRole)
    {
        case BotRole::TANK:
        case BotRole::OFF_TANK:
            reasoning += "Tank role - ";
            if (this->IsStrategyActive(STRATEGY_DEFENSIVE))
                reasoning += "Defensive strategy";
            else if (this->IsStrategyActive(STRATEGY_AOE_FOCUS))
                reasoning += "AoE threat generation";
            else
                reasoning += "Threat maintenance";

            // Tanks have higher urgency if not tanking current target
    if (target && target->GetVictim() != this->_bot)
                vote.urgency += 0.2f;
            break;

        case BotRole::HEALER:
        case BotRole::OFF_HEALER:
            reasoning += "Healer role - ";
            if (this->IsStrategyActive(STRATEGY_EMERGENCY_HEAL))
                reasoning += "Emergency healing";
            else if (this->_groupComposition.averageItemLevel > 0)
            {
                float healthPercentage = static_cast<float>(this->_groupComposition.alive) /
                                        static_cast<float>(this->_groupComposition.totalMembers) * 100.0f;
                if (healthPercentage < 60.0f)
                {
                    reasoning += "Group health critical";
                    vote.urgency += 0.3f;
                }
                else if (healthPercentage < 80.0f)
                {
                    reasoning += "Group health low";
                    vote.urgency += 0.15f;
                }
                else
                    reasoning += "Maintenance healing";
            }
            else
                reasoning += "Group healing";
            break;

        case BotRole::MELEE_DPS:
            reasoning += "Melee DPS - ";
            if (this->IsStrategyActive(STRATEGY_AOE_FOCUS))
                reasoning += "AoE damage";
            else if (this->IsStrategyActive(STRATEGY_BURST_DAMAGE))
                reasoning += "Burst window";
            else if (target && target->GetHealthPct() < 20.0f)
            {
                reasoning += "Execute range";
                vote.urgency += 0.2f;
            }
            else
                reasoning += "Single target rotation";
            break;

        case BotRole::RANGED_DPS:
            reasoning += "Ranged DPS - ";
            if (this->IsStrategyActive(STRATEGY_AOE_FOCUS))
                reasoning += "AoE damage";
            else if (this->IsStrategyActive(STRATEGY_BURST_DAMAGE))
                reasoning += "Burst window";
            else if (target && target->GetHealthPct() < 20.0f)
            {
                reasoning += "Execute range";
                vote.urgency += 0.2f;
            }
            else
                reasoning += "Single target rotation";
            break;

        case BotRole::CROWD_CONTROL:
            reasoning += "Crowd Control - ";
            if (this->IsStrategyActive(STRATEGY_CROWD_CONTROL))
            {
                reasoning += "CC priority";
                vote.urgency += 0.25f;
            }
            else
                reasoning += "DPS rotation";
            break;

        case BotRole::SUPPORT:
            reasoning += "Support role - ";
            if (this->IsStrategyActive(STRATEGY_INTERRUPT_FOCUS))
                reasoning += "Interrupt focus";
            else
                reasoning += "Utility support";
            break;

        case BotRole::HYBRID:
            reasoning += "Hybrid role - ";
            // Hybrid adapts based on needs
    if (this->ShouldUseDefensiveCooldowns())
                reasoning += "Defensive support";
            else if (this->_groupComposition.healers == 0)
                reasoning += "Off-healing";
            else
                reasoning += "DPS";
            break;

        default:
            reasoning += "No specific role";
            vote.confidence = 0.2f; // Low confidence without role
            break;
    }

    // Add active profile information if any
    if (this->_activeProfile)
        reasoning += " (" + this->_activeProfile->name + ")";

    // Clamp urgency to 0-1 range
    vote.urgency = std::min(std::max(vote.urgency, 0.0f), 1.0f);

    vote.reasoning = reasoning;

    // ========================================================================
    // 6. RETURN VOTE
    // ========================================================================

    // Note: actionId remains 0 as placeholder
    // In production, this would query ClassAI for role-appropriate spell
    // For now, DecisionFusion will use this vote for weighting other systems

    return vote;
    */ // END COMMENTED OUT SECTION
}

} // namespace Playerbot