/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "PlayerbotWorldScript.h"
#include "ScriptMgr.h"
#include "World.h"
#include "GameTime.h"
#include "Log.h"
#include "Timer.h"
#include "Config/PlayerbotConfig.h"
#include "Lifecycle/BotSpawner.h"
#include "Session/BotSessionMgr.h"
#include "Session/BotWorldSessionMgr.h"
#include "Lifecycle/BotLifecycleMgr.h"

PlayerbotWorldScript::PlayerbotWorldScript() : WorldScript("PlayerbotWorldScript")
{
    TC_LOG_INFO("module.playerbot.script", "PlayerbotWorldScript constructor called - script registered");
}

void PlayerbotWorldScript::OnUpdate(uint32 diff)
{
    static bool initialized = false;
    static uint32 initRetryCount = 0;

    // Handle deferred initialization - check if Playerbot module is ready
    if (!initialized)
    {
        if (!IsPlayerbotEnabled())
        {
            // Module not ready yet, retry every 100 updates
            if (++initRetryCount % 100 == 0)
            {
                TC_LOG_DEBUG("module.playerbot.script", "PlayerbotWorldScript: Waiting for Playerbot module initialization (attempt {})", initRetryCount);
            }
            return;
        }

        // Module is now ready
        TC_LOG_INFO("module.playerbot.script", "PlayerbotWorldScript: Playerbot module initialized");
        initialized = true;

        // Initialize performance tracking
        _lastMetricUpdate = GameTime::GetGameTimeMS();
        _totalUpdateTime = 0;
        _updateCount = 0;
    }

    uint32 startTime = getMSTime();

    try
    {
        UpdateBotSystems(diff);
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("module.playerbot.script",
            "PlayerbotWorldScript::OnUpdate: Exception caught: {}", e.what());
    }

    uint32 updateTime = GetMSTimeDiffToNow(startTime);
    UpdateMetrics(updateTime);

    // Performance warning if update takes too long
    if (updateTime > 100) // 100ms threshold
    {
        TC_LOG_WARN("module.playerbot.script",
            "PlayerbotWorldScript::OnUpdate: Slow update detected - {}ms", updateTime);
    }
}

void PlayerbotWorldScript::OnConfigLoad(bool reload)
{
    if (!IsPlayerbotEnabled())
        return;

    TC_LOG_INFO("module.playerbot.script",
        "PlayerbotWorldScript::OnConfigLoad: {} configuration",
        reload ? "Reloading" : "Loading");

    if (reload)
    {
        // Handle dynamic configuration changes
        try
        {
            // Reload BotSpawner configuration
            Playerbot::sBotSpawner->LoadConfig();

            // Reload other module configurations as needed
            // Note: Full module restart may be required for some settings

            TC_LOG_INFO("module.playerbot.script", "Playerbot configuration reloaded successfully");
        }
        catch (std::exception const& e)
        {
            TC_LOG_ERROR("module.playerbot.script",
                "PlayerbotWorldScript::OnConfigLoad: Failed to reload configuration: {}", e.what());
        }
    }
}

void PlayerbotWorldScript::OnStartup()
{
    TC_LOG_INFO("module.playerbot.script", "PlayerbotWorldScript::OnStartup called");

    // Note: OnStartup is called before Playerbot module initialization,
    // so we defer enabling check to OnUpdate() when module is ready
    TC_LOG_INFO("module.playerbot.script", "PlayerbotWorldScript: Deferring initialization to OnUpdate (module loads later)");
}

void PlayerbotWorldScript::OnShutdownInitiate(ShutdownExitCode code, ShutdownMask mask)
{
    if (!IsPlayerbotEnabled())
        return;

    TC_LOG_INFO("module.playerbot.script", "PlayerbotWorldScript: Shutdown initiated (code: {}, mask: {})",
                static_cast<uint32>(code), static_cast<uint32>(mask));

    try
    {
        // Ensure clean shutdown of all bot systems
        if (Playerbot::sBotSpawner)
        {
            TC_LOG_INFO("module.playerbot.script", "Despawning all active bots for shutdown");
            Playerbot::sBotSpawner->DespawnAllBots();
        }

        // Log final performance metrics
        if (_updateCount > 0)
        {
            float avgUpdateTime = static_cast<float>(_totalUpdateTime) / _updateCount;
            TC_LOG_INFO("module.playerbot.script",
                "PlayerbotWorldScript: Final metrics - {} updates, {:.2f}ms average",
                _updateCount, avgUpdateTime);
        }
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("module.playerbot.script",
            "PlayerbotWorldScript::OnShutdownInitiate: Exception during shutdown: {}", e.what());
    }
}

