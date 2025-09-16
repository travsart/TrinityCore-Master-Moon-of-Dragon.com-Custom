/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#ifndef BOT_SESSION_H
#define BOT_SESSION_H

#include "WorldSession.h"
#include "Define.h"
#include <atomic>
#include <array>
#include <boost/circular_buffer.hpp>
#include <boost/pool/object_pool.hpp>
#include <tbb/concurrent_queue.h>
#include <chrono>
#include <memory>

// Forward declarations
class BotAI;
class PreparedStatement;

namespace Playerbot {

/**
 * CRITICAL IMPLEMENTATION RULES:
 *
 * 1. MEMORY RULES:
 *    - MUST use boost::circular_buffer for bounded queues
 *    - MUST use boost::object_pool for packet allocation
 *    - MUST implement hibernation after 5 minutes inactivity
 *    - MUST achieve < 500KB RAM per active session
 *    - MUST achieve < 5KB RAM per hibernated session
 *
 * 2. CPU RULES:
 *    - NO mutex in hot paths (use atomics only)
 *    - MUST use Intel TBB concurrent structures
 *    - MUST batch process minimum 16 packets
 *    - MUST skip hibernated sessions in Update()
 *    - MUST achieve < 500ns per packet processing
 *
 * 3. TRINITY INTEGRATION RULES:
 *    - ONLY override: SendPacket, QueuePacket, Update
 *    - NEVER modify other WorldSession methods
 *    - MUST use WorldSession's existing Player* management
 *    - MUST initialize with nullptr socket
 */
class TC_GAME_API BotSession final : public WorldSession
{
public:
    explicit BotSession(uint32 bnetAccountId);
    virtual ~BotSession();

    // === MINIMAL WorldSession Overrides ===
    void SendPacket(WorldPacket const* packet, bool forced = false) override;
    void QueuePacket(WorldPacket&& packet) override;
    void Update(uint32 diff) override;

    // Query methods for Trinity compatibility
    bool IsConnectionIdle() const override { return false; }
    uint32 GetLatency() const override { return _simulatedLatency; }

    // === Bot-Specific High-Performance Methods ===

    // Lock-free packet processing with time budget
    void ProcessBotPackets(uint32 maxProcessingTimeUs);

    // Memory optimization via hibernation
    void Hibernate();
    void Reactivate();
    bool IsHibernated() const { return _state.load() == State::HIBERNATED; }

    // Bot metadata queries - Links to Trinity's standard account/character data
    void ExecuteBotMetadataQuery(PreparedStatement* stmt);

    // AI Integration
    void SetAI(std::unique_ptr<BotAI> ai) { _ai = std::move(ai); }
    BotAI* GetAI() const { return _ai.get(); }

    // Performance monitoring
    struct Metrics {
        std::atomic<uint32> packetsProcessed{0};
        std::atomic<uint32> bytesProcessed{0};
        std::atomic<uint32> cpuTimeUs{0};
        std::atomic<size_t> memoryUsage{0};
        std::atomic<uint32> hibernationCount{0};
        std::atomic<uint32> cacheHits{0};
        std::atomic<uint32> cacheMisses{0};
    };

    Metrics const& GetMetrics() const { return _metrics; }
    bool IsBot() const { return true; }
    bool IsActive() const { return _state.load() == State::ACTIVE; }

    // BattleNet account ID (primary account)
    uint32 GetBnetAccountId() const { return _bnetAccountId; }

    // Legacy account ID (for character operations)
    uint32 GetLegacyAccountId() const { return GetAccountId(); }

private:
    // Cache-line aligned state for performance
    alignas(64) struct {
        enum class State : uint8 {
            ACTIVE = 0,
            HIBERNATED = 1,
            DISCONNECTING = 2
        };
        std::atomic<State> state{State::ACTIVE};
    } _state;

    // High-performance packet processing
    alignas(64) struct PacketSystem {
        // TBB concurrent queue for lock-free packet processing
        tbb::concurrent_queue<std::unique_ptr<WorldPacket>> incomingQueue;

        // Boost circular buffer for bounded outgoing packets (prevents memory leaks)
        boost::circular_buffer<std::unique_ptr<WorldPacket>> outgoingBuffer{256};

        // Object pool for zero-allocation packet creation after warmup
        mutable boost::object_pool<WorldPacket> packetPool;

        // Processing batch size (minimum 16 for vectorization)
        static constexpr size_t BATCH_SIZE = 16;
        std::array<std::unique_ptr<WorldPacket>, BATCH_SIZE> processingBatch;

        std::atomic<uint32> queuedPackets{0};
        std::atomic<uint32> droppedPackets{0};
    } _packets;

    // Memory hibernation system
    struct HibernationData {
        std::chrono::steady_clock::time_point lastActivity;
        std::chrono::steady_clock::duration inactivityThreshold{std::chrono::minutes(5)};

        // Hibernated state storage (minimal memory footprint)
        struct HibernatedState {
            uint32 accountId;
            uint32 bnetAccountId;
            std::string lastKnownCharacterName;
            uint32 simulatedLatency;
        };

        std::unique_ptr<HibernatedState> hibernatedState;
        std::atomic<bool> hibernationPending{false};
    } _hibernation;

    // Performance metrics
    mutable Metrics _metrics;

    // Bot AI system
    std::unique_ptr<BotAI> _ai;

    // Account information
    uint32 _bnetAccountId;
    uint32 _simulatedLatency;

    // === Private Implementation Methods ===

    // Packet processing core
    size_t ProcessIncomingPacketBatch();
    void ProcessOutgoingPackets();
    void HandlePacketDrop(WorldPacket const& packet);

    // Memory management
    void CheckHibernationConditions();
    void AllocateHibernationState();
    void FreeActiveMemory();
    void RestoreFromHibernation();

    // Performance optimization
    void UpdateMetrics(uint32 diff);
    void RecordPacketProcessing(size_t packetCount, uint32 processingTimeUs);

    // Account integration with Trinity's systems
    void InitializeAccount();
    void LoadBotMetadata();

    // Deleted copy operations (non-copyable for performance)
    BotSession(BotSession const&) = delete;
    BotSession& operator=(BotSession const&) = delete;
    BotSession(BotSession&&) = delete;
    BotSession& operator=(BotSession&&) = delete;
};

} // namespace Playerbot

#endif // BOT_SESSION_H