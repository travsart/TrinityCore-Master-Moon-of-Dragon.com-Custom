/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * REFACTORED BotAI Implementation - Clean Update Chain
 *
 * This implementation provides:
 * 1. Single clean UpdateAI() method - no DoUpdateAI/UpdateEnhanced confusion
 * 2. Clear separation of base behaviors and combat specialization
 * 3. Every-frame updates for smooth movement and following
 * 4. Proper delegation to ClassAI for combat-only updates
 */

#include "BotAI.h"
#include "BehaviorPriorityManager.h"
#include "Strategy/Strategy.h"
#include "Strategy/GroupCombatStrategy.h"
#include "Strategy/SoloCombatStrategy.h"
#include "Strategy/SoloStrategy.h"
#include "Strategy/QuestStrategy.h"
#include "Strategy/LootStrategy.h"
#include "Strategy/RestStrategy.h"
#include "Actions/Action.h"
#include "Triggers/Trigger.h"
#include "Group/GroupInvitationHandler.h"
#include "Movement/LeaderFollowBehavior.h"
#include "Game/QuestManager.h"
#include "Social/TradeManager.h"
#include "Professions/GatheringManager.h"
#include "Economy/AuctionManager.h"
#include "Combat/TargetScanner.h"
#include "Combat/CombatStateManager.h"
#include "Equipment/EquipmentManager.h"
#include "Professions/ProfessionManager.h"
#include "Advanced/GroupCoordinator.h"
#include "Spatial/SpatialGridManager.h"
#include "Spatial/DoubleBufferedSpatialGrid.h"
// Phase 7.3: Direct EventDispatcher integration (BotEventSystem and Observers removed as dead code)
#include "Core/Events/EventDispatcher.h"
#include "Core/Managers/ManagerRegistry.h"
#include "Lifecycle/DeathRecoveryManager.h"
#include "Movement/Arbiter/MovementArbiter.h"
#include "Movement/Arbiter/MovementRequest.h"
#include "Movement/Arbiter/MovementPriorityMapper.h"
#include "Session/BotPriorityManager.h"
#include "Player.h"
#include "Unit.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "ThreatManager.h"
#include "Group.h"
#include "ObjectAccessor.h"
#include "MotionMaster.h"
#include "WorldSession.h"
#include "Log.h"
#include "Timer.h"
#include "DatabaseEnv.h"
#include "QueryResult.h"
#include <chrono>
#include <set>
#include <unordered_map>

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

BotAI::BotAI(Player* bot) : _bot(bot)
{
    if (!_bot)
    {
        TC_LOG_ERROR("playerbots.ai", "BotAI created with null bot pointer");
        return;
    }

    // Initialize performance tracking
    _performanceMetrics.lastUpdate = std::chrono::steady_clock::now();

    // Initialize priority-based behavior manager
    _priorityManager = std::make_unique<BehaviorPriorityManager>(this);

    // Initialize group management
    _groupInvitationHandler = std::make_unique<GroupInvitationHandler>(_bot);

    // Initialize target scanner for autonomous enemy detection
    _targetScanner = std::make_unique<TargetScanner>(_bot);

    // Initialize all game system managers
    _questManager = std::make_unique<QuestManager>(_bot, this);
    _tradeManager = std::make_unique<TradeManager>(_bot, this);
    _gatheringManager = std::make_unique<GatheringManager>(_bot, this);
    _auctionManager = std::make_unique<AuctionManager>(_bot, this);
    _groupCoordinator = std::make_unique<GroupCoordinator>(_bot, this);

    // Initialize death recovery system
    _deathRecoveryManager = std::make_unique<DeathRecoveryManager>(_bot, this);

    // Initialize movement arbiter for priority-based movement request arbitration
    _movementArbiter = std::make_unique<MovementArbiter>(_bot);

    // Initialize combat state manager for automatic combat state synchronization
    _combatStateManager = std::make_unique<CombatStateManager>(_bot, this);

    // Phase 4: Initialize Shared Blackboard (thread-safe shared state system)
    _sharedBlackboard = BlackboardManager::GetBotBlackboard(_bot->GetGUID());

    // Phase 2 Week 3: Initialize Hybrid AI Decision System (Utility AI + Behavior Trees)
    InitializeHybridAI();

    TC_LOG_INFO("module.playerbot", "📋 MANAGERS INITIALIZED: {} - Quest, Trade, Gathering, Auction, Group, DeathRecovery, MovementArbiter, CombatState, SharedBlackboard, HybridAI systems ready",
                _bot->GetName());

    // Phase 7.1: Initialize event dispatcher and manager registry
    _eventDispatcher = std::make_unique<Events::EventDispatcher>(512);  // Initial queue size: 512 events
    _managerRegistry = std::make_unique<ManagerRegistry>();

    TC_LOG_INFO("module.playerbot", "🔄 EVENT DISPATCHER & MANAGER REGISTRY: {} - Phase 7.1 integration ready",
                _bot->GetName());

    // Phase 7.3: Legacy Phase 6 observer system removed (dead code)
    // Events now flow directly: PlayerbotEventScripts → EventDispatcher → Managers

    // Phase 7.1: Register managers with ManagerRegistry and subscribe to events
    // Events flow: TrinityCore ScriptMgr → PlayerbotEventScripts → EventDispatcher → Managers
    if (_managerRegistry && _eventDispatcher)
    {
        // Note: We can't transfer ownership yet since managers are still used directly
        // For now, we just initialize them through the registry
        // Full migration to ManagerRegistry will happen after testing

        // Initialize managers through IManagerBase interface
        if (_questManager)
        {
            _questManager->Initialize();
            TC_LOG_INFO("module.playerbot.managers", "✅ QuestManager initialized via IManagerBase");

            // Subscribe QuestManager to quest events
            _eventDispatcher->Subscribe(StateMachine::EventType::QUEST_ACCEPTED, _questManager.get());
            _eventDispatcher->Subscribe(StateMachine::EventType::QUEST_COMPLETED, _questManager.get());
            _eventDispatcher->Subscribe(StateMachine::EventType::QUEST_TURNED_IN, _questManager.get());
            _eventDispatcher->Subscribe(StateMachine::EventType::QUEST_ABANDONED, _questManager.get());
            _eventDispatcher->Subscribe(StateMachine::EventType::QUEST_FAILED, _questManager.get());
            _eventDispatcher->Subscribe(StateMachine::EventType::QUEST_STATUS_CHANGED, _questManager.get());
            _eventDispatcher->Subscribe(StateMachine::EventType::QUEST_OBJECTIVE_COMPLETE, _questManager.get());
            _eventDispatcher->Subscribe(StateMachine::EventType::QUEST_OBJECTIVE_PROGRESS, _questManager.get());
            _eventDispatcher->Subscribe(StateMachine::EventType::QUEST_ITEM_COLLECTED, _questManager.get());
            _eventDispatcher->Subscribe(StateMachine::EventType::QUEST_CREATURE_KILLED, _questManager.get());
            _eventDispatcher->Subscribe(StateMachine::EventType::QUEST_EXPLORATION, _questManager.get());
            _eventDispatcher->Subscribe(StateMachine::EventType::QUEST_REWARD_RECEIVED, _questManager.get());
            _eventDispatcher->Subscribe(StateMachine::EventType::QUEST_REWARD_CHOSEN, _questManager.get());
            _eventDispatcher->Subscribe(StateMachine::EventType::QUEST_EXPERIENCE_GAINED, _questManager.get());
            _eventDispatcher->Subscribe(StateMachine::EventType::QUEST_REPUTATION_GAINED, _questManager.get());
            _eventDispatcher->Subscribe(StateMachine::EventType::QUEST_CHAIN_ADVANCED, _questManager.get());
            TC_LOG_INFO("module.playerbot.managers", "🔗 QuestManager subscribed to 16 quest events");
        }

        if (_tradeManager)
        {
            _tradeManager->Initialize();
            TC_LOG_INFO("module.playerbot.managers", "✅ TradeManager initialized via IManagerBase");

            // Subscribe TradeManager to trade events
            _eventDispatcher->Subscribe(StateMachine::EventType::TRADE_INITIATED, _tradeManager.get());
            _eventDispatcher->Subscribe(StateMachine::EventType::TRADE_ACCEPTED, _tradeManager.get());
            _eventDispatcher->Subscribe(StateMachine::EventType::TRADE_CANCELLED, _tradeManager.get());
            _eventDispatcher->Subscribe(StateMachine::EventType::TRADE_ITEM_ADDED, _tradeManager.get());
            _eventDispatcher->Subscribe(StateMachine::EventType::TRADE_GOLD_ADDED, _tradeManager.get());
            _eventDispatcher->Subscribe(StateMachine::EventType::GOLD_RECEIVED, _tradeManager.get());
            _eventDispatcher->Subscribe(StateMachine::EventType::GOLD_SPENT, _tradeManager.get());
            _eventDispatcher->Subscribe(StateMachine::EventType::LOW_GOLD_WARNING, _tradeManager.get());
            _eventDispatcher->Subscribe(StateMachine::EventType::VENDOR_PURCHASE, _tradeManager.get());
            _eventDispatcher->Subscribe(StateMachine::EventType::VENDOR_SALE, _tradeManager.get());
            _eventDispatcher->Subscribe(StateMachine::EventType::REPAIR_COST, _tradeManager.get());
            TC_LOG_INFO("module.playerbot.managers", "🔗 TradeManager subscribed to 11 trade/gold events");
        }

        if (_gatheringManager)
        {
            _gatheringManager->Initialize();
            TC_LOG_INFO("module.playerbot.managers", "✅ GatheringManager initialized via IManagerBase");
        }

        if (_auctionManager)
        {
            _auctionManager->Initialize();
            TC_LOG_INFO("module.playerbot.managers", "✅ AuctionManager initialized via IManagerBase");

            // Subscribe AuctionManager to auction events
            _eventDispatcher->Subscribe(StateMachine::EventType::AUCTION_BID_PLACED, _auctionManager.get());
            _eventDispatcher->Subscribe(StateMachine::EventType::AUCTION_WON, _auctionManager.get());
            _eventDispatcher->Subscribe(StateMachine::EventType::AUCTION_OUTBID, _auctionManager.get());
            _eventDispatcher->Subscribe(StateMachine::EventType::AUCTION_EXPIRED, _auctionManager.get());
            _eventDispatcher->Subscribe(StateMachine::EventType::AUCTION_SOLD, _auctionManager.get());
            TC_LOG_INFO("module.playerbot.managers", "🔗 AuctionManager subscribed to 5 auction events");
        }

        if (_groupCoordinator)
        {
            _groupCoordinator->Initialize();
            TC_LOG_INFO("module.playerbot.managers", "✅ GroupCoordinator initialized - Dungeon/Raid coordination active");
        }
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
    return nullptr;
}

        // CRITICAL: Initialize combat state manager for automatic combat state synchronization
        if (_combatStateManager)
        {
            _combatStateManager->Initialize();
            TC_LOG_INFO("module.playerbot.managers", "✅ CombatStateManager initialized - DAMAGE_TAKEN event subscription active");
        }

        TC_LOG_INFO("module.playerbot.managers",
            "🎯 PHASE 7.1 INTEGRATION COMPLETE: {} - {} managers initialized, {} events subscribed",
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return;
            }
            _bot->GetName(),
            (_questManager ? 1 : 0) + (_tradeManager ? 1 : 0) + (_gatheringManager ? 1 : 0) + (_auctionManager ? 1 : 0) + (_combatStateManager ? 1 : 0),
            16 + 11 + 5 + 1); // Quest + Trade + Auction + Combat event subscriptions
    }

    // Initialize default strategies for basic functionality
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
        return nullptr;
    }
    InitializeDefaultStrategies();

    // Initialize default triggers
    sBotAIFactory->InitializeDefaultTriggers(this);

    // Phase 4: Subscribe to all event buses for comprehensive event handling
    SubscribeToEventBuses();

    // Check if bot is already in a VALID group (e.g., after server restart)
    // CRITICAL FIX: Groups should persist if there's at least one real player character
    // who has been offline for less than 1 hour. This applies to all group sizes:
    // - 2-person groups (1 player + 1 bot)
    // - 5-person dungeon groups (1 player + 4 bots)
    // - 40-person raid groups (1 player + 39 bots)
    // If all players are offline for > 1 hour, disband the group
    if (Group* group = _bot->GetGroup())
        if (!bot)
        {
            if (!member)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: member in method IsInWorld");
                return nullptr;
            }
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                    return nullptr;
                }
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
        return nullptr;
    }
    {
        bool hasValidPlayer = false;
        ObjectGuid playerGuidToCheck = ObjectGuid::Empty;
if (!member)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: member in method GetGUID");
    return nullptr;
}

        // Check all group members for a real player character
        for (GroupReference const& itr : group->GetMembers())
        {
            Player* member = itr.GetSource();
            if (!member)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: member in method IsInWorld");
                return nullptr;
            }
            if (!member)
                continue;

            // Skip if this is a bot
            if (Playerbot::PlayerBotHooks::IsPlayerBot(member))
                continue;

            // Found a real player character
            // Check if they're online OR offline for less than 1 hour
            if (member->IsInWorld())
            {
                // Player is currently online
                hasValidPlayer = true;
                TC_LOG_INFO("playerbot", "Bot {} group validation: Found online player {} in group",
                            if (!bot)
                            {
                                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                                return;
                            }
                            _bot->GetName(), member->GetName());
                break;
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return nullptr;
            }
            }
            else
            {
                // Player is offline - need to check logout time from database
                playerGuidToCheck = member->GetGUID();
                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                    return;
                }
                if (!member)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: member in method GetGUID");
                    return;
                }
                TC_LOG_INFO("playerbot", "Bot {} group validation: Found offline player {} - will check logout time",
                            if (!bot)
                            {
                                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                                if (!bot)
                                {
                                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                                    return nullptr;
                                }
                                return;
                            }
                            _bot->GetName(), member->GetName());
                break;
            }
        }

        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
        // If we found an offline player, check their logout time via database query
        if (!hasValidPlayer && !playerGuidToCheck.IsEmpty())
        {
            // Query the characters database for logout_time
            QueryResult result = CharacterDatabase.PQuery(
                "SELECT logout_time FROM characters WHERE guid = {}", playerGuidToCheck.GetCounter());

            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGUID");
                return nullptr;
            }
            if (result)
            {
                Field* fields = result->Fetch();
                uint64 logoutTime = fields[0].GetUInt64();
                uint64 currentTime = static_cast<uint64>(time(nullptr));
                uint64 timeSinceLogout = currentTime - logoutTime;

                // 1 hour = 3600 seconds
                if (timeSinceLogout < 3600)
                {
                    hasValidPlayer = true;
                    TC_LOG_INFO("playerbot", "Bot {} group validation: Player offline for {}s (< 1 hour), group persists",
                                if (!bot)
                                {
                                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                                    return;
                                }
                                _bot->GetName(), timeSinceLogout);
                }
                else
                {
                    TC_LOG_INFO("playerbot", "Bot {} group validation: Player offline for {}s (> 1 hour), group invalid",
                                if (!bot)
                                {
                                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                                    return;
                                }
                                _bot->GetName(), timeSinceLogout);
                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                    return nullptr;
                }
                }
            }
        }

        if (hasValidPlayer)
        {
            // Valid group with active or recently logged out player
            TC_LOG_INFO("playerbot", "Bot {} in valid group (members: {}), activating follow strategy",
                        if (!bot)
                        {
                            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                            return;
                        }
                        _bot->GetName(), group->GetMembersCount());
            OnGroupJoined(group);
        }
        else
        {
            // No valid player found - all offline > 1 hour or only bots
            TC_LOG_WARN("playerbot", "Bot {} group has no valid player (all offline > 1 hour), disbanding group",
                        if (!bot)
                        {
                            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                            return;
                        }
                        _bot->GetName());
            // Leave the invalid group
            _bot->RemoveFromGroup();
            _aiState = BotAIState::SOLO; // Explicitly set to SOLO
        }
    }

    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGUID");
        return nullptr;
    }
    TC_LOG_DEBUG("playerbots.ai", "BotAI created for bot {}", _bot->GetGUID().ToString());
}

