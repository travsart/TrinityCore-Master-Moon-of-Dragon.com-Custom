/*
 * Copyright (C) 2025 TrinityCore <https://www.trinitycore.org/>
 */

#include "LazyManagerFactory.h"
#include "Game/QuestManager.h"
#include "Social/TradeManager.h"
#include "Professions/GatheringManager.h"
#include "Economy/AuctionManager.h"
#include "Advanced/GroupCoordinator.h"
#include "Lifecycle/DeathRecoveryManager.h"
#include "AI/BotAI.h"
#include "Player.h"
#include "Log.h"
#include "Timer.h"
#include <chrono>

namespace Playerbot
{

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

LazyManagerFactory::LazyManagerFactory(Player* bot, BotAI* ai)
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
                 if (!bot)
                 {
                     TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                     return;
                 }
    : _bot(bot), _ai(ai)
{
    if (!_bot)
    {
        TC_LOG_ERROR("module.playerbot.lazy", "LazyManagerFactory created with null bot pointer");
        return;
    }

    if (!_ai)
    {
        TC_LOG_ERROR("module.playerbot.lazy", "LazyManagerFactory created with null AI pointer");
        return;
    }

    TC_LOG_DEBUG("module.playerbot.lazy", "LazyManagerFactory initialized for bot {} - Managers will be created on-demand",
                 _bot->GetName());
}
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
    return nullptr;
}

LazyManagerFactory::~LazyManagerFactory()
{
    ShutdownAll();
    TC_LOG_DEBUG("module.playerbot.lazy", "LazyManagerFactory destroyed for bot {} - {} managers initialized, total init time: {}ms",
                 if (!bot)
                 {
                     TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                     return;
                 }
                 _bot ? _bot->GetName() : "Unknown",
                 _initCount.load(),
                 _totalInitTime.count());
}

// ============================================================================
// LAZY MANAGER GETTERS - Double-checked locking pattern
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
    return;
}
// ============================================================================

QuestManager* LazyManagerFactory::GetQuestManager()
{
    return GetOrCreate<QuestManager>(
        _questManager,
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return;
        }
        _questManagerInit,
        [this]() -> std::unique_ptr<QuestManager> {
            auto start = std::chrono::steady_clock::now();

            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return nullptr;
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return nullptr;
            }
            }
            TC_LOG_DEBUG("module.playerbot.lazy", "Creating QuestManager for bot {}", _bot->GetName());
            auto manager = std::make_unique<QuestManager>(_bot, _ai);
                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                    return nullptr;
                }
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
    return nullptr;
}
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return;
            }
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return;
            }

            // Initialize manager (calls OnInitialize())
            if (!manager->Initialize())
            {
                TC_LOG_ERROR("module.playerbot.lazy", "Failed to initialize QuestManager for bot {}", _bot->GetName());
                return nullptr;
            }

            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);

            RecordInitTime("QuestManager", duration);
            TC_LOG_INFO("module.playerbot.lazy", "✅ QuestManager created for bot {} in {}ms", _bot->GetName(), duration.count());

            return manager;
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return;
        }
        }
    );
}

TradeManager* LazyManagerFactory::GetTradeManager()
{
    return GetOrCreate<TradeManager>(
        _tradeManager,
        _tradeManagerInit,
        [this]() -> std::unique_ptr<TradeManager> {
            auto start = std::chrono::steady_clock::now();

            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                    return nullptr;
                }
                return nullptr;
            }
            TC_LOG_DEBUG("module.playerbot.lazy", "Creating TradeManager for bot {}", _bot->GetName());
            auto manager = std::make_unique<TradeManager>(_bot, _ai);
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return nullptr;
            }
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return;
            }
                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                    return nullptr;
                }
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return;
            }

            if (!manager->Initialize())
            {
                TC_LOG_ERROR("module.playerbot.lazy", "Failed to initialize TradeManager for bot {}", _bot->GetName());
                return nullptr;
            }

            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);

            RecordInitTime("TradeManager", duration);
            TC_LOG_INFO("module.playerbot.lazy", "✅ TradeManager created for bot {} in {}ms", _bot->GetName(), duration.count());

            return manager;
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return;
        }
        }
    );
}

