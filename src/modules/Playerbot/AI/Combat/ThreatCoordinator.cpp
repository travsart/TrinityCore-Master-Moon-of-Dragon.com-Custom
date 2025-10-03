/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "ThreatCoordinator.h"
#include "BotThreatManager.h"
#include "ThreatAbilities.h"
#include "InterruptCoordinator.h"
#include "PositionManager.h"
#include "BotAI.h"
#include "Player.h"
#include "Unit.h"
#include "Group.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "SpellHistory.h"
#include "ObjectAccessor.h"
#include "Log.h"
#include <algorithm>
#include <sstream>

namespace Playerbot
{

ThreatCoordinator::ThreatCoordinator(Group* group)
    : _group(group)
{
    _lastUpdate = std::chrono::steady_clock::now();
    _lastEmergencyCheck = std::chrono::steady_clock::now();

    TC_LOG_DEBUG("playerbots", "ThreatCoordinator: Initialized for group");
}

void ThreatCoordinator::Update(uint32 diff)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    // Check if it's time for standard update
    auto now = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastUpdate).count();

    if (elapsedMs >= _updateInterval)
    {
        _lastUpdate = now;

        // Core update cycle
        UpdateGroupThreatStatus();
        UpdateBotAssignments();
        ProcessThreatResponses();
        UpdateStabilityMetrics();

        _metrics.threatUpdates++;
    }

    // Emergency checks run more frequently
    auto emergencyElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastEmergencyCheck).count();
    if (_emergencyResponseEnabled && emergencyElapsed >= _emergencyCheckInterval)
    {
        _lastEmergencyCheck = now;
        CheckEmergencySituations();
    }

    // Execute queued responses
    ExecuteQueuedResponses();

    // Cleanup
    CleanupExpiredResponses();

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    TrackPerformance(duration, "Update");
}

void ThreatCoordinator::RegisterBot(Player* bot, BotAI* ai)
{
    if (!bot || !ai)
        return;

    std::lock_guard<std::mutex> lock(_coordinatorMutex);

    ObjectGuid botGuid = bot->GetGUID();

    // Create threat manager for the bot
    _botThreatManagers[botGuid] = std::make_unique<BotThreatManager>(bot);

    // Store AI reference
    _botAIs[botGuid] = ai;

    // Initialize assignment
    BotThreatAssignment assignment;
    assignment.botGuid = botGuid;
    assignment.assignedRole = DetermineRole(bot);
    assignment.targetThreatPercent = CalculateOptimalThreatPercent(botGuid, assignment.assignedRole);

    // Load available threat abilities for this bot
    auto& db = ThreatAbilitiesDB::Instance();
    auto abilities = db.GetClassAbilities(static_cast<Classes>(bot->GetClass()));
    for (const auto& ability : abilities)
    {
        if (bot->HasSpell(ability.spellId))
        {
            assignment.availableAbilities.push_back(ability.spellId);
        }
    }

    _botAssignments[botGuid] = assignment;

    // Auto-assign tanks
    if (assignment.assignedRole == ThreatRole::TANK)
    {
        if (_primaryTank.IsEmpty())
        {
            AssignPrimaryTank(botGuid);
        }
        else if (_offTank.IsEmpty())
        {
            AssignOffTank(botGuid);
        }
        else
        {
            _backupTanks.push_back(botGuid);
        }
    }

    TC_LOG_DEBUG("playerbots", "ThreatCoordinator: Registered bot {} with role {}",
                bot->GetName(), static_cast<uint32>(assignment.assignedRole));
}

