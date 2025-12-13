/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#ifndef BOT_SESSION_H
#define BOT_SESSION_H

#include "WorldSession.h"
#include "Threading/LockHierarchy.h"
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
class BotPacketSimulator;
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
    static ::std::shared_ptr<BotSession> Create(uint32 bnetAccountId);

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

    // === Deferred Packet System (Main Thread Processing) ===
    /**
     * @brief Queue packet for main thread execution (thread-safe)
     * @param packet Packet to defer (ownership transferred)
     *
     * Used for packets that modify game state and require serialization
     * with Map::Update() to prevent race conditions.
     *
     * Called from: Bot worker threads (ProcessBotPackets)
     * Executed by: Main world thread (World::UpdateSessions)
     */
    void QueueDeferredPacket(::std::unique_ptr<WorldPacket> packet);

    /**
     * @brief Process all deferred packets (main thread only!)
     * @return Number of packets processed
     *
     * CRITICAL: Must only be called from World::UpdateSessions() on main thread
     * Processes packets that were deferred due to race condition risk.
     */
    uint32 ProcessDeferredPackets();

    /**
     * @brief Check if session has deferred packets pending
     * @return true if deferred packet queue is non-empty
     */
    bool HasDeferredPackets() const;


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

    // ========================================================================
    // THREAD-SAFE FACING SYSTEM
    // ========================================================================
    // SetFacingToObject() manipulates movement splines which is NOT thread-safe.
    // Bot AI runs on worker threads, but movement APIs require main thread.
    // Solution: Queue the facing request atomically, execute on main thread.
    // ========================================================================

    /**
     * @brief Queue a facing request to be executed on main thread (thread-safe)
     * @param targetGuid The GUID of the unit to face
     *
     * Called from: Worker threads (ClassAI::OnCombatUpdate)
     * Executed by: Main thread (BotSession::Update -> ProcessPendingFacing)
     */
    void QueueFacingTarget(ObjectGuid targetGuid);

    /**
     * @brief Process pending facing request on main thread
     * @return true if facing was executed
     *
     * CRITICAL: Must only be called from main thread (BotSession::Update)
     */
    bool ProcessPendingFacing();

    /**
     * @brief Check if there's a pending facing request
     */
    bool HasPendingFacing() const { return !_pendingFacingTarget.IsEmpty(); }

    // ========================================================================
    // THREAD-SAFE MOVEMENT STOP SYSTEM
    // ========================================================================
    // MotionMaster::Clear() is NOT thread-safe (modifies splines, unit state).
    // Bot AI runs on worker threads, but movement APIs require main thread.
    // Solution: Queue the stop request atomically, execute on main thread.
    // ========================================================================

    /**
     * @brief Queue a stop movement request to be executed on main thread (thread-safe)
     *
     * Called from: Worker threads (LeaderFollowBehavior::OnDeactivate, OnGroupLeft)
     * Executed by: Main thread (BotWorldSessionMgr::ProcessAllDeferredPackets)
     */
    void QueueStopMovement();

    /**
     * @brief Process pending stop movement request on main thread
     * @return true if stop was executed
     *
     * CRITICAL: Must only be called from main thread
     */
    bool ProcessPendingStopMovement();

    /**
     * @brief Check if there's a pending stop movement request
     */
    bool HasPendingStopMovement() const { return _pendingStopMovement.load(); }

    // ========================================================================
    // THREAD-SAFE SAFE RESURRECTION SYSTEM (SpawnCorpseBones Crash Fix)
    // ========================================================================
    // HandleReclaimCorpse → SpawnCorpseBones → Map::RemoveWorldObject crashes
    // due to corrupted i_worldObjects tree structure (infinite loop in _Erase).
    //
    // FIX: Instead of using CMSG_RECLAIM_CORPSE packet (which calls SpawnCorpseBones),
    // we queue a safe resurrection request that calls ResurrectPlayer() directly
    // WITHOUT SpawnCorpseBones. The corpse will decay naturally.
    //
    // Thread Safety:
    // - QueueSafeResurrection() called from worker threads (atomic flag)
    // - ProcessPendingSafeResurrection() called from main thread
    // ========================================================================

    /**
     * @brief Queue a safe resurrection request to be executed on main thread
     *
     * Called from: Worker threads (DeathRecoveryManager::HandleAtCorpse)
     * Executed by: Main thread (BotWorldSessionMgr::ProcessAllDeferredPackets)
     *
     * This bypasses HandleReclaimCorpse/SpawnCorpseBones crash by calling
     * ResurrectPlayer() directly on the main thread.
     */
    void QueueSafeResurrection();

    /**
     * @brief Process pending safe resurrection request on main thread
     * @return true if resurrection was executed
     *
     * CRITICAL: Must only be called from main thread
     */
    bool ProcessPendingSafeResurrection();

    /**
     * @brief Check if there's a pending safe resurrection request
     */
    bool HasPendingSafeResurrection() const { return _pendingSafeResurrection.load(); }

    /**
     * @brief Queue a creature for looting on main thread (thread-safe)
     * @param creatureGuid GUID of the creature corpse to loot
     *
     * Called from worker threads to defer SendLoot to main thread.
     * Prevents ACCESS_VIOLATION crash in Map::SendObjectUpdates.
     */
    void QueueLootTarget(ObjectGuid creatureGuid);

    /**
     * @brief Process pending loot targets on main thread
     * @return true if any loot was processed
     *
     * MUST be called from main thread only.
     */
    bool ProcessPendingLoot();

    /**
     * @brief Check if there are pending loot targets
     */
    bool HasPendingLoot() const;

