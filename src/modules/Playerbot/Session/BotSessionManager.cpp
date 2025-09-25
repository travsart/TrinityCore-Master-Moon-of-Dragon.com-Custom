/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#include "BotSessionManager.h"
#include "BotSession.h"
#include "WorldSession.h"
#include "Player.h"
#include "Log.h"
#include "AI/BotAI.h"
#include <unordered_map>
#include <mutex>

namespace Playerbot {

// Thread-safe registry of bot AI instances
static std::unordered_map<WorldSession*, BotAI*> s_botAIRegistry;
static std::mutex s_botAIMutex;

void BotSessionManager::UpdateBotSession(WorldSession* session, uint32 diff)
{
    if (!session)
        return;

#ifdef BUILD_PLAYERBOT
    if (!session->IsBot())
        return;
#else
    return; // Playerbot not enabled
#endif

    try
    {
        // SAFE APPROACH: Process bot callbacks first (if needed)
        ProcessBotCallbacks(session);

        // Then update bot AI
        ProcessBotAI(session, diff);
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("module.playerbot.session",
            "Exception in BotSessionManager::UpdateBotSession for account {}: {}",
            session->GetAccountId(), e.what());
    }
    catch (...)
    {
        TC_LOG_ERROR("module.playerbot.session",
            "Unknown exception in BotSessionManager::UpdateBotSession for account {}",
            session->GetAccountId());
    }
}

void BotSessionManager::RegisterBotAI(WorldSession* session, BotAI* ai)
{
    if (!session || !ai)
        return;

    std::lock_guard<std::mutex> lock(s_botAIMutex);
    s_botAIRegistry[session] = ai;

    TC_LOG_DEBUG("module.playerbot.session",
        "Registered BotAI for session {}", session->GetAccountId());
}

void BotSessionManager::UnregisterBotAI(WorldSession* session)
{
    if (!session)
        return;

    std::lock_guard<std::mutex> lock(s_botAIMutex);
    auto it = s_botAIRegistry.find(session);
    if (it != s_botAIRegistry.end())
    {
        s_botAIRegistry.erase(it);
        TC_LOG_DEBUG("module.playerbot.session",
            "Unregistered BotAI for session {}", session->GetAccountId());
    }
}

BotSession* BotSessionManager::GetBotSession(WorldSession* session)
{
    if (!session)
        return nullptr;

#ifdef BUILD_PLAYERBOT
    if (!session->IsBot())
        return nullptr;

    // Safe cast - only attempt if we know it's a bot session
    return static_cast<BotSession*>(session);
#else
    return nullptr; // Playerbot not enabled
#endif
}

void BotSessionManager::ProcessBotAI(WorldSession* session, uint32 diff)
{
    if (!session)
        return;

    Player* player = session->GetPlayer();
    if (!player || !player->IsInWorld())
        return;

    // Get registered AI
    BotAI* ai = nullptr;
    {
        std::lock_guard<std::mutex> lock(s_botAIMutex);
        auto it = s_botAIRegistry.find(session);
        if (it != s_botAIRegistry.end())
            ai = it->second;
    }

    if (!ai)
        return;

    try
    {
        // Update AI safely
        ai->Update(diff);
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("module.playerbot.session",
            "Exception in BotAI::Update for account {}: {}",
            session->GetAccountId(), e.what());

        // Unregister failed AI
        UnregisterBotAI(session);
    }
    catch (...)
    {
        TC_LOG_ERROR("module.playerbot.session",
            "Unknown exception in BotAI::Update for account {}",
            session->GetAccountId());

        // Unregister failed AI
        UnregisterBotAI(session);
    }
}

void BotSessionManager::ProcessBotCallbacks(WorldSession* session)
{
    if (!session)
        return;

    BotSession* botSession = GetBotSession(session);
    if (!botSession)
        return;

    try
    {
        // Only process callbacks if bot session specifically needs them
        // This avoids the recursive WorldSession::Update() calls
        if (botSession->GetLoginState() == BotSession::LoginState::QUERY_PENDING ||
            botSession->GetLoginState() == BotSession::LoginState::QUERY_COMPLETE)
        {
            // CRITICAL FIX: Process ALL callback processors, not just _queryProcessor
            // Previously only processed GetQueryProcessor(), missing _queryHolderProcessor!
            // AddQueryHolderCallback adds to _queryHolderProcessor, so we need this too
            session->ProcessQueryCallbacks(); // Processes all three: query, transaction, queryHolder
        }

        // Process pending login state changes
        if (botSession->GetLoginState() != BotSession::LoginState::NONE &&
            botSession->GetLoginState() != BotSession::LoginState::LOGIN_COMPLETE)
        {
            botSession->ProcessPendingLogin();
        }
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("module.playerbot.session",
            "Exception in ProcessBotCallbacks for account {}: {}",
            session->GetAccountId(), e.what());
    }
}

} // namespace Playerbot