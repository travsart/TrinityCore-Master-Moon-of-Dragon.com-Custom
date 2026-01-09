/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifdef BUILD_PLAYERBOT

#include "PlayerbotModule.h"
#include "Config/PlayerbotConfig.h"
#include "Config/PlayerbotTradeConfig.h"
#include "Config/GuidedSetupHelper.h"
#include "Account/BotAccountMgr.h"
#include "Character/BotNameMgr.h"
#include "Character/BotCharacterDistribution.h"
#include "Database/PlayerbotDatabase.h"
#include "Database/PlayerbotCharacterDBInterface.h"
#include "Database/PlayerbotMigrationMgr.h"
#include "Lifecycle/BotSpawner.h"
#include "Lifecycle/Protection/BotProtectionRegistry.h"
#include "Lifecycle/Retirement/BotRetirementManager.h"
#include "Lifecycle/Prediction/BracketFlowPredictor.h"
#include "Lifecycle/Demand/PlayerActivityTracker.h"
#include "Lifecycle/Demand/DemandCalculator.h"
#include "Lifecycle/PopulationLifecycleController.h"
// #include "Lifecycle/BotLifecycleMgr.h"

// Instance Bot Pool System (Hybrid Warm Pool + Elastic Overflow)
#include "Lifecycle/Instance/InstanceBotPool.h"
#include "Lifecycle/Instance/InstanceBotOrchestrator.h"
#include "Lifecycle/Instance/InstanceBotHooks.h"
#include "Lifecycle/Instance/ContentRequirements.h"
#include "Lifecycle/Instance/JITBotFactory.h"
#include "Lifecycle/Instance/BotTemplateRepository.h"
#include "Lifecycle/Instance/BotCloneEngine.h"

#include "Session/BotSessionMgr.h"
#include "Session/BotWorldSessionMgr.h"
#include "Session/BotPacketRelay.h"
#include "Chat/BotChatCommandHandler.h"
#include "Professions/ProfessionManager.h"
#include "Professions/ProfessionDatabase.h"
#include "Professions/ProfessionAuctionBridge.h"
#include "Professions/GatheringMaterialsBridge.h"
#include "Professions/AuctionMaterialsBridge.h"
#include "Banking/BankingManager.h"
#include "Quest/QuestHubDatabase.h"
#include "Travel/PortalDatabase.h"
#include "Equipment/BotGearFactory.h"
#include "AI/ClassAI/ClassBehaviorTreeRegistry.h"
#include "PlayerbotModuleAdapter.h"
#include "Update/ModuleUpdateManager.h"
#include "Group/GroupEventBus.h"
#include "Network/PlayerbotPacketSniffer.h"
#include "Threading/BotActionManager.h"
#include "Log.h"
#include "GitRevision.h"
#include <chrono>

// Module state - using inline static in header for DLL compatibility

