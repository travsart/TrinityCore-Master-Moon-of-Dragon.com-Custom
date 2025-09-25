/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#include "BotWorldEntry.h"
#include "BotSession.h"
#include "Player.h"
#include "World.h"
#include "WorldSession.h"
#include "Map.h"
#include "MapManager.h"
#include "ObjectAccessor.h"
#include "CharacterCache.h"
#include "CharacterDatabase.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "GameTime.h"
#include "AI/BotAI.h"
#include "SpellMgr.h"
#include "Chat/Chat.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "SocialMgr.h"
#include "CharacterPackets.h"
#include "MiscPackets.h"
#include <thread>

namespace Playerbot {

BotWorldEntry::BotWorldEntry(std::shared_ptr<BotSession> session, ObjectGuid characterGuid)
    : _session(session)
    , _characterGuid(characterGuid)
    , _player(nullptr)
    , _state(BotWorldEntryState::NONE)
    , _processing(false)
    , _retryCount(0)
{
    _metrics.startTime = std::chrono::steady_clock::now();
    _metrics.memoryBeforeEntry = 0; // TODO: Implement memory tracking
}

BotWorldEntry::~BotWorldEntry()
{
    if (_state != BotWorldEntryState::FULLY_ACTIVE && _state != BotWorldEntryState::CLEANUP)
    {
        Cleanup();
    }
}

bool BotWorldEntry::BeginWorldEntry(EntryCallback callback)
{
    if (_processing.exchange(true))
    {
        TC_LOG_ERROR("module.playerbot.worldentry", "Bot {} world entry already in progress",
                     _characterGuid.ToString());
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(_callbackMutex);
        _callback = callback;
    }

    TC_LOG_INFO("module.playerbot.worldentry", "Beginning world entry for bot {}",
                _characterGuid.ToString());

    // Start with loading character data
    if (!TransitionToState(BotWorldEntryState::CHARACTER_LOADED))
    {
        _processing = false;
        return false;
    }

    // Begin the entry process
    return LoadCharacterData();
}

bool BotWorldEntry::ProcessWorldEntry(uint32 diff)
{
    if (!_processing.load())
        return false;

    // Check for phase timeout
    auto now = std::chrono::steady_clock::now();
    if (now - _phaseStartTime > PHASE_TIMEOUT)
    {
        HandleWorldEntryFailure("Phase timeout exceeded");
        return false;
    }

    bool result = false;

    switch (_state.load())
    {
        case BotWorldEntryState::CHARACTER_LOADED:
            result = CreatePlayerObject();
            break;

        case BotWorldEntryState::PLAYER_CREATED:
            result = LoadTargetMap();
            break;

        case BotWorldEntryState::MAP_LOADED:
            result = AddPlayerToWorld();
            break;

        case BotWorldEntryState::IN_WORLD:
            result = InitializeAI();
            break;

        case BotWorldEntryState::AI_ACTIVE:
            result = FinalizeBotActivation();
            break;

        case BotWorldEntryState::FULLY_ACTIVE:
        {
            // Calculate final metrics
            _metrics.endTime = std::chrono::steady_clock::now();
            _metrics.totalTime = static_cast<uint32>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    _metrics.endTime - _metrics.startTime).count());

            TC_LOG_INFO("module.playerbot.worldentry",
                       "Bot {} world entry completed in {} ms",
                       _characterGuid.ToString(),
                       _metrics.totalTime / 1000);

            // Invoke callback if set
            {
                std::lock_guard<std::mutex> lock(_callbackMutex);
                if (_callback)
                {
                    _callback(true, _metrics);
                    _callback = nullptr;
                }
            }

            _processing = false;
            return false; // Entry complete
        }

        case BotWorldEntryState::FAILED:
        case BotWorldEntryState::CLEANUP:
            _processing = false;
            return false;

        default:
            TC_LOG_ERROR("module.playerbot.worldentry",
                        "Bot {} in unexpected state: {}",
                        _characterGuid.ToString(),
                        static_cast<uint32>(_state.load()));
            HandleWorldEntryFailure("Unexpected state");
            return false;
    }

    if (!result)
    {
        if (++_retryCount >= MAX_RETRY_COUNT)
        {
            HandleWorldEntryFailure("Maximum retry count exceeded");
            return false;
        }

        TC_LOG_WARN("module.playerbot.worldentry",
                   "Bot {} world entry phase failed, retrying ({}/{})",
                   _characterGuid.ToString(),
                   _retryCount,
                   MAX_RETRY_COUNT);
    }
    else
    {
        _retryCount = 0; // Reset retry count on success
    }

    return true;
}

