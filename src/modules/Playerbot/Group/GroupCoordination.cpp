/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "GroupCoordination.h"
#include "Player.h"
#include "Group.h"
#include "Unit.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include <algorithm>

namespace Playerbot
{

GroupCoordination::GroupCoordination(uint32 groupId)
    : _groupId(groupId)
    , _currentPhase(EncounterPhase::PREPARATION)
    , _overallThreat(ThreatLevel::NONE)
    , _maintainFormationDuringMove(true)
{
    _formationCenter = Position();
    _currentDestination = Position();
    _combatStartTime = std::chrono::steady_clock::now();
    _metrics.Reset();

    TC_LOG_DEBUG("playerbot", "GroupCoordination: Created coordination for group %u", groupId);
}

void GroupCoordination::ExecuteCommand(CoordinationCommand command, const std::vector<uint32>& targets)
{
    std::lock_guard<std::mutex> lock(_commandMutex);

    CoordinationCommandData commandData(command, targets, 0, 100);

    if (ValidateCommand(commandData))
    {
        ExecuteCommandInternal(commandData);
        _metrics.commandsExecuted.fetch_add(1);
    }

    TC_LOG_DEBUG("playerbot", "GroupCoordination: Executed command %u for group %u",
                 static_cast<uint8>(command), _groupId);
}

void GroupCoordination::IssueCommand(uint32 memberGuid, CoordinationCommand command, const std::vector<uint32>& targets)
{
    std::lock_guard<std::mutex> lock(_commandMutex);

    if (_commandQueue.size() >= MAX_COMMAND_QUEUE_SIZE)
    {
        TC_LOG_WARN("playerbot", "GroupCoordination: Command queue full for group %u", _groupId);
        return;
    }

    CoordinationCommandData commandData(command, targets, memberGuid);
    _commandQueue.push(commandData);
    _metrics.commandsIssued.fetch_add(1);
}

void GroupCoordination::BroadcastCommand(CoordinationCommand command, const std::vector<uint32>& targets)
{
    // Execute command for all group members
    if (Group* group = sGroupMgr->GetGroupByGUID(_groupId))
    {
        for (GroupReference const& itr : group->GetMembers())
        {
            if (Player* member = itr.GetSource())
            {
                IssueCommand(member->GetGUID().GetCounter(), command, targets);
            }
        }
    }
}

void GroupCoordination::SetPrimaryTarget(ObjectGuid targetGuid, uint32 priority)
{
    std::lock_guard<std::mutex> lock(_targetMutex);

    _primaryTarget = targetGuid;

    // Add or update target in coordination system
    auto it = _targets.find(targetGuid);
    if (it != _targets.end())
    {
        it->second.priority = priority;
    }
    else
    {
        _targets.emplace(targetGuid, CoordinationTarget(targetGuid, priority, ThreatLevel::HIGH));
    }

    TC_LOG_DEBUG("playerbot", "GroupCoordination: Set primary target for group %u", _groupId);
}

void GroupCoordination::AddSecondaryTarget(ObjectGuid targetGuid, uint32 priority)
{
    std::lock_guard<std::mutex> lock(_targetMutex);

    _targets.emplace(targetGuid, CoordinationTarget(targetGuid, priority, ThreatLevel::MEDIUM));
}

void GroupCoordination::RemoveTarget(ObjectGuid targetGuid)
{
    std::lock_guard<std::mutex> lock(_targetMutex);

    _targets.erase(targetGuid);

    if (_primaryTarget == targetGuid)
    {
        _primaryTarget = ObjectGuid::Empty;

        // Find new primary target with highest priority
        uint32 highestPriority = 0;
        for (const auto& [guid, target] : _targets)
        {
            if (target.priority > highestPriority)
            {
                highestPriority = target.priority;
                _primaryTarget = guid;
            }
        }
    }
}

ObjectGuid GroupCoordination::GetPrimaryTarget() const
{
    std::lock_guard<std::mutex> lock(_targetMutex);
    return _primaryTarget;
}

std::vector<ObjectGuid> GroupCoordination::GetTargetPriorityList() const
{
    std::lock_guard<std::mutex> lock(_targetMutex);

    std::vector<std::pair<ObjectGuid, uint32>> targetPairs;
    for (const auto& [guid, target] : _targets)
    {
        targetPairs.emplace_back(guid, target.priority);
    }

    // Sort by priority (highest first)
    std::sort(targetPairs.begin(), targetPairs.end(),
              [](const auto& a, const auto& b) {
                  return a.second > b.second;
              });

    std::vector<ObjectGuid> priorityList;
    for (const auto& pair : targetPairs)
    {
        priorityList.push_back(pair.first);
    }

    return priorityList;
}

void GroupCoordination::SetFormation(const std::vector<FormationSlot>& formation)
{
    std::lock_guard<std::mutex> lock(_formationMutex);
    _formation = formation;
}

void GroupCoordination::UpdateFormation(const Position& leaderPosition)
{
    std::lock_guard<std::mutex> lock(_formationMutex);
    _formationCenter = leaderPosition;
    UpdateFormationPositions();
}

Position GroupCoordination::GetFormationPosition(uint32 memberGuid) const
{
    std::lock_guard<std::mutex> lock(_formationMutex);

    for (const auto& slot : _formation)
    {
        if (slot.memberGuid == memberGuid)
        {
            // Transform relative position to world position
            return Position(_formationCenter.GetPositionX() + slot.relativePosition.GetPositionX(),
                           _formationCenter.GetPositionY() + slot.relativePosition.GetPositionY(),
                           _formationCenter.GetPositionZ() + slot.relativePosition.GetPositionZ());
        }
    }

    return _formationCenter; // Default to formation center
}

bool GroupCoordination::IsInFormation(uint32 memberGuid, float tolerance) const
{
    Position assignedPos = GetFormationPosition(memberGuid);

    if (Player* player = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(memberGuid)))
    {
        float distance = assignedPos.GetExactDist(player->GetPosition());
        return distance <= tolerance;
    }

