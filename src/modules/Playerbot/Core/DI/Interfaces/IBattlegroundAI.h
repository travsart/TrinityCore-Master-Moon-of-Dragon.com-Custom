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
#include <cstdint>
#include <vector>

class Player;
struct Position;

namespace Playerbot
{

enum class BGType : uint8;
enum class BGRole : uint8;
struct BGObjective;
struct BGStrategyProfile;
struct BGMetrics;

/**
 * @brief Interface for Battleground AI Management
 *
 * Abstracts battleground AI operations to enable dependency injection and testing.
 * Manages automated bot behavior in all PvP battleground types.
 *
 * **Testability:**
 * - Can be mocked for testing without real battleground instances
 * - Enables testing of PvP strategies in isolation
 */
class TC_GAME_API IBattlegroundAI
{
public:
    virtual ~IBattlegroundAI() = default;

    /**
     * @brief Initialize battleground AI system
     */
    virtual void Initialize() = 0;

    /**
     * @brief Update battleground AI for player
     *
     * @param player Player to update
     * @param diff Time elapsed since last update (milliseconds)
     */
    virtual void Update(Player* player, uint32 diff) = 0;

    /**
     * @brief Assign role to player based on class/spec
     *
     * @param player Player to assign role
     * @param bgType Battleground type
     */
    virtual void AssignRole(Player* player, BGType bgType) = 0;

    /**
     * @brief Get player's current BG role
     *
     * @param player Player to query
     * @return Current battleground role
     */
    virtual BGRole GetPlayerRole(Player* player) const = 0;

    /**
     * @brief Switch player to new role
     *
     * @param player Player to update
     * @param newRole New role to assign
     * @return True if role switch successful, false otherwise
     */
    virtual bool SwitchRole(Player* player, BGRole newRole) = 0;

    /**
     * @brief Check if role is appropriate for class
     *
     * @param player Player to check
     * @param role Role to validate
     * @return True if role appropriate, false otherwise
     */
    virtual bool IsRoleAppropriate(Player* player, BGRole role) const = 0;

    /**
     * @brief Get all active objectives for battleground
     *
     * @param player Player to query
     * @return Vector of active objectives
     */
    virtual ::std::vector<BGObjective> GetActiveObjectives(Player* player) const = 0;

    /**
     * @brief Get highest priority objective for player
     *
     * @param player Player to query
     * @return Best objective for player
     */
    virtual BGObjective GetPlayerObjective(Player* player) const = 0;

    /**
     * @brief Assign players to objective
     *
     * @param player Player to assign
     * @param objective Objective to assign
     * @return True if assignment successful, false otherwise
     */
    virtual bool AssignObjective(Player* player, BGObjective const& objective) = 0;

    /**
     * @brief Complete objective
     *
     * @param player Player completing objective
     * @param objective Objective to complete
     * @return True if completion successful, false otherwise
     */
    virtual bool CompleteObjective(Player* player, BGObjective const& objective) = 0;

    /**
     * @brief Check if objective is being attacked
     *
     * @param objective Objective to check
     * @return True if contested, false otherwise
     */
    virtual bool IsObjectiveContested(BGObjective const& objective) const = 0;

    /**
     * @brief Execute Warsong Gulch / Twin Peaks strategy
     *
     * @param player Player to control
     */
    virtual void ExecuteWSGStrategy(Player* player) = 0;

    /**
     * @brief Execute Arathi Basin / Battle for Gilneas strategy
     *
     * @param player Player to control
     */
    virtual void ExecuteABStrategy(Player* player) = 0;

    /**
     * @brief Execute Alterac Valley strategy
     *
     * @param player Player to control
     */
    virtual void ExecuteAVStrategy(Player* player) = 0;

    /**
     * @brief Execute Eye of the Storm strategy
     *
     * @param player Player to control
     */
    virtual void ExecuteEOTSStrategy(Player* player) = 0;

