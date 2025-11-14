/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#ifndef BOT_SESSION_MGR_H
#define BOT_SESSION_MGR_H

#include "Define.h"
#include "BotSession.h"
#include "ObjectGuid.h"
#include "Threading/LockHierarchy.h"
#include "Core/DI/Interfaces/IBotSessionMgr.h"
#include <unordered_map>
#include <memory>
#include <vector>
#include <mutex>
#include <atomic>

namespace Playerbot {

/**
 * Simple, thread-safe BotSessionMgr without complex threading to avoid deadlocks
 *
 * Implements IBotSessionMgr for dependency injection compatibility.
 * Uses singleton pattern for backward compatibility (transitional).
 *
 * **Migration Path:**
 * - Old code: sBotSessionMgr->GetSession(accountId)
 * - New code: Services::Container().Resolve<IBotSessionMgr>()->GetSession(accountId)
 */
class TC_GAME_API BotSessionMgr final : public IBotSessionMgr
{
public:
    // Thread-safe singleton
    static BotSessionMgr* instance()
    {
        static BotSessionMgr instance;
        return &instance;
    }

    // IBotSessionMgr interface implementation
    bool Initialize() override;
    void Shutdown() override;
    BotSession* CreateSession(uint32 bnetAccountId) override;
    BotSession* CreateSession(uint32 bnetAccountId, ObjectGuid characterGuid) override;
    BotSession* CreateAsyncSession(uint32 bnetAccountId, ObjectGuid characterGuid) override;
    void ReleaseSession(uint32 bnetAccountId) override;
    BotSession* GetSession(uint32 bnetAccountId) const override;
    void UpdateAllSessions(uint32 diff) override;
    bool IsEnabled() const override { return _enabled.load(); }
    void SetEnabled(bool enabled) override { _enabled.store(enabled); }
    uint32 GetActiveSessionCount() const override;
    void TriggerCharacterLoginForAllSessions() override;

private:
    BotSessionMgr() = default;
    ~BotSessionMgr() = default;
    BotSessionMgr(BotSessionMgr const&) = delete;
    BotSessionMgr& operator=(BotSessionMgr const&) = delete;

    // Simple data structures
    ::std::unordered_map<uint32, ::std::unique_ptr<BotSession>> _sessions;
    ::std::vector<BotSession*> _activeSessions;

    // Simple thread safety with deadlock prevention
    mutable OrderedRecursiveMutex<LockOrder::SESSION_MANAGER> _sessionsMutex;

    // State
    ::std::atomic<bool> _enabled{false};
    ::std::atomic<bool> _initialized{false};
};

} // namespace Playerbot

// Global access macro for convenience
#define sBotSessionMgr Playerbot::BotSessionMgr::instance()

#endif // BOT_SESSION_MGR_H