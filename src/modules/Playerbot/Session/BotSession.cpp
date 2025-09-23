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
#include "ObjectAccessor.h"
#include "GameTime.h"
#include <condition_variable>
#include <unordered_set>
#include <boost/asio/io_context.hpp>

// BotLoginQueryHolder implementation moved to header

namespace Playerbot {

// Simple forward declaration - no complex socket implementation needed

// Global io_context for bot sockets
static boost::asio::io_context g_botIoContext;

// Implementation of BotLoginQueryHolder::Initialize() following TrinityCore patterns
bool BotLoginQueryHolder::Initialize()
{
    // EXACT TRINITYCORE IMPLEMENTATION: Copy TrinityCore LoginQueryHolder::Initialize() line by line
    SetSize(MAX_PLAYER_LOGIN_QUERY);

    bool res = true;
    ObjectGuid::LowType lowGuid = m_guid.GetCounter();

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

    TC_LOG_INFO("module.playerbot.session", "🔧 BotLoginQueryHolder EXACT TrinityCore implementation with {} queries for character {}", MAX_PLAYER_LOGIN_QUERY, m_guid.ToString());

    return res;
}

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
    // Validate account IDs
    if (GetAccountId() == 0) {
        return;
    }

    TC_LOG_INFO("module.playerbot.session", "🤖 BotSession constructor complete for account {}", bnetAccountId);
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
    // Clean up AI if present
    if (_ai) {
        delete _ai;
        _ai = nullptr;
    }
}

CharacterDatabasePreparedStatement* BotSession::GetSafePreparedStatement(CharacterDatabaseStatements statementId, const char* statementName)
{
    // CRITICAL FIX: Add statement index validation before accessing to prevent assertion failure
    if (statementId >= MAX_CHARACTERDATABASE_STATEMENTS) {
        TC_LOG_ERROR("module.playerbot", "BotSession::GetSafePreparedStatement: Invalid statement index {} >= {} for {}",
                     static_cast<uint32>(statementId), MAX_CHARACTERDATABASE_STATEMENTS, statementName);
        return nullptr;
    }

    // Use PlayerbotCharacterDBInterface for safe statement access with automatic routing
    TC_LOG_DEBUG("module.playerbot.session",
        "Accessing statement {} ({}) through PlayerbotCharacterDBInterface",
        static_cast<uint32>(statementId), statementName);

    CharacterDatabasePreparedStatement* stmt = sPlayerbotCharDB->GetPreparedStatement(statementId);
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

    // Simple packet handling - just store in outgoing queue
    std::lock_guard<std::mutex> lock(_packetMutex);

    // Create a copy of the packet
    auto packetCopy = std::make_unique<WorldPacket>(*packet);
    _outgoingPackets.push(std::move(packetCopy));
}

void BotSession::QueuePacket(WorldPacket* packet)
{
    if (!packet) return;

    // Simple packet handling - just store in incoming queue
    std::lock_guard<std::mutex> lock(_packetMutex);

    // Create a copy of the packet
    auto packetCopy = std::make_unique<WorldPacket>(*packet);
    _incomingPackets.push(std::move(packetCopy));
}

