/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "Define.h"
#include "Group/RoleDefinitions.h"
#include <string>
#include <vector>

class Player;
class Unit;
class WorldObject;

namespace Playerbot
{

// Forward declarations
enum class GroupFormationType : uint8;
enum class GroupCoordinationMode : uint8;
struct GroupMemberInfo;
struct GroupFormationData;
struct GroupObjective;
struct Position;

/**
 * @brief Group Statistics
 */
struct GroupStatistics
{
    std::atomic<uint32> totalDamageDealt{0};
    std::atomic<uint32> totalHealingDone{0};
    std::atomic<uint32> totalDamageTaken{0};
    std::atomic<uint32> encountersCompleted{0};
    std::atomic<uint32> wipes{0};
    std::atomic<float> avgEncounterTime{0.0f};
    std::atomic<float> groupEfficiency{1.0f};
    std::chrono::steady_clock::time_point formationTime;
    std::chrono::steady_clock::time_point lastCombat;

    void Reset();
};

/**
 * @brief Interface for Playerbot Group Manager
 *
 * Manages bot group formation, coordination, and combat for dungeon/raid content.
 *
 * **Responsibilities:**
 * - Group creation and member management
 * - Group finder and role matching
 * - Combat coordination and threat management
 * - Movement and formation control
 * - Leadership and decision making
 * - Statistics tracking
 * - Automated group management
 */
class TC_GAME_API IPlayerbotGroupManager
{
public:
    virtual ~IPlayerbotGroupManager() = default;

    // Core group management
    virtual bool CreateGroup(Player* leader, GroupFormationType type) = 0;
    virtual bool AddMemberToGroup(uint32 groupId, Player* member, GroupRole preferredRole) = 0;
    virtual bool RemoveMemberFromGroup(uint32 groupId, uint32 memberGuid) = 0;
    virtual bool DisbandGroup(uint32 groupId) = 0;

    // Group finder and matching
    virtual uint32 FindSuitableGroup(Player* player, GroupRole role) = 0;
    virtual std::vector<uint32> FindMembersForGroup(uint32 groupId, GroupRole role, uint32 minLevel, uint32 maxLevel) = 0;
    virtual bool CanJoinGroup(Player* player, uint32 groupId, GroupRole role) = 0;

    // Group coordination
    virtual void UpdateGroupCoordination(uint32 groupId) = 0;
    virtual void SetGroupObjective(uint32 groupId, GroupObjective const& objective) = 0;
    virtual void UpdateGroupFormation(uint32 groupId, GroupFormationData const& formation) = 0;
    virtual Position GetOptimalPositionForMember(uint32 groupId, uint32 memberGuid) = 0;

    // Leadership and decision making
    virtual void AssignGroupLeader(uint32 groupId, uint32 newLeaderGuid) = 0;
    virtual void HandleLeaderDisconnect(uint32 groupId) = 0;
    virtual void MakeGroupDecision(uint32 groupId, std::string const& decision) = 0;

    // Combat coordination
    virtual void OnCombatStart(uint32 groupId, Unit* target) = 0;
    virtual void OnCombatEnd(uint32 groupId) = 0;
    virtual void CoordinateGroupAttack(uint32 groupId, Unit* target) = 0;
    virtual void HandleGroupThreat(uint32 groupId) = 0;

    // Movement and positioning
    virtual void UpdateGroupMovement(uint32 groupId) = 0;
    virtual void MoveGroupToLocation(uint32 groupId, Position const& destination) = 0;
    virtual void FormationMove(uint32 groupId, Position const& destination) = 0;

    // Communication and chat
    virtual void BroadcastToGroup(uint32 groupId, std::string const& message, ChatMsg type) = 0;
    virtual void HandleGroupChat(uint32 groupId, Player* sender, std::string const& message) = 0;

    // Statistics and monitoring
    virtual GroupStatistics GetGroupStatistics(uint32 groupId) = 0;
    virtual void UpdateGroupStatistics(uint32 groupId, GroupStatistics const& stats) = 0;

    // Automated group management
    virtual void ProcessGroupQueue() = 0;
    virtual void AutoFormGroups() = 0;
    virtual void AutoDisbandInactiveGroups() = 0;
    virtual void RebalanceGroups() = 0;

    // Configuration and settings
    virtual void SetGroupCoordinationMode(uint32 groupId, GroupCoordinationMode mode) = 0;
    virtual void EnableAutoGrouping(bool enable) = 0;
    virtual void SetMaxGroupsPerMap(uint32 mapId, uint32 maxGroups) = 0;

    // Update and maintenance
    virtual void Update(uint32 diff) = 0;
    virtual void CleanupInactiveGroups() = 0;
};

} // namespace Playerbot
