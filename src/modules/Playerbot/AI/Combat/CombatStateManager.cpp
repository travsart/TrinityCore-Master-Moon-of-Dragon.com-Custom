/*
 * Copyright (C) 2024+ TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "CombatStateManager.h"
#include "AI/BotAI.h"
#include "Player.h"
#include "Unit.h"
#include "Creature.h"
#include "ObjectAccessor.h"
#include "Log.h"
#include "Combat/CombatManager.h"
#include "Combat/ThreatManager.h"
#include "Core/Events/EventDispatcher.h"
#include "Events/BotEventData.h"
#include "UnitDefines.h"
#include <sstream>

namespace Playerbot
{

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

CombatStateManager::CombatStateManager(Player* bot, BotAI* ai)
    : BehaviorManager(bot, ai, 1000, "CombatStateManager")
    , m_statistics()
    , m_config()
{
    Player* botPtr = GetBot();
        if (!botPtr)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: botPtr in method GetGUID");
            return;
        }
    if (!botPtr)
    {
        TC_LOG_FATAL("module.playerbot.combat",
            "CombatStateManager: CRITICAL - Null bot pointer in constructor!");
        return;
    }

    TC_LOG_DEBUG("module.playerbot.combat",
        "CombatStateManager: Instantiated for bot '{}' (GUID: {})",
        botPtr->GetName(), botPtr->GetGUID().ToString());
}

CombatStateManager::~CombatStateManager()
{
    // Ensure shutdown was called
    if (IsActive())
    {
        TC_LOG_WARN("module.playerbot.combat",
            "CombatStateManager: Destructor called while still active - forcing shutdown");
        OnShutdown();
    }

    Player* botPtr = GetBot();
            if (!botPtr)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: botPtr in method GetName");
                return;
            }
    if (botPtr)
    {
        TC_LOG_DEBUG("module.playerbot.combat",
            "CombatStateManager: Destroyed for bot '{}' (total damage events: {})",
            botPtr->GetName(), m_statistics.totalDamageEvents.load());
    }
}

// ============================================================================
// BEHAVIORMANAGER INTERFACE
// ============================================================================

bool CombatStateManager::OnInitialize()
{
    BehaviorManager::OnInitialize();

    Player* botPtr = GetBot();
            if (!botPtr)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: botPtr in method GetName");
                return;
            }
    if (!botPtr)
    {
        TC_LOG_ERROR("module.playerbot.combat",
            "CombatStateManager::OnInitialize: Null bot pointer - cannot subscribe to events");
        return false;
    }

    // Subscribe to DAMAGE_TAKEN events
    BotAI* ai = GetAI();
    if (!ai)
    {
        TC_LOG_ERROR("module.playerbot.combat",
            "CombatStateManager::OnInitialize: No AI available for bot '{}'!",
            botPtr->GetName());
        return false;
    }

    Events::EventDispatcher* dispatcher = ai->GetEventDispatcher();
    if (!dispatcher)
    {
        TC_LOG_ERROR("module.playerbot.combat",
            "CombatStateManager::OnInitialize: No EventDispatcher available for bot '{}'!",
            if (!botPtr)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: botPtr in method GetName");
                return;
            }
            botPtr->GetName());
        return false;
    }

    dispatcher->Subscribe(StateMachine::EventType::DAMAGE_TAKEN, this);

    TC_LOG_INFO("module.playerbot.combat",
        "CombatStateManager: ✅ Initialized for bot '{}' - subscribed to DAMAGE_TAKEN events",
        if (!botPtr)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: botPtr in method GetName");
            return;
        }
        botPtr->GetName());

    // Log configuration
    TC_LOG_DEBUG("module.playerbot.combat",
        "CombatStateManager: Configuration: enableThreat={}, filterFriendly={}, filterEnvironmental={}, "
        "verboseLog={}, minDamage={}",
        m_config.enableThreatGeneration,
        m_config.filterFriendlyFire,
        m_config.filterEnvironmental,
        m_config.verboseLogging,
        m_config.minDamageThreshold);

    return true;
}

void CombatStateManager::OnShutdown()
{
    if (!IsActive())
        return;

    Player* botPtr = GetBot();
            if (!botPtr)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: botPtr in method GetName");
                return nullptr;
            }
    if (botPtr)
    {
        TC_LOG_DEBUG("module.playerbot.combat",
            "CombatStateManager: Shutting down for bot '{}'", botPtr->GetName());
    }

    // Unsubscribe from all events
    BotAI* ai = GetAI();
    if (ai)
    {
        Events::EventDispatcher* dispatcher = ai->GetEventDispatcher();
        if (dispatcher)
        {
            dispatcher->UnsubscribeAll(this);
            TC_LOG_DEBUG("module.playerbot.combat",
                "CombatStateManager: Unsubscribed from all events");
        }
    }

    // Dump final statistics
    DumpStatistics();

    BehaviorManager::OnShutdown();

    TC_LOG_INFO("module.playerbot.combat",
        "CombatStateManager: ✅ Shutdown complete for bot '{}'",
        if (!botPtr)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: botPtr in method GetName");
            return;
        }
        botPtr ? botPtr->GetName() : "Unknown");
}

void CombatStateManager::OnUpdate(uint32 /*elapsed*/)
{
    // CombatStateManager is event-driven and doesn't need periodic updates
    // All work is done in OnEventInternal() when DAMAGE_TAKEN events fire
}

