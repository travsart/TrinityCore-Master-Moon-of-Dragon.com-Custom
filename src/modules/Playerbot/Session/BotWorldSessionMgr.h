/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#ifndef BOT_WORLD_SESSION_MGR_H
#define BOT_WORLD_SESSION_MGR_H

#include "Define.h"
#include "ObjectGuid.h"
#include "QueryHolder.h"
#include "DatabaseEnvFwd.h"
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <mutex>
#include <atomic>

class WorldSession;
class Player;
class SQLQueryHolderBase;

namespace Playerbot {
    class BotSession;
}

namespace Playerbot {

/**
 * Clean implementation using TrinityCore's native login pattern
 * Based on mod-playerbots' proven approach with modern enhancements
 */
class TC_GAME_API BotWorldSessionMgr final
{
public:
    // Thread-safe singleton
    static BotWorldSessionMgr* instance()
    {
        static BotWorldSessionMgr instance;
        return &instance;
    }

    // Basic lifecycle
    bool Initialize();
    void Shutdown();

    // Bot management using TrinityCore's native login
    bool AddPlayerBot(ObjectGuid playerGuid, uint32 masterAccountId = 0);
    void RemovePlayerBot(ObjectGuid playerGuid);
    Player* GetPlayerBot(ObjectGuid playerGuid) const;

    // Session updates
    void UpdateSessions(uint32 diff);

    // Administrative
    uint32 GetBotCount() const;
    bool IsEnabled() const { return _enabled.load(); }
    void SetEnabled(bool enabled) { _enabled.store(enabled); }

    // Character login trigger (compatibility with existing system)
    void TriggerCharacterLoginForAllSessions();

    // Chat command support - NEW APIs for command system
    std::vector<Player*> GetPlayerBotsByAccount(uint32 accountId) const;
    void RemoveAllPlayerBots(uint32 accountId);
    uint32 GetBotCountByAccount(uint32 accountId) const;

private:
    BotWorldSessionMgr() = default;
    ~BotWorldSessionMgr() = default;
    BotWorldSessionMgr(const BotWorldSessionMgr&) = delete;
    BotWorldSessionMgr& operator=(const BotWorldSessionMgr&) = delete;

    // Native login callback (TrinityCore pattern)
    void HandlePlayerBotLoginCallback(SQLQueryHolderBase const& holder, uint32 masterAccountId, ObjectGuid playerGuid);

    // Character cache synchronization
    bool SynchronizeCharacterCache(ObjectGuid playerGuid);

    // Session management
    std::unordered_map<ObjectGuid, std::shared_ptr<BotSession>> _botSessions;
    std::unordered_set<ObjectGuid> _botsLoading;

    // Thread safety
    mutable std::mutex _sessionsMutex;
    std::atomic<bool> _initialized{false};
    std::atomic<bool> _enabled{false};
};

// Global instance accessor
#define sBotWorldSessionMgr BotWorldSessionMgr::instance()

} // namespace Playerbot

#endif // BOT_WORLD_SESSION_MGR_H