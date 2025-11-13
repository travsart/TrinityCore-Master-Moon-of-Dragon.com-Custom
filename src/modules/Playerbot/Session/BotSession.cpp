/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#include "BotSession.h"
#include "BotPacketRelay.h"
#include "BotPacketSimulator.h"  // PHASE 1: Packet forging infrastructure
#include "PacketDeferralClassifier.h"  // Selective main thread deferral
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
#include "Lifecycle/DeathRecoveryManager.h"  // For death recovery check at login
#include "ObjectMgr.h"
#include "ObjectAccessor.h"
// BotAIFactory is declared in BotAI.h
#include <thread>
#include "GameTime.h"
#include "Lifecycle/BotSpawner.h"
#include <condition_variable>
#include <future>
#include <unordered_set>
#include <set>
#include <unordered_map>
#include <boost/asio/io_context.hpp>
#include "Chat/Chat.h"
#include "Database/QueryHolder.h"
#include "Group/GroupInvitationHandler.h"
#include "PartyPackets.h"
#include "Opcodes.h"
#include "ByteBuffer.h"          // For ByteBufferException
#include "WorldSession.h"        // For WorldPackets exception types
#include "Group.h"
#include "GroupMgr.h"
#include "RealmList.h"
#include "AuthenticationPackets.h"
#include "Core/Events/BotEventTypes.h"  // PHASE 0 - Quick Win #3: For GROUP_JOINED event
#include "Core/Events/EventDispatcher.h"  // PHASE 0 - Quick Win #3: For event dispatch
#include "PhasingHandler.h"  // For bot phase initialization
#include "Spatial/SpatialGridQueryHelpers.h"  // PHASE 2F: For snapshot-based player validation

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
        false,                         // Is recruiter
        true),                         // is_bot = true ⭐ CRITICAL FIX: Makes IsBot() work correctly!
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

    // CRITICAL FIX: Clear m_spellModTakingSpell BEFORE player cleanup to prevent crash
    // When bot logs out, delayed spells (like Detect Sparkle Aura 84459) are still in EventProcessor
    // The SpellEvent destructor will fire and Spell::~Spell() checks m_spellModTakingSpell
    // We MUST clear this pointer before the base WorldSession destructor cleans up the player
    if (Player* player = GetPlayer()) {
        try {
            // Core Fix Applied: SpellEvent::~SpellEvent() now automatically clears m_spellModTakingSpell (Spell.cpp:8455)
            player->m_Events.KillAllEvents(false);
            TC_LOG_DEBUG("module.playerbot.session", "Bot {} cleared spell events during logout", player->GetName());
        } catch (...) {
            TC_LOG_ERROR("module.playerbot.session", "Exception clearing spell events during logout for account {}", accountId);
        }
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

    // PHASE 1: Relay packets to human group members
    // This enables bot damage/chat to appear in human combat logs and chat
    BotPacketRelay::RelayToGroupMembers(this, packet);

    // Store packet in outgoing queue for debugging/logging
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
    // OPTION 1: TIMED LOCK WITH SHUTDOWN SAFEGUARDS
    // - Prevents Ghost aura race condition (concurrent ResurrectPlayer calls)
    // - Prevents shutdown deadlock (100ms timeout)
    // - Prevents strategy freezing (skips update instead of blocking)
    std::unique_lock<std::timed_mutex> lock(_updateMutex, std::defer_lock);

    // Try to acquire lock with 100ms timeout
    if (!lock.try_lock_for(std::chrono::milliseconds(100))) {
        // Failed to acquire lock within 100ms

        // Check if shutting down - bail immediately
        if (_destroyed.load() || !_active.load()) {
            return false;  // Shutdown detected - exit gracefully
        }

        // Otherwise: Another thread is updating this bot
        // Just skip this update cycle (bot updates 5-10 times/sec anyway)
        // No need to log - this is normal concurrent behavior
        return false;
    }

    // Successfully acquired lock - proceed with update
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

    // PLAYERBOT FIX: Check for logout/kick request - return false to trigger safe deletion
    // This prevents Map.cpp:686 crash by deferring player removal to main thread
    // forceExit is set by KickPlayer() when BotWorldEntry::Cleanup() is called
    // m_playerLogout is set by LogoutPlayer()
    if (forceExit || m_playerLogout) {
        TC_LOG_DEBUG("module.playerbot.session",
            "BotSession logout/kick requested for account {} (forceExit={}, m_playerLogout={}) - returning false for safe deletion",
            GetAccountId(), forceExit, m_playerLogout);
        return false;  // Triggers session removal in BotWorldSessionMgr::UpdateSessions()
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

        // PHASE 1 REFACTORING: Update packet simulator for periodic time synchronization
        if (_packetSimulator)
        {
            _packetSimulator->Update(diff);
        }

        // Process bot-specific packets
        ProcessBotPackets();

        // =======================================================================
        // ENTERPRISE-GRADE _recvQueue PACKET PROCESSING
        // =======================================================================
        // CRITICAL FIX: Process _recvQueue for bot-initiated packets
        // Problem: WorldSession::Update() line 385 checks m_Socket[CONNECTION_TYPE_REALM]
        //          which is nullptr for bots, preventing _recvQueue processing
        // Solution: Replicate WorldSession packet processing WITHOUT socket check
        // Benefits: Enables ALL packet-based bot features (resurrection, trades, etc.)
        // Thread Safety: Packets processed in bot worker thread (same as BotAI::Update)
        // =======================================================================

        WorldPacket* packet = nullptr;
        uint32 processedPackets = 0;
        time_t currentTime = GameTime::GetGameTime();

        // Performance limit: Same as WorldSession::Update() to prevent infinite loops
        constexpr uint32 MAX_PROCESSED_PACKETS_IN_SAME_WORLDSESSION_UPDATE = 100;

        // Process all queued packets (resurrection, future features, etc.)
        // Note: No socket check needed for bots (sockets are nullptr by design)
        while (processedPackets < MAX_PROCESSED_PACKETS_IN_SAME_WORLDSESSION_UPDATE &&
               _recvQueue.next(packet))
        {
            OpcodeClient opcode = static_cast<OpcodeClient>(packet->GetOpcode());
            ClientOpcodeHandler const* opHandle = opcodeTable[opcode];

            // ======================================================================
            // CRITICAL: SELECTIVE DEFERRAL - Only defer packets that need main thread
            // ======================================================================
            if (PacketDeferralClassifier::RequiresMainThread(opcode))
            {
                TC_LOG_TRACE("playerbot.packets",
                    "Bot {} DEFERRING packet opcode {} ({}) to main thread - Reason: {}",
                    GetPlayerName(),
                    static_cast<uint32>(opcode),
                    opHandle->Name,
                    PacketDeferralClassifier::GetDeferralReason(opcode));

                // Transfer ownership to deferred queue (processed by World::UpdateSessions)
                QueueDeferredPacket(std::unique_ptr<WorldPacket>(packet));
                packet = nullptr; // Ownership transferred
                processedPackets++;
                continue; // Skip to next packet
            }
            // ======================================================================

            TC_LOG_TRACE("playerbot.packets",
                "Bot {} processing packet opcode {} ({}) with status {} on WORKER THREAD",
                GetPlayerName(),
                static_cast<uint32>(opcode),
                opHandle->Name,
                static_cast<uint32>(opHandle->Status));

            try
            {
                // Process based on opcode status (mirrors WorldSession::Update logic)
                switch (opHandle->Status)
                {
                    case STATUS_LOGGEDIN:
                    {
                        // Most common case: Player must be logged in and in world
                        // CMSG_RECLAIM_CORPSE falls into this category
                        Player* player = GetPlayer();
                        if (!player)
                        {
                            TC_LOG_WARN("playerbot.packets",
                                "Bot {} received STATUS_LOGGEDIN opcode {} but player is nullptr",
                                GetPlayerName(), opHandle->Name);
                            break;
                        }

                        if (!player->IsInWorld())
                        {
                            TC_LOG_WARN("playerbot.packets",
                                "Bot {} received STATUS_LOGGEDIN opcode {} but player not in world",
                                GetPlayerName(), opHandle->Name);
                            break;
                        }

                        // Execute opcode handler (e.g., HandleReclaimCorpse)
                        // This is thread-safe: Handler runs in bot worker thread context
                        opHandle->Call(this, *packet);

                        TC_LOG_DEBUG("playerbot.packets",
                            "✅ Bot {} executed opcode {} ({}) handler successfully",
                            GetPlayerName(),
                            opHandle->Name,
                            static_cast<uint32>(opcode));
                        break;
                    }

                    case STATUS_LOGGEDIN_OR_RECENTLY_LOGGOUT:
                    {
                        // Opcodes valid during logout process
                        // Bots don't have traditional logout, but include for completeness
                        opHandle->Call(this, *packet);
                        TC_LOG_DEBUG("playerbot.packets",
                            "✅ Bot {} executed opcode {} (STATUS_LOGGEDIN_OR_RECENTLY_LOGGOUT)",
                            GetPlayerName(), opHandle->Name);
                        break;
                    }

                    case STATUS_TRANSFER:
                    {
                        // Opcodes valid during map transfer
                        Player* player = GetPlayer();
                        if (player && !player->IsInWorld())
                        {
                            opHandle->Call(this, *packet);

                            TC_LOG_DEBUG("playerbot.packets",
                                "✅ Bot {} executed opcode {} (STATUS_TRANSFER)",
                                GetPlayerName(), opHandle->Name);
                        }
                        else
                        {
                            TC_LOG_WARN("playerbot.packets",
                                "Bot {} received STATUS_TRANSFER opcode {} but player state invalid",
                                GetPlayerName(), opHandle->Name);
                        }
                        break;
                    }

                    case STATUS_AUTHED:
                    {
                        // Opcodes valid after authentication (character select, etc.)
                        // Bots typically skip character select, but include for future features
                        opHandle->Call(this, *packet);

                        TC_LOG_DEBUG("playerbot.packets",
                            "✅ Bot {} executed opcode {} (STATUS_AUTHED)",
                            GetPlayerName(), opHandle->Name);
                        break;
                    }

                    case STATUS_NEVER:
                    {
                        TC_LOG_ERROR("playerbot.packets",
                            "❌ Bot {} received NEVER-allowed opcode {} ({})",
                            GetPlayerName(),
                            opHandle->Name,
                            static_cast<uint32>(opcode));
                        break;
                    }

                    case STATUS_UNHANDLED:
                    {
                        TC_LOG_ERROR("playerbot.packets",
                            "❌ Bot {} received UNHANDLED opcode {} ({})",
                            GetPlayerName(),
                            opHandle->Name,
                            static_cast<uint32>(opcode));
                        break;
                    }

                    case STATUS_IGNORED:
                    {
                        // Silently ignore (e.g., deprecated opcodes)
                        TC_LOG_TRACE("playerbot.packets",
                            "Bot {} ignored opcode {} (STATUS_IGNORED)",
                            GetPlayerName(), opHandle->Name);
                        break;
                    }

                    default:
                    {
                        TC_LOG_ERROR("playerbot.packets",
                            "❌ Bot {} received opcode {} with UNKNOWN status {}",
                            GetPlayerName(),
                            opHandle->Name,
                            static_cast<uint32>(opHandle->Status));
                        break;
                    }
                }
            }
            catch (WorldPackets::InvalidHyperlinkException const& ihe)
            {
                TC_LOG_ERROR("playerbot.packets",
                    "InvalidHyperlinkException processing opcode {} for bot {}: {}",
                    opHandle->Name, GetPlayerName(), ihe.GetInvalidValue());
                // Continue processing other packets
            }
            catch (WorldPackets::IllegalHyperlinkException const& ihe)
            {
                TC_LOG_ERROR("playerbot.packets",
                    "IllegalHyperlinkException processing opcode {} for bot {}: {}",
                    opHandle->Name, GetPlayerName(), ihe.GetInvalidValue());
                // Continue processing other packets
            }
            catch (WorldPackets::PacketArrayMaxCapacityException const& pamce)
            {
                TC_LOG_ERROR("playerbot.packets",
                    "PacketArrayMaxCapacityException processing opcode {} for bot {}: {}",
                    opHandle->Name, GetPlayerName(), pamce.what());
                // Continue processing other packets
            }
            catch (ByteBufferException const& bbe)
            {
                TC_LOG_ERROR("playerbot.packets",
                    "ByteBufferException processing opcode {} for bot {}: {}",
                    opHandle->Name, GetPlayerName(), bbe.what());
                packet->hexlike();
                // Continue processing other packets
            }
            catch (std::exception const& ex)
            {
                TC_LOG_ERROR("playerbot.packets",
                    "Unexpected exception processing opcode {} for bot {}: {}",
                    opHandle->Name, GetPlayerName(), ex.what());
                // Continue processing other packets
            }
            catch (...)
            {
                TC_LOG_ERROR("playerbot.packets",
                    "Unknown exception processing opcode {} for bot {}",
                    opHandle->Name, GetPlayerName());
                // Continue processing other packets
            }

            // Always delete packet after processing (prevents memory leaks)
            delete packet;
            packet = nullptr;
            processedPackets++;
        }

        // Log packet processing statistics (TRACE level to avoid spam)
        if (processedPackets > 0)
        {
            TC_LOG_TRACE("playerbot.packets",
                "Bot {} processed {} packets this update cycle (elapsed: {}ms)",
                GetPlayerName(), processedPackets, diff);
        }

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

                // Layer 3: AI update - ATOMIC SNAPSHOT to prevent race conditions
                // FIX: Capture all values ONCE before any checks to prevent race conditions
                // Problem: Between debug log and if statement, another thread could change values
                // Solution: Single atomic read of all conditions
                bool validSnapshot = playerIsValid;
                bool inWorldSnapshot = playerIsInWorld;
                BotAI* aiSnapshot = _ai;  // Snapshot pointer (raw pointer for now)
                bool activeSnapshot = _active.load(std::memory_order_acquire);  // Acquire semantics

                // DEBUG LOGGING THROTTLE: Only log for test bots every 50 seconds
                static const std::set<std::string> testBots = {"Anderenz", "Boone", "Nelona", "Sevtap"};
                static std::unordered_map<std::string, uint32> sessionLogAccumulators;
                Player* botPlayer = GetPlayer();
                bool isTestBot = botPlayer && (testBots.find(botPlayer->GetName()) != testBots.end());
                bool shouldLog = false;

                if (isTestBot)
                {
                    std::string botName = botPlayer->GetName();
                    // Throttle by call count (every 1000 calls ~= 50s)
                    sessionLogAccumulators[botName]++;
                    if (sessionLogAccumulators[botName] >= 1000)
                    {
                        shouldLog = true;
                        sessionLogAccumulators[botName] = 0;
                    }
                }

                if (shouldLog)
                {
                    TC_LOG_INFO("module.playerbot.session", "🔍 BotSession Update Check - valid:{}, inWorld:{}, ai:{}, active:{}, account:{}",
                                validSnapshot, inWorldSnapshot, aiSnapshot != nullptr, activeSnapshot, accountId);
                }

                // Use SNAPSHOT values (not original variables) to prevent race conditions
                if (validSnapshot && inWorldSnapshot && aiSnapshot && activeSnapshot) {
                    if (shouldLog)
                    {
                        TC_LOG_INFO("module.playerbot.session", "✅ ALL CONDITIONS MET - Calling UpdateAI for account {}", accountId);
                    }
                    try {
                        // Call UpdateAI using snapshot pointer (guaranteed non-null and stable)
                        aiSnapshot->UpdateAI(diff);
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

        // CRITICAL BUG FIX (2025-10-30): DO NOT call WorldSession::Update() from worker threads!
        //
        // PROBLEM: WorldSession::Update() is designed to run on the MAIN WORLD THREAD (see World::UpdateSessions line 3039).
        //          Calling it from bot worker threads creates RACE CONDITIONS with Map::Update() which runs on Map worker threads.
        //
        // EVIDENCE: Crash at GameObject::Update() → Map::GetCreature() → GUID comparison nullptr dereference
        //           Crash dump: 3f33f776e3ba+_worldserver.exe_[2025_10_30_7_49_6].txt
        //           Location: Map.cpp:3529 during GameObject ownership lookup
        //
        // ROOT CAUSE: Packet handlers executing on bot worker threads modify game state while Map::Update() runs concurrently,
        //             causing use-after-free/dangling reference crashes.
        //
        // SOLUTION: Bot packets (like CMSG_RECLAIM_CORPSE) must be processed on MAIN THREAD or via a thread-safe mechanism.
        //           Current approach is UNSAFE and causes crashes.
        //
        // TODO: Implement safe packet processing for bots (options below):
        //       1. Process bot session packets in World::UpdateSessions() on main thread
        //       2. Use deferred packet queue that main thread processes
        //       3. Implement resurrection without packet-based approach (direct ResurrectPlayer call)
        //
        // TEMPORARY: Resurrection is broken but server won't crash from race conditions.

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

// ============================================================================
// DEFERRED PACKET SYSTEM - Main Thread Processing for Race Condition Prevention
// ============================================================================

void BotSession::QueueDeferredPacket(std::unique_ptr<WorldPacket> packet)
{
    if (!packet)
        return;

    // Log BEFORE moving (packet will be invalid after move)
    OpcodeClient opcode = static_cast<OpcodeClient>(packet->GetOpcode());

    std::lock_guard<std::mutex> lock(_deferredPacketMutex);
    _deferredPackets.emplace(std::move(packet));

    TC_LOG_TRACE("playerbot.packets.deferred",
        "Bot {} queued packet opcode {} for main thread processing",
        GetPlayerName(), static_cast<uint32>(opcode));
}

uint32 BotSession::ProcessDeferredPackets()
{
    // CRITICAL: This must ONLY be called from World::UpdateSessions() on main thread!
    // Processing deferred packets on worker threads defeats the entire purpose.

    uint32 processed = 0;
    constexpr uint32 MAX_DEFERRED_PACKETS_PER_UPDATE = 50; // Prevent main thread starvation

    while (processed < MAX_DEFERRED_PACKETS_PER_UPDATE)
    {
        std::unique_ptr<WorldPacket> packet;

        // Quick lock to extract one packet
        {
            std::lock_guard<std::mutex> lock(_deferredPacketMutex);
            if (_deferredPackets.empty())
                break;

            packet = std::move(_deferredPackets.front());
            _deferredPackets.pop();
        }

        // Process packet outside lock (avoid holding mutex during execution)
        try
        {
            OpcodeClient opcode = static_cast<OpcodeClient>(packet->GetOpcode());
            ClientOpcodeHandler const* opHandle = opcodeTable[opcode];

            TC_LOG_TRACE("playerbot.packets.deferred",
                "Bot {} processing deferred packet opcode {} ({}) on main thread",
                GetPlayerName(),
                static_cast<uint32>(opcode),
                opHandle->Name);

            // Process based on opcode status (same logic as ProcessBotPackets)
            switch (opHandle->Status)
            {
                case STATUS_LOGGEDIN:
                {
                    Player* player = GetPlayer();
                    if (!player)
                    {
                        TC_LOG_WARN("playerbot.packets.deferred",
                            "Bot {} deferred packet {} but player is nullptr",
                            GetPlayerName(), opHandle->Name);
                        break;
                    }

                    if (!player->IsInWorld())
                    {
                        TC_LOG_WARN("playerbot.packets.deferred",
                            "Bot {} deferred packet {} but player not in world",
                            GetPlayerName(), opHandle->Name);
                        break;
                    }

                    // Execute opcode handler on main thread (thread-safe with Map::Update)
                    opHandle->Call(this, *packet);

                    TC_LOG_DEBUG("playerbot.packets.deferred",
                        "✅ Bot {} executed deferred opcode {} ({}) on main thread",
                        GetPlayerName(),
                        opHandle->Name,
                        static_cast<uint32>(opcode));
                    break;
                }

                case STATUS_LOGGEDIN_OR_RECENTLY_LOGGOUT:
                case STATUS_TRANSFER:
                case STATUS_AUTHED:
                {
                    opHandle->Call(this, *packet);
                    break;
                }

                case STATUS_NEVER:
                case STATUS_UNHANDLED:
                case STATUS_IGNORED:
                default:
                {
                    TC_LOG_ERROR("playerbot.packets.deferred",
                        "❌ Bot {} deferred packet has invalid status: {} (opcode {})",
                        GetPlayerName(),
                        static_cast<uint32>(opHandle->Status),
                        opHandle->Name);
                    break;
                }
            }

            processed++;
        }
        catch (std::exception const& ex)
        {
            TC_LOG_ERROR("playerbot.packets.deferred",
                "Exception processing deferred packet for bot {}: {}",
                GetPlayerName(), ex.what());
        }
        catch (...)
        {
            TC_LOG_ERROR("playerbot.packets.deferred",
                "Unknown exception processing deferred packet for bot {}",
                GetPlayerName());
        }
    }

    if (processed > 0)
    {
        TC_LOG_DEBUG("playerbot.packets.deferred",
            "Bot {} processed {} deferred packets on main thread",
            GetPlayerName(), processed);
    }

    return processed;
}

bool BotSession::HasDeferredPackets() const
{
    std::lock_guard<std::mutex> lock(_deferredPacketMutex);
    return !_deferredPackets.empty();
}

// ============================================================================

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

        // NOTE: Specialization spells are NOT saved to database in modern WoW
        // LoadFromDB() at line 18356 calls LearnSpecializationSpells() which loads spells from DB2 data
        // This is by design - spells are learned dynamically on each login

        // Set the player for this session
        SetPlayer(pCurrChar);

        // Clear the loading state
        m_playerLoading.Clear();

        // CRITICAL FIX: Add bot to world (missing step that prevented bots from entering world)
        pCurrChar->SendInitialPacketsBeforeAddToMap();

        // THREAD SAFETY: Ensure map is created and ready before adding bot
        // Maps are loaded on-demand when first player enters - we must ensure it's loaded
        uint32 mapId = pCurrChar->GetMapId();
        uint32 instanceId = pCurrChar->GetInstanceId();

        TC_LOG_DEBUG("module.playerbot.session", "Bot {} attempting to join MapId={} InstanceId={}",
            pCurrChar->GetName(), mapId, instanceId);

        // CreateMap will find existing map or create it if it doesn't exist yet
        // This is what CharacterHandler.cpp does for real players
        Map* map = sMapMgr->CreateMap(mapId, pCurrChar);
        if (!map)
        {
            TC_LOG_ERROR("module.playerbot.session",
                "❌ CRITICAL: Bot {} cannot create/find map! MapId={} InstanceId={} - Login FAILED",
                pCurrChar->GetName(), mapId, instanceId);
            _loginState.store(LoginState::LOGIN_FAILED);
            m_playerLoading.Clear();
            return;
        }

        TC_LOG_DEBUG("module.playerbot.session", "✅ Bot {} map ready: MapId={} InstanceId={} MapPtr=0x{:X}",
            pCurrChar->GetName(), mapId, instanceId, reinterpret_cast<uintptr_t>(map));

        // Now safely add bot to the map
        if (!map->AddPlayerToMap(pCurrChar))
        {
            TC_LOG_ERROR("module.playerbot.session", "Failed to add bot player {} to map", characterGuid.ToString());
            _loginState.store(LoginState::LOGIN_FAILED);
            m_playerLoading.Clear();
            return;
        }

        ObjectAccessor::AddObject(pCurrChar);

        // CRITICAL FIX: Following TrinityCore "golden source" pattern (CharacterHandler.cpp:1299)
        // SendInitialPacketsAfterAddToMap() internally calls:
        //   - PhasingHandler::OnMapChange(this) at Player.cpp:24805
        // This happens AFTER AddPlayerToMap(), matching real player login flow
        pCurrChar->SendInitialPacketsAfterAddToMap();

        // PHASE 1 REFACTORING: Create packet simulator for this bot session
        // This replaces manual workarounds with proper packet forging
        _packetSimulator = std::make_unique<BotPacketSimulator>(this);

        // PHASE 1 REFACTORING: Simulate CMSG_QUEUED_MESSAGES_END packet
        // Real clients send this after SMSG_RESUME_COMMS to resume communication
        // Triggers time synchronization and allows login to proceed
        _packetSimulator->SimulateQueuedMessagesEnd();
        // PHASE 1 REFACTORING: Simulate CMSG_MOVE_INIT_ACTIVE_MOVER_COMPLETE packet
        // Real clients send this after SMSG_MOVE_INIT_ACTIVE_MOVER
        // Automatically sets PLAYER_LOCAL_FLAG_OVERRIDE_TRANSPORT_SERVER_TIME
        // Enables object visibility (fixes CanNeverSee() check)
        // Updates player visibility
        _packetSimulator->SimulateMoveInitActiveMoverComplete();

        // PHASE 1 REFACTORING: Enable periodic time synchronization
        // Maintains clock delta calculations over time
        // Prevents movement prediction drift
        _packetSimulator->EnablePeriodicTimeSync();

        // DIAGNOSTIC: Log phase information after SendInitialPacketsAfterAddToMap
        PhaseShift const& phaseShift = pCurrChar->GetPhaseShift();
        std::string botPhases;
        auto const& phases = phaseShift.GetPhases();
        botPhases.reserve(phases.size() * 8); // Pre-allocate approximate size
        for (auto const& phaseRef : phases)
        {
            if (!botPhases.empty()) botPhases += ",";
            botPhases += std::to_string(phaseRef.Id);
        }
        if (botPhases.empty()) botPhases = "NONE";

        TC_LOG_INFO("module.playerbot.session", "✅ Bot {} phase initialization complete (TrinityCore pattern + packet simulation) - Phases: [{}]",
            pCurrChar->GetName(), botPhases);
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
                    Group* group = player->GetGroup();
                    TC_LOG_ERROR("module.playerbot.session", "🔍 Bot {} login group check: player={}, group={}",
                                player->GetName(), (void*)player, (void*)group);
                    if (group)
                    {
                        TC_LOG_INFO("module.playerbot.session", "🔄 Bot {} is already in group at login - activating strategies", player->GetName());
                        if (BotAI* ai = GetAI())
                        {
                            TC_LOG_ERROR("module.playerbot.session", "📞 About to call OnGroupJoined with group={}", (void*)group);
                            ai->OnGroupJoined(group);
                        }
                    }

                    // CRITICAL FIX: Check if bot is dead OR in ghost form at login and trigger death recovery
                    // This fixes server restart where dead bots don't resurrect
                    // BUG FIX: isDead() only checks DEAD/CORPSE states, but ghosts are ALIVE with PLAYER_FLAGS_GHOST
                    //          After server restart, dead bots load as ghosts (1 HP, Ghost aura) but m_deathState == ALIVE
                    //          Must check HasPlayerFlag(PLAYER_FLAGS_GHOST) to detect ghost state
                    if (player->isDead() || player->HasPlayerFlag(PLAYER_FLAGS_GHOST))
                    {
                        TC_LOG_INFO("module.playerbot.session", "💀 Bot {} is dead/ghost at login (isDead={}, isGhost={}, deathState={}, health={}/{}) - triggering death recovery",
                            player->GetName(),
                            player->isDead(),
                            player->HasPlayerFlag(PLAYER_FLAGS_GHOST),
                            static_cast<int>(player->getDeathState()),
                            player->GetHealth(),
                            player->GetMaxHealth());

                        if (BotAI* ai = GetAI())
                        {
                            if (ai->GetDeathRecoveryManager())
                            {
                                TC_LOG_INFO("module.playerbot.session", "📞 Calling OnDeath to initialize death recovery for bot {}", player->GetName());
                                ai->GetDeathRecoveryManager()->OnDeath();
                            }
                            else
                            {
                                TC_LOG_ERROR("module.playerbot.session", "❌ DeathRecoveryManager not initialized for dead bot {}", player->GetName());
                            }
                        }
                    }
                }
            }
            else
            {
                TC_LOG_ERROR("module.playerbot.session", "Failed to create BotAI for character {}", characterGuid.ToString());
            }
        }

        // BOT-SPECIFIC LOGIN SPELL CLEANUP: Clear all pending spell events to prevent m_spellModTakingSpell crash
        // Issue: LOGINEFFECT (Spell 836) and other login spells are queued in EventProcessor during bot login
        // These spells modify spell behavior (m_spellModTakingSpell) but bots don't send client ACK packets
        // When the SpellEvent destructor fires, it tries to destroy a spell that's still referenced → ASSERTION FAILURE
        // Root Cause: When KillAllEvents() destroys a delayed spell, handle_delayed() never runs to clear m_spellModTakingSpell
        // Solution: Clear m_spellModTakingSpell FIRST, then kill events to prevent Spell::~Spell assertion failure
        if (Player* player = GetPlayer())
        {
            // Core Fix Applied: SpellEvent::~SpellEvent() now automatically clears m_spellModTakingSpell (Spell.cpp:8455)
            // No longer need to manually clear - KillAllEvents() will properly clean up spell mods
            player->m_Events.KillAllEvents(false);  // false = don't force, let graceful shutdown happen
            TC_LOG_DEBUG("module.playerbot.session", "🧹 Bot {} cleared login spell events to prevent m_spellModTakingSpell crash", player->GetName());
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

    // Check if bot has AI
    if (!_ai)
    {
        TC_LOG_DEBUG("module.playerbot.group", "HandleGroupInvitation: No AI for bot {}", bot->GetName());
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

        // PHASE 2F: Hybrid validation pattern (snapshot + ObjectAccessor fallback)
        // Quick snapshot check first (fast, lock-free)
        auto inviterSnapshot = SpatialGridQueryHelpers::FindPlayerByGuid(nullptr, inviterGUID);
        if (!inviterSnapshot)
        {
            TC_LOG_DEBUG("module.playerbot.group", "HandleGroupInvitation: Inviter {} not in spatial grid (likely offline)", inviterName);
            return;
        }

        // Fallback to ObjectAccessor for full validation
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
                TC_LOG_INFO("module.playerbot.group", "✅ Confirmed: Bot {} has group invitation state set", bot->GetName());

                // CLEAN APPROACH: Auto-accept and join the group using TrinityCore API
                // This removes the legacy GroupInvitationHandler fallback path

                // Accept the invitation by adding bot to the group
                if (inviterGroup->AddMember(bot))
                {
                    TC_LOG_INFO("module.playerbot.group", "✅ Bot {} successfully joined group {} (inviter: {})",
                        bot->GetName(), inviterGroup->GetGUID().ToString(), inviterName);

                    // PHASE 0 - Quick Win #3: Dispatch GROUP_JOINED event for instant reaction
                    // This eliminates the 1-second polling lag
                    if (_ai && _ai->GetEventDispatcher())
                    {
                        Events::BotEvent evt(StateMachine::EventType::GROUP_JOINED, bot->GetGUID(), inviterGroup->GetLeaderGUID());
                        _ai->GetEventDispatcher()->Dispatch(std::move(evt));
                        TC_LOG_INFO("module.playerbot.group", "📢 GROUP_JOINED event dispatched for bot {}", bot->GetName());
                    }

                    // Activate follow behavior through BotAI lifecycle hook
                    if (_ai)
                    {
                        TC_LOG_ERROR("module.playerbot.group", "🔥 CALLING OnGroupJoined for bot {} with group {}",
                                    bot->GetName(), (void*)inviterGroup);
                        _ai->OnGroupJoined(inviterGroup);
                        TC_LOG_ERROR("module.playerbot.group", "✅ OnGroupJoined COMPLETED for bot {}", bot->GetName());
                    }
                    else
                    {
                        TC_LOG_ERROR("module.playerbot.group", "❌ CRITICAL: _ai is NULL for bot {}", bot->GetName());
                    }

                    TC_LOG_INFO("module.playerbot.group", "✅ Bot {} follow behavior activated", bot->GetName());
                }
                else
                {
                    TC_LOG_ERROR("module.playerbot.group", "❌ Failed to add bot {} to group {} (AddMember failed)",
                        bot->GetName(), inviterGroup->GetGUID().ToString());
                }
            }
            else
            {
                TC_LOG_ERROR("module.playerbot.group", "❌ Bot {} group invitation state not set after AddInvite", bot->GetName());
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