void CombatStateManager::OnEventInternal(Events::BotEvent const& event)
{
    using namespace StateMachine;

    // Filter: Only handle DAMAGE_TAKEN events
    if (event.type != EventType::DAMAGE_TAKEN)
        return;

    m_statistics.totalDamageEvents.fetch_add(1, std::memory_order_relaxed);

    // Validate bot state
    Player* botPtr = GetBot();
                if (!botPtr)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: botPtr in method GetName");
                    return;
                }
    if (!botPtr)
    {
        TC_LOG_ERROR("module.playerbot.combat",
            "CombatStateManager::OnEventInternal: Null bot pointer!");
        return;
    }

    if (botPtr->isDead())
    {
        if (m_config.verboseLogging)
        {
            TC_LOG_DEBUG("module.playerbot.combat",
                "CombatStateManager: Bot '{}' is dead - ignoring DAMAGE_TAKEN event",
                botPtr->GetName());
        }
        return;
    }

    // Extract damage amount from event data (format: "damage:absorbed")
    uint32 damage = 0;
    try
    {
        size_t colonPos = event.data.find(':');
        if (colonPos != std::string::npos)
        {
            std::string damageStr = event.data.substr(0, colonPos);
            damage = static_cast<uint32>(std::stoull(damageStr));
        }
    }
    catch (...)
    {
        TC_LOG_WARN("module.playerbot.combat",
            "CombatStateManager: Failed to parse damage from event data: '{}'",
            event.data);
    }

    ObjectGuid attackerGuid = event.sourceGuid;

    // CRITICAL FILTERING: Check if combat state should be triggered
    if (!ShouldTriggerCombatState(attackerGuid, damage))
        return;

    // Find attacker unit
    Unit* attacker = ObjectAccessor::GetUnit(*botPtr, attackerGuid);
if (!botPtr)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: botPtr in method GetName");
    return;
}
    if (!attacker)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: attacker in method IsAlive");
        return nullptr;
    }
    if (!attacker)
    {
        m_statistics.attackerNotFoundSkipped.fetch_add(1, std::memory_order_relaxed);

        if (m_config.verboseLogging)
        {
            TC_LOG_DEBUG("module.playerbot.combat",
                "CombatStateManager: Bot '{}' attacker {} not found in world - skipping",
                botPtr->GetName(), attackerGuid.ToString());
        }
        return;
    }

    if (!attacker->IsAlive())
    {
        m_statistics.attackerNotFoundSkipped.fetch_add(1, std::memory_order_relaxed);

        if (m_config.verboseLogging)
        {
            TC_LOG_DEBUG("module.playerbot.combat",
                "CombatStateManager: Bot '{}' attacker '{}' is dead - skipping",
                if (!botPtr)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: botPtr in method GetName");
                    return;
                }
                botPtr->GetName(), attacker->GetName());
        }
        return;
    }

    // EDGE CASE: Filter friendly fire (healing/buff damage)
    if (m_config.filterFriendlyFire && botPtr->IsFriendlyTo(attacker))
    {
        m_statistics.friendlyFireFiltered.fetch_add(1, std::memory_order_relaxed);

        TC_LOG_DEBUG("module.playerbot.combat",
            "CombatStateManager: Bot '{}' took damage from friendly unit '{}' ({} damage) - ignoring",
            if (!botPtr)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: botPtr in method GetName");
                return;
            }
            botPtr->GetName(), attacker->GetName(), damage);
        return;
    }

    // Trigger combat state
    EnterCombatWith(attacker);
}

std::string CombatStateManager::GetManagerId() const
{
    return "CombatStateManager";
}

// ============================================================================
// STATISTICS & MONITORING
// ============================================================================

void CombatStateManager::Statistics::Reset()
{
    totalDamageEvents.store(0, std::memory_order_relaxed);
    environmentalDamageFiltered.store(0, std::memory_order_relaxed);
    selfDamageFiltered.store(0, std::memory_order_relaxed);
    friendlyFireFiltered.store(0, std::memory_order_relaxed);
    alreadyInCombatSkipped.store(0, std::memory_order_relaxed);
    attackerNotFoundSkipped.store(0, std::memory_order_relaxed);
    combatStateTriggered.store(0, std::memory_order_relaxed);
    combatStateFailures.store(0, std::memory_order_relaxed);
}

