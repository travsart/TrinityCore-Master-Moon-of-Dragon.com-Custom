/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#include "BotSession.h"
#include "Lifecycle/BotWorldEntry.h"
#include "AccountMgr.h"
#include "Log.h"
#include "WorldPacket.h"
#include "Player.h"
#include "DatabaseEnv.h"
#include "QueryHolder.h"
#include "CharacterDatabase.h"
#include "World.h"
#include "Map.h"
#include "MapManager.h"
#include "AI/BotAI.h"
#include "ObjectAccessor.h"
#include "GameTime.h"
#include "CharacterCache.h"
#include <thread>
#include <future>

namespace Playerbot {

/**
 * Enhanced Login Method using synchronous database queries and BotWorldEntry
 *
 * This implementation replaces the failing async callback pattern with a robust
 * synchronous approach that properly handles bot world entry.
 */
bool BotSession::LoginCharacterSync(ObjectGuid characterGuid)
{
    TC_LOG_INFO("module.playerbot.session",
                "Starting synchronous login for bot character {}",
                characterGuid.ToString());

    // Validate session state
    if (_loginState != LoginState::NONE)
    {
        TC_LOG_ERROR("module.playerbot.session",
                    "Bot {} already in login state {}",
                    characterGuid.ToString(),
                    static_cast<uint32>(_loginState.load()));
        return false;
    }

    // Check if character already exists in world
    if (Player* existingPlayer = ObjectAccessor::FindPlayer(characterGuid))
    {
        TC_LOG_WARN("module.playerbot.session",
                   "Character {} already exists in world",
                   characterGuid.ToString());
        return false;
    }

    // Set login state
    _loginState = LoginState::QUERY_PENDING;
    _pendingLoginGuid = characterGuid;
    _loginStartTime = std::chrono::steady_clock::now();

    try
    {
        // === PHASE 1: Load Character Data Synchronously ===
        TC_LOG_DEBUG("module.playerbot.session", "Phase 1: Loading character data");

        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER);
        stmt->setUInt64(0, characterGuid.GetCounter());
        PreparedQueryResult characterResult = CharacterDatabase.Query(stmt);

        if (!characterResult)
        {
            TC_LOG_ERROR("module.playerbot.session",
                        "Character {} not found in database",
                        characterGuid.ToString());
            _loginState = LoginState::LOGIN_FAILED;
            return false;
        }

        // === PHASE 2: Create Player Object ===
        TC_LOG_DEBUG("module.playerbot.session", "Phase 2: Creating Player object");

        _loginState = LoginState::LOGIN_IN_PROGRESS;

        Player* newPlayer = new Player(this);
        SetPlayer(newPlayer);

        // Initialize player motion master
        newPlayer->GetMotionMaster()->Initialize();

        // Load player from database result
        Field* fields = characterResult->Fetch();
        if (!newPlayer->LoadFromDB(characterGuid, fields))
        {
            TC_LOG_ERROR("module.playerbot.session",
                        "Failed to load player data for {}",
                        characterGuid.ToString());

            SetPlayer(nullptr);
            delete newPlayer;
            _loginState = LoginState::LOGIN_FAILED;
            return false;
        }

        // === PHASE 3: Load Additional Character Data ===
        TC_LOG_DEBUG("module.playerbot.session", "Phase 3: Loading additional character data");

        // Load customizations
        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_CUSTOMIZATIONS);
        stmt->setUInt64(0, characterGuid.GetCounter());
        PreparedQueryResult customizationResult = CharacterDatabase.Query(stmt);
        if (customizationResult)
        {
            newPlayer->LoadCustomizations(customizationResult);
        }