bool PlayerbotModule::Initialize()
{
    TC_LOG_INFO("server.loading", "Initializing Playerbot Module...");

    // Run guided setup check FIRST - ensures config file exists
    // This will create a default config from .dist if missing
    if (!Playerbot::GuidedSetupHelper::CheckAndRunSetup())
    {
        _lastError = "Configuration setup failed - see logs for details";
        TC_LOG_ERROR("server.loading", "Playerbot Module: {}", _lastError);
        return false;
    }

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

    // Load subsystem configurations
    TC_LOG_INFO("server.loading", "Loading Playerbot subsystem configurations...");
    Playerbot::PlayerbotTradeConfig::Load();
    TC_LOG_INFO("server.loading", "Playerbot subsystem configurations loaded");

    // Initialize Playerbot Database
    // CRITICAL: If this fails, the server MUST NOT start when Playerbot is enabled
    if (!InitializeDatabase())
    {
        // _lastError is already set by InitializeDatabase() with detailed information
        TC_LOG_ERROR("server.loading", "");
        TC_LOG_ERROR("server.loading", "  >>> SERVER STARTUP ABORTED <<<");
        TC_LOG_ERROR("server.loading", "");
        TC_LOG_ERROR("server.loading", "  Playerbot module failed to initialize due to database connection failure.");
        TC_LOG_ERROR("server.loading", "  Error: {}", _lastError);
        TC_LOG_ERROR("server.loading", "");
        return false;
    }

    // Initialize Character Database Interface
    if (!sPlayerbotCharDB->Initialize())
    {
        _lastError = "Failed to initialize Character Database Interface";
        TC_LOG_ERROR("server.loading", "Playerbot Module: {}", _lastError);
        return false;
    }

    // Initialize Migration Manager FIRST (before any database access)
    if (!PlayerbotMigrationMgr::instance()->Initialize())
    {
        _lastError = "Failed to initialize Migration Manager";
        return false;
    }

    // Apply pending migrations
    if (!PlayerbotMigrationMgr::instance()->ApplyMigrations())
    {
        _lastError = "Failed to apply database migrations";
        return false;
    }

    // Verify database version matches source code version
    // This ensures schema synchronization between builds
    PlayerbotMigrationMgr::instance()->CheckVersionMismatch();

    // Initialize Bot Account Manager
    TC_LOG_INFO("server.loading", "PlayerbotModule: About to call sBotAccountMgr->Initialize()...");
    if (!sBotAccountMgr->Initialize())
    {
        _lastError = "Failed to initialize Bot Account Manager";
        TC_LOG_ERROR("server.loading", "PlayerbotModule: BotAccountMgr->Initialize() returned FALSE");
        // Check if strict mode is enabled
        bool strictMode = sPlayerbotConfig->GetBool("Playerbot.StrictCharacterLimit", true);
        TC_LOG_ERROR("server.loading", "PlayerbotModule: StrictCharacterLimit = {}", strictMode);
        if (strictMode)
        {
            TC_LOG_ERROR("server.loading", "PlayerbotModule: ABORTING initialization due to strict mode");
            return false;
        }
        else
        {
            TC_LOG_WARN("server.loading", "PlayerbotModule: Continuing despite BotAccountMgr failure (strict mode disabled)");
        }
    }
    else
    {
        TC_LOG_INFO("server.loading", "PlayerbotModule: sBotAccountMgr->Initialize() returned TRUE");
    }

    // Initialize Bot Name Manager
    if (!sBotNameMgr->Initialize())
    {
        _lastError = "Failed to initialize Bot Name Manager";
        return false;
    }

    // Initialize Bot Character Distribution
    if (!sBotCharacterDistribution->LoadFromDatabase())
    {
        _lastError = "Failed to initialize Bot Character Distribution";
        return false;
    }

    // Initialize Bot Session Manager
    if (!sBotSessionMgr->Initialize())
    {
        _lastError = "Failed to initialize Bot Session Manager";
        return false;
    }

    // Initialize Bot World Session Manager (native TrinityCore login)
    if (!Playerbot::sBotWorldSessionMgr->Initialize())
    {
        _lastError = "Failed to initialize Bot World Session Manager";
        return false;
    }

    // Initialize Bot Packet Relay (Phase 1-3: Group packet relay for combat logs and chat)
    TC_LOG_INFO("server.loading", "Initializing Bot Packet Relay...");
    Playerbot::BotPacketRelay::Initialize();
    TC_LOG_INFO("server.loading", "Bot Packet Relay initialized successfully");

    // Initialize Bot Chat Command Handler (Phase 4: Command processing system)
    TC_LOG_INFO("server.loading", "Initializing Bot Chat Command Handler...");
    Playerbot::BotChatCommandHandler::Initialize();
    TC_LOG_INFO("server.loading", "Bot Chat Command Handler initialized successfully");

    // NOTE: Bot Spawner initialization is handled by PlayerbotModuleAdapter::OnModuleStartup()
    // This ensures proper timing - BotSpawner needs the world to be fully loaded, which happens
    // AFTER this Initialize() function completes. The ModuleManager calls OnModuleStartup()
    // at the correct time, and it will initialize both BotAccountMgr and BotSpawner.

    // Initialize Bot Lifecycle Manager
    //     TC_LOG_INFO("server.loading", "Initializing Bot Lifecycle Manager...");
    //     if (!BotLifecycleMgr::instance()->Initialize())
    //     {
    //         _lastError = "Failed to initialize Bot Lifecycle Manager";
    //         TC_LOG_ERROR("server.loading", "Playerbot Module: {}", _lastError);
    //         return false;
    //     }
    //
    // Initialize Profession Database (must happen before bots are created)
    // Phase 1B: Shared profession data now in ProfessionDatabase singleton
    // Per-bot ProfessionManager instances created by GameSystemsManager
    TC_LOG_INFO("server.loading", "Initializing Profession Database...");
    Playerbot::ProfessionDatabase::instance()->Initialize();
    TC_LOG_INFO("server.loading", "Profession Database initialized successfully");

    // Initialize Profession Bridges (Phase 3 Option C: Economic integration)
    // NOTE: All bridges now per-bot (Phase 4.1-4.3), initialized in GameSystemsManager
    //  - GatheringMaterialsBridge (Phase 4.1)
    //  - AuctionMaterialsBridge (Phase 4.2)
    //  - ProfessionAuctionBridge (Phase 4.3)
    //  - BankingManager (Phase 5.1) - Personal banking automation

    TC_LOG_INFO("server.loading", "Initializing Profession Event Bus...");
    // ProfessionEventBus is event-driven, no initialization required (lazy init)
    TC_LOG_INFO("server.loading", "Profession Event Bus ready");

    // Initialize Class Behavior Tree Registry (Phase 5: Class-specific AI trees for all 13 classes)
    TC_LOG_INFO("server.loading", "Initializing Class Behavior Tree Registry...");
    Playerbot::ClassBehaviorTreeRegistry::Initialize();
    TC_LOG_INFO("server.loading", "Class Behavior Tree Registry initialized successfully - {} class/spec trees registered",
        39); // 13 classes × 3 specs = 39 combinations

    // Initialize Quest Hub Database (spatial clustering of quest givers for efficient pathfinding)
    TC_LOG_INFO("server.loading", "Initializing Quest Hub Database...");
    if (!Playerbot::QuestHubDatabase::Instance().Initialize())
    {
        _lastError = "Failed to initialize Quest Hub Database";
        TC_LOG_ERROR("server.loading", "Playerbot Module: {}", _lastError);
        return false;
    }
    TC_LOG_INFO("server.loading", "Quest Hub Database initialized successfully - {} quest hubs loaded",
        Playerbot::QuestHubDatabase::Instance().GetQuestHubCount());

    // Initialize Portal Database (portal GameObject positions loaded from gameobject/spell tables)
    TC_LOG_INFO("server.loading", "Initializing Portal Database...");
    if (!Playerbot::PortalDatabase::Instance().Initialize())
    {
        // Non-fatal - portals will fall back to hardcoded coordinates or dynamic search
        TC_LOG_WARN("server.loading", "Portal Database initialization incomplete - using fallback portal detection");
    }
    else
    {
        auto stats = Playerbot::PortalDatabase::Instance().GetStatistics();
        TC_LOG_INFO("server.loading", "Portal Database initialized successfully - {} portals loaded ({} static, {} dungeon, {} expansion)",
            stats.totalPortals, stats.staticPortals, stats.dungeonPortals, stats.expansionPortals);
    }

    // Initialize Bot Gear Factory (automated gear generation for instant level-up)
    TC_LOG_INFO("server.loading", "Initializing Bot Gear Factory...");
    Playerbot::sBotGearFactory->Initialize();
    TC_LOG_INFO("server.loading", "Bot Gear Factory initialized successfully");

    // Initialize Packet Sniffer (centralized event detection system)
    TC_LOG_INFO("server.loading", "Initializing Packet Sniffer...");
    Playerbot::PlayerbotPacketSniffer::Initialize();
    TC_LOG_INFO("server.loading", "Packet Sniffer initialized successfully");

    // Register hooks with TrinityCore
    RegisterHooks();

    // Initialize Bot Action Manager for worker thread → main thread action queue
    TC_LOG_INFO("server.loading", "Initializing Bot Action Manager...");
    sBotActionMgr->Initialize();
    TC_LOG_INFO("server.loading", "Bot Action Manager initialized successfully");

    // Initialize Bot Protection Registry for lifecycle management
    TC_LOG_INFO("server.loading", "Initializing Bot Protection Registry...");
    if (!sBotProtectionRegistry->Initialize())
    {
        TC_LOG_WARN("server.loading", "Bot Protection Registry initialization failed - continuing without protection tracking");
    }
    else
    {
        TC_LOG_INFO("server.loading", "Bot Protection Registry initialized successfully");
    }

    // Initialize Bot Retirement Manager for lifecycle management
    TC_LOG_INFO("server.loading", "Initializing Bot Retirement Manager...");
    sBotRetirementManager->SetProtectionRegistry(sBotProtectionRegistry);
    if (!sBotRetirementManager->Initialize())
    {
        TC_LOG_WARN("server.loading", "Bot Retirement Manager initialization failed - continuing without retirement processing");
    }
    else
    {
        TC_LOG_INFO("server.loading", "Bot Retirement Manager initialized successfully");
    }

    // Initialize Bracket Flow Predictor for level bracket analysis
    TC_LOG_INFO("server.loading", "Initializing Bracket Flow Predictor...");
    if (!sBracketFlowPredictor->Initialize())
    {
        TC_LOG_WARN("server.loading", "Bracket Flow Predictor initialization failed - continuing without flow prediction");
    }
    else
    {
        TC_LOG_INFO("server.loading", "Bracket Flow Predictor initialized successfully");
    }

    // Initialize Player Activity Tracker for demand-driven spawning
    TC_LOG_INFO("server.loading", "Initializing Player Activity Tracker...");
    if (!sPlayerActivityTracker->Initialize())
    {
        TC_LOG_WARN("server.loading", "Player Activity Tracker initialization failed - continuing without activity tracking");
    }
    else
    {
        TC_LOG_INFO("server.loading", "Player Activity Tracker initialized successfully");
    }

    // Initialize Demand Calculator for spawn request generation
    TC_LOG_INFO("server.loading", "Initializing Demand Calculator...");
    if (!sDemandCalculator->Initialize())
    {
        TC_LOG_WARN("server.loading", "Demand Calculator initialization failed - continuing without demand calculation");
    }
    else
    {
        // Wire up dependencies for demand calculator
        sDemandCalculator->SetActivityTracker(sPlayerActivityTracker);
        sDemandCalculator->SetProtectionRegistry(sBotProtectionRegistry);
        sDemandCalculator->SetFlowPredictor(sBracketFlowPredictor);
        TC_LOG_INFO("server.loading", "Demand Calculator initialized successfully");
    }

    // Initialize Population Lifecycle Controller (master orchestrator)
    TC_LOG_INFO("server.loading", "Initializing Population Lifecycle Controller...");
    if (!sPopulationLifecycleController->Initialize())
    {
        TC_LOG_WARN("server.loading", "Population Lifecycle Controller initialization failed - continuing without lifecycle orchestration");
    }
    else
    {
        TC_LOG_INFO("server.loading", "Population Lifecycle Controller initialized successfully");
    }

    // ==========================================================================
    // INSTANCE BOT POOL SYSTEM - Hybrid Warm Pool + Elastic Overflow
    // ==========================================================================

    // Initialize Content Requirements Database (required by all instance systems)
    TC_LOG_INFO("server.loading", "Initializing Content Requirements Database...");
    if (!sContentRequirementDb->Initialize())
    {
        TC_LOG_WARN("server.loading", "Content Requirements Database initialization failed - instance bots may not have content info");
    }
    else
    {
        TC_LOG_INFO("server.loading", "Content Requirements Database initialized successfully");
    }

    // Initialize Bot Template Repository (used by BotCloneEngine)
    TC_LOG_INFO("server.loading", "Initializing Bot Template Repository...");
    if (!sBotTemplateRepository->Initialize())
    {
        TC_LOG_WARN("server.loading", "Bot Template Repository initialization failed - JIT bot creation may be slower");
    }
    else
    {
        TC_LOG_INFO("server.loading", "Bot Template Repository initialized with {} templates",
            sBotTemplateRepository->GetTemplateCount());
    }

    // Initialize Bot Clone Engine (fast bot creation from templates)
    TC_LOG_INFO("server.loading", "Initializing Bot Clone Engine...");
    if (!sBotCloneEngine->Initialize())
    {
        TC_LOG_WARN("server.loading", "Bot Clone Engine initialization failed - JIT bot creation disabled");
    }
    else
    {
        TC_LOG_INFO("server.loading", "Bot Clone Engine initialized successfully");
    }

    // Initialize Instance Bot Pool (warm pool of pre-logged bots)
    TC_LOG_INFO("server.loading", "Initializing Instance Bot Pool...");
    if (!sInstanceBotPool->Initialize())
    {
        TC_LOG_WARN("server.loading", "Instance Bot Pool initialization failed - instance bots will use JIT only");
    }
    else
    {
        TC_LOG_INFO("server.loading", "Instance Bot Pool initialized - {} total capacity",
            sInstanceBotPool->GetTotalPoolSize());
    }

    // Initialize JIT Bot Factory (elastic overflow for large content)
    TC_LOG_INFO("server.loading", "Initializing JIT Bot Factory...");
    if (!sJITBotFactory->Initialize())
    {
        TC_LOG_WARN("server.loading", "JIT Bot Factory initialization failed - large content may have delays");
    }
    else
    {
        TC_LOG_INFO("server.loading", "JIT Bot Factory initialized successfully");
    }

    // Initialize Instance Bot Orchestrator (master coordinator)
    TC_LOG_INFO("server.loading", "Initializing Instance Bot Orchestrator...");
    if (!sInstanceBotOrchestrator->Initialize())
    {
        TC_LOG_WARN("server.loading", "Instance Bot Orchestrator initialization failed - instance bot system disabled");
    }
    else
    {
        TC_LOG_INFO("server.loading", "Instance Bot Orchestrator initialized successfully");
    }

    // Initialize Instance Bot Hooks (core integration points)
    TC_LOG_INFO("server.loading", "Initializing Instance Bot Hooks...");
    if (!Playerbot::InstanceBotHooks::Initialize())
    {
        TC_LOG_WARN("server.loading", "Instance Bot Hooks initialization failed - core integration disabled");
    }
    else
    {
        TC_LOG_INFO("server.loading", "Instance Bot Hooks initialized - core integration active");
    }

    // Note: Instance Bot Scripts are registered in AddSC_playerbot_world()
    // during the proper script loading phase (see PlayerbotWorldScript.cpp)

    // Warm the Instance Bot Pool (create and login bots) - deferred to OnWorldLoad
    // Pool warming happens after world is fully loaded to avoid database contention
    TC_LOG_INFO("server.loading", "Instance Bot Pool warming will occur after world load");

    // ==========================================================================

    // Register with the shared ModuleUpdateManager for world updates
    if (!sModuleUpdateManager->RegisterModule("playerbot", [](uint32 diff) { OnWorldUpdate(diff); }))
    {
        _lastError = "Failed to register with ModuleUpdateManager";
        TC_LOG_ERROR("server.loading", "Playerbot Module: ModuleUpdateManager registration failed");
        return false;
    }
    TC_LOG_INFO("server.loading", "Playerbot Module: Successfully registered with ModuleUpdateManager");

    _initialized = true;
    _enabled = true;


    return true;
}

