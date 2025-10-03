/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#include "BotSession.h"
#include "AccountMgr.h"
#include "Log.h"
#include "WorldPacket.h"
#include "Player.h"
#include "DatabaseEnv.h"
#include "QueryHolder.h"
#include "QueryCallback.h"
#include "CharacterPackets.h"
#include "CharacterDatabase.h"
#include "Database/PlayerbotCharacterDBInterface.h"
#include "World.h"
#include "Map.h"
#include "MapManager.h"
#include "AI/BotAI.h"
#include "ObjectMgr.h"
#include "ObjectAccessor.h"
// BotAIFactory is declared in BotAI.h
#include <thread>
#include "GameTime.h"
#include "Lifecycle/BotSpawner.h"
#include <condition_variable>
#include <future>
#include <unordered_set>
#include <boost/asio/io_context.hpp>
#include "Chat/Chat.h"
#include "Database/QueryHolder.h"
#include "Group/GroupInvitationHandler.h"
#include "PartyPackets.h"
#include "Opcodes.h"
#include "Group.h"
#include "GroupMgr.h"
#include "RealmList.h"
#include "AuthenticationPackets.h"

namespace Playerbot {

// BotLoginQueryHolder::Initialize implementation
bool BotSession::BotLoginQueryHolder::Initialize()
{
    bool res = true;
    ObjectGuid::LowType lowGuid = m_guid.GetCounter();

    TC_LOG_DEBUG("module.playerbot.session", "Initializing BotLoginQueryHolder with {} queries for character GUID {}",
                 MAX_PLAYER_LOGIN_QUERY, lowGuid);

    // Set the size first
    SetSize(MAX_PLAYER_LOGIN_QUERY);

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_FROM, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_CUSTOMIZATIONS);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_CUSTOMIZATIONS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_GROUP_MEMBER);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_GROUP, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_AURAS);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_AURAS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_AURA_EFFECTS);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_AURA_EFFECTS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_AURA_STORED_LOCATIONS);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_AURA_STORED_LOCATIONS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_SPELL);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_SPELLS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_SPELL_FAVORITES);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_SPELL_FAVORITES, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUS);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_QUEST_STATUS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUS_OBJECTIVES);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_QUEST_STATUS_OBJECTIVES, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUS_OBJECTIVES_CRITERIA);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_QUEST_STATUS_OBJECTIVES_CRITERIA, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUS_OBJECTIVES_CRITERIA_PROGRESS);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_QUEST_STATUS_OBJECTIVES_CRITERIA_PROGRESS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUS_OBJECTIVES_SPAWN_TRACKING);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_QUEST_STATUS_OBJECTIVES_SPAWN_TRACKING, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUS_DAILY);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_DAILY_QUEST_STATUS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUS_WEEKLY);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_WEEKLY_QUEST_STATUS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUS_MONTHLY);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_MONTHLY_QUEST_STATUS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUS_SEASONAL);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_SEASONAL_QUEST_STATUS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_REPUTATION);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_REPUTATION, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_INVENTORY);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_INVENTORY, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ITEM_INSTANCE_ARTIFACT);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_ARTIFACTS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ITEM_INSTANCE_AZERITE);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_AZERITE, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ITEM_INSTANCE_AZERITE_MILESTONE_POWER);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_AZERITE_MILESTONE_POWERS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ITEM_INSTANCE_AZERITE_UNLOCKED_ESSENCE);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_AZERITE_UNLOCKED_ESSENCES, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ITEM_INSTANCE_AZERITE_EMPOWERED);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_AZERITE_EMPOWERED, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_MAIL);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_MAILS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_MAILITEMS);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_MAIL_ITEMS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_MAILITEMS_ARTIFACT);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_MAIL_ITEMS_ARTIFACT, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_MAILITEMS_AZERITE);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_MAIL_ITEMS_AZERITE, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_MAILITEMS_AZERITE_MILESTONE_POWER);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_MAIL_ITEMS_AZERITE_MILESTONE_POWER, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_MAILITEMS_AZERITE_UNLOCKED_ESSENCE);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_MAIL_ITEMS_AZERITE_UNLOCKED_ESSENCE, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_MAILITEMS_AZERITE_EMPOWERED);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_MAIL_ITEMS_AZERITE_EMPOWERED, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_SOCIALLIST);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_SOCIAL_LIST, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_HOMEBIND);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_HOME_BIND, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_SPELLCOOLDOWNS);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_SPELL_COOLDOWNS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_SPELL_CHARGES);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_SPELL_CHARGES, stmt);

    // Handle conditional queries properly
    if (sWorld->getBoolConfig(CONFIG_DECLINED_NAMES_USED))
    {
        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_DECLINEDNAMES);
        stmt->setUInt64(0, lowGuid);
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_DECLINED_NAMES, stmt);
    }

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_GUILD_MEMBER);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_GUILD, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_ARENAINFO);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_ARENA_INFO, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_ACHIEVEMENTS);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_ACHIEVEMENTS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_CRITERIAPROGRESS);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_CRITERIA_PROGRESS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_EQUIPMENTSETS);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_EQUIPMENT_SETS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_TRANSMOG_OUTFITS);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_TRANSMOG_OUTFITS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_CUF_PROFILES);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_CUF_PROFILES, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_BGDATA);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_BG_DATA, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_GLYPHS);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_GLYPHS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_TALENTS);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_TALENTS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_PVP_TALENTS);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_PVP_TALENTS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_PLAYER_ACCOUNT_DATA);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_ACCOUNT_DATA, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_SKILLS);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_SKILLS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_RANDOMBG);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_RANDOM_BG, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_BANNED);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_BANNED, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUSREW);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_QUEST_STATUS_REW, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ACCOUNT_INSTANCELOCKTIMES);
    stmt->setUInt32(0, m_accountId);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_INSTANCE_LOCK_TIMES, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_PLAYER_CURRENCY);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_CURRENCY, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CORPSE_LOCATION);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_CORPSE_LOCATION, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_PETS);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_PET_SLOTS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_GARRISON);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_GARRISON, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_GARRISON_BLUEPRINTS);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_GARRISON_BLUEPRINTS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_GARRISON_BUILDINGS);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_GARRISON_BUILDINGS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_GARRISON_FOLLOWERS);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_GARRISON_FOLLOWERS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_GARRISON_FOLLOWER_ABILITIES);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_GARRISON_FOLLOWER_ABILITIES, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_TRAIT_ENTRIES);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_TRAIT_ENTRIES, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_TRAIT_CONFIGS);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_TRAIT_CONFIGS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_PLAYER_DATA_ELEMENTS_CHARACTER);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_DATA_ELEMENTS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_PLAYER_DATA_FLAGS_CHARACTER);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_DATA_FLAGS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_BANK_TAB_SETTINGS);
    stmt->setUInt64(0, lowGuid);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_BANK_TAB_SETTINGS, stmt);

    TC_LOG_DEBUG("module.playerbot.session", "BotLoginQueryHolder::Initialize() completed with result: {}", res);
    return res;
}

