/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 */

#pragma once

#include "ServiceContainer.h"
#include "Interfaces/ISpatialGridManager.h"
#include "Interfaces/IBotSessionMgr.h"
#include "Spatial/SpatialGridManager.h"
#include "Session/BotSessionMgr.h"
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
