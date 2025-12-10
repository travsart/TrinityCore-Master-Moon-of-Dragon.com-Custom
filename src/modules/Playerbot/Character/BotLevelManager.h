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
#include "Threading/LockHierarchy.h"
#include "SharedDefines.h"
#include "ObjectGuid.h"
#include "Core/DI/Interfaces/IBotLevelManager.h"
#include "../Equipment/BotGearFactory.h"  // Need full definition for std::unique_ptr<GearSet>
#include <atomic>
#include <vector>
#include <queue>
#include <mutex>
#include <memory>
#include <functional>

class Player;

namespace Playerbot
{

// Forward declarations
struct LevelBracket;
struct TalentLoadout;
struct ZonePlacement;
class BotLevelDistribution;
class BotGearFactory;
class BotTalentManager;
class BotWorldPositioner;

/**
 * Bot Creation Task
 * Data prepared by worker thread, applied by main thread
 */
struct BotCreationTask
{
    // Bot identity
    ObjectGuid botGuid;
    uint32 accountId;
    ::std::string botName;

    // Character data (prepared in worker thread)
    uint8 race;
    uint8 cls;
    uint8 gender;
    TeamId faction;

    // Level data (prepared in worker thread)
    uint32 targetLevel;
    LevelBracket const* levelBracket;

    // Specialization data (prepared in worker thread)
    uint8 primarySpec;
    uint8 secondarySpec;  // For dual-spec (level 10+)
    bool useDualSpec;

    // Gear data (prepared in worker thread)
    ::std::unique_ptr<GearSet> gearSet;

    // Zone data (prepared in worker thread)
    ZonePlacement const* zonePlacement;

    // Task metadata
    uint64 taskId;
    ::std::chrono::steady_clock::time_point createdAt;
    ::std::chrono::steady_clock::time_point preparedAt;

    BotCreationTask()
        : accountId(0), race(0), cls(0), gender(0), faction(TEAM_ALLIANCE)
        , targetLevel(0), levelBracket(nullptr), primarySpec(0), secondarySpec(0)
        , useDualSpec(false), zonePlacement(nullptr), taskId(0)
    {
        createdAt = ::std::chrono::steady_clock::now();
    }
};

/**
 * Bot Level Manager - Orchestrator for Automated World Population
 *
 * Purpose: Coordinate all systems for instant bot creation and level-up
 *
 * Orchestrated Systems:
 * 1. BotLevelDistribution - Level bracket selection
 * 2. BotGearFactory - Gear generation
 * 3. BotTalentManager - Spec/talent application
 * 4. BotWorldPositioner - Zone placement
 *
 * Two-Phase Bot Creation:
 * Phase 1 (Worker Thread):
 *   - Select level bracket
 *   - Choose specialization(s)
 *   - Generate gear set
 *   - Select zone placement
 *   - NO Player API calls
 *
 * Phase 2 (Main Thread):
 *   - GiveLevel() to target level
 *   - Apply specialization
 *   - Apply talents (InitTalentForLevel, LearnTalent)
 *   - Equip gear (EquipItem)
 *   - Teleport to zone (TeleportTo)
 *   - Save to database
 *
 * Thread Safety:
 * - Worker threads prepare data using lock-free caches
 * - Main thread applies data using Player API
 * - Task queue protected by mutex (low contention)
 * - Atomic counters for statistics
 *
 * Performance:
 * - Worker thread prep: <5ms per bot
 * - Main thread apply: <50ms per bot (throttled to 10/update)
 * - Memory: ~1KB per queued task
 * - Scales to 5000+ bots
 *
 * Throttling:
 * - Maximum 10 bots processed per server update (configurable)
 * - Prevents server stalls from bulk bot creation
 * - Queue drains naturally over time
 *
 * Distribution Monitoring:
 * - Tracks bot distribution across level brackets
 * - Automatic rebalancing (future enhancement)
 * - Statistics reporting
 */
class TC_GAME_API BotLevelManager final : public IBotLevelManager
{
public:
    static BotLevelManager* instance();

    // IBotLevelManager interface implementation

    /**
     * Initialize all subsystems
     * MUST be called before any bot operations
     * Single-threaded execution required
     */
    bool Initialize() override;

    /**
     * Shutdown all subsystems
     * Called during server shutdown
     */
    void Shutdown() override;

    /**
     * Check if manager is ready
     */
    bool IsReady() const override
    {
        return _initialized.load(::std::memory_order_acquire);
    }

    // ====================================================================
    // BOT CREATION API (Two-Phase Workflow)
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
    uint64 CreateBotAsync(Player* bot) override;

    /**
     * Create multiple bots in batch (async)
     * More efficient than individual creation
     *
     * @param bots          Vector of bot player objects
     * @return              Number of tasks submitted
     */
    uint32 CreateBotsAsync(::std::vector<Player*> const& bots) override;

    /**
     * Process queued bot creation tasks (main thread only)
     * Called from server update loop
     *
     * Throttling: Processes up to maxBots per call
     *
     * @param maxBots       Maximum bots to process (default: 10)
     * @return              Number of bots processed
     */
    uint32 ProcessBotCreationQueue(uint32 maxBots = 10) override;

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
    LevelBracket const* SelectLevelBracket(TeamId faction) override;

