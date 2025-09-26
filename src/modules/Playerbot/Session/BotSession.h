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

// Use TrinityCore's native LoginQueryHolder (defined in CharacterHandler.cpp)
// No forward declaration needed - we'll include it in the .cpp file

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
    // Bot-specific login handling (forward declaration moved to public)
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

    explicit BotSession(uint32 bnetAccountId);

    // Factory method to create BotSession with dummy socket
    static std::shared_ptr<BotSession> Create(uint32 bnetAccountId);

    virtual ~BotSession();

    // === WorldSession Overrides ===
    void SendPacket(WorldPacket const* packet, bool forced = false) override;
    void QueuePacket(WorldPacket* packet);
    bool Update(uint32 diff, PacketFilter& updater);

    // Override socket-related methods to handle socketless sessions
    bool PlayerDisconnected() const;  // Always return false for bots

    // Query methods for Trinity compatibility
    bool IsConnectionIdle() const { return false; }
    uint32 GetLatency() const { return _simulatedLatency; }

    // Socket safety overrides for bot sessions
    bool HasSocket() const { return false; }
    bool IsSocketOpen() const { return false; }
    void CloseSocket() { /* No socket to close */ }

    // === Bot-Specific Methods ===
    void ProcessBotPackets();

    // Group invitation handling
    void HandleGroupInvitation(WorldPacket const& packet);


    // Character Login System (SYNCHRONOUS Pattern)
    bool LoginCharacter(ObjectGuid characterGuid);

    // Simplified login state tracking for synchronous operation
    enum class LoginState : uint8
    {
        NONE,                   // Not logging in
        LOGIN_IN_PROGRESS,      // LoginCharacter() executing synchronously
        LOGIN_COMPLETE,         // Login successful
        LOGIN_FAILED            // Login failed
    };

    LoginState GetLoginState() const { return _loginState.load(); }
    bool IsLoginComplete() const { return _loginState.load() == LoginState::LOGIN_COMPLETE; }
    bool IsLoginFailed() const { return _loginState.load() == LoginState::LOGIN_FAILED; }

    // Bot identification
    bool IsBot() const { return true; }
    bool IsActive() const { return _active.load(); }

    // Process pending async login operations
    void ProcessPendingLogin();

    // Handle async login query holder callback
    void HandleBotPlayerLogin(BotLoginQueryHolder const& holder);

    // AI Integration (public access for GroupInvitationHandler)
    void SetAI(BotAI* ai) { _ai = ai; }
    BotAI* GetAI() const { return _ai; }

private:
    // Helper methods for safe database access
    CharacterDatabasePreparedStatement* GetSafePreparedStatement(CharacterDatabaseStatements statementId, const char* statementName);

    // Removed AsyncLoginState - using async approach now

    // Account accessors
    uint32 GetBnetAccountId() const { return _bnetAccountId; }
    uint32 GetLegacyAccountId() const { return GetAccountId(); }

private:
    // Simple packet queues - NO TBB, NO BOOST
    std::queue<std::unique_ptr<WorldPacket>> _incomingPackets;
    std::queue<std::unique_ptr<WorldPacket>> _outgoingPackets;
    mutable std::recursive_timed_mutex _packetMutex;

    // Bot state
    std::atomic<bool> _active{true};
    std::atomic<bool> _destroyed{false};

    // DEADLOCK FIX: Lock-free packet processing flag
    std::atomic<bool> _packetProcessing{false};

    // Synchronous login state machine
    std::atomic<LoginState> _loginState{LoginState::NONE};

    // Bot AI system
    BotAI* _ai{nullptr};

    // Account information
    uint32 _bnetAccountId;
    uint32 _simulatedLatency{50};

    // Player loading state for async operations
    ObjectGuid m_playerLoading;

    // Deleted copy operations
    BotSession(BotSession const&) = delete;
    BotSession& operator=(BotSession const&) = delete;
    BotSession(BotSession&&) = delete;
    BotSession& operator=(BotSession&&) = delete;
};

} // namespace Playerbot

#endif // BOT_SESSION_H