BotAI::~BotAI()
{
    // Phase 4: CRITICAL - Unsubscribe from all event buses to prevent dangling pointers
    UnsubscribeFromEventBuses();

    // ========================================================================
    // CRITICAL FIX: Explicit Manager Destruction Order
    // ========================================================================
    //
    // Problem: C++ destroys members in REVERSE declaration order
    // - _eventDispatcher (line 640) destroyed BEFORE _combatStateManager (line 637)
    // - Managers try to UnsubscribeAll() from already-destroyed EventDispatcher
    // - Results in ACCESS_VIOLATION at EventDispatcher.cpp:132
    //
    // Solution: Manually destroy managers HERE, before automatic destruction
    // - Ensures EventDispatcher is still alive during manager cleanup
    // - Managers can safely call UnsubscribeAll() during OnShutdown()
    // - EventDispatcher will then be destroyed after all managers are gone
    //
    // Destruction Order (CORRECT):
    // 1. Manual reset() of managers (HERE) ← Managers alive, EventDispatcher alive ✅
    // 2. Automatic _eventDispatcher destruction ← All managers gone, safe ✅
    // ========================================================================

    TC_LOG_DEBUG("module.playerbot", "BotAI::~BotAI: Begin explicit manager cleanup for bot '{}'",
                 if (!bot)
                 {
                     TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                     return;
                 }
                 _bot ? _bot->GetName() : "Unknown");

    // Destroy managers in dependency order (most dependent first)
    // Each manager's OnShutdown() will safely call EventDispatcher::UnsubscribeAll()

    // 1. Combat state manager - monitors other managers
    if (_combatStateManager)
    {
        TC_LOG_DEBUG("module.playerbot", "BotAI::~BotAI: Destroying CombatStateManager");
        _combatStateManager.reset();
    }

    // 2. Death recovery manager - may interact with combat
    if (_deathRecoveryManager)
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return;
    }
    {
        TC_LOG_DEBUG("module.playerbot", "BotAI::~BotAI: Destroying DeathRecoveryManager");
        _deathRecoveryManager.reset();
    }

    // 3. Movement arbiter - coordinates movement requests
    if (_movementArbiter)
    {
        TC_LOG_DEBUG("module.playerbot", "BotAI::~BotAI: Destroying MovementArbiter");
        _movementArbiter.reset();
    }

    // 4. Game system managers (order doesn't matter, no interdependencies)
    if (_questManager)
    {
        TC_LOG_DEBUG("module.playerbot", "BotAI::~BotAI: Destroying QuestManager");
        _questManager.reset();
    }

    if (_tradeManager)
    {
        TC_LOG_DEBUG("module.playerbot", "BotAI::~BotAI: Destroying TradeManager");
        _tradeManager.reset();
    }

    if (_gatheringManager)
    {
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
        TC_LOG_DEBUG("module.playerbot", "BotAI::~BotAI: Destroying GatheringManager");
        _gatheringManager.reset();
    }

    if (_auctionManager)
    {
        TC_LOG_DEBUG("module.playerbot", "BotAI::~BotAI: Destroying AuctionManager");
        _auctionManager.reset();
    }

    if (_groupCoordinator)
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return;
    }
    {
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGUID");
            return nullptr;
        }
        TC_LOG_DEBUG("module.playerbot", "BotAI::~BotAI: Destroying GroupCoordinator");
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsInWorld");
            return;
        }
        _groupCoordinator.reset();
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsInCombat");
        return nullptr;
    }
    }
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
    return nullptr;
}

    // Phase 4: Cleanup Shared Blackboard
    if (_sharedBlackboard && _bot)
    {
        TC_LOG_DEBUG("module.playerbot", "BotAI::~BotAI: Removing bot from BlackboardManager");
        BlackboardManager::RemoveBotBlackboard(_bot->GetGUID());
        _sharedBlackboard = nullptr;
    }

    // 5. Support systems
    if (_targetScanner)
    {
        TC_LOG_DEBUG("module.playerbot", "BotAI::~BotAI: Destroying TargetScanner");
        _targetScanner.reset();
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsInWorld");
        return nullptr;
    }
    }

    if (_groupInvitationHandler)
    {
        TC_LOG_DEBUG("module.playerbot", "BotAI::~BotAI: Destroying GroupInvitationHandler");
        _groupInvitationHandler.reset();
    }

    if (_priorityManager)
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGUID");
        return;
    }
    {
        TC_LOG_DEBUG("module.playerbot", "BotAI::~BotAI: Destroying BehaviorPriorityManager");
        _priorityManager.reset();
    }

    TC_LOG_DEBUG("module.playerbot", "BotAI::~BotAI: ✅ All managers destroyed, EventDispatcher can now safely destruct");

    // Now automatic destruction proceeds:
    // - _managerRegistry destroyed (no dependencies)
    // - _eventDispatcher destroyed (all subscribers already unsubscribed during manager cleanup above)
    // - No more ACCESS_VIOLATION because all managers are already gone!

    TC_LOG_INFO("module.playerbot", "BotAI::~BotAI: Destructor complete for bot '{}'",
                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                    return;
                }
                _bot ? _bot->GetName() : "Unknown");
}

// ============================================================================
// MAIN UPDATE METHOD - CLEAN SINGLE ENTRY POINT
// ============================================================================

