/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
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

#pragma once

#include "Define.h"
#include "ObjectGuid.h"
#include <vector>

namespace Playerbot
{

// Forward declarations
class BotAI;
struct CombatEvent;
enum class CombatEventType : uint8;

/**
 * @brief Interface for Combat Event Bus
 *
 * Central event distribution system for all combat-related events
 * (spell casting, damage, healing, threat, interrupts, CC).
 *
 * Features:
 * - Event publishing and subscription
 * - Priority-based event processing
 * - Thread-safe operations
 * - Event TTL and queue management
 * - Performance metrics and statistics
 * - Configurable batch processing
 *
 * Performance Targets:
 * - Event publishing: <5 microseconds
 * - Event processing: <500 microseconds per event
 * - Batch processing: 100 events in <5ms
 *
 * Thread Safety: All methods are thread-safe
 */
class TC_GAME_API ICombatEventBus
{
public:
    virtual ~ICombatEventBus() = default;

    // ====================================================================
    // EVENT PUBLISHING
    // ====================================================================

    /**
     * @brief Publish a combat event to all subscribers
     * @param event The event to publish
     * @return true if event was queued successfully
     * @note Thread-safe: Can be called from any thread
     */
    virtual bool PublishEvent(CombatEvent const& event) = 0;

    // ====================================================================
    // SUBSCRIPTION MANAGEMENT
    // ====================================================================

    /**
     * @brief Subscribe to specific combat event types
     * @param subscriber The BotAI that wants to receive events
     * @param types Vector of event types to subscribe to
     * @return true if subscription was successful
     * @note Subscriber must call Unsubscribe before destruction
     */
    virtual bool Subscribe(BotAI* subscriber, ::std::vector<CombatEventType> const& types) = 0;

    /**
     * @brief Subscribe to all combat event types
     * @param subscriber The BotAI that wants to receive all events
     * @return true if subscription was successful
     */
    virtual bool SubscribeAll(BotAI* subscriber) = 0;

    /**
     * @brief Unsubscribe from all events
     * @param subscriber The BotAI to remove
     * @note Must be called in BotAI destructor to prevent dangling pointers
     */
    virtual void Unsubscribe(BotAI* subscriber) = 0;

    // ====================================================================
    // EVENT PROCESSING
    // ====================================================================

    /**
     * @brief Process pending events and deliver to subscribers
     * @param diff Time elapsed since last update (milliseconds)
     * @param maxEvents Maximum events to process (100 default)
     * @return Number of events processed
     * @note Should be called from World update loop
     */
    virtual uint32 ProcessEvents(uint32 diff, uint32 maxEvents = 100) = 0;

    /**
     * @brief Process events for a specific unit only
     * @param unitGuid The unit to process events for
     * @param diff Time elapsed since last update
     * @return Number of events processed
     */
    virtual uint32 ProcessUnitEvents(ObjectGuid unitGuid, uint32 diff) = 0;

    /**
     * @brief Clear all events for a specific unit
     * @param unitGuid The unit to clear events for
     */
    virtual void ClearUnitEvents(ObjectGuid unitGuid) = 0;

    // ====================================================================
    // CONFIGURATION
    // ====================================================================

    /**
     * @brief Set maximum queue size
     * @param size Maximum number of events in queue
     */
    virtual void SetMaxQueueSize(uint32 size) = 0;

    /**
     * @brief Set event time-to-live
     * @param ttlMs Event TTL in milliseconds
     */
    virtual void SetEventTTL(uint32 ttlMs) = 0;

    /**
     * @brief Set batch processing size
     * @param size Number of events to process per batch
     */
    virtual void SetBatchSize(uint32 size) = 0;

    /**
     * @brief Get maximum queue size
     * @return Maximum queue size
     */
    virtual uint32 GetMaxQueueSize() const = 0;

    /**
     * @brief Get event time-to-live
     * @return Event TTL in milliseconds
     */
    virtual uint32 GetEventTTL() const = 0;

    /**
     * @brief Get batch processing size
     * @return Batch size
     */
    virtual uint32 GetBatchSize() const = 0;

    // ====================================================================
    // DIAGNOSTICS & DEBUGGING
    // ====================================================================

    /**
     * @brief Dump subscribers to log
     */
    virtual void DumpSubscribers() const = 0;

    /**
     * @brief Dump event queue to log
     */
    virtual void DumpEventQueue() const = 0;

    /**
     * @brief Get snapshot of current queue
     * @return Vector of queued events
     */
    virtual ::std::vector<CombatEvent> GetQueueSnapshot() const = 0;
};

} // namespace Playerbot
