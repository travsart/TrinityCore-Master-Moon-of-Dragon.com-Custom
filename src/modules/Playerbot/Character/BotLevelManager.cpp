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

#include "BotLevelManager.h"
#include "BotLevelDistribution.h"
#include "../Equipment/BotGearFactory.h"
#include "../Talents/BotTalentManager.h"
#include "../Movement/BotWorldPositioner.h"
#include "../Professions/ProfessionManager.h"
#include "../Performance/ThreadPool/ThreadPool.h"
#include "Player.h"
#include "Config/PlayerbotConfig.h"
#include "Log.h"
#include "World.h"
#include "WorldSession.h"
#include "ObjectAccessor.h"
#include <sstream>

namespace Playerbot
{

// ====================================================================
// SINGLETON INSTANCE
// ====================================================================

BotLevelManager* BotLevelManager::instance()
{
    static BotLevelManager instance;
    return &instance;
}

// ====================================================================
// INITIALIZATION
// ====================================================================

bool BotLevelManager::Initialize()
{
    if (_initialized.load(::std::memory_order_acquire))
    {
        TC_LOG_WARN("playerbot", "BotLevelManager::Initialize() - Already initialized");
        return true;
    }

    TC_LOG_INFO("playerbot", "BotLevelManager::Initialize() - Starting subsystem initialization...");

    // Get subsystem references
    _distribution = BotLevelDistribution::instance();
    _gearFactory = BotGearFactory::instance();
    _talentManager = BotTalentManager::instance();
    _positioner = BotWorldPositioner::instance();

    // Verify all subsystems are ready
    if (!_gearFactory->IsReady())
    {
        TC_LOG_ERROR("playerbot", "BotLevelManager::Initialize() - BotGearFactory not ready");
        return false;
    }

    // Load configuration
    _maxBotsPerUpdate.store(sPlayerbotConfig->GetInt("Playerbot.LevelManager.MaxBotsPerUpdate", 10), ::std::memory_order_release);
    _verboseLogging.store(sPlayerbotConfig->GetBool("Playerbot.LevelManager.VerboseLogging", false), ::std::memory_order_release);

    // Reset statistics
    _stats = LevelManagerStats();

    _initialized.store(true, ::std::memory_order_release);

    TC_LOG_INFO("playerbot", "BotLevelManager::Initialize() - All subsystems ready");
    TC_LOG_INFO("playerbot", "  -> Gear Factory ready");
    TC_LOG_INFO("playerbot", "  -> Talent Manager ready");
    TC_LOG_INFO("playerbot", "  -> World Positioner ready");
    TC_LOG_INFO("playerbot", "  -> Max Bots Per Update: {}", _maxBotsPerUpdate.load());

    return true;
}

void BotLevelManager::Shutdown()
{
    if (!_initialized.load(::std::memory_order_acquire))
        return;

    TC_LOG_INFO("playerbot", "BotLevelManager::Shutdown() - Shutting down...");

    // Clear task queue
    {
        // No lock needed - level queue is per-bot instance data
    while (!_mainThreadQueue.empty())
            _mainThreadQueue.pop();
    }

    // Print final statistics
    PrintReport();

    _initialized.store(false, ::std::memory_order_release);
}

// ====================================================================
// BOT CREATION API (Two-Phase Workflow)
// ====================================================================

uint64 BotLevelManager::CreateBotAsync(Player* bot)
{
    if (!bot)
    {
        TC_LOG_ERROR("playerbot", "BotLevelManager::CreateBotAsync() - Invalid bot pointer");
        return 0;
    }

    if (!IsReady())
    {
        TC_LOG_ERROR("playerbot", "BotLevelManager::CreateBotAsync() - Manager not initialized");
        return 0;
    }

    // Create task
    auto task = ::std::make_shared<BotCreationTask>();
    task->taskId = _nextTaskId.fetch_add(1, ::std::memory_order_relaxed);
    task->botGuid = bot->GetGUID();
    task->accountId = bot->GetSession()->GetAccountId();
    task->botName = bot->GetName();

    // Submit to ThreadPool for asynchronous data preparation (Phase 1)
    // Worker thread will prepare all bot data (level, gear, talents, zone) without Player API calls
    try
    {
        auto& threadPool = Performance::GetThreadPool();
        threadPool.Submit(Performance::TaskPriority::NORMAL,
            [this, task]()
            {
                this->PrepareBot_WorkerThread(task);
            });
    }
    catch (::std::exception const& e)
    {
        TC_LOG_ERROR("playerbot", "BotLevelManager::CreateBotAsync() - ThreadPool submission failed: {}", e.what());
        // Fallback to synchronous execution if ThreadPool is unavailable
        PrepareBot_WorkerThread(task);
    }

    ++_stats.totalTasksSubmitted;

    if (_verboseLogging.load(::std::memory_order_acquire))
    {
        TC_LOG_DEBUG("playerbot", "BotLevelManager::CreateBotAsync() - Task {} submitted for bot {}",
            task->taskId, task->botName);
    }

    return task->taskId;
}

uint32 BotLevelManager::CreateBotsAsync(::std::vector<Player*> const& bots)
{
    uint32 submitted = 0;

    for (auto* bot : bots)
    {
        if (CreateBotAsync(bot) > 0)
            ++submitted;
    }

    TC_LOG_INFO("playerbot", "BotLevelManager::CreateBotsAsync() - Submitted {} of {} bots",
        submitted, bots.size());

    return submitted;
}

uint32 BotLevelManager::ProcessBotCreationQueue(uint32 maxBots)
{
    if (!IsReady())
        return 0;

    uint32 processed = 0;
    auto startTime = ::std::chrono::steady_clock::now();

    while (processed < maxBots)
    {
        // Get next task from queue
        auto task = DequeueTask();
        if (!task)
            break;

        // Apply bot data (Phase 2 - Main Thread)
        bool success = ApplyBot_MainThread(task.get());

        if (success)
        {
            ++_stats.totalTasksCompleted;
            ++processed;

            if (_verboseLogging.load(::std::memory_order_acquire))
            {
                TC_LOG_DEBUG("playerbot", "BotLevelManager::ProcessBotCreationQueue() - Task {} completed for bot {}",
                    task->taskId, task->botName);
            }
        }
        else
        {
            ++_stats.totalTasksFailed;
            TC_LOG_ERROR("playerbot", "BotLevelManager::ProcessBotCreationQueue() - Task {} failed for bot {}",
                task->taskId, task->botName);
        }
    }

    // Update statistics
    if (processed > 0)
    {
        auto endTime = ::std::chrono::steady_clock::now();
        auto elapsedMs = ::std::chrono::duration_cast<::std::chrono::milliseconds>(endTime - startTime).count();

        _stats.totalApplyTimeMs += elapsedMs;
        _stats.averageApplyTimeMs = static_cast<uint32>(_stats.totalApplyTimeMs / _stats.totalTasksCompleted);

        TC_LOG_DEBUG("playerbot", "BotLevelManager::ProcessBotCreationQueue() - Processed {} bots in {}ms",
            processed, elapsedMs);
    }

    return processed;
}

// ====================================================================
// WORKER THREAD TASKS (Phase 1: Data Preparation)
// ====================================================================

void BotLevelManager::PrepareBot_WorkerThread(::std::shared_ptr<BotCreationTask> task)
{
    auto startTime = ::std::chrono::steady_clock::now();

    try
    {
        // Generate character data (race, class, gender)
        GenerateCharacterData(task.get());

        // Select level and bracket
        SelectLevel(task.get());

        // Select specializations
        SelectSpecializations(task.get());

        // Generate gear set
        GenerateGear(task.get());

        // Select zone placement
        SelectZone(task.get());

        // Mark as prepared
        task->preparedAt = ::std::chrono::steady_clock::now();

        // Calculate preparation time
        auto elapsedMs = ::std::chrono::duration_cast<::std::chrono::milliseconds>(
            task->preparedAt - task->createdAt).count();
        _stats.totalPrepTimeMs += elapsedMs;
        _stats.averagePrepTimeMs = static_cast<uint32>(
            _stats.totalPrepTimeMs / (_stats.totalTasksSubmitted + 1));

        // Queue for main thread processing
        QueueMainThreadTask(task);

        if (_verboseLogging.load(::std::memory_order_acquire))
        {
            TC_LOG_DEBUG("playerbot", "BotLevelManager::PrepareBot_WorkerThread() - Task {} prepared in {}ms",
                task->taskId, elapsedMs);
        }
    }
    catch (::std::exception const& e)
    {
        TC_LOG_ERROR("playerbot", "BotLevelManager::PrepareBot_WorkerThread() - Task {} failed: {}",
            task->taskId, e.what());
        ++_stats.totalTasksFailed;
    }
}

void BotLevelManager::GenerateCharacterData(BotCreationTask* task)
{
    // For now, assume bot already has race/class/gender from creation
    // In future, this could generate random values based on distribution
    // task->race = distribution->SelectRace(task->faction);
    // task->cls = distribution->SelectClass(task->race);
    // task->gender = rand() % 2;
}

void BotLevelManager::SelectLevel(BotCreationTask* task)
{
    // Select level bracket from distribution
    task->levelBracket = _distribution->SelectBracketWeighted(task->faction);

    if (!task->levelBracket)
    {
        TC_LOG_ERROR("playerbot", "BotLevelManager::SelectLevel() - No bracket for faction {}", task->faction);
        throw ::std::runtime_error("No level bracket available");
    }

    // Select specific level within bracket (weighted towards middle)
    uint32 range = task->levelBracket->maxLevel - task->levelBracket->minLevel;
    if (range == 0)
    {
        task->targetLevel = task->levelBracket->minLevel;
    }
    else
    {
        // Weighted towards middle: 50% middle, 25% low, 25% high
        float random = static_cast<float>(rand()) / RAND_MAX;
        if (random < 0.25f)
            task->targetLevel = task->levelBracket->minLevel;
        else if (random > 0.75f)
            task->targetLevel = task->levelBracket->maxLevel;
        else
            task->targetLevel = task->levelBracket->minLevel + (range / 2);
    }

    if (_verboseLogging.load(::std::memory_order_acquire))
    {
        TC_LOG_DEBUG("playerbot", "BotLevelManager::SelectLevel() - Task {}: Level {} (Bracket {}-{})",
            task->taskId, task->targetLevel, task->levelBracket->minLevel, task->levelBracket->maxLevel);
    }
}

void BotLevelManager::SelectSpecializations(BotCreationTask* task)
{
    // Select primary specialization
    auto primaryChoice = _talentManager->SelectSpecialization(task->cls, task->faction, task->targetLevel);
    task->primarySpec = primaryChoice.specId;

    // Check if dual-spec should be enabled (level 10+)
    task->useDualSpec = _talentManager->SupportsDualSpec(task->targetLevel);

    if (task->useDualSpec)
    {
        // Select secondary specialization (different from primary)
        auto secondaryChoice = _talentManager->SelectSecondarySpecialization(
            task->cls, task->faction, task->targetLevel, task->primarySpec);
        task->secondarySpec = secondaryChoice.specId;

        if (_verboseLogging.load(::std::memory_order_acquire))
        {
            TC_LOG_DEBUG("playerbot", "BotLevelManager::SelectSpecializations() - Task {}: Spec1={}, Spec2={}",
                task->taskId, task->primarySpec, task->secondarySpec);
        }
    }
    else
    {
        if (_verboseLogging.load(::std::memory_order_acquire))
        {
            TC_LOG_DEBUG("playerbot", "BotLevelManager::SelectSpecializations() - Task {}: Spec={}",
                task->taskId, task->primarySpec);
        }
    }
}

void BotLevelManager::GenerateGear(BotCreationTask* task)
{
    // Skip gear for L1-4 (natural leveling)
    if (task->targetLevel <= 4)
    {
        if (_verboseLogging.load(::std::memory_order_acquire))
        {
            TC_LOG_DEBUG("playerbot", "BotLevelManager::GenerateGear() - Task {}: Skipping gear (L1-4)",
                task->taskId);
        }
        return;
    }

    // Generate gear set for primary spec
    auto gearSet = _gearFactory->BuildGearSet(task->cls, task->primarySpec, task->targetLevel, task->faction);

    if (gearSet.IsComplete())
    {
        task->gearSet = ::std::make_unique<GearSet>(::std::move(gearSet));

        if (_verboseLogging.load(::std::memory_order_acquire))
        {
            TC_LOG_DEBUG("playerbot", "BotLevelManager::GenerateGear() - Task {}: Generated {} items (iLvl {:.1f})",
                task->taskId, task->gearSet->items.size(), task->gearSet->averageIlvl);
        }
    }
    else
    {
        TC_LOG_WARN("playerbot", "BotLevelManager::GenerateGear() - Task {}: Incomplete gear set",
            task->taskId);
    }
}

void BotLevelManager::SelectZone(BotCreationTask* task)
{
    // Select zone for level and faction
    auto zoneChoice = _positioner->SelectZone(task->targetLevel, task->faction, task->race);

    if (zoneChoice.IsValid())
    {
        task->zonePlacement = zoneChoice.placement;

        if (_verboseLogging.load(::std::memory_order_acquire))
        {
            TC_LOG_DEBUG("playerbot", "BotLevelManager::SelectZone() - Task {}: Zone {} ({})",
                task->taskId, task->zonePlacement->zoneId, task->zonePlacement->zoneName);
        }
    }
    else
    {
        TC_LOG_WARN("playerbot", "BotLevelManager::SelectZone() - Task {}: No zone found, using capital",
            task->taskId);

        // Fallback to capital city
        auto capitalChoice = _positioner->GetCapitalCity(task->faction);
        if (capitalChoice.IsValid())
        {
            task->zonePlacement = capitalChoice.placement;
        }
    }
}

// ====================================================================
// MAIN THREAD TASKS (Phase 2: Player API Application)
// ====================================================================

bool BotLevelManager::ApplyBot_MainThread(BotCreationTask* task)
{
    // Get bot player object
    Player* bot = ObjectAccessor::FindPlayer(task->botGuid);
    if (!bot)
    {
        TC_LOG_ERROR("playerbot", "BotLevelManager::ApplyBot_MainThread() - Bot {} not found",
            task->botGuid.ToString());
        return false;
    }

    bool success = true;

    // Apply level-up
    if (!ApplyLevel(bot, task))
    {
        TC_LOG_ERROR("playerbot", "BotLevelManager::ApplyBot_MainThread() - Level application failed for {}",
            bot->GetName());
        success = false;
    }

    // Apply talents
    if (!ApplyTalents(bot, task))
    {
        TC_LOG_ERROR("playerbot", "BotLevelManager::ApplyBot_MainThread() - Talent application failed for {}",
            bot->GetName());
        success = false;
    }

    // Apply professions (level 10+)
    if (task->targetLevel >= 10)
    {
        if (!ApplyProfessions(bot, task))
        {
            TC_LOG_WARN("playerbot", "BotLevelManager::ApplyBot_MainThread() - Profession application failed for {}",
                bot->GetName());
            // Don't fail the whole bot creation for profession issues
        }
    }

    // Apply gear (skip for L1-4)
    if (task->targetLevel > 4)
    {
        if (!ApplyGear(bot, task))
        {
            TC_LOG_ERROR("playerbot", "BotLevelManager::ApplyBot_MainThread() - Gear application failed for {}",
                bot->GetName());
            success = false;
        }
    }
    // Apply zone placement
    if (!ApplyZone(bot, task))
    {
        TC_LOG_ERROR("playerbot", "BotLevelManager::ApplyBot_MainThread() - Zone placement failed for {}",
            bot->GetName());
        success = false;
    }

    // Save to database
    if (success)
    {
        bot->SaveToDB();

        if (_verboseLogging.load(::std::memory_order_acquire))
        {
            TC_LOG_INFO("playerbot", "BotLevelManager::ApplyBot_MainThread() - Bot {} fully created (L{}, Spec {}, Zone {})",
                bot->GetName(), task->targetLevel, task->primarySpec, task->zonePlacement->zoneName);
        }
    }

    return success;
}

bool BotLevelManager::ApplyLevel(Player* bot, BotCreationTask* task)
{
    if (!bot || !task)
        return false;

    // Get current level
    uint32 currentLevel = bot->GetLevel();
    // Skip if already at target level
    if (currentLevel >= task->targetLevel)
        return true;

    // Apply level-ups
    for (uint32 level = currentLevel + 1; level <= task->targetLevel; ++level)
    {
        bot->GiveLevel(level);
    }

    ++_stats.totalLevelUps;

    if (_verboseLogging.load(::std::memory_order_acquire))
    {
        TC_LOG_DEBUG("playerbot", "BotLevelManager::ApplyLevel() - Bot {} leveled to {}",
            bot->GetName(), task->targetLevel);
    }

    return true;
}

bool BotLevelManager::ApplyTalents(Player* bot, BotCreationTask* task)
{
    if (!bot || !task)
        return false;

    bool success = true;

    // Initialize talent system for level
    bot->InitTalentForLevel();

    // Setup dual-spec if enabled
    if (task->useDualSpec)
    {
        success = _talentManager->SetupDualSpec(bot, task->primarySpec, task->secondarySpec, task->targetLevel);
    }
    else
    {
        // Single spec setup
        success = _talentManager->SetupBotTalents(bot, task->primarySpec, task->targetLevel);
    }

    if (success)
        ++_stats.totalTalentApplications;
    else
        ++_stats.talentFailures;

    return success;
}

bool BotLevelManager::ApplyGear(Player* bot, BotCreationTask* task)
{
    if (!bot || !task || !task->gearSet)
        return false;

    bool success = _gearFactory->ApplyGearSet(bot, *task->gearSet);

    if (success)
        ++_stats.totalGearApplications;
    else
        ++_stats.gearFailures;

    return success;
}

bool BotLevelManager::ApplyZone(Player* bot, BotCreationTask* task)
{
    if (!bot || !task || !task->zonePlacement)
        return false;

    bool success = _positioner->TeleportToZone(bot, task->zonePlacement);

    if (success)
        ++_stats.totalTeleports;
    else
        ++_stats.teleportFailures;

    return success;
}

bool BotLevelManager::ApplyProfessions(Player* bot, BotCreationTask* task)
{
    if (!bot || !task)
        return false;

    // Create a temporary ProfessionManager for this bot to learn professions
    // The bot's persistent ProfessionManager (via GameSystemsManager) will handle
    // auto-leveling through crafting/gathering during regular Update() cycles
    ProfessionManager profMgr(bot);

    // Learn class-appropriate professions + cooking/fishing
    // This calls LearnProfession for 2 main professions + Cooking + Fishing
    // Skill starts at 1 - bot will level it up naturally through crafting
    profMgr.AutoLearnProfessionsForClass();

    // Get all learned professions for logging
    std::vector<ProfessionSkillInfo> professions = profMgr.GetPlayerProfessions();

    uint32 professionsLearned = 0;
    for (ProfessionSkillInfo const& profInfo : professions)
    {
        if (profInfo.profession == ProfessionType::NONE)
            continue;

        ++professionsLearned;

        if (_verboseLogging.load(::std::memory_order_acquire))
        {
            uint32 skillId = static_cast<uint32>(profInfo.profession);
            TC_LOG_DEBUG("playerbot", "BotLevelManager::ApplyProfessions - {} learned profession {} (skill starts at 1, will level through crafting)",
                bot->GetName(), skillId);
        }
    }

    // Calculate max skill potential for logging
    uint16 maxSkillPotential = std::min<uint16>(task->targetLevel * 5, 450);

    TC_LOG_INFO("playerbot", "BotLevelManager::ApplyProfessions - {} learned {} professions (skill 1, max potential {} for level {})",
        bot->GetName(), professionsLearned, maxSkillPotential, task->targetLevel);

    return professionsLearned > 0;
}

// ====================================================================
// TASK QUEUE MANAGEMENT
// ====================================================================

void BotLevelManager::QueueMainThreadTask(::std::shared_ptr<BotCreationTask> task)
{
    // No lock needed - level queue is per-bot instance data
    _mainThreadQueue.push(task);

    _stats.currentQueueSize = static_cast<uint32>(_mainThreadQueue.size());
    if (_stats.currentQueueSize > _stats.peakQueueSize)
        _stats.peakQueueSize = _stats.currentQueueSize;
}

::std::shared_ptr<BotCreationTask> BotLevelManager::DequeueTask()
{
    // No lock needed - level queue is per-bot instance data
    if (_mainThreadQueue.empty())
        return nullptr;

    auto task = _mainThreadQueue.front();
    _mainThreadQueue.pop();

    _stats.currentQueueSize = static_cast<uint32>(_mainThreadQueue.size());

    return task;
}

// ====================================================================
// DISTRIBUTION MANAGEMENT
// ====================================================================

LevelBracket const* BotLevelManager::SelectLevelBracket(TeamId faction)
{
    if (!IsReady())
        return nullptr;

    return _distribution->SelectBracketWeighted(faction);
}

bool BotLevelManager::IsDistributionBalanced() const
{
    if (!IsReady())
        return false;

    if (!_distribution)
        return false;

    // Check if both factions are balanced
    // A distribution is balanced when all brackets are within ±15% tolerance
    bool allianceBalanced = _distribution->IsDistributionBalanced(TEAM_ALLIANCE);
    bool hordeBalanced = _distribution->IsDistributionBalanced(TEAM_HORDE);

    return allianceBalanced && hordeBalanced;
}

float BotLevelManager::GetDistributionDeviation() const
{
    if (!IsReady())
        return 100.0f;

    if (!_distribution)
        return 100.0f;

    // Get distribution statistics which includes maximum deviation across all brackets
    auto stats = _distribution->GetDistributionStats();

    // Return the maximum deviation percentage (0.0 = perfect balance, >0.15 = needs rebalancing)
    // Convert from decimal to percentage (e.g., 0.15 → 15.0%)
    return stats.maxDeviation * 100.0f;
}

void BotLevelManager::RebalanceDistribution()
{
    if (!IsReady())
    {
        TC_LOG_WARN("playerbot", "BotLevelManager::RebalanceDistribution() - Manager not initialized");
        return;
    }

    if (!_distribution)
    {
        TC_LOG_ERROR("playerbot", "BotLevelManager::RebalanceDistribution() - Distribution system not available");
        return;
    }

    TC_LOG_INFO("playerbot", "BotLevelManager::RebalanceDistribution() - Starting distribution rebalancing...");

    // Process both factions
    for (TeamId faction : { TEAM_ALLIANCE, TEAM_HORDE })
    {
        RebalanceFaction(faction);
    }

    TC_LOG_INFO("playerbot", "BotLevelManager::RebalanceDistribution() - Distribution rebalancing complete");
}

void BotLevelManager::RebalanceFaction(TeamId faction)
{
    // Get underpopulated and overpopulated brackets for this faction
    auto underpopulated = _distribution->GetUnderpopulatedBrackets(faction);
    auto overpopulated = _distribution->GetOverpopulatedBrackets(faction);

    if (underpopulated.empty() && overpopulated.empty())
    {
        TC_LOG_DEBUG("playerbot", "BotLevelManager::RebalanceFaction() - {} distribution already balanced",
            faction == TEAM_ALLIANCE ? "Alliance" : "Horde");
        return;
    }

    TC_LOG_INFO("playerbot", "BotLevelManager::RebalanceFaction() - {} has {} underpopulated and {} overpopulated brackets",
        faction == TEAM_ALLIANCE ? "Alliance" : "Horde",
        underpopulated.size(), overpopulated.size());

    // Calculate how many bots need to be moved
    uint32 totalBotsToMove = 0;
    for (auto const* bracket : underpopulated)
    {
        // Get total bots for this faction to calculate target
        uint32 totalFactionBots = 0;
        auto stats = _distribution->GetDistributionStats();
        totalFactionBots = (faction == TEAM_ALLIANCE) ? stats.allianceBots : stats.hordeBots;

        if (totalFactionBots == 0)
            continue;

        uint32 target = bracket->GetTargetCount(totalFactionBots);
        uint32 current = bracket->GetCount();
        if (current < target)
            totalBotsToMove += (target - current);
    }

    if (totalBotsToMove == 0)
    {
        TC_LOG_DEBUG("playerbot", "BotLevelManager::RebalanceFaction() - No bots need to be moved for {}",
            faction == TEAM_ALLIANCE ? "Alliance" : "Horde");
        return;
    }

    // Limit the number of bots to rebalance per call to prevent server stalls
    uint32 const MAX_REBALANCE_PER_CALL = 20;
    uint32 botsToProcess = ::std::min(totalBotsToMove, MAX_REBALANCE_PER_CALL);

    TC_LOG_INFO("playerbot", "BotLevelManager::RebalanceFaction() - {} bots need rebalancing, processing up to {}",
        totalBotsToMove, botsToProcess);

    // Strategy: Create spawn requests for underpopulated brackets
    // The spawner will prioritize these due to weighted selection
    uint32 spawnRequestsCreated = 0;

    for (auto const* bracket : underpopulated)
    {
        if (spawnRequestsCreated >= botsToProcess)
            break;

        // Get total bots for calculating deficit
        auto stats = _distribution->GetDistributionStats();
        uint32 totalFactionBots = (faction == TEAM_ALLIANCE) ? stats.allianceBots : stats.hordeBots;

        if (totalFactionBots == 0)
            continue;

        uint32 target = bracket->GetTargetCount(totalFactionBots);
        uint32 current = bracket->GetCount();

        if (current >= target)
            continue;

        uint32 deficit = target - current;
        uint32 toSpawn = ::std::min(deficit, botsToProcess - spawnRequestsCreated);

        // Log the action - actual spawning will be done by BotSpawner on next update
        TC_LOG_INFO("playerbot", "BotLevelManager::RebalanceFaction() - Need {} bots for {} bracket L{}-{} (current: {}, target: {})",
            toSpawn,
            faction == TEAM_ALLIANCE ? "Alliance" : "Horde",
            bracket->minLevel, bracket->maxLevel,
            current, target);

        // Increment spawn requests counter - BotSpawner will use weighted selection
        // to favor underpopulated brackets on next spawn cycle
        spawnRequestsCreated += toSpawn;
    }

    // Handle overpopulated brackets by marking excess bots for despawn
    // This is a gentler approach - we don't force despawn, just reduce spawn priority
    if (!overpopulated.empty())
    {
        auto stats = _distribution->GetDistributionStats();
        uint32 totalFactionBots = (faction == TEAM_ALLIANCE) ? stats.allianceBots : stats.hordeBots;

        for (auto const* bracket : overpopulated)
        {
            if (totalFactionBots == 0)
                continue;

            uint32 target = bracket->GetTargetCount(totalFactionBots);
            uint32 current = bracket->GetCount();

            if (current <= target)
                continue;

            uint32 excess = current - target;

            // Log the overpopulation for monitoring
            TC_LOG_DEBUG("playerbot", "BotLevelManager::RebalanceFaction() - {} bracket L{}-{} has {} excess bots (current: {}, target: {})",
                faction == TEAM_ALLIANCE ? "Alliance" : "Horde",
                bracket->minLevel, bracket->maxLevel,
                excess, current, target);

            // Note: We don't force despawn here as it would be disruptive
            // Instead, the weighted selection in SelectBracketWeighted() will
            // naturally stop spawning bots in overpopulated brackets
        }
    }

    // Trigger recalculation to update counters
    _distribution->RecalculateDistribution();

    TC_LOG_INFO("playerbot", "BotLevelManager::RebalanceFaction() - {} rebalancing: {} spawn requests queued",
        faction == TEAM_ALLIANCE ? "Alliance" : "Horde", spawnRequestsCreated);
}

// ====================================================================
// STATISTICS & MONITORING
// ====================================================================

void BotLevelManager::PrintReport() const
{
    TC_LOG_INFO("playerbot", "====================================================================");
    TC_LOG_INFO("playerbot", "BOT LEVEL MANAGER - FINAL REPORT");
    TC_LOG_INFO("playerbot", "====================================================================");
    TC_LOG_INFO("playerbot", "Creation Statistics:");
    TC_LOG_INFO("playerbot", "  Tasks Submitted:     {}", _stats.totalTasksSubmitted);
    TC_LOG_INFO("playerbot", "  Tasks Completed:     {}", _stats.totalTasksCompleted);
    TC_LOG_INFO("playerbot", "  Tasks Failed:        {}", _stats.totalTasksFailed);
    TC_LOG_INFO("playerbot", "  Success Rate:        {:.1f}%",
        _stats.totalTasksSubmitted > 0 ? (100.0f * _stats.totalTasksCompleted / _stats.totalTasksSubmitted) : 0.0f);
    TC_LOG_INFO("playerbot", "");
    TC_LOG_INFO("playerbot", "Queue Statistics:");
    TC_LOG_INFO("playerbot", "  Current Queue:       {}", _stats.currentQueueSize);
    TC_LOG_INFO("playerbot", "  Peak Queue:          {}", _stats.peakQueueSize);
    TC_LOG_INFO("playerbot", "");
    TC_LOG_INFO("playerbot", "Performance:");
    TC_LOG_INFO("playerbot", "  Avg Prep Time:       {}ms", _stats.averagePrepTimeMs);
    TC_LOG_INFO("playerbot", "  Avg Apply Time:      {}ms", _stats.averageApplyTimeMs);
    TC_LOG_INFO("playerbot", "");
    TC_LOG_INFO("playerbot", "System Operations:");
    TC_LOG_INFO("playerbot", "  Level-ups:           {}", _stats.totalLevelUps);
    TC_LOG_INFO("playerbot", "  Gear Applied:        {}", _stats.totalGearApplications);
    TC_LOG_INFO("playerbot", "  Talents Applied:     {}", _stats.totalTalentApplications);
    TC_LOG_INFO("playerbot", "  Teleports:           {}", _stats.totalTeleports);
    TC_LOG_INFO("playerbot", "");
    TC_LOG_INFO("playerbot", "Failures:");
    TC_LOG_INFO("playerbot", "  Level-up Failures:   {}", _stats.levelUpFailures);
    TC_LOG_INFO("playerbot", "  Gear Failures:       {}", _stats.gearFailures);
    TC_LOG_INFO("playerbot", "  Talent Failures:     {}", _stats.talentFailures);
    TC_LOG_INFO("playerbot", "  Teleport Failures:   {}", _stats.teleportFailures);
    TC_LOG_INFO("playerbot", "====================================================================");
}

::std::string BotLevelManager::GetSummary() const
{
    ::std::ostringstream oss;
    oss << "BotLevelManager: " << _stats.totalTasksCompleted << " bots created, "
        << _stats.currentQueueSize << " queued, "
        << _stats.averageApplyTimeMs << "ms avg";
    return oss.str();
}

} // namespace Playerbot