void ThreatCoordinator::UnregisterBot(ObjectGuid botGuid)
{
    std::lock_guard<std::mutex> lock(_coordinatorMutex);

    _botThreatManagers.erase(botGuid);
    _botAssignments.erase(botGuid);
    _botAIs.erase(botGuid);

    // Update tank assignments if needed
    if (botGuid == _primaryTank)
    {
        _primaryTank.Clear();
        if (!_offTank.IsEmpty())
        {
            _primaryTank = _offTank;
            _offTank.Clear();
        }
        else if (!_backupTanks.empty())
        {
            _primaryTank = _backupTanks.front();
            _backupTanks.erase(_backupTanks.begin());
        }
    }
    else if (botGuid == _offTank)
    {
        _offTank.Clear();
        if (!_backupTanks.empty())
        {
            _offTank = _backupTanks.front();
            _backupTanks.erase(_backupTanks.begin());
        }
    }
    else
    {
        auto it = std::find(_backupTanks.begin(), _backupTanks.end(), botGuid);
        if (it != _backupTanks.end())
            _backupTanks.erase(it);
    }

    TC_LOG_DEBUG("playerbots", "ThreatCoordinator: Unregistered bot {}", botGuid.ToString());
}

void ThreatCoordinator::UpdateBotRole(ObjectGuid botGuid, ThreatRole role)
{
    std::lock_guard<std::mutex> lock(_coordinatorMutex);

    auto it = _botAssignments.find(botGuid);
    if (it != _botAssignments.end())
    {
        it->second.assignedRole = role;
        it->second.targetThreatPercent = CalculateOptimalThreatPercent(botGuid, role);

        TC_LOG_DEBUG("playerbots", "ThreatCoordinator: Updated bot {} role to {}",
                    botGuid.ToString(), static_cast<uint32>(role));
    }
}

bool ThreatCoordinator::AssignPrimaryTank(ObjectGuid botGuid)
{
    std::lock_guard<std::mutex> lock(_coordinatorMutex);

    auto it = _botAssignments.find(botGuid);
    if (it == _botAssignments.end())
        return false;

    _primaryTank = botGuid;
    it->second.assignedRole = ThreatRole::TANK;
    it->second.targetThreatPercent = _tankThreatThreshold;
    it->second.useAbilities = true;

    TC_LOG_INFO("playerbots", "ThreatCoordinator: Assigned {} as primary tank", botGuid.ToString());
    return true;
}

bool ThreatCoordinator::AssignOffTank(ObjectGuid botGuid)
{
    std::lock_guard<std::mutex> lock(_coordinatorMutex);

    auto it = _botAssignments.find(botGuid);
    if (it == _botAssignments.end())
        return false;

    _offTank = botGuid;
    it->second.assignedRole = ThreatRole::TANK;
    it->second.targetThreatPercent = _tankThreatThreshold * 0.8f; // Slightly lower than main tank
    it->second.useAbilities = true;

    TC_LOG_INFO("playerbots", "ThreatCoordinator: Assigned {} as off-tank", botGuid.ToString());
    return true;
}

void ThreatCoordinator::InitiateTankSwap(ObjectGuid fromTank, ObjectGuid toTank)
{
    std::lock_guard<std::mutex> lock(_coordinatorMutex);

    // Queue taunt from new tank
    for (const auto& targetGuid : _groupStatus.activeTargets)
    {
        Unit* target = ObjectAccessor::GetUnit(*ObjectAccessor::FindPlayer(toTank), targetGuid);
        if (!target)
            continue;

        ThreatResponseAction action;
        action.executorBot = toTank;
        action.targetUnit = targetGuid;
        action.abilitySpellId = GetTauntSpellForBot(toTank);
        action.abilityType = ThreatAbilityType::TAUNT;
        action.executeTime = std::chrono::steady_clock::now();
        action.priority = 1; // Highest priority

        _queuedResponses.push_back(action);
    }

    // Queue threat reduction from old tank
    ThreatResponseAction reduction;
    reduction.executorBot = fromTank;
    reduction.abilityType = ThreatAbilityType::THREAT_REDUCTION;
    reduction.executeTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    reduction.priority = 2;

    _queuedResponses.push_back(reduction);

    _metrics.tankSwaps++;

    TC_LOG_INFO("playerbots", "ThreatCoordinator: Initiated tank swap from {} to {}",
                fromTank.ToString(), toTank.ToString());
}