GatheringManager* LazyManagerFactory::GetGatheringManager()
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
    return;
}
{
    return GetOrCreate<GatheringManager>(
        _gatheringManager,
        _gatheringManagerInit,
        [this]() -> std::unique_ptr<GatheringManager> {
            auto start = std::chrono::steady_clock::now();

            if (!bot)
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return nullptr;
            }
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return nullptr;
            }
            TC_LOG_DEBUG("module.playerbot.lazy", "Creating GatheringManager for bot {}", _bot->GetName());
            auto manager = std::make_unique<GatheringManager>(_bot, _ai);
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return;
            }
                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                    return nullptr;
                }
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return;
            }

            if (!manager->Initialize())
            {
                TC_LOG_ERROR("module.playerbot.lazy", "Failed to initialize GatheringManager for bot {}", _bot->GetName());
                return nullptr;
            }

            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
    return;
}

            RecordInitTime("GatheringManager", duration);
            TC_LOG_INFO("module.playerbot.lazy", "✅ GatheringManager created for bot {} in {}ms", _bot->GetName(), duration.count());

            return manager;
        }
    );
}

AuctionManager* LazyManagerFactory::GetAuctionManager()
{
    return GetOrCreate<AuctionManager>(
        _auctionManager,
        _auctionManagerInit,
        [this]() -> std::unique_ptr<AuctionManager> {
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return;
            }
            auto start = std::chrono::steady_clock::now();

            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return nullptr;
            }
            TC_LOG_DEBUG("module.playerbot.lazy", "Creating AuctionManager for bot {}", _bot->GetName());
            auto manager = std::make_unique<AuctionManager>(_bot, _ai);
                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                    return;
                }
                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                    return nullptr;
                }
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return;
            }

            if (!manager->Initialize())
            {
                TC_LOG_ERROR("module.playerbot.lazy", "Failed to initialize AuctionManager for bot {}", _bot->GetName());
                return nullptr;
            }

            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);

            RecordInitTime("AuctionManager", duration);
            TC_LOG_INFO("module.playerbot.lazy", "✅ AuctionManager created for bot {} in {}ms", _bot->GetName(), duration.count());

            return manager;
        }
    );
}

GroupCoordinator* LazyManagerFactory::GetGroupCoordinator()
{
    return GetOrCreate<GroupCoordinator>(
        _groupCoordinator,
        _groupCoordinatorInit,
        [this]() -> std::unique_ptr<GroupCoordinator> {
            auto start = std::chrono::steady_clock::now();

            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return nullptr;
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return nullptr;
            }
            }
            TC_LOG_DEBUG("module.playerbot.lazy", "Creating GroupCoordinator for bot {}", _bot->GetName());
            auto manager = std::make_unique<GroupCoordinator>(_bot, _ai);
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
    return;
}
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return;
        }
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return;
            }

            // Initialize() returns void, just call it
            manager->Initialize();

            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);

            RecordInitTime("GroupCoordinator", duration);
            TC_LOG_INFO("module.playerbot.lazy", "✅ GroupCoordinator created for bot {} in {}ms", _bot->GetName(), duration.count());

            return manager;
        }
    );
}

DeathRecoveryManager* LazyManagerFactory::GetDeathRecoveryManager()
{
    return GetOrCreate<DeathRecoveryManager>(
        _deathRecoveryManager,
        _deathRecoveryManagerInit,
        [this]() -> std::unique_ptr<DeathRecoveryManager> {
            auto start = std::chrono::steady_clock::now();

            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return nullptr;
            }
            TC_LOG_DEBUG("module.playerbot.lazy", "Creating DeathRecoveryManager for bot {}", _bot->GetName());
            auto manager = std::make_unique<DeathRecoveryManager>(_bot, _ai);
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return;
            }

            // DeathRecoveryManager doesn't have Initialize() method - ready after construction

            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);

            RecordInitTime("DeathRecoveryManager", duration);
            TC_LOG_INFO("module.playerbot.lazy", "✅ DeathRecoveryManager created for bot {} in {}ms", _bot->GetName(), duration.count());

            return manager;
        }
    );
}

// ============================================================================
// GENERIC LAZY INITIALIZATION TEMPLATE
// ============================================================================

