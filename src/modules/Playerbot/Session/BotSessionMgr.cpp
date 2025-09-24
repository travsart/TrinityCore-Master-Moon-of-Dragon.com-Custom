/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#include "BotSessionMgr.h"
#include "BotSession.h"
#include "WorldSession.h"
#include "Log.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "DatabaseEnv.h"
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

    // Use synchronous character login (simplified approach)
    if (!session->LoginCharacter(characterGuid))
    {
        TC_LOG_ERROR("module.playerbot.session", "Failed to login character {} for bot session", characterGuid.ToString());
        // Clean up failed session
        delete session;
        return nullptr;
    }

    // Return successfully logged-in session
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
                ++it;
                continue;
            }

            // Removed async login checking - using synchronous approach
            // Sessions are now fully loaded when created, so always update if active
            if (session->IsActive()) {

                try {
                    WorldSessionFilter updater(session);
                    session->Update(diff, updater);
                } catch (std::exception const& e) {
                    TC_LOG_ERROR("module.playerbot.session", "🔍 Exception in async session update: {}", e.what());
                }

                ++it;
                continue;
            }

            // Ensure session has valid player before creating WorldSessionFilter
            // WorldSessionFilter expects fully initialized session state
            if (!session->GetPlayer()) {
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

void BotSessionMgr::TriggerCharacterLoginForAllSessions()
{
    TC_LOG_INFO("module.playerbot.session", "🚀 TriggerCharacterLoginForAllSessions: Starting character login for sessions without players");

    std::lock_guard<std::mutex> lock(_sessionsMutex);

    uint32 sessionsFound = 0;
    uint32 loginsTriggered = 0;

    // Iterate through all active sessions
    for (BotSession* session : _activeSessions)
    {
        if (!session) {
            continue;
        }

        sessionsFound++;

        // Check if session has no player (needs character login)
        if (!session->GetPlayer())
        {
            TC_LOG_INFO("module.playerbot.session",
                "🔑 Session for account {} has no player - looking up character for login",
                session->GetAccountId());

            // Query database to find a character for this account
            CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARS_BY_ACCOUNT_ID);
            stmt->setUInt32(0, session->GetAccountId());
            PreparedQueryResult result = CharacterDatabase.Query(stmt);

            if (!result || result->GetRowCount() == 0)
            {
                TC_LOG_WARN("module.playerbot.session",
                    "🔑 No characters found for account {}", session->GetAccountId());
                continue;
            }

            // Get the first character
            Field* fields = result->Fetch();
            ObjectGuid characterGuid = ObjectGuid::Create<HighGuid::Player>(fields[0].GetUInt64()); // guid

            TC_LOG_INFO("module.playerbot.session",
                "🔑 Triggering synchronous LoginCharacter for account {} with character {}",
                session->GetAccountId(), characterGuid.ToString());

            // Trigger synchronous character login
            if (session->LoginCharacter(characterGuid))
            {
                loginsTriggered++;
            }
            else
            {
                TC_LOG_ERROR("module.playerbot.session", "Failed to login character {} for account {}",
                           characterGuid.ToString(), session->GetAccountId());
            }
        }
        else
        {
            // MEMORY SAFETY: Protect against use-after-free when accessing Player name
            Player* player = session->GetPlayer();
            if (player) {
                try {
                    TC_LOG_INFO("module.playerbot.session",
                        "✅ Session for account {} already has player {}",
                        session->GetAccountId(), player->GetName().c_str());
                }
                catch (...) {
                    TC_LOG_WARN("module.playerbot.session",
                        "✅ Session for account {} already has player (name unavailable - use-after-free protection)",
                        session->GetAccountId());
                }
            }
            else {
                TC_LOG_WARN("module.playerbot.session",
                    "✅ Session for account {} has null player pointer",
                    session->GetAccountId());
            }
        }
    }

    TC_LOG_INFO("module.playerbot.session",
        "🚀 TriggerCharacterLoginForAllSessions: Complete - {} sessions found, {} logins triggered",
        sessionsFound, loginsTriggered);
}

} // namespace Playerbot