/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#ifndef BOT_SESSION_H
#define BOT_SESSION_H

#include "WorldSession.h"
#include "WorldSocket.h"
#include "Define.h"
#include "DatabaseEnv.h"
#include "QueryHolder.h"
#include "Socket.h"
#include <atomic>
#include <memory>
#include <queue>
#include <mutex>
#include <chrono>

// Forward declaration for bot socket stub
namespace Playerbot {
    class BotSocketStub;
}

// Forward declarations
namespace Playerbot {
class BotAI;
}

namespace Playerbot {

// BotLoginQueryHolder - simplified LoginQueryHolder for bot sessions following TrinityCore patterns
class BotLoginQueryHolder : public CharacterDatabaseQueryHolder
{
private:
    uint32 m_accountId;
    ObjectGuid m_guid;
public:
    BotLoginQueryHolder(uint32 accountId, ObjectGuid guid)
        : m_accountId(accountId), m_guid(guid) { }
    ObjectGuid GetGuid() const { return m_guid; }
    uint32 GetAccountId() const { return m_accountId; }
    bool Initialize();
};

// Forward declaration - implement in BotSession.cpp
class BotSocket;

/**
 * Simplified BotSession to eliminate TBB deadlock issues
 *
 * DEADLOCK FIX: Removed all TBB and Boost dependencies that were causing
 * "resource deadlock would occur" exceptions during BotSession creation.
 *
 * Uses simple std:: containers with mutex protection instead of complex
 * lock-free structures to ensure stability during character creation.
 */
class TC_GAME_API BotSession final : public WorldSession
{
public:
    explicit BotSession(uint32 bnetAccountId);

    // Factory method to create BotSession with dummy socket
    static std::shared_ptr<BotSession> Create(uint32 bnetAccountId);

    virtual ~BotSession();

    // === WorldSession Overrides ===
    void SendPacket(WorldPacket const* packet, bool forced = false);
    void QueuePacket(WorldPacket* packet);
    bool Update(uint32 diff, PacketFilter& updater);

    // Override socket-related methods to handle socketless sessions
    bool PlayerDisconnected() const;  // Always return false for bots

    // Query methods for Trinity compatibility
    bool IsConnectionIdle() const override { return false; }
    uint32 GetLatency() const { return _simulatedLatency; }

    // Socket safety overrides for bot sessions
    bool HasSocket() const { return false; }
    bool IsSocketOpen() const { return false; }
    void CloseSocket() { /* No socket to close */ }

    // === Bot-Specific Methods ===
    void ProcessBotPackets();

    // Safe callback processing for async login
    void ProcessBotQueryCallbacks();

    // Character Login System
    bool LoginCharacter(ObjectGuid characterGuid);

    // Async login system for 5000 bot scalability
    void StartAsyncLogin(ObjectGuid characterGuid);
    void HandlePlayerLogin(BotLoginQueryHolder const& holder);
    void CompleteAsyncLogin(Player* player, ObjectGuid characterGuid);

    // Bot identification
    bool IsBot() const { return true; }
    bool IsActive() const { return _active.load(); }

    // Async login state checking for race condition prevention
    bool IsAsyncLoginInProgress() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(_asyncLogin.mutex));
        return _asyncLogin.inProgress;
    }

private:
    // Helper methods for safe database access
    CharacterDatabasePreparedStatement* GetSafePreparedStatement(CharacterDatabaseStatements statementId, const char* statementName);

    // Async login state for scalability with thread safety
    struct AsyncLoginState
    {
        std::mutex mutex;
        ObjectGuid characterGuid;
        Player* player = nullptr;
        bool inProgress = false;
        std::chrono::steady_clock::time_point startTime;
    };
    AsyncLoginState _asyncLogin;

    // AI Integration
    void SetAI(BotAI* ai) { _ai = ai; }
    BotAI* GetAI() const { return _ai; }

    // Account accessors
    uint32 GetBnetAccountId() const { return _bnetAccountId; }
    uint32 GetLegacyAccountId() const { return GetAccountId(); }

private:
    // Simple packet queues - NO TBB, NO BOOST
    std::queue<std::unique_ptr<WorldPacket>> _incomingPackets;
    std::queue<std::unique_ptr<WorldPacket>> _outgoingPackets;
    mutable std::mutex _packetMutex;

    // Bot state
    std::atomic<bool> _active{true};

    // Bot AI system
    BotAI* _ai{nullptr};

    // Account information
    uint32 _bnetAccountId;
    uint32 _simulatedLatency{50};

    // Deleted copy operations
    BotSession(BotSession const&) = delete;
    BotSession& operator=(BotSession const&) = delete;
    BotSession(BotSession&&) = delete;
    BotSession& operator=(BotSession&&) = delete;
};

} // namespace Playerbot

#endif // BOT_SESSION_H