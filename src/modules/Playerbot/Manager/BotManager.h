/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#ifndef BOT_MANAGER_H
#define BOT_MANAGER_H

#include "ObjectGuid.h"
#include "Define.h"
#include <map>
#include <memory>

class Player;
class WorldSession;

namespace Playerbot {

/**
 * Simplified Bot Manager - Direct TrinityCore Integration
 *
 * Based on mod-playerbot's proven approach:
 * - Direct WorldSession creation
 * - Uses TrinityCore's HandlePlayerLogin()
 * - No complex async callbacks
 * - Standard session lifecycle
 *
 * REWRITE GOAL: Clean, simple bot creation without the complex
 * BotSession inheritance and async query holder issues.
 */
class TC_GAME_API BotManager
{
public:
    static BotManager* instance();

    // Core bot operations
    bool SpawnBot(ObjectGuid characterGuid, uint32 masterAccountId = 0);
    void RemoveBot(ObjectGuid characterGuid);
    Player* GetBot(ObjectGuid characterGuid) const;

    // Bulk operations
    void RemoveAllBots();
    uint32 GetActiveBotCount() const;

    // Update cycle
    void Update(uint32 diff);

private:
    BotManager() = default;
    ~BotManager() = default;

    // Bot creation pipeline - mod-playerbot style
    bool CreateBotSession(ObjectGuid characterGuid, uint32 botAccountId, uint32 masterAccountId);
    bool LoadBotFromDatabase(WorldSession* session, ObjectGuid characterGuid);

    // Bot storage
    std::map<ObjectGuid, Player*> _activeBots;
    std::map<ObjectGuid, WorldSession*> _botSessions;

    // Cleanup
    void CleanupBot(ObjectGuid characterGuid);

    // Singleton
    static std::unique_ptr<BotManager> _instance;
};

#define sBotManager BotManager::instance()

} // namespace Playerbot

#endif // BOT_MANAGER_H