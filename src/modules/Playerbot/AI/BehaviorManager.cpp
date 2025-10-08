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

#include "BehaviorManager.h"
#include "BotAI.h"
#include "Player.h"
#include "Log.h"
#include "Timer.h"
#include <algorithm>
#include <set>
#include <unordered_map>

namespace Playerbot
{
    BehaviorManager::BehaviorManager(Player* bot, BotAI* ai, uint32 updateInterval, std::string managerName)
        : m_bot(bot)
        , m_ai(ai)
        , m_managerName(std::move(managerName))
        , m_updateInterval(std::max(updateInterval, 50u)) // Minimum 50ms interval
        , m_lastUpdate(0)
        , m_timeSinceLastUpdate(0)
    {
        // Validate input parameters
        if (!m_bot)
        {
            TC_LOG_ERROR("module.playerbot", "[{}] BehaviorManager created with null bot pointer!", m_managerName);
            m_enabled.store(false, std::memory_order_release);
            return;
        }

        if (!m_ai)
        {
            TC_LOG_ERROR("module.playerbot", "[{}] BehaviorManager created with null AI pointer for bot {}",
                         m_managerName, m_bot->GetName());
            m_enabled.store(false, std::memory_order_release);
            return;
        }

        // Initialize with current time to prevent immediate update
        m_lastUpdate = getMSTime();

        TC_LOG_DEBUG("module.playerbot", "[{}] Created for bot {} with {}ms update interval",
                     m_managerName, m_bot->GetName(), m_updateInterval);
    }

    void BehaviorManager::Update(uint32 diff)
    {
        // DEBUG: Only log for whitelisted test bots to prevent log spam
        static const std::set<std::string> testBots = {"Anderenz", "Boone", "Nelona", "Sevtap"};
        bool isTestBot = m_bot && (testBots.find(m_bot->GetName()) != testBots.end());

        // Per-bot debug log accumulator (only for test bots)
        static std::unordered_map<std::string, uint32> debugLogAccumulators;
        bool shouldLog = false;

        if (isTestBot)
        {
            std::string botName = m_bot->GetName();
            debugLogAccumulators[botName] += diff;

            // Log every 50 seconds per test bot
            if (debugLogAccumulators[botName] >= 50000)
            {
                shouldLog = true;
                debugLogAccumulators[botName] = 0;
            }
        }

        if (shouldLog)
        {
            TC_LOG_ERROR("module.playerbot", "🔍 [{}] Update() ENTRY: enabled={}, busy={}, bot={}, botInWorld={}",
                        m_managerName,
                        m_enabled.load(std::memory_order_acquire),
                        m_isBusy.load(std::memory_order_acquire),
                        (void*)m_bot,
                        m_bot ? m_bot->IsInWorld() : false);
        }

        // Fast path: Skip if disabled (atomic check, <0.001ms)
        if (!m_enabled.load(std::memory_order_acquire))
        {
            if (shouldLog)
                TC_LOG_ERROR("module.playerbot", "❌ [{}] DISABLED - returning early", m_managerName);
            return;
        }

        // Fast path: Skip if currently busy (prevents re-entrance)
        if (m_isBusy.load(std::memory_order_acquire))
        {
            if (shouldLog)
                TC_LOG_ERROR("module.playerbot", "⏳ [{}] BUSY - returning early", m_managerName);
            return;
        }

        // Validate pointers are still valid
        if (!ValidatePointers())
        {
            m_enabled.store(false, std::memory_order_release);
            TC_LOG_ERROR("module.playerbot", "❌ [{}] DISABLED due to ValidatePointers() returning false", m_managerName);
            return;
        }

        if (shouldLog)
            TC_LOG_ERROR("module.playerbot", "✅ [{}] ValidatePointers() passed", m_managerName);

        // Handle initialization on first update
        if (!m_initialized.load(std::memory_order_acquire))
        {
            if (!OnInitialize())
            {
                TC_LOG_DEBUG("module.playerbot", "[{}] Initialization pending for bot {}",
                            m_managerName, m_bot->GetName());
                return;
            }

            m_initialized.store(true, std::memory_order_release);
            m_lastUpdate = getMSTime();
            TC_LOG_DEBUG("module.playerbot", "[{}] Initialized successfully for bot {}",
                        m_managerName, m_bot->GetName());
        }

        // Accumulate time since last update
        m_timeSinceLastUpdate += diff;

        // Check if we should perform update (throttling check)
        bool shouldUpdate = false;

        // Priority 1: Force update requested
        if (m_forceUpdate.exchange(false, std::memory_order_acq_rel))
        {
            shouldUpdate = true;
            TC_LOG_DEBUG("module.playerbot", "[{}] Forced update for bot {}",
                        m_managerName, m_bot->GetName());
        }
        // Priority 2: Enough time has elapsed
        else if (m_timeSinceLastUpdate >= m_updateInterval)
        {
            shouldUpdate = true;
        }
        // Priority 3: Manager indicates it needs immediate update
        else if (m_needsUpdate.load(std::memory_order_acquire))
        {
            shouldUpdate = true;
            m_needsUpdate.store(false, std::memory_order_release);
        }

        // Fast return if no update needed (<0.001ms when throttled)
        if (!shouldUpdate)
            return;

        // Perform the actual update
        DoUpdate(m_timeSinceLastUpdate);

        // Reset accumulated time
        m_timeSinceLastUpdate = 0;
    }

