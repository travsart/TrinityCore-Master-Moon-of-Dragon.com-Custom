/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#ifndef BOT_SESSION_MANAGER_H
#define BOT_SESSION_MANAGER_H

#include "Define.h"

class WorldSession;

namespace Playerbot {

class BotAI;
class BotSession;

/**
 * Static manager for safe bot session updates
 * Prevents infinite recursion and dynamic_cast issues
 */
class TC_GAME_API BotSessionManager
{
public:
    // Static update method called from WorldSession::Update()
    static void UpdateBotSession(WorldSession* session, uint32 diff);

    // Register/unregister bot AI with sessions
    static void RegisterBotAI(WorldSession* session, BotAI* ai);
    static void UnregisterBotAI(WorldSession* session);

    // Safe bot session retrieval
    static BotSession* GetBotSession(WorldSession* session);

private:
    // Internal update logic
    static void ProcessBotAI(WorldSession* session, uint32 diff);
    static void ProcessBotCallbacks(WorldSession* session);

    BotSessionManager() = delete;
    BotSessionManager(const BotSessionManager&) = delete;
    BotSessionManager& operator=(const BotSessionManager&) = delete;
};

} // namespace Playerbot

#endif // BOT_SESSION_MANAGER_H