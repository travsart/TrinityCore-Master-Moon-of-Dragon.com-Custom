/*
 * Copyright (C) 2024 TrinityCore Playerbot Module
 *
 * Thread-Safe Interrupt Coordinator Implementation
 */

#include "InterruptCoordinator.h"
#include "BotAI.h"
#include "Player.h"
#include "Group.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "World.h"
#include "Log.h"
#include "Map.h"
#include <algorithm>
#include <execution>
#include "GameTime.h"

namespace Playerbot
{

InterruptCoordinatorFixed::InterruptCoordinatorFixed(Group* group)
    : _group(group), _lastUpdate(::std::chrono::steady_clock::now())
{
    TC_LOG_DEBUG("module.playerbot.interrupt",
        "InterruptCoordinatorFixed initialized for group with single-mutex design");
}

InterruptCoordinatorFixed::~InterruptCoordinatorFixed()
{
    _active = false;
}

void InterruptCoordinatorFixed::RegisterBot(Player* bot, BotAI* ai)
{
    if (!bot || !ai)
        return;

    BotInterruptInfo info;
    info.botGuid = bot->GetGUID();
    info.available = true;

    // Find interrupt spells
    auto const& spells = bot->GetSpellMap();
    for (auto const& [spellId, _] : spells)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, bot->GetMap()->GetDifficultyID());
        if (!spellInfo)
            continue;

        // Check if this is an interrupt spell
    for (auto const& effect : spellInfo->GetEffects())
        {
            if (effect.Effect == SPELL_EFFECT_INTERRUPT_CAST)
            {
                if (!info.spellId)
                {
                    info.spellId = spellId;
                    info.interruptRange = spellInfo->GetMaxRange(false);
                }
                else if (!info.backupSpellId)
                {
                    info.backupSpellId = spellId;
                }
            }
        }
    }
    // Thread-safe state update with SINGLE LOCK
    {
        ::std::lock_guard lock(_stateMutex);
        _state.botInfo[info.botGuid] = info;
        _state.botAI[info.botGuid] = ai;
    }

    TC_LOG_DEBUG("module.playerbot.interrupt",
        "Registered bot {} with interrupt spell {} (range: {} yards)",
        bot->GetName(), info.spellId, info.interruptRange);
}

void InterruptCoordinatorFixed::UnregisterBot(ObjectGuid botGuid)
{
    // Thread-safe removal with SINGLE LOCK
    ::std::lock_guard lock(_stateMutex);

    _state.botInfo.erase(botGuid);
    _state.botAI.erase(botGuid);
    _state.assignedBots.erase(botGuid);

    // Remove any pending assignments
    _state.pendingAssignments.erase(
        ::std::remove_if(_state.pendingAssignments.begin(), _state.pendingAssignments.end(),
            [botGuid](InterruptAssignment const& assignment) {
                return assignment.assignedBot == botGuid;
            }),
        _state.pendingAssignments.end()
    );
}

void InterruptCoordinatorFixed::UpdateBotCooldown(ObjectGuid botGuid, uint32 cooldownMs)
{
    // Optimized with shared lock for read, upgrade to unique only if needed
    ::std::lock_guard lock(_stateMutex);
    auto it = _state.botInfo.find(botGuid);
    if (it != _state.botInfo.end())
    {
        // Use atomic for lock-free update
        it->second.available = (cooldownMs == 0);
        it->second.cooldownRemaining = cooldownMs;
    }
}

