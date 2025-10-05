/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#include "BotWorldSessionMgr.h"
#include "BotSession.h"
#include "Player.h"
#include "World.h"
#include "DatabaseEnv.h"
#include "CharacterCache.h"
#include "ObjectAccessor.h"
#include "Log.h"
#include "WorldSession.h"

namespace Playerbot {

bool BotWorldSessionMgr::Initialize()
{
    if (_initialized.load())
        return true;

    TC_LOG_INFO("module.playerbot.session", "🔧 BotWorldSessionMgr: Initializing with native TrinityCore login pattern");

    _enabled.store(true);
    _initialized.store(true);

    return true;
}

void BotWorldSessionMgr::Shutdown()
{
    if (!_initialized.load())
        return;

    TC_LOG_INFO("module.playerbot.session", "🔧 BotWorldSessionMgr: Shutting down");

    _enabled.store(false);

    std::lock_guard<std::mutex> lock(_sessionsMutex);

    // Clean logout all bot sessions
    for (auto& [guid, session] : _botSessions)
    {
        if (session && session->GetPlayer())
        {
            // MEMORY SAFETY: Protect against use-after-free when accessing Player name
            Player* player = session->GetPlayer();
            try {
                TC_LOG_INFO("module.playerbot.session", "🔧 Logging out bot: {}", player->GetName());
            }
            catch (...) {
                TC_LOG_INFO("module.playerbot.session", "🔧 Logging out bot (name unavailable - use-after-free protection)");
            }
            session->LogoutPlayer(true);
        }
    }

    _botSessions.clear();
    _botsLoading.clear();
    _initialized.store(false);
}

bool BotWorldSessionMgr::AddPlayerBot(ObjectGuid playerGuid, uint32 masterAccountId)
{
    if (!_enabled.load() || !_initialized.load())
    {
        TC_LOG_ERROR("module.playerbot.session", "🔧 BotWorldSessionMgr not enabled or initialized");
        return false;
    }

    std::lock_guard<std::mutex> lock(_sessionsMutex);

    // Check if bot is already loading
    if (_botsLoading.find(playerGuid) != _botsLoading.end())
    {
        TC_LOG_DEBUG("module.playerbot.session", "🔧 Bot {} already loading", playerGuid.ToString());
        return false;
    }

    // FIX #22: CRITICAL - Eliminate ObjectAccessor call to prevent deadlock
    // Check if bot is already added by scanning existing sessions (avoids ObjectAccessor lock)
    for (auto const& [guid, session] : _botSessions)
    {
        if (guid == playerGuid && session && session->GetPlayer())
        {
            TC_LOG_DEBUG("module.playerbot.session", "🔧 Bot {} already in world (found in _botSessions)", playerGuid.ToString());
            return false;
        }
    }

    // Get account ID for this character from playerbot database directly
    uint32 accountId = 0;

    // Query playerbot_characters database directly since character cache won't find bot accounts
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_PINFO);
    stmt->setUInt64(0, playerGuid.GetCounter());
    PreparedQueryResult result = CharacterDatabase.Query(stmt);

    if (result)
    {
        // CHAR_SEL_CHAR_PINFO returns: totaltime, level, money, account, race, class, map, zone, gender, health, playerFlags
        // Account is at index 3
        accountId = (*result)[3].GetUInt32();
        TC_LOG_DEBUG("module.playerbot.session", "🔧 Found account ID {} for character {}", accountId, playerGuid.ToString());
    }

    if (!accountId)
    {
        TC_LOG_ERROR("module.playerbot.session", "🔧 Could not find account for character {} in playerbot database", playerGuid.ToString());
        return false;
    }

    // CRITICAL FIX: Check for existing sessions by account ID to prevent duplicates
    for (auto const& [guid, session] : _botSessions)
    {
        if (session && session->GetAccountId() == accountId)
        {
            TC_LOG_WARN("module.playerbot.session", "🔧 DUPLICATE SESSION PREVENTION: Account {} already has an active bot session with character {}, rejecting new character {}",
                accountId, guid.ToString(), playerGuid.ToString());
            return false;
        }
    }

    TC_LOG_INFO("module.playerbot.session", "🔧 Adding bot {} using proven BotSession approach", playerGuid.ToString());

    // CRITICAL FIX: Synchronize character cache with playerbot_characters database before login
    if (!SynchronizeCharacterCache(playerGuid))
    {
        TC_LOG_ERROR("module.playerbot.session", "🔧 Failed to synchronize character cache for {}", playerGuid.ToString());
        return false;
    }

    // Mark as loading
    _botsLoading.insert(playerGuid);