bool ThreatCoordinator::ExecuteTaunt(ObjectGuid tankGuid, Unit* target)
{
    if (!target)
        return false;

    auto it = _botAssignments.find(tankGuid);
    if (it == _botAssignments.end())
        return false;

    Player* tank = ObjectAccessor::FindPlayer(tankGuid);
    if (!tank)
        return false;

    // Find appropriate taunt ability
    uint32 tauntSpell = GetTauntSpellForBot(tankGuid);
    if (tauntSpell == 0)
        return false;

    // Check if taunt is ready
    if (tank->GetSpellHistory()->HasCooldown(tauntSpell))
        return false;

    // Execute taunt through AI
    auto aiIt = _botAIs.find(tankGuid);
    if (aiIt != _botAIs.end() && aiIt->second)
    {
        // Cast the taunt
        tank->CastSpell(target, tauntSpell, false);

        _metrics.tauntExecutions++;

        TC_LOG_DEBUG("playerbots", "ThreatCoordinator: {} executed taunt on {}",
                    tank->GetName(), target->GetName());
        return true;
    }

    return false;
}

bool ThreatCoordinator::ExecuteThreatReduction(ObjectGuid botGuid, float reductionPercent)
{
    auto it = _botAssignments.find(botGuid);
    if (it == _botAssignments.end())
        return false;

    Player* bot = ObjectAccessor::FindPlayer(botGuid);
    if (!bot)
        return false;

    // Find appropriate threat reduction ability
    auto& db = ThreatAbilitiesDB::Instance();
    auto abilities = db.GetClassAbilities(static_cast<Classes>(bot->GetClass()));

    for (const auto& ability : abilities)
    {
        if (ability.type == ThreatAbilityType::THREAT_REDUCTION ||
            ability.type == ThreatAbilityType::THREAT_DROP)
        {
            if (bot->HasSpell(ability.spellId) && !bot->GetSpellHistory()->HasCooldown(ability.spellId))
            {
                bot->CastSpell(bot, ability.spellId, false);

                _metrics.threatReductions++;

                TC_LOG_DEBUG("playerbots", "ThreatCoordinator: {} executed threat reduction ({})",
                            bot->GetName(), ability.name);
                return true;
            }
        }
    }

    return false;
}

bool ThreatCoordinator::ExecuteThreatTransfer(ObjectGuid fromBot, ObjectGuid toBot, Unit* target)
{
    if (!target)
        return false;

    Player* from = ObjectAccessor::FindPlayer(fromBot);
    Player* to = ObjectAccessor::FindPlayer(toBot);
    if (!from || !to)
        return false;

    // Check for threat transfer abilities (Misdirection, Tricks of the Trade)
    auto& db = ThreatAbilitiesDB::Instance();
    auto abilities = db.GetClassAbilities(static_cast<Classes>(from->GetClass()));

    for (const auto& ability : abilities)
    {
        if (ability.type == ThreatAbilityType::THREAT_TRANSFER)
        {
            if (from->HasSpell(ability.spellId) && !from->GetSpellHistory()->HasCooldown(ability.spellId))
            {
                from->CastSpell(to, ability.spellId, false);

                TC_LOG_DEBUG("playerbots", "ThreatCoordinator: {} transferred threat to {} using {}",
                            from->GetName(), to->GetName(), ability.name);
                return true;
            }
        }
    }

    return false;
}

