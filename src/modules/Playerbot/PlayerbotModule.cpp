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
#include "Character/BotCharacterDistribution.h"
#include "Database/PlayerbotDatabase.h"
#include "Database/PlayerbotMigrationMgr.h"
#include "Lifecycle/BotSpawner.h"
// #include "Lifecycle/BotLifecycleMgr.h"
#include "Session/BotSessionMgr.h"
#include "PlayerbotModuleAdapter.h"
#include "Log.h"
#include "GitRevision.h"

// Module state
bool PlayerbotModule::_initialized = false;
bool PlayerbotModule::_enabled = false;
std::string PlayerbotModule::_lastError = "";

bool PlayerbotModule::Initialize()
{
    // CRITICAL DEBUG: Using printf to ensure visibility
    printf("=== PLAYERBOT DEBUG: PlayerbotModule::Initialize() ENTERED ===\n");
    TC_LOG_INFO("server.loading", "Initializing Playerbot Module...");

    // Load configuration first
    printf("=== PLAYERBOT DEBUG: About to call sPlayerbotConfig->Initialize() ===\n");
    if (!sPlayerbotConfig->Initialize())
    {
        _lastError = "Failed to load playerbot configuration";
        TC_LOG_ERROR("server.loading", "Playerbot Module: {}", _lastError);
        printf("=== PLAYERBOT DEBUG: sPlayerbotConfig->Initialize() FAILED ===\n");
        return false;
    }
    printf("=== PLAYERBOT DEBUG: sPlayerbotConfig->Initialize() SUCCESS ===\n");

    // Check if playerbot is enabled
    printf("=== PLAYERBOT DEBUG: About to check Playerbot.Enable ===\n");
    if (!sPlayerbotConfig->GetBool("Playerbot.Enable", false))
    {
        TC_LOG_INFO("server.loading", "Playerbot Module: Disabled in configuration");
        printf("=== PLAYERBOT DEBUG: Playerbot.Enable = false, returning true but disabled ===\n");
        _initialized = true;
        _enabled = false;
        return true;
    }
    printf("=== PLAYERBOT DEBUG: Playerbot.Enable = true, continuing initialization ===\n");

    // Validate configuration
    printf("=== PLAYERBOT DEBUG: About to call ValidateConfig() ===\n");
    if (!ValidateConfig())
    {
        _lastError = "Configuration validation failed";
        TC_LOG_ERROR("server.loading", "Playerbot Module: {}", _lastError);
        printf("=== PLAYERBOT DEBUG: ValidateConfig() FAILED ===\n");
        return false;
    }
    printf("=== PLAYERBOT DEBUG: ValidateConfig() SUCCESS ===\n");

    // Initialize logging
    printf("=== PLAYERBOT DEBUG: About to call InitializeLogging() ===\n");
    fflush(stdout);
    InitializeLogging();
    printf("=== PLAYERBOT DEBUG: InitializeLogging() completed ===\n");
    fflush(stdout);

    // Initialize Playerbot Database
    printf("=== PLAYERBOT DEBUG: About to call InitializeDatabase() ===\n");
    fflush(stdout);
    if (!InitializeDatabase())
    {
        _lastError = "Failed to initialize Playerbot Database";
        printf("=== PLAYERBOT DEBUG: InitializeDatabase() FAILED - returning false ===\n");
        return false;
    }
    printf("=== PLAYERBOT DEBUG: InitializeDatabase() SUCCESS ===\n");

    // Initialize Migration Manager FIRST (before any database access)
    printf("=== PLAYERBOT DEBUG: About to call PlayerbotMigrationMgr::Initialize() ===\n");
    if (!PlayerbotMigrationMgr::instance()->Initialize())
    {
        _lastError = "Failed to initialize Migration Manager";
        printf("=== PLAYERBOT DEBUG: PlayerbotMigrationMgr::Initialize() FAILED ===\n");
        return false;
    }
    printf("=== PLAYERBOT DEBUG: PlayerbotMigrationMgr::Initialize() SUCCESS ===\n");

    // Apply pending migrations
    printf("=== PLAYERBOT DEBUG: About to call ApplyMigrations() ===\n");
    if (!PlayerbotMigrationMgr::instance()->ApplyMigrations())
    {
        _lastError = "Failed to apply database migrations";
        printf("=== PLAYERBOT DEBUG: ApplyMigrations() FAILED ===\n");
        return false;
    }
    printf("=== PLAYERBOT DEBUG: ApplyMigrations() SUCCESS ===\n");

    // Initialize Bot Account Manager
    printf("=== PLAYERBOT DEBUG: About to call sBotAccountMgr->Initialize() ===\n");
    if (!sBotAccountMgr->Initialize())
    {
        _lastError = "Failed to initialize Bot Account Manager";
        printf("=== PLAYERBOT DEBUG: sBotAccountMgr->Initialize() FAILED ===\n");

        // Check if strict mode is enabled
        if (sPlayerbotConfig->GetBool("Playerbot.StrictCharacterLimit", true))
        {
            printf("=== PLAYERBOT DEBUG: StrictCharacterLimit enabled - returning false ===\n");
            return false;
        }
    }
    printf("=== PLAYERBOT DEBUG: sBotAccountMgr->Initialize() SUCCESS ===\n");

    // Initialize Bot Name Manager
    printf("=== PLAYERBOT DEBUG: About to call sBotNameMgr->Initialize() ===\n");
    if (!sBotNameMgr->Initialize())
    {
        _lastError = "Failed to initialize Bot Name Manager";
        printf("=== PLAYERBOT DEBUG: sBotNameMgr->Initialize() FAILED ===\n");
        return false;
    }
    printf("=== PLAYERBOT DEBUG: sBotNameMgr->Initialize() SUCCESS ===\n");

    // Initialize Bot Character Distribution
    printf("=== PLAYERBOT DEBUG: About to call sBotCharacterDistribution->LoadFromDatabase() ===\n");
    if (!sBotCharacterDistribution->LoadFromDatabase())
    {
        _lastError = "Failed to initialize Bot Character Distribution";
        printf("=== PLAYERBOT DEBUG: sBotCharacterDistribution->LoadFromDatabase() FAILED ===\n");
        return false;
    }
    printf("=== PLAYERBOT DEBUG: sBotCharacterDistribution->LoadFromDatabase() SUCCESS ===\n");

    // Initialize Bot Session Manager
    printf("=== PLAYERBOT DEBUG: About to call sBotSessionMgr->Initialize() ===\n");
    if (!sBotSessionMgr->Initialize())
    {
        _lastError = "Failed to initialize Bot Session Manager";
        printf("=== PLAYERBOT DEBUG: sBotSessionMgr->Initialize() FAILED ===\n");
        return false;
    }
    printf("=== PLAYERBOT DEBUG: sBotSessionMgr->Initialize() SUCCESS ===\n");

    // Initialize Bot Spawner
    printf("=== PLAYERBOT DEBUG: About to call Playerbot::sBotSpawner->Initialize() ===\n");
    if (!Playerbot::sBotSpawner->Initialize())
    {
        _lastError = "Failed to initialize Bot Spawner";
        printf("=== PLAYERBOT DEBUG: Playerbot::sBotSpawner->Initialize() FAILED ===\n");
        return false;
    }
    printf("=== PLAYERBOT DEBUG: Playerbot::sBotSpawner->Initialize() SUCCESS ===\n");

    // Initialize Bot Lifecycle Manager
    //     TC_LOG_INFO("server.loading", "Initializing Bot Lifecycle Manager...");
    //     if (!BotLifecycleMgr::instance()->Initialize())
    //     {
    //         _lastError = "Failed to initialize Bot Lifecycle Manager";
    //         TC_LOG_ERROR("server.loading", "Playerbot Module: {}", _lastError);
    //         return false;
    //     }
    // 
    // Register hooks with TrinityCore
    printf("=== PLAYERBOT DEBUG: About to call RegisterHooks() ===\n");
    fflush(stdout);
    RegisterHooks();
    printf("=== PLAYERBOT DEBUG: RegisterHooks() completed successfully ===\n");
    fflush(stdout);

    _initialized = true;
    _enabled = true;

    printf("=== PLAYERBOT DEBUG: INITIALIZATION COMPLETE - ALL SYSTEMS READY! ===\n");
    printf("=== PLAYERBOT DEBUG: Initialize() about to return true ===\n");
    fflush(stdout);

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
        //         TC_LOG_INFO("server.loading", "Shutting down Bot Lifecycle Manager...");
        //         BotLifecycleMgr::instance()->Shutdown();
        // 
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
    // Register with ModuleManager for reliable lifecycle management
    printf("=== PLAYERBOT DEBUG: Registering Playerbot with ModuleManager... ===\n");
    fflush(stdout);
    Playerbot::PlayerbotModuleAdapter::RegisterWithModuleManager();
    printf("=== PLAYERBOT DEBUG: Playerbot successfully registered with ModuleManager ===\n");
    fflush(stdout);
}