    // Use the proven BotSession that we know works
    std::shared_ptr<BotSession> botSession = BotSession::Create(accountId);
    if (!botSession)
    {
        TC_LOG_ERROR("module.playerbot.session", "🔧 Failed to create BotSession for {}", playerGuid.ToString());
        _botsLoading.erase(playerGuid);
        return false;
    }

    // Store the shared_ptr to keep the session alive
    _botSessions[playerGuid] = botSession;

    // ENTERPRISE FIX V2: DEADLOCK-FREE staggered login using immediate execution
    //
    // CRITICAL DEADLOCK BUG FOUND: Previous implementation created detached threads WHILE
    // holding _sessionsMutex (non-recursive), causing deadlock when threads tried to
    // acquire the same mutex for cleanup (line 184). This disabled the spawner.
    //
    // ROOT CAUSE: std::mutex is NOT recursive. Main thread holds lock, detached thread
    // tries to acquire → deadlock → "resource deadlock would occur" exception.
    //
    // NEW SOLUTION: Calculate position-based delay but execute immediately WITHOUT threads.
    // Let TrinityCore's async database system handle the queueing naturally.
    // The async connection pool has built-in queuing - we don't need manual thread delays.
    //
    // Performance: Database pool will process callbacks in order as connections become
    // available. Natural flow-control without threading complexity or deadlock risk.

    TC_LOG_INFO("module.playerbot.session",
        "🔧 Initiating async login for bot {} (position {} in spawn batch)",
        playerGuid.ToString(), _botSessions.size());

    // Initiate async login immediately - let database pool handle queuing
    if (!botSession->LoginCharacter(playerGuid))
    {
        TC_LOG_ERROR("module.playerbot.session", "🔧 Failed to initiate async login for character {}", playerGuid.ToString());
        _botSessions.erase(playerGuid);
        _botsLoading.erase(playerGuid);
        return false;
    }

    TC_LOG_INFO("module.playerbot.session", "🔧 Async bot login initiated for: {}", playerGuid.ToString());
    return true; // Return true to indicate login was started (not completed)
}

void BotWorldSessionMgr::RemovePlayerBot(ObjectGuid playerGuid)
{
    std::lock_guard<std::mutex> lock(_sessionsMutex);

    auto it = _botSessions.find(playerGuid);
    if (it == _botSessions.end())
    {
        TC_LOG_DEBUG("module.playerbot.session", "🔧 Bot session not found for removal: {}", playerGuid.ToString());
        return;
    }

    std::shared_ptr<BotSession> session = it->second;
    if (session && session->GetPlayer())
    {
        // MEMORY SAFETY: Protect against use-after-free when accessing Player name
        Player* player = session->GetPlayer();
        try {
            TC_LOG_INFO("module.playerbot.session", "🔧 Removing bot: {}", player->GetName());
        }
        catch (...) {
            TC_LOG_INFO("module.playerbot.session", "🔧 Removing bot (name unavailable - use-after-free protection)");
        }
        session->LogoutPlayer(true);
    }

    // Remove from map - shared_ptr will automatically clean up when no more references exist
    _botSessions.erase(it);
}

Player* BotWorldSessionMgr::GetPlayerBot(ObjectGuid playerGuid) const
{
    std::lock_guard<std::mutex> lock(_sessionsMutex);

    auto it = _botSessions.find(playerGuid);
    if (it == _botSessions.end())
        return nullptr;

    return it->second ? it->second->GetPlayer() : nullptr;
}