bool BotWorldEntry::EnterWorldSync(uint32 timeoutMs)
{
    if (!BeginWorldEntry())
        return false;

    auto startTime = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(timeoutMs);

    while (_processing.load())
    {
        if (std::chrono::steady_clock::now() - startTime > timeout)
        {
            TC_LOG_ERROR("module.playerbot.worldentry",
                        "Bot {} world entry sync timeout after {} ms",
                        _characterGuid.ToString(),
                        timeoutMs);
            HandleWorldEntryFailure("Synchronous entry timeout");
            return false;
        }

        // Process one step
        ProcessWorldEntry(100);

        // Small sleep to prevent CPU spinning
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return _state == BotWorldEntryState::FULLY_ACTIVE;
}

bool BotWorldEntry::IsProcessing() const
{
    BotWorldEntryState state = _state.load();
    return state != BotWorldEntryState::NONE &&
           state != BotWorldEntryState::FULLY_ACTIVE &&
           state != BotWorldEntryState::FAILED &&
           state != BotWorldEntryState::CLEANUP;
}

uint32 BotWorldEntry::GetElapsedTime() const
{
    auto now = std::chrono::steady_clock::now();
    return static_cast<uint32>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now - _metrics.startTime).count());
}

void BotWorldEntry::SetError(std::string const& error)
{
    _metrics.lastError = error;
    _metrics.failedState = _state.load();
}

bool BotWorldEntry::TransitionToState(BotWorldEntryState newState)
{
    std::lock_guard<std::mutex> lock(_stateMutex);

    BotWorldEntryState oldState = _state.exchange(newState);
    _phaseStartTime = std::chrono::steady_clock::now();

    RecordStateTransition(oldState, newState);

    TC_LOG_DEBUG("module.playerbot.worldentry",
                "Bot {} transitioned from state {} to state {}",
                _characterGuid.ToString(),
                static_cast<uint32>(oldState),
                static_cast<uint32>(newState));

    return true;
}

void BotWorldEntry::RecordStateTransition(BotWorldEntryState oldState, BotWorldEntryState newState)
{
    auto now = std::chrono::steady_clock::now();
    uint32 phaseDuration = static_cast<uint32>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            now - _phaseStartTime).count());

    switch (oldState)
    {
        case BotWorldEntryState::CHARACTER_LOADED:
            _metrics.databaseLoadTime = phaseDuration;
            break;
        case BotWorldEntryState::PLAYER_CREATED:
            _metrics.playerCreationTime = phaseDuration;
            break;
        case BotWorldEntryState::MAP_LOADED:
            _metrics.mapLoadTime = phaseDuration;
            break;
        case BotWorldEntryState::IN_WORLD:
            _metrics.worldEntryTime = phaseDuration;
            break;
        case BotWorldEntryState::AI_ACTIVE:
            _metrics.aiInitTime = phaseDuration;
            break;
        default:
            break;
    }
}

bool BotWorldEntry::LoadCharacterData()
{
    TC_LOG_DEBUG("module.playerbot.worldentry",
                "Loading character data for bot {}",
                _characterGuid.ToString());

    // The session should handle the database loading
    // Here we just verify it was successful
    if (!_session)
    {
        SetError("Invalid session");
        return false;
    }

    // Check if character exists
    CharacterCacheEntry const* cacheEntry = sCharacterCache->GetCharacterCacheByGuid(_characterGuid);
    if (!cacheEntry)
    {
        // For bots, we may need to query directly
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_DATA_BY_GUID);
        stmt->setUInt64(0, _characterGuid.GetCounter());
        PreparedQueryResult result = CharacterDatabase.Query(stmt);

        if (!result)
        {
            SetError("Character not found in database");
            return false;
        }
    }

    // Mark data as loaded
    TransitionToState(BotWorldEntryState::CHARACTER_LOADED);
    return true;
}

