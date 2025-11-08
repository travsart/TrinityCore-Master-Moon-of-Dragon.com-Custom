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

#include "ServiceContainer.h"
#include "Interfaces/ISpatialGridManager.h"
#include "Interfaces/IBotSessionMgr.h"
#include "Interfaces/IConfigManager.h"
#include "Interfaces/IBotLifecycleManager.h"
#include "Interfaces/IBotDatabasePool.h"
#include "Interfaces/IBotNameMgr.h"
#include "Interfaces/IDungeonScriptMgr.h"
#include "Interfaces/IEquipmentManager.h"
#include "Interfaces/IBotAccountMgr.h"
#include "Interfaces/ILFGBotManager.h"
#include "Interfaces/IBotGearFactory.h"
#include "Interfaces/IBotMonitor.h"
#include "Interfaces/IBotLevelManager.h"
#include "Interfaces/IPlayerbotGroupManager.h"
#include "Interfaces/IBotTalentManager.h"
#include "Interfaces/IPlayerbotConfig.h"
#include "Interfaces/IBotSpawner.h"
#include "Interfaces/IBotWorldPositioner.h"
#include "Interfaces/IBotHealthCheck.h"
#include "Interfaces/IBotScheduler.h"
#include "Interfaces/IBotCharacterDistribution.h"
#include "Interfaces/IBotLevelDistribution.h"
#include "Spatial/SpatialGridManager.h"
#include "Session/BotSessionMgr.h"
#include "Config/ConfigManager.h"
#include "Lifecycle/BotLifecycleManager.h"
#include "Database/BotDatabasePool.h"
#include "Character/BotNameMgr.h"
#include "Dungeon/DungeonScriptMgr.h"
#include "Equipment/EquipmentManager.h"
#include "Account/BotAccountMgr.h"
#include "LFG/LFGBotManager.h"
#include "Equipment/BotGearFactory.h"
#include "Monitoring/BotMonitor.h"
#include "Character/BotLevelManager.h"
#include "Group/PlayerbotGroupManager.h"
#include "Talents/BotTalentManager.h"
#include "Config/PlayerbotConfig.h"
#include "Lifecycle/BotSpawner.h"
#include "Movement/BotWorldPositioner.h"
#include "Session/BotHealthCheck.h"
#include "Lifecycle/BotScheduler.h"
#include "Character/BotCharacterDistribution.h"
#include "Character/BotLevelDistribution.h"
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

        // Register BotNameMgr (Phase 3)
        container.RegisterInstance<IBotNameMgr>(
            std::shared_ptr<IBotNameMgr>(
                BotNameMgr::instance(),
                [](IBotNameMgr*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IBotNameMgr");

        // Register DungeonScriptMgr (Phase 3)
        container.RegisterInstance<IDungeonScriptMgr>(
            std::shared_ptr<IDungeonScriptMgr>(
                Playerbot::DungeonScriptMgr::instance(),
                [](IDungeonScriptMgr*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IDungeonScriptMgr");

        // Register EquipmentManager (Phase 4)
        container.RegisterInstance<IEquipmentManager>(
            std::shared_ptr<IEquipmentManager>(
                Playerbot::EquipmentManager::instance(),
                [](IEquipmentManager*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IEquipmentManager");

        // Register BotAccountMgr (Phase 4)
        container.RegisterInstance<IBotAccountMgr>(
            std::shared_ptr<IBotAccountMgr>(
                Playerbot::BotAccountMgr::instance(),
                [](IBotAccountMgr*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IBotAccountMgr");

        // Register LFGBotManager (Phase 5)
        container.RegisterInstance<ILFGBotManager>(
            std::shared_ptr<ILFGBotManager>(
                LFGBotManager::instance(),
                [](ILFGBotManager*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered ILFGBotManager");

        // Register BotGearFactory (Phase 5)
        container.RegisterInstance<IBotGearFactory>(
            std::shared_ptr<IBotGearFactory>(
                Playerbot::BotGearFactory::instance(),
                [](IBotGearFactory*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IBotGearFactory");

        // Register BotMonitor (Phase 6)
        container.RegisterInstance<IBotMonitor>(
            std::shared_ptr<IBotMonitor>(
                Playerbot::BotMonitor::instance(),
                [](IBotMonitor*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IBotMonitor");

        // Register BotLevelManager (Phase 6)
        container.RegisterInstance<IBotLevelManager>(
            std::shared_ptr<IBotLevelManager>(
                Playerbot::BotLevelManager::instance(),
                [](IBotLevelManager*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IBotLevelManager");

        // Register PlayerbotGroupManager (Phase 7)
        container.RegisterInstance<IPlayerbotGroupManager>(
            std::shared_ptr<IPlayerbotGroupManager>(
                Playerbot::PlayerbotGroupManager::instance(),
                [](IPlayerbotGroupManager*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IPlayerbotGroupManager");

        // Register BotTalentManager (Phase 7)
        container.RegisterInstance<IBotTalentManager>(
            std::shared_ptr<IBotTalentManager>(
                Playerbot::BotTalentManager::instance(),
                [](IBotTalentManager*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IBotTalentManager");

        // Register PlayerbotConfig (Phase 8)
        container.RegisterInstance<IPlayerbotConfig>(
            std::shared_ptr<IPlayerbotConfig>(
                PlayerbotConfig::instance(),
                [](IPlayerbotConfig*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IPlayerbotConfig");

        // Register BotSpawner (Phase 8)
        container.RegisterInstance<IBotSpawner>(
            std::shared_ptr<IBotSpawner>(
                Playerbot::BotSpawner::instance(),
                [](IBotSpawner*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IBotSpawner");

        // Register BotWorldPositioner (Phase 9)
        container.RegisterInstance<IBotWorldPositioner>(
            std::shared_ptr<IBotWorldPositioner>(
                Playerbot::BotWorldPositioner::instance(),
                [](IBotWorldPositioner*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IBotWorldPositioner");

        // Register BotHealthCheck (Phase 9)
        container.RegisterInstance<IBotHealthCheck>(
            std::shared_ptr<IBotHealthCheck>(
                Playerbot::BotHealthCheck::instance(),
                [](IBotHealthCheck*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IBotHealthCheck");

        // Register BotScheduler (Phase 10)
        container.RegisterInstance<IBotScheduler>(
            std::shared_ptr<IBotScheduler>(
                Playerbot::BotScheduler::instance(),
                [](IBotScheduler*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IBotScheduler");

        // Register BotCharacterDistribution (Phase 11)
        container.RegisterInstance<IBotCharacterDistribution>(
            std::shared_ptr<IBotCharacterDistribution>(
                Playerbot::BotCharacterDistribution::instance(),
                [](IBotCharacterDistribution*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IBotCharacterDistribution");

        // Register BotLevelDistribution (Phase 11)
        container.RegisterInstance<IBotLevelDistribution>(
            std::shared_ptr<IBotLevelDistribution>(
                Playerbot::BotLevelDistribution::instance(),
                [](IBotLevelDistribution*) {} // No-op deleter (singleton)
            )
        );
        TC_LOG_INFO("playerbot.di", "  - Registered IBotLevelDistribution");

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
