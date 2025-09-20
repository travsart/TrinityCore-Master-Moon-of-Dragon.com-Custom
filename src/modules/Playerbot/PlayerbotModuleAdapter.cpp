/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "PlayerbotModuleAdapter.h"
#include "Modules/ModuleManager.h"
#include "Log.h"
#include "Config/PlayerbotConfig.h"
#include "Lifecycle/BotSpawner.h"
#include "Session/BotSessionMgr.h"

namespace Playerbot
{

bool PlayerbotModuleAdapter::s_registered = false;
bool PlayerbotModuleAdapter::s_initialized = false;

void PlayerbotModuleAdapter::RegisterWithModuleManager()
{
    if (s_registered)
    {
        TC_LOG_WARN("module.playerbot", "PlayerbotModuleAdapter: Already registered with ModuleManager");
        return;
    }

    TC_LOG_INFO("module.playerbot", "PlayerbotModuleAdapter: Registering with ModuleManager");

    ModuleManager::RegisterModule("Playerbot",
        OnModuleStartup,
        OnModuleUpdate,
        OnModuleShutdown);

    s_registered = true;
    TC_LOG_INFO("module.playerbot", "PlayerbotModuleAdapter: Successfully registered with ModuleManager");
}

void PlayerbotModuleAdapter::OnModuleStartup()
{
    TC_LOG_INFO("module.playerbot", "=== PlayerbotModuleAdapter::OnModuleStartup() CALLED ===");

    // Check if playerbot is enabled
    if (!sPlayerbotConfig)
    {
        TC_LOG_ERROR("module.playerbot", "PlayerbotModuleAdapter: sPlayerbotConfig is null during startup");
        return;
    }

    bool enabled = sPlayerbotConfig->GetBool("Playerbot.Enable", false);
    if (!enabled)
    {
        TC_LOG_INFO("module.playerbot", "PlayerbotModuleAdapter: Playerbot disabled (Playerbot.Enable = false)");
        return;
    }

    TC_LOG_INFO("module.playerbot", "PlayerbotModuleAdapter: Playerbot enabled - initializing systems");

    try
    {
        // Initialize bot systems now that everything is ready
        // This timing ensures the world is fully loaded and module is initialized

        TC_LOG_INFO("module.playerbot", "PlayerbotModuleAdapter: Starting bot spawning systems");

        s_initialized = true;
        TC_LOG_INFO("module.playerbot", "PlayerbotModuleAdapter: Startup completed successfully");
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("module.playerbot", "PlayerbotModuleAdapter: Startup failed: {}", e.what());
    }
}

void PlayerbotModuleAdapter::OnModuleUpdate(uint32 diff)
{
    if (!s_initialized)
        return;

    // Check if playerbot is enabled
    if (!sPlayerbotConfig || !sPlayerbotConfig->GetBool("Playerbot.Enable", false))
        return;

    static uint32 logCounter = 0;
    if (++logCounter % 100 == 0) // Log every 100 updates
    {
        TC_LOG_DEBUG("module.playerbot", "PlayerbotModuleAdapter::OnModuleUpdate() #{}", logCounter);
    }

    try
    {
        // Update BotSpawner for population management and character creation
        if (sBotSpawner)
        {
            sBotSpawner->Update(diff);
        }

        // Update BotSessionMgr for active bot management
        if (sBotSessionMgr)
        {
            sBotSessionMgr->UpdateAllSessions(diff);
        }
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("module.playerbot", "PlayerbotModuleAdapter: Update failed: {}", e.what());
    }
}

void PlayerbotModuleAdapter::OnModuleShutdown()
{
    if (!s_initialized)
        return;

    TC_LOG_INFO("module.playerbot", "PlayerbotModuleAdapter: Shutting down Playerbot systems");

    try
    {
        // Ensure clean shutdown of all bot systems
        if (sBotSpawner)
        {
            TC_LOG_INFO("module.playerbot", "PlayerbotModuleAdapter: Despawning all active bots");
            sBotSpawner->DespawnAllBots();
        }

        s_initialized = false;
        TC_LOG_INFO("module.playerbot", "PlayerbotModuleAdapter: Shutdown completed");
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("module.playerbot", "PlayerbotModuleAdapter: Shutdown failed: {}", e.what());
    }
}

} // namespace Playerbot