void PlayerbotWorldScript::UpdateBotSystems(uint32 diff)
{
    static uint32 lastDebugLog = 0;
    uint32 currentTime = getMSTime();
    bool shouldLog = (currentTime - lastDebugLog > 5000); // Log every 5 seconds

    // Update BotSpawner for population management and character creation
    if (Playerbot::sBotSpawner)
    {
        try
        {
            Playerbot::sBotSpawner->Update(diff);
        }
        catch (std::exception const& e)
        {
            TC_LOG_ERROR("module.playerbot.script",
                "PlayerbotWorldScript::UpdateBotSystems: BotSpawner exception: {}", e.what());
        }
    }

    // ENTERPRISE FIX: Use BotWorldSessionMgr (correct system) instead of BotSessionMgr (legacy/unused)
    // Root Cause: Two competing session management systems exist:
    //   1. BotSessionMgr - Legacy system with empty _activeSessions (NOT used by BotSpawner)
    //   2. BotWorldSessionMgr - Active system with all bot sessions (used by BotSpawner)
    // BotSpawner calls sBotWorldSessionMgr->AddPlayerBot() which stores sessions in BotWorldSessionMgr
    // Therefore, we must call sBotWorldSessionMgr->UpdateSessions() to update those sessions
    if (Playerbot::sBotWorldSessionMgr)
    {
        if (shouldLog)
        {
            TC_LOG_INFO("module.playerbot.script", "🔄 UpdateBotSystems: Calling sBotWorldSessionMgr->UpdateSessions(), active bots: {}",
                        Playerbot::sBotWorldSessionMgr->GetBotCount());
        }
        try
        {
            Playerbot::sBotWorldSessionMgr->UpdateSessions(diff);
        }
        catch (std::exception const& e)
        {
            TC_LOG_ERROR("module.playerbot.script",
                "PlayerbotWorldScript::UpdateBotSystems: BotWorldSessionMgr exception: {}", e.what());
        }
    }
    else if (shouldLog)
    {
        TC_LOG_ERROR("module.playerbot.script", "❌ UpdateBotSystems: sBotWorldSessionMgr is NULL!");
    }

    if (shouldLog)
        lastDebugLog = currentTime;

    // Update BotLifecycleMgr for bot lifecycle management
    // Note: This may be commented out in current implementation
    /*
    if (Playerbot::sBotLifecycleMgr)
    {
        Playerbot::sBotLifecycleMgr->Update(diff);
    }
    */
}

bool PlayerbotWorldScript::IsPlayerbotEnabled() const
{
    // Check if playerbot module is enabled
    if (!sPlayerbotConfig)
    {
        TC_LOG_DEBUG("module.playerbot.script", "IsPlayerbotEnabled: sPlayerbotConfig is null - module not initialized yet");
        return false;
    }

    bool enabled = sPlayerbotConfig->GetBool("Playerbot.Enable", false);
    static bool loggedOnce = false;
    if (!loggedOnce)
    {
        TC_LOG_INFO("module.playerbot.script", "IsPlayerbotEnabled: Config reads Playerbot.Enable = {} (config loaded: {}), forcing TRUE for testing",
                    enabled ? "true" : "false", sPlayerbotConfig ? "yes" : "no");
        loggedOnce = true;
    }

    // TEMPORARY: Force enable for testing the WorldScript integration
    return true;
}

void PlayerbotWorldScript::UpdateMetrics(uint32 updateTime)
{
    _totalUpdateTime += updateTime;
    _updateCount++;

    uint32 currentTime = GameTime::GetGameTimeMS();
    if (currentTime - _lastMetricUpdate >= METRIC_UPDATE_INTERVAL)
    {
        if (_updateCount > 0)
        {
            float avgUpdateTime = static_cast<float>(_totalUpdateTime) / _updateCount;

            TC_LOG_DEBUG("module.playerbot.script",
                "PlayerbotWorldScript: Performance metrics - {} updates, {:.2f}ms average",
                _updateCount, avgUpdateTime);

            // Performance warning if average is too high
            if (avgUpdateTime > 50.0f) // 50ms average threshold
            {
                TC_LOG_WARN("module.playerbot.script",
                    "PlayerbotWorldScript: High average update time: {:.2f}ms", avgUpdateTime);
            }
        }

        // Reset metrics for next interval
        _lastMetricUpdate = currentTime;
        _totalUpdateTime = 0;
        _updateCount = 0;
    }
}

// Forward declaration for playerbot command script
void AddSC_playerbot_commandscript();

// Script registration function
void AddSC_playerbot_world()
{
    new PlayerbotWorldScript();

    // Register playerbot commands (module-only approach)
    #ifdef BUILD_PLAYERBOT
    AddSC_playerbot_commandscript();
    #endif
}