void BotAI::UpdateAI(uint32 diff)
{
    // CRITICAL: This is the SINGLE entry point for ALL AI updates
    // No more confusion with DoUpdateAI/UpdateEnhanced
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
    return;
}

    // ========================================================================
    // BOT-SPECIFIC LOGIN SPELL CLEANUP: Clear events on first update to prevent LOGINEFFECT crash
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return;
    }
    // ========================================================================
    // Issue: LOGINEFFECT (Spell 836) is cast during SendInitialPacketsAfterAddToMap() at Player.cpp:24742
    //        and queued in EventProcessor. It fires during first Player::Update() → EventProcessor::Update()
    //        which causes Spell.cpp:603 assertion failure: m_spellModTakingSpell != this
    // Root Cause: Bot doesn't send CMSG_CAST_SPELL ACK packets like real players, leaving stale spell references
    // Solution: Clear ALL pending events HERE on first update, BEFORE EventProcessor::Update() can fire them
    // Timing: This runs in UpdateAI() which is called DURING Player::Update(), BEFORE EventProcessor::Update()
    if (!_firstUpdateComplete)
    {
        // Core Fix Applied: SpellEvent::~SpellEvent() now automatically clears m_spellModTakingSpell (Spell.cpp:8455)
        // No longer need to manually clear - KillAllEvents() will properly clean up spell mods
        _bot->m_Events.KillAllEvents(false);  // false = graceful shutdown, not forced
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return;
        }
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return;
        }
        _firstUpdateComplete = true;
        TC_LOG_DEBUG("module.playerbot", "🧹 Bot {} cleared login spell events on FIRST UPDATE to prevent m_spellModTakingSpell crash", _bot->GetName());
    }

    // DIAGNOSTIC: Log UpdateAI entry for first bot only, once per 10 seconds
    static uint32 lastUpdateLog = 0;
    static bool loggedFirstBot = false;
    uint32 now = getMSTime();

    if (!loggedFirstBot || (now - lastUpdateLog > 10000))
    {
        TC_LOG_INFO("module.playerbot", "✅ UpdateAI active: Bot {} (ID: {}), InWorld={}, InCombat={}, InGroup={}, Strategies={}",
                            if (!bot)
                            {
                                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
                                return nullptr;
                            }
                        if (!bot)
                        {
                            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                            return;
                        }
                        if (!bot)
                        {
                            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGUID");
                            return;
                        }
                        if (!bot)
                        {
                            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsInWorld");
                            return nullptr;
                        }
                    if (!bot)
                    {
                        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsInCombat");
                        return;
                    }
                    _bot->GetName(),
                    _bot->GetGUID().GetCounter(),
                    _bot->IsInWorld(),
                    _bot->IsInCombat(),
                    _bot->GetGroup() != nullptr,
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
                    if (!bot)
                    {
                        if (!group)
                        {
                            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: group in method GetLeaderGUID");
                            return nullptr;
                        }
                        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
                        return;
                    }
                    if (!bot)
                    {
                        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsInWorld");
                        return;
                    }
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGUID");
        return nullptr;
    }
                    _activeStrategies.size());
        lastUpdateLog = now;
        loggedFirstBot = true;
    }

    if (!_bot || !_bot->IsInWorld())
        return;

    // ========================================================================
    // STALL DETECTION - Record update timestamp for health monitoring
    // ========================================================================
    // CRITICAL FIX: BotPriorityManager::RecordUpdateStart() was never called!
    // This caused all bots to be flagged as stalled after stallThresholdMs elapsed.
    // We must call this at the start of EVERY UpdateAI() to keep lastUpdateTime current.
    sBotPriorityMgr->RecordUpdateStart(_bot->GetGUID(), now);

    // ========================================================================
    // DEATH RECOVERY - Runs BEFORE all other AI (even when dead/ghost)
    // ========================================================================
    // Death recovery MUST run even when bot is dead (ghost state)
    // This allows bots to release spirit, run to corpse, and resurrect
    if (_deathRecoveryManager)
        _deathRecoveryManager->Update(diff);

    // PRIORITY: If bot is in death recovery, skip expensive AI updates
    // Death recovery handles its own movement (corpse run), so we don't need strategies/combat
    // But we still allow managers to update (see PHASE 5) to prevent system freezing
    bool isInDeathRecovery = _deathRecoveryManager && _deathRecoveryManager->IsInDeathRecovery();

    // Performance tracking - declare BEFORE the if block so it's accessible after
    auto startTime = std::chrono::high_resolution_clock::now();
    _performanceMetrics.totalUpdates++;

    // Only run normal AI if NOT in death recovery
    if (!isInDeathRecovery)
    {

    // ========================================================================
    // SOLO STRATEGY ACTIVATION - Once per bot after first login
    // ========================================================================
    // For bots not in a group, activate solo-relevant strategies on first UpdateAI() call
    // This ensures solo bots have active strategies and can perform autonomous actions
    // Group-related strategies (follow, group_combat) are activated in OnGroupJoined()
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
        return nullptr;
    }
    if (!_bot->GetGroup() && !_soloStrategiesActivated)
    {
        TC_LOG_INFO("module.playerbot.ai", "🎯 ACTIVATING SOLO STRATEGIES: Bot {} (not in group, first UpdateAI)",
                    if (!bot)
                    {
                        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                        return;
                    }
                    _bot->GetName());

        // Activate all solo-relevant strategies in priority order:
        ActivateStrategy("rest");
        ActivateStrategy("solo_combat");
        ActivateStrategy("quest");
        ActivateStrategy("loot");
        ActivateStrategy("solo");

        _soloStrategiesActivated = true;

        TC_LOG_INFO("module.playerbot.ai", "✅ SOLO BOT ACTIVATION COMPLETE: Bot {} - {} strategies active",
                    if (!bot)
                    {
                        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                        return;
                    }
                    _bot->GetName(), _activeStrategies.size());
    }

    // PHASE 0 - Quick Win #3: Periodic group check REMOVED
    // Now using event-driven GROUP_JOINED/GROUP_LEFT events for instant reactions
    // Events dispatched in BotSession.cpp (GROUP_JOINED) and BotAI.cpp (GROUP_LEFT)
    // This eliminates 1-second polling lag

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
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
    return nullptr;
}
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
                return;
            }
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
        return nullptr;
    }
    {
        // Get group leader from group members directly
        Player* leader = nullptr;
        std::vector<Player*> members;

        for (GroupReference const& itr : group->GetMembers())
        {
            Player* member = itr.GetSource();
            if (!member)
                continue;

            // FIX #3: Comprehensive safety checks to prevent crash when player logs out
            // Multiple conditions ensure member is fully valid before caching
            try
            {
                // Check if member is being destroyed or logging out
                if (!member->IsInWorld())
                    continue;

                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                    return;
                }
                WorldSession* session = member->GetSession();
                if (!session)
                    continue;

                // Check if session is valid and not logging out
                if (session->PlayerLogout())
                    continue;

                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                    return nullptr;
                }
                // Member is safe to cache
                members.push_back(member);
                if (member->GetGUID() == group->GetLeaderGUID())
                if (!group)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: group in method GetLeaderGUID");
                    return;
                }
                    leader = member;
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
                return;
            }
            }
            catch (...)
            {
                // Catch any exceptions during member access (e.g., destroyed objects)
                if (!bot)
                {
                    if (!bot)
                    {
                        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                        return nullptr;
                    }
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                    return nullptr;
                }
                TC_LOG_ERROR("playerbot", "Exception while accessing group member for bot {}", _bot->GetName());
                continue;
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGUID");
                return;
            }
            }
        }
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
    return nullptr;
}

        _objectCache.SetGroupLeader(leader);
        _objectCache.SetGroupMembers(members);
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
    return;
}

        // Follow target is usually the leader (only if leader is online)
        if (leader)
            _objectCache.SetFollowTarget(leader);
    }
    else
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return nullptr;
    }
    {
        _objectCache.SetGroupLeader(nullptr);
        _objectCache.SetGroupMembers({});
        _objectCache.SetFollowTarget(nullptr);
    }
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGUID");
    return nullptr;
}

    // ========================================================================
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return nullptr;
    }
    // PHASE 1: CORE BEHAVIORS - Always run every frame
    // ========================================================================

    // Update internal values and caches
    UpdateValues(diff);

    // Phase 2 Week 3: Update Hybrid AI (Utility AI + Behavior Trees, throttled to 500ms)
    UpdateHybridAI(diff);

    // Update all active strategies (including follow, idle, social)
    // CRITICAL: Must run every frame for smooth following
    UpdateStrategies(diff);

    // Process all triggers
    ProcessTriggers();

    // Execute queued and triggered actions
    UpdateActions(diff);

    // Update movement based on strategy decisions
    // CRITICAL: Must run every frame for smooth movement
    UpdateMovement(diff);

    // ========================================================================
    // PHASE 2: STATE MANAGEMENT - Check for state transitions
    // ========================================================================

    // Update combat state (enter/exit combat detection)
    UpdateCombatState(diff);

    // ========================================================================
    // PHASE 3: COMBAT SPECIALIZATION - Only when in combat
    // ========================================================================

    // If in combat AND this is a ClassAI instance, delegate combat updates
    if (IsInCombat())
    {
        // Virtual call to ClassAI::OnCombatUpdate() if overridden
        // ClassAI handles rotation, cooldowns, targeting
        // But NOT movement - that's already handled by strategies
        OnCombatUpdate(diff);
    }
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
    return nullptr;
}

    // ========================================================================
    // PHASE 4: GROUP INVITATION PROCESSING - Critical for joining groups
    // ========================================================================

    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return nullptr;
    }
    // Process pending group invitations
    // CRITICAL: Must run every frame to accept invitations promptly
    if (_groupInvitationHandler)
    {
        _groupInvitationHandler->Update(diff);
    }

    }  // End of if (!isInDeathRecovery) block - normal AI skipped when dead

    // ========================================================================
    // CRITICAL: Movement Arbiter MUST update even during death recovery
    // ========================================================================
    // Death recovery uses MovementArbiter for corpse navigation with HIGHEST priority (255)
    // If we skip this, bots can't move to their corpse!
    if (_movementArbiter)
        _movementArbiter->Update(diff);

    // ========================================================================
    // PHASE 5: MANAGER UPDATES - Throttled heavyweight operations
    // ========================================================================

    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return nullptr;
    }
    // Managers run even during death recovery to prevent system freezing
    // Update all BehaviorManager-based managers
    // These handle quest, trade, gathering with their own throttling
    // NOTE: Managers continue to update even during death recovery to prevent system freezing
    UpdateManagers(diff);

    // ========================================================================
    // PHASE 7.3: EVENT SYSTEM - Events processed via EventDispatcher
    // ========================================================================
    // Legacy BotEventSystem removed - events now flow through per-bot EventDispatcher

    // ========================================================================
    // PHASE 7: SOLO BEHAVIORS - Only when bot is in solo play mode
    // ========================================================================

    // Update solo behaviors (questing, gathering, autonomous combat, etc.)
    // Only runs when bot is in solo play mode (not in group or following)

    // DIAGNOSTIC: Log why UpdateSoloBehaviors might not be running
    static uint32 lastSoloBehaviorLog = 0;
    uint32 soloCheckTime = getMSTime();
    if (soloCheckTime - lastSoloBehaviorLog > 5000) // Every 5 seconds
    {
        TC_LOG_ERROR("module.playerbot", "🔍 UpdateSoloBehaviors check: Bot {} - IsInCombat()={}, IsFollowing()={}, _aiState={}, InGroup={}",
                     if (!bot)
                     {
                         TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                         return nullptr;
                     }
                     if (!bot)
                     {
                         TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                         return;
                     }
                     _bot->GetName(),
                     IsInCombat(),
                     IsFollowing(),
                     static_cast<uint32>(_aiState),
                     _bot->GetGroup() != nullptr);
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return nullptr;
    }
                     if (!bot)
                     {
                         TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
                         return nullptr;
                     }
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
        lastSoloBehaviorLog = soloCheckTime;
    }

    if (!IsInCombat() && !IsFollowing())
    {
        TC_LOG_ERROR("module.playerbot", "🚀 CALLING UpdateSoloBehaviors for bot {}", _bot->GetName());
        UpdateSoloBehaviors(diff);
    }
    else
    {
        if (soloCheckTime - lastSoloBehaviorLog < 100) // Only log once per 5-second window
        {
            TC_LOG_ERROR("module.playerbot", "⚠️ SKIPPING UpdateSoloBehaviors for bot {} - IsInCombat={}, IsFollowing={}",
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
    return nullptr;
}
                         if (!bot)
                         {
                             TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                             return nullptr;
                         }
                         _bot->GetName(), IsInCombat(), IsFollowing());
        }
    }

    // ========================================================================
    // PHASE 8: GROUP MANAGEMENT - Check for group changes
    // ========================================================================

    // Check if bot left group and trigger cleanup
    bool isInGroup = (_bot->GetGroup() != nullptr);
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return;
    }
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return;
    }
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return;
        }
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
        return;
    }
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return;
        }
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGUID");
    return;
}
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return;
        }
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
            return nullptr;
        }

    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return nullptr;
    }
    // FIX #1: Handle bot joining group on server reboot (was already in group before restart)
    if (!_wasInGroup && isInGroup)
    {
        TC_LOG_INFO("playerbot", "Bot {} detected in group (server reboot or first login), calling OnGroupJoined()",
                    _bot->GetName());

        // PHASE 0 - Quick Win #3: Dispatch GROUP_JOINED event
        if (_eventDispatcher)
        {
            Events::BotEvent evt(StateMachine::EventType::GROUP_JOINED, _bot->GetGUID());
            _eventDispatcher->Dispatch(std::move(evt));
            TC_LOG_INFO("playerbot", "📢 GROUP_JOINED event dispatched for bot {} (reboot detection)", _bot->GetName());
        }

        OnGroupJoined(_bot->GetGroup());
    }
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsAlive");
        return nullptr;
    }
    // FIX #2: Handle bot leaving group
    else if (_wasInGroup && !isInGroup)
    {
        TC_LOG_INFO("playerbot", "Bot {} left group, calling OnGroupLeft()",
                    if (!bot)
                    {
                        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                        return;
                    }
                    _bot->GetName());

        // PHASE 0 - Quick Win #3: Dispatch GROUP_LEFT event for instant cleanup
        if (_eventDispatcher)
        {
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGUID");
                return nullptr;
            }
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsInCombat");
                return nullptr;
            }
            Events::BotEvent evt(StateMachine::EventType::GROUP_LEFT, _bot->GetGUID());
            _eventDispatcher->Dispatch(std::move(evt));
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return;
            }
            TC_LOG_INFO("playerbot", "📢 GROUP_LEFT event dispatched for bot {}", _bot->GetName());
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
        }

        OnGroupLeft();
    }
    _wasInGroup = isInGroup;

    // ========================================================================
    // PHASE 9: PERFORMANCE TRACKING
    // ========================================================================

    auto endTime = std::chrono::high_resolution_clock::now();
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return;
    }
    auto updateTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    // Update performance metrics
    if (_performanceMetrics.averageUpdateTime.count() == 0)
        _performanceMetrics.averageUpdateTime = updateTime;
    else
        _performanceMetrics.averageUpdateTime = (_performanceMetrics.averageUpdateTime + updateTime) / 2;

    if (!target)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: target in method GetName");
        return nullptr;
    }
    if (updateTime > _performanceMetrics.maxUpdateTime)
        _performanceMetrics.maxUpdateTime = updateTime;

    _performanceMetrics.lastUpdate = std::chrono::steady_clock::now();
}