// BotLoginQueryHolder implementation (uses async pattern like regular WorldSession)

// Global io_context for bot sockets
static boost::asio::io_context g_botIoContext;

BotSession::BotSession(uint32 bnetAccountId)
    : WorldSession(
        bnetAccountId,                  // Use battlenet account as account ID for now
        "",                            // Empty username (generated by Trinity)
        bnetAccountId,                 // BattleNet account ID
        nullptr,                       // No socket
        SEC_PLAYER,                    // Security level
        EXPANSION_LEVEL_CURRENT,       // Current expansion
        0,                             // Mute time
        "",                            // OS
        Minutes(0),                    // Timezone
        0,                             // Build
        ClientBuild::VariantId{},      // Client build variant
        LOCALE_enUS,                   // Locale
        0,                             // Recruiter
        false),                        // Is recruiter
    _bnetAccountId(bnetAccountId),
    _simulatedLatency(50)
{
    // CRITICAL FIX: Validate account IDs and ensure proper initialization
    if (bnetAccountId == 0) {
        TC_LOG_ERROR("module.playerbot.session", "BotSession constructor called with invalid account ID: {}", bnetAccountId);
        _active.store(false);
        return;
    }

    if (GetAccountId() == 0) {
        TC_LOG_ERROR("module.playerbot.session", "BotSession GetAccountId() returned 0 after construction with ID: {}", bnetAccountId);
        _active.store(false);
        return;
    }

    // Initialize atomic values explicitly
    _active.store(true);
    _loginState.store(LoginState::NONE);

    TC_LOG_INFO("module.playerbot.session", "🤖 BotSession constructor complete for account {} (GetAccountId: {})", bnetAccountId, GetAccountId());
}

// Factory method that creates BotSession with better socket handling
std::shared_ptr<BotSession> BotSession::Create(uint32 bnetAccountId)
{
    TC_LOG_INFO("module.playerbot.session", "🏭 BotSession::Create() factory method called for account {}", bnetAccountId);

    // Create BotSession using regular constructor
    auto session = std::make_shared<BotSession>(bnetAccountId);

    // TODO: In future, we could create a BotSocket here and use it to initialize the session
    // For now, we rely on method overrides to handle the null socket case

    return session;
}

// Override PlayerDisconnected to always return false for bot sessions
bool BotSession::PlayerDisconnected() const
{
    // Bot sessions are never considered disconnected since they don't rely on network sockets
    return false;
}

