/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#include "BotSessionMgr.h"
#include "BotSession.h"
#include "WorldSession.h"
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
        TC_LOG_ERROR("module.playerbot.session",
            "Exception during session creation for account {}: {}", bnetAccountId, e.what());
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

    // DEBUG: Periodic update confirmation
    static uint32 updateCounter = 0;
    if (++updateCounter % 10000 == 0) {  // Every 10000 updates
        TC_LOG_INFO("module.playerbot.session", "🔄 BotSessionMgr::UpdateAllSessions called {} times, {} sessions active",
            updateCounter, _activeSessions.size());
    }

    std::lock_guard<std::mutex> lock(_sessionsMutex);

    // DEBUG: Log session update activity
    if (!_activeSessions.empty()) {
        TC_LOG_INFO("module.playerbot.session", "🔄 UpdateAllSessions: Processing {} active sessions", _activeSessions.size());
    }

    // Simple sequential update with async login state awareness
    for (auto it = _activeSessions.begin(); it != _activeSessions.end();) {
        BotSession* session = *it;

        // Comprehensive null pointer checks
        if (!session) {
            TC_LOG_ERROR("module.playerbot.session", "Found null session in _activeSessions, removing");
            it = _activeSessions.erase(it);
            continue;
        }

        // Additional safety: verify session is still valid
        try {
            if (!session->IsActive()) {
                TC_LOG_INFO("module.playerbot.session", "🔍 Session not active, skipping update");
                ++it;
                continue;
            }

            // Allow sessions in async login to be updated so callbacks can execute
            if (session->IsAsyncLoginInProgress()) {
                TC_LOG_INFO("module.playerbot.session", "🔍 Session in async login - updating to process callbacks");

                try {
                    TC_LOG_INFO("module.playerbot.session", "🔍 Creating WorldSessionFilter for async session");
                    WorldSessionFilter asyncUpdater(session);
                    TC_LOG_INFO("module.playerbot.session", "🔍 WorldSessionFilter created, calling session->Update()");
                    session->Update(diff, asyncUpdater);
                    TC_LOG_INFO("module.playerbot.session", "🔍 Session Update completed successfully");
                } catch (std::exception const& e) {
                    TC_LOG_ERROR("module.playerbot.session", "🔍 Exception in async session update: {}", e.what());
                }

                ++it;
                continue;
            }

            // Ensure session has valid player before creating WorldSessionFilter
            // WorldSessionFilter expects fully initialized session state
            if (!session->GetPlayer()) {
                TC_LOG_INFO("module.playerbot.session", "🔍 Session has no player yet, skipping update");
                ++it;
                continue;
            }

            // CRITICAL: Call Update to process async callbacks and AI
            // Create WorldSessionFilter only for fully initialized sessions
            WorldSessionFilter updater(session);
            TC_LOG_INFO("module.playerbot.session", "🔄 Calling session->Update() for session with player");
            session->Update(diff, updater);
            ++it;
        }
        catch (std::exception const& e) {
            TC_LOG_ERROR("module.playerbot.session",
                "Exception during BotSession update for session {}: {}",
                session ? "valid" : "null", e.what());
            ++it;
        }
        catch (...) {
            TC_LOG_ERROR("module.playerbot.session",
                "Unknown exception during BotSession update for session {}",
                session ? "valid" : "null");
            ++it;
        }
    }
}

uint32 BotSessionMgr::GetActiveSessionCount() const
{
    std::lock_guard<std::mutex> lock(_sessionsMutex);
    return static_cast<uint32>(_activeSessions.size());
}

} // namespace Playerbot