    return false;
}

void GroupCoordination::MoveToPosition(const Position& destination, bool maintainFormation)
{
    std::lock_guard<std::mutex> lock(_movementMutex);

    _currentDestination = destination;
    _maintainFormationDuringMove = maintainFormation;

    // Create movement waypoint
    MovementWaypoint waypoint(destination, 0.0f, true, "Group movement destination");

    // Clear current path and add new destination
    while (!_movementPath.empty())
        _movementPath.pop();

    _movementPath.push(waypoint);
}

void GroupCoordination::FollowLeader(uint32 leaderGuid, float distance)
{
    if (Player* leader = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(leaderGuid)))
    {
        Position leaderPos = leader->GetPosition();
        UpdateFormation(leaderPos);
    }
}

Position GroupCoordination::GetNextWaypoint() const
{
    std::lock_guard<std::mutex> lock(_movementMutex);

    if (!_movementPath.empty())
        return _movementPath.front().position;

    return Position();
}

bool GroupCoordination::HasReachedDestination() const
{
    std::lock_guard<std::mutex> lock(_movementMutex);

    if (_movementPath.empty())
        return true;

    const Position& destination = _movementPath.front().position;

    // Check if formation center is close to destination
    float distance = _formationCenter.GetExactDist(destination);
    return distance <= WAYPOINT_REACH_DISTANCE;
}

void GroupCoordination::InitiateCombat(Unit* target)
{
    if (!target)
        return;

    _inCombat.store(true);
    _combatStartTime = std::chrono::steady_clock::now();
    _currentPhase = EncounterPhase::ENGAGE;

    SetPrimaryTarget(target->GetGUID(), 150);
    BroadcastCommand(CoordinationCommand::ATTACK_TARGET, {target->GetGUID().GetCounter()});

    TC_LOG_DEBUG("playerbot", "GroupCoordination: Initiated combat for group %u", _groupId);
}

void GroupCoordination::UpdateCombatCoordination()
{
    if (!_inCombat.load())
        return;

    UpdateTargetAssessment();
    UpdateCombatTactics();
    HandleTankThreatManagement();
    HandleHealerPriorities();
    HandleDPSTargeting();
    HandleSupportActions();
}