void ThreatCoordinator::HandleEmergencyThreat(Unit* looseTarget)
{
    if (!looseTarget)
        return;

    std::lock_guard<std::mutex> lock(_coordinatorMutex);

    // Find available tank for emergency taunt
    ObjectGuid emergencyTank;

    if (!_primaryTank.IsEmpty() && CanBotTaunt(_primaryTank))
    {
        emergencyTank = _primaryTank;
    }
    else if (!_offTank.IsEmpty() && CanBotTaunt(_offTank))
    {
        emergencyTank = _offTank;
    }
    else
    {
        for (const auto& backup : _backupTanks)
        {
            if (CanBotTaunt(backup))
            {
                emergencyTank = backup;
                break;
            }
        }
    }

    if (!emergencyTank.IsEmpty())
    {
        ThreatResponseAction action;
        action.executorBot = emergencyTank;
        action.targetUnit = looseTarget->GetGUID();
        action.abilitySpellId = GetTauntSpellForBot(emergencyTank);
        action.abilityType = ThreatAbilityType::TAUNT;
        action.executeTime = std::chrono::steady_clock::now();
        action.priority = 0; // Maximum priority

        _queuedResponses.push_back(action);

        _metrics.emergencyResponses++;

        TC_LOG_WARN("playerbots", "ThreatCoordinator: Emergency taunt queued for {} by {}",
                   looseTarget->GetName(), emergencyTank.ToString());
    }
}

void ThreatCoordinator::ProtectHealer(ObjectGuid healerGuid, Unit* attacker)
{
    if (!attacker)
        return;

    std::lock_guard<std::mutex> lock(_coordinatorMutex);

    // Priority 1: Tank taunt
    HandleEmergencyThreat(attacker);

    // Priority 2: Healer threat reduction
    Player* healer = ObjectAccessor::FindPlayer(healerGuid);
    if (healer)
    {
        ThreatResponseAction action;
        action.executorBot = healerGuid;
        action.abilityType = ThreatAbilityType::THREAT_REDUCTION;
        action.executeTime = std::chrono::steady_clock::now();
        action.priority = 1;

        _queuedResponses.push_back(action);
    }

    TC_LOG_WARN("playerbots", "ThreatCoordinator: Protecting healer {} from {}",
               healerGuid.ToString(), attacker->GetName());
}

GroupThreatStatus ThreatCoordinator::GetGroupThreatStatus() const
{
    std::lock_guard<std::mutex> lock(_coordinatorMutex);
    return _groupStatus;
}

bool ThreatCoordinator::IsGroupThreatStable() const
{
    std::lock_guard<std::mutex> lock(_coordinatorMutex);
    return _groupStatus.state == ThreatState::STABLE;
}

float ThreatCoordinator::GetGroupThreatStability() const
{
    std::lock_guard<std::mutex> lock(_coordinatorMutex);

    if (_groupStatus.activeTargets.empty())
        return 1.0f;

    uint32 controlledTargets = 0;
    for (const auto& [target, tank] : _groupStatus.targetTanks)
    {
        if (tank == _primaryTank || tank == _offTank)
            controlledTargets++;
    }

    return static_cast<float>(controlledTargets) / _groupStatus.activeTargets.size();
}

// Private implementation methods