// ============================================================================
// STRATEGY UPDATES - Core behavior system
// ============================================================================

void BotAI::UpdateStrategies(uint32 diff)
if (!target)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: target in method GetName");
    return;
}
{
    // CRITICAL: This must run EVERY frame for following to work properly
    // No throttling allowed here!

    // DEBUG LOGGING THROTTLE: Only log for test bots every 50 seconds
    static const std::set<std::string> testBots = {"Anderenz", "Boone", "Nelona", "Sevtap"};
    static std::unordered_map<std::string, uint32> strategyLogAccumulators;

    bool isTestBot = _bot && (testBots.find(_bot->GetName()) != testBots.end());
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return nullptr;
    }
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
    return;
}
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return;
    }
    bool shouldLogStrategy = false;

    if (isTestBot)
    {
        std::string const& botName = _bot->GetName();
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
        strategyLogAccumulators[botName] += diff;
        if (strategyLogAccumulators[botName] >= 50000)
        {
            shouldLogStrategy = true;
            strategyLogAccumulators[botName] = 0;
                        if (!bot)
                        {
                            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                            return;
                        }
        }
    }

    // ========================================================================
    // PHASE 1: Collect all active strategies WITHOUT holding lock
    // ========================================================================

    std::vector<Strategy*> strategiesToCheck;
    {
        std::lock_guard<std::recursive_mutex> lock(_mutex);

        // DEBUG LOGGING THROTTLE: Use shouldLogStrategy from above (already throttled)
        if (shouldLogStrategy)
        {
            TC_LOG_ERROR("module.playerbot", "🔍 ACTIVE STRATEGIES: Bot {} has {} active strategies in _activeStrategies set",
                        _bot->GetName(), _activeStrategies.size());
        }

        for (auto const& strategyName : _activeStrategies)
        {
            auto it = _strategies.find(strategyName);
            if (it != _strategies.end())
            {
                strategiesToCheck.push_back(it->second.get());
            }
        }
    } // RELEASE LOCK IMMEDIATELY

    // ========================================================================
    // PHASE 2: Filter active strategies and check IsActive()
    // ========================================================================

    std::vector<Strategy*> activeStrategies;

    // OLD DEBUG CODE REMOVED - now using test bot whitelist throttling above (lines 433-454)
    // static uint32 debugStrategyCounter = 0;
    // bool shouldLogStrategy = (++debugStrategyCounter % 10 == 0);

    for (Strategy* strategy : strategiesToCheck)
    {
        if (strategy && strategy->IsActive(this))
        {
            activeStrategies.push_back(strategy);
            if (shouldLogStrategy)
            {
                TC_LOG_ERROR("module.playerbot.ai", "🎯 STRATEGY ACTIVE: Bot {} strategy '{}'",
                            if (!bot)
                            {
                                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                                return;
                            }
                            _bot->GetName(), strategy->GetName());
            }
        }
    }

    // ========================================================================
    // PHASE 3: Use BehaviorPriorityManager to select highest priority strategy
    // ========================================================================

    Strategy* selectedStrategy = nullptr;
    if (_priorityManager && !activeStrategies.empty())
    {
        // Update context (combat state, fleeing, etc.)
        _priorityManager->UpdateContext();

        // Select highest priority valid strategy
        selectedStrategy = _priorityManager->SelectActiveBehavior(activeStrategies);

        if (shouldLogStrategy && selectedStrategy)
        {
            TC_LOG_ERROR("module.playerbot", "🏆 PRIORITY WINNER: Bot {} selected strategy '{}' from {} candidates",
                        if (!bot)
                        {
                            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                            return;
                        }
                        _bot->GetName(), selectedStrategy->GetName(), activeStrategies.size());
        }
    }

    // ========================================================================
    // PHASE 4: Execute the selected strategy
    // ========================================================================

    if (selectedStrategy)
    {
        if (shouldLogStrategy)
        {
            TC_LOG_ERROR("module.playerbot", "⚡ EXECUTING: Bot {} strategy '{}'",
                        if (!bot)
                        {
                            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
                            return;
                        }
                        if (!bot)
                        {
                            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                            return;
                        }
                        _bot->GetName(), selectedStrategy->GetName());
        }

        // Special handling for follow strategy - needs every frame update
        if (auto* followBehavior = dynamic_cast<LeaderFollowBehavior*>(selectedStrategy))
        {
            if (shouldLogStrategy)
                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                    return nullptr;
                }
                TC_LOG_ERROR("module.playerbot", "🚀 CALLING UpdateFollowBehavior for bot {}", _bot->GetName());
            followBehavior->UpdateFollowBehavior(this, diff);
        }
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetMapId");
            return nullptr;
        }
        else
        {
            // Other strategies can use their normal update
            if (shouldLogStrategy)
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetPosition");
                return nullptr;
            }
            {
                TC_LOG_ERROR("module.playerbot", "🚀 CALLING UpdateBehavior for bot {} strategy '{}'",
                            if (!bot)
                            {
                                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                                return;
                            }
                            _bot->GetName(), selectedStrategy->GetName());
            }
            selectedStrategy->UpdateBehavior(this, diff);
            if (shouldLogStrategy)
            {
                TC_LOG_ERROR("module.playerbot", "✔️ RETURNED from UpdateBehavior for bot {} strategy '{}'",
                            if (!bot)
                            {
                                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                                return;
                            }
                            _bot->GetName(), selectedStrategy->GetName());
            }
        }

        _performanceMetrics.strategiesEvaluated = 1;
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return nullptr;
    }
    }
    else
    {
        if (shouldLogStrategy)
        {
            TC_LOG_ERROR("module.playerbot", "⚠️ NO STRATEGY SELECTED for bot {} (had {} active)",
                        if (!bot)
                        {
                            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                            return;
                        }
                        _bot->GetName(), activeStrategies.size());
        }
        _performanceMetrics.strategiesEvaluated = 0;
    }
}

// ============================================================================
// MOVEMENT UPDATES - Strategy-controlled movement
// ============================================================================