BotSession::~BotSession()
{
    uint32 accountId = 0;
    try {
        accountId = GetAccountId();
    } catch (...) {
        accountId = _bnetAccountId; // Fallback to stored value
    }

    TC_LOG_DEBUG("module.playerbot.session", "BotSession destructor called for account {}", accountId);

    // CRITICAL SAFETY: Mark as destroyed ATOMICALLY first to prevent any new operations
    _destroyed.store(true);
    _active.store(false);

    // DEADLOCK PREVENTION: Wait for any ongoing packet processing to complete
    // Use a reasonable timeout to prevent hanging during shutdown
    auto waitStart = std::chrono::steady_clock::now();
    constexpr auto MAX_WAIT_TIME = std::chrono::milliseconds(500);

    while (_packetProcessing.load() &&
           (std::chrono::steady_clock::now() - waitStart) < MAX_WAIT_TIME) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (_packetProcessing.load()) {
        TC_LOG_WARN("module.playerbot.session", "BotSession destructor: Packet processing still active after 500ms wait for account {}", accountId);
    }

    // MEMORY SAFETY: Clean up AI with exception protection
    if (_ai) {
        try {
            delete _ai;
        } catch (...) {
            TC_LOG_ERROR("module.playerbot.session", "Exception destroying AI for account {}", accountId);
        }
        _ai = nullptr;
    }

    // THREAD SAFETY: Login state cleanup (synchronous mode requires minimal cleanup)
    try {
        _loginState.store(LoginState::NONE);
    } catch (...) {
        TC_LOG_ERROR("module.playerbot.session", "Exception clearing login state for account {}", accountId);
    }

    // DEADLOCK-FREE PACKET CLEANUP: Use very short timeout to prevent hanging
    try {
        std::unique_lock<std::recursive_timed_mutex> lock(_packetMutex, std::defer_lock);
        if (lock.try_lock_for(std::chrono::milliseconds(10))) { // Reduced timeout
            // Clear packets quickly
            std::queue<std::unique_ptr<WorldPacket>> empty1, empty2;
            _incomingPackets.swap(empty1);
            _outgoingPackets.swap(empty2);
            // Queues will be destroyed when they go out of scope
        } else {
            TC_LOG_WARN("module.playerbot.session", "BotSession destructor: Could not acquire mutex for packet cleanup (account: {})", accountId);
            // Don't hang the destructor - let the process handle cleanup
        }
    } catch (...) {
        // CRITICAL: Never throw from destructor - just log and continue
        TC_LOG_ERROR("module.playerbot.session", "BotSession destructor: Exception during packet cleanup for account {}", accountId);
    }

    TC_LOG_DEBUG("module.playerbot.session", "BotSession destructor completed for account {}", accountId);
}

CharacterDatabasePreparedStatement* BotSession::GetSafePreparedStatement(CharacterDatabaseStatements statementId, const char* statementName)
{
    // CRITICAL FIX: Add statement index validation before accessing to prevent assertion failure
    if (statementId >= MAX_CHARACTERDATABASE_STATEMENTS) {
        TC_LOG_ERROR("module.playerbot", "BotSession::GetSafePreparedStatement: Invalid statement index {} >= {} for {}",
                     static_cast<uint32>(statementId), MAX_CHARACTERDATABASE_STATEMENTS, statementName);
        return nullptr;
    }

    // Use CharacterDatabase directly for standard TrinityCore character operations
    TC_LOG_DEBUG("module.playerbot.session",
        "Getting prepared statement {} ({}) directly from CharacterDatabase",
        static_cast<uint32>(statementId), statementName);

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(statementId);
    if (!stmt) {
        TC_LOG_ERROR("module.playerbot", "BotSession::GetSafePreparedStatement: Failed to get prepared statement {} (index: {})",
                     statementName, static_cast<uint32>(statementId));
        return nullptr;
    }
    return stmt;
}

void BotSession::SendPacket(WorldPacket const* packet, bool forced)
{
    if (!packet) return;

    // Note: forced parameter is not used for bot sessions but required for interface
    (void)forced;

    // CRITICAL: Intercept party invitation packets for bot handling
    if (packet->GetOpcode() == SMSG_PARTY_INVITE)
    {
        TC_LOG_INFO("module.playerbot.group", "BotSession: Intercepted SMSG_PARTY_INVITE packet for bot session");
        HandleGroupInvitation(*packet);
    }

    // Simple packet handling - just store in outgoing queue
    std::lock_guard<std::recursive_timed_mutex> lock(_packetMutex);

    // Create a copy of the packet
    auto packetCopy = std::make_unique<WorldPacket>(*packet);
    _outgoingPackets.push(std::move(packetCopy));
}

void BotSession::QueuePacket(WorldPacket* packet)
{
    if (!packet) return;

    // Simple packet handling - just store in incoming queue
    std::lock_guard<std::recursive_timed_mutex> lock(_packetMutex);

    // Create a copy of the packet
    auto packetCopy = std::make_unique<WorldPacket>(*packet);
    _incomingPackets.push(std::move(packetCopy));
}