bool BotWorldEntry::CreatePlayerObject()
{
    TC_LOG_DEBUG("module.playerbot.worldentry",
                "Creating player object for bot {}",
                _characterGuid.ToString());

    // Create the Player object
    _player = new Player(_session.get());
    _player->GetMotionMaster()->Initialize();

    // Load player from database using synchronous query
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER);
    stmt->setUInt64(0, _characterGuid.GetCounter());
    PreparedQueryResult result = CharacterDatabase.Query(stmt);

    if (!result)
    {
        SetError("Failed to load character from database");
        delete _player;
        _player = nullptr;
        return false;
    }

    Field* fields = result->Fetch();
    if (!_player->LoadFromDB(_characterGuid, fields))
    {
        SetError("Failed to load player data");
        delete _player;
        _player = nullptr;
        return false;
    }

    // Set player in session
    _session->SetPlayer(_player);

    // Initialize player for bot
    _player->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_IS_OUT_OF_BOUNDS); // Mark as bot
    _player->SetInitialized(true);

    TransitionToState(BotWorldEntryState::PLAYER_CREATED);
    return true;
}

bool BotWorldEntry::LoadTargetMap()
{
    if (!_player)
    {
        SetError("Player object not created");
        return false;
    }

    TC_LOG_DEBUG("module.playerbot.worldentry",
                "Loading map {} for bot {}",
                _player->GetMapId(),
                _characterGuid.ToString());

    // Get the map where the player should be
    Map* map = sMapMgr->CreateMap(_player->GetMapId(), _player);
    if (!map)
    {
        SetError("Failed to create/load map");
        return false;
    }

    // Verify map is valid and can accept player
    if (map->CannotEnter(_player) != TRANSFER_ABORT_NONE)
    {
        SetError("Map cannot accept player");
        return false;
    }

    TransitionToState(BotWorldEntryState::MAP_LOADED);
    return true;
}

bool BotWorldEntry::AddPlayerToWorld()
{
    if (!_player)
    {
        SetError("Player object not created");
        return false;
    }

    TC_LOG_DEBUG("module.playerbot.worldentry",
                "Adding bot {} to world",
                _characterGuid.ToString());

    // Send initial packets before adding to map
    SendInitialPacketsBeforeMap();

    // Get the target map
    Map* map = sMapMgr->CreateMap(_player->GetMapId(), _player);
    if (!map)
    {
        SetError("Failed to get map for player");
        return false;
    }

    // Add player to map
    if (!map->AddPlayerToMap(_player))
    {
        SetError("Failed to add player to map");
        return false;
    }

    // Player is now in world
    _player->SetInWorld(true);

    // Initialize visibility
    _player->UpdateObjectVisibility();

    // Send initial packets after adding to map
    SendInitialPacketsAfterMap();

    // Initialize position and movement
    InitializeBotPosition();

    // Set up appearance
    InitializeBotAppearance();

    // Register with object accessor for finding
    sObjectAccessor->AddObject(_player);

    TC_LOG_INFO("module.playerbot.worldentry",
               "Bot {} successfully added to world at position ({:.2f}, {:.2f}, {:.2f}) in map {}",
               _player->GetName(),
               _player->GetPositionX(),
               _player->GetPositionY(),
               _player->GetPositionZ(),
               _player->GetMapId());

    TransitionToState(BotWorldEntryState::IN_WORLD);
    return true;
}

bool BotWorldEntry::InitializeAI()
{
    if (!_player || !_player->IsInWorld())
    {
        SetError("Player not in world");
        return false;
    }

    TC_LOG_DEBUG("module.playerbot.worldentry",
                "Initializing AI for bot {}",
                _characterGuid.ToString());

    // Create and initialize AI
    BotAI* ai = new BotAI(_player);

    // Initialize AI systems
    ai->Reset();

    // Set AI in session for reference
    if (BotSession* botSession = dynamic_cast<BotSession*>(_session.get()))
    {
        botSession->SetAI(ai);
    }

    // Store AI reference in player
    _player->SetAI(ai);

    // Start AI updates
    ai->OnRespawn();

    TC_LOG_INFO("module.playerbot.worldentry",
               "AI initialized for bot {}",
               _player->GetName());

    TransitionToState(BotWorldEntryState::AI_ACTIVE);
    return true;
}