template<typename T>
T* LazyManagerFactory::GetOrCreate(
    std::unique_ptr<T>& manager,
    std::atomic<bool>& flag,
    std::function<std::unique_ptr<T>()> factory)
{
    // Fast path: Check if already initialized (lock-free)
    if (flag.load(std::memory_order_acquire))
    {
        return manager.get();
    }

    // Slow path: Need to create manager
    {
        std::unique_lock lock(_mutex);

        // Double-check after acquiring lock (another thread may have created it)
        if (flag.load(std::memory_order_relaxed))
        {
            return manager.get();
        }

        // Create manager via factory function
        try
        {
            manager = factory();
            if (!manager)
            {
                TC_LOG_ERROR("module.playerbot.lazy", "Factory function returned null manager for bot {}",
                             if (!bot)
                             {
                                 TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                                 return;
                             }
                             _bot ? _bot->GetName() : "Unknown");
                return nullptr;
            }

            // Mark as initialized (memory_order_release ensures all writes are visible)
            flag.store(true, std::memory_order_release);
            _initCount.fetch_add(1, std::memory_order_relaxed);

            return manager.get();
        }
        catch (std::exception const& e)
        {
            TC_LOG_ERROR("module.playerbot.lazy", "Exception creating manager for bot {}: {}",
                         if (!bot)
                         if (!bot)
                         {
                             TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                             return;
                         }
                         {
                             TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                             return;
                         }
                         _bot ? _bot->GetName() : "Unknown", e.what());
            return nullptr;
        }
        catch (...)
        {
            TC_LOG_ERROR("module.playerbot.lazy", "Unknown exception creating manager for bot {}",
                         if (!bot)
                         {
                             TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                             return;
                         }
                         _bot ? _bot->GetName() : "Unknown");
            return nullptr;
        }
    }
}

// ============================================================================
// STATE QUERIES
// ============================================================================

template<>
bool LazyManagerFactory::IsInitialized<QuestManager>() const
{
    return _questManagerInit.load(std::memory_order_acquire);
}

template<>
bool LazyManagerFactory::IsInitialized<TradeManager>() const
{
    return _tradeManagerInit.load(std::memory_order_acquire);
}

template<>
bool LazyManagerFactory::IsInitialized<GatheringManager>() const
{
    return _gatheringManagerInit.load(std::memory_order_acquire);
}

template<>
bool LazyManagerFactory::IsInitialized<AuctionManager>() const
{
    return _auctionManagerInit.load(std::memory_order_acquire);
}

template<>
bool LazyManagerFactory::IsInitialized<GroupCoordinator>() const
{
    return _groupCoordinatorInit.load(std::memory_order_acquire);
}

template<>
bool LazyManagerFactory::IsInitialized<DeathRecoveryManager>() const
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
    return;
}
{
    return _deathRecoveryManagerInit.load(std::memory_order_acquire);
}

size_t LazyManagerFactory::GetInitializedCount() const
{
    return _initCount.load(std::memory_order_relaxed);
}

std::chrono::milliseconds LazyManagerFactory::GetTotalInitTime() const
{
    std::lock_guard lock(_metricsMutex);
    return _totalInitTime;
}

if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
    return;
}
// ============================================================================
// LIFECYCLE MANAGEMENT
// ============================================================================

void LazyManagerFactory::Update(uint32 diff)
{
    // Only update managers that have been initialized (zero overhead for uninit)
    std::shared_lock lock(_mutex);

    if (_questManager)
        _questManager->Update(diff);

    if (_tradeManager)
        _tradeManager->Update(diff);

    if (_gatheringManager)
        _gatheringManager->Update(diff);

    if (_auctionManager)
        _auctionManager->Update(diff);

    if (_groupCoordinator)
        _groupCoordinator->Update(diff);

    if (_deathRecoveryManager)
        _deathRecoveryManager->Update(diff);
}

