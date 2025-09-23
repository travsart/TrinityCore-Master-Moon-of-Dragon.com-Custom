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
            TC_LOG_INFO("module.playerbot.session", "🔧 Logging out bot: {}", session->GetPlayer()->GetName());
            session->LogoutPlayer(true);
        }
        delete session;
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

    // Check if bot is already added
    Player* existingBot = ObjectAccessor::FindConnectedPlayer(playerGuid);
    if (existingBot && existingBot->IsInWorld())
    {
        TC_LOG_DEBUG("module.playerbot.session", "🔧 Bot {} already in world", playerGuid.ToString());
        return false;
    }

    // Get account ID for this character
    uint32 accountId = sCharacterCache->GetCharacterAccountIdByGuid(playerGuid);
    if (!accountId)
    {
        TC_LOG_ERROR("module.playerbot.session", "🔧 Could not find account for character {}", playerGuid.ToString());
        return false;
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

    // Store the session (cast to WorldSession* for compatibility)
    _botSessions[playerGuid] = botSession.get();

    // Use the existing BotSession login character mechanism
    if (!botSession->LoginCharacter(playerGuid))
    {
        TC_LOG_ERROR("module.playerbot.session", "🔧 Failed to login character {}", playerGuid.ToString());
        _botSessions.erase(playerGuid);
        _botsLoading.erase(playerGuid);
        return false;
    }

    _botsLoading.erase(playerGuid);
    TC_LOG_INFO("module.playerbot.session", "🔧 ✅ Bot login successful using proven BotSession: {}", playerGuid.ToString());
    return true;
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

    WorldSession* session = it->second;
    if (session && session->GetPlayer())
    {
        TC_LOG_INFO("module.playerbot.session", "🔧 Removing bot: {}", session->GetPlayer()->GetName());
        session->LogoutPlayer(true);
    }

    delete session;
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

    std::lock_guard<std::mutex> lock(_sessionsMutex);

    // Update all bot sessions using their natural Update method
    for (auto it = _botSessions.begin(); it != _botSessions.end();)
    {
        WorldSession* session = it->second;
        Player* bot = session ? session->GetPlayer() : nullptr;

        if (!bot || !bot->IsInWorld())
        {
            // Clean up disconnected bot
            if (session)
            {
                session->LogoutPlayer(true);
                // Don't delete - BotSession manages its own lifetime via shared_ptr
            }
            it = _botSessions.erase(it);
            continue;
        }

        // BotSession handles its own update mechanism
        if (session)
        {
            // Cast back to BotSession to use its update method with PacketFilter
            BotSession* botSession = static_cast<BotSession*>(session);

            // Create a proper PacketFilter for the bot session
            class BotPacketFilter : public PacketFilter
            {
            public:
                explicit BotPacketFilter(WorldSession* session) : PacketFilter(session) {}
                virtual ~BotPacketFilter() override = default;

                bool Process(WorldPacket* /*packet*/) override { return true; }
                bool ProcessUnsafe() const override { return true; }
            } filter(session);

            botSession->Update(diff, filter);
        }

        ++it;
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

} // namespace Playerbot