bool BotWorldEntry::FinalizeBotActivation()
{
    if (!_player || !_player->IsInWorld())
    {
        SetError("Player not in world");
        return false;
    }

    TC_LOG_DEBUG("module.playerbot.worldentry",
                "Finalizing activation for bot {}",
                _characterGuid.ToString());

    // Update last login time
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_ONLINE);
    stmt->setUInt8(0, 1); // online
    stmt->setUInt64(1, _characterGuid.GetCounter());
    CharacterDatabase.Execute(stmt);

    // Join world channel if configured
    if (sWorld->getBoolConfig(CONFIG_CHAT_WORLD_ENABLE))
    {
        if (ChannelMgr* cMgr = ChannelMgr::forTeam(_player->GetTeam()))
        {
            if (Channel* world = cMgr->GetJoinChannel(sWorld->getStringConfig(CONFIG_GLOBAL_CHANNEL_NAME), 0))
            {
                world->JoinChannel(_player);
            }
        }
    }

    // Load social lists
    sSocialMgr->LoadFromDB(nullptr, _characterGuid);

    // Update guild information if in guild
    if (Guild* guild = sGuildMgr->GetGuildById(_player->GetGuildId()))
    {
        guild->OnLogin(_player);
    }

    // Set bot as fully active
    _player->SetCanModifyStats(true);
    _player->UpdateAllStats();
    _player->UpdateAllCritPercentages();
    _player->UpdateAllSpellCritChances();
    _player->UpdateArmor();

    // Send final initialization complete
    WorldPackets::Misc::SetTimeZoneInformation packet;
    _session->SendPacket(packet.Write());

    TC_LOG_INFO("module.playerbot.worldentry",
               "Bot {} is now fully active in world",
               _player->GetName());

    TransitionToState(BotWorldEntryState::FULLY_ACTIVE);
    return true;
}

void BotWorldEntry::SendInitialPacketsBeforeMap()
{
    if (!_player || !_session)
        return;

    // Send login verification
    WorldPackets::Character::LoginVerifyWorld packet;
    packet.MapID = _player->GetMapId();
    packet.Pos = _player->GetPosition();
    packet.Orientation = _player->GetOrientation();
    _session->SendPacket(packet.Write());

    // Account data times (empty for bots)
    WorldPackets::ClientConfig::AccountDataTimes accountData;
    accountData.PlayerGuid = _player->GetGUID();
    accountData.ServerTime = GameTime::GetGameTime();
    _session->SendPacket(accountData.Write());

    // Features
    WorldPackets::Character::FeatureSystemStatus features;
    features.ComplaintStatus = 2;
    features.ScrollOfResurrectionRequestsRemaining = 0;
    features.ScrollOfResurrectionMaxRequestsPerDay = 0;
    features.CfgRealmID = 0;
    features.CfgRealmRecID = 0;
    features.EuropaTicketSystemStatus = false;
    features.GameRulesEnabled = false;
    features.BpayStoreAvailable = false;
    features.BpayStoreDisabledByParentalControls = false;
    features.ItemRestorationButtonEnabled = false;
    features.BrowserEnabled = false;
    _session->SendPacket(features.Write());

    // MOTD
    WorldPackets::Character::MOTD motd;
    motd.Text = sWorld->GetMotd();
    _session->SendPacket(motd.Write());

    // Set rest bonus
    _player->SetRestBonus(_player->GetRestBonus());
}

void BotWorldEntry::SendInitialPacketsAfterMap()
{
    if (!_player || !_session)
        return;

    // Update time speed
    WorldPackets::Misc::LoginSetTimeSpeed loginSetTimeSpeed;
    loginSetTimeSpeed.ServerTime = GameTime::GetGameTime();
    loginSetTimeSpeed.GameTime = GameTime::GetGameTime();
    loginSetTimeSpeed.NewSpeed = 0.01666667f;
    _session->SendPacket(loginSetTimeSpeed.Write());

    // Send initial spells
    _player->SendInitialSpells();

    // Send action bars
    _player->SendInitialActionButtons();

    // Initialize factions
    _player->GetReputationMgr().SendInitialReputations();

    // Send taxi nodes
    _player->SendTaxiMenu(_player);

    // Send equipment sets
    _player->SendEquipmentSetList();

    // Update zone
    _player->UpdateZone(_player->GetZoneId(), _player->GetAreaId());
}

void BotWorldEntry::InitializeBotPosition()
{
    if (!_player)
        return;

    // Ensure player is at a valid position
    if (!_player->IsPositionValid())
    {
        TC_LOG_WARN("module.playerbot.worldentry",
                   "Bot {} has invalid position, relocating to homebind",
                   _player->GetName());

        // Teleport to homebind
        _player->TeleportTo(_player->m_homebind);
    }

    // Make sure player is not falling
    if (_player->HasUnitMovementFlag(MOVEMENTFLAG_FALLING))
    {
        _player->RemoveUnitMovementFlag(MOVEMENTFLAG_FALLING);
        _player->SetFallInformation(GameTime::GetGameTimeMS(), _player->GetPositionZ());
    }

    // Stop any movement
    _player->StopMoving();
}

