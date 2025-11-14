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
#include "SharedDefines.h"
#include <vector>
#include <string>

class Player;

namespace Playerbot
{

struct LevelBracket;

/**
 * @brief Level Manager Statistics
 */
struct LevelManagerStats
{
    // Creation statistics
    uint64 totalTasksSubmitted{0};
    uint64 totalTasksCompleted{0};
    uint64 totalTasksFailed{0};

    // Queue statistics
    uint32 currentQueueSize{0};
    uint32 peakQueueSize{0};

    // Performance statistics
    uint64 totalPrepTimeMs{0};     // Worker thread time
    uint64 totalApplyTimeMs{0};    // Main thread time
    uint32 averagePrepTimeMs{0};   // Per bot
    uint32 averageApplyTimeMs{0};  // Per bot

    // System statistics
    uint64 totalLevelUps{0};
    uint64 totalGearApplications{0};
    uint64 totalTalentApplications{0};
    uint64 totalTeleports{0};

    // Error statistics
    uint32 levelUpFailures{0};
    uint32 gearFailures{0};
    uint32 talentFailures{0};
    uint32 teleportFailures{0};
};

/**
 * @brief Interface for Bot Level Manager
 *
 * Orchestrator for automated world population with instant bot creation and level-up.
 * Coordinates all systems for bot character generation, gear, talents, and placement.
 *
 * **Responsibilities:**
 * - Two-phase bot creation (worker thread prep + main thread apply)
 * - Level bracket distribution management
 * - Gear and talent application coordination
 * - Zone placement and teleportation
 * - Performance statistics and monitoring
 * - Queue management and throttling
 */
class TC_GAME_API IBotLevelManager
{
public:
    virtual ~IBotLevelManager() = default;

    // ====================================================================
    // INITIALIZATION
    // ====================================================================

    /**
     * Initialize all subsystems
     * MUST be called before any bot operations
     */
    virtual bool Initialize() = 0;

    /**
     * Shutdown all subsystems
     * Called during server shutdown
     */
    virtual void Shutdown() = 0;

    /**
     * Check if manager is ready
     */
    virtual bool IsReady() const = 0;

    // ====================================================================
    // BOT CREATION API
    // ====================================================================

    /**
     * Create bot with instant level-up (async)
     * Returns task ID for tracking
     *
     * Workflow:
     * 1. Submit task to ThreadPool (worker thread)
     * 2. Worker prepares all data (level, gear, talents, zone)
     * 3. Task queued for main thread
     * 4. Main thread applies data on next update
     *
     * @param bot           Bot player object
     * @return              Task ID (0 if failed)
     */
    virtual uint64 CreateBotAsync(Player* bot) = 0;

    /**
     * Create multiple bots in batch (async)
     * More efficient than individual creation
     *
     * @param bots          Vector of bot player objects
     * @return              Number of tasks submitted
     */
    virtual uint32 CreateBotsAsync(::std::vector<Player*> const& bots) = 0;

    /**
     * Process queued bot creation tasks (main thread only)
     * Called from server update loop
     *
     * Throttling: Processes up to maxBots per call
     *
     * @param maxBots       Maximum bots to process (default: 10)
     * @return              Number of bots processed
     */
    virtual uint32 ProcessBotCreationQueue(uint32 maxBots = 10) = 0;

    // ====================================================================
    // DISTRIBUTION MANAGEMENT
    // ====================================================================

    /**
     * Get target level bracket for new bot
     * Thread-safe, uses weighted selection
     *
     * @param faction       TEAM_ALLIANCE or TEAM_HORDE
     * @return              Level bracket, or nullptr if none available
     */
    virtual LevelBracket const* SelectLevelBracket(TeamId faction) = 0;

    /**
     * Check distribution balance
     * Returns true if all brackets within tolerance (±15%)
     */
    virtual bool IsDistributionBalanced() const = 0;

    /**
     * Get distribution deviation percentage
     * 0% = perfect balance, >15% = needs rebalancing
     */
    virtual float GetDistributionDeviation() const = 0;

    /**
     * Force rebalance distribution (future enhancement)
     * Redistributes bots to match target percentages
     */
    virtual void RebalanceDistribution() = 0;

    // ====================================================================
    // STATISTICS & MONITORING
    // ====================================================================

    /**
     * Get statistics
     */
    virtual LevelManagerStats GetStats() const = 0;

    /**
     * Print statistics report to log
     */
    virtual void PrintReport() const = 0;

    /**
     * Get formatted statistics summary
     */
    virtual ::std::string GetSummary() const = 0;

    // ====================================================================
    // CONFIGURATION
    // ====================================================================

    /**
     * Set maximum bots to process per update
     * Default: 10
     */
    virtual void SetMaxBotsPerUpdate(uint32 maxBots) = 0;

    /**
     * Get maximum bots per update
     */
    virtual uint32 GetMaxBotsPerUpdate() const = 0;

    /**
     * Enable/disable verbose logging
     */
    virtual void SetVerboseLogging(bool enabled) = 0;
};

} // namespace Playerbot