bool BotSession::Update(uint32 diff, PacketFilter& updater)
{
    if (!_active.load()) {
        return false;
    }

    try {
        TC_LOG_DEBUG("module.playerbot.session", "BotSession::Update processing callbacks and AI for account {}", GetAccountId());

        // SAFE APPROACH: Skip dangerous parent Update, implement our own callback processing
        // The parent WorldSession::Update() contains socket operations that cause ACCESS_VIOLATION
        // Instead, we'll process essential functionality manually

        // Process bot-specific packets
        ProcessBotPackets();

        // CRITICAL: Process async login callbacks if needed
        // This is the key functionality we need for async login completion
        if (IsAsyncLoginInProgress()) {
            TC_LOG_DEBUG("module.playerbot.session",
                "Bot session {} has async login in progress, attempting callback processing", GetAccountId());

            // Call our safe callback processing method
            ProcessBotQueryCallbacks();
        }

        // Update AI if available and player is valid
        Player* player = GetPlayer();
        if (_ai && player && player->IsInWorld()) {
            _ai->Update(diff);
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
    // Use batch processing with larger batch sizes for better performance under load
    constexpr size_t BATCH_SIZE = 50; // Increased from 10 to reduce overhead

    // Batch process packets outside lock to minimize critical sections
    std::vector<std::unique_ptr<WorldPacket>> incomingBatch;
    incomingBatch.reserve(BATCH_SIZE);

    // Extract batch of incoming packets under lock
    {
        std::lock_guard<std::mutex> lock(_packetMutex);

        // Reserve space and extract up to BATCH_SIZE packets
        size_t batchCount = std::min(_incomingPackets.size(), BATCH_SIZE);
        for (size_t i = 0; i < batchCount; ++i) {
            incomingBatch.emplace_back(std::move(_incomingPackets.front()));
            _incomingPackets.pop();
        }

        // Clear outgoing packets (simulate processing) - also batched
        size_t clearCount = std::min(_outgoingPackets.size(), BATCH_SIZE);
        for (size_t i = 0; i < clearCount; ++i) {
            _outgoingPackets.pop();
        }
    }

    // Process the extracted batch outside of lock for better concurrency
    for (auto& packet : incomingBatch) {
        try {
            // Process packet through WorldSession's standard queue system
            WorldSession::QueuePacket(packet.get());
        }
        catch (std::exception const& e) {
            // Log exception through ModuleLogManager
        }
    }
}

bool BotSession::LoginCharacter(ObjectGuid characterGuid)
{
    // Validate inputs
    if (characterGuid.IsEmpty())
    {
        return false;
    }

    if (GetAccountId() == 0)
    {
        return false;
    }

    try
    {
        // Create player and load directly from database (use smart pointer for safety)
        std::unique_ptr<Player> newPlayer = std::make_unique<Player>(this);
        if (!newPlayer)
        {
            return false;
        }

        // CRITICAL FIX: Use async pattern for scalability - avoid blocking main thread
        // For synchronous login, we'll use the existing approach but note this should be async for 5000+ bots
        // The StartAsyncLogin method provides the proper async implementation for high scalability

        // Use prepared statement for security - avoid SQL injection vulnerabilities
        CharacterDatabasePreparedStatement* stmt = GetSafePreparedStatement(CHAR_SEL_CHARACTER, "CHAR_SEL_CHARACTER");
        if (!stmt) {
            return false;
        }
        stmt->setUInt64(0, characterGuid.GetCounter());

        // Use PlayerbotCharacterDBInterface for safe execution with automatic routing
        PreparedQueryResult accountResult = sPlayerbotCharDB->ExecuteSync(stmt);

        if (!accountResult)
        {
            // unique_ptr automatically cleans up newPlayer
            return false;
        }

        Field* accountFields = accountResult->Fetch();
        uint32 characterAccountId = accountFields[0].GetUInt32(); // Account is the only field returned

        // Create LoginQueryHolder with character's REAL account ID (this is the key fix!)
        auto botHolder = std::make_shared<BotLoginQueryHolder>(characterAccountId, characterGuid);
        if (!botHolder->Initialize())
        {
            // unique_ptr automatically cleans up newPlayer
            return false;
        }

        // Load character data using CORRECT account ID
        bool loadResult = newPlayer->LoadFromDB(characterGuid, *botHolder);

        if (!loadResult)
        {
            // unique_ptr automatically cleans up newPlayer
            return false;
        }

        // Set the player for this session (transfer ownership from unique_ptr to session)
        SetPlayer(newPlayer.release());

        // Set character as online
        CharacterDatabasePreparedStatement* onlineStmt = GetSafePreparedStatement(CHAR_UPD_CHAR_ONLINE, "CHAR_UPD_CHAR_ONLINE");
        if (!onlineStmt) {
            return false;
        }
        onlineStmt->setUInt32(0, 1);
        onlineStmt->setUInt64(1, characterGuid.GetCounter());
        sPlayerbotCharDB->ExecuteAsync(onlineStmt);

        // Create and assign BotAI to take control of the character
        auto botAI = BotAIFactory::instance()->CreateAI(GetPlayer());
        if (botAI)
        {
            SetAI(botAI.release()); // Transfer ownership to BotSession
        }

        return true;
    }
    catch (std::exception const& e)
    {
        return false;
    }
    catch (...)
    {
        return false;
    }
}

void BotSession::StartAsyncLogin(ObjectGuid characterGuid)
{
    TC_LOG_INFO("module.playerbot.session", "🔑 StartAsyncLogin called for character {}", characterGuid.ToString());

    // Check if async login already in progress (thread-safe)
    {
        std::lock_guard<std::mutex> lock(_asyncLogin.mutex);
        if (_asyncLogin.inProgress)
        {
            TC_LOG_WARN("module.playerbot.session", "🔑 Async login already in progress for {}", characterGuid.ToString());
            return;
        }
        // Set async login state under lock
        _asyncLogin.characterGuid = characterGuid;
        _asyncLogin.inProgress = true;
        _asyncLogin.startTime = std::chrono::steady_clock::now();
        _asyncLogin.player = nullptr;
    }

    // Validate inputs
    if (characterGuid.IsEmpty())
    {
        TC_LOG_ERROR("module.playerbot.session", "🔑 Empty character GUID provided to StartAsyncLogin");
        {
            std::lock_guard<std::mutex> lock(_asyncLogin.mutex);
            _asyncLogin.inProgress = false;
        }
        return;
    }

    try
    {
        // PHASE 1: Create BotLoginQueryHolder using the TrinityCore pattern
        TC_LOG_INFO("module.playerbot.session", "🔑 Using session account ID: {}", GetAccountId());
        auto holder = std::make_shared<BotLoginQueryHolder>(GetAccountId(), characterGuid);

        if (!holder->Initialize())
        {
            TC_LOG_ERROR("module.playerbot.session", "🔑 Failed to initialize BotLoginQueryHolder");
            {
                std::lock_guard<std::mutex> lock(_asyncLogin.mutex);
                _asyncLogin.inProgress = false;
            }
            return;
        }

        // PHASE 2: Execute query holder through TrinityCore's async system
        TC_LOG_INFO("module.playerbot.session", "🔑 Executing LoginQueryHolder async...");

        // Use the standard TrinityCore pattern: CharacterDatabase.DelayQueryHolder()
        // Cast to base class for DelayQueryHolder call
        std::shared_ptr<CharacterDatabaseQueryHolder> baseHolder = std::static_pointer_cast<CharacterDatabaseQueryHolder>(holder);

        TC_LOG_INFO("module.playerbot.session", "🔧 About to call CharacterDatabase.DelayQueryHolder for character {}", characterGuid.ToString());

        // Use TrinityCore's standard pattern - directly call CharacterDatabase.DelayQueryHolder
        auto& queryHolderCallback = AddQueryHolderCallback(CharacterDatabase.DelayQueryHolder(baseHolder));
        TC_LOG_INFO("module.playerbot.session", "🔧 QueryHolderCallback added to session processor");

        queryHolderCallback.AfterComplete([this, characterGuid](SQLQueryHolderBase const& holder)
        {
            TC_LOG_INFO("module.playerbot.session", "🎯 ASYNC CALLBACK EXECUTED! HandlePlayerLogin callback executing for character {}", characterGuid.ToString());
            HandlePlayerLogin(static_cast<BotLoginQueryHolder const&>(holder));
        });

        TC_LOG_INFO("module.playerbot.session", "🔧 AfterComplete callback registered for character {}", characterGuid.ToString());

    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("module.playerbot.session", "🔑 StartAsyncLogin exception: {}", e.what());
        {
            std::lock_guard<std::mutex> lock(_asyncLogin.mutex);
            _asyncLogin.inProgress = false;
        }
    }
    catch (...)
    {
        TC_LOG_ERROR("module.playerbot.session", "🔑 StartAsyncLogin unknown exception");
        {
            std::lock_guard<std::mutex> lock(_asyncLogin.mutex);
            _asyncLogin.inProgress = false;
        }
    }
}

void BotSession::HandlePlayerLogin(BotLoginQueryHolder const& holder)
{
    ObjectGuid playerGuid = holder.GetGuid();
    TC_LOG_INFO("module.playerbot.session", "🔑 HandlePlayerLogin called for character {}", playerGuid.ToString());

    // Create player (like in CharacterHandler.cpp:1154)
    Player* pCurrChar = new Player(this);
    if (!pCurrChar)
    {
        TC_LOG_ERROR("module.playerbot.session", "🔑 Failed to create Player instance");
        {
            std::lock_guard<std::mutex> lock(_asyncLogin.mutex);
            _asyncLogin.inProgress = false;
        }
        return;
    }

    // Load player from database using QueryHolder (like in CharacterHandler.cpp:1159)
    if (!pCurrChar->LoadFromDB(playerGuid, holder))
    {
        TC_LOG_ERROR("module.playerbot.session", "🔑 Player::LoadFromDB failed for {}", playerGuid.ToString());
        delete pCurrChar;
        {
            std::lock_guard<std::mutex> lock(_asyncLogin.mutex);
            _asyncLogin.inProgress = false;
        }
        return;
    }

    TC_LOG_INFO("module.playerbot.session", "🔑 Player::LoadFromDB successful, calling CompleteAsyncLogin");

    // Store player pointer for CompleteAsyncLogin
    {
        std::lock_guard<std::mutex> lock(_asyncLogin.mutex);
        _asyncLogin.player = pCurrChar;
    }

    // Complete the async login process
    CompleteAsyncLogin(pCurrChar, playerGuid);
}

void BotSession::CompleteAsyncLogin(Player* player, ObjectGuid characterGuid)
{
    TC_LOG_INFO("module.playerbot.session", "🎯 CompleteAsyncLogin called for character {}", characterGuid.ToString());

    // Thread-safe check of async login state
    {
        std::lock_guard<std::mutex> lock(_asyncLogin.mutex);
        if (!player || !_asyncLogin.inProgress)
        {
            TC_LOG_ERROR("module.playerbot.session", "🎯 CompleteAsyncLogin failed: player={}, inProgress={}",
                player ? "valid" : "null", _asyncLogin.inProgress);
            return;
        }
    }

    try
    {
        // Calculate async login time for performance monitoring (thread-safe access)
        auto endTime = std::chrono::steady_clock::now();
        std::chrono::milliseconds duration;
        {
            std::lock_guard<std::mutex> lock(_asyncLogin.mutex);
            duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - _asyncLogin.startTime);
        }

        // Set the player for this session
        TC_LOG_INFO("module.playerbot.session", "🎯 Setting player for session...");
        SetPlayer(player);

        // CRITICAL: Follow TrinityCore's exact player login pattern
        // This matches WorldSession::HandlePlayerLogin exactly

        // Send initial packets before adding to map (like TrinityCore)
        TC_LOG_INFO("module.playerbot.session", "🎯 Sending initial packets before map...");
        player->SendInitialPacketsBeforeAddToMap();

        // Add player to map (THE CRITICAL STEP)
        TC_LOG_INFO("module.playerbot.session", "🎯 Adding player to map...");
        if (!player->GetMap()->AddPlayerToMap(player))
        {
            TC_LOG_ERROR("module.playerbot.session", "🎯 Failed to add player to map");
            {
                std::lock_guard<std::mutex> lock(_asyncLogin.mutex);
                _asyncLogin.inProgress = false;
            }
            return;
        }

        // Add to object accessor (makes player visible in world)
        TC_LOG_INFO("module.playerbot.session", "🎯 Adding to ObjectAccessor...");
        ObjectAccessor::AddObject(player);

        // Send final packets after adding to map (like TrinityCore)
        TC_LOG_INFO("module.playerbot.session", "🎯 Sending initial packets after map...");
        player->SendInitialPacketsAfterAddToMap();

        // Set character as online in database (TrinityCore pattern)
        TC_LOG_INFO("module.playerbot.session", "🎯 Setting character as online in database...");
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHAR_ONLINE);
        stmt->setUInt64(0, characterGuid.GetCounter());
        CharacterDatabase.Execute(stmt);

        // Set account as online in login database (TrinityCore pattern)
        LoginDatabasePreparedStatement* loginStmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_ACCOUNT_ONLINE);
        loginStmt->setUInt32(0, GetAccountId());
        LoginDatabase.Execute(loginStmt);

        // Create and assign BotAI for character control
        TC_LOG_INFO("module.playerbot.session", "🎯 Creating BotAI...");
        auto botAI = BotAIFactory::instance()->CreateAI(player);
        if (botAI)
        {
            TC_LOG_INFO("module.playerbot.session", "🎯 BotAI created successfully, setting AI...");
            SetAI(botAI.release()); // Transfer ownership to BotSession
        }
        else
        {
            TC_LOG_WARN("module.playerbot.session", "🎯 Failed to create BotAI for character {}", characterGuid.ToString());
        }

        // Clear async login state (thread-safe)
        {
            std::lock_guard<std::mutex> lock(_asyncLogin.mutex);
            _asyncLogin.inProgress = false;
            _asyncLogin.player = nullptr;
            _asyncLogin.characterGuid = ObjectGuid::Empty;
        }

        TC_LOG_INFO("module.playerbot.session", "🎯 CompleteAsyncLogin finished successfully for character {}",
            characterGuid.ToString());

    }
    catch (std::exception const& e)
    {
        {
            std::lock_guard<std::mutex> lock(_asyncLogin.mutex);
            _asyncLogin.inProgress = false;
        }
    }
    catch (...)
    {
        {
            std::lock_guard<std::mutex> lock(_asyncLogin.mutex);
            _asyncLogin.inProgress = false;
        }
    }
}

void BotSession::ProcessBotQueryCallbacks()
{
    // SAFE CALLBACK PROCESSING: Use TrinityCore's public QueryProcessor interface
    // We can access the QueryProcessor through the public GetQueryProcessor() method

    try {
        TC_LOG_DEBUG("module.playerbot.session",
            "ProcessBotQueryCallbacks: Processing async callbacks for account {}", GetAccountId());

        // BREAKTHROUGH: Use the public GetQueryProcessor() method to access callbacks
        // This is the safe way to process async login callbacks without calling dangerous Update()
        auto& queryProcessor = GetQueryProcessor();
        queryProcessor.ProcessReadyCallbacks();

        // Process async login callbacks which are essential for completing bot login
        // The DelayQueryHolder() method routes callbacks to the main QueryProcessor
        // so calling ProcessReadyCallbacks() should execute our async login callbacks

        TC_LOG_DEBUG("module.playerbot.session",
            "ProcessBotQueryCallbacks: Callback processing completed for account {}", GetAccountId());
    }
    catch (std::exception const& e) {
        TC_LOG_ERROR("module.playerbot.session",
            "Exception in ProcessBotQueryCallbacks for account {}: {}", GetAccountId(), e.what());
    }
}

} // namespace Playerbot