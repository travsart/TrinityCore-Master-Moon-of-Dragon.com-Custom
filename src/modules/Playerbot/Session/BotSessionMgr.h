/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#ifndef BOT_SESSION_MGR_H
#define BOT_SESSION_MGR_H

#include "Define.h"
#include "BotSession.h"
#include "ObjectGuid.h"
#include <unordered_map>
#include <memory>
#include <vector>
#include <mutex>
#include <atomic>

namespace Playerbot {

/**
 * Simple, thread-safe BotSessionMgr without complex threading to avoid deadlocks
 */
class TC_GAME_API BotSessionMgr final
{
public:
    // Thread-safe singleton
    static BotSessionMgr* instance()
    {
        static BotSessionMgr instance;
        return &instance;
    }

    // Basic lifecycle
    bool Initialize();
    void Shutdown();

    // Session management
    BotSession* CreateSession(uint32 bnetAccountId);
    BotSession* CreateSession(uint32 bnetAccountId, ObjectGuid characterGuid);
    BotSession* CreateAsyncSession(uint32 bnetAccountId, ObjectGuid characterGuid);
    void ReleaseSession(uint32 bnetAccountId);
    BotSession* GetSession(uint32 bnetAccountId) const;

    // Update operations
    void UpdateAllSessions(uint32 diff);

    // Administrative
    bool IsEnabled() const { return _enabled.load(); }
    void SetEnabled(bool enabled) { _enabled.store(enabled); }
    uint32 GetActiveSessionCount() const;

    // Character login management
    void TriggerCharacterLoginForAllSessions();

private:
    BotSessionMgr() = default;
    ~BotSessionMgr() = default;
    BotSessionMgr(BotSessionMgr const&) = delete;
    BotSessionMgr& operator=(BotSessionMgr const&) = delete;

    // Simple data structures
    std::unordered_map<uint32, std::unique_ptr<BotSession>> _sessions;
    std::vector<BotSession*> _activeSessions;

    // Simple thread safety
    mutable std::mutex _sessionsMutex;

    // State
    std::atomic<bool> _enabled{false};
    std::atomic<bool> _initialized{false};
};

} // namespace Playerbot

// Global access macro for convenience
#define sBotSessionMgr Playerbot::BotSessionMgr::instance()

#endif // BOT_SESSION_MGR_H