void ThreatCoordinator::UpdateGroupThreatStatus()
{
    _groupStatus = GroupThreatStatus();
    _groupStatus.primaryTank = _primaryTank;
    _groupStatus.offTank = _offTank;
    _groupStatus.lastUpdate = std::chrono::steady_clock::now();

    // Collect all active combat targets
    std::unordered_set<ObjectGuid> allTargets;

    for (const auto& [botGuid, threatMgr] : _botThreatManagers)
    {
        if (!threatMgr)
            continue;

        auto targets = threatMgr->GetAllThreatTargets();
        for (Unit* target : targets)
        {
            if (target && target->IsAlive() && target->IsInCombat())
            {
                ObjectGuid targetGuid = target->GetGUID();
                allTargets.insert(targetGuid);

                // Track highest threat holder for this target
                float threat = threatMgr->GetThreatPercent(target);
                auto it = _groupStatus.targetThreatLevels.find(targetGuid);
                if (it == _groupStatus.targetThreatLevels.end() || threat > it->second)
                {
                    _groupStatus.targetThreatLevels[targetGuid] = threat;
                    _groupStatus.targetTanks[targetGuid] = botGuid;
                }
            }
        }
    }

    _groupStatus.activeTargets.assign(allTargets.begin(), allTargets.end());

    // Analyze threat distribution
    for (const auto& targetGuid : _groupStatus.activeTargets)
    {
        Unit* target = ObjectAccessor::GetUnit(*ObjectAccessor::FindPlayer(_primaryTank), targetGuid);
        if (!target)
            continue;

        Unit* victim = target->GetVictim();
        if (!victim || !victim->IsPlayer())
            continue;

        Player* victimPlayer = victim->ToPlayer();
        ObjectGuid victimGuid = victimPlayer->GetGUID();

        // Check if target is on appropriate tank
        bool onTank = (victimGuid == _primaryTank || victimGuid == _offTank);
        if (!onTank)
        {
            _groupStatus.looseTargets++;

            // Check if attacking healer/DPS
            auto assignmentIt = _botAssignments.find(victimGuid);
            if (assignmentIt != _botAssignments.end())
            {
                if (assignmentIt->second.assignedRole == ThreatRole::HEALER ||
                    assignmentIt->second.assignedRole == ThreatRole::DPS)
                {
                    _groupStatus.criticalTargets++;
                }
            }
        }
    }

    // Determine overall state
    if (_groupStatus.criticalTargets > 0)
    {
        _groupStatus.state = ThreatState::CRITICAL;
        _groupStatus.requiresEmergencyResponse = true;
    }
    else if (_groupStatus.looseTargets > 1)
    {
        _groupStatus.state = ThreatState::UNSTABLE;
        _groupStatus.requiresTaunt = true;
    }
    else if (_groupStatus.looseTargets == 1)
    {
        _groupStatus.state = ThreatState::RECOVERING;
    }
    else
    {
        _groupStatus.state = ThreatState::STABLE;
    }
}

void ThreatCoordinator::UpdateBotAssignments()
{
    for (auto& [botGuid, assignment] : _botAssignments)
    {
        auto threatMgrIt = _botThreatManagers.find(botGuid);
        if (threatMgrIt == _botThreatManagers.end() || !threatMgrIt->second)
            continue;

        // Update current threat levels
        for (const auto& targetGuid : _groupStatus.activeTargets)
        {
            Unit* target = ObjectAccessor::GetUnit(*ObjectAccessor::FindPlayer(botGuid), targetGuid);
            if (!target)
                continue;

            assignment.currentThreatPercent = threatMgrIt->second->GetThreatPercent(target);
            assignment.targetGuid = targetGuid;

            // Determine if bot needs to adjust threat
            assignment.useAbilities = ShouldUseThreatAbility(assignment);
        }
    }
}

void ThreatCoordinator::ProcessThreatResponses()
{
    GenerateThreatResponses();
}

void ThreatCoordinator::CheckEmergencySituations()
{
    if (_groupStatus.state == ThreatState::CRITICAL ||
        _groupStatus.requiresEmergencyResponse)
    {
        InitiateEmergencyProtocol();
    }
}

void ThreatCoordinator::GenerateThreatResponses()
{
    for (const auto& [botGuid, assignment] : _botAssignments)
    {
        if (!assignment.useAbilities)
            continue;

        // Tank: Generate taunt if needed
        if (assignment.assignedRole == ThreatRole::TANK)
        {
            if (assignment.currentThreatPercent < 100.0f && _groupStatus.requiresTaunt)
            {
                Unit* target = ObjectAccessor::GetUnit(*ObjectAccessor::FindPlayer(botGuid), assignment.targetGuid);
                if (target && target->GetVictim() != ObjectAccessor::FindPlayer(botGuid))
                {
                    ThreatResponseAction action;
                    action.executorBot = botGuid;
                    action.targetUnit = assignment.targetGuid;
                    action.abilitySpellId = GetTauntSpellForBot(botGuid);
                    action.abilityType = ThreatAbilityType::TAUNT;
                    action.executeTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
                    action.priority = 2;

                    QueueThreatResponse(action);
                }
            }
        }
        // DPS/Healer: Generate threat reduction if needed
        else if (assignment.currentThreatPercent > assignment.targetThreatPercent)
        {
            ThreatResponseAction action;
            action.executorBot = botGuid;
            action.abilityType = ThreatAbilityType::THREAT_REDUCTION;
            action.executeTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
            action.priority = 3;

            QueueThreatResponse(action);
        }
    }
}

