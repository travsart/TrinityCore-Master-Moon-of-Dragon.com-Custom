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
#include <string>
#include <functional>
#include <memory>

namespace Playerbot
{

// Forward declarations
struct BotSpawnEvent;
struct SpawnRequest;
class BotSession;
enum class BotSpawnEventType : uint32;

/**
 * @brief Interface for bot spawn event bus
 *
 * Provides publish-subscribe event bus for bot spawning workflow with
 * event queuing, batch processing, and performance monitoring.
 */
class TC_GAME_API IBotSpawnEventBus
{
public:
    using EventHandler = ::std::function<void(::std::shared_ptr<BotSpawnEvent>)>;
    using HandlerId = uint64;

    struct EventStats
    {
        uint64 eventsPublished;
        uint64 eventsProcessed;
        uint64 eventsDropped;
        uint64 totalProcessingTimeUs;
        uint32 queuedEvents;
    };

    virtual ~IBotSpawnEventBus() = default;

    // Lifecycle
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual void Update(uint32 diff) = 0;

    // Event publishing
    virtual void PublishEvent(::std::shared_ptr<BotSpawnEvent> event) = 0;
    virtual void PublishSpawnRequest(SpawnRequest const& request, ::std::function<void(bool, ObjectGuid)> callback) = 0;
    virtual void PublishCharacterSelected(ObjectGuid characterGuid, SpawnRequest const& request) = 0;
    virtual void PublishSessionCreated(::std::shared_ptr<BotSession> session, SpawnRequest const& request) = 0;
    virtual void PublishSpawnCompleted(ObjectGuid botGuid, bool success, ::std::string const& details = "") = 0;
    virtual void PublishPopulationChanged(uint32 zoneId, uint32 oldCount, uint32 newCount) = 0;

    // Event subscription
    virtual HandlerId Subscribe(BotSpawnEventType eventType, EventHandler handler) = 0;
    virtual HandlerId SubscribeToAll(EventHandler handler) = 0;
    virtual void Unsubscribe(HandlerId handlerId) = 0;

    // Event processing
    virtual void ProcessEvents() = 0;
    virtual void ProcessEventsOfType(BotSpawnEventType eventType) = 0;

    // Performance and monitoring
    virtual EventStats const& GetStats() const = 0;
    virtual void ResetStats() = 0;

    // Configuration
    virtual void SetMaxQueueSize(uint32 maxSize) = 0;
    virtual void SetBatchSize(uint32 batchSize) = 0;
    virtual void SetProcessingEnabled(bool enabled) = 0;

    virtual uint32 GetQueuedEventCount() const = 0;
    virtual bool IsHealthy() const = 0;
};

} // namespace Playerbot
