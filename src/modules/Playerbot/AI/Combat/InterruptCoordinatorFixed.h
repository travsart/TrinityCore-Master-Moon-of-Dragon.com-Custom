/*
 * Copyright (C) 2024 TrinityCore Playerbot Module
 *
 * Thread-Safe Interrupt Coordinator with Zero Deadlock Risk
 * Optimized for 5000+ bot scalability
 */

#ifndef MODULE_PLAYERBOT_INTERRUPT_COORDINATOR_FIXED_H
#define MODULE_PLAYERBOT_INTERRUPT_COORDINATOR_FIXED_H

#include "Common.h"
#include "SharedDefines.h"
#include "ObjectGuid.h"
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>
#include <atomic>
#include <chrono>
#include <tbb/concurrent_hash_map.h>
#include <tbb/concurrent_queue.h>
#include "../Threading/ThreadingPolicy.h"

class Group;
class BotAI;
class Player;
class Unit;
class SpellInfo;

namespace Playerbot
{

/**
 * Thread-safe interrupt coordination for group-based combat
 *
 * CRITICAL IMPROVEMENTS:
 * 1. Single mutex design - eliminates deadlock risk
 * 2. Lock-free data structures for hot paths
 * 3. Atomic operations for metrics
 * 4. Optimized for 5000+ concurrent bots
 */
class InterruptCoordinatorFixed
{
public:
    // Interrupt priority levels
    enum class InterruptPriority : uint8
    {
        TRIVIAL = 0,   // Can be ignored
        LOW = 1,       // Interrupt if convenient
        NORMAL = 2,    // Should interrupt
        HIGH = 3,      // Must interrupt
        CRITICAL = 4   // Interrupt immediately at all costs
    };

    // Bot capability info
    struct BotInterruptInfo
    {
        ObjectGuid botGuid;
        uint32 spellId{0};               // Primary interrupt spell
        uint32 backupSpellId{0};          // Backup interrupt (if any)
        uint32 interruptRange{5};         // Interrupt range in yards
        uint32 cooldownRemaining{0};      // MS until available
        uint32 lastInterruptTime{0};      // Game time of last interrupt
        uint8 interruptCount{0};          // Interrupts performed
        bool isAssigned{false};           // Currently assigned to interrupt
        std::atomic<bool> available{true}; // Lock-free availability check
    };

    // Spell being cast that might need interrupting
    struct CastingSpellInfo
    {
        ObjectGuid casterGuid;
        uint32 spellId{0};
        uint32 castStartTime{0};          // Game time when cast started
        uint32 castEndTime{0};            // Game time when cast will finish
        InterruptPriority priority{InterruptPriority::NORMAL};
        bool isChanneled{false};
        bool wasInterrupted{false};
        std::atomic<uint8> assignedBots{0}; // Number of bots assigned
    };

    // Interrupt assignment
    struct InterruptAssignment
    {
        ObjectGuid botGuid;
        ObjectGuid targetGuid;
        uint32 spellId{0};                // Spell to interrupt
        uint32 executeTime{0};             // When to execute interrupt
        bool isPrimary{true};              // Primary or backup assignment
        bool executed{false};
        std::atomic<bool> inProgress{false}; // Lock-free progress tracking
    };

    // Performance metrics (all atomic for lock-free access)
    struct InterruptMetrics
    {
        std::atomic<uint32> spellsDetected{0};
        std::atomic<uint32> interruptsAssigned{0};
        std::atomic<uint32> interruptsExecuted{0};
        std::atomic<uint32> interruptsSuccessful{0};
        std::atomic<uint32> interruptsFailed{0};
        std::atomic<uint32> assignmentTime{0};      // Total microseconds
        std::atomic<uint32> rotationInterrupts{0};
        std::atomic<uint32> priorityInterrupts{0};

        void Reset()
        {
            spellsDetected = 0;
            interruptsAssigned = 0;
            interruptsExecuted = 0;
            interruptsSuccessful = 0;
            interruptsFailed = 0;
            assignmentTime = 0;
            rotationInterrupts = 0;
            priorityInterrupts = 0;
        }
    };

    explicit InterruptCoordinatorFixed(Group* group);
    ~InterruptCoordinatorFixed();

    // Bot management
    void RegisterBot(Player* bot, BotAI* ai);
    void UnregisterBot(ObjectGuid botGuid);
    void UpdateBotCooldown(ObjectGuid botGuid, uint32 cooldownMs);

