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

    std::lock_guard lock(_mutex);

    // Update all active coordinators
    for (auto& [bgId, coordinator] : _coordinators)
    {
        if (coordinator)
            coordinator->Update(diff);
    }
}

// ============================================================================
// COORDINATOR MANAGEMENT
// ============================================================================

void BattlegroundCoordinatorManager::OnBattlegroundStart(Battleground* bg)
{
    if (!bg || !_initialized)
        return;

    std::lock_guard lock(_mutex);

    uint32 bgInstanceId = bg->GetInstanceID();

    // Check if coordinator already exists
    if (_coordinators.find(bgInstanceId) != _coordinators.end())
    {
        TC_LOG_DEBUG("playerbots.bg.coordinator",
            "BattlegroundCoordinatorManager: Coordinator already exists for BG instance {}",
            bgInstanceId);
        return;
    }

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
        TC_LOG_DEBUG("playerbots.bg.coordinator",
            "BattlegroundCoordinatorManager: No bots in BG instance {}, not creating coordinator",
            bgInstanceId);
        return;
    }

    TC_LOG_INFO("playerbots.bg.coordinator",
        "BattlegroundCoordinatorManager: Creating coordinator for BG instance {} ({}) with {} bots",
        bgInstanceId, bg->GetName(), bots.size());

    // Create coordinator
    auto coordinator = std::make_unique<BattlegroundCoordinator>(bg, bots);
    coordinator->Initialize();

    _coordinators[bgInstanceId] = std::move(coordinator);

    TC_LOG_INFO("playerbots.bg.coordinator",
        "BattlegroundCoordinatorManager: Coordinator created for BG {} (instance {})",
        bg->GetName(), bgInstanceId);
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

    std::lock_guard lock(_mutex);

    auto itr = _coordinators.find(bgInstanceId);
    if (itr == _coordinators.end())
    {
        // Coordinator doesn't exist - might need to create it
        // This handles the case where a bot joins after BG start
        TC_LOG_DEBUG("playerbots.bg.coordinator",
            "BattlegroundCoordinatorManager: Creating coordinator for late-joining bot in BG {}",
            bgInstanceId);

        // Collect all bots in the BG
        std::vector<Player*> bots;
        for (auto const& playerItr : bg->GetPlayers())
        {
            if (Player* player = ObjectAccessor::FindPlayer(playerItr.first))
            {
                if (PlayerBotHooks::IsPlayerBot(player))
                    bots.push_back(player);
            }
        }

        if (!bots.empty())
        {
            auto coordinator = std::make_unique<BattlegroundCoordinator>(bg, bots);
            coordinator->Initialize();
            _coordinators[bgInstanceId] = std::move(coordinator);
            itr = _coordinators.find(bgInstanceId);
        }
    }

    if (itr != _coordinators.end() && itr->second)
    {
        // Ensure the coordinator tracks this bot (handles late-joiners)
        itr->second->AddBot(bot);
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
