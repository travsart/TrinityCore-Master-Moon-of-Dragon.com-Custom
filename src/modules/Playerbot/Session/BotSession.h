/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#ifndef BOT_SESSION_H
#define BOT_SESSION_H

#include "WorldSession.h"
#include "Define.h"
#include "DatabaseEnv.h"
#include <atomic>
#include <memory>
#include <queue>
#include <mutex>
#include <chrono>

// Forward declarations
namespace Playerbot {
class BotAI;
}

namespace Playerbot {

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
    virtual ~BotSession();

    // === WorldSession Overrides ===
    void SendPacket(WorldPacket const* packet, bool forced = false);
    void QueuePacket(WorldPacket* packet);
    bool Update(uint32 diff, PacketFilter& updater);

    // Query methods for Trinity compatibility
    bool IsConnectionIdle() const { return false; }
    uint32 GetLatency() const { return _simulatedLatency; }

    // === Bot-Specific Methods ===
    void ProcessBotPackets();

    // Character Login System
    bool LoginCharacter(ObjectGuid characterGuid);

    // Async login system for 5000 bot scalability
    void StartAsyncLogin(ObjectGuid characterGuid);
    void CompleteAsyncLogin(Player* player, ObjectGuid characterGuid);

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

    // Bot identification
    bool IsBot() const { return true; }
    bool IsActive() const { return _active.load(); }

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