/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license
 * Copyright (C) 2008-2016 TrinityCore <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "BattlegroundCoordinatorManager.h"
#include "BattlegroundCoordinator.h"
#include "Battleground.h"
#include "Player.h"
#include "ObjectAccessor.h"
#include "Log.h"
#include "../Core/PlayerBotHooks.h"

namespace Playerbot {

// ============================================================================
// SINGLETON
// ============================================================================

BattlegroundCoordinatorManager* BattlegroundCoordinatorManager::instance()
{
    static BattlegroundCoordinatorManager instance;
    return &instance;
}

BattlegroundCoordinatorManager::BattlegroundCoordinatorManager()
    : _initialized(false)
{
}

BattlegroundCoordinatorManager::~BattlegroundCoordinatorManager()
{
    Shutdown();
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void BattlegroundCoordinatorManager::Initialize()
{
    std::lock_guard lock(_mutex);

    if (_initialized)
        return;

    TC_LOG_INFO("playerbots.bg.coordinator", "BattlegroundCoordinatorManager: Initializing...");

    _coordinators.clear();
    _initialized = true;

    TC_LOG_INFO("playerbots.bg.coordinator", "BattlegroundCoordinatorManager: Initialized");
}

void BattlegroundCoordinatorManager::Shutdown()
{
    std::lock_guard lock(_mutex);

    if (!_initialized)
        return;

    TC_LOG_INFO("playerbots.bg.coordinator", "BattlegroundCoordinatorManager: Shutting down...");

    // Shutdown all coordinators
    for (auto& [bgId, coordinator] : _coordinators)
    {
        if (coordinator)
            coordinator->Shutdown();
    }

    _coordinators.clear();
    _initialized = false;

    TC_LOG_INFO("playerbots.bg.coordinator", "BattlegroundCoordinatorManager: Shut down");
}

void BattlegroundCoordinatorManager::Update(uint32 diff)
{
    if (!_initialized)
        return;

    // ========================================================================
    // COPY-AND-RELEASE: Only hold _mutex for the map snapshot, NOT during
    // coordinator->Update() which can be expensive (spatial cache rebuild,
    // strategy evaluation, script updates). Holding _mutex during Update()
    // would block ALL worker threads calling GetCoordinatorForPlayer() or
    // UpdateBot(), causing 10-30 second thread pool hangs.
    // ========================================================================
    std::vector<BattlegroundCoordinator*> activeCoordinators;
    {
        std::lock_guard lock(_mutex);
        activeCoordinators.reserve(_coordinators.size());
        for (auto& [bgId, coordinator] : _coordinators)
        {
            if (coordinator)
                activeCoordinators.push_back(coordinator.get());
        }
    }
    // _mutex released — update coordinators without blocking other threads
    for (auto* coordinator : activeCoordinators)
        coordinator->Update(diff);
}

// ============================================================================
// COORDINATOR MANAGEMENT
// ============================================================================

void BattlegroundCoordinatorManager::OnBattlegroundStart(Battleground* bg)
{
    if (!bg || !_initialized)
        return;

    uint32 bgInstanceId = bg->GetInstanceID();

    // Quick check under lock: coordinator exists or creation already in progress?
    {
        std::lock_guard lock(_mutex);
        if (_coordinators.find(bgInstanceId) != _coordinators.end())
        {
            TC_LOG_DEBUG("playerbots.bg.coordinator",
                "BattlegroundCoordinatorManager: Coordinator already exists for BG instance {}",
                bgInstanceId);
            return;
        }
        if (_creatingCoordinators.count(bgInstanceId))
        {
            TC_LOG_DEBUG("playerbots.bg.coordinator",
                "BattlegroundCoordinatorManager: Coordinator creation already in progress for BG instance {}",
                bgInstanceId);
            return;
        }
        // Mark creation in progress so other threads don't also try
        _creatingCoordinators.insert(bgInstanceId);
    }
    // _mutex released — collect bots and initialize coordinator without lock

    // Collect all bots in the BG
    std::vector<Player*> bots;
    for (auto const& itr : bg->GetPlayers())
    {
        if (Player* player = ObjectAccessor::FindPlayer(itr.first))
        {
            if (PlayerBotHooks::IsPlayerBot(player))
                bots.push_back(player);
        }
    }

    if (bots.empty())
    {
        // Clear creation flag
        std::lock_guard lock(_mutex);
        _creatingCoordinators.erase(bgInstanceId);
        TC_LOG_DEBUG("playerbots.bg.coordinator",
            "BattlegroundCoordinatorManager: No bots in BG instance {}, not creating coordinator",
            bgInstanceId);
        return;
    }

    TC_LOG_INFO("playerbots.bg.coordinator",
        "BattlegroundCoordinatorManager: Creating coordinator for BG instance {} ({}) with {} bots",
        bgInstanceId, bg->GetName(), bots.size());

    // Create and initialize coordinator WITHOUT holding the lock.
    // Initialize() is expensive: creates sub-managers, spatial cache,
    // loads BG script which calls InitializePositionDiscovery() →
    // FindNearestGameObject(entry, 500.0f) grid scan.
    auto coordinator = std::make_unique<BattlegroundCoordinator>(bg, bots);
    coordinator->Initialize();

    // Re-lock to insert and clear creation flag
    {
        std::lock_guard lock(_mutex);
        _creatingCoordinators.erase(bgInstanceId);
        if (_coordinators.find(bgInstanceId) == _coordinators.end())
        {
            _coordinators[bgInstanceId] = std::move(coordinator);
            TC_LOG_INFO("playerbots.bg.coordinator",
                "BattlegroundCoordinatorManager: Coordinator created for BG {} (instance {})",
                bg->GetName(), bgInstanceId);
        }
        else
        {
            TC_LOG_DEBUG("playerbots.bg.coordinator",
                "BattlegroundCoordinatorManager: Another thread already created coordinator for BG instance {}, discarding duplicate",
                bgInstanceId);
        }
    }
}

void BattlegroundCoordinatorManager::OnBattlegroundEnd(Battleground* bg)
{
    if (!bg || !_initialized)
        return;

    std::lock_guard lock(_mutex);

    uint32 bgInstanceId = bg->GetInstanceID();

    auto itr = _coordinators.find(bgInstanceId);
    if (itr == _coordinators.end())
        return;

    TC_LOG_INFO("playerbots.bg.coordinator",
        "BattlegroundCoordinatorManager: Removing coordinator for BG instance {}",
        bgInstanceId);

    if (itr->second)
        itr->second->Shutdown();

    _coordinators.erase(itr);
}

BattlegroundCoordinator* BattlegroundCoordinatorManager::GetCoordinator(uint32 bgInstanceId)
{
    std::lock_guard lock(_mutex);

    auto itr = _coordinators.find(bgInstanceId);
    if (itr != _coordinators.end())
        return itr->second.get();

    return nullptr;
}

BattlegroundCoordinator* BattlegroundCoordinatorManager::GetCoordinatorForPlayer(Player* player)
{
    if (!player)
        return nullptr;

    Battleground* bg = player->GetBattleground();
    if (!bg)
        return nullptr;

    return GetCoordinator(bg->GetInstanceID());
}

void BattlegroundCoordinatorManager::UpdateBot(Player* bot, uint32 diff)
{
    if (!bot || !_initialized)
        return;

    Battleground* bg = bot->GetBattleground();
    if (!bg || bg->GetStatus() != STATUS_IN_PROGRESS)
        return;

    uint32 bgInstanceId = bg->GetInstanceID();

    // ========================================================================
    // FAST PATH: Coordinator exists — quick AddBot under lock and return.
    // This is the common case after the first update cycle.
    // ========================================================================
    {
        std::lock_guard lock(_mutex);
        auto itr = _coordinators.find(bgInstanceId);
        if (itr != _coordinators.end() && itr->second)
        {
            // Coordinator exists, ensure this bot is tracked (handles late-joiners)
            itr->second->AddBot(bot);
            return;
        }
        // If creation is already in progress by another thread, skip — the
        // coordinator will be available on the next update cycle (500ms).
        if (_creatingCoordinators.count(bgInstanceId))
            return;

        // Mark creation in progress to prevent redundant Initialize() calls
        _creatingCoordinators.insert(bgInstanceId);
    }
    // _mutex released — coordinator doesn't exist, need to create one

    // ========================================================================
    // SLOW PATH: No coordinator yet — create and initialize WITHOUT lock.
    // Initialize() is expensive (grid scans, pathfinding, script loading).
    // Holding _mutex here caused 10-30s thread pool hangs because:
    //   1. Worker thread holds _mutex → runs Initialize() (expensive grid ops)
    //   2. Main thread blocks on _mutex in Update() (can't update anything)
    //   3. Other worker threads block on _mutex in GetCoordinatorForPlayer()
    //   4. Thread pool timeout → bots never execute strategies
    // ========================================================================
    TC_LOG_DEBUG("playerbots.bg.coordinator",
        "BattlegroundCoordinatorManager: Creating coordinator for late-joining bot in BG {}",
        bgInstanceId);

    // Collect all bots in the BG (no lock needed — _mutex protects _coordinators, not BG player list)
    std::vector<Player*> bots;
    for (auto const& playerItr : bg->GetPlayers())
    {
        if (Player* player = ObjectAccessor::FindPlayer(playerItr.first))
        {
            if (PlayerBotHooks::IsPlayerBot(player))
                bots.push_back(player);
        }
    }

    if (bots.empty())
    {
        // Clear creation flag
        std::lock_guard lock(_mutex);
        _creatingCoordinators.erase(bgInstanceId);
        return;
    }

    // Create and initialize without holding the lock
    auto coordinator = std::make_unique<BattlegroundCoordinator>(bg, bots);
    coordinator->Initialize();

    // Re-lock to insert and clear creation flag
    {
        std::lock_guard lock(_mutex);
        _creatingCoordinators.erase(bgInstanceId);
        auto itr = _coordinators.find(bgInstanceId);
        if (itr == _coordinators.end())
        {
            // We won the race — insert our coordinator
            _coordinators[bgInstanceId] = std::move(coordinator);
            TC_LOG_INFO("playerbots.bg.coordinator",
                "BattlegroundCoordinatorManager: Late-join coordinator created for BG instance {} with {} bots",
                bgInstanceId, bots.size());
        }
        else if (itr->second)
        {
            // Another thread created it while we were initializing — just add our bot
            itr->second->AddBot(bot);
            TC_LOG_DEBUG("playerbots.bg.coordinator",
                "BattlegroundCoordinatorManager: Coordinator already created by another thread for BG {}, added bot",
                bgInstanceId);
        }
    }
}

bool BattlegroundCoordinatorManager::HasCoordinator(uint32 bgInstanceId) const
{
    std::lock_guard lock(_mutex);
    return _coordinators.find(bgInstanceId) != _coordinators.end();
}

uint32 BattlegroundCoordinatorManager::GetActiveCoordinatorCount() const
{
    std::lock_guard lock(_mutex);
    return static_cast<uint32>(_coordinators.size());
}

} // namespace Playerbot