        // Load auras
        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_AURAS);
        stmt->setUInt64(0, characterGuid.GetCounter());
        PreparedQueryResult auraResult = CharacterDatabase.Query(stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_AURA_EFFECTS);
        stmt->setUInt64(0, characterGuid.GetCounter());
        PreparedQueryResult auraEffectResult = CharacterDatabase.Query(stmt);

        // Load spells
        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_SPELL);
        stmt->setUInt64(0, characterGuid.GetCounter());
        PreparedQueryResult spellResult = CharacterDatabase.Query(stmt);
        if (spellResult)
        {
            newPlayer->_LoadSpells(spellResult);
        }

        // Load action buttons
        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_ACTION);
        stmt->setUInt64(0, characterGuid.GetCounter());
        PreparedQueryResult actionResult = CharacterDatabase.Query(stmt);
        if (actionResult)
        {
            newPlayer->_LoadActions(actionResult);
        }

        // Load reputation
        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_REPUTATION);
        stmt->setUInt64(0, characterGuid.GetCounter());
        PreparedQueryResult reputationResult = CharacterDatabase.Query(stmt);
        if (reputationResult)
        {
            newPlayer->GetReputationMgr().LoadFromDB(reputationResult);
        }

        // Load inventory
        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_INVENTORY);
        stmt->setUInt64(0, characterGuid.GetCounter());
        PreparedQueryResult inventoryResult = CharacterDatabase.Query(stmt);
        if (inventoryResult)
        {
            newPlayer->_LoadInventory(inventoryResult, GameTime::GetGameTime());
        }

        // === PHASE 4: Initialize Bot-Specific Settings ===
        TC_LOG_DEBUG("module.playerbot.session", "Phase 4: Configuring bot settings");

        // Mark as bot
        newPlayer->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_IS_OUT_OF_BOUNDS);

        // Set initialization complete
        newPlayer->SetInitialized(true);

        // Prepare for world entry
        newPlayer->PrepareGossipMenu(newPlayer, newPlayer->GetDefaultGossipMenuForSource());

        // === PHASE 5: Use BotWorldEntry for World Integration ===
        TC_LOG_DEBUG("module.playerbot.session", "Phase 5: Beginning world entry");

        auto worldEntry = std::make_shared<BotWorldEntry>(shared_from_this(), characterGuid);

        // Perform synchronous world entry with 30 second timeout
        if (!worldEntry->EnterWorldSync(30000))
        {
            TC_LOG_ERROR("module.playerbot.session",
                        "Failed to complete world entry for bot {}",
                        characterGuid.ToString());

            // World entry handles cleanup
            _loginState = LoginState::LOGIN_FAILED;
            return false;
        }

        // === PHASE 6: Finalize Login ===
        TC_LOG_DEBUG("module.playerbot.session", "Phase 6: Finalizing login");

        // Update character online status
        stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_ONLINE);
        stmt->setUInt8(0, 1); // online
        stmt->setUInt64(1, characterGuid.GetCounter());
        CharacterDatabase.Execute(stmt);

        // Set login complete
        _loginState = LoginState::LOGIN_COMPLETE;
        m_playerLoading.Clear();
        m_playerLogout = false;
        m_playerRecentlyLogout = false;

        // Log success metrics
        auto loginDuration = std::chrono::steady_clock::now() - _loginStartTime;
        uint32 loginMs = static_cast<uint32>(
            std::chrono::duration_cast<std::chrono::milliseconds>(loginDuration).count());

        TC_LOG_INFO("module.playerbot.session",
                   "✅ Bot {} successfully logged in and entered world in {} ms",
                   newPlayer->GetName(),
                   loginMs);

        // Performance metrics from world entry
        BotWorldEntry::BotWorldEntryMetrics metrics = worldEntry->GetMetrics();
        TC_LOG_DEBUG("module.playerbot.session",
                    "World entry metrics - DB: {} µs, Player: {} µs, Map: {} µs, World: {} µs, AI: {} µs",
                    metrics.databaseLoadTime,
                    metrics.playerCreationTime,
                    metrics.mapLoadTime,
                    metrics.worldEntryTime,
                    metrics.aiInitTime);

        return true;
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("module.playerbot.session",
                    "Exception during bot login: {}",
                    e.what());

        _loginState = LoginState::LOGIN_FAILED;

        // Clean up player if created
        if (GetPlayer())
        {
            if (GetPlayer()->IsInWorld())
            {
                GetPlayer()->RemoveFromWorld();
            }
            SetPlayer(nullptr);
        }

        return false;
    }
}

/**
 * Enhanced Update method with world entry processing
 */
bool BotSession::UpdateEnhanced(uint32 diff, PacketFilter& updater)
{
    // Check for timeout on pending logins
    if (_loginState == LoginState::QUERY_PENDING || _loginState == LoginState::LOGIN_IN_PROGRESS)
    {
        auto now = std::chrono::steady_clock::now();
        if (now - _loginStartTime > LOGIN_TIMEOUT)
        {
            TC_LOG_ERROR("module.playerbot.session",
                        "Bot login timeout for character {}",
                        _pendingLoginGuid.ToString());
            _loginState = LoginState::LOGIN_FAILED;
        }
    }

    // Process bot packets
    ProcessBotPackets();

    // Update player if logged in
    if (GetPlayer() && GetPlayer()->IsInWorld())
    {
        // Update AI if available
        if (BotAI* ai = GetAI())
        {
            ai->UpdateAI(diff);
        }

        // Process player updates
        GetPlayer()->Update(diff);
    }

    // Handle logout if needed
    if (m_playerLogout)
    {
        if (GetPlayer())
        {
            LogoutPlayer(false);
        }
        return false;
    }

    return true;
}

/**
 * Queue-based world entry for multiple bots
 */
void BotSession::QueueWorldEntry(ObjectGuid characterGuid)
{
    auto worldEntry = std::make_shared<BotWorldEntry>(shared_from_this(), characterGuid);

    // Queue with the world entry manager
    uint32 queuePosition = BotWorldEntryQueue::instance()->QueueEntry(worldEntry);

    TC_LOG_INFO("module.playerbot.session",
               "Bot {} queued for world entry (position: {})",
               characterGuid.ToString(),
               queuePosition);
}

/**
 * Process queued world entries (called from world update)
 */
void ProcessBotWorldEntryQueue()
{
    // Process up to 10 concurrent bot entries per world update
    BotWorldEntryQueue::instance()->ProcessQueue(10);

    // Log statistics periodically
    static uint32 logCounter = 0;
    if (++logCounter >= 100) // Every 100 updates
    {
        logCounter = 0;

        auto stats = BotWorldEntryQueue::instance()->GetStats();
        if (stats.queuedEntries > 0 || stats.activeEntries > 0)
        {
            TC_LOG_INFO("module.playerbot.worldentry",
                       "World entry queue - Queued: {}, Active: {}, Completed: {}, Failed: {}, Avg time: {:.2f}s",
                       stats.queuedEntries,
                       stats.activeEntries,
                       stats.completedEntries,
                       stats.failedEntries,
                       stats.averageEntryTime);
        }
    }
}

} // namespace Playerbot