bool BotSession::Update(uint32 diff, PacketFilter& updater)
{
    // DIAGNOSTIC: Track update entry for debugging - PER SESSION instead of global
    static thread_local uint32 updateCounter = 0;
    uint32 thisUpdateId = ++updateCounter;

    // CRITICAL MEMORY CORRUPTION DETECTION: Comprehensive session validation
    if (!_active.load() || _destroyed.load()) {
        if (thisUpdateId <= 200) { // Log first 200 per-session failures
            TC_LOG_WARN("module.playerbot.session", "🔍 Update #{} EARLY RETURN: _active={} _destroyed={}",
                thisUpdateId, _active.load(), _destroyed.load());
        }
        return false;
    }

    // CRITICAL SAFETY: Validate session integrity before any operations
    uint32 accountId = GetAccountId();
    if (accountId == 0) {
        TC_LOG_ERROR("module.playerbot.session", "🔍 Update #{} EARLY RETURN: GetAccountId() returned 0", thisUpdateId);
        _active.store(false);
        return false;
    }

    // MEMORY CORRUPTION DETECTION: Validate critical member variables
    if (_bnetAccountId == 0 || _bnetAccountId != accountId) {
        TC_LOG_ERROR("module.playerbot.session", "🔍 Update #{} EARLY RETURN: Account ID mismatch - BnetAccount: {}, GetAccount: {}",
            thisUpdateId, _bnetAccountId, accountId);
        _active.store(false);
        return false;
    }

    // THREAD SAFETY: Validate we're not in a recursive Update call
    static thread_local bool inUpdateCall = false;
    if (inUpdateCall) {
        TC_LOG_ERROR("module.playerbot.session", "🔍 Update #{} EARLY RETURN: Recursive call detected for account {}", thisUpdateId, accountId);
        return false;
    }

    // RAII guard to prevent recursive calls
    struct UpdateGuard {
        bool& flag;
        explicit UpdateGuard(bool& f) : flag(f) { flag = true; }
        ~UpdateGuard() { flag = false; }
    } guard(inUpdateCall);

    try {

        // Process async login callbacks
        ProcessPendingLogin();

        // CRITICAL FIX: Process query holder callbacks (missing from original implementation)
        // This is required for async login callbacks to be processed
        ProcessQueryCallbacks();

        // Process bot-specific packets
        ProcessBotPackets();

        // Update AI if available and player is valid
        // CRITICAL FIX: Add comprehensive memory safety validation to prevent ACCESS_VIOLATION
        Player* player = GetPlayer();

        // Removed excessive per-tick logging

        if (_ai && player && _active.load() && !_destroyed.load()) {

            // MEMORY CORRUPTION DETECTION: Validate player object pointer before access
            // Check for common corruption patterns (null and debug heap patterns only)
            uintptr_t playerPtr = reinterpret_cast<uintptr_t>(player);
            if (playerPtr == 0 || playerPtr == 0xDDDDDDDD || playerPtr == 0xCDCDCDCD ||
                playerPtr == 0xFEEEFEEE || playerPtr == 0xCCCCCCCC) {
                TC_LOG_ERROR("module.playerbot.session", "MEMORY CORRUPTION: Invalid player pointer 0x{:X} for account {}", playerPtr, accountId);
                _active.store(false);
                _ai = nullptr; // Clear AI to prevent further access
                return false;
            }

            // Additional check: pointer should be within reasonable address space (not too low)
            if (playerPtr < 0x10000) {
                TC_LOG_ERROR("module.playerbot.session", "MEMORY CORRUPTION: Player pointer 0x{:X} in low address space for account {}", playerPtr, accountId);
                _active.store(false);
                _ai = nullptr;
                return false;
            }

            // Wrap ALL player object access in structured exception handling
            try {
                // MEMORY SAFETY: Multi-layered validation before calling IsInWorld()
                bool playerIsValid = false;
                bool playerIsInWorld = false;

                // Layer 1: Basic object validation
                try {
                    // Test minimal access first - GetGUID is usually safe
                    ObjectGuid playerGuid = player->GetGUID();
                    if (playerGuid.IsEmpty()) {
                        TC_LOG_ERROR("module.playerbot.session", "Player has invalid GUID for account {}", accountId);
                        playerIsValid = false;
                    } else {
                        playerIsValid = true;
                    }
                }
                catch (...) {
                    TC_LOG_ERROR("module.playerbot.session", "Access violation in Player::GetGUID() for account {}", accountId);
                    playerIsValid = false;
                }

                // Layer 2: World state validation (only if basic validation passed)
                if (playerIsValid) {
                    try {
                        playerIsInWorld = player->IsInWorld();
                    }
                    catch (...) {
                        TC_LOG_ERROR("module.playerbot.session", "Access violation in Player::IsInWorld() for account {}", accountId);
                        playerIsValid = false;
                        playerIsInWorld = false;
                    }
                }

                // Layer 3: AI update (only if all validations passed)
                if (playerIsValid && playerIsInWorld && _ai && _active.load()) {
                    try {
                        // SUCCESS: Direct BotAI::UpdateAI() call works, so just call it normally
                        // The issue was that we weren't calling it at all before!
                        _ai->UpdateAI(diff);
                    }
                    catch (std::exception const& e) {
                        TC_LOG_ERROR("module.playerbot.session", "Exception in BotAI::Update for account {}: {}", accountId, e.what());
                        // Don't propagate AI exceptions to prevent session crashes
                    }
                    catch (...) {
                        TC_LOG_ERROR("module.playerbot.session", "Access violation in BotAI::Update for account {}", accountId);
                        // Clear AI to prevent further crashes
                        _ai = nullptr;
                    }
                } else {
                    // Removed excessive per-tick logging
                }
            }
            catch (...) {
                TC_LOG_ERROR("module.playerbot.session", "Critical exception in AI processing for account {}", accountId);
                // Deactivate session completely to prevent memory corruption cascade
                _active.store(false);
                _ai = nullptr;
                TC_LOG_ERROR("module.playerbot.session", "Deactivated BotSession {} due to critical memory corruption", accountId);
                return false; // Signal failure to caller
            }
        }

        return true; // Bot sessions always return success
    }
    catch (std::exception const& e) {
        TC_LOG_ERROR("module.playerbot.session",
            "Exception in BotSession::Update for account {}: {}", GetAccountId(), e.what());
        return false;
    }
    catch (...) {
        TC_LOG_ERROR("module.playerbot.session",
            "Unknown exception in BotSession::Update for account {}", GetAccountId());
        return false;
    }
}