void BotAI::UpdateMovement(uint32 diff)
{
    // CRITICAL: Movement is controlled by strategies (especially follow)
    // This method just ensures movement commands are processed
    // Must run every frame for smooth movement

    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsAlive");
        return;
    }
    if (!_bot || !_bot->IsAlive())
        if (!target)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: target in method GetGUID");
            return;
        }
        return;

    // Movement is primarily handled by strategies (follow, combat positioning, etc.)
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return nullptr;
    }
    // This is just for ensuring movement updates are processed
    if (_bot->GetMotionMaster())
    {
        // Motion master will handle actual movement updates
        // We just ensure it's being processed
    }
}

// ============================================================================
// COMBAT STATE MANAGEMENT
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
    return;
}
// ============================================================================

void BotAI::UpdateCombatState(uint32 diff)
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
    return nullptr;
}
{
    bool wasInCombat = IsInCombat();
    bool isInCombat = _bot && _bot->IsInCombat();
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return;
    }
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsInCombat");
        return;
    }

    // DIAGNOSTIC: Log combat state every 2 seconds
    static uint32 lastCombatStateLog = 0;
    uint32 now = getMSTime();
    if (now - lastCombatStateLog > 2000)
    {
        TC_LOG_ERROR("module.playerbot", "🔍 UpdateCombatState: Bot {} - wasInCombat={}, isInCombat={}, AIState={}, HasVictim={}",
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return;
        }
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
                     _bot ? _bot->GetName() : "null",
                     wasInCombat, isInCombat,
                     static_cast<uint32>(_aiState),
                     (_bot && _bot->GetVictim()) ? "YES" : "NO");
        lastCombatStateLog = now;
    }

    // Handle combat state transitions
    if (!wasInCombat && isInCombat)
    {
        // Entering combat
        TC_LOG_ERROR("module.playerbot", "⚔️ ENTERING COMBAT: Bot {}", _bot->GetName());
        SetAIState(BotAIState::COMBAT);

        // Find initial target
        // FIX #19: Use ObjectCache instead of ObjectAccessor to avoid TrinityCore deadlock
        ::Unit* target = _objectCache.GetTarget();
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return;
            }
            if (!target)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: target in method GetName");
                return nullptr;
            }
        if (target)
        {
            TC_LOG_ERROR("module.playerbot", "🎯 Target from cache: {}", target->GetName());
        }

        if (!target)
        {
            target = _bot->GetVictim();
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return nullptr;
            }
            if (!target)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: target in method GetName");
                return;
            }
            if (!target)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: target in method GetName");
                return nullptr;
            }
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
            TC_LOG_ERROR("module.playerbot", "🎯 Target from GetVictim(): {}", target ? target->GetName() : "null");
        }

        if (target)
        {
            TC_LOG_ERROR("module.playerbot", "✅ Calling OnCombatStart() with target {}", target->GetName());
            OnCombatStart(target);
        }
        else
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
            return;
        }
        {
            TC_LOG_ERROR("module.playerbot", "❌ COMBAT START FAILED: No valid target found!");
        }
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return nullptr;
    }
    }
    else if (wasInCombat && !isInCombat)
    {
        // Leaving combat
        TC_LOG_ERROR("module.playerbot", "🏳️ LEAVING COMBAT: Bot {}", _bot->GetName());
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return;
        }
        OnCombatEnd();

        // Determine new state
        if (!bot)
        {
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return nullptr;
            }
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
            return nullptr;
        }
        if (_bot->GetGroup() && GetStrategy("follow"))
            SetAIState(BotAIState::FOLLOWING);
        else
            SetAIState(BotAIState::SOLO);
    }
}

// ============================================================================
// TRIGGER PROCESSING
// ============================================================================

void BotAI::ProcessTriggers()
{
    if (!_bot)
        return;

    // Clear previous triggered actions
    while (!_triggeredActions.empty())
        _triggeredActions.pop();

    // Evaluate all triggers
    for (auto const& trigger : _triggers)
    {
        if (trigger && trigger->Check(this))
        {
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return nullptr;
            }
            auto result = trigger->Evaluate(this);
            if (result.triggered && result.suggestedAction)
            {
                _triggeredActions.push(result);
                _performanceMetrics.triggersProcessed++;
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return;
            }
            }
        }
    }
}

// ============================================================================
// ACTION EXECUTION
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
    return;
}
// ============================================================================

void BotAI::UpdateActions(uint32 diff)
{
    // Execute current action if in progress
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return;
    }
    if (_currentAction)
    {
        // Check if action is still valid
        if (!_currentAction->IsUseful(this))
        {
            CancelCurrentAction();
        }
        else
        {
            // Action still in progress
            return;
        }
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return;
    }
    }

    // Process triggered actions first (higher priority)
    if (!_triggeredActions.empty())
    {
        auto const& result = _triggeredActions.top();
        if (result.suggestedAction && CanExecuteAction(result.suggestedAction.get()))
        {
            auto execResult = ExecuteActionInternal(result.suggestedAction.get(), result.context);
            if (execResult == ActionResult::SUCCESS || execResult == ActionResult::IN_PROGRESS)
            {
                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                    return;
                }
                _currentAction = result.suggestedAction;
                _performanceMetrics.actionsExecuted++;
            }
        }
        _triggeredActions.pop();
        return;
    }

    // Process queued actions
    if (!_actionQueue.empty())
    {
        auto [action, context] = _actionQueue.front();
        _actionQueue.pop();

        if (action && CanExecuteAction(action.get()))
        {
            auto result = ExecuteActionInternal(action.get(), context);
            if (result == ActionResult::SUCCESS || result == ActionResult::IN_PROGRESS)
            {
                _currentAction = action;
                _currentContext = context;
                _performanceMetrics.actionsExecuted++;
            }
        }
    }
}

// ============================================================================
// SOLO BEHAVIORS - Autonomous play when not in group
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
    return;
}
// ============================================================================

void BotAI::UpdateSoloBehaviors(uint32 diff)
{
    // Only run solo behaviors when in solo play mode (not grouped/following)
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return;
    }
    if (IsInCombat() || IsFollowing())
        return;

    uint32 currentTime = getMSTime();

    // ========================================================================
    // AUTONOMOUS TARGET SCANNING - Find enemies when solo
    // ========================================================================

    // For solo bots (not in a group), actively scan for nearby enemies
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
        return nullptr;
    }
    if (!_bot->GetGroup() && _targetScanner)
    {
        // Check if it's time to scan (throttled for performance)
        if (_targetScanner->ShouldScan(currentTime))
        {
            _targetScanner->UpdateScanTime(currentTime);

            // Clean up blacklist
            _targetScanner->UpdateBlacklist(currentTime);
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
    return;
}

            // ENTERPRISE-GRADE THREAD-SAFE TARGET RESOLUTION
            // Find best target to engage (returns GUID, thread-safe)
            ObjectGuid bestTargetGuid = _targetScanner->FindBestTarget();

            // DEADLOCK FIX: Validate and set target using spatial grid snapshots (lock-free, thread-safe)
            // DO NOT use ObjectAccessor::GetUnit() from worker thread!

            if (!bestTargetGuid.IsEmpty())
            {
                // Get spatial grid for lock-free snapshot queries
                auto spatialGrid = sSpatialGridManager.GetGrid(_bot->GetMapId());
                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetMapId");
                    return nullptr;
                }
                if (spatialGrid)
                {
                    // Query nearby creature snapshots (lock-free read from atomic buffer)
                    auto creatureSnapshots = spatialGrid->QueryNearbyCreatures(_bot->GetPosition(), 60.0f);
                    if (!bot)
                    {
                        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetPosition");
                        return nullptr;
                    }

                    // Find the snapshot matching our target GUID
                    for (auto const& snapshot : creatureSnapshots)
                    {
                        if (snapshot.guid == bestTargetGuid)
                        {
                            // Snapshot found - validate using snapshot data (no Map access needed)
                            // 1. Check if creature is alive (!isDead flag)
                            // 2. Check if creature is attackable (isHostile flag)
                            // 3. Check distance is within engage range (60.0f)

                            float distance = std::sqrt(_bot->GetExactDistSq(snapshot.position)); // Calculate once from squared distance
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
    return nullptr;
}
                                    if (!bot)
                                    {
                                        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                                        return;
                                    }
                            if (!snapshot.isDead &&
                                snapshot.isHostile &&
                                distance <= 60.0f)
                            {
                                // Target is valid based on snapshot data
                                // SetTarget() is thread-safe (just sets a GUID)
                                _bot->SetTarget(bestTargetGuid);

                                TC_LOG_DEBUG("playerbot.solo",
                                    "Solo bot {} selected target Entry {} (level {}) via spatial grid snapshot - combat will engage naturally",
                                    _bot->GetName(), snapshot.entry, snapshot.level);

                                // Combat will initiate naturally:
                                // - Bot has target set → ClassAI will cast spells/attack
                                // - CombatMovementStrategy will handle positioning
                                // - Threat will be established when damage lands
                                // NO NEED for explicit Attack() or SetInCombatWith() calls from worker thread
                            }
                            break;
                        if (!bot)
                        {
                            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
                            return nullptr;
                        }
                        }
                    }
                }
            }
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
            return nullptr;
        }
        }
    }

    // ========================================================================
    // GAME SYSTEM MANAGER UPDATES
    // ========================================================================

    // Managers will be updated via UpdateManagers() which is called in UpdateAI()
    // No need to manually update them here since they inherit from BehaviorManager
    // and have their own throttling mechanisms

    // TODO: Add social interactions (chat, emotes) when implemented
}

// ============================================================================
// STATE TRANSITIONS
// ============================================================================

void BotAI::OnCombatStart(::Unit* target)
{
    _currentTarget = target ? target->GetGUID() : ObjectGuid::Empty;
    if (!target)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: target in method GetGUID");
        return;
    }

    TC_LOG_DEBUG("playerbot", "Bot {} entering combat with {}",
                 if (!bot)
                 {
                     TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                     return;
                 }
                 _bot->GetName(), target ? target->GetName() : "unknown");

    // Strategies don't have OnCombatStart - combat is handled by ClassAI
    // through the OnCombatUpdate() method
}

void BotAI::OnCombatEnd()
{
    _currentTarget = ObjectGuid::Empty;

    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return nullptr;
    }
    TC_LOG_DEBUG("playerbot", "Bot {} leaving combat", _bot->GetName());

    // FIX #4: Resume following after combat ends (if in group)
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
        return nullptr;
    }
    if (_bot->GetGroup())
    {
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
        TC_LOG_INFO("playerbot", "Bot {} combat ended, resuming follow behavior", _bot->GetName());
        SetAIState(BotAIState::FOLLOWING);

        // Clear ONLY non-follow movement types to allow follow strategy to take over
        // Don't clear if already following, as that would cause stuttering
        MotionMaster* mm = _bot->GetMotionMaster();
                            if (!bot)
                            {
                                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                                return;
                            }
        if (mm)
        {
            MovementGeneratorType currentType = mm->GetCurrentMovementGeneratorType(MOTION_SLOT_ACTIVE);
            if (currentType != FOLLOW_MOTION_TYPE && currentType != IDLE_MOTION_TYPE)
            {
                TC_LOG_ERROR("playerbot", "🧹 OnCombatEnd: Clearing {} motion type for bot {} to allow follow",
                            static_cast<uint32>(currentType), _bot->GetName());
                mm->Clear();
            }
        }
    }
    else
    {
        // Not in group, return to solo play mode
        SetAIState(BotAIState::SOLO);
    }

    // Strategies don't have OnCombatEnd - combat is handled by ClassAI
    // through the OnCombatUpdate() method
}

