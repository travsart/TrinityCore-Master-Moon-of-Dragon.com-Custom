/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "Define.h"
#include "ObjectGuid.h"
#include <vector>

class Player;

namespace Playerbot
{

/**
 * @brief Interface for bot world session management
 *
 * Manages WorldSession instances for player bots, handling session lifecycle,
 * packet processing, and account-level bot operations.
 */
class TC_GAME_API IBotWorldSessionMgr
{
public:
    virtual ~IBotWorldSessionMgr() = default;

    // Lifecycle management
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;

    // Bot session management
    // bypassLimit: If true, allows this bot to exceed MaxBots limit (used by Instance Bot Pool)
    virtual bool AddPlayerBot(ObjectGuid playerGuid, uint32 masterAccountId = 0, bool bypassLimit = false) = 0;
    virtual void RemovePlayerBot(ObjectGuid playerGuid) = 0;
    virtual void UpdateSessions(uint32 diff) = 0;

    // Packet processing
    virtual uint32 ProcessAllDeferredPackets() = 0;

    // Status queries
    virtual uint32 GetBotCount() const = 0;
    virtual bool IsEnabled() const = 0;
    virtual void SetEnabled(bool enabled) = 0;

    // Session operations
    virtual void TriggerCharacterLoginForAllSessions() = 0;

    // Account-level operations
    virtual ::std::vector<Player*> GetPlayerBotsByAccount(uint32 accountId) const = 0;
    virtual void RemoveAllPlayerBots(uint32 accountId) = 0;
    virtual uint32 GetBotCountByAccount(uint32 accountId) const = 0;

    // All-bots operations (for LFG, BG, etc.)
    virtual ::std::vector<Player*> GetAllBotPlayers() const = 0;
};

} // namespace Playerbot
