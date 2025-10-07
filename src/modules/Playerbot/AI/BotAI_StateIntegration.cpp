/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * REFACTORED BotAI Implementation with State Machine Integration
 *
 * This implementation fixes Issue #1 by integrating the BotInitStateMachine
 * to properly sequence bot initialization and group joining.
 */

#include "BotAI.h"
#include "Strategy/Strategy.h"
#include "Strategy/GroupCombatStrategy.h"
#include "Strategy/IdleStrategy.h"
#include "Actions/Action.h"
#include "Triggers/Trigger.h"
#include "Group/GroupInvitationHandler.h"
#include "Movement/LeaderFollowBehavior.h"
#include "Game/QuestManager.h"
#include "Social/TradeManager.h"
#include "Professions/GatheringManager.h"
#include "Economy/AuctionManager.h"
#include "Combat/TargetScanner.h"
#include "Player.h"
#include "Unit.h"
#include "Group.h"
#include "ObjectAccessor.h"
#include "MotionMaster.h"
#include "Log.h"
#include "Timer.h"
#include <chrono>

namespace Playerbot
{

// TriggerResultComparator implementation
bool TriggerResultComparator::operator()(TriggerResult const& a, TriggerResult const& b) const
{
    // Higher urgency has higher priority (reverse order for max-heap)
    return a.urgency < b.urgency;
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

BotAI::BotAI(Player* bot)
    : _bot(bot)
    , m_initStateMachine(nullptr)  // Will be created in first UpdateAI
    , m_groupLeader()              // Safe reference, initially empty
{
    if (!_bot)
    {
        TC_LOG_ERROR("playerbots.ai", "BotAI created with null bot pointer");
        return;
    }

    // Initialize performance tracking
    _performanceMetrics.lastUpdate = std::chrono::steady_clock::now();

    // Initialize group management
    _groupInvitationHandler = std::make_unique<GroupInvitationHandler>(_bot);

    // Initialize target scanner for autonomous enemy detection
    _targetScanner = std::make_unique<TargetScanner>(_bot);

    // Initialize all game system managers
    _questManager = std::make_unique<QuestManager>(_bot, this);
    _tradeManager = std::make_unique<TradeManager>(_bot, this);
    _gatheringManager = std::make_unique<GatheringManager>(_bot, this);
    _auctionManager = std::make_unique<AuctionManager>(_bot, this);

    TC_LOG_INFO("module.playerbot", "MANAGERS INITIALIZED: {} - Quest, Trade, Gathering, Auction systems ready",
                _bot->GetName());

    // Initialize default strategies for basic functionality
    InitializeDefaultStrategies();

    // Initialize default triggers
    sBotAIFactory->InitializeDefaultTriggers(this);

    // DON'T check for group here - let state machine handle it
    // This was the old broken code:
    // if (_bot->GetGroup()) {
    //     OnGroupJoined(_bot->GetGroup());  // TOO EARLY!
    // }

    TC_LOG_DEBUG("playerbots.ai", "BotAI created for bot {} - state machine will handle initialization",
                 _bot->GetGUID().ToString());
}

BotAI::~BotAI() = default;

// ============================================================================
// MAIN UPDATE METHOD - CLEAN SINGLE ENTRY POINT with STATE MACHINE
// ============================================================================

void BotAI::UpdateAI(uint32 diff)
{
    // CRITICAL: This is the SINGLE entry point for ALL AI updates
    // No more confusion with DoUpdateAI/UpdateEnhanced

    if (!_bot || !_bot->IsInWorld())
        return;

    // ========================================================================
    // PHASE 1: STATE MACHINE INITIALIZATION (Fixes Issue #1)
    // ========================================================================

    // Create state machine on first update when bot is in world
    if (!m_initStateMachine && _bot->IsInWorld())
    {
        m_initStateMachine = std::make_unique<StateMachine::BotInitStateMachine>(_bot);
        m_initStateMachine->Start();

        TC_LOG_INFO("module.playerbot",
            "BotInitStateMachine created and started for bot {}",
            _bot->GetName());
    }

    // Update state machine until initialization complete
    if (m_initStateMachine && !m_initStateMachine->IsReady())
    {
        m_initStateMachine->Update(diff);

        // Don't process AI logic until initialization complete
        if (!m_initStateMachine->IsReady())
        {
            return; // Skip rest of update
        }

        // State machine just became ready!
        TC_LOG_INFO("module.playerbot",
            "Bot {} initialization complete - now ready for AI updates",
            _bot->GetName());

        // Check if bot was in group at login
        if (m_initStateMachine->WasInGroupAtLogin())
        {
            // The state machine has already verified the bot is IsInWorld()
            // and has cached the group information, so this is safe
            if (Group* group = _bot->GetGroup())
            {
                OnGroupJoined(group);
            }
        }
        else
        {
            // Solo bot - activate idle strategy
            ActivateStrategy("idle");
        }
    }

    // ========================================================================
    // PHASE 2: NORMAL AI UPDATES (only after initialization complete)
    // ========================================================================

    // FIX #22: Populate ObjectCache WITHOUT calling ObjectAccessor
    // Bot code provides objects directly from already-available sources
    // ZERO ObjectAccessor calls = ZERO deadlock risk

    // 1. Cache combat target (from GetVictim - no ObjectAccessor needed)
    if (::Unit* victim = _bot->GetVictim())
        _objectCache.SetTarget(victim);
    else
        _objectCache.SetTarget(nullptr);

    // 2. Cache group data (from GetGroup - no ObjectAccessor needed)
    if (Group* group = _bot->GetGroup())
    {
        // Get group leader from group members directly
        Player* leader = nullptr;
        std::vector<Player*> members;

        for (GroupReference const& itr : group->GetMembers())
        {
            if (Player* member = itr.GetSource())
            {
                members.push_back(member);
                if (member->GetGUID() == group->GetLeaderGUID())
                {
                    leader = member;
                    // Update safe reference
                    SetGroupLeader(leader);
                }
            }
        }

        _objectCache.SetGroupLeader(leader);
        _objectCache.SetGroupMembers(members);

        // Follow target is usually the leader
        if (leader)
            _objectCache.SetFollowTarget(leader);
    }
    else
    {
        _objectCache.SetGroupLeader(nullptr);
        _objectCache.SetGroupMembers({});
        _objectCache.SetFollowTarget(nullptr);
        SetGroupLeader(nullptr);
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    // Track performance
    _performanceMetrics.totalUpdates++;

    // Update internal values and caches
    UpdateValues(diff);

    // Update all active strategies (including follow, idle, social)
    // CRITICAL: Must run every frame for smooth following
    UpdateStrategies(diff);

    // Process all triggers
    ProcessTriggers();

    // Execute queued and triggered actions
    UpdateActions(diff);

    // Update movement based on active strategies
    // CRITICAL: Must run every frame for responsive movement
    UpdateMovement(diff);

    // Update all BehaviorManager-based managers
    UpdateManagers(diff);

    // Check and handle combat state transitions
    UpdateCombatState(diff);

    // If in combat, call class-specific combat update
    if (IsInCombat())
    {
        OnCombatUpdate(diff);
    }
    else if (IsIdle())
    {
        // Update idle behaviors (questing, trading, etc.)
        UpdateIdleBehaviors(diff);
    }

    // Track performance
    auto endTime = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    UpdatePerformanceMetrics(static_cast<uint32>(elapsed.count() / 1000));
}

// ============================================================================
// NEW METHOD: Activate Base Strategies (called by state machine)
// ============================================================================

void BotAI::ActivateBaseStrategies()
{
    // Called by BotInitStateMachine when ready
    // Activate idle strategy by default
    ActivateStrategy("idle");

    TC_LOG_DEBUG("module.playerbot",
        "Base strategies activated for bot {}",
        _bot ? _bot->GetName() : "NULL");
}

// ============================================================================
// MODIFIED: OnGroupJoined to use safe references
// ============================================================================

void BotAI::OnGroupJoined(Group* group)
{
    if (!group || !_bot)
        return;

    // Set group leader using safe reference
    ObjectGuid leaderGuid = group->GetLeaderGUID();
    if (Player* leader = ObjectAccessor::FindPlayer(leaderGuid))
    {
        SetGroupLeader(leader);

        TC_LOG_INFO("module.playerbot",
            "Bot {} joined group, leader set to {}",
            _bot->GetName(), leader->GetName());
    }

    // Activate follow strategy
    ActivateStrategy("follow");

    // Activate group combat strategy
    ActivateStrategy("group_combat");

    // Mark that we're now in a group
    _wasInGroup = true;

    TC_LOG_INFO("module.playerbot",
        "Bot {} activated group strategies - follow and group_combat",
        _bot->GetName());
}

// The rest of the implementation remains the same as the original BotAI.cpp...
// (Include all other methods from the original file here)