void BotAI::OnDeath()
{
    SetAIState(BotAIState::DEAD);
    CancelCurrentAction();

    // Clear action queue
    while (!_actionQueue.empty())
        _actionQueue.pop();

    // Initiate death recovery process
    if (_deathRecoveryManager)
        _deathRecoveryManager->OnDeath();

    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return nullptr;
    }
    TC_LOG_DEBUG("playerbots.ai", "Bot {} died, AI state reset, death recovery initiated", _bot->GetName());
}

void BotAI::OnRespawn()
{
    SetAIState(BotAIState::SOLO);
    Reset();

    // Complete death recovery process
    if (_deathRecoveryManager)
        _deathRecoveryManager->OnResurrection();

    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return nullptr;
    }
    TC_LOG_DEBUG("playerbots.ai", "Bot {} respawned, AI reset, death recovery completed", _bot->GetName());
}

void BotAI::Reset()
{
    _currentTarget = ObjectGuid::Empty;
    _aiState = BotAIState::SOLO;

    CancelCurrentAction();

    while (!_actionQueue.empty())
        _actionQueue.pop();

    while (!_triggeredActions.empty())
        _triggeredActions.pop();
}

// ============================================================================
// GROUP MANAGEMENT
// ============================================================================

void BotAI::OnGroupJoined(Group* group)
{
    // Get group from bot if not provided (handles login scenario)
    if (!group && _bot)
        group = _bot->GetGroup();
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
            return nullptr;
        }

    TC_LOG_INFO("module.playerbot.ai", "🚨 OnGroupJoined called for bot {}, provided group={}, bot's group={}",
                _bot ? _bot->GetName() : "NULL", (void*)group, _bot ? (void*)_bot->GetGroup() : nullptr);
                            if (!bot)
                            {
                                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                                return nullptr;
                            }
                        if (!bot)
                        {
                            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                            return nullptr;
                        }
                    if (!bot)
                    {
                        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                        return;
                    }
                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                    return;
                }
                    if (!bot)
                    {
                        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                        return;
                    }

    if (!group)
    {
        TC_LOG_INFO("module.playerbot.ai", "❌ OnGroupJoined: No group available for bot {}",
                    _bot ? _bot->GetName() : "NULL");
        return;
    }

    TC_LOG_INFO("module.playerbot.ai", "Bot {} joined group {}, activating follow and combat strategies",
                _bot->GetName(), (void*)group);
                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                    return;
                }

    // DEADLOCK FIX #12: This method was acquiring mutex MULTIPLE times:
    // 1. GetStrategy("follow") - shared_lock
    // 2. AddStrategy() - unique_lock
    // 3. GetStrategy("group_combat") - shared_lock
    // 4. AddStrategy() - unique_lock
    // 5. ActivateStrategy("follow") - unique_lock
    // 6. ActivateStrategy("group_combat") - unique_lock
    // 7. Final confirmation block - shared_lock
    //
    // When UpdateStrategies() runs in another thread and releases its lock,
    // then ActivateStrategy() tries to acquire unique_lock, and THIS method
    // tries to acquire another lock, DEADLOCK occurs due to writer-preference!
    //
    // Solution: Do ALL mutex operations in a SINGLE critical section,
    // then call OnActivate() callbacks AFTER releasing the lock

    std::vector<Strategy*> strategiesToActivate;

    // PHASE 1: Check strategy existence and activate - ALL UNDER ONE LOCK
    {
        std::lock_guard<std::recursive_mutex> lock(_mutex);

        // Check if follow strategy exists
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
        if (_strategies.find("follow") == _strategies.end())
        {
            TC_LOG_ERROR("playerbot", "CRITICAL: Follow strategy not found for bot {} - creating emergency fallback",
                        if (!bot)
                        {
                            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                            return;
                        }
                        _bot->GetName());

            // Create it immediately while we hold the lock
            auto followBehavior = std::make_unique<LeaderFollowBehavior>();
            _strategies["follow"] = std::move(followBehavior);
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return;
            }
            TC_LOG_WARN("playerbot", "Created emergency follow strategy for bot {}", _bot->GetName());
        }

        // Check if group combat strategy exists
        if (_strategies.find("group_combat") == _strategies.end())
        {
            TC_LOG_ERROR("playerbot", "CRITICAL: GroupCombat strategy not found for bot {} - creating emergency fallback",
                        if (!bot)
                        {
                            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                            return;
                        }
                        _bot->GetName());

            // Create it immediately while we hold the lock
            auto groupCombat = std::make_unique<GroupCombatStrategy>();
            _strategies["group_combat"] = std::move(groupCombat);
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return;
            }
            TC_LOG_WARN("playerbot", "Created emergency group_combat strategy for bot {}", _bot->GetName());
        }

        // Activate follow strategy (while still holding lock)
        {
            bool alreadyInList = std::find(_activeStrategies.begin(), _activeStrategies.end(), "follow") != _activeStrategies.end();
            auto it = _strategies.find("follow");
            if (it != _strategies.end())
            {
                bool wasActive = it->second->IsActive(this);

                TC_LOG_ERROR("playerbot", "🔍 OnGroupJoined: Bot {} follow strategy - alreadyInList={}, wasActive={}",
                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                    return;
                }
                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                    return nullptr;
                }
                            _bot->GetName(), alreadyInList, wasActive);

                if (!alreadyInList)
                    _activeStrategies.push_back("follow");

                it->second->SetActive(true);

                // CRITICAL FIX: ALWAYS call OnActivate to ensure follow target is set
                // This handles server restart where bot loads with group but follow not initialized
                strategiesToActivate.push_back(it->second.get());

                TC_LOG_ERROR("playerbot", "✅ OnGroupJoined: Bot {} queued follow strategy for OnActivate callback", _bot->GetName());
            }
        }

        // Activate group combat strategy (while still holding lock)
        {
            bool alreadyInList = std::find(_activeStrategies.begin(), _activeStrategies.end(), "group_combat") != _activeStrategies.end();
            auto it = _strategies.find("group_combat");
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsAlive");
                return;
            }
            if (it != _strategies.end())
            {
                bool wasActive = it->second->IsActive(this);

                if (!alreadyInList)
                    _activeStrategies.push_back("group_combat");

                it->second->SetActive(true);
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsAlive");
    return;
}

                // CRITICAL FIX: Call OnActivate if newly added OR not properly initialized
                if (!alreadyInList || !wasActive)
                    strategiesToActivate.push_back(it->second.get());
            }
        }

        // Confirm activation (still under same lock)
        bool followActive = std::find(_activeStrategies.begin(), _activeStrategies.end(), "follow") != _activeStrategies.end();
        bool combatActive = std::find(_activeStrategies.begin(), _activeStrategies.end(), "group_combat") != _activeStrategies.end();

        if (followActive && combatActive)
        {
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return nullptr;
            }
            TC_LOG_INFO("playerbot", "✅ Successfully activated follow and group_combat strategies for bot {}", _bot->GetName());
        }
        else
        {
            TC_LOG_ERROR("playerbot", "❌ Strategy activation FAILED for bot {} - follow={}, combat={}",
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
                        if (!bot)
                        {
                            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                            return;
                        }
                        _bot->GetName(), followActive, combatActive);
        }
    } // RELEASE LOCK - all operations completed

    // PHASE 2: Call OnActivate() callbacks WITHOUT holding lock
    for (Strategy* strategy : strategiesToActivate)
    {
        if (strategy)
            strategy->OnActivate(this);
    }

    // Deactivate solo strategy when joining a group
    DeactivateStrategy("solo");

    // Set state to following if not in combat (no lock needed - atomic operation)
    if (!IsInCombat())
        SetAIState(BotAIState::FOLLOWING);

    _wasInGroup = true;
}

void BotAI::OnGroupLeft()
{
    TC_LOG_INFO("playerbot", "Bot {} left group, deactivating follow and combat strategies",
                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                    return;
                }
                _bot->GetName());

    // DEADLOCK FIX #12: Same as OnGroupJoined - do all operations under one lock
    std::vector<Strategy*> strategiesToDeactivate;

    {
        std::lock_guard<std::recursive_mutex> lock(_mutex);

        // Deactivate follow strategy
        auto followIt = _strategies.find("follow");
        if (followIt != _strategies.end())
        {
            followIt->second->SetActive(false);
            strategiesToDeactivate.push_back(followIt->second.get());
        }

        _activeStrategies.erase(
            std::remove(_activeStrategies.begin(), _activeStrategies.end(), "follow"),
            _activeStrategies.end()
        );

        // Deactivate group combat strategy
        auto combatIt = _strategies.find("group_combat");
        if (combatIt != _strategies.end())
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return;
        }
        {
            combatIt->second->SetActive(false);
            strategiesToDeactivate.push_back(combatIt->second.get());
        }

        _activeStrategies.erase(
            std::remove(_activeStrategies.begin(), _activeStrategies.end(), "group_combat"),
            _activeStrategies.end()
        );
    } // RELEASE LOCK

    // Call OnDeactivate() callbacks WITHOUT holding lock
    for (Strategy* strategy : strategiesToDeactivate)
    {
        if (strategy)
            strategy->OnDeactivate(this);
    }

    // Activate all solo strategies when leaving a group
    // These are the same strategies activated in UpdateAI() for solo bots
    ActivateStrategy("rest");    // Priority: 90 - eating/drinking
    ActivateStrategy("quest");   // Priority: 70 - quest objectives
    ActivateStrategy("loot");    // Priority: 60 - corpse looting
    ActivateStrategy("solo");    // Priority: 10 - fallback coordinator

    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return;
    }
    TC_LOG_INFO("module.playerbot.ai", "🎯 SOLO BOT REACTIVATION: Bot {} reactivated solo strategies after leaving group",
                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                    return;
                }
                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                    return;
                }
                _bot->GetName());

    // Set state to solo if not in combat
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsInWorld");
        return nullptr;
    }
    if (!IsInCombat())
        SetAIState(BotAIState::SOLO);

    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return nullptr;
    }
    _wasInGroup = false;
}