void LazyManagerFactory::ShutdownAll()
{
    std::unique_lock lock(_mutex);

    TC_LOG_DEBUG("module.playerbot.lazy", "Shutting down {} managers for bot {}",
                 if (!bot)
                 {
                     TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                     return;
                 }
                 _initCount.load(), _bot ? _bot->GetName() : "Unknown");

    if (_questManager)
    {
        _questManager->Shutdown();
        _questManager.reset();
        TC_LOG_DEBUG("module.playerbot.lazy", "QuestManager shutdown complete");
    }

    if (_tradeManager)
    {
        _tradeManager->Shutdown();
        _tradeManager.reset();
        TC_LOG_DEBUG("module.playerbot.lazy", "TradeManager shutdown complete");
    }

    if (_gatheringManager)
    {
        _gatheringManager->Shutdown();
        _gatheringManager.reset();
        TC_LOG_DEBUG("module.playerbot.lazy", "GatheringManager shutdown complete");
    }

    if (_auctionManager)
    {
        _auctionManager->Shutdown();
        _auctionManager.reset();
        TC_LOG_DEBUG("module.playerbot.lazy", "AuctionManager shutdown complete");
    }

    if (_groupCoordinator)
    {
        _groupCoordinator->Shutdown();
        _groupCoordinator.reset();
        TC_LOG_DEBUG("module.playerbot.lazy", "GroupCoordinator shutdown complete");
    }

    if (_deathRecoveryManager)
    {
        // DeathRecoveryManager doesn't have Shutdown() method - cleanup via reset()
        _deathRecoveryManager.reset();
        TC_LOG_DEBUG("module.playerbot.lazy", "DeathRecoveryManager shutdown complete");
    }

    // Reset flags
    _questManagerInit.store(false, std::memory_order_release);
    _tradeManagerInit.store(false, std::memory_order_release);
    _gatheringManagerInit.store(false, std::memory_order_release);
    _auctionManagerInit.store(false, std::memory_order_release);
    _groupCoordinatorInit.store(false, std::memory_order_release);
    _deathRecoveryManagerInit.store(false, std::memory_order_release);
    _initCount.store(0, std::memory_order_release);
}

void LazyManagerFactory::InitializeAll()
{
    TC_LOG_WARN("module.playerbot.lazy", "Force-initializing ALL managers for bot {} - this defeats lazy initialization!",
                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                    return;
                }
                _bot ? _bot->GetName() : "Unknown");

    auto start = std::chrono::steady_clock::now();

    GetQuestManager();
    GetTradeManager();
    GetGatheringManager();
    GetAuctionManager();
    GetGroupCoordinator();
    GetDeathRecoveryManager();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    TC_LOG_INFO("module.playerbot.lazy", "All managers initialized for bot {} in {}ms (lazy init would be instant)",
                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                    return;
                }
                _bot ? _bot->GetName() : "Unknown", duration.count());
}

// ============================================================================
// INTERNAL IMPLEMENTATION
// ============================================================================

void LazyManagerFactory::RecordInitTime(std::string const& managerName, std::chrono::milliseconds duration)
{
    std::lock_guard lock(_metricsMutex);
    _totalInitTime += duration;

    TC_LOG_DEBUG("module.playerbot.lazy", "{} initialization time: {}ms (total: {}ms)",
                 managerName, duration.count(), _totalInitTime.count());
}

// Explicit template instantiations for all manager types
template QuestManager* LazyManagerFactory::GetOrCreate(
    std::unique_ptr<QuestManager>&,
    std::atomic<bool>&,
    std::function<std::unique_ptr<QuestManager>()>);

template TradeManager* LazyManagerFactory::GetOrCreate(
    std::unique_ptr<TradeManager>&,
    std::atomic<bool>&,
    std::function<std::unique_ptr<TradeManager>()>);

template GatheringManager* LazyManagerFactory::GetOrCreate(
    std::unique_ptr<GatheringManager>&,
    std::atomic<bool>&,
    std::function<std::unique_ptr<GatheringManager>()>);

template AuctionManager* LazyManagerFactory::GetOrCreate(
    std::unique_ptr<AuctionManager>&,
    std::atomic<bool>&,
    std::function<std::unique_ptr<AuctionManager>()>);

template GroupCoordinator* LazyManagerFactory::GetOrCreate(
    std::unique_ptr<GroupCoordinator>&,
    std::atomic<bool>&,
    std::function<std::unique_ptr<GroupCoordinator>()>);

template DeathRecoveryManager* LazyManagerFactory::GetOrCreate(
    std::unique_ptr<DeathRecoveryManager>&,
    std::atomic<bool>&,
    std::function<std::unique_ptr<DeathRecoveryManager>()>);

} // namespace Playerbot
