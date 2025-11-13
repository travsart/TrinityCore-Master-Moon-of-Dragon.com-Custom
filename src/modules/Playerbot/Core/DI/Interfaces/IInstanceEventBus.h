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
#include <vector>
#include <functional>

namespace Playerbot
{

// Forward declarations
class BotAI;
struct InstanceEvent;
enum class InstanceEventType : uint8;

/**
 * @brief Interface for Instance Event Bus
 *
 * Central event distribution system for instance-related events
 * (resets, encounters, raid info, saves).
 *
 * Features:
 * - Event publishing and subscription
 * - Callback-based subscription support
 * - Thread-safe operations
 * - Event statistics tracking
 *
 * Thread Safety: All methods are thread-safe
 */
class TC_GAME_API IInstanceEventBus
{
public:
    virtual ~IInstanceEventBus() = default;

    using EventHandler = ::std::function<void(InstanceEvent const&)>;

    // ====================================================================
    // EVENT PUBLISHING
    // ====================================================================

    /**
     * @brief Publish an instance event to all subscribers
     * @param event The event to publish
     * @return true if event was delivered successfully
     * @note Thread-safe: Can be called from any thread
     */
    virtual bool PublishEvent(InstanceEvent const& event) = 0;

    // ====================================================================
    // SUBSCRIPTION MANAGEMENT
    // ====================================================================

    /**
     * @brief Subscribe to specific instance event types
     * @param subscriber The BotAI that wants to receive events
     * @param types Vector of event types to subscribe to
     * @note Subscriber must call Unsubscribe before destruction
     */
    virtual void Subscribe(BotAI* subscriber, ::std::vector<InstanceEventType> const& types) = 0;

    /**
     * @brief Subscribe to all instance event types
     * @param subscriber The BotAI that wants to receive all events
     */
    virtual void SubscribeAll(BotAI* subscriber) = 0;

    /**
     * @brief Unsubscribe from all events
     * @param subscriber The BotAI to remove
     * @note Must be called in BotAI destructor to prevent dangling pointers
     */
    virtual void Unsubscribe(BotAI* subscriber) = 0;

    /**
     * @brief Subscribe a callback handler to specific event types
     * @param handler Callback function to invoke when events occur
     * @param types Vector of event types to subscribe to
     * @return Subscription ID for later unsubscription
     */
    virtual uint32 SubscribeCallback(EventHandler handler, ::std::vector<InstanceEventType> const& types) = 0;

    /**
     * @brief Unsubscribe a callback handler
     * @param subscriptionId The ID returned from SubscribeCallback
     */
    virtual void UnsubscribeCallback(uint32 subscriptionId) = 0;

    // ====================================================================
    // STATISTICS
    // ====================================================================

    /**
     * @brief Get total events published
     * @return Total number of events published since creation
     */
    virtual uint64 GetTotalEventsPublished() const = 0;

    /**
     * @brief Get event count for specific type
     * @param type The event type to query
     * @return Number of events of this type published
     */
    virtual uint64 GetEventCount(InstanceEventType type) const = 0;
};

} // namespace Playerbot