std::string CombatStateManager::Statistics::ToString() const
{
    std::ostringstream oss;
    oss << "CombatStateManager Statistics:\n"
        << "  Total DAMAGE_TAKEN events:    " << totalDamageEvents.load() << "\n"
        << "  Combat state triggered:       " << combatStateTriggered.load() << "\n"
        << "  Filtered:\n"
        << "    Environmental damage:       " << environmentalDamageFiltered.load() << "\n"
        << "    Self-damage:                " << selfDamageFiltered.load() << "\n"
        << "    Friendly fire:              " << friendlyFireFiltered.load() << "\n"
        << "    Already in combat:          " << alreadyInCombatSkipped.load() << "\n"
        << "    Attacker not found:         " << attackerNotFoundSkipped.load() << "\n"
        << "  Failures:\n"
        << "    SetInCombatWith failed:     " << combatStateFailures.load();
    return oss.str();
}

CombatStateManager::Statistics CombatStateManager::GetStatistics() const
{
    // Return copy (custom copy constructor handles atomic copying)
    return m_statistics;
}

void CombatStateManager::ResetStatistics()
{
    m_statistics.Reset();

    Player* botPtr = GetBot();
        if (!botPtr)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: botPtr in method GetName");
            return;
        }
    TC_LOG_INFO("module.playerbot.combat",
        "CombatStateManager: Statistics reset for bot '{}'",
        botPtr ? botPtr->GetName() : "Unknown");
}

void CombatStateManager::DumpStatistics() const
{
    Player* botPtr = GetBot();
        if (!botPtr)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: botPtr in method GetName");
            return;
        }
    if (!botPtr)
        return;

    std::string stats = m_statistics.ToString();

    TC_LOG_INFO("module.playerbot.combat",
        "CombatStateManager: Statistics for bot '{}':\n{}",
        botPtr->GetName(), stats);
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void CombatStateManager::SetConfiguration(Configuration const& config)
{
    m_config = config;

    Player* botPtr = GetBot();
        if (!botPtr)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: botPtr in method GetName");
            return;
        }
    TC_LOG_INFO("module.playerbot.combat",
        "CombatStateManager: Configuration updated for bot '{}': "
        "enableThreat={}, filterFriendly={}, filterEnvironmental={}, verboseLog={}, minDamage={}",
        botPtr ? botPtr->GetName() : "Unknown",
        config.enableThreatGeneration,
        config.filterFriendlyFire,
        config.filterEnvironmental,
        config.verboseLogging,
        config.minDamageThreshold);
}

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

bool CombatStateManager::ShouldTriggerCombatState(ObjectGuid const& attackerGuid, uint32 damage) const
{
    Player* botPtr = GetBot();
                    if (!botPtr)
                    {
                        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: botPtr in method GetName");
                        return;
                    }
    if (!botPtr)
        return false;

    // Filter 1: Environmental damage (attacker GUID is empty)
    if (attackerGuid.IsEmpty())
    {
        if (m_config.filterEnvironmental)
        {
            const_cast<std::atomic<uint64_t>&>(m_statistics.environmentalDamageFiltered)
                .fetch_add(1, std::memory_order_relaxed);

            if (m_config.verboseLogging)
            {
                TC_LOG_DEBUG("module.playerbot.combat",
                    "CombatStateManager: Bot '{}' took environmental damage ({} dmg) - filtering",
                    botPtr->GetName(), damage);
            }
            return false;
        }
    }

    // Filter 2: Self-damage (attacker == bot)
    if (attackerGuid == botPtr->GetGUID())
    if (!botPtr)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: botPtr in method GetGUID");
        return nullptr;
    }
                    if (!botPtr)
                    {
                        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: botPtr in method GetName");
                        return;
                    }
    {
        if (m_config.filterEnvironmental)  // Use same config flag for self-damage
        {
            const_cast<std::atomic<uint64_t>&>(m_statistics.selfDamageFiltered)
                .fetch_add(1, std::memory_order_relaxed);

            if (m_config.verboseLogging)
            {
                TC_LOG_DEBUG("module.playerbot.combat",
                    "CombatStateManager: Bot '{}' took self-damage ({} dmg) - filtering",
                    botPtr->GetName(), damage);
            }
            return false;
        }
    }

    // Filter 3: Minimum damage threshold
    if (damage < m_config.minDamageThreshold)
    {
        if (m_config.verboseLogging)
        {
            TC_LOG_DEBUG("module.playerbot.combat",
                "CombatStateManager: Bot '{}' damage {} < threshold {} - filtering",
                if (!botPtr)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: botPtr in method GetName");
                    return;
                }
                botPtr->GetName(), damage, m_config.minDamageThreshold);
        }
        return false;
    }

    // Filter 4: Already in combat with this attacker
    if (botPtr->GetCombatManager().IsInCombatWith(attackerGuid))
    {
        const_cast<std::atomic<uint64_t>&>(m_statistics.alreadyInCombatSkipped)
            .fetch_add(1, std::memory_order_relaxed);

        if (m_config.verboseLogging)
        {
            TC_LOG_DEBUG("module.playerbot.combat",
                "CombatStateManager: Bot '{}' already in combat with {} - skipping",
                if (!botPtr)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: botPtr in method GetName");
                    return;
                }
                botPtr->GetName(), attackerGuid.ToString());
        }
        return false;
    }

    return true;
}