void BotAI::HandleGroupChange()
{
    // Check current group status
    bool inGroup = (_bot && _bot->GetGroup() != nullptr);
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return nullptr;
    }
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
        return;
    }
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetGroup");
            return nullptr;
        }

    if (inGroup && !_wasInGroup)
    {
        OnGroupJoined(_bot->GetGroup());
    }
    else if (!inGroup && _wasInGroup)
    {
        OnGroupLeft();
    }
}
if (!bot)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
    return;
}

// ============================================================================
// STRATEGY MANAGEMENT
// ============================================================================

void BotAI::AddStrategy(std::unique_ptr<Strategy> strategy)
{
    if (!strategy)
        return;

    std::lock_guard<std::recursive_mutex> lock(_mutex);
    std::string const& name = strategy->GetName();
    Strategy* strategyPtr = strategy.get();
    _strategies[name] = std::move(strategy);

    // Auto-register with priority manager based on strategy name
    if (_priorityManager)
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return nullptr;
    }
    {
        BehaviorPriority priority = BehaviorPriority::SOLO; // Default
        bool exclusive = false;

        // Determine priority from strategy name
        if (name.find("combat") != std::string::npos || name.find("group_combat") != std::string::npos)
        {
            priority = BehaviorPriority::COMBAT;
            exclusive = true; // Combat gets exclusive control
        }
        else if (name == "follow")
        {
            priority = BehaviorPriority::FOLLOW;
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
        }
        else if (name.find("flee") != std::string::npos)
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return;
        }
        {
            priority = BehaviorPriority::FLEEING;
            exclusive = true;
        }
        else if (name.find("cast") != std::string::npos)
        {
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return;
            }
            priority = BehaviorPriority::CASTING;
        }
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
        else if (name == "quest")
        {
            // Quest strategy gets FOLLOW priority (50) to ensure it runs for solo bots
            // This allows quests to take priority over gathering/trading/social
            priority = BehaviorPriority::FOLLOW;
        }
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
        else if (name == "loot")
        {
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return nullptr;
            }
            // Loot strategy gets MOVEMENT priority (45) - slightly lower than quest
            priority = BehaviorPriority::MOVEMENT;
        }
        else if (name == "rest")
        {
            // Rest strategy gets FLEEING priority (90) - HIGHEST for survival
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return nullptr;
            }
            // Bots must rest when health/mana low before doing anything else
            priority = BehaviorPriority::FLEEING;
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
        }
        else if (name.find("gather") != std::string::npos)
        {
            priority = BehaviorPriority::GATHERING;
        }
        else if (name.find("trade") != std::string::npos)
        {
            priority = BehaviorPriority::TRADING;
        }
        else if (name == "solo")
        {
            priority = BehaviorPriority::SOLO;
        }

        _priorityManager->RegisterStrategy(strategyPtr, priority, exclusive);

        TC_LOG_DEBUG("module.playerbot.ai", "Registered strategy '{}' with priority {} (exclusive={})",
                     name, static_cast<uint8>(priority), exclusive);
    }
}

void BotAI::RemoveStrategy(std::string const& name)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    // Unregister from priority manager before removing
    auto it = _strategies.find(name);
    if (it != _strategies.end() && _priorityManager)
    {
        _priorityManager->UnregisterStrategy(it->second.get());
    }

    _strategies.erase(name);

    // Also remove from active strategies
    _activeStrategies.erase(
        std::remove(_activeStrategies.begin(), _activeStrategies.end(), name),
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return;
        }
        _activeStrategies.end()
    );
}

Strategy* BotAI::GetStrategy(std::string const& name) const
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    auto it = _strategies.find(name);
    return it != _strategies.end() ? it->second.get() : nullptr;
}

std::vector<Strategy*> BotAI::GetActiveStrategies() const
{
    // DEADLOCK FIX #11: Release lock BEFORE returning vector
    // The previous implementation held the lock during return, which means
    // the lock was held while the vector was being copied/moved.
    // If another thread requested unique_lock during that time,
    // and the calling code then tried to call GetStrategy(), DEADLOCK!
    std::vector<Strategy*> result;
    {
        std::lock_guard<std::recursive_mutex> lock(_mutex);

        // Access _strategies directly to avoid recursive mutex acquisition
        for (auto const& name : _activeStrategies)
        {
            auto it = _strategies.find(name);
            if (it != _strategies.end())
                result.push_back(it->second.get());
        }
    } // RELEASE LOCK BEFORE RETURN

    return result;  // Lock already released
}

void BotAI::ActivateStrategy(std::string const& name)
{
    // DEADLOCK FIX: Collect strategy pointer FIRST, then release lock BEFORE calling OnActivate
    // OnActivate callbacks may call GetStrategy() which acquires shared_lock
    // If another thread holds shared_lock and we hold unique_lock, calling OnActivate
    // which tries to acquire another shared_lock will deadlock due to writer-preference
    Strategy* strategy = nullptr;
    bool needsOnActivate = false;
    {
        std::lock_guard<std::recursive_mutex> lock(_mutex);

        // Check if strategy exists
        auto it = _strategies.find(name);
        if (it == _strategies.end())
            return;

        // Check if already active
        bool alreadyInList = std::find(_activeStrategies.begin(), _activeStrategies.end(), name) != _activeStrategies.end();
        bool wasActive = it->second->IsActive(this);  // Check BEFORE setting active

        if (!alreadyInList)
        {
            _activeStrategies.push_back(name);
        }

        // CRITICAL FIX: Set the strategy's internal _active flag so IsActive() returns true
        it->second->SetActive(true);

        // CRITICAL FIX: Call OnActivate if strategy is newly activated OR was never properly initialized
        // This handles both: new activations and re-activation of strategies that were improperly added
        needsOnActivate = !alreadyInList || !wasActive;

        TC_LOG_ERROR("module.playerbot.ai", "🔥 ACTIVATED STRATEGY: '{}' for bot {}, alreadyInList={}, wasActive={}, needsOnActivate={}",
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return;
    }
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return;
    }
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return;
        }
                     name, _bot->GetName(), alreadyInList, wasActive, needsOnActivate);

        // Get strategy pointer for callback
        strategy = it->second.get();
    } // RELEASE LOCK BEFORE CALLBACK

    // Call OnActivate hook WITHOUT holding lock if needed
    if (strategy && needsOnActivate)
    {
        TC_LOG_ERROR("module.playerbot.ai", "🎬 Calling OnActivate() for strategy '{}' on bot {}", name, _bot->GetName());
        strategy->OnActivate(this);
        TC_LOG_DEBUG("playerbot", "Activated strategy '{}' for bot {}", name, _bot->GetName());
    }
}

void BotAI::DeactivateStrategy(std::string const& name)
{
    // DEADLOCK FIX: Collect strategy pointer FIRST, then release lock BEFORE calling OnDeactivate
    // OnDeactivate callbacks may call GetStrategy() which acquires shared_lock
    // If another thread holds shared_lock and we hold unique_lock, calling OnDeactivate
    // which tries to acquire another shared_lock will deadlock due to writer-preference
    Strategy* strategy = nullptr;
    {
        std::lock_guard<std::recursive_mutex> lock(_mutex);

        // Find the strategy
        auto it = _strategies.find(name);
        if (it != _strategies.end())
        {
            // Set the strategy's internal _active flag to false
            it->second->SetActive(false);

            // Get strategy pointer for callback
            strategy = it->second.get();
        }

        _activeStrategies.erase(
            std::remove(_activeStrategies.begin(), _activeStrategies.end(), name),
            _activeStrategies.end()
        );
    } // RELEASE LOCK BEFORE CALLBACK

    // Call OnDeactivate hook WITHOUT holding lock
    if (strategy)
    {
        strategy->OnDeactivate(this);
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return;
        }
        TC_LOG_DEBUG("playerbot", "Deactivated strategy '{}' for bot {}", name, _bot->GetName());
    }
}

// ============================================================================
// ACTION EXECUTION
// ============================================================================

bool BotAI::ExecuteAction(std::string const& actionName)
{
    return ExecuteAction(actionName, ActionContext());
}

bool BotAI::ExecuteAction(std::string const& name, ActionContext const& context)
{
    // TODO: Implement action execution from name
    // This would look up the action by name and execute it
    return false;
}

bool BotAI::IsActionPossible(std::string const& actionName) const
{
    // TODO: Check if action is possible
    return false;
}

uint32 BotAI::GetActionPriority(std::string const& actionName) const
{
    // TODO: Get action priority
    return 0;
}

void BotAI::QueueAction(std::shared_ptr<Action> action, ActionContext const& context)
{
    if (!action)
        return;

    _actionQueue.push({action, context});
}

void BotAI::CancelCurrentAction()
{
    _currentAction = nullptr;
    _currentContext = ActionContext();
}

bool BotAI::CanExecuteAction(Action* action) const
{
    if (!action || !_bot)
        return false;

    return action->IsPossible(const_cast<BotAI*>(this)) && action->IsUseful(const_cast<BotAI*>(this));
}

ActionResult BotAI::ExecuteActionInternal(Action* action, ActionContext const& context)
{
    if (!action)
        return ActionResult::FAILED;

    return action->Execute(this, context);
}

// ============================================================================
// TARGET MANAGEMENT
// ============================================================================

::Unit* BotAI::GetTargetUnit() const
{
    if (!_bot || _currentTarget.IsEmpty())
        return nullptr;

    // FIX #19: Use ObjectCache instead of ObjectAccessor to avoid TrinityCore deadlock
    return _objectCache.GetTarget();
}

// ============================================================================
// MOVEMENT CONTROL
// ============================================================================

void BotAI::MoveTo(float x, float y, float z)
{
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsAlive");
        return;
    }
    if (!_bot || !_bot->IsAlive())
        return;

    _bot->GetMotionMaster()->MovePoint(0, x, y, z);
}

void BotAI::Follow(::Unit* target, float distance)
{
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsAlive");
        return nullptr;
    }
    if (!_bot || !_bot->IsAlive() || !target)
        return;

    _bot->GetMotionMaster()->MoveFollow(target, distance, 0.0f);
}

void BotAI::StopMovement()
{
    if (!_bot)
        return;

    _bot->StopMoving();
    _bot->GetMotionMaster()->Clear();
}

bool BotAI::IsMoving() const
{
    return _bot && _bot->isMoving();
}

// ============================================================================
// HELPER METHODS
// ============================================================================

void BotAI::SetAIState(BotAIState state)
{
    if (_aiState != state)
    {
        TC_LOG_DEBUG("playerbot", "Bot {} state change: {} -> {}",
                     if (!bot)
                     {
                         TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                         return;
                     }
                     _bot->GetName(),
                     static_cast<int>(_aiState),
                     static_cast<int>(state));
        _aiState = state;
    }
}