void PlayerbotModule::UnregisterHooks()
{
    // TODO: Unregister event hooks
}

void PlayerbotModule::OnWorldUpdate(uint32 diff)
{
    static uint32 logCounter = 0;
    if (++logCounter % 1000 == 0) // Log every 1000 calls
    {
        printf("=== PLAYERBOT DEBUG: PlayerbotModule::OnWorldUpdate() called #%u (enabled=%s, initialized=%s) ===\n",
               logCounter, _enabled ? "true" : "false", _initialized ? "true" : "false");
    }

    if (!_enabled || !_initialized)
        return;

    if (logCounter % 1000 == 0) // Same counter to log when BotSpawner is about to be called
    {
        printf("=== PLAYERBOT DEBUG: About to call Playerbot::sBotSpawner->Update(diff=%u) ===\n", diff);
    }

    // Update BotSpawner for automatic character creation and management
    // RE-ENABLED: Fixed TC_LOG calls that were causing crash
    printf("=== PLAYERBOT DEBUG: About to call sBotSpawner->Update() (TC_LOG fixed) ===\n");
    fflush(stdout);
    Playerbot::sBotSpawner->Update(diff);

    if (logCounter % 1000 == 0)
    {
        printf("=== PLAYERBOT DEBUG: Finished calling Playerbot::sBotSpawner->Update() ===\n");
    }
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
    printf("=== PLAYERBOT DEBUG: InitializeLogging() ENTERED ===\n");
    fflush(stdout);

    // TEMPORARILY REMOVED: TC_LOG_INFO calls that might be causing crashes
    printf("=== PLAYERBOT DEBUG: Skipping first TC_LOG_INFO call ===\n");
    fflush(stdout);

    // Initialize TrinityCore logging integration via config system
    printf("=== PLAYERBOT DEBUG: About to call sPlayerbotConfig->InitializeLogging() ===\n");
    fflush(stdout);
    sPlayerbotConfig->InitializeLogging();
    printf("=== PLAYERBOT DEBUG: sPlayerbotConfig->InitializeLogging() completed ===\n");
    fflush(stdout);

    // TEMPORARILY REMOVED: Second TC_LOG_INFO call
    printf("=== PLAYERBOT DEBUG: Skipping second TC_LOG_INFO call ===\n");
    fflush(stdout);
    printf("=== PLAYERBOT DEBUG: InitializeLogging() EXITING ===\n");
    fflush(stdout);
}

