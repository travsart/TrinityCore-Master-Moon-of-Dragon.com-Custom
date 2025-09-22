/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#include "BotSessionMgr.h"
#include "BotSession.h"
#include "Log.h"
#include "ObjectGuid.h"
#include <algorithm>

namespace Playerbot {

bool BotSessionMgr::Initialize()
{
    printf("=== PLAYERBOT DEBUG: BotSessionMgr::Initialize() - simple version ===\n");
    fflush(stdout);

    if (_initialized.load()) {
        printf("=== PLAYERBOT DEBUG: BotSessionMgr already initialized ===\n");
        fflush(stdout);
        return true;
    }

    printf("=== PLAYERBOT DEBUG: Setting initialized and enabled flags ===\n");
    fflush(stdout);

    _initialized.store(true);
    _enabled.store(true);

    printf("=== PLAYERBOT DEBUG: BotSessionMgr initialized successfully (simple version) ===\n");
    fflush(stdout);

    return true;
}

void BotSessionMgr::Shutdown()
{
    if (!_initialized.load()) {
        return;
    }

    _enabled.store(false);

    // Clean up all sessions
    std::lock_guard<std::mutex> lock(_sessionsMutex);
    _sessions.clear();
    _activeSessions.clear();

    _initialized.store(false);
}

BotSession* BotSessionMgr::CreateSession(uint32 bnetAccountId)
{
    printf("=== PLAYERBOT DEBUG: BotSessionMgr::CreateSession() ENTERED for account %u (simple version) ===\n", bnetAccountId);
    fflush(stdout);

    if (!_enabled.load() || !_initialized.load()) {
        printf("=== PLAYERBOT DEBUG: BotSessionMgr not enabled or initialized ===\n");
        fflush(stdout);
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(_sessionsMutex);

    printf("=== PLAYERBOT DEBUG: Acquired lock, checking if session already exists ===\n");
    fflush(stdout);

    // Check if session already exists
    if (_sessions.find(bnetAccountId) != _sessions.end()) {
        printf("=== PLAYERBOT DEBUG: Session for account %u already exists ===\n", bnetAccountId);
        fflush(stdout);
        return nullptr;
    }

    printf("=== PLAYERBOT DEBUG: About to create new BotSession object ===\n");
    fflush(stdout);

    // Create new session
    try {
        auto session = std::make_unique<BotSession>(bnetAccountId);
        BotSession* sessionPtr = session.get();

        printf("=== PLAYERBOT DEBUG: BotSession object created successfully ===\n");
        fflush(stdout);

        // Store session
        _sessions[bnetAccountId] = std::move(session);
        _activeSessions.push_back(sessionPtr);

        printf("=== PLAYERBOT DEBUG: Session stored in maps, returning success ===\n");
        fflush(stdout);

        return sessionPtr;
    } catch (std::exception const& e) {
        printf("=== PLAYERBOT DEBUG: Exception creating session: %s ===\n", e.what());
        fflush(stdout);
        return nullptr;
    }
}

BotSession* BotSessionMgr::CreateSession(uint32 bnetAccountId, ObjectGuid characterGuid)
{
    printf("=== PLAYERBOT DEBUG: BotSessionMgr::CreateSession() with character GUID for account %u, character %s ===\n",
           bnetAccountId, characterGuid.ToString().c_str());
    fflush(stdout);

    // Create session first
    BotSession* session = CreateSession(bnetAccountId);
    if (!session)
    {
        printf("=== PLAYERBOT DEBUG: Failed to create session for account %u ===\n", bnetAccountId);
        fflush(stdout);
        return nullptr;
    }

    // Now login the character immediately
    printf("=== PLAYERBOT DEBUG: Calling LoginCharacter(%s) on session ===\n", characterGuid.ToString().c_str());
    fflush(stdout);

    if (!session->LoginCharacter(characterGuid))
    {
        printf("=== PLAYERBOT DEBUG: LoginCharacter FAILED for character %s ===\n", characterGuid.ToString().c_str());
        fflush(stdout);
        // Clean up the session if login failed
        ReleaseSession(bnetAccountId);
        return nullptr;
    }

    printf("=== PLAYERBOT DEBUG: LoginCharacter SUCCESS for character %s ===\n", characterGuid.ToString().c_str());
    fflush(stdout);

    return session;
}

BotSession* BotSessionMgr::CreateAsyncSession(uint32 bnetAccountId, ObjectGuid characterGuid)
{
    printf("=== PLAYERBOT DEBUG: BotSessionMgr::CreateAsyncSession() for account %u, character %s (ASYNC SCALABILITY) ===\n",
           bnetAccountId, characterGuid.ToString().c_str());
    fflush(stdout);

    // Create session first
    BotSession* session = CreateSession(bnetAccountId);
    if (!session)
    {
        printf("=== PLAYERBOT DEBUG: Failed to create session for account %u ===\n", bnetAccountId);
        fflush(stdout);
        return nullptr;
    }

    // Start async character login for 5000 bot scalability
    printf("=== PLAYERBOT DEBUG: Starting async login for character %s (scalable for 5000 bots) ===\n", characterGuid.ToString().c_str());
    fflush(stdout);

    session->StartAsyncLogin(characterGuid);

    // Return session immediately - login will complete asynchronously
    return session;
}

void BotSessionMgr::ReleaseSession(uint32 bnetAccountId)
{
    std::lock_guard<std::mutex> lock(_sessionsMutex);

    auto it = _sessions.find(bnetAccountId);
    if (it == _sessions.end()) {
        return;
    }

    BotSession* session = it->second.get();

    // Remove from active sessions
    _activeSessions.erase(
        std::remove(_activeSessions.begin(), _activeSessions.end(), session),
        _activeSessions.end());

    // Remove from sessions map
    _sessions.erase(it);
}

BotSession* BotSessionMgr::GetSession(uint32 bnetAccountId) const
{
    std::lock_guard<std::mutex> lock(_sessionsMutex);

    auto it = _sessions.find(bnetAccountId);
    return (it != _sessions.end()) ? it->second.get() : nullptr;
}

void BotSessionMgr::UpdateAllSessions(uint32 diff)
{
    if (!_enabled.load() || !_initialized.load()) {
        return;
    }

    std::lock_guard<std::mutex> lock(_sessionsMutex);

    // Simple sequential update - no complex threading
    for (BotSession* session : _activeSessions) {
        if (session) {
            // Session update would go here - for now just skip
            // Note: Removed IsActive() check to avoid access issues
        }
    }
}

uint32 BotSessionMgr::GetActiveSessionCount() const
{
    std::lock_guard<std::mutex> lock(_sessionsMutex);
    return static_cast<uint32>(_activeSessions.size());
}

} // namespace Playerbot