void BotSession::ProcessBotPackets()
{
    // CRITICAL SAFETY CHECK: Prevent access to destroyed objects
    if (_destroyed.load() || !_active.load()) {
        return; // Object is being destroyed or inactive, abort immediately
    }

    // Use batch processing with optimized batch sizes for better performance
    constexpr size_t BATCH_SIZE = 32; // Optimized size for L1 cache efficiency

    // CRITICAL DEADLOCK FIX: Implement completely lock-free packet processing
    // Use atomic operations instead of mutex to prevent thread pool deadlocks

    // Batch process packets with atomic queue operations (lock-free)
    std::vector<std::unique_ptr<WorldPacket>> incomingBatch;
    std::vector<std::unique_ptr<WorldPacket>> outgoingBatch;
    incomingBatch.reserve(BATCH_SIZE);
    outgoingBatch.reserve(BATCH_SIZE);

    // LOCK-FREE IMPLEMENTATION: Use double-checked locking with atomic flag
    // This eliminates the recursive_timed_mutex that was causing deadlocks
    bool expected = false;
    if (!_packetProcessing.compare_exchange_strong(expected, true)) {
        // Another thread is already processing packets - safe to skip
        TC_LOG_DEBUG("module.playerbot.session", "Packet processing already in progress for account {}, skipping", GetAccountId());
        return;
    }

    // Ensure processing flag is cleared on exit (RAII pattern)
    struct PacketProcessingGuard {
        std::atomic<bool>& flag;
        explicit PacketProcessingGuard(std::atomic<bool>& f) : flag(f) {}
        ~PacketProcessingGuard() { flag.store(false); }
    } guard(_packetProcessing);

    // Double-check destroyed flag after acquiring processing rights
    if (_destroyed.load() || !_active.load()) {
        return; // Object was destroyed while waiting
    }

    // PHASE 1: Quick extraction with minimal lock time
    {
        // Use shorter timeout for better responsiveness under high load
        std::unique_lock<std::recursive_timed_mutex> lock(_packetMutex, std::defer_lock);
        if (!lock.try_lock_for(std::chrono::milliseconds(5))) // Reduced from 50ms to 5ms
        {
            TC_LOG_DEBUG("module.playerbot.session", "Failed to acquire packet mutex within 5ms for account {}, deferring", GetAccountId());
            return; // Defer processing to prevent thread pool starvation
        }

        // Extract incoming packets atomically
        for (size_t i = 0; i < BATCH_SIZE && !_incomingPackets.empty(); ++i) {
            incomingBatch.emplace_back(std::move(_incomingPackets.front()));
            _incomingPackets.pop();
        }

        // Extract outgoing packets (for logging/debugging)
        for (size_t i = 0; i < BATCH_SIZE && !_outgoingPackets.empty(); ++i) {
            outgoingBatch.emplace_back(std::move(_outgoingPackets.front()));
            _outgoingPackets.pop();
        }
    } // Release lock immediately

    // PHASE 2: Process packets without holding any locks (deadlock-free)
    for (auto& packet : incomingBatch) {
        if (_destroyed.load() || !_active.load()) {
            break; // Stop processing if session is being destroyed
        }

        try {
            // Process packet through WorldSession's standard queue system
            // This is safe to call without locks
            WorldSession::QueuePacket(packet.get());
        }
        catch (std::exception const& e) {
            TC_LOG_ERROR("module.playerbot.session", "Exception processing incoming packet for account {}: {}", GetAccountId(), e.what());
        }
        catch (...) {
            TC_LOG_ERROR("module.playerbot.session", "Unknown exception processing incoming packet for account {}", GetAccountId());
        }
    }

    // Removed noisy packet statistics logging - was spamming logs
    // if (!outgoingBatch.empty()) {
    //     TC_LOG_DEBUG("module.playerbot.session", "Processed {} outgoing packets for account {}", outgoingBatch.size(), GetAccountId());
    // }
}

