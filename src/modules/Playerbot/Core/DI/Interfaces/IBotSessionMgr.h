/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "Define.h"
#include "ObjectGuid.h"
#include <cstdint>

namespace Playerbot
{

class BotSession;

/**
 * @brief Interface for Bot Session Management
 *
 * Abstracts bot session lifecycle and operations to enable dependency injection.
 * Manages network sessions for bot characters, handling creation, updates, and cleanup.
 *
 * **Responsibilities:**
 * - Create and release bot sessions
 * - Maintain session state and activity
 * - Update all sessions each frame
 * - Provide session lookup and statistics
 *
 * **Testability:**
 * - Can be mocked for testing without real network sessions
 * - Enables testing of bot AI without WorldSession dependencies
 *
 * **Usage:**
 * @code
 * auto sessionMgr = Services::Container().Resolve<IBotSessionMgr>();
 * auto session = sessionMgr->CreateSession(accountId, characterGuid);
 * @endcode
 */
class TC_GAME_API IBotSessionMgr
{
public:
    virtual ~IBotSessionMgr() = default;

    /**
     * @brief Initialize session manager
     *
     * Performs one-time initialization.
     * Must be called before any session operations.
     *
     * @return True if initialization successful, false otherwise
     */
    virtual bool Initialize() = 0;

    /**
     * @brief Shutdown session manager
     *
     * Releases all sessions and cleanup resources.
     * Called during server shutdown.
     */
    virtual void Shutdown() = 0;

    /**
     * @brief Create bot session for account
     *
     * Creates a new bot session for the specified account.
     * If session already exists, returns existing session.
     *
     * @param bnetAccountId Battle.net account ID
     * @return Pointer to bot session, or nullptr on failure
     */
    virtual BotSession* CreateSession(uint32 bnetAccountId) = 0;

    /**
     * @brief Create bot session with character
     *
     * Creates bot session and associates it with a character GUID.
     * Used when spawning bot for specific character.
     *
     * @param bnetAccountId Battle.net account ID
     * @param characterGuid Character GUID to associate
     * @return Pointer to bot session, or nullptr on failure
     */
    virtual BotSession* CreateSession(uint32 bnetAccountId, ObjectGuid characterGuid) = 0;

    /**
     * @brief Create async bot session
     *
     * Creates bot session asynchronously (non-blocking).
     * Useful for bulk bot spawning without blocking main thread.
     *
     * @param bnetAccountId Battle.net account ID
     * @param characterGuid Character GUID to associate
     * @return Pointer to bot session (may not be fully initialized yet)
     */
    virtual BotSession* CreateAsyncSession(uint32 bnetAccountId, ObjectGuid characterGuid) = 0;

    /**
     * @brief Release bot session
     *
     * Destroys and removes bot session for the specified account.
     * Safe to call even if session doesn't exist.
     *
     * @param bnetAccountId Battle.net account ID
     */
    virtual void ReleaseSession(uint32 bnetAccountId) = 0;

    /**
     * @brief Get bot session by account ID
     *
     * Retrieves existing bot session for the account.
     *
     * @param bnetAccountId Battle.net account ID
     * @return Pointer to bot session, or nullptr if not found
     *
     * @note Thread-safe for concurrent access
     */
    virtual BotSession* GetSession(uint32 bnetAccountId) const = 0;

    /**
     * @brief Update all bot sessions
     *
     * Called each frame to update all active bot sessions.
     * Processes packets, updates state, etc.
     *
     * @param diff Time elapsed since last update (milliseconds)
     */
    virtual void UpdateAllSessions(uint32 diff) = 0;

    /**
     * @brief Check if session manager is enabled
     *
     * @return True if enabled and processing sessions, false otherwise
     */
    virtual bool IsEnabled() const = 0;

    /**
     * @brief Enable or disable session manager
     *
     * When disabled, sessions are not updated.
     *
     * @param enabled True to enable, false to disable
     */
    virtual void SetEnabled(bool enabled) = 0;

    /**
     * @brief Get count of active sessions
     *
     * Returns the number of currently active bot sessions.
     *
     * @return Active session count
     */
    virtual uint32 GetActiveSessionCount() const = 0;

    /**
     * @brief Trigger character login for all sessions
     *
     * Forces all bot sessions to initiate character login process.
     * Used after server restart or when bots need to reconnect.
     */
    virtual void TriggerCharacterLoginForAllSessions() = 0;
};

} // namespace Playerbot