    void BehaviorManager::DoUpdate(uint32 elapsed)
    {
        // Mark as busy (prevents re-entrance)
        m_isBusy.store(true, std::memory_order_release);

        // Performance monitoring - capture start time
        uint32 startTime = getMSTime();

        try
        {
            // Call derived class implementation
            OnUpdate(elapsed);

            // Increment update counter
            m_updateCount.fetch_add(1, std::memory_order_acq_rel);
        }
        catch (const std::exception& e)
        {
            TC_LOG_ERROR("module.playerbot", "[{}] Exception in OnUpdate for bot {}: {}",
                        m_managerName, m_bot->GetName(), e.what());
            // Disable manager after exception to prevent spam
            m_enabled.store(false, std::memory_order_release);
        }
        catch (...)
        {
            TC_LOG_ERROR("module.playerbot", "[{}] Unknown exception in OnUpdate for bot {}",
                        m_managerName, m_bot->GetName());
            // Disable manager after exception to prevent spam
            m_enabled.store(false, std::memory_order_release);
        }

        // Calculate update duration
        uint32 updateDuration = getMSTimeDiff(startTime, getMSTime());

        // Performance monitoring - check for slow updates
        if (updateDuration > m_slowUpdateThreshold)
        {
            m_consecutiveSlowUpdates++;
            m_totalSlowUpdates++;

            // Log warning for slow updates
            if (m_consecutiveSlowUpdates == 1)
            {
                TC_LOG_DEBUG("module.playerbot", "[{}] Slow update detected for bot {}: {}ms (threshold: {}ms)",
                            m_managerName, m_bot->GetName(), updateDuration, m_slowUpdateThreshold);
            }
            else if (m_consecutiveSlowUpdates >= 5)
            {
                TC_LOG_WARN("module.playerbot", "[{}] {} consecutive slow updates for bot {} (latest: {}ms)",
                           m_managerName, m_consecutiveSlowUpdates, m_bot->GetName(), updateDuration);
            }

            // Auto-adjust update interval if consistently slow
            if (m_consecutiveSlowUpdates >= 10 && m_updateInterval < 5000)
            {
                uint32 newInterval = std::min(m_updateInterval * 2, 5000u);
                TC_LOG_INFO("module.playerbot", "[{}] Auto-adjusting update interval from {}ms to {}ms for bot {} due to performance",
                           m_managerName, m_updateInterval, newInterval, m_bot->GetName());
                m_updateInterval = newInterval;
                m_consecutiveSlowUpdates = 0;
            }
        }
        else
        {
            // Reset consecutive slow update counter on fast update
            if (m_consecutiveSlowUpdates > 0)
            {
                TC_LOG_DEBUG("module.playerbot", "[{}] Performance recovered for bot {} after {} slow updates",
                            m_managerName, m_bot->GetName(), m_consecutiveSlowUpdates);
                m_consecutiveSlowUpdates = 0;
            }
        }

        // Update last update timestamp
        m_lastUpdate = getMSTime();

        // Clear busy flag
        m_isBusy.store(false, std::memory_order_release);
    }

    void BehaviorManager::SetUpdateInterval(uint32 interval)
    {
        // Clamp to reasonable range (50ms to 60000ms)
        m_updateInterval = std::clamp(interval, 50u, 60000u);

        TC_LOG_DEBUG("module.playerbot", "[{}] Update interval changed to {}ms for bot {}",
                    m_managerName, m_updateInterval, m_bot ? m_bot->GetName() : "unknown");
    }