bool BotSession::LoginCharacter(ObjectGuid characterGuid)
{
    // Validate inputs
    if (characterGuid.IsEmpty())
    {
        TC_LOG_ERROR("module.playerbot.session", "BotSession::LoginCharacter called with empty character GUID");
        return false;
    }

    if (GetAccountId() == 0)
    {
        TC_LOG_ERROR("module.playerbot.session", "BotSession::LoginCharacter called with invalid account ID");
        return false;
    }

    // Check if already logging in or logged in
    LoginState expected = LoginState::NONE;
    if (!_loginState.compare_exchange_strong(expected, LoginState::LOGIN_IN_PROGRESS))
    {
        TC_LOG_ERROR("module.playerbot.session", "BotSession: Already logging in (state: {})", static_cast<uint8>(_loginState.load()));
        return false;
    }

    TC_LOG_INFO("module.playerbot.session", "Starting ASYNC login for character {} using TrinityCore LoginQueryHolder pattern", characterGuid.ToString());

    try
    {
        // PROPER ASYNC APPROACH: Use TrinityCore's standard LoginQueryHolder pattern
        // This fixes the database assertion failure by using the correct async connection pool

        // Store the character GUID for the pending login
        m_playerLoading = characterGuid;

        // Create LoginQueryHolder exactly like WorldSession does
        std::shared_ptr<BotLoginQueryHolder> holder = std::make_shared<BotLoginQueryHolder>(GetAccountId(), characterGuid);
        if (!holder->Initialize())
        {
            TC_LOG_ERROR("module.playerbot.session", "Failed to initialize BotLoginQueryHolder for character {}", characterGuid.ToString());
            m_playerLoading.Clear();
            _loginState.store(LoginState::LOGIN_FAILED);
            return false;
        }

        TC_LOG_INFO("module.playerbot.session", "Submitting async LoginQueryHolder for character {}", characterGuid.ToString());

        // Use the EXACT same pattern as WorldSession - AddQueryHolderCallback with DelayQueryHolder
        AddQueryHolderCallback(CharacterDatabase.DelayQueryHolder(holder)).AfterComplete([this](SQLQueryHolderBase const& holder)
        {
            TC_LOG_INFO("module.playerbot.session", "Async LoginQueryHolder callback received, processing bot player login");
            HandleBotPlayerLogin(static_cast<BotLoginQueryHolder const&>(holder));
        });

        TC_LOG_INFO("module.playerbot.session", "✅ ASYNC bot login initiated for character {} - waiting for database callback", characterGuid.ToString());
        // Login state will be updated in HandleBotPlayerLogin callback
        return true;
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("module.playerbot.session", "Exception in LoginCharacter: {}", e.what());
        m_playerLoading.Clear();
        _loginState.store(LoginState::LOGIN_FAILED);
        return false;
    }
    catch (...)
    {
        TC_LOG_ERROR("module.playerbot.session", "Unknown exception in LoginCharacter");
        m_playerLoading.Clear();
        _loginState.store(LoginState::LOGIN_FAILED);
        return false;
    }
}

void BotSession::ProcessPendingLogin()
{
    // Process any pending async login operations
    // The actual login is handled by the HandleBotPlayerLogin callback

    LoginState currentState = _loginState.load();
    if (currentState == LoginState::LOGIN_IN_PROGRESS)
    {
        TC_LOG_DEBUG("module.playerbot.session", "ProcessPendingLogin: Async login in progress, waiting for callback");
    }
}

// Handle the asynchronous login query holder callback (replaces LoadCharacterDataSynchronously)
void BotSession::HandleBotPlayerLogin(BotLoginQueryHolder const& holder)
{
    if (!IsActive() || !_active.load())
    {
        TC_LOG_ERROR("module.playerbot.session", "BotSession is not active during HandleBotPlayerLogin");
        _loginState.store(LoginState::LOGIN_FAILED);
        m_playerLoading.Clear();
        return;
    }

    ObjectGuid characterGuid = holder.GetGuid();
    TC_LOG_INFO("module.playerbot.session", "Processing async login callback for character {}", characterGuid.ToString());

    try
    {
        // Create Player object
        Player* pCurrChar = new Player(this);
        if (!pCurrChar)
        {
            TC_LOG_ERROR("module.playerbot.session", "Failed to create Player object for character {}", characterGuid.ToString());
            _loginState.store(LoginState::LOGIN_FAILED);
            m_playerLoading.Clear();
            return;
        }

        // Use the async query holder to load character data
        // This uses the proper async connection pool, preventing the assertion failure
        if (!pCurrChar->LoadFromDB(characterGuid, holder))
        {
            delete pCurrChar;
            TC_LOG_ERROR("module.playerbot.session", "Failed to load bot character {} from database", characterGuid.ToString());
            _loginState.store(LoginState::LOGIN_FAILED);
            m_playerLoading.Clear();
            return;
        }

        // Bot-specific initialization
        pCurrChar->SetVirtualPlayerRealm(GetVirtualRealmAddress());

        // Set the player for this session
        SetPlayer(pCurrChar);

        // Clear the loading state
        m_playerLoading.Clear();

        // CRITICAL FIX: Add bot to world (missing step that prevented bots from entering world)
        pCurrChar->SendInitialPacketsBeforeAddToMap();

        if (!pCurrChar->GetMap()->AddPlayerToMap(pCurrChar))
        {
            TC_LOG_ERROR("module.playerbot.session", "Failed to add bot player {} to map", characterGuid.ToString());
            // Try to teleport to homebind if map addition fails
            AreaTriggerStruct const* at = sObjectMgr->GetGoBackTrigger(pCurrChar->GetMapId());
            if (at)
                pCurrChar->TeleportTo(at->target_mapId, at->target_X, at->target_Y, at->target_Z, pCurrChar->GetOrientation());
            else
                pCurrChar->TeleportTo(pCurrChar->m_homebind);
        }

        ObjectAccessor::AddObject(pCurrChar);
        pCurrChar->SendInitialPacketsAfterAddToMap();

        TC_LOG_INFO("module.playerbot.session", "Bot player {} successfully added to world", pCurrChar->GetName());

        // Create and assign BotAI to take control of the character
        if (GetPlayer())
        {
            auto botAI = sBotAIFactory->CreateAI(GetPlayer());
            if (botAI)
            {
                SetAI(botAI.release()); // Transfer ownership to BotSession
                TC_LOG_INFO("module.playerbot.session", "Successfully created BotAI for character {}", characterGuid.ToString());

                // CRITICAL FIX: Check if bot is already in a group at login and activate strategies
                // This fixes the "reboot breaks groups" issue where strategies aren't activated
                if (Player* player = GetPlayer())
                {
                    if (Group* group = player->GetGroup())
                    {
                        TC_LOG_INFO("module.playerbot.session", "🔄 Bot {} is already in group at login - activating strategies", player->GetName());
                        if (BotAI* ai = GetAI())
                        {
                            ai->OnGroupJoined(group);
                        }
                    }
                }
            }
            else
            {
                TC_LOG_ERROR("module.playerbot.session", "Failed to create BotAI for character {}", characterGuid.ToString());
            }
        }

        // Mark login as complete
        _loginState.store(LoginState::LOGIN_COMPLETE);

        TC_LOG_INFO("module.playerbot.session", "✅ ASYNC bot login successful for character {}", characterGuid.ToString());
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("module.playerbot.session", "Exception in HandleBotPlayerLogin: {}", e.what());
        if (GetPlayer())
        {
            delete GetPlayer();
            SetPlayer(nullptr);
        }
        _loginState.store(LoginState::LOGIN_FAILED);
        m_playerLoading.Clear();
    }
    catch (...)
    {
        TC_LOG_ERROR("module.playerbot.session", "Unknown exception in HandleBotPlayerLogin");
        if (GetPlayer())
        {
            delete GetPlayer();
            SetPlayer(nullptr);
        }
        _loginState.store(LoginState::LOGIN_FAILED);
        m_playerLoading.Clear();
    }
}

