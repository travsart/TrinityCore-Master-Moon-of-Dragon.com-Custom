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
#include <memory>

namespace Playerbot
{

// Forward declarations
class BotSession;

/**
 * @brief Interface for bot session resource pooling
 *
 * Manages a pool of reusable bot session objects to reduce allocation overhead
 * and improve performance for bot management systems.
 */
class TC_GAME_API IBotResourcePool
{
public:
    virtual ~IBotResourcePool() = default;

    // Lifecycle management
    virtual bool Initialize(uint32 initialPoolSize = 100) = 0;
    virtual void Shutdown() = 0;
    virtual void Update(uint32 diff) = 0;

    // Session management
    virtual ::std::shared_ptr<BotSession> AcquireSession(uint32 accountId) = 0;
    virtual void ReleaseSession(::std::shared_ptr<BotSession> session) = 0;
    virtual void ReturnSession(ObjectGuid botGuid) = 0;
    virtual void AddSession(::std::shared_ptr<BotSession> session) = 0;

    // Statistics and monitoring
    virtual void ResetStats() = 0;

    // Configuration
    virtual void SetMaxPoolSize(uint32 maxSize) = 0;
    virtual void SetMinPoolSize(uint32 minSize) = 0;

    // Status queries
    virtual uint32 GetActiveSessionCount() const = 0;
    virtual uint32 GetPooledSessionCount() const = 0;
    virtual uint32 GetAvailableSessionCount() const = 0;
    virtual bool CanAllocateSession() const = 0;

    // Maintenance
    virtual void CleanupIdleSessions() = 0;
};

} // namespace Playerbot