void BotAI::InitializeDefaultStrategies()
{
    // CRITICAL FIX: Create and register the follow strategy so it exists when activated
    // Without this, ActivateStrategy("follow") fails silently when bot joins group
    auto followBehavior = std::make_unique<LeaderFollowBehavior>();
    AddStrategy(std::move(followBehavior));

    // CRITICAL FIX: Create and register group combat strategy for combat assistance
    // This strategy makes bots attack when group members enter combat
    auto groupCombat = std::make_unique<GroupCombatStrategy>();
    AddStrategy(std::move(groupCombat));

    // CRITICAL: Create and register solo combat strategy for solo bot combat (Priority 70)
    // This strategy handles ALL combat when bot is solo (not in group):
    //  - Quest combat (kills quest targets)
    //  - Gathering defense (attacked while gathering)
    //  - Autonomous combat (bot finds hostile mob)
    //  - Trading interruption (attacked at vendor)
    // It provides positioning coordination and lets ClassAI handle spell rotation
    auto soloCombat = std::make_unique<SoloCombatStrategy>();
    AddStrategy(std::move(soloCombat));

    // Create quest strategy for quest objective navigation and completion
    // This strategy drives bots to quest locations, kills mobs, collects items, and turns in quests
    auto questStrategy = std::make_unique<QuestStrategy>();
    AddStrategy(std::move(questStrategy));

    // Create loot strategy for corpse looting and item pickup
    // This strategy drives bots to loot kills and collect valuable items
    auto lootStrategy = std::make_unique<LootStrategy>();
    AddStrategy(std::move(lootStrategy));

    // Create rest strategy for eating, drinking, and healing
    // This strategy drives bots to rest when resources are low
    auto restStrategy = std::make_unique<RestStrategy>();
    AddStrategy(std::move(restStrategy));

    // Create solo strategy for solo bot behavior coordination
    // This strategy coordinates all solo behaviors (questing, gathering, autonomous combat, etc.)
    auto soloStrategy = std::make_unique<SoloStrategy>();
    AddStrategy(std::move(soloStrategy));

    // NOTE: Mutual exclusion rules are automatically configured in BehaviorPriorityManager constructor
    // No need to add them here - they're already set up when _priorityManager is initialized

    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return nullptr;
    }
    TC_LOG_INFO("module.playerbot.ai", "✅ Initialized follow, group_combat, solo_combat, quest, loot, rest, and solo strategies for bot {}", _bot->GetName());

    // NOTE: Do NOT activate strategies here!
    // Strategy activation happens AFTER bot is fully loaded:
    // - For bots in groups: OnGroupJoined() activates follow/combat (called from BotSession after login)
    // - For solo bots: First UpdateAI() will activate solo strategies (see UpdateAI after basic checks)

    // Combat strategies are added by ClassAI
    // Additional strategies can be added based on configuration
}

void BotAI::UpdateValues(uint32 diff)
{
    // Update cached values used by triggers and actions
    // This includes distances, health percentages, resource levels, etc.
}

void BotAI::UpdateManagers(uint32 diff)
{
    // Phase 7.1: Integrated EventDispatcher + ManagerRegistry architecture
    // This replaces the old manual manager update approach with centralized event routing
    // DEBUG LOGGING THROTTLE: Only log every 50 seconds for whitelisted test bots
    // Using per-bot instance variable instead of static to prevent cross-bot interference
    _debugLogAccumulator += diff;
    static const std::set<std::string> testBots = {"Anderenz", "Boone", "Nelona", "Sevtap"};
    bool isTestBot = _bot && (testBots.find(_bot->GetName()) != testBots.end());
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return;
    }
    bool shouldLog = isTestBot && (_debugLogAccumulator >= 50000);
    if (shouldLog) _debugLogAccumulator = 0;

    if (shouldLog)
    {
        TC_LOG_ERROR("module.playerbot", "🔧 UpdateManagers ENTRY: Bot {}, IsInWorld()={}", _bot->GetName(), _bot->IsInWorld());
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsInWorld");
                    return nullptr;
                }
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return nullptr;
            }
    }

    if (!_bot || !_bot->IsInWorld())
    {
        if (shouldLog)
            TC_LOG_ERROR("module.playerbot", "❌ UpdateManagers EARLY RETURN: Bot {} not in world", _bot->GetName());
        return;
    }

    // ========================================================================
    // PHASE 7.1: EVENT DISPATCHER - Process queued events first
    // ========================================================================
    // Events from observers are queued and dispatched to managers.
    // This is the bridge between Phase 6 (observers) and Phase 7 (managers).
    if (_eventDispatcher)
    {
        // Process up to 100 events per update cycle to maintain performance
        uint32 eventsProcessed = _eventDispatcher->ProcessQueue(100);

        if (eventsProcessed > 0)
        {
            TC_LOG_TRACE("module.playerbot.events",
                "Bot {} processed {} events this cycle",
                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                    return;
                }
                _bot->GetName(), eventsProcessed);
        }

        // Warn if queue is backing up (>500 events indicates processing bottleneck)
        size_t queueSize = _eventDispatcher->GetQueueSize();
        if (queueSize > 500)
        {
            TC_LOG_WARN("module.playerbot.events",
                "Bot {} event queue backlog: {} events pending",
                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                    return;
                }
                _bot->GetName(), queueSize);
        }
    }

    // ========================================================================
    // PHASE 7.1: MANAGER REGISTRY - Update all registered managers
    // ========================================================================
    // The ManagerRegistry coordinates all manager updates with throttling.
    // This replaces the old manual update approach for each manager.
    if (_managerRegistry)
    {
        uint32 managersUpdated = _managerRegistry->UpdateAll(diff);

        if (managersUpdated > 0)
        {
            TC_LOG_TRACE("module.playerbot.managers",
                "Bot {} updated {} managers this cycle",
                if (!bot)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                    return;
                }
                _bot->GetName(), managersUpdated);
        }
    }

    // ========================================================================
    // LEGACY: Keep old manager updates for now during Phase 7 transition
    // ========================================================================
    // These will be removed once all managers are integrated with IManagerBase
    // and registered in ManagerRegistry during Phase 7.2-7.6

    // Quest manager handles quest acceptance, turn-in, and tracking
    if (_questManager)
    {
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return 0;
        }
        // TC_LOG_ERROR("module.playerbot", "🎯 Calling QuestManager->Update() for bot {}", _bot->GetName());
        _questManager->Update(diff);
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
        // TC_LOG_ERROR("module.playerbot", "✅ Returned from QuestManager->Update() for bot {}", _bot->GetName());
    }

    // Trade manager handles vendor interactions, repairs, and consumables
    if (_tradeManager)
    {
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return 0;
        }
        // TC_LOG_ERROR("module.playerbot", "🎯 Calling TradeManager->Update() for bot {}", _bot->GetName());
        _tradeManager->Update(diff);
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
        // TC_LOG_ERROR("module.playerbot", "✅ Returned from TradeManager->Update() for bot {}", _bot->GetName());
    }

    // Gathering manager handles mining, herbalism, skinning
    if (_gatheringManager)
    {
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
        // TC_LOG_ERROR("module.playerbot", "🎯 Calling GatheringManager->Update() for bot {}", _bot->GetName());
        _gatheringManager->Update(diff);
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
        // TC_LOG_ERROR("module.playerbot", "✅ Returned from GatheringManager->Update() for bot {}", _bot->GetName());
    }

    // Auction manager handles auction house buying, selling, and market scanning
    if (_auctionManager)
    {
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
        // TC_LOG_ERROR("module.playerbot", "🎯 Calling AuctionManager->Update() for bot {}", _bot->GetName());
        _auctionManager->Update(diff);
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
        // TC_LOG_ERROR("module.playerbot", "✅ Returned from AuctionManager->Update() for bot {}", _bot->GetName());
    }

    // Group coordinator handles group/raid mechanics, role assignment, and coordination
    if (_groupCoordinator)
    {
        _groupCoordinator->Update(diff);
    }

    // ========================================================================
    // EQUIPMENT AUTO-EQUIP - Check every 10 seconds
    // ========================================================================
    // EquipmentManager is a singleton that handles gear optimization for all bots
    // Only check periodically to avoid excessive inventory scanning
    _equipmentCheckTimer += diff;
    if (_equipmentCheckTimer >= 10000) // 10 seconds
    {
        _equipmentCheckTimer = 0;

        // Auto-equip better gear from inventory
        EquipmentManager::instance()->AutoEquipBestGear(_bot);
    }

    // ========================================================================
    // PROFESSION AUTOMATION - Check every 15 seconds
    // ========================================================================
    // ProfessionManager handles auto-learning, auto-leveling, and crafting automation
    // Less frequent checks to avoid excessive profession processing
    _professionCheckTimer += diff;
    if (_professionCheckTimer >= 15000) // 15 seconds
    {
        _professionCheckTimer = 0;

        // Update profession automation (auto-learn, auto-level, crafting)
        ProfessionManager::instance()->Update(_bot, diff);
    }

    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return nullptr;
    }
    // TC_LOG_ERROR("module.playerbot", "✅ UpdateManagers COMPLETE for bot {}", _bot->GetName());
}

// ============================================================================
// MOVEMENT ARBITER INTEGRATION - Convenience Methods
// ============================================================================

bool BotAI::RequestMovement(MovementRequest const& request)
{
    if (!_movementArbiter)
        return false;

    return _movementArbiter->RequestMovement(request);
}

bool BotAI::RequestPointMovement(
    PlayerBotMovementPriority priority,
    Position const& position,
    std::string const& reason,
    std::string const& sourceSystem)
{
    if (!_movementArbiter)
        return false;

    MovementRequest req = MovementRequest::MakePointMovement(
        priority,
        position,
        true,  // generatePath
        {},    // finalOrient
        {},    // speed
        {},    // closeEnoughDistance
        reason,
        sourceSystem);

    return _movementArbiter->RequestMovement(req);
}

bool BotAI::RequestChaseMovement(
    PlayerBotMovementPriority priority,
    ObjectGuid targetGuid,
    std::string const& reason,
    std::string const& sourceSystem)
{
    if (!_movementArbiter)
        return false;

    MovementRequest req = MovementRequest::MakeChaseMovement(
        priority,
        targetGuid,
        {},  // range
        {},  // angle
        reason,
        sourceSystem);

    return _movementArbiter->RequestMovement(req);
}

bool BotAI::RequestFollowMovement(
    PlayerBotMovementPriority priority,
    ObjectGuid targetGuid,
    float distance,
    std::string const& reason,
    std::string const& sourceSystem)
{
    if (!_movementArbiter)
        return false;

    MovementRequest req = MovementRequest::MakeFollowMovement(
        priority,
        targetGuid,
        distance,
        {},  // angle
        {},  // duration
        reason,
        sourceSystem);

    return _movementArbiter->RequestMovement(req);
}

void BotAI::StopAllMovement()
{
    if (_movementArbiter)
        _movementArbiter->StopMovement();
}

// NOTE: BotAIFactory implementation is in BotAIFactory.cpp
// Do not duplicate method definitions here to avoid linker errors

} // namespace Playerbot