void ThreatCoordinator::QueueThreatResponse(ThreatResponseAction const& action)
{
    if (_queuedResponses.size() >= MAX_RESPONSE_QUEUE_SIZE)
    {
        // Remove lowest priority action
        auto it = std::max_element(_queuedResponses.begin(), _queuedResponses.end(),
            [](const ThreatResponseAction& a, const ThreatResponseAction& b) {
                return a.priority < b.priority;
            });
        if (it != _queuedResponses.end())
            _queuedResponses.erase(it);
    }

    _queuedResponses.push_back(action);

    // Sort by priority and execution time
    std::sort(_queuedResponses.begin(), _queuedResponses.end(),
        [](const ThreatResponseAction& a, const ThreatResponseAction& b) {
            if (a.priority != b.priority)
                return a.priority < b.priority;
            return a.executeTime < b.executeTime;
        });
}

void ThreatCoordinator::ExecuteQueuedResponses()
{
    auto now = std::chrono::steady_clock::now();

    for (auto& action : _queuedResponses)
    {
        if (action.executed || !action.IsReady())
            continue;

        Player* executor = ObjectAccessor::FindPlayer(action.executorBot);
        if (!executor)
        {
            action.executed = true;
            continue;
        }

        bool success = false;

        switch (action.abilityType)
        {
            case ThreatAbilityType::TAUNT:
            {
                Unit* target = ObjectAccessor::GetUnit(*executor, action.targetUnit);
                success = ExecuteTaunt(action.executorBot, target);
                if (success)
                    _metrics.tauntSuccesses++;
                break;
            }
            case ThreatAbilityType::THREAT_REDUCTION:
            case ThreatAbilityType::THREAT_DROP:
            {
                success = ExecuteThreatReduction(action.executorBot, 0.5f);
                break;
            }
            case ThreatAbilityType::THREAT_TRANSFER:
            {
                // Find appropriate target for transfer (usually tank)
                Unit* target = ObjectAccessor::GetUnit(*executor, action.targetUnit);
                success = ExecuteThreatTransfer(action.executorBot, _primaryTank, target);
                break;
            }
            default:
                break;
        }

        action.executed = true;
        action.succeeded = success;
    }
}

void ThreatCoordinator::InitiateEmergencyProtocol()
{
    TC_LOG_WARN("playerbots", "ThreatCoordinator: Initiating emergency threat protocol");

    // Execute immediate taunts on all loose targets
    for (const auto& targetGuid : _groupStatus.activeTargets)
    {
        Unit* target = ObjectAccessor::GetUnit(*ObjectAccessor::FindPlayer(_primaryTank), targetGuid);
        if (!target)
            continue;

        Unit* victim = target->GetVictim();
        if (!victim || !victim->IsPlayer())
            continue;

        ObjectGuid victimGuid = victim->ToPlayer()->GetGUID();

        // If not on tank, execute emergency taunt
        if (victimGuid != _primaryTank && victimGuid != _offTank)
        {
            ExecuteEmergencyTaunt(target);
        }
    }

    // Mass threat reduction for non-tanks
    ExecuteMassTheatReduction();

    _metrics.emergencyResponses++;
}

void ThreatCoordinator::ExecuteEmergencyTaunt(Unit* target)
{
    if (!target)
        return;

    // Try primary tank first
    if (!_primaryTank.IsEmpty())
    {
        Player* tank = ObjectAccessor::FindPlayer(_primaryTank);
        if (tank && tank->IsAlive() && tank->GetDistance2d(target) <= 30.0f)
        {
            ExecuteTaunt(_primaryTank, target);
            return;
        }
    }

    // Try off-tank
    if (!_offTank.IsEmpty())
    {
        Player* tank = ObjectAccessor::FindPlayer(_offTank);
        if (tank && tank->IsAlive() && tank->GetDistance2d(target) <= 30.0f)
        {
            ExecuteTaunt(_offTank, target);
            return;
        }
    }

    // Try backup tanks
    for (const auto& backupGuid : _backupTanks)
    {
        Player* tank = ObjectAccessor::FindPlayer(backupGuid);
        if (tank && tank->IsAlive() && tank->GetDistance2d(target) <= 30.0f)
        {
            ExecuteTaunt(backupGuid, target);
            return;
        }
    }
}

