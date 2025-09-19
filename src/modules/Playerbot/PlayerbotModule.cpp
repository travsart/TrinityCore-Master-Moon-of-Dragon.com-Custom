/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifdef PLAYERBOT_ENABLED

#include "PlayerbotModule.h"
#include "Config/PlayerbotConfig.h"
#include "Account/BotAccountMgr.h"
#include "Character/BotNameMgr.h"
#include "Database/PlayerbotDatabase.h"
#include "Database/PlayerbotMigrationMgr.h"
#include "Lifecycle/BotLifecycleMgr.h"
#include "Log.h"
#include "GitRevision.h"

// Module state
bool PlayerbotModule::_initialized = false;
bool PlayerbotModule::_enabled = false;
std::string PlayerbotModule::_lastError = "";

bool PlayerbotModule::Initialize()
{
    TC_LOG_INFO("server.loading", "Initializing Playerbot Module...");

    // Load configuration first
    if (!sPlayerbotConfig->Initialize())
    {
        _lastError = "Failed to load playerbot configuration";
        TC_LOG_ERROR("server.loading", "Playerbot Module: {}", _lastError);
        return false;
    }

    // Check if playerbot is enabled
    if (!sPlayerbotConfig->GetBool("Playerbot.Enable", false))
    {
        TC_LOG_INFO("server.loading", "Playerbot Module: Disabled in configuration");
        _initialized = true;
        _enabled = false;
        return true;
    }

    // Validate configuration
    if (!ValidateConfig())
    {
        _lastError = "Configuration validation failed";
        TC_LOG_ERROR("server.loading", "Playerbot Module: {}", _lastError);
        return false;
    }

    // Initialize logging
    InitializeLogging();

    // Initialize Playerbot Database
    TC_LOG_INFO("server.loading", "Initializing Playerbot Database...");
    if (!InitializeDatabase())
    {
        _lastError = "Failed to initialize Playerbot Database";
        TC_LOG_ERROR("server.loading", "Playerbot Module: {}", _lastError);
        return false;
    }

    // Initialize Migration Manager FIRST (before any database access)
    TC_LOG_INFO("server.loading", "Initializing Database Migration Manager...");
    if (!PlayerbotMigrationMgr::instance()->Initialize())
    {
        _lastError = "Failed to initialize Migration Manager";
        TC_LOG_ERROR("server.loading", "Playerbot Module: {}", _lastError);
        return false;
    }

    // Apply pending migrations
    if (!PlayerbotMigrationMgr::instance()->ApplyMigrations())
    {
        _lastError = "Failed to apply database migrations";
        TC_LOG_ERROR("server.loading", "Playerbot Module: {}", _lastError);
        return false;
    }

    // Initialize Bot Account Manager
    TC_LOG_INFO("server.loading", "Initializing Bot Account Manager...");
    if (!sBotAccountMgr->Initialize())
    {
        _lastError = "Failed to initialize Bot Account Manager";
        TC_LOG_ERROR("server.loading", "Playerbot Module: {}", _lastError);
        
        // Check if strict mode is enabled
        if (sPlayerbotConfig->GetBool("Playerbot.StrictCharacterLimit", true))
        {
            TC_LOG_FATAL("server.loading", 
                "Playerbot Module: Bot accounts exceed character limit! "
                "Server startup aborted. Set Playerbot.StrictCharacterLimit = 0 to allow startup.");
            return false;
        }
    }

    // Initialize Bot Name Manager
    TC_LOG_INFO("server.loading", "Initializing Bot Name Manager...");
    if (!sBotNameMgr->Initialize())
    {
        _lastError = "Failed to initialize Bot Name Manager";
        TC_LOG_ERROR("server.loading", "Playerbot Module: {}", _lastError);
        return false;
    }

    // Initialize Bot Lifecycle Manager
    TC_LOG_INFO("server.loading", "Initializing Bot Lifecycle Manager...");
    if (!BotLifecycleMgr::instance()->Initialize())
    {
        _lastError = "Failed to initialize Bot Lifecycle Manager";
        TC_LOG_ERROR("server.loading", "Playerbot Module: {}", _lastError);
        return false;
    }

    // Register hooks with TrinityCore
    RegisterHooks();

    _initialized = true;
    _enabled = true;

    TC_LOG_INFO("server.loading", "Playerbot Module: Successfully initialized (Version {})", GetVersion());
    TC_LOG_INFO("server.loading", "  - {} bot accounts loaded", sBotAccountMgr->GetTotalBotAccounts());
    TC_LOG_INFO("server.loading", "  - {} names available ({} used)", 
        sBotNameMgr->GetTotalNameCount(), sBotNameMgr->GetUsedNameCount());
    
    return true;
}