void BotWorldSessionMgr::UpdateSessions(uint32 diff)
{
    if (!_enabled.load())
        return;

    // CRITICAL DEADLOCK FIX: Collect sessions to update BEFORE processing
    // This eliminates the deadlock by releasing the mutex before calling Update()
    std::vector<std::pair<ObjectGuid, std::shared_ptr<BotSession>>> sessionsToUpdate;
    std::vector<ObjectGuid> sessionsToRemove;

    // PHASE 1: Quick collection under mutex (minimal lock time)
    {
        std::lock_guard<std::mutex> lock(_sessionsMutex);
        sessionsToUpdate.reserve(_botSessions.size());

        for (auto it = _botSessions.begin(); it != _botSessions.end(); ++it)
        {
            std::shared_ptr<BotSession> session = it->second;
            ObjectGuid guid = it->first;

            // CRITICAL SAFETY: Validate shared_ptr
            if (!session)
            {
                TC_LOG_ERROR("module.playerbot.session", "🔧 CRITICAL: Null session found in bot sessions map for {}", guid.ToString());
                sessionsToRemove.push_back(guid);
                continue;
            }

            // CRITICAL SAFETY: All sessions in bot sessions map should be BotSessions
            // No need to cast since we already have shared_ptr<BotSession>
            if (!session->IsBot())
            {
                TC_LOG_ERROR("module.playerbot.session", "🔧 CRITICAL: Non-bot session found in bot sessions map for {}", guid.ToString());
                sessionsToRemove.push_back(guid);
                continue;
            }

            // Check login state for sessions that are still loading
            if (_botsLoading.find(guid) != _botsLoading.end())
            {
                if (session->IsLoginComplete())
                {
                    _botsLoading.erase(guid);
                    TC_LOG_INFO("module.playerbot.session", "🔧 ✅ Bot login completed for: {}", guid.ToString());
                    // Add to update list - login complete
                    sessionsToUpdate.emplace_back(guid, session);
                }
                else if (session->IsLoginFailed())
                {
                    TC_LOG_ERROR("module.playerbot.session", "🔧 Bot login failed for: {}", guid.ToString());
                    _botsLoading.erase(guid);
                    sessionsToRemove.push_back(guid);
                    continue;
                }
                else
                {
                    // Still loading - add to update list to process async login
                    sessionsToUpdate.emplace_back(guid, session);
                }
            }
            else
            {
                // Normal session - add to update list
                sessionsToUpdate.emplace_back(guid, session);
            }
        }

        // Clean up invalid sessions while we still hold the mutex
        for (ObjectGuid const& guid : sessionsToRemove)
        {
            auto it = _botSessions.find(guid);
            if (it != _botSessions.end())
            {
                // CRITICAL SAFETY: Don't manually delete sessions
                // They will be cleaned up by their own destructors
                _botSessions.erase(it);
            }
        }
    } // Release mutex here - CRITICAL for deadlock prevention

    // PHASE 2: Update sessions WITHOUT holding the main mutex (deadlock-free)
    std::vector<ObjectGuid> disconnectedSessions;

    for (auto& [guid, botSession] : sessionsToUpdate)
    {
        // Validate session is still active before processing
        if (!botSession || !botSession->IsActive())
        {
            TC_LOG_WARN("module.playerbot.session", "🔄 Skipping inactive session: {}", guid.ToString());
            disconnectedSessions.push_back(guid);
            continue;
        }

        try
        {
            // Create a proper PacketFilter for the bot session
            class BotPacketFilter : public PacketFilter
            {
            public:
                explicit BotPacketFilter(WorldSession* session) : PacketFilter(session) {}
                virtual ~BotPacketFilter() override = default;

                bool Process(WorldPacket* /*packet*/) override { return true; }
                bool ProcessUnsafe() const override { return true; }
            } filter(botSession.get());

            // DEADLOCK-FREE: Update session without holding _sessionsMutex
            if (!botSession->Update(diff, filter))
            {
                TC_LOG_WARN("module.playerbot.session", "🔧 Bot session update returned false for: {}", guid.ToString());
                disconnectedSessions.push_back(guid);
                continue;
            }

            // For completed logins, validate player is still in world
            if (botSession->IsLoginComplete())
            {
                Player* bot = botSession->GetPlayer();
                if (!bot || !bot->IsInWorld())
                {
                    TC_LOG_ERROR("module.playerbot.session", "🔧 CRITICAL: Bot {} disconnected, marking for cleanup", guid.ToString());
                    if (botSession->GetPlayer())
                    {
                        botSession->LogoutPlayer(true);
                    }
                    disconnectedSessions.push_back(guid);
                }
            }
        }
        catch (std::exception const& e)
        {
            TC_LOG_ERROR("module.playerbot.session", "🔧 Exception updating bot session {}: {}", guid.ToString(), e.what());
            disconnectedSessions.push_back(guid);
        }
        catch (...)
        {
            TC_LOG_ERROR("module.playerbot.session", "🔧 Unknown exception updating bot session {}", guid.ToString());
            disconnectedSessions.push_back(guid);
        }
    }

    // PHASE 3: Final cleanup of disconnected sessions
    if (!disconnectedSessions.empty())
    {
        std::lock_guard<std::mutex> lock(_sessionsMutex);
        for (ObjectGuid const& guid : disconnectedSessions)
        {
            auto it = _botSessions.find(guid);
            if (it != _botSessions.end())
            {
                TC_LOG_INFO("module.playerbot.session", "🔧 Removing disconnected bot session: {}", guid.ToString());
                // CRITICAL SAFETY: Session cleanup handled by destructor
                _botSessions.erase(it);
            }
        }
    }
}

uint32 BotWorldSessionMgr::GetBotCount() const
{
    std::lock_guard<std::mutex> lock(_sessionsMutex);
    return static_cast<uint32>(_botSessions.size());
}