    /**
     * @brief Execute Siege strategy (SotA / IoC)
     *
     * @param player Player to control
     */
    virtual void ExecuteSiegeStrategy(Player* player) = 0;

    /**
     * @brief Execute Temple of Kotmogu strategy
     *
     * @param player Player to control
     */
    virtual void ExecuteKotmoguStrategy(Player* player) = 0;

    /**
     * @brief Execute Silvershard Mines strategy
     *
     * @param player Player to control
     */
    virtual void ExecuteSilvershardStrategy(Player* player) = 0;

    /**
     * @brief Execute Deepwind Gorge strategy
     *
     * @param player Player to control
     */
    virtual void ExecuteDeepwindStrategy(Player* player) = 0;

    /**
     * @brief Group up for objective
     *
     * @param player Player to group
     * @param objective Objective to group for
     * @return True if grouped successfully, false otherwise
     */
    virtual bool GroupUpForObjective(Player* player, BGObjective const& objective) = 0;

    /**
     * @brief Find nearby team members
     *
     * @param player Player to search from
     * @param range Search range
     * @return Vector of nearby teammates
     */
    virtual ::std::vector<Player*> GetNearbyTeammates(Player* player, float range) const = 0;

    /**
     * @brief Call for backup at location
     *
     * @param player Player calling for backup
     * @param location Backup location
     * @return True if call sent successfully, false otherwise
     */
    virtual bool CallForBackup(Player* player, Position const& location) = 0;

    /**
     * @brief Respond to backup call
     *
     * @param player Player responding
     * @param location Backup location
     * @return True if response successful, false otherwise
     */
    virtual bool RespondToBackupCall(Player* player, Position const& location) = 0;

    /**
     * @brief Move to objective location
     *
     * @param player Player to move
     * @param objective Objective to move to
     * @return True if movement initiated, false otherwise
     */
    virtual bool MoveToObjective(Player* player, BGObjective const& objective) = 0;

    /**
     * @brief Take defensive position
     *
     * @param player Player to position
     * @param location Defense location
     * @return True if position taken, false otherwise
     */
    virtual bool TakeDefensivePosition(Player* player, Position const& location) = 0;

    /**
     * @brief Check if player is at objective
     *
     * @param player Player to check
     * @param objective Objective to check
     * @return True if at objective, false otherwise
     */
    virtual bool IsAtObjective(Player* player, BGObjective const& objective) const = 0;

    /**
     * @brief Adjust strategy based on score
     *
     * @param player Player to adjust strategy
     */
    virtual void AdjustStrategyBasedOnScore(Player* player) = 0;

    /**
     * @brief Check if team is winning
     *
     * @param player Player to check
     * @return True if team winning, false otherwise
     */
    virtual bool IsTeamWinning(Player* player) const = 0;

    /**
     * @brief Switch to defensive strategy when winning
     *
     * @param player Player to adjust
     */
    virtual void SwitchToDefensiveStrategy(Player* player) = 0;

    /**
     * @brief Switch to aggressive strategy when losing
     *
     * @param player Player to adjust
     */
    virtual void SwitchToAggressiveStrategy(Player* player) = 0;

    /**
     * @brief Set strategy profile for player
     *
     * @param playerGuid Player GUID
     * @param profile Strategy profile
     */
    virtual void SetStrategyProfile(uint32 playerGuid, BGStrategyProfile const& profile) = 0;

    /**
     * @brief Get strategy profile for player
     *
     * @param playerGuid Player GUID
     * @return Strategy profile
     */
    virtual BGStrategyProfile GetStrategyProfile(uint32 playerGuid) const = 0;

    /**
     * @brief Get player battleground metrics
     *
     * @param playerGuid Player GUID
     * @return Player metrics
     */
    virtual BGMetrics const& GetPlayerMetrics(uint32 playerGuid) const = 0;

    /**
     * @brief Get global battleground metrics
     *
     * @return Global metrics
     */
    virtual BGMetrics const& GetGlobalMetrics() const = 0;
};

} // namespace Playerbot