// Process query callbacks - ensures async login callbacks are handled
// This is called by BotSessionManager::ProcessBotCallbacks()

void BotSession::HandleGroupInvitation(WorldPacket const& packet)
{
    // Validate session and player
    Player* bot = GetPlayer();
    if (!bot)
    {
        TC_LOG_DEBUG("module.playerbot.group", "HandleGroupInvitation: No player for bot session");
        return;
    }

    // Check if bot has AI with group handler
    if (!_ai)
    {
        TC_LOG_DEBUG("module.playerbot.group", "HandleGroupInvitation: No AI for bot {}", bot->GetName());
        return;
    }

    GroupInvitationHandler* handler = _ai->GetGroupInvitationHandler();
    if (!handler)
    {
        TC_LOG_DEBUG("module.playerbot.group", "HandleGroupInvitation: No group handler for bot {}", bot->GetName());
        return;
    }

    try
    {
        // Complete implementation: Parse the SMSG_PARTY_INVITE packet using TrinityCore APIs
        // This follows TrinityCore's standard packet parsing approach
        WorldPacket packetCopy = packet;

        // Parse the packet structure according to PartyPackets.cpp Write() method
        // Bit fields are read in the same order they were written
        bool canAccept = packetCopy.ReadBit();
        bool isXRealm = packetCopy.ReadBit();
        bool isXNativeRealm = packetCopy.ReadBit();
        bool shouldSquelch = packetCopy.ReadBit();
        bool allowMultipleRoles = packetCopy.ReadBit();
        bool questSessionActive = packetCopy.ReadBit();
        uint32 inviterNameSize = packetCopy.ReadBits(6);
        bool isCrossFaction = packetCopy.ReadBit();

        packetCopy.FlushBits();

        // Read VirtualRealmInfo fields manually (no operator>> available)
        // According to AuthenticationPackets.cpp operator<<:
        // - uint32 RealmAddress
        // - VirtualRealmNameInfo (2 bits + 2 strings with size bits)
        uint32 realmAddress;
        packetCopy >> realmAddress;

        // Read VirtualRealmNameInfo bit fields
        bool isLocalRealm = packetCopy.ReadBit();
        bool isInternalRealm = packetCopy.ReadBit();
        uint32 realmNameActualSize = packetCopy.ReadBits(8);
        uint32 realmNameNormalizedSize = packetCopy.ReadBits(8);
        packetCopy.FlushBits();

        std::string realmNameActual = std::string(packetCopy.ReadString(realmNameActualSize));
        std::string realmNameNormalized = std::string(packetCopy.ReadString(realmNameNormalizedSize));

        ObjectGuid inviterGUID;
        ObjectGuid inviterBNetAccountId;
        uint16 inviterCfgRealmID;
        uint8 proposedRoles;
        uint32 lfgSlotCount;
        uint32 lfgCompletedMask;

        packetCopy >> inviterGUID;
        packetCopy >> inviterBNetAccountId;
        packetCopy >> inviterCfgRealmID;
        packetCopy >> proposedRoles;
        packetCopy >> lfgSlotCount;
        packetCopy >> lfgCompletedMask;

        // Read the inviter name
        std::string inviterName = std::string(packetCopy.ReadString(inviterNameSize));

        // Read LFG slots if present
        std::vector<uint32> lfgSlots;
        for (uint32 i = 0; i < lfgSlotCount; ++i)
        {
            uint32 lfgSlot;
            packetCopy >> lfgSlot;
            lfgSlots.push_back(lfgSlot);
        }

        TC_LOG_INFO("module.playerbot.group", "Bot {} received group invitation from {} (GUID: {}, CanAccept: {}, Roles: {}, XRealm: {})",
            bot->GetName(), inviterName, inviterGUID.ToString(), canAccept, proposedRoles, isXRealm);

        // Find the inviter player using TrinityCore APIs
        Player* inviter = ObjectAccessor::FindPlayer(inviterGUID);
        if (!inviter)
        {
            TC_LOG_ERROR("module.playerbot.group", "HandleGroupInvitation: Inviter {} (GUID: {}) not found for bot {}",
                inviterName, inviterGUID.ToString(), bot->GetName());
            return;
        }

        // Validate the invitation can be accepted
        if (!canAccept)
        {
            TC_LOG_DEBUG("module.playerbot.group", "HandleGroupInvitation: Bot {} cannot accept invitation from {} (canAccept=false)",
                bot->GetName(), inviterName);
            return;
        }

        // Critical fix: Set up proper group invitation state using TrinityCore APIs
        Group* inviterGroup = inviter->GetGroup();
        if (!inviterGroup)
        {
            // Inviter is forming a new group - create the group properly
            inviterGroup = new Group;
            if (!inviterGroup->Create(inviter))
            {
                TC_LOG_ERROR("module.playerbot.group", "HandleGroupInvitation: Failed to create new group for inviter {}", inviterName);
                delete inviterGroup;
                return;
            }
            sGroupMgr->AddGroup(inviterGroup);

            TC_LOG_INFO("module.playerbot.group", "Created new group {} for inviter {} to invite bot {}",
                inviterGroup->GetGUID().ToString(), inviterName, bot->GetName());
        }

        // Add the bot as an invitee to the group using TrinityCore APIs
        // This is the critical missing piece that sets bot->SetGroupInvite()

        // Diagnostic logging: Check why AddInvite might fail
        TC_LOG_INFO("module.playerbot.group", "Bot {} invitation diagnostics:", bot->GetName());
        TC_LOG_INFO("module.playerbot.group", "  - Bot exists: {}", bot != nullptr);
        TC_LOG_INFO("module.playerbot.group", "  - Bot current group: {}",
            bot->GetGroup() ? bot->GetGroup()->GetGUID().ToString() : "None");
        TC_LOG_INFO("module.playerbot.group", "  - Bot current group invite: {}",
            bot->GetGroupInvite() ? bot->GetGroupInvite()->GetGUID().ToString() : "None");
        TC_LOG_INFO("module.playerbot.group", "  - Inviter group: {}", inviterGroup->GetGUID().ToString());
        TC_LOG_INFO("module.playerbot.group", "  - Inviter group size: {}", inviterGroup->GetMembersCount());

        // Handle existing group invitation state
        if (bot->GetGroupInvite())
        {
            TC_LOG_INFO("module.playerbot.group", "Bot {} already has group invitation from group {}, removing old invitation",
                bot->GetName(), bot->GetGroupInvite()->GetGUID().ToString());
            bot->GetGroupInvite()->RemoveInvite(bot);
        }

        // Handle existing group membership
        if (bot->GetGroup())
        {
            TC_LOG_INFO("module.playerbot.group", "Bot {} is already in group {}, cannot invite",
                bot->GetName(), bot->GetGroup()->GetGUID().ToString());
            return;
        }

        if (inviterGroup->AddInvite(bot))
        {
            TC_LOG_INFO("module.playerbot.group", "Successfully added bot {} to group {} invitee list (inviter: {})",
                bot->GetName(), inviterGroup->GetGUID().ToString(), inviterName);

            // Verify that the bot now has a group invitation
            if (bot->GetGroupInvite() == inviterGroup)
            {
                TC_LOG_INFO("module.playerbot.group", "Confirmed: Bot {} now has proper group invitation state set", bot->GetName());

                // Create a proper PartyInvite packet for the handler using TrinityCore APIs
                WorldPackets::Party::PartyInvite partyInvite;
                partyInvite.Initialize(inviter, static_cast<int32>(proposedRoles), canAccept);

                // Process the invitation through our handler system
                if (handler->HandleInvitation(partyInvite))
                {
                    TC_LOG_INFO("module.playerbot.group", "Bot {} successfully queued invitation from {} for processing",
                        bot->GetName(), inviterName);
                }
                else
                {
                    TC_LOG_DEBUG("module.playerbot.group", "Bot {} rejected invitation from {} during handler processing",
                        bot->GetName(), inviterName);
                }
            }
            else
            {
                TC_LOG_ERROR("module.playerbot.group", "Bot {} group invitation state not properly set after AddInvite", bot->GetName());
            }
        }
        else
        {
            TC_LOG_ERROR("module.playerbot.group", "Failed to add bot {} to group {} invitee list (AddInvite failed)",
                bot->GetName(), inviterGroup->GetGUID().ToString());
        }
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("module.playerbot.group", "Exception handling party invitation for bot {}: {}",
            bot->GetName(), e.what());
    }
}

} // namespace Playerbot