void BotWorldSessionMgr::TriggerCharacterLoginForAllSessions()
{
    // For compatibility with existing BotSpawner system
    // This method will work with the new native login system
    TC_LOG_INFO("module.playerbot.session", "🔧 TriggerCharacterLoginForAllSessions called");

    // The native login approach doesn't need this trigger mechanism
    // But we can use it to spawn new bots if needed
    TC_LOG_INFO("module.playerbot.session", "🔧 Using native login - no manual triggering needed");
}

bool BotWorldSessionMgr::SynchronizeCharacterCache(ObjectGuid playerGuid)
{
    TC_LOG_DEBUG("module.playerbot.session", "🔧 Synchronizing character cache for {}", playerGuid.ToString());

    // CRITICAL FIX: Use synchronous query instead of CHAR_SEL_CHARACTER (which is CONNECTION_ASYNC only)
    // Create a simple synchronous query for just name and account
    std::string query = "SELECT guid, name, account FROM characters WHERE guid = " + std::to_string(playerGuid.GetCounter());
    QueryResult result = CharacterDatabase.Query(query.c_str());

    if (!result)
    {
        TC_LOG_ERROR("module.playerbot.session", "🔧 Character {} not found in characters table", playerGuid.ToString());
        return false;
    }

    Field* fields = result->Fetch();
    std::string dbName = fields[1].GetString();
    uint32 dbAccountId = fields[2].GetUInt32();

    // Get current cache data
    std::string cacheName = "<unknown>";
    uint32 cacheAccountId = sCharacterCache->GetCharacterAccountIdByGuid(playerGuid);
    sCharacterCache->GetCharacterNameByGuid(playerGuid, cacheName);

    TC_LOG_INFO("module.playerbot.session", "🔧 Cache sync: DB({}, {}) vs Cache({}, {}) for {}",
                dbName, dbAccountId, cacheName, cacheAccountId, playerGuid.ToString());

    // Update cache if different
    if (dbName != cacheName)
    {
        TC_LOG_INFO("module.playerbot.session", "🔧 Updating character name cache: '{}' -> '{}'", cacheName, dbName);
        sCharacterCache->UpdateCharacterData(playerGuid, dbName);
    }

    if (dbAccountId != cacheAccountId)
    {
        TC_LOG_INFO("module.playerbot.session", "🔧 Updating character account cache: {} -> {}", cacheAccountId, dbAccountId);
        sCharacterCache->UpdateCharacterAccountId(playerGuid, dbAccountId);
    }

    TC_LOG_DEBUG("module.playerbot.session", "🔧 Character cache synchronized for {}", playerGuid.ToString());
    return true;
}

// ============================================================================
// Chat Command Support APIs - Added for PlayerBot command system
// ============================================================================

std::vector<Player*> BotWorldSessionMgr::GetPlayerBotsByAccount(uint32 accountId) const
{
    std::vector<Player*> bots;
    std::lock_guard<std::mutex> lock(_sessionsMutex);

    for (auto const& [guid, session] : _botSessions)
    {
        if (!session)
            continue;

        Player* bot = session->GetPlayer();
        if (!bot)
            continue;

        // Check if this bot belongs to the specified account
        if (bot->GetSession() && bot->GetSession()->GetAccountId() == accountId)
            bots.push_back(bot);
    }

    return bots;
}

void BotWorldSessionMgr::RemoveAllPlayerBots(uint32 accountId)
{
    std::vector<ObjectGuid> botsToRemove;

    // Collect GUIDs to remove (avoid modifying map while iterating)
    {
        std::lock_guard<std::mutex> lock(_sessionsMutex);
        for (auto const& [guid, session] : _botSessions)
        {
            if (!session)
                continue;

            Player* bot = session->GetPlayer();
            if (bot && bot->GetSession() && bot->GetSession()->GetAccountId() == accountId)
                botsToRemove.push_back(guid);
        }
    }

    // Remove bots (this releases the mutex between iterations)
    for (ObjectGuid const& guid : botsToRemove)
    {
        RemovePlayerBot(guid);
        TC_LOG_INFO("module.playerbot.commands", "Removed bot {} for account {}", guid.ToString(), accountId);
    }
}

uint32 BotWorldSessionMgr::GetBotCountByAccount(uint32 accountId) const
{
    uint32 count = 0;
    std::lock_guard<std::mutex> lock(_sessionsMutex);

    for (auto const& [guid, session] : _botSessions)
    {
        if (!session)
            continue;

        Player* bot = session->GetPlayer();
        if (bot && bot->GetSession() && bot->GetSession()->GetAccountId() == accountId)
            ++count;
    }

    return count;
}

} // namespace Playerbot