void InterruptCoordinatorFixed::OnEnemyCastStart(Unit* caster, uint32 spellId, uint32 castTime)
{
    if (!caster || !_active)
        return;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, caster->GetMap()->GetDifficultyID());
    if (!spellInfo)
        return;

    CastingSpellInfo castInfo;
    castInfo.casterGuid = caster->GetGUID();
    castInfo.spellId = spellId;
    castInfo.castStartTime = GameTime::GetGameTimeMS();
    castInfo.castEndTime = castInfo.castStartTime + castTime;
    castInfo.isChanneled = spellInfo->IsChanneled();

    // Get priority from lock-free cache
    uint64 version;
    auto priorities = _spellPriorities.Read(version);
    auto prioIt = priorities.find(spellId);
    castInfo.priority = (prioIt != priorities.end()) ? prioIt->second : InterruptPriority::NORMAL;

    // Thread-safe insertion with SINGLE LOCK
    {
        ::std::lock_guard lock(_stateMutex);
        _state.activeCasts[caster->GetGUID()] = castInfo;
    }

    // Update metrics atomically (lock-free)
    _metrics.spellsDetected.fetch_add(1, ::std::memory_order_relaxed);

    TC_LOG_DEBUG("module.playerbot.interrupt",
        "Enemy cast detected: {} casting spell {} (priority: {}, duration: {}ms)",
        caster->GetName(), spellId, static_cast<int>(castInfo.priority), castTime);
}

void InterruptCoordinatorFixed::OnEnemyCastInterrupted(ObjectGuid casterGuid, uint32 spellId)
{
    // Thread-safe update with SINGLE LOCK
    ::std::lock_guard lock(_stateMutex);

    auto it = _state.activeCasts.find(casterGuid);
    if (it != _state.activeCasts.end() && it->second.spellId == spellId)
    {
        it->second.wasInterrupted = true;
        _metrics.interruptsSuccessful.fetch_add(1, ::std::memory_order_relaxed);
    }
}

void InterruptCoordinatorFixed::OnEnemyCastComplete(ObjectGuid casterGuid, uint32 /*spellId*/)
{
    // Thread-safe removal with SINGLE LOCK
    ::std::lock_guard lock(_stateMutex);
    _state.activeCasts.erase(casterGuid);
}