    /**
     * Check distribution balance
     * Returns true if all brackets within tolerance (±15%)
     */
    bool IsDistributionBalanced() const override;

    /**
     * Get distribution deviation percentage
     * 0% = perfect balance, >15% = needs rebalancing
     */
    float GetDistributionDeviation() const override;

    /**
     * Force rebalance distribution
     * Redistributes bots to match target percentages
     * Analyzes over/underpopulated brackets and coordinates spawning
     */
    void RebalanceDistribution() override;

private:
    /**
     * Rebalance distribution for a specific faction
     * @param faction The faction to rebalance (TEAM_ALLIANCE or TEAM_HORDE)
     */
    void RebalanceFaction(TeamId faction);

    // ====================================================================
    // STATISTICS & MONITORING
    // ====================================================================

    using LevelManagerStats = Playerbot::LevelManagerStats;

    LevelManagerStats GetStats() const override { return _stats; }
    void PrintReport() const override;
    ::std::string GetSummary() const override;

    // ====================================================================
    // CONFIGURATION
    // ====================================================================

    /**
     * Set maximum bots to process per update
     * Default: 10
     */
    void SetMaxBotsPerUpdate(uint32 maxBots) override
    {
        _maxBotsPerUpdate.store(maxBots, ::std::memory_order_release);
    }

    uint32 GetMaxBotsPerUpdate() const override
    {
        return _maxBotsPerUpdate.load(::std::memory_order_acquire);
    }

    /**
     * Enable/disable verbose logging
     */
    void SetVerboseLogging(bool enabled) override
    {
        _verboseLogging.store(enabled, ::std::memory_order_release);
    }

private:
    BotLevelManager() = default;
    ~BotLevelManager() = default;
    BotLevelManager(BotLevelManager const&) = delete;
    BotLevelManager& operator=(BotLevelManager const&) = delete;

    // ====================================================================
    // WORKER THREAD TASKS (Phase 1: Data Preparation)
    // ====================================================================

    /**
     * Prepare bot creation data (worker thread)
     * NO Player API calls allowed
     */
    void PrepareBot_WorkerThread(::std::shared_ptr<BotCreationTask> task);

    /**
     * Generate bot character data
     */
    void GenerateCharacterData(BotCreationTask* task);

    /**
     * Select level and bracket
     */
    void SelectLevel(BotCreationTask* task);

    /**
     * Select specializations (primary + secondary for dual-spec)
     */
    void SelectSpecializations(BotCreationTask* task);

    /**
     * Generate gear set
     */
    void GenerateGear(BotCreationTask* task);

    /**
     * Select zone placement
     */
    void SelectZone(BotCreationTask* task);

    // ====================================================================
    // MAIN THREAD TASKS (Phase 2: Player API Application)
    // ====================================================================

    /**
     * Apply bot creation data (main thread only)
     * Uses Player API
     */
    bool ApplyBot_MainThread(BotCreationTask* task);

    /**
     * Apply level-up to target level
     */
    bool ApplyLevel(Player* bot, BotCreationTask* task);

    /**
     * Apply specialization and talents
     */
    bool ApplyTalents(Player* bot, BotCreationTask* task);

    /**
     * Equip gear set
     */
    bool ApplyGear(Player* bot, BotCreationTask* task);

    /**
     * Teleport to zone
     */
    bool ApplyZone(Player* bot, BotCreationTask* task);

    /**
     * Apply professions for bot (level 10+)
     * Learns 2 main professions + cooking/fishing and levels up skills
     */
    bool ApplyProfessions(Player* bot, BotCreationTask* task);

    // ====================================================================
    // TASK QUEUE MANAGEMENT
    // ====================================================================

    /**
     * Queue task for main thread processing
     */
    void QueueMainThreadTask(::std::shared_ptr<BotCreationTask> task);

    /**
     * Get next task from queue (main thread only)
     */
    ::std::shared_ptr<BotCreationTask> DequeueTask();

    // ====================================================================
    // SUBSYSTEM REFERENCES
    // ====================================================================

    BotLevelDistribution* _distribution;
    BotGearFactory* _gearFactory;
    BotTalentManager* _talentManager;
    BotWorldPositioner* _positioner;

    // ====================================================================
    // TASK QUEUE
    // ====================================================================

    ::std::queue<::std::shared_ptr<BotCreationTask>> _mainThreadQueue;
    Playerbot::OrderedMutex<Playerbot::LockOrder::BEHAVIOR_MANAGER> _queueMutex;

    // ====================================================================
    // STATISTICS
    // ====================================================================

    LevelManagerStats _stats;
    ::std::atomic<uint64> _nextTaskId{1};

    // ====================================================================
    // CONFIGURATION
    // ====================================================================

    ::std::atomic<uint32> _maxBotsPerUpdate{10};
    ::std::atomic<bool> _verboseLogging{false};
    ::std::atomic<bool> _initialized{false};
};

} // namespace Playerbot

#define sBotLevelManager Playerbot::BotLevelManager::instance()
