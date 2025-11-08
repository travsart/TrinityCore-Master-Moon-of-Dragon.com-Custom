/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 */

#pragma once

#include "ServiceContainer.h"
#include "Interfaces/ISpatialGridManager.h"
#include "Interfaces/IBotSessionMgr.h"
#include "Interfaces/IConfigManager.h"
#include "Interfaces/IBotLifecycleManager.h"
#include "Interfaces/IBotDatabasePool.h"
#include "Spatial/SpatialGridManager.h"
#include "Session/BotSessionMgr.h"
#include "Config/ConfigManager.h"
#include "Lifecycle/BotLifecycleManager.h"
#include "Database/BotDatabasePool.h"
#include "Log.h"

namespace Playerbot
{

/**
 * @brief Service registration for Playerbot module
 *
 * Registers all core services with the DI container at startup.
 * This enables dependency injection throughout the module.
 *
 * **Call this during module initialization (World::SetInitialWorldSettings)**
 *
 * Example:
 * @code
 * void World::SetInitialWorldSettings()
 * {
 *     // ... existing code ...
 *
 *     // Register Playerbot services
 *     Playerbot::RegisterPlayerbotServices();
 * }
 * @endcode
 */
inline void RegisterPlayerbotServices()
{
    TC_LOG_INFO("playerbot.di", "Registering Playerbot services with DI container...");

    try
    {
        auto& container = Services::Container();

        // Register SpatialGridManager
        container.RegisterInstance<ISpatialGridManager>(
            std::shared_ptr<ISpatialGridManager>(
                &SpatialGridManager::Instance(),
                [](ISpatialGridManager*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered ISpatialGridManager");

        // Register BotSessionMgr
        container.RegisterInstance<IBotSessionMgr>(
            std::shared_ptr<IBotSessionMgr>(
                BotSessionMgr::instance(),
                [](IBotSessionMgr*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IBotSessionMgr");

        // Register ConfigManager (Phase 2)
        container.RegisterInstance<IConfigManager>(
            std::shared_ptr<IConfigManager>(
                ConfigManager::instance(),
                [](IConfigManager*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IConfigManager");

        // Register BotLifecycleManager (Phase 2)
        container.RegisterInstance<IBotLifecycleManager>(
            std::shared_ptr<IBotLifecycleManager>(
                BotLifecycleManager::instance(),
                [](IBotLifecycleManager*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IBotLifecycleManager");

        // Register BotDatabasePool (Phase 2)
        container.RegisterInstance<IBotDatabasePool>(
            std::shared_ptr<IBotDatabasePool>(
                BotDatabasePool::instance(),
                [](IBotDatabasePool*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IBotDatabasePool");

        TC_LOG_INFO("playerbot.di", "Playerbot service registration complete. {} services registered.",
            container.GetServiceCount());
    }
    catch (std::exception const& e)
    {
        TC_LOG_FATAL("playerbot.di", "Failed to register Playerbot services: {}", e.what());
    }
}

/**
 * @brief Clear all registered services
 *
 * Useful for shutdown or testing cleanup.
 * Called during World::CleanupsBeforeStop()
 */
inline void UnregisterPlayerbotServices()
{
    TC_LOG_INFO("playerbot.di", "Unregistering Playerbot services...");
    Services::Container().Clear();
}

} // namespace Playerbot
