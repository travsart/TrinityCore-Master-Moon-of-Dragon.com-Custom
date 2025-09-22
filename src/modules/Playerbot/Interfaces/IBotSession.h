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

class Player;
class WorldPacket;

namespace Playerbot
{

/**
 * @interface IBotSession
 * @brief Abstract interface for bot session management
 *
 * ABSTRACTION LAYER: Provides clean interface for bot sessions that can be
 * implemented by different session types (socket-based, socketless, etc.)
 *
 * Benefits:
 * - Clean separation between session management and implementation
 * - Easy testing through mocking
 * - Support for different session implementations
 * - Clear API contract for session consumers
 * - Facilitates session pooling and lifecycle management
 */
class IBotSession
{
public:
    virtual ~IBotSession() = default;

    // === LIFECYCLE ===
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual void Update(uint32 diff) = 0;

    // === SESSION PROPERTIES ===
    virtual uint32 GetAccountId() const = 0;
    virtual ObjectGuid GetCharacterGuid() const = 0;
    virtual Player* GetPlayer() const = 0;
    virtual bool IsBot() const = 0;
    virtual bool IsActive() const = 0;

    // === PACKET HANDLING ===
    virtual void SendPacket(WorldPacket const* packet) = 0;
    virtual void QueuePacket(WorldPacket&& packet) = 0;
    virtual bool HasQueuedPackets() const = 0;
    virtual void ProcessQueuedPackets() = 0;

    // === BOT-SPECIFIC FUNCTIONALITY ===
    virtual void SimulatePacketReceive(WorldPacket const& packet) = 0;
    virtual void SetBotBehavior(std::string const& behavior) = 0;
    virtual std::string GetBotBehavior() const = 0;

    // === SESSION STATE ===
    virtual void SetOnline(bool online) = 0;
    virtual bool IsOnline() const = 0;
    virtual void SetInWorld(bool inWorld) = 0;
    virtual bool IsInWorld() const = 0;

    // === PERFORMANCE TRACKING ===
    virtual uint32 GetPacketsProcessed() const = 0;
    virtual uint32 GetPacketsQueued() const = 0;
    virtual uint64 GetLastActivity() const = 0;
    virtual void ResetStats() = 0;
};

/**
 * @interface IBotSessionFactory
 * @brief Abstract factory interface for creating bot sessions
 */
class IBotSessionFactory
{
public:
    virtual ~IBotSessionFactory() = default;

    virtual std::shared_ptr<IBotSession> CreateSession(uint32 accountId, ObjectGuid characterGuid) = 0;
    virtual std::shared_ptr<IBotSession> CreateSessionFromTemplate(std::string const& templateName, ObjectGuid characterGuid) = 0;
    virtual bool ValidateSession(std::shared_ptr<IBotSession> session) const = 0;
};

/**
 * @interface IBotSessionManager
 * @brief Abstract interface for managing multiple bot sessions
 */
class IBotSessionManager
{
public:
    virtual ~IBotSessionManager() = default;

    // === LIFECYCLE ===
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual void Update(uint32 diff) = 0;

    // === SESSION MANAGEMENT ===
    virtual std::shared_ptr<IBotSession> GetSession(ObjectGuid characterGuid) const = 0;
    virtual std::shared_ptr<IBotSession> GetSession(uint32 accountId) const = 0;
    virtual bool AddSession(std::shared_ptr<IBotSession> session) = 0;
    virtual bool RemoveSession(ObjectGuid characterGuid) = 0;

    // === QUERIES ===
    virtual uint32 GetSessionCount() const = 0;
    virtual uint32 GetActiveSessionCount() const = 0;
    virtual std::vector<std::shared_ptr<IBotSession>> GetAllSessions() const = 0;
    virtual std::vector<std::shared_ptr<IBotSession>> GetActiveSessions() const = 0;

    // === BATCH OPERATIONS ===
    virtual void UpdateAllSessions(uint32 diff) = 0;
    virtual void ShutdownAllSessions() = 0;
    virtual uint32 CleanupInactiveSessions() = 0;
};

} // namespace Playerbot