void BotWorldEntry::InitializeBotAppearance()
{
    if (!_player)
        return;

    // Ensure player has proper display
    if (!_player->GetDisplayId())
    {
        _player->SetDisplayId(_player->GetNativeDisplayId());
    }

    // Update model/race/gender if needed
    _player->InitDisplayIds();

    // Make visible
    _player->SetVisibility(VISIBILITY_ON);
}

void BotWorldEntry::HandleWorldEntryFailure(std::string const& reason)
{
    TC_LOG_ERROR("module.playerbot.worldentry",
                "Bot {} world entry failed: {}",
                _characterGuid.ToString(),
                reason);

    SetError(reason);
    TransitionToState(BotWorldEntryState::FAILED);

    // Invoke callback with failure
    {
        std::lock_guard<std::mutex> lock(_callbackMutex);
        if (_callback)
        {
            _callback(false, _metrics);
            _callback = nullptr;
        }
    }

    // Clean up
    Cleanup();

    _processing = false;
}

void BotWorldEntry::Cleanup()
{
    TransitionToState(BotWorldEntryState::CLEANUP);

    if (_player)
    {
        if (_player->IsInWorld())
        {
            _player->RemoveFromWorld();
        }

        // Clean logout
        _session->LogoutPlayer(false);
        _player = nullptr;
    }

    _session.reset();
}

// === BotWorldEntryQueue Implementation ===

BotWorldEntryQueue* BotWorldEntryQueue::instance()
{
    static BotWorldEntryQueue instance;
    return &instance;
}

uint32 BotWorldEntryQueue::QueueEntry(std::shared_ptr<BotWorldEntry> entry)
{
    if (!entry)
        return 0;

    std::lock_guard<std::mutex> lock(_queueMutex);

    _pendingQueue.push(entry);
    return static_cast<uint32>(_pendingQueue.size());
}

void BotWorldEntryQueue::ProcessQueue(uint32 maxConcurrent)
{
    std::lock_guard<std::mutex> lock(_queueMutex);

    // Remove completed entries
    _activeEntries.erase(
        std::remove_if(_activeEntries.begin(), _activeEntries.end(),
            [this](std::shared_ptr<BotWorldEntry> const& entry)
            {
                if (!entry->IsProcessing())
                {
                    if (entry->IsComplete())
                    {
                        _totalCompleted++;
                        _totalEntryTime += entry->GetMetrics().totalTime;
                    }
                    else if (entry->IsFailed())
                    {
                        _totalFailed++;
                    }
                    return true;
                }
                return false;
            }),
        _activeEntries.end()
    );

    // Process active entries
    for (auto& entry : _activeEntries)
    {
        entry->ProcessWorldEntry(100);
    }

    // Start new entries if below concurrent limit
    while (_activeEntries.size() < maxConcurrent && !_pendingQueue.empty())
    {
        auto entry = _pendingQueue.front();
        _pendingQueue.pop();

        if (entry->BeginWorldEntry())
        {
            _activeEntries.push_back(entry);
        }
    }
}

BotWorldEntryQueue::QueueStats BotWorldEntryQueue::GetStats() const
{
    std::lock_guard<std::mutex> lock(_queueMutex);

    QueueStats stats;
    stats.queuedEntries = static_cast<uint32>(_pendingQueue.size());
    stats.activeEntries = static_cast<uint32>(_activeEntries.size());
    stats.completedEntries = _totalCompleted.load();
    stats.failedEntries = _totalFailed.load();

    if (_totalCompleted > 0)
    {
        stats.averageEntryTime = static_cast<float>(_totalEntryTime.load()) /
                                 static_cast<float>(_totalCompleted.load()) / 1000000.0f; // Convert to seconds
    }
    else
    {
        stats.averageEntryTime = 0.0f;
    }

    return stats;
}

void BotWorldEntryQueue::ClearQueue()
{
    std::lock_guard<std::mutex> lock(_queueMutex);

    while (!_pendingQueue.empty())
    {
        _pendingQueue.pop();
    }

    _activeEntries.clear();

    TC_LOG_WARN("module.playerbot.worldentry", "Bot world entry queue cleared");
}

} // namespace Playerbot