void ThreatCoordinator::ExecuteMassTheatReduction()
{
    for (const auto& [botGuid, assignment] : _botAssignments)
    {
        if (assignment.assignedRole == ThreatRole::DPS ||
            assignment.assignedRole == ThreatRole::HEALER)
        {
            ExecuteThreatReduction(botGuid, 0.5f);
        }
    }
}

void ThreatCoordinator::CleanupExpiredResponses()
{
    _queuedResponses.erase(
        std::remove_if(_queuedResponses.begin(), _queuedResponses.end(),
            [](const ThreatResponseAction& action) {
                return action.executed;
            }),
        _queuedResponses.end()
    );
}

float ThreatCoordinator::CalculateOptimalThreatPercent(ObjectGuid botGuid, ThreatRole role) const
{
    switch (role)
    {
        case ThreatRole::TANK:
            return _tankThreatThreshold;
        case ThreatRole::DPS:
            return _dpsThreatThreshold;
        case ThreatRole::HEALER:
            return _healerThreatThreshold;
        case ThreatRole::SUPPORT:
            return _healerThreatThreshold * 0.9f;
        default:
            return 50.0f;
    }
}

bool ThreatCoordinator::ShouldUseThreatAbility(BotThreatAssignment const& assignment) const
{
    // Tanks should always be ready to use threat abilities
    if (assignment.assignedRole == ThreatRole::TANK)
    {
        return assignment.currentThreatPercent < assignment.targetThreatPercent ||
               _groupStatus.requiresTaunt;
    }

    // DPS/Healers use threat reduction when approaching cap
    if (assignment.assignedRole == ThreatRole::DPS ||
        assignment.assignedRole == ThreatRole::HEALER)
    {
        return assignment.currentThreatPercent > assignment.targetThreatPercent;
    }

    return false;
}

void ThreatCoordinator::TrackPerformance(std::chrono::microseconds duration, std::string const& operation)
{
    _metrics.maxUpdateTime = std::max(_metrics.maxUpdateTime, duration);

    // Update moving average
    static std::chrono::microseconds totalTime{0};
    static uint32 samples = 0;

    totalTime += duration;
    samples++;

    if (samples >= 100) // Reset every 100 samples
    {
        _metrics.averageUpdateTime = totalTime / samples;
        totalTime = std::chrono::microseconds{0};
        samples = 0;
    }
}

void ThreatCoordinator::UpdateStabilityMetrics()
{
    float stability = GetGroupThreatStability();

    // Update moving average
    static float totalStability = 0.0f;
    static uint32 samples = 0;

    totalStability += stability;
    samples++;

    if (samples >= 100)
    {
        _metrics.averageThreatStability = totalStability / samples;
        totalStability = 0.0f;
        samples = 0;
    }

    // Update tank control rate
    if (!_groupStatus.activeTargets.empty())
    {
        uint32 tankedTargets = 0;
        for (const auto& [target, tank] : _groupStatus.targetTanks)
        {
            if (tank == _primaryTank || tank == _offTank)
                tankedTargets++;
        }

        _metrics.tankControlRate = static_cast<float>(tankedTargets) / _groupStatus.activeTargets.size();
    }
}

void ThreatCoordinator::ResetMetrics()
{
    _metrics.Reset();
}

// Helper methods