void PlayerbotModule::Shutdown()
{
    if (!_initialized)
        return;

    TC_LOG_INFO("server.loading", "Shutting down Playerbot Module...");

    // Unregister hooks
    UnregisterHooks();

    if (_enabled)
    {
        // Shutdown Bot Lifecycle Manager
        TC_LOG_INFO("server.loading", "Shutting down Bot Lifecycle Manager...");
        BotLifecycleMgr::instance()->Shutdown();

        // Shutdown Bot Name Manager
        TC_LOG_INFO("server.loading", "Shutting down Bot Name Manager...");
        sBotNameMgr->Shutdown();

        // Shutdown Bot Account Manager
        TC_LOG_INFO("server.loading", "Shutting down Bot Account Manager...");
        sBotAccountMgr->Shutdown();

        // Shutdown Playerbot Database
        TC_LOG_INFO("server.loading", "Shutting down Playerbot Database...");
        ShutdownDatabase();
    }

    // Mark as shutdown
    _initialized = false;
    _enabled = false;

    TC_LOG_INFO("server.loading", "Playerbot Module: Shutdown complete");
}

bool PlayerbotModule::IsEnabled()
{
    return _initialized && _enabled;
}

std::string PlayerbotModule::GetVersion()
{
    return Trinity::StringFormat("{}.{}.{}", MODULE_VERSION_MAJOR, MODULE_VERSION_MINOR, MODULE_VERSION_PATCH);
}

std::string PlayerbotModule::GetBuildInfo()
{
    return Trinity::StringFormat("Playerbot Module {} (Built: {} {})",
        GetVersion(), __DATE__, __TIME__);
}

void PlayerbotModule::RegisterHooks()
{
    // TODO: Register event hooks with TrinityCore when needed
    // Examples:
    // - Player login/logout events
    // - World update cycles
    // - Configuration reload events
}

void PlayerbotModule::UnregisterHooks()
{
    // TODO: Unregister event hooks
}

bool PlayerbotModule::ValidateConfig()
{
    // Basic configuration validation
    if (!sPlayerbotConfig)
    {
        _lastError = "Configuration system not initialized";
        return false;
    }

    // Validate bot count limits
    uint32 maxBots = sPlayerbotConfig->GetInt("Playerbot.MaxBotsPerAccount", 10);
    if (maxBots < 1 || maxBots > 50)
    {
        _lastError = Trinity::StringFormat("Playerbot.MaxBotsPerAccount invalid ({}), must be between 1-50", maxBots);
        TC_LOG_WARN("server.loading", "Playerbot Module: {}", _lastError);
        return false;
    }

    // Validate character limit per account - WICHTIG: MAX 10!
    uint32 maxChars = sPlayerbotConfig->GetInt("Playerbot.MaxCharactersPerAccount", 10);
    if (maxChars < 1 || maxChars > 10)
    {
        _lastError = Trinity::StringFormat("Playerbot.MaxCharactersPerAccount invalid ({}), must be between 1-10", maxChars);
        TC_LOG_WARN("server.loading", "Playerbot Module: {}", _lastError);
        return false;
    }

    // Validate update intervals
    uint32 updateMs = sPlayerbotConfig->GetInt("Playerbot.UpdateInterval", 1000);
    if (updateMs < 100)
    {
        _lastError = "Playerbot.UpdateInterval too low (minimum 100ms)";
        TC_LOG_WARN("server.loading", "Playerbot Module: {}", _lastError);
        return false;
    }

    return true;
}

void PlayerbotModule::InitializeLogging()
{
    // Initialize specialized logging channels for playerbot
    // These use TrinityCore's existing logging system with module-specific categories

    // Log categories are configured in Logger.conf:
    // - module.playerbot (general module logging)
    // - module.playerbot.ai (AI decision logging)
    // - module.playerbot.performance (performance metrics)
    // - module.playerbot.account (account management)
    // - module.playerbot.character (character management)
    // - module.playerbot.names (name allocation)

    TC_LOG_DEBUG("module.playerbot", "Playerbot logging initialized");
}

bool PlayerbotModule::InitializeDatabase()
{
    // Get individual database settings from configuration
    std::string host = sPlayerbotConfig->GetString("Playerbot.Database.Host", "127.0.0.1");
    uint32 port = sPlayerbotConfig->GetInt("Playerbot.Database.Port", 3306);
    std::string user = sPlayerbotConfig->GetString("Playerbot.Database.User", "trinity");
    std::string password = sPlayerbotConfig->GetString("Playerbot.Database.Password", "trinity");
    std::string database = sPlayerbotConfig->GetString("Playerbot.Database.Name", "characters");

    // Construct connection string in format: hostname;port;username;password;database
    std::string dbString = Trinity::StringFormat("{};{};{};{};{}", host, port, user, password, database);

    TC_LOG_INFO("server.loading", "Playerbot Database: Connecting to {}:{}/{}", host, port, database);

    // Initialize database connection using our custom manager
    if (!sPlayerbotDatabase->Initialize(dbString))
    {
        TC_LOG_ERROR("server.loading", "Playerbot Database: Failed to initialize connection");
        return false;
    }

    TC_LOG_INFO("server.loading", "Playerbot Database: Successfully connected");
    return true;
}

void PlayerbotModule::ShutdownDatabase()
{
    TC_LOG_DEBUG("module.playerbot", "Closing Playerbot Database connection");
    sPlayerbotDatabase->Close();
    TC_LOG_DEBUG("module.playerbot", "Playerbot Database connection closed");
}

#endif // PLAYERBOT_ENABLED