    uint32 BehaviorManager::GetTimeSinceLastUpdate() const
    {
        if (m_lastUpdate == 0)
            return 0;

        return getMSTimeDiff(m_lastUpdate, getMSTime());
    }

    bool BehaviorManager::ValidatePointers() const
    {
        // DEBUG LOGGING THROTTLE: Only log for test bots every 50 seconds
        static const std::set<std::string> testBots = {"Anderenz", "Boone", "Nelona", "Sevtap"};
        static std::unordered_map<std::string, uint32> validateLogAccumulators;

        bool isTestBot = m_bot && (testBots.find(m_bot->GetName()) != testBots.end());
        bool shouldLog = false;

        if (isTestBot)
        {
            std::string botName = m_bot->GetName();
            // Note: We can't use diff here, so we throttle by call count instead (every 1000 calls ~= 50s)
            validateLogAccumulators[botName]++;
            if (validateLogAccumulators[botName] >= 1000)
            {
                shouldLog = true;
                validateLogAccumulators[botName] = 0;
            }
        }

        // Check bot pointer validity
        if (!m_bot)
        {
            TC_LOG_ERROR("module.playerbot", "❌ [{}] ValidatePointers FAILED: Bot pointer is null", m_managerName);
            return false;
        }

        // Check if bot is in world
        if (!m_bot->IsInWorld())
        {
            if (shouldLog)
            {
                TC_LOG_ERROR("module.playerbot", "❌ [{}] ValidatePointers FAILED: Bot {} IsInWorld()=false (THIS IS THE PROBLEM!)",
                            m_managerName, m_bot->GetName());
            }
            return false;
        }

        // Check AI pointer validity
        if (!m_ai)
        {
            TC_LOG_ERROR("module.playerbot", "❌ [{}] ValidatePointers FAILED: AI pointer is null for bot {}", m_managerName, m_bot->GetName());
            return false;
        }

        if (shouldLog)
        {
            TC_LOG_ERROR("module.playerbot", "✅ [{}] ValidatePointers PASSED: Bot {} is valid and in world", m_managerName, m_bot->GetName());
        }

        return true;
    }

    // Template specializations for logging
    template<>
    void BehaviorManager::LogDebug(const char* format) const
    {
        TC_LOG_DEBUG("module.playerbot", "[{}] {}", m_managerName, format);
    }

    template<typename... Args>
    void BehaviorManager::LogDebug(const char* format, Args... args) const
    {
        std::string message = fmt::format(format, args...);
        TC_LOG_DEBUG("module.playerbot", "[{}] {}", m_managerName, message);
    }

    template<>
    void BehaviorManager::LogWarning(const char* format) const
    {
        TC_LOG_WARN("module.playerbot", "[{}] {}", m_managerName, format);
    }

    template<typename... Args>
    void BehaviorManager::LogWarning(const char* format, Args... args) const
    {
        std::string message = fmt::format(format, args...);
        TC_LOG_WARN("module.playerbot", "[{}] {}", m_managerName, message);
    }

    // ============================================================================
    // PHASE 7.1: IManagerBase Interface Implementation
    // ============================================================================

    bool BehaviorManager::Initialize()
    {
        // Validate pointers before initialization
        if (!ValidatePointers())
        {
            TC_LOG_ERROR("module.playerbot", "[{}] Initialize() failed: Invalid bot or AI pointers", m_managerName);
            return false;
        }

        // Call derived class initialization
        bool success = OnInitialize();

        if (success)
        {
            m_initialized.store(true, std::memory_order_release);
            TC_LOG_INFO("module.playerbot", "[{}] Initialized successfully for bot {}",
                        m_managerName, m_bot->GetName());
        }
        else
        {
            TC_LOG_ERROR("module.playerbot", "[{}] OnInitialize() failed for bot {}",
                         m_managerName, m_bot->GetName());
        }

        return success;
    }

    void BehaviorManager::Shutdown()
    {
        // Disable further updates
        m_enabled.store(false, std::memory_order_release);

        // Call derived class shutdown
        OnShutdown();

        m_initialized.store(false, std::memory_order_release);

        TC_LOG_INFO("module.playerbot", "[{}] Shutdown complete for bot {}",
                    m_managerName, m_bot ? m_bot->GetName() : "Unknown");
    }

    void BehaviorManager::OnEvent(Events::BotEvent const& event)
    {
        // Delegate to the virtual OnEventInternal method
        // Derived classes override OnEventInternal() to handle events
        OnEventInternal(event);
    }

} // namespace Playerbot