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

    if (_initialized.load()) {
        return true;
    }


    _initialized.store(true);
    _enabled.store(true);


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

    if (!_enabled.load() || !_initialized.load()) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(_sessionsMutex);


    // Check if session already exists
    if (_sessions.find(bnetAccountId) != _sessions.end()) {
        return nullptr;
    }


    // Create new session
    try {
        auto session = std::make_unique<BotSession>(bnetAccountId);
        BotSession* sessionPtr = session.get();


        // Store session
        _sessions[bnetAccountId] = std::move(session);
        _activeSessions.push_back(sessionPtr);


        return sessionPtr;
    } catch (std::exception const& e) {
        return nullptr;
    }
}

BotSession* BotSessionMgr::CreateSession(uint32 bnetAccountId, ObjectGuid characterGuid)
{

    // Create session first
    BotSession* session = CreateSession(bnetAccountId);
    if (!session)
    {
        return nullptr;
    }

    // Now login the character immediately

    if (!session->LoginCharacter(characterGuid))
    {
        // Clean up the session if login failed
        ReleaseSession(bnetAccountId);
        return nullptr;
    }


    return session;
}

BotSession* BotSessionMgr::CreateAsyncSession(uint32 bnetAccountId, ObjectGuid characterGuid)
{

    // Create session first
    BotSession* session = CreateSession(bnetAccountId);
    if (!session)
    {
        return nullptr;
    }

    // Start async character login for 5000 bot scalability

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