void PlayerbotModule::Shutdown()
{
    if (!_initialized)
        return;

    TC_LOG_INFO("server.loading", "Shutting down Playerbot Module...");

    // Shutdown Bot Action Manager
    sBotActionMgr->Shutdown();
    TC_LOG_INFO("server.loading", "Bot Action Manager shutdown complete");

    // Shutdown Population Lifecycle Controller (depends on all lifecycle components)
    TC_LOG_INFO("server.loading", "Shutting down Population Lifecycle Controller...");
    sPopulationLifecycleController->Shutdown();
    TC_LOG_INFO("server.loading", "Population Lifecycle Controller shutdown complete");

    // ==========================================================================
    // INSTANCE BOT POOL SYSTEM SHUTDOWN
    // ==========================================================================

    // Shutdown Instance Bot Hooks (stops core integration first)
    TC_LOG_INFO("server.loading", "Shutting down Instance Bot Hooks...");
    Playerbot::InstanceBotHooks::Shutdown();
    TC_LOG_INFO("server.loading", "Instance Bot Hooks shutdown complete");

    // Shutdown Instance Bot Orchestrator (coordinates all instance bot operations)
    TC_LOG_INFO("server.loading", "Shutting down Instance Bot Orchestrator...");
    sInstanceBotOrchestrator->Shutdown();
    TC_LOG_INFO("server.loading", "Instance Bot Orchestrator shutdown complete");

    // Shutdown JIT Bot Factory (stop async creation)
    TC_LOG_INFO("server.loading", "Shutting down JIT Bot Factory...");
    sJITBotFactory->Shutdown();
    TC_LOG_INFO("server.loading", "JIT Bot Factory shutdown complete");

    // Shutdown Instance Bot Pool (logout pool bots)
    TC_LOG_INFO("server.loading", "Shutting down Instance Bot Pool...");
    sInstanceBotPool->Shutdown();
    TC_LOG_INFO("server.loading", "Instance Bot Pool shutdown complete");

    // Shutdown Bot Clone Engine (stop async cloning)
    TC_LOG_INFO("server.loading", "Shutting down Bot Clone Engine...");
    sBotCloneEngine->Shutdown();
    TC_LOG_INFO("server.loading", "Bot Clone Engine shutdown complete");

    // Shutdown Bot Template Repository
    TC_LOG_INFO("server.loading", "Shutting down Bot Template Repository...");
    sBotTemplateRepository->Shutdown();
    TC_LOG_INFO("server.loading", "Bot Template Repository shutdown complete");

    // ==========================================================================

    // Shutdown Demand Calculator (depends on ActivityTracker, ProtectionRegistry, FlowPredictor)
    TC_LOG_INFO("server.loading", "Shutting down Demand Calculator...");
    sDemandCalculator->Shutdown();
    TC_LOG_INFO("server.loading", "Demand Calculator shutdown complete");

    // Shutdown Player Activity Tracker
    TC_LOG_INFO("server.loading", "Shutting down Player Activity Tracker...");
    sPlayerActivityTracker->Shutdown();
    TC_LOG_INFO("server.loading", "Player Activity Tracker shutdown complete");

    // Shutdown Bracket Flow Predictor
    TC_LOG_INFO("server.loading", "Shutting down Bracket Flow Predictor...");
    sBracketFlowPredictor->Shutdown();
    TC_LOG_INFO("server.loading", "Bracket Flow Predictor shutdown complete");

    // Shutdown Bot Retirement Manager (before Protection Registry)
    TC_LOG_INFO("server.loading", "Shutting down Bot Retirement Manager...");
    sBotRetirementManager->Shutdown();
    TC_LOG_INFO("server.loading", "Bot Retirement Manager shutdown complete");

    // Shutdown Bot Protection Registry
    TC_LOG_INFO("server.loading", "Shutting down Bot Protection Registry...");
    sBotProtectionRegistry->Shutdown();
    TC_LOG_INFO("server.loading", "Bot Protection Registry shutdown complete");

    // Unregister hooks
    UnregisterHooks();

    // Unregister from ModuleUpdateManager
    sModuleUpdateManager->UnregisterModule("playerbot");
    TC_LOG_DEBUG("server.loading", "Unregistered playerbot from ModuleUpdateManager");

    if (_enabled)
    {
        // Shutdown Bot Lifecycle Manager
        //         TC_LOG_INFO("server.loading", "Shutting down Bot Lifecycle Manager...");
        //         BotLifecycleMgr::instance()->Shutdown();
        //
        // Shutdown Bot Chat Command Handler
        TC_LOG_INFO("server.loading", "Shutting down Bot Chat Command Handler...");
        Playerbot::BotChatCommandHandler::Shutdown();

        // Shutdown Bot Packet Relay
        TC_LOG_INFO("server.loading", "Shutting down Bot Packet Relay...");
        Playerbot::BotPacketRelay::Shutdown();

        // Shutdown Packet Sniffer
        TC_LOG_INFO("server.loading", "Shutting down Packet Sniffer...");
        Playerbot::PlayerbotPacketSniffer::Shutdown();

        // Shutdown Bot World Session Manager
        TC_LOG_INFO("server.loading", "Shutting down Bot World Session Manager...");
        Playerbot::sBotWorldSessionMgr->Shutdown();

        // Shutdown Bot Name Manager
        TC_LOG_INFO("server.loading", "Shutting down Bot Name Manager...");
        sBotNameMgr->Shutdown();

        // Shutdown Bot Account Manager
        TC_LOG_INFO("server.loading", "Shutting down Bot Account Manager...");
        sBotAccountMgr->Shutdown();

        // Shutdown Playerbot Database first to stop all operations
        TC_LOG_INFO("server.loading", "Shutting down Playerbot Database...");
        ShutdownDatabase();

        // Then shutdown Character Database Interface after operations complete
        TC_LOG_INFO("server.loading", "Shutting down Character Database Interface...");
        sPlayerbotCharDB->Shutdown();
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
    Playerbot::PlayerbotModuleAdapter::RegisterWithModuleManager();
}

void PlayerbotModule::UnregisterHooks()
{
    // TODO: Unregister event hooks
}

void PlayerbotModule::OnWorldUpdate(uint32 diff)
{
    if (!_enabled || !_initialized)
        return;

    // CRITICAL SAFETY: Wrap entire update in try-catch to prevent crashes
    try
    {

    // One-time trigger to complete login for existing sessions
    static bool loginTriggered = false;
    static uint32 totalTime = 0;
    totalTime += diff;

    if (!loginTriggered && totalTime > 5000) // Wait 5 seconds after startup
    {
        TC_LOG_INFO("module.playerbot", " OnWorldUpdate: Auto-triggering character logins for existing sessions");
        TriggerBotCharacterLogins();
        loginTriggered = true;
    }

    // PERFORMANCE PROFILING: Track time for each manager
    auto timeStart = std::chrono::high_resolution_clock::now();
    auto lastTime = timeStart;

    // Update BotAccountMgr for thread-safe callback processing
    sBotAccountMgr->Update(diff);
    auto t1 = std::chrono::high_resolution_clock::now();
    auto accountTime = std::chrono::duration_cast<std::chrono::microseconds>(t1 - lastTime).count();
    lastTime = t1;

    // Update BotSpawner for automatic character creation and management
    Playerbot::sBotSpawner->Update(diff);
    auto t2 = std::chrono::high_resolution_clock::now();
    auto spawnerTime = std::chrono::duration_cast<std::chrono::microseconds>(t2 - lastTime).count();
    lastTime = t2;

    // Update BotSessionMgr for active bot session processing (legacy)
    sBotSessionMgr->UpdateAllSessions(diff);
    auto t3 = std::chrono::high_resolution_clock::now();
    auto sessionMgrTime = std::chrono::duration_cast<std::chrono::microseconds>(t3 - lastTime).count();
    lastTime = t3;

    // Update BotWorldSessionMgr for native TrinityCore login sessions
    Playerbot::sBotWorldSessionMgr->UpdateSessions(diff);
    auto t4 = std::chrono::high_resolution_clock::now();
    auto worldSessionTime = std::chrono::duration_cast<std::chrono::microseconds>(t4 - lastTime).count();
    lastTime = t4;

    // Update PlayerbotCharacterDBInterface to process sync queue
    sPlayerbotCharDB->Update(diff);
    auto t5 = std::chrono::high_resolution_clock::now();
    auto charDBTime = std::chrono::duration_cast<std::chrono::microseconds>(t5 - lastTime).count();
    lastTime = t5;

    // Update GroupEventBus to process pending group events
    Playerbot::GroupEventBus::instance()->ProcessEvents(diff);
    auto t6 = std::chrono::high_resolution_clock::now();
    auto groupEventTime = std::chrono::duration_cast<std::chrono::microseconds>(t6 - lastTime).count();
    lastTime = t6;

    // Update BotProtectionRegistry for periodic maintenance
    sBotProtectionRegistry->Update(diff);
    auto t7 = std::chrono::high_resolution_clock::now();
    auto protectionTime = std::chrono::duration_cast<std::chrono::microseconds>(t7 - lastTime).count();
    lastTime = t7;

    // Update BotRetirementManager for retirement queue processing
    sBotRetirementManager->Update(diff);
    auto t8 = std::chrono::high_resolution_clock::now();
    auto retirementTime = std::chrono::duration_cast<std::chrono::microseconds>(t8 - lastTime).count();
    lastTime = t8;

    // Update BracketFlowPredictor for flow tracking
    sBracketFlowPredictor->Update(diff);
    auto t9 = std::chrono::high_resolution_clock::now();
    auto predictionTime = std::chrono::duration_cast<std::chrono::microseconds>(t9 - lastTime).count();
    lastTime = t9;

    // Update PlayerActivityTracker for player location tracking
    sPlayerActivityTracker->Update(diff);
    auto t10 = std::chrono::high_resolution_clock::now();
    auto activityTime = std::chrono::duration_cast<std::chrono::microseconds>(t10 - lastTime).count();
    lastTime = t10;

    // Update DemandCalculator for spawn demand analysis
    sDemandCalculator->Update(diff);
    auto t11 = std::chrono::high_resolution_clock::now();
    auto demandTime = std::chrono::duration_cast<std::chrono::microseconds>(t11 - lastTime).count();
    lastTime = t11;

    // Update PopulationLifecycleController for lifecycle orchestration
    sPopulationLifecycleController->Update(diff);
    auto t12 = std::chrono::high_resolution_clock::now();
    auto lifecycleTime = std::chrono::duration_cast<std::chrono::microseconds>(t12 - lastTime).count();

    // Calculate total time
    auto totalUpdateTime = std::chrono::duration_cast<std::chrono::microseconds>(t12 - timeStart).count();

    // Log if total time exceeds 100ms
    if (totalUpdateTime > 100000) // 100ms in microseconds
    {
        TC_LOG_WARN("module.playerbot.performance", "PERFORMANCE: OnWorldUpdate took {:.2f}ms - Account:{:.2f}ms, Spawner:{:.2f}ms, SessionMgr:{:.2f}ms, WorldSession:{:.2f}ms, CharDB:{:.2f}ms, GroupEvent:{:.2f}ms, Protection:{:.2f}ms, Retirement:{:.2f}ms, Prediction:{:.2f}ms, Activity:{:.2f}ms, Demand:{:.2f}ms, Lifecycle:{:.2f}ms",
            totalUpdateTime / 1000.0f,
            accountTime / 1000.0f,
            spawnerTime / 1000.0f,
            sessionMgrTime / 1000.0f,
            worldSessionTime / 1000.0f,
            charDBTime / 1000.0f,
            groupEventTime / 1000.0f,
            protectionTime / 1000.0f,
            retirementTime / 1000.0f,
            predictionTime / 1000.0f,
            activityTime / 1000.0f,
            demandTime / 1000.0f,
            lifecycleTime / 1000.0f);
    }

    }
    catch (std::exception const& ex)
    {
        TC_LOG_ERROR("module.playerbot", "CRITICAL EXCEPTION in PlayerbotModule::OnWorldUpdate: {}", ex.what());
        TC_LOG_ERROR("module.playerbot", "Disabling playerbot to prevent further crashes");
        _enabled = false;
    }
    catch (...)
    {
        TC_LOG_ERROR("module.playerbot", "CRITICAL UNKNOWN EXCEPTION in PlayerbotModule::OnWorldUpdate");
        TC_LOG_ERROR("module.playerbot", "Disabling playerbot to prevent further crashes");
        _enabled = false;
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
    // Check if sPlayerbotConfig is valid
    if (!sPlayerbotConfig)
    {
        return;
    }

    sPlayerbotConfig->InitializeLogging();
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
        // ============================================================================
        // CRITICAL DATABASE CONNECTION FAILURE - BLOCK SERVER STARTUP
        // ============================================================================
        TC_LOG_ERROR("server.loading", "");
        TC_LOG_ERROR("server.loading", "================================================================================");
        TC_LOG_ERROR("server.loading", "  PLAYERBOT DATABASE CONNECTION FAILED - SERVER STARTUP BLOCKED");
        TC_LOG_ERROR("server.loading", "================================================================================");
        TC_LOG_ERROR("server.loading", "");
        TC_LOG_ERROR("server.loading", "  Playerbot is ENABLED but cannot connect to its database.");
        TC_LOG_ERROR("server.loading", "  The server cannot start safely without a working database connection.");
        TC_LOG_ERROR("server.loading", "");
        TC_LOG_ERROR("server.loading", "  Current Configuration:");
        TC_LOG_ERROR("server.loading", "    Host:     {}", host);
        TC_LOG_ERROR("server.loading", "    Port:     {}", port);
        TC_LOG_ERROR("server.loading", "    User:     {}", user);
        TC_LOG_ERROR("server.loading", "    Database: {}", database);
        TC_LOG_ERROR("server.loading", "");
        TC_LOG_ERROR("server.loading", "  Possible Causes:");
        TC_LOG_ERROR("server.loading", "    1. MySQL server is not running");
        TC_LOG_ERROR("server.loading", "    2. Wrong hostname or port in configuration");
        TC_LOG_ERROR("server.loading", "    3. Invalid username or password");
        TC_LOG_ERROR("server.loading", "    4. Database '{}' does not exist", database);
        TC_LOG_ERROR("server.loading", "    5. User '{}' has no access to database '{}'", user, database);
        TC_LOG_ERROR("server.loading", "    6. Firewall blocking connection to port {}", port);
        TC_LOG_ERROR("server.loading", "");
        TC_LOG_ERROR("server.loading", "  Solutions:");
        TC_LOG_ERROR("server.loading", "    - Check worldserver.conf for Playerbot.Database.* settings");
        TC_LOG_ERROR("server.loading", "    - Verify MySQL server is running: mysql -u {} -p -h {} -P {}", user, host, port);
        TC_LOG_ERROR("server.loading", "    - Create database if missing: CREATE DATABASE {};", database);
        TC_LOG_ERROR("server.loading", "    - Or disable Playerbot: set Playerbot.Enable = 0");
        TC_LOG_ERROR("server.loading", "");
        TC_LOG_ERROR("server.loading", "================================================================================");
        TC_LOG_ERROR("server.loading", "");

        _lastError = Trinity::StringFormat(
            "CRITICAL: Playerbot database connection failed! "
            "Cannot connect to {}:{}/{} as user '{}'. "
            "Check your Playerbot.Database.* configuration or disable Playerbot (Playerbot.Enable = 0)",
            host, port, database, user);

        return false;
    }

    TC_LOG_INFO("server.loading", "Playerbot Database: Successfully connected to {}:{}/{}", host, port, database);

    // Validate database schema
    TC_LOG_INFO("server.loading", "Validating Playerbot Database Schema...");
    if (!sPlayerbotDatabase->ValidateSchema())
    {
        TC_LOG_WARN("server.loading", "");
        TC_LOG_WARN("server.loading", "================================================================================");
        TC_LOG_WARN("server.loading", "  PLAYERBOT DATABASE SCHEMA WARNING");
        TC_LOG_WARN("server.loading", "================================================================================");
        TC_LOG_WARN("server.loading", "  Database schema validation failed - some tables may be missing or outdated.");
        TC_LOG_WARN("server.loading", "  Playerbot will continue but some features may not work correctly.");
        TC_LOG_WARN("server.loading", "");
        TC_LOG_WARN("server.loading", "  Solution: Run the database migrations in sql/playerbot/ directory");
        TC_LOG_WARN("server.loading", "================================================================================");
        TC_LOG_WARN("server.loading", "");
    }

    return true;
}

void PlayerbotModule::TriggerBotCharacterLogins()
{
    if (!_enabled || !_initialized)
    {
        TC_LOG_WARN("module.playerbot", "TriggerBotCharacterLogins: Module not enabled or initialized");
        return;
    }

    TC_LOG_INFO("module.playerbot", " TriggerBotCharacterLogins: Manually triggering character logins for existing sessions");

    // Call the BotSessionMgr method to trigger logins (legacy approach)
    sBotSessionMgr->TriggerCharacterLoginForAllSessions();

    // Call the BotWorldSessionMgr method to trigger native logins
    Playerbot::sBotWorldSessionMgr->TriggerCharacterLoginForAllSessions();

    TC_LOG_INFO("module.playerbot", " TriggerBotCharacterLogins: Complete");
}

void PlayerbotModule::ShutdownDatabase()
{
    TC_LOG_DEBUG("module.playerbot", "Closing Playerbot Database connection");
    sPlayerbotDatabase->Close();
    TC_LOG_DEBUG("module.playerbot", "Playerbot Database connection closed");
}

#endif // BUILD_PLAYERBOT