private:
    // Helper methods for safe database access
    CharacterDatabasePreparedStatement* GetSafePreparedStatement(CharacterDatabaseStatements statementId, const char* statementName);

    // Removed AsyncLoginState - using async approach now

    // Account accessors
    uint32 GetBnetAccountId() const { return _bnetAccountId; }
    uint32 GetLegacyAccountId() const { return GetAccountId(); }

private:
    // Simple packet queues - NO TBB, NO BOOST
    ::std::queue<::std::unique_ptr<WorldPacket>> _incomingPackets;
    ::std::queue<::std::unique_ptr<WorldPacket>> _outgoingPackets;
    mutable ::std::recursive_timed_mutex _packetMutex;
    mutable ::std::timed_mutex _updateMutex;  // TIMED MUTEX: Prevents both deadlock AND race conditions

    // Deferred packet queue (main thread processing)
    // Packets that require serialization with Map::Update() to prevent race conditions
    ::std::queue<::std::unique_ptr<WorldPacket>> _deferredPackets;
    mutable OrderedMutex<LockOrder::SESSION_MANAGER> _deferredPacketMutex; // Simple mutex (no recursion needed)

    // Bot state
    ::std::atomic<bool> _active{true};
    ::std::atomic<bool> _destroyed{false};

    // DEADLOCK FIX: Lock-free packet processing flag
    ::std::atomic<bool> _packetProcessing{false};

    // Synchronous login state machine
    ::std::atomic<LoginState> _loginState{LoginState::NONE};

    // Bot AI system
    BotAI* _ai{nullptr};

    // Packet simulation system (Phase 1 refactoring)
    ::std::unique_ptr<BotPacketSimulator> _packetSimulator;

    // Account information
    uint32 _bnetAccountId;
    uint32 _simulatedLatency{50};

    // Player loading state for async operations
    ObjectGuid m_playerLoading;

    // Thread-safe pending facing target
    // Worker threads set this, main thread processes it
    // Uses atomic operations for lock-free thread safety
    mutable std::mutex _facingMutex;
    ObjectGuid _pendingFacingTarget;

    // Thread-safe pending stop movement flag
    // Worker threads set this, main thread processes it
    std::atomic<bool> _pendingStopMovement{false};

    // Thread-safe pending safe resurrection flag (SpawnCorpseBones crash fix)
    // Worker threads set this, main thread processes it
    std::atomic<bool> _pendingSafeResurrection{false};

    // Thread-safe pending loot queue (SendLoot crash fix)
    // Worker threads queue targets, main thread processes loot
    mutable std::mutex _pendingLootMutex;
    std::vector<ObjectGuid> _pendingLootTargets;

    // Deleted copy operations
    BotSession(BotSession const&) = delete;
    BotSession& operator=(BotSession const&) = delete;
    BotSession(BotSession&&) = delete;
    BotSession& operator=(BotSession&&) = delete;
};

} // namespace Playerbot

#endif // BOT_SESSION_H