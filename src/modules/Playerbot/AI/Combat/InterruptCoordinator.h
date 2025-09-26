/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "Define.h"
#include "ObjectGuid.h"
#include "Position.h"
#include "SharedDefines.h"
#include "Group.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>
#include <mutex>
#include <chrono>
#include <atomic>
#include <queue>

class Unit;
class Player;
class Spell;

namespace Playerbot
{

class BotAI;

// Spell interrupt priority levels
enum class InterruptPriority : uint8
{
    CRITICAL = 1,       // Must interrupt: Fear, Polymorph, Mass heals
    HIGH = 2,           // Should interrupt: Single target heals, damage buffs
    MEDIUM = 3,         // Nice to interrupt: Minor buffs, channeled damage
    LOW = 4,            // Optional: Minor effects
    IGNORE = 5          // Don't interrupt: Harmless spells
};

// Interrupt ability information
struct InterruptAbility
{
    uint32 spellId;
    uint32 cooldownMs;
    float range;
    uint32 lockoutDurationMs;
    Classes requiredClass;
    uint32 requiredLevel;
    bool requiresTarget;
    bool breaksOnDamage;

    InterruptAbility() = default;
    InterruptAbility(uint32 spell, uint32 cd, float r, uint32 lockout, Classes cls, uint32 level = 1, bool target = true, bool breaks = false)
        : spellId(spell), cooldownMs(cd), range(r), lockoutDurationMs(lockout), requiredClass(cls), requiredLevel(level), requiresTarget(target), breaksOnDamage(breaks) {}
};

// Spell casting information for interrupt decision making
struct CastingSpellInfo
{
    ObjectGuid casterGuid;
    uint32 spellId;
    InterruptPriority priority;
    std::chrono::steady_clock::time_point castStart;
    std::chrono::steady_clock::time_point castEnd;
    Position casterPosition;
    uint32 castTimeMs;
    bool isChanneled;
    bool canBeInterrupted;
    uint32 schoolMask;

    CastingSpellInfo() = default;

    bool IsExpired() const
    {
        return std::chrono::steady_clock::now() >= castEnd;
    }

    uint32 GetRemainingCastTime() const
    {
        auto now = std::chrono::steady_clock::now();
        if (now >= castEnd) return 0;
        return static_cast<uint32>(std::chrono::duration_cast<std::chrono::milliseconds>(castEnd - now).count());
    }

    float GetCastProgress() const
    {
        if (castTimeMs == 0) return 1.0f;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - castStart).count();
        return std::min(1.0f, static_cast<float>(elapsed) / castTimeMs);
    }
};

// Bot interrupt capability and cooldown tracking
struct BotInterruptInfo
{
    ObjectGuid botGuid;
    std::vector<InterruptAbility> abilities;
    std::unordered_map<uint32, std::chrono::steady_clock::time_point> cooldowns; // spellId -> ready time
    std::chrono::steady_clock::time_point lastInterrupt;
    uint32 interruptCount = 0;
    bool isInRange = false;
    float distanceToTarget = 0.0f;

    BotInterruptInfo() = default;
    explicit BotInterruptInfo(ObjectGuid guid) : botGuid(guid) {}

    bool HasAvailableInterrupt(float targetDistance = 0.0f) const
    {
        auto now = std::chrono::steady_clock::now();
        for (const auto& ability : abilities)
        {
            auto it = cooldowns.find(ability.spellId);
            bool onCooldown = (it != cooldowns.end() && it->second > now);
            bool inRange = (targetDistance == 0.0f || targetDistance <= ability.range);

            if (!onCooldown && inRange)
                return true;
        }
        return false;
    }

    InterruptAbility const* GetBestAvailableInterrupt(float targetDistance = 0.0f) const
    {
        auto now = std::chrono::steady_clock::now();
        const InterruptAbility* best = nullptr;
        uint32 shortestCooldown = std::numeric_limits<uint32>::max();

        for (const auto& ability : abilities)
        {
            auto it = cooldowns.find(ability.spellId);
            bool onCooldown = (it != cooldowns.end() && it->second > now);
            bool inRange = (targetDistance == 0.0f || targetDistance <= ability.range);

            if (!onCooldown && inRange && ability.cooldownMs < shortestCooldown)
            {
                best = &ability;
                shortestCooldown = ability.cooldownMs;
            }
        }
        return best;
    }
};

// Interrupt assignment for a specific spell cast
struct InterruptAssignment
{
    ObjectGuid assignedBot;
    ObjectGuid targetCaster;
    uint32 targetSpell;
    uint32 interruptSpell;
    std::chrono::steady_clock::time_point assignmentTime;
    std::chrono::steady_clock::time_point executionDeadline;
    bool executed = false;
    bool succeeded = false;

    InterruptAssignment() = default;

    bool IsExpired() const
    {
        return std::chrono::steady_clock::now() >= executionDeadline;
    }

    uint32 GetTimeUntilDeadline() const
    {
        auto now = std::chrono::steady_clock::now();
        if (now >= executionDeadline) return 0;
        return static_cast<uint32>(std::chrono::duration_cast<std::chrono::milliseconds>(executionDeadline - now).count());
    }
};

// Performance metrics for interrupt coordination
struct InterruptMetrics
{
    std::atomic<uint32> spellsDetected{0};
    std::atomic<uint32> interruptsAssigned{0};
    std::atomic<uint32> interruptsExecuted{0};
    std::atomic<uint32> interruptsSucceeded{0};
    std::atomic<uint32> interruptsFailed{0};
    std::atomic<uint32> assignmentFailures{0};
    std::chrono::microseconds averageAssignmentTime{0};
    std::chrono::microseconds maxAssignmentTime{0};

    float GetSuccessRate() const
    {
        uint32 executed = interruptsExecuted.load();
        return executed > 0 ? static_cast<float>(interruptsSucceeded.load()) / executed : 0.0f;
    }

    float GetAssignmentRate() const
    {
        uint32 detected = spellsDetected.load();
        return detected > 0 ? static_cast<float>(interruptsAssigned.load()) / detected : 0.0f;
    }

    void Reset()
    {
        spellsDetected.store(0);
        interruptsAssigned.store(0);
        interruptsExecuted.store(0);
        interruptsSucceeded.store(0);
        interruptsFailed.store(0);
        assignmentFailures.store(0);
        averageAssignmentTime = std::chrono::microseconds{0};
        maxAssignmentTime = std::chrono::microseconds{0};
    }
};

/**
 * Advanced interrupt coordination system for bot groups
 *
 * Manages spell interrupt prioritization, bot rotation, and execution timing
 * to ensure optimal interrupt coverage across group encounters.
 *
 * Features:
 * - Real-time spell cast detection and priority assignment
 * - Intelligent bot rotation to avoid cooldown conflicts
 * - Backup interrupt assignment for critical spells
 * - Performance optimization for 5+ bot scenarios
 * - Integration with combat positioning system
 */
class TC_GAME_API InterruptCoordinator
{
public:
    explicit InterruptCoordinator(Group* group = nullptr);
    ~InterruptCoordinator() = default;

    // === Core Coordination Interface ===

    // Update coordination system (called from combat update loop)
    void Update(uint32 diff);

    // Register/unregister bots for interrupt coordination
    void RegisterBot(Player* bot, BotAI* ai);
    void UnregisterBot(ObjectGuid botGuid);
    void RefreshBotCapabilities(ObjectGuid botGuid);

    // Spell cast detection and assignment
    bool OnSpellCastStart(Unit* caster, Spell const* spell);
    void OnSpellCastFinish(Unit* caster, uint32 spellId, bool interrupted);
    void OnSpellCastCancel(Unit* caster, uint32 spellId);

    // Interrupt execution reporting
    void OnInterruptExecuted(ObjectGuid botGuid, ObjectGuid targetGuid, uint32 interruptSpell, bool succeeded);
    void OnInterruptFailed(ObjectGuid botGuid, ObjectGuid targetGuid, uint32 interruptSpell, std::string const& reason);

    // === Configuration ===

    // Priority management
    void SetSpellPriority(uint32 spellId, InterruptPriority priority);
    InterruptPriority GetSpellPriority(uint32 spellId) const;
    void LoadDefaultPriorities();

    // Coordination settings
    void SetMinInterruptDelay(uint32 delayMs) { _minInterruptDelay = delayMs; }
    void SetMaxAssignmentTime(uint32 timeMs) { _maxAssignmentTime = timeMs; }
    void SetBackupAssignmentEnabled(bool enabled) { _enableBackupAssignment = enabled; }
    void SetRotationMode(bool enabled) { _useRotation = enabled; }

    // === Status and Metrics ===

    // Current state
    bool IsActive() const { return _active.load(); }
    void SetActive(bool active) { _active.store(active); }

    size_t GetRegisteredBotCount() const;
    size_t GetActiveCastCount() const;
    size_t GetPendingAssignmentCount() const;

    // Performance metrics
    InterruptMetrics const& GetMetrics() const { return _metrics; }
    void ResetMetrics();

    // Debug information
    std::vector<CastingSpellInfo> GetActiveCasts() const;
    std::vector<InterruptAssignment> GetPendingAssignments() const;
    std::unordered_map<ObjectGuid, BotInterruptInfo> const& GetBotInfo() const { return _botInfo; }

    // === Advanced Features ===

    // Predictive interrupts for known encounter patterns
    void AddEncounterPattern(uint32 npcId, std::vector<uint32> const& spellSequence, std::vector<uint32> const& timings);
    void ClearEncounterPatterns();

    // Dynamic priority adjustment based on group composition
    void AdjustPrioritiesForGroup();

    // Integration with positioning system (optional)
    void SetPositionManager(void* posManager) { _positionManager = posManager; }

private:
    // === Internal Management ===

    // Assignment algorithm
    bool AssignInterrupt(CastingSpellInfo const& castInfo);
    ObjectGuid SelectBestInterrupter(CastingSpellInfo const& castInfo, std::vector<ObjectGuid>& candidates);
    bool CanBotInterrupt(ObjectGuid botGuid, CastingSpellInfo const& castInfo) const;

    // Bot capability management
    void InitializeBotCapabilities(ObjectGuid botGuid, Player* bot);
    void UpdateBotCooldowns(ObjectGuid botGuid);
    std::vector<InterruptAbility> GetClassInterrupts(Classes playerClass) const;

    // Spell analysis
    InterruptPriority AnalyzeSpellPriority(uint32 spellId, Unit* caster) const;
    bool ShouldInterrupt(uint32 spellId, Unit* caster) const;
    uint32 CalculateInterruptTiming(CastingSpellInfo const& castInfo, InterruptAbility const& ability) const;

    // Cleanup and maintenance
    void CleanupExpiredCasts();
    void CleanupExpiredAssignments();
    void UpdateAssignmentDeadlines();

    // Performance optimization
    void OptimizeForPerformance();

private:
    // Group reference
    Group* _group;
    std::atomic<bool> _active{true};

    // Bot management
    std::unordered_map<ObjectGuid, BotInterruptInfo> _botInfo;
    std::unordered_map<ObjectGuid, BotAI*> _botAI;
    mutable std::shared_mutex _botMutex;

    // Spell tracking
    std::unordered_map<ObjectGuid, CastingSpellInfo> _activeCasts; // casterGuid -> cast info
    std::unordered_map<uint32, InterruptPriority> _spellPriorities;
    mutable std::shared_mutex _castMutex;

    // Assignment tracking
    std::vector<InterruptAssignment> _pendingAssignments;
    std::unordered_set<ObjectGuid> _assignedBots; // Bots with pending assignments
    mutable std::shared_mutex _assignmentMutex;

    // Configuration
    uint32 _minInterruptDelay = 100;        // Min ms before interrupt after cast start
    uint32 _maxAssignmentTime = 50;         // Max ms to spend on assignment algorithm
    bool _enableBackupAssignment = true;    // Assign backup interrupters for critical spells
    bool _useRotation = true;               // Use rotation to distribute interrupts

    // Performance tracking
    mutable InterruptMetrics _metrics;
    std::chrono::steady_clock::time_point _lastUpdate;
    uint32 _updateCount = 0;

    // Integration components (optional)
    void* _positionManager = nullptr;

    // Encounter patterns (future enhancement)
    struct EncounterPattern
    {
        uint32 npcId;
        std::vector<uint32> spellSequence;
        std::vector<uint32> timings;
    };
    std::unordered_map<uint32, EncounterPattern> _encounterPatterns;

    // Thread safety
    mutable std::mutex _metricsMutex;

    // Deleted operations
    InterruptCoordinator(InterruptCoordinator const&) = delete;
    InterruptCoordinator& operator=(InterruptCoordinator const&) = delete;
    InterruptCoordinator(InterruptCoordinator&&) = delete;
    InterruptCoordinator& operator=(InterruptCoordinator&&) = delete;
};

} // namespace Playerbot