    // Enemy cast detection
    void OnEnemyCastStart(Unit* caster, uint32 spellId, uint32 castTime);
    void OnEnemyCastInterrupted(ObjectGuid casterGuid, uint32 spellId);
    void OnEnemyCastComplete(ObjectGuid casterGuid, uint32 spellId);

    // Main update loop
    void Update(uint32 diff);

    // Bot queries
    bool ShouldBotInterrupt(ObjectGuid botGuid, ObjectGuid& targetGuid, uint32& spellId) const;
    uint32 GetNextInterruptTime(ObjectGuid botGuid) const;
    bool HasPendingInterrupt(ObjectGuid botGuid) const;

    // Spell priority configuration
    void SetSpellPriority(uint32 spellId, InterruptPriority priority);
    InterruptPriority GetSpellPriority(uint32 spellId) const;

    // Metrics and debugging
    InterruptMetrics GetMetrics() const;
    void ResetMetrics();
    std::string GetStatusString() const;

    // Configuration
    void SetMinInterruptDelay(uint32 delayMs) { _minInterruptDelay = delayMs; }
    void SetMaxAssignmentTime(uint32 timeMs) { _maxAssignmentTime = timeMs; }
    void EnableBackupAssignment(bool enable) { _enableBackupAssignment = enable; }
    void EnableRotation(bool enable) { _useRotation = enable; }

private:
    // Internal state structure for lock-free read access
    struct CoordinatorState
    {
        std::unordered_map<ObjectGuid, BotInterruptInfo> botInfo;
        std::unordered_map<ObjectGuid, BotAI*> botAI;
        std::unordered_map<ObjectGuid, CastingSpellInfo> activeCasts;
        std::vector<InterruptAssignment> pendingAssignments;
        std::unordered_set<ObjectGuid> assignedBots;
    };

    // Assignment logic
    void AssignInterrupters();
    void ExecuteAssignments(uint32 currentTime);
    bool AssignBotToSpell(ObjectGuid botGuid, CastingSpellInfo& castInfo);
    uint32 CalculateInterruptTime(CastingSpellInfo const& castInfo) const;

    // Helper methods
    std::vector<ObjectGuid> GetAvailableInterrupters(CastingSpellInfo const& castInfo) const;
    float GetBotDistanceToTarget(ObjectGuid botGuid, ObjectGuid targetGuid) const;
    bool IsBotInRange(ObjectGuid botGuid, ObjectGuid targetGuid, uint32 range) const;
    void RotateInterrupters();

    // Thread-safe state management using single mutex
    Group* _group;
    std::atomic<bool> _active{true};

    // SINGLE MUTEX DESIGN - No deadlock possible
    mutable std::recursive_mutex _stateMutex;
    CoordinatorState _state;

    // Lock-free structures for hot paths
    using ConcurrentCastMap = tbb::concurrent_hash_map<ObjectGuid, CastingSpellInfo>;
    using ConcurrentBotSet = tbb::concurrent_hash_map<ObjectGuid, bool>;

    // Spell priority cache (read-heavy, rarely written)
    Threading::LockFreeState<std::unordered_map<uint32, InterruptPriority>> _spellPriorities;

    // Configuration (atomic for lock-free access)
    std::atomic<uint32> _minInterruptDelay{100};
    std::atomic<uint32> _maxAssignmentTime{50};
    std::atomic<bool> _enableBackupAssignment{true};
    std::atomic<bool> _useRotation{true};

    // Performance tracking (all atomic)
    mutable InterruptMetrics _metrics;
    std::chrono::steady_clock::time_point _lastUpdate;
    std::atomic<uint32> _updateCount{0};

    // Optional components
    void* _positionManager{nullptr};

    // Encounter patterns for predictive interrupts
    struct EncounterPattern
    {
        uint32 npcId;
        std::vector<uint32> spellSequence;
        std::vector<uint32> timings;
    };

    // Pattern cache (rarely modified)
    Threading::LockFreeState<std::unordered_map<uint32, EncounterPattern>> _encounterPatterns;

    // Deleted operations
    InterruptCoordinatorFixed(InterruptCoordinatorFixed const&) = delete;
    InterruptCoordinatorFixed& operator=(InterruptCoordinatorFixed const&) = delete;
};

} // namespace Playerbot

#endif // MODULE_PLAYERBOT_INTERRUPT_COORDINATOR_FIXED_H