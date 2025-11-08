/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 */

#pragma once

#include "Define.h"
#include "ObjectGuid.h"
#include <memory>
#include <vector>
#include <functional>
#include <cstdint>

// Forward declarations
namespace Playerbot
{
    class BotLifecycle;
    class BotSession;
    enum class BotLifecycleState : uint8;
}

namespace Playerbot
{

/**
 * @brief Interface for Global Bot Lifecycle Management
 *
 * Abstracts bot lifecycle management to enable dependency injection and testing.
 *
 * **Responsibilities:**
 * - Create and manage bot lifecycle controllers
 * - Track all active bot lifecycles
 * - Update all bots globally
 * - Provide global statistics
 *
 * **Testability:**
 * - Can be mocked for testing lifecycle logic without real bots
 * - Enables isolated testing of lifecycle state machines
 *
 * Example:
 * @code
 * auto lifecycleMgr = Services::Container().Resolve<IBotLifecycleManager>();
 * auto lifecycle = lifecycleMgr->CreateBotLifecycle(botGuid, session);
 * lifecycle->Start();
 * @endcode
 */
class TC_GAME_API IBotLifecycleManager
{
public:
    /**
     * @brief Global statistics for all bots
     */
    struct GlobalStats
    {
        uint32 totalBots = 0;
        uint32 activeBots = 0;
        uint32 idleBots = 0;
        uint32 combatBots = 0;
        uint32 questingBots = 0;
        uint32 offlineBots = 0;
        float avgAiUpdateTime = 0.0f;
        size_t totalMemoryUsage = 0;
        uint32 totalActionsPerSecond = 0;
    };

    /**
     * @brief Lifecycle event handler callback type
     */
    using LifecycleEventHandler = std::function<void(ObjectGuid, BotLifecycleState, BotLifecycleState)>;

    virtual ~IBotLifecycleManager() = default;

    /**
     * @brief Create a new bot lifecycle
     * @param botGuid The bot's GUID
     * @param session The bot's session
     * @return Shared pointer to the lifecycle controller
     */
    virtual std::shared_ptr<BotLifecycle> CreateBotLifecycle(ObjectGuid botGuid, std::shared_ptr<BotSession> session) = 0;

    /**
     * @brief Remove a bot lifecycle
     * @param botGuid The bot's GUID
     */
    virtual void RemoveBotLifecycle(ObjectGuid botGuid) = 0;

    /**
     * @brief Get a bot's lifecycle controller
     * @param botGuid The bot's GUID
     * @return Shared pointer to lifecycle, nullptr if not found
     */
    virtual std::shared_ptr<BotLifecycle> GetBotLifecycle(ObjectGuid botGuid) const = 0;

    /**
     * @brief Get all active bot lifecycles
     * @return Vector of all active lifecycle controllers
     */
    virtual std::vector<std::shared_ptr<BotLifecycle>> GetActiveLifecycles() const = 0;

    /**
     * @brief Update all bot lifecycles
     * @param diff Time since last update in milliseconds
     */
    virtual void UpdateAll(uint32 diff) = 0;

    /**
     * @brief Stop all bots
     * @param immediate If true, stop immediately without cleanup
     */
    virtual void StopAll(bool immediate = false) = 0;

    /**
     * @brief Get global statistics
     * @return Global statistics for all bots
     */
    virtual GlobalStats GetGlobalStats() const = 0;

    /**
     * @brief Print performance report
     */
    virtual void PrintPerformanceReport() const = 0;

    /**
     * @brief Set maximum concurrent bot logins
     * @param max Maximum concurrent logins
     */
    virtual void SetMaxConcurrentLogins(uint32 max) = 0;

    /**
     * @brief Set bot update interval
     * @param intervalMs Update interval in milliseconds
     */
    virtual void SetUpdateInterval(uint32 intervalMs) = 0;

    /**
     * @brief Register for lifecycle events
     * @param handler Event handler callback
     */
    virtual void RegisterEventHandler(LifecycleEventHandler handler) = 0;
};

} // namespace Playerbot
