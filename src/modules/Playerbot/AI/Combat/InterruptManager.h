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
#include <unordered_map>
#include <vector>
#include <memory>
#include <atomic>
#include <shared_mutex>
#include <chrono>

// Forward declarations
class Player;
class Unit;
class Spell;
class SpellInfo;

namespace Playerbot
{

// Interrupt priority levels
enum class InterruptPriority : uint8
{
    CRITICAL = 0,           // Must interrupt immediately (healing, fear, etc.)
    HIGH = 1,               // High priority interrupt (major damage, CC)
    MODERATE = 2,           // Standard interrupt priority
    LOW = 3,                // Low priority interrupt
    IGNORE = 4              // Do not interrupt
};

// Interrupt types based on spell effects
enum class InterruptType : uint8
{
    DAMAGE_PREVENTION = 0,  // Prevent high damage spells
    HEALING_DENIAL = 1,     // Prevent healing spells
    CROWD_CONTROL = 2,      // Prevent CC effects
    BUFF_DENIAL = 3,        // Prevent beneficial buffs
    DEBUFF_PREVENTION = 4,  // Prevent harmful debuffs
    CHANNEL_BREAK = 5,      // Break channeled spells
    CAST_DELAY = 6,         // Delay important casts
    RESOURCE_DENIAL = 7     // Prevent resource generation
};

// Interrupt method types
enum class InterruptMethod : uint8
{
    SPELL_INTERRUPT = 0,    // Direct spell interrupt (Counterspell, etc.)
    STUN = 1,              // Stun to interrupt
    KNOCKBACK = 2,         // Knockback interrupt
    SILENCE = 3,           // Silence effect
    DISPEL = 4,            // Dispel the spell
    LINE_OF_SIGHT = 5,     // Break LoS to interrupt
    MOVEMENT = 6,          // Movement-based interrupt
    FEAR = 7               // Fear to interrupt
};

// Interrupt target information
struct InterruptTarget
{
    ObjectGuid guid;
    Unit* unit;
    Position position;
    Spell* currentSpell;
    const SpellInfo* spellInfo;
    uint32 spellId;
    InterruptPriority priority;
    InterruptType type;
    float remainingCastTime;
    float totalCastTime;
    float castProgress;
    uint32 detectedTime;
    uint32 timeWindow;
    bool isChanneled;
    bool isInterruptible;
    bool requiresLoS;
    std::string spellName;
    std::string targetName;

    InterruptTarget() : unit(nullptr), currentSpell(nullptr), spellInfo(nullptr),
                       spellId(0), priority(InterruptPriority::IGNORE),
                       type(InterruptType::DAMAGE_PREVENTION), remainingCastTime(0.0f),
                       totalCastTime(0.0f), castProgress(0.0f), detectedTime(0),
                       timeWindow(0), isChanneled(false), isInterruptible(false),
                       requiresLoS(true) {}
};

// Interrupt capability information
struct InterruptCapability
{
    uint32 spellId;
    std::string spellName;
    InterruptMethod method;
    float range;
    float cooldown;
    uint32 manaCost;
    float castTime;
    bool requiresLoS;
    bool requiresFacing;
    bool isAvailable;
    uint32 lastUsed;
    InterruptPriority minPriority;
    std::vector<InterruptType> effectiveAgainst;

    InterruptCapability() : spellId(0), method(InterruptMethod::SPELL_INTERRUPT),
                           range(0.0f), cooldown(0.0f), manaCost(0), castTime(0.0f),
                           requiresLoS(true), requiresFacing(true), isAvailable(false),
                           lastUsed(0), minPriority(InterruptPriority::MODERATE) {}
};

// Interrupt execution plan
struct InterruptPlan
{
    InterruptTarget* target;
    InterruptCapability* capability;
    InterruptMethod method;
    float executionTime;
    float successProbability;
    float reactionTime;
    bool requiresMovement;
    Position executionPosition;
    uint32 priority;
    std::string reasoning;

    InterruptPlan() : target(nullptr), capability(nullptr), method(InterruptMethod::SPELL_INTERRUPT),
                     executionTime(0.0f), successProbability(0.0f), reactionTime(0.0f),
                     requiresMovement(false), priority(0) {}

    bool operator<(const InterruptPlan& other) const
    {
        if (priority != other.priority)
            return priority < other.priority;
        return successProbability > other.successProbability;
    }
};

// Interrupt execution result
struct InterruptResult
{
    bool success;
    bool attemptMade;
    InterruptMethod usedMethod;
    uint32 usedSpell;
    float timingAccuracy;
    uint32 executionTime;
    std::string failureReason;
    InterruptTarget originalTarget;

    InterruptResult() : success(false), attemptMade(false),
                       usedMethod(InterruptMethod::SPELL_INTERRUPT), usedSpell(0),
                       timingAccuracy(0.0f), executionTime(0) {}
};

// Interrupt performance metrics
struct InterruptMetrics
{
    std::atomic<uint32> interruptAttempts{0};
    std::atomic<uint32> successfulInterrupts{0};
    std::atomic<uint32> failedInterrupts{0};
    std::atomic<uint32> missedOpportunities{0};
    std::atomic<uint32> criticalInterrupts{0};
    std::chrono::microseconds averageReactionTime{0};
    std::chrono::microseconds minReactionTime{std::chrono::microseconds::max()};
    std::chrono::microseconds maxReactionTime{0};
    float averageTimingAccuracy{0.0f};
    std::chrono::steady_clock::time_point lastUpdate;

    void Reset()
    {
        interruptAttempts = 0;
        successfulInterrupts = 0;
        failedInterrupts = 0;
        missedOpportunities = 0;
        criticalInterrupts = 0;
        averageReactionTime = std::chrono::microseconds{0};
        minReactionTime = std::chrono::microseconds::max();
        maxReactionTime = std::chrono::microseconds{0};
        averageTimingAccuracy = 0.0f;
        lastUpdate = std::chrono::steady_clock::now();
    }

    float GetSuccessRate() const
    {
        uint32 total = interruptAttempts.load();
        return total > 0 ? static_cast<float>(successfulInterrupts.load()) / total : 0.0f;
    }
};

class TC_GAME_API InterruptManager
{
public:
    explicit InterruptManager(Player* bot);
    ~InterruptManager() = default;

    // Primary interrupt interface
    void UpdateInterruptSystem(uint32 diff);
    std::vector<InterruptTarget> ScanForInterruptTargets();
    InterruptResult AttemptInterrupt(const InterruptTarget& target);
    void ProcessInterruptOpportunities();

    // Target analysis and prioritization
    InterruptPriority AssessInterruptPriority(const SpellInfo* spellInfo, Unit* caster);
    InterruptType ClassifyInterruptType(const SpellInfo* spellInfo);
    bool IsSpellInterruptWorthy(uint32 spellId, Unit* caster);
    float CalculateInterruptUrgency(const InterruptTarget& target);

    // Capability management
    void InitializeInterruptCapabilities();
    void UpdateInterruptCapabilities();
    std::vector<InterruptCapability> GetAvailableInterrupts();
    InterruptCapability* GetBestInterruptForTarget(const InterruptTarget& target);
    bool CanInterrupt(uint32 spellId, InterruptMethod method = InterruptMethod::SPELL_INTERRUPT);

    // Interrupt planning and execution
    InterruptPlan CreateInterruptPlan(const InterruptTarget& target);
    std::vector<InterruptPlan> GenerateInterruptPlans(const std::vector<InterruptTarget>& targets);
    bool ExecuteInterruptPlan(const InterruptPlan& plan);
    bool IsInterruptExecutable(const InterruptPlan& plan);

    // Spell-specific interrupt logic
    bool ShouldInterruptHealing(const SpellInfo* spellInfo, Unit* caster);
    bool ShouldInterruptCrowdControl(const SpellInfo* spellInfo, Unit* caster);
    bool ShouldInterruptDamage(const SpellInfo* spellInfo, Unit* caster);
    bool ShouldInterruptBuff(const SpellInfo* spellInfo, Unit* caster);

    // Timing and prediction
    float PredictCastCompletion(const InterruptTarget& target);
    float CalculateInterruptWindow(const InterruptTarget& target, InterruptMethod method);
    bool CanInterruptInTime(const InterruptTarget& target, InterruptMethod method);
    uint32 GetOptimalInterruptTiming(const InterruptTarget& target);

    // Group coordination
    void CoordinateInterruptsWithGroup(const std::vector<Player*>& groupMembers);
    bool ShouldLetOthersInterrupt(const InterruptTarget& target);
    void RegisterInterruptAttempt(const InterruptTarget& target);
    void ShareInterruptInformation(const std::vector<Player*>& groupMembers);

    // Advanced interrupt strategies
    void HandleMultipleInterruptTargets();
    void OptimizeInterruptRotation();
    void ManageInterruptCooldowns();
    void HandleInterruptImmunity(Unit* target);

    // Configuration and management
    void SetReactionTime(uint32 reactionTimeMs) { _reactionTime = reactionTimeMs; }
    uint32 GetReactionTime() const { return _reactionTime; }
    void SetInterruptRange(float range) { _maxInterruptRange = range; }
    float GetInterruptRange() const { return _maxInterruptRange; }
    void EnablePredictiveInterrupts(bool enable) { _predictiveInterrupts = enable; }
    bool IsPredictiveInterruptsEnabled() const { return _predictiveInterrupts; }

    // Performance monitoring
    InterruptMetrics const& GetMetrics() const { return _metrics; }
    void ResetMetrics() { _metrics.Reset(); }

    // Query methods
    bool IsCurrentlyInterrupting() const { return _isInterrupting; }
    InterruptTarget* GetCurrentInterruptTarget() { return _currentTarget; }
    uint32 GetInterruptCapabilityCount() const { return static_cast<uint32>(_interruptCapabilities.size()); }

    // Emergency interrupt handling
    void ActivateEmergencyInterruptMode() { _emergencyMode = true; }
    void DeactivateEmergencyInterruptMode() { _emergencyMode = false; }
    bool IsEmergencyMode() const { return _emergencyMode; }
    InterruptResult ExecuteEmergencyInterrupt(Unit* target);

private:
    // Core interrupt detection
    void ScanNearbyUnitsForCasts();
    bool IsValidInterruptTarget(Unit* unit);
    void AnalyzeCastingSpell(Unit* caster);
    void UpdateTargetInformation(InterruptTarget& target);

    // Priority assessment methods
    InterruptPriority AssessHealingPriority(const SpellInfo* spellInfo, Unit* caster);
    InterruptPriority AssessDamagePriority(const SpellInfo* spellInfo, Unit* caster);
    InterruptPriority AssessCrowdControlPriority(const SpellInfo* spellInfo, Unit* caster);
    InterruptPriority AssessBuffPriority(const SpellInfo* spellInfo, Unit* caster);

    // Capability analysis
    void LoadClassSpecificInterrupts();
    void CheckInterruptAvailability();
    void UpdateCooldownStatus();
    float CalculateInterruptEffectiveness(const InterruptCapability& capability, const InterruptTarget& target);

    // Execution methods
    bool CastInterruptSpell(uint32 spellId, Unit* target);
    bool AttemptLoSInterrupt(Unit* target);
    bool AttemptMovementInterrupt(Unit* target);
    void HandleInterruptFailure(const InterruptPlan& plan);

    // Timing calculations
    float CalculateReactionDelay();
    float CalculateExecutionTime(InterruptMethod method);
    float CalculateCastTimeRemaining(Unit* caster);
    bool IsWithinInterruptWindow(const InterruptTarget& target, float executionTime);

    // Group coordination helpers
    bool IsGroupMemberInterrupting(const InterruptTarget& target);
    void BroadcastInterruptIntent(const InterruptTarget& target);
    void ReceiveInterruptCoordination(Player* sender, const InterruptTarget& target);

    // Performance tracking
    void TrackInterruptAttempt(const InterruptResult& result);
    void UpdateReactionTimeMetrics(std::chrono::microseconds reactionTime);
    void UpdateTimingAccuracy(const InterruptResult& result);

    // Utility methods
    bool HasLineOfSightToTarget(Unit* target);
    bool IsInRangeForInterrupt(Unit* target, float range);
    Position CalculateOptimalInterruptPosition(Unit* target);

private:
    Player* _bot;

    // Current state
    bool _isInterrupting;
    InterruptTarget* _currentTarget;
    std::vector<InterruptTarget> _trackedTargets;
    std::vector<InterruptCapability> _interruptCapabilities;

    // Configuration
    uint32 _reactionTime;
    float _maxInterruptRange;
    uint32 _scanInterval;
    bool _predictiveInterrupts;
    bool _emergencyMode;
    float _timingAccuracyTarget;

    // Timing tracking
    uint32 _lastScan;
    uint32 _lastInterruptAttempt;
    std::unordered_map<ObjectGuid, uint32> _targetFirstDetected;

    // Group coordination
    std::unordered_map<ObjectGuid, uint32> _groupInterruptClaims;
    uint32 _lastCoordinationUpdate;

    // Performance metrics
    mutable InterruptMetrics _metrics;

    // Thread safety
    mutable std::shared_mutex _mutex;

    // Constants
    static constexpr uint32 DEFAULT_REACTION_TIME = 250;        // 250ms reaction time
    static constexpr float DEFAULT_MAX_RANGE = 30.0f;          // 30 yards max interrupt range
    static constexpr uint32 DEFAULT_SCAN_INTERVAL = 100;       // 100ms scan interval
    static constexpr float TIMING_ACCURACY_TARGET = 0.8f;      // 80% timing accuracy target
    static constexpr uint32 COORDINATION_UPDATE_INTERVAL = 500; // 500ms coordination updates
    static constexpr uint32 TARGET_TRACKING_DURATION = 5000;   // 5 seconds target tracking
};

// Interrupt utilities and spell database
class TC_GAME_API InterruptUtils
{
public:
    // Spell analysis utilities
    static InterruptPriority GetSpellInterruptPriority(uint32 spellId);
    static InterruptType GetSpellInterruptType(uint32 spellId);
    static bool IsSpellInterruptible(uint32 spellId);
    static bool IsSpellChanneled(uint32 spellId);
    static float GetSpellDangerRating(uint32 spellId);

    // Class-specific interrupt utilities
    static std::vector<uint32> GetClassInterruptSpells(uint8 playerClass);
    static uint32 GetBestInterruptSpell(uint8 playerClass, InterruptType type);
    static bool CanClassInterrupt(uint8 playerClass);
    static float GetClassInterruptRange(uint8 playerClass);

    // Timing calculation utilities
    static float CalculateOptimalInterruptTiming(float totalCastTime, float reactionTime, float executionTime);
    static bool IsInterruptTimingFeasible(float castTimeRemaining, float executionTime, float marginError = 0.1f);
    static float PredictSpellCompletionTime(Unit* caster);
    static uint32 GetInterruptGracePeriod(uint32 spellId);

    // Priority assessment utilities
    static float CalculateHealingThreat(const SpellInfo* spellInfo, Unit* caster);
    static float CalculateDamageThreat(const SpellInfo* spellInfo, Unit* caster);
    static float CalculateCCThreat(const SpellInfo* spellInfo, Unit* caster);
    static bool IsSpellGameChanging(uint32 spellId);

    // Group coordination utilities
    static bool ShouldPrioritizeInterrupt(Player* bot, const InterruptTarget& target, const std::vector<Player*>& group);
    static Player* GetBestInterrupter(const InterruptTarget& target, const std::vector<Player*>& group);
    static void DistributeInterruptTargets(const std::vector<InterruptTarget>& targets, const std::vector<Player*>& group);

    // Database and configuration utilities
    static void LoadInterruptDatabase();
    static void UpdateInterruptPriorities();
    static bool IsInInterruptDatabase(uint32 spellId);
    static InterruptPriority GetConfiguredPriority(uint32 spellId);

    // Emergency interrupt utilities
    static bool RequiresEmergencyInterrupt(uint32 spellId, Unit* caster);
    static std::vector<uint32> GetEmergencyInterruptSpells();
    static bool CanForceInterrupt(Player* bot, Unit* target);
};

} // namespace Playerbot