bool PlayerbotModule::InitializeDatabase()
{
    printf("=== PLAYERBOT DEBUG: InitializeDatabase() ENTERED ===\n");

    // Get individual database settings from configuration
    std::string host = sPlayerbotConfig->GetString("Playerbot.Database.Host", "127.0.0.1");
    uint32 port = sPlayerbotConfig->GetInt("Playerbot.Database.Port", 3306);
    std::string user = sPlayerbotConfig->GetString("Playerbot.Database.User", "trinity");
    std::string password = sPlayerbotConfig->GetString("Playerbot.Database.Password", "trinity");
    std::string database = sPlayerbotConfig->GetString("Playerbot.Database.Name", "characters");

    printf("=== PLAYERBOT DEBUG: DB Config - Host: %s, Port: %u, User: %s, Database: %s ===\n",
           host.c_str(), port, user.c_str(), database.c_str());

    // Construct connection string in format: hostname;port;username;password;database
    std::string dbString = Trinity::StringFormat("{};{};{};{};{}", host, port, user, password, database);

    printf("=== PLAYERBOT DEBUG: Connection string: %s ===\n", dbString.c_str());

    TC_LOG_INFO("server.loading", "Playerbot Database: Connecting to {}:{}/{}", host, port, database);

    printf("=== PLAYERBOT DEBUG: About to call sPlayerbotDatabase->Initialize() ===\n");

    // Initialize database connection using our custom manager
    if (!sPlayerbotDatabase->Initialize(dbString))
    {
        printf("=== PLAYERBOT DEBUG: sPlayerbotDatabase->Initialize() FAILED ===\n");
        TC_LOG_ERROR("server.loading", "Playerbot Database: Failed to initialize connection");
        return false;
    }

    printf("=== PLAYERBOT DEBUG: sPlayerbotDatabase->Initialize() SUCCESS ===\n");

    TC_LOG_INFO("server.loading", "Playerbot Database: Successfully connected");

    // Validate database schema
    TC_LOG_INFO("server.loading", "Validating Playerbot Database Schema...");
    if (!sPlayerbotDatabase->ValidateSchema())
    {
        TC_LOG_WARN("server.loading", "Playerbot Database: Schema validation failed - some features may not work correctly");
        TC_LOG_WARN("server.loading", "Consider running database migrations or checking table structures");
    }

    return true;
}

void PlayerbotModule::ShutdownDatabase()
{
    TC_LOG_DEBUG("module.playerbot", "Closing Playerbot Database connection");
    sPlayerbotDatabase->Close();
    TC_LOG_DEBUG("module.playerbot", "Playerbot Database connection closed");
}

#endif // PLAYERBOT_ENABLED
