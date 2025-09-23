/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#include "BotWorldSessionMgr.h"
#include "WorldSession.h"
#include "Player.h"
#include "World.h"
#include "DatabaseEnv.h"
#include "CharacterCache.h"
#include "ObjectAccessor.h"
#include "Log.h"
#include "ScriptMgr.h"

namespace Playerbot {

/**
 * Custom LoginQueryHolder for bot sessions (mod-playerbots pattern)
 */
class BotLoginQueryHolder : public LoginQueryHolder
{
private:
    uint32 _masterAccountId;
    BotWorldSessionMgr* _sessionMgr;

public:
    BotLoginQueryHolder(BotWorldSessionMgr* sessionMgr, uint32 masterAccountId, uint32 accountId, ObjectGuid guid)
        : LoginQueryHolder(accountId, guid), _masterAccountId(masterAccountId), _sessionMgr(sessionMgr)
    {
    }

    uint32 GetMasterAccountId() const { return _masterAccountId; }
    BotWorldSessionMgr* GetSessionMgr() const { return _sessionMgr; }
};

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

    TC_LOG_INFO("module.playerbot.session", "🔧 Adding bot {} using native TrinityCore login pattern", playerGuid.ToString());

    // Create LoginQueryHolder (mod-playerbots pattern)
    std::shared_ptr<BotLoginQueryHolder> holder =
        std::make_shared<BotLoginQueryHolder>(this, masterAccountId, accountId, playerGuid);

    if (!holder->Initialize())
    {
        TC_LOG_ERROR("module.playerbot.session", "🔧 Failed to initialize LoginQueryHolder for {}", playerGuid.ToString());
        return false;
    }

    // Mark as loading
    _botsLoading.insert(playerGuid);

    // Use TrinityCore's native async query pattern (exactly like mod-playerbots)
    sWorld->AddQueryHolderCallback(CharacterDatabase.DelayQueryHolder(holder))
        .AfterComplete([this, masterAccountId](SQLQueryHolderBase const& holder)
        {
            HandlePlayerBotLoginCallback(holder, masterAccountId);
        });

    TC_LOG_INFO("module.playerbot.session", "🔧 Bot login query scheduled for {}", playerGuid.ToString());
    return true;
}

void BotWorldSessionMgr::HandlePlayerBotLoginCallback(SQLQueryHolderBase const& holder, uint32 masterAccountId)
{
    const BotLoginQueryHolder& botHolder = static_cast<const BotLoginQueryHolder&>(holder);
    ObjectGuid playerGuid = botHolder.GetGuid();
    uint32 botAccountId = botHolder.GetAccountId();

    TC_LOG_INFO("module.playerbot.session", "🔧 Processing bot login callback for {}", playerGuid.ToString());

    // Create real WorldSession (mod-playerbots pattern - no custom subclass!)
    WorldSession* botSession = new WorldSession(botAccountId, "", 0x0, nullptr, SEC_PLAYER,
        EXPANSION_DRAGONFLIGHT, time_t(0), sWorld->GetDefaultDbcLocale(), 0, false, false, 0, true);

    if (!botSession)
    {
        TC_LOG_ERROR("module.playerbot.session", "🔧 Failed to create WorldSession for bot {}", playerGuid.ToString());

        std::lock_guard<std::mutex> lock(_sessionsMutex);
        _botsLoading.erase(playerGuid);
        return;
    }

    // Use TrinityCore's native login method (THE KEY DIFFERENCE!)
    TC_LOG_INFO("module.playerbot.session", "🔧 Calling HandlePlayerLoginFromDB for {}", playerGuid.ToString());
    botSession->HandlePlayerLoginFromDB(static_cast<LoginQueryHolder const&>(botHolder));

    Player* bot = botSession->GetPlayer();
    if (!bot)
    {
        TC_LOG_ERROR("module.playerbot.session", "🔧 Failed to load player from database for {}", playerGuid.ToString());
        botSession->LogoutPlayer(true);
        delete botSession;

        std::lock_guard<std::mutex> lock(_sessionsMutex);
        _botsLoading.erase(playerGuid);
        return;
    }

    TC_LOG_INFO("module.playerbot.session", "🔧 ✅ Bot login successful: {} ({})",
        bot->GetName(), playerGuid.ToString());

    // Store session in our manager
    {
        std::lock_guard<std::mutex> lock(_sessionsMutex);
        _botSessions[playerGuid] = botSession;
        _botsLoading.erase(playerGuid);
    }

    // At this point, TrinityCore has already:
    // 1. Called AddPlayerToMap()
    // 2. Set online=1 in database
    // 3. Added to ObjectAccessor
    // 4. Sent all required packets
    // Everything should work automatically!
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

    // Update all bot sessions (like mod-playerbots)
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
                delete session;
            }
            it = _botSessions.erase(it);
            continue;
        }

        // Process packets for this bot session
        if (session)
        {
            session->Update(diff, PacketFilter());
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

} // namespace Playerbot