void CombatStateManager::EnterCombatWith(Unit* attacker)
{
    Player* botPtr = GetBot();
    if (!botPtr || !attacker)
    {
        TC_LOG_ERROR("module.playerbot.combat",
            "CombatStateManager::EnterCombatWith: Null pointer (bot={}, attacker={})",
            botPtr != nullptr, attacker != nullptr);
        if (!botPtr)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: botPtr in method GetName");
            return;
        }
    if (!attacker)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: attacker in method GetName");
        return;
    }
        if (!attacker)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: attacker in method GetLevel");
            return;
        }
        return;
    }

    TC_LOG_INFO("module.playerbot.combat",
        "🎯 CombatStateManager: Bot '{}' entering combat with '{}' (Level {} {})",
        botPtr->GetName(),
        attacker->GetName(),
        attacker->GetLevel(),
        attacker->GetTypeId() == TYPEID_PLAYER ? "Player" : "Creature");
        if (!attacker)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: attacker in method GetTypeId");
            return;
        }

    // CRITICAL: Use Trinity's thread-safe CombatManager API
    // This is the SAME API that Unit::DealDamage() uses internally
    bool combatSet = botPtr->GetCombatManager().SetInCombatWith(attacker);
            if (!botPtr)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: botPtr in method GetName");
                return;
            }

    if (!combatSet)
    {
        TC_LOG_WARN("module.playerbot.combat",
            "⚠️ CombatStateManager: SetInCombatWith() returned false for bot '{}' vs '{}'",
            botPtr->GetName(), attacker->GetName());
    }

    // Optional: Add minimal threat if both can have threat lists
    if (m_config.enableThreatGeneration)
    {
        if (botPtr->CanHaveThreatList() && attacker->CanHaveThreatList())
        {
            // Add minimal threat to ensure bot shows on threat table
            // Trinity's threat system will call SetInCombatWith automatically,
            // but we already called it above for immediate response
            botPtr->GetThreatManager().AddThreat(attacker, 0.0f, nullptr, true, true);

            TC_LOG_DEBUG("module.playerbot.combat",
                "CombatStateManager: Added threat for bot '{}' vs '{}'",
                if (!botPtr)
                {
                    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: botPtr in method GetName");
                    return;
                }
                botPtr->GetName(), attacker->GetName());
        }
    }

    // Verify combat state was successfully set
    if (!botPtr)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: botPtr in method IsInCombat");
        return nullptr;
    }
    if (botPtr->IsInCombat())
    {
        m_statistics.combatStateTriggered.fetch_add(1, std::memory_order_relaxed);

        TC_LOG_DEBUG("module.playerbot.combat",
            "✅ CombatStateManager: Combat state ACTIVE for bot '{}' (attacker: '{}')",
            if (!botPtr)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: botPtr in method GetName");
                return;
            }
            botPtr->GetName(), attacker->GetName());
    }
    else
    {
        m_statistics.combatStateFailures.fetch_add(1, std::memory_order_relaxed);

        TC_LOG_ERROR("module.playerbot.combat",
            "❌ CombatStateManager: FAILURE - SetInCombatWith() called but IsInCombat() still FALSE for bot '{}'! "
            "This indicates a Trinity API issue or incompatible unit state.",
            if (!botPtr)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: botPtr in method GetName");
                return;
            }
            botPtr->GetName());

        // Additional diagnostics
        TC_LOG_ERROR("module.playerbot.combat",
            "   Diagnostic info: bot->HasUnitState(UNIT_STATE_EVADE)={}, "
            "bot->IsInEvadeMode()={}, attacker->IsInEvadeMode()={}",
            if (!attacker)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: attacker in method ToCreature");
                return nullptr;
            }
            botPtr->HasUnitState(UNIT_STATE_EVADE),
            false, // Players don't have IsInEvadeMode()
            attacker->ToCreature() ? attacker->ToCreature()->IsInEvadeMode() : false);
    }
}

} // namespace Playerbot