ThreatRole ThreatCoordinator::DetermineRole(Player* bot) const
{
    if (!bot)
        return ThreatRole::UNDEFINED;

    uint8 classId = bot->GetClass();

    // Check specialization for hybrid classes
    // This is simplified - in production, check actual spec
    switch (classId)
    {
        case CLASS_WARRIOR:
            return ThreatRole::TANK; // Protection assumed
        case CLASS_PALADIN:
            return ThreatRole::TANK; // Protection assumed
        case CLASS_DEATH_KNIGHT:
            return ThreatRole::TANK; // Blood assumed
        case CLASS_DEMON_HUNTER:
            return ThreatRole::TANK; // Vengeance assumed
        case CLASS_MONK:
            return ThreatRole::TANK; // Brewmaster assumed
        case CLASS_DRUID:
            return ThreatRole::TANK; // Guardian assumed
        case CLASS_PRIEST:
        case CLASS_SHAMAN:
        case CLASS_EVOKER:
            return ThreatRole::HEALER;
        case CLASS_ROGUE:
        case CLASS_HUNTER:
        case CLASS_MAGE:
        case CLASS_WARLOCK:
            return ThreatRole::DPS;
        default:
            return ThreatRole::UNDEFINED;
    }
}

uint32 ThreatCoordinator::GetTauntSpellForBot(ObjectGuid botGuid) const
{
    auto it = _botAssignments.find(botGuid);
    if (it == _botAssignments.end())
        return 0;

    Player* bot = ObjectAccessor::FindPlayer(botGuid);
    if (!bot)
        return 0;

    // Get class-specific taunt
    switch (bot->GetClass())
    {
        case CLASS_WARRIOR:
            return ThreatSpells::TAUNT;
        case CLASS_PALADIN:
            return ThreatSpells::HAND_OF_RECKONING;
        case CLASS_DEATH_KNIGHT:
            return ThreatSpells::DARK_COMMAND;
        case CLASS_DEMON_HUNTER:
            return ThreatSpells::TORMENT;
        case CLASS_MONK:
            return ThreatSpells::PROVOKE;
        case CLASS_DRUID:
            return ThreatSpells::GROWL;
        default:
            return 0;
    }
}

bool ThreatCoordinator::CanBotTaunt(ObjectGuid botGuid) const
{
    Player* bot = ObjectAccessor::FindPlayer(botGuid);
    if (!bot)
        return false;

    uint32 tauntSpell = GetTauntSpellForBot(botGuid);
    if (tauntSpell == 0)
        return false;

    return !bot->GetSpellHistory()->HasCooldown(tauntSpell);
}

void ThreatCoordinator::LogThreatStatus() const
{
    std::stringstream ss;
    ss << "=== Threat Status Report ===\n";
    ss << "State: " << static_cast<uint32>(_groupStatus.state) << "\n";
    ss << "Active Targets: " << _groupStatus.activeTargets.size() << "\n";
    ss << "Loose Targets: " << _groupStatus.looseTargets << "\n";
    ss << "Critical Targets: " << _groupStatus.criticalTargets << "\n";
    ss << "Group Stability: " << GetGroupThreatStability() * 100 << "%\n";
    ss << "Tank Control Rate: " << _metrics.tankControlRate * 100 << "%\n";

    TC_LOG_INFO("playerbots", "{}", ss.str());
}

std::string ThreatCoordinator::GetThreatReport() const
{
    std::stringstream report;

    report << "Threat Coordination Report:\n";
    report << "- Updates: " << _metrics.threatUpdates << "\n";
    report << "- Taunts: " << _metrics.tauntExecutions << " (Success: " << _metrics.tauntSuccesses << ")\n";
    report << "- Reductions: " << _metrics.threatReductions << "\n";
    report << "- Emergencies: " << _metrics.emergencyResponses << "\n";
    report << "- Tank Swaps: " << _metrics.tankSwaps << "\n";
    report << "- Avg Update Time: " << _metrics.averageUpdateTime.count() << " μs\n";
    report << "- Max Update Time: " << _metrics.maxUpdateTime.count() << " μs\n";
    report << "- Avg Stability: " << _metrics.averageThreatStability * 100 << "%\n";

    return report.str();
}

} // namespace Playerbot