void GroupCoordination::EndCombat()
{
    _inCombat.store(false);
    _currentPhase = EncounterPhase::RECOVERY;
    _overallThreat = ThreatLevel::NONE;

    // Clear combat targets
    {
        std::lock_guard<std::mutex> lock(_targetMutex);
        _targets.clear();
        _primaryTarget = ObjectGuid::Empty;
    }

    BroadcastCommand(CoordinationCommand::DEFENSIVE_MODE);

    TC_LOG_DEBUG("playerbot", "GroupCoordination: Ended combat for group %u", _groupId);
}

void GroupCoordination::SetEncounterPhase(EncounterPhase phase)
{
    _currentPhase = phase;

    switch (phase)
    {
        case EncounterPhase::BURN:
            BroadcastCommand(CoordinationCommand::USE_COOLDOWNS);
            BroadcastCommand(CoordinationCommand::BURN_PHASE);
            break;
        case EncounterPhase::RECOVERY:
            BroadcastCommand(CoordinationCommand::DEFENSIVE_MODE);
            break;
        case EncounterPhase::TRANSITION:
            BroadcastCommand(CoordinationCommand::SAVE_COOLDOWNS);
            break;
        default:
            break;
    }
}

void GroupCoordination::HandleEmergencySituation(ThreatLevel level)
{
    _overallThreat = level;

    switch (level)
    {
        case ThreatLevel::CRITICAL:
            BroadcastCommand(CoordinationCommand::USE_COOLDOWNS);
            BroadcastCommand(CoordinationCommand::SPREAD_OUT);
            break;
        case ThreatLevel::HIGH:
            BroadcastCommand(CoordinationCommand::DEFENSIVE_MODE);
            break;
        case ThreatLevel::MEDIUM:
            // Normal coordination continues
            break;
        default:
            break;
    }
}

void GroupCoordination::Update(uint32 diff)
{
    if (!_isActive.load())
        return;

    ProcessCommandQueue();
    UpdateTargetAssessment();
    UpdateFormationPositions();
    UpdateMovementProgress();

    if (_inCombat.load())
    {
        UpdateCombatCoordination();
    }

    UpdateMetrics();
}

void GroupCoordination::UpdateMetrics()
{
    // Calculate response time
    auto now = std::chrono::steady_clock::now();
    auto timeSinceUpdate = std::chrono::duration_cast<std::chrono::milliseconds>(now - _metrics.lastUpdate);
    _metrics.responseTime.store(timeSinceUpdate.count());

    // Update formation compliance
    float compliance = AssessFormationCompliance();
    _metrics.formationCompliance.store(compliance);

    _metrics.lastUpdate = now;
}

void GroupCoordination::ProcessCommandQueue()
{
    std::lock_guard<std::mutex> lock(_commandMutex);

    while (!_commandQueue.empty())
    {
        CoordinationCommandData command = _commandQueue.top();
        _commandQueue.pop();

        if (ValidateCommand(command))
        {
            ExecuteCommandInternal(command);
            _metrics.commandsExecuted.fetch_add(1);
        }
    }
}

void GroupCoordination::UpdateTargetAssessment()
{
    std::lock_guard<std::mutex> lock(_targetMutex);

    // Update target information
    for (auto& [guid, target] : _targets)
    {
        if (Unit* unit = ObjectAccessor::GetUnit(*ObjectAccessor::FindPlayer(_primaryTarget), guid))
        {
            target.lastKnownPosition = unit->GetPosition();
            target.lastSeen = getMSTime();
        }
    }

    // Remove stale targets
    auto it = _targets.begin();
    while (it != _targets.end())
    {
        if (getMSTime() - it->second.lastSeen > 30000) // 30 seconds
        {
            it = _targets.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void GroupCoordination::UpdateFormationPositions()
{
    // Formation positions are updated when formation center changes
    // This is handled in UpdateFormation method
}

void GroupCoordination::UpdateMovementProgress()
{
    std::lock_guard<std::mutex> lock(_movementMutex);

    if (!_movementPath.empty() && HasReachedDestination())
    {
        _movementPath.pop();

        if (!_movementPath.empty())
        {
            // Move to next waypoint
            const Position& nextPos = _movementPath.front().position;
            UpdateFormation(nextPos);
        }
    }
}

void GroupCoordination::UpdateCombatTactics()
{
    // Adapt tactics based on current phase and threat level
    switch (_currentPhase)
    {
        case EncounterPhase::ENGAGE:
            OptimizeTargetAssignments();
            break;
        case EncounterPhase::BURN:
            BroadcastCommand(CoordinationCommand::FOCUS_FIRE);
            break;
        case EncounterPhase::TRANSITION:
            BroadcastCommand(CoordinationCommand::SAVE_COOLDOWNS);
            break;
        default:
            break;
    }
}

float GroupCoordination::AssessFormationCompliance()
{
    std::lock_guard<std::mutex> lock(_formationMutex);

    if (_formation.empty())
        return 1.0f;

    uint32 membersInPosition = 0;

    for (const auto& slot : _formation)
    {
        if (IsInFormation(slot.memberGuid, FORMATION_TOLERANCE))
            membersInPosition++;
    }

    return static_cast<float>(membersInPosition) / _formation.size();
}

void GroupCoordination::OptimizeTargetAssignments()
{
    std::lock_guard<std::mutex> lock(_targetMutex);

    // Assign targets based on priority and member capabilities
    auto priorityList = GetTargetPriorityList();

    if (!priorityList.empty())
    {
        // Focus fire on primary target
        BroadcastCommand(CoordinationCommand::FOCUS_FIRE, {priorityList[0].GetCounter()});
    }
}

bool GroupCoordination::ValidateCommand(const CoordinationCommandData& command)
{
    // Check command timeout
    if (getMSTime() - command.timestamp > COMMAND_TIMEOUT)
        return false;

    // Basic validation - can be extended with more sophisticated checks
    return true;
}

void GroupCoordination::ExecuteCommandInternal(const CoordinationCommandData& command)
{
    switch (command.command)
    {
        case CoordinationCommand::ATTACK_TARGET:
            if (!command.targets.empty())
            {
                ObjectGuid targetGuid = ObjectGuid::Create<HighGuid::Unit>(command.targets[0]);
                SetPrimaryTarget(targetGuid, 100);
            }
            break;

        case CoordinationCommand::FOCUS_FIRE:
            // All DPS focus on primary target
            if (!command.targets.empty())
            {
                ObjectGuid targetGuid = ObjectGuid::Create<HighGuid::Unit>(command.targets[0]);
                SetPrimaryTarget(targetGuid, 150);
            }
            break;

        case CoordinationCommand::SPREAD_OUT:
            // Increase formation spacing
            {
                std::lock_guard<std::mutex> lock(_formationMutex);
                for (auto& slot : _formation)
                {
                    slot.maxDistance *= 1.5f; // Increase spacing
                }
            }
            break;

        case CoordinationCommand::STACK_UP:
            // Decrease formation spacing
            {
                std::lock_guard<std::mutex> lock(_formationMutex);
                for (auto& slot : _formation)
                {
                    slot.maxDistance *= 0.7f; // Decrease spacing
                }
            }
            break;

        case CoordinationCommand::RETREAT:
            // Move away from current position
            {
                Position retreatPos = _formationCenter;
                retreatPos.SetOrientation(retreatPos.GetOrientation() + M_PI); // Turn around
                retreatPos.m_positionX += 20.0f * std::cos(retreatPos.GetOrientation());
                retreatPos.m_positionY += 20.0f * std::sin(retreatPos.GetOrientation());
                MoveToPosition(retreatPos, true);
            }
            break;

        default:
            // Command handled by individual AI systems
            break;
    }
}

void GroupCoordination::HandleTankThreatManagement()
{
    // TODO: Implement tank-specific threat management
    // This would coordinate taunt rotations, threat transfers, etc.
}

void GroupCoordination::HandleHealerPriorities()
{
    // TODO: Implement healer coordination
    // This would prioritize healing targets, coordinate dispelling, etc.
}

void GroupCoordination::HandleDPSTargeting()
{
    // TODO: Implement DPS coordination
    // This would manage target switching, interrupt rotations, etc.
}

void GroupCoordination::HandleSupportActions()
{
    // TODO: Implement support coordination
    // This would coordinate buffs, utility abilities, etc.
}

} // namespace Playerbot