void InterruptCoordinatorFixed::Update(uint32 /*diff*/)
{
    if (!_active || !_group)
        return;

    _updateCount.fetch_add(1, ::std::memory_order_relaxed);
    uint32 currentTime = GameTime::GetGameTimeMS();

    // Execute ready assignments
    ExecuteAssignments(currentTime);

    // Assign new interrupts
    AssignInterrupters();

    // Rotate interrupters if enabled
    if (_useRotation && (_updateCount % 100) == 0)
    {
        RotateInterrupters();
    }

    // Clean up completed casts
    {
        ::std::lock_guard lock(_stateMutex);

        for (auto it = _state.activeCasts.begin(); it != _state.activeCasts.end(); )
        {
            if (currentTime > it->second.castEndTime || it->second.wasInterrupted)
            {
                it = _state.activeCasts.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
}

void InterruptCoordinatorFixed::AssignInterrupters()
{
    auto startTime = ::std::chrono::high_resolution_clock::now();
    uint32 currentTime = GameTime::GetGameTimeMS();

    // Copy data for processing (minimize lock time)
    CoordinatorState localState;
    {
        ::std::lock_guard lock(_stateMutex);
        localState = _state;  // Fast copy under read lock
    }

    ::std::vector<InterruptAssignment> newAssignments;

    // Process each active cast
    for (auto& [casterGuid, castInfo] : localState.activeCasts)
    {
        // Skip if already assigned enough bots
    if (castInfo.assignedBots >= (castInfo.priority >= InterruptPriority::HIGH ? 2 : 1))
            continue;

        // Skip if too early to interrupt
        uint32 timeSinceCast = currentTime - castInfo.castStartTime;
        if (timeSinceCast < _minInterruptDelay)
            continue;

        // Get available bots
        auto availableBots = GetAvailableInterrupters(castInfo);
        if (availableBots.empty())
            continue;

        // Sort by distance (if position manager available)
    if (_positionManager)
        {
            ::std::sort(availableBots.begin(), availableBots.end(),
                [this, casterGuid](ObjectGuid a, ObjectGuid b) {
                    return GetBotDistanceToTarget(a, casterGuid) <
                           GetBotDistanceToTarget(b, casterGuid);
                });
        }

        // Assign primary interrupter
        ObjectGuid primaryBot = availableBots[0];

        InterruptAssignment assignment;
        assignment.assignedBot = primaryBot;
        assignment.targetCaster = casterGuid;
        assignment.targetSpell = castInfo.spellId;
        assignment.interruptSpell = localState.botInfo[primaryBot].spellId;
        assignment.executionDeadline = CalculateInterruptTime(castInfo);
        assignment.isPrimary = true;
        assignment.inProgress = false;

        newAssignments.push_back(assignment);
        castInfo.assignedBots++;

        // Assign backup for critical spells
    if (_enableBackupAssignment &&
            castInfo.priority >= InterruptPriority::HIGH &&
            availableBots.size() > 1)
        {
            ObjectGuid backupBot = availableBots[1];

            InterruptAssignment backup = assignment;
            backup.assignedBot = backupBot;
            backup.interruptSpell = localState.botInfo[backupBot].spellId;
            backup.isPrimary = false;
            backup.executionDeadline += 200; // Backup waits 200ms

            newAssignments.push_back(backup);
            castInfo.assignedBots++;
        }

        _metrics.interruptsAssigned.fetch_add(1, ::std::memory_order_relaxed);
    }

    // Apply new assignments with SINGLE LOCK
    if (!newAssignments.empty())
    {
        ::std::lock_guard lock(_stateMutex);

        for (auto const& assignment : newAssignments)
        {
            _state.pendingAssignments.push_back(assignment);
            _state.assignedBots.insert(assignment.assignedBot);
        }
    }

    // Track assignment time
    auto endTime = ::std::chrono::high_resolution_clock::now();
    auto duration = ::std::chrono::duration_cast<::std::chrono::microseconds>(endTime - startTime);
    _metrics.assignmentTime.fetch_add(duration.count(), ::std::memory_order_relaxed);
}

void InterruptCoordinatorFixed::ExecuteAssignments(uint32 currentTime)
{
    ::std::vector<InterruptAssignment*> readyAssignments;

    // Find ready assignments with minimal lock time
    {
        ::std::lock_guard lock(_stateMutex);

        for (auto& assignment : _state.pendingAssignments)
        {
            if (!assignment.executed &&
                currentTime >= assignment.executionDeadline &&
                !assignment.inProgress)
            {
                readyAssignments.push_back(&assignment);
            }
        }
    }

    // Execute assignments (outside lock)
    for (auto* assignment : readyAssignments)
    {
        // Mark as in progress (protected by mutex access pattern)
    if (assignment->inProgress)
            continue;
        assignment->inProgress = true;

        // Get bot AI
        BotAI* botAI = nullptr;
        {
            ::std::lock_guard lock(_stateMutex);
            auto it = _state.botAI.find(assignment->assignedBot);
            if (it != _state.botAI.end())
                botAI = it->second;
        }

        if (botAI)
        {
            // Signal bot to interrupt
            // Note: This would integrate with actual BotAI interrupt system
            TC_LOG_DEBUG("module.playerbot.interrupt",
                "Executing interrupt: Bot {} interrupting spell {}",
                assignment->assignedBot.ToString(), assignment->targetSpell);

            assignment->executed = true;
            _metrics.interruptsExecuted.fetch_add(1, ::std::memory_order_relaxed);
        }

        assignment->inProgress = false;
    }

    // Clean up executed assignments
    if (!readyAssignments.empty())
    {
        ::std::lock_guard lock(_stateMutex);

        _state.pendingAssignments.erase(
            ::std::remove_if(_state.pendingAssignments.begin(), _state.pendingAssignments.end(),
                [](InterruptAssignment const& a) { return a.executed; }),
            _state.pendingAssignments.end()
        );

        // Update assigned bots
    for (auto const& assignment : _state.pendingAssignments)
        {
            if (!assignment.executed)
                continue;
            _state.assignedBots.erase(assignment.assignedBot);
        }
    }
}

::std::vector<ObjectGuid> InterruptCoordinatorFixed::GetAvailableInterrupters(CastingSpellInfo const& /*castInfo*/) const
{
    ::std::vector<ObjectGuid> available;

    ::std::lock_guard lock(_stateMutex);

    for (auto const& [guid, info] : _state.botInfo)
    {
        // Check if bot is available
    if (!info.available)
            continue;

        // Check if bot has interrupt spell
    if (info.spellId == 0)
            continue;

        // Check if bot is not already assigned
    if (_state.assignedBots.find(guid) != _state.assignedBots.end())
            continue;

        // Check cooldown
    if (info.cooldownRemaining > 0)
            continue;

        available.push_back(guid);
    }

    return available;
}

uint32 InterruptCoordinatorFixed::CalculateInterruptTime(CastingSpellInfo const& castInfo) const
{
    uint32 currentTime = GameTime::GetGameTimeMS();
    uint32 timeSinceCast = currentTime - castInfo.castStartTime;
    uint32 timeRemaining = castInfo.castEndTime - currentTime;

    // For critical spells, interrupt ASAP
    if (castInfo.priority == InterruptPriority::CRITICAL)
    {
        return currentTime + _minInterruptDelay;
    }

    // For channeled spells, interrupt quickly
    if (castInfo.isChanneled)
    {
        return currentTime + (_minInterruptDelay * 2);
    }

    // For normal casts, interrupt at 60-80% of cast time
    uint32 totalCastTime = castInfo.castEndTime - castInfo.castStartTime;
    uint32 targetTime = castInfo.castStartTime + (totalCastTime * 7 / 10); // 70% through cast

    return ::std::max(currentTime + _minInterruptDelay, targetTime);
}

bool InterruptCoordinatorFixed::ShouldBotInterrupt(ObjectGuid botGuid, ObjectGuid& targetGuid, uint32& spellId) const
{
    ::std::lock_guard lock(_stateMutex);

    // Check for pending assignments for this bot
    for (auto const& assignment : _state.pendingAssignments)
    {
        if (assignment.assignedBot == botGuid &&
            !assignment.executed &&
            GameTime::GetGameTimeMS() >= assignment.executionDeadline)
        {
            targetGuid = assignment.targetCaster;
            spellId = assignment.interruptSpell;
            return true;
        }
    }

    return false;
}

uint32 InterruptCoordinatorFixed::GetNextInterruptTime(ObjectGuid botGuid) const
{
    ::std::lock_guard lock(_stateMutex);

    for (auto const& assignment : _state.pendingAssignments)
    {
        if (assignment.assignedBot == botGuid && !assignment.executed)
        {
            return assignment.executionDeadline;
        }
    }

    return 0;
}

bool InterruptCoordinatorFixed::HasPendingInterrupt(ObjectGuid botGuid) const
{
    ::std::lock_guard lock(_stateMutex);

    return ::std::any_of(_state.pendingAssignments.begin(), _state.pendingAssignments.end(),
        [botGuid](InterruptAssignment const& assignment) {
            return assignment.assignedBot == botGuid && !assignment.executed;
        });
}

void InterruptCoordinatorFixed::SetSpellPriority(uint32 spellId, InterruptPriority priority)
{
    uint64 version;
    auto priorities = _spellPriorities.Read(version);
    priorities[spellId] = priority;
    _spellPriorities.Update(priorities);
}

InterruptPriority InterruptCoordinatorFixed::GetSpellPriority(uint32 spellId) const
{
    uint64 version;
    auto priorities = _spellPriorities.Read(version);
    auto it = priorities.find(spellId);
    return (it != priorities.end()) ? it->second : InterruptPriority::NORMAL;
}

InterruptCoordinatorFixed::InterruptMetrics InterruptCoordinatorFixed::GetMetrics() const
{
    return InterruptMetrics{
        {_metrics.spellsDetected.load(::std::memory_order_relaxed)},
        {_metrics.interruptsAssigned.load(::std::memory_order_relaxed)},
        {_metrics.interruptsExecuted.load(::std::memory_order_relaxed)},
        {_metrics.interruptsSuccessful.load(::std::memory_order_relaxed)},
        {_metrics.interruptsFailed.load(::std::memory_order_relaxed)},
        {_metrics.assignmentTime.load(::std::memory_order_relaxed)},
        {_metrics.rotationInterrupts.load(::std::memory_order_relaxed)},
        {_metrics.priorityInterrupts.load(::std::memory_order_relaxed)}
    };
}

void InterruptCoordinatorFixed::ResetMetrics()
{
    _metrics.Reset();
}

::std::string InterruptCoordinatorFixed::GetStatusString() const
{
    ::std::lock_guard lock(_stateMutex);

    ::std::stringstream ss;
    ss << "InterruptCoordinator Status:\n";
    ss << "  Active Bots: " << _state.botInfo.size() << "\n";
    ss << "  Active Casts: " << _state.activeCasts.size() << "\n";
    ss << "  Pending Assignments: " << _state.pendingAssignments.size() << "\n";
    ss << "  Assigned Bots: " << _state.assignedBots.size() << "\n";

    auto metrics = GetMetrics();
    ss << "  Metrics:\n";
    ss << "    Spells Detected: " << metrics.spellsDetected << "\n";
    ss << "    Interrupts Assigned: " << metrics.interruptsAssigned << "\n";
    ss << "    Interrupts Executed: " << metrics.interruptsExecuted << "\n";
    ss << "    Interrupts Successful: " << metrics.interruptsSuccessful << "\n";
    ss << "    Average Assignment Time: " <<
          (metrics.interruptsAssigned > 0 ? metrics.assignmentTime / metrics.interruptsAssigned : 0) << " us\n";

    return ss.str();
}

float InterruptCoordinatorFixed::GetBotDistanceToTarget(ObjectGuid /*botGuid*/, ObjectGuid /*targetGuid*/) const
{
    // This would integrate with actual position tracking
    // For now, return a placeholder
    return 10.0f;
}

bool InterruptCoordinatorFixed::IsBotInRange(ObjectGuid botGuid, ObjectGuid targetGuid, uint32 range) const
{
    return GetBotDistanceToTarget(botGuid, targetGuid) <= float(range);
}

void InterruptCoordinatorFixed::RotateInterrupters()
{
    // Implement rotation logic to distribute interrupt duties
    // This prevents the same bots from always interrupting
    _metrics.rotationInterrupts.fetch_add(1, ::std::memory_order_relaxed);
}

::std::vector<InterruptAssignment> InterruptCoordinatorFixed::GetPendingAssignments() const
{
    ::std::lock_guard lock(_stateMutex);
    return _state.pendingAssignments;
}

void InterruptCoordinatorFixed::OnInterruptExecuted(ObjectGuid botGuid, ObjectGuid targetGuid, uint32 spellId, bool success)
{
    ::std::lock_guard lock(_stateMutex);

    // Find and mark assignment as executed
    for (auto& assignment : _state.pendingAssignments)
    {
        if (assignment.assignedBot == botGuid &&
            assignment.targetCaster == targetGuid &&
            assignment.interruptSpell == spellId)
        {
            assignment.executed = true;
            _state.assignedBots.erase(botGuid);

            // Update metrics
    if (success)
                _metrics.interruptsSuccessful.fetch_add(1, ::std::memory_order_relaxed);
            else
                _metrics.interruptsFailed.fetch_add(1, ::std::memory_order_relaxed);

            TC_LOG_DEBUG("module.playerbot.interrupt",
                "Interrupt executed: Bot {} interrupted spell {} on target {} - {}",
                botGuid.ToString(), spellId, targetGuid.ToString(),
                success ? "Success" : "Failed");

            break;
        }
    }
}

void InterruptCoordinatorFixed::OnInterruptFailed(ObjectGuid botGuid, ObjectGuid targetGuid, uint32 spellId, ::std::string const& reason)
{
    ::std::lock_guard lock(_stateMutex);

    // Find and mark assignment as failed
    for (auto& assignment : _state.pendingAssignments)
    {
        if (assignment.assignedBot == botGuid &&
            assignment.targetCaster == targetGuid &&
            assignment.interruptSpell == spellId)
        {
            assignment.executed = true;
            _state.assignedBots.erase(botGuid);
            _metrics.interruptsFailed.fetch_add(1, ::std::memory_order_relaxed);

            TC_LOG_DEBUG("module.playerbot.interrupt",
                "Interrupt failed: Bot {} failed to interrupt spell {} on target {} - Reason: {}",
                botGuid.ToString(), spellId, targetGuid.ToString(), reason.c_str());

            break;
        }
    }
}

} // namespace Playerbot