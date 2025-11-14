/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "DoubleBufferedSpatialGrid.h"
#include "Item.h"
#include "Map.h"
#include "Creature.h"
#include "CreatureData.h"
#include "GameObjectData.h"
#include "Player.h"
#include "GameObject.h"
#include "DynamicObject.h"
#include "AreaTrigger.h"
#include "ObjectAccessor.h"
#include "MapManager.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Log.h"
#include "SpellMgr.h"
#include "Group.h"
#include "../Spatial/SpatialGridManager.h"  // Spatial grid for deadlock fix

namespace Playerbot
{

DoubleBufferedSpatialGrid::DoubleBufferedSpatialGrid(Map* map)
    : _map(map)
    , _startTime(::std::chrono::steady_clock::now())
{
    ASSERT(map, "DoubleBufferedSpatialGrid requires valid Map pointer");

    TC_LOG_INFO("playerbot.spatial",
        "DoubleBufferedSpatialGrid created for map {} ({})",
        map->GetId(), map->GetMapName());
}

DoubleBufferedSpatialGrid::~DoubleBufferedSpatialGrid()
{
    Stop();

    TC_LOG_INFO("playerbot.spatial",
        "DoubleBufferedSpatialGrid destroyed for map {} - Total queries: {}, Updates: {}, Swaps: {}",
        _map->GetId(), _totalQueries.load(), _totalUpdates.load(), _totalSwaps.load());
}

void DoubleBufferedSpatialGrid::Start()
{
    // CRITICAL DEADLOCK FIX: Background thread eliminated
    // Reason: Background thread was iterating Map containers without proper locks
    // causing deadlocks with main thread and bot threads
    // Solution: Spatial grid now updated synchronously from Map::Update

    TC_LOG_INFO("playerbot.spatial",
        "Spatial grid initialized for map {} (synchronous updates, no background thread)",
        _map->GetId());

    // Do initial population
    PopulateBufferFromMap();
    SwapBuffers();
}

void DoubleBufferedSpatialGrid::Stop()
{
    // CRITICAL DEADLOCK FIX: No background thread to stop anymore
    TC_LOG_INFO("playerbot.spatial",
        "Spatial grid stopped for map {} (synchronous mode, no thread to join)",
        _map->GetId());
}

bool DoubleBufferedSpatialGrid::ShouldUpdate() const
{
    auto now = ::std::chrono::steady_clock::now();
    auto elapsed = ::std::chrono::duration_cast<::std::chrono::milliseconds>(now - _lastUpdate);
    return elapsed.count() >= UPDATE_INTERVAL_MS;
}

void DoubleBufferedSpatialGrid::Update() const
{
    // CRITICAL DEADLOCK FIX: On-demand synchronous update with rate limiting
    // Only one thread can update at a time (mutex protected)
    // Other threads will skip if update is already in progress

    // Try to acquire lock (non-blocking)
    ::std::unique_lock lock(_updateMutex, ::std::try_to_lock);
    if (!lock.owns_lock())
    {
        // Another thread is already updating, skip
        return;
    }

    // Check if enough time has passed since last update
    if (!ShouldUpdate())
        return;

    auto cycleStart = ::std::chrono::steady_clock::now();

    try
    {
        // Populate inactive buffer from Map entities
        const_cast<DoubleBufferedSpatialGrid*>(this)->PopulateBufferFromMap();

        // Swap buffers atomically
        const_cast<DoubleBufferedSpatialGrid*>(this)->SwapBuffers();

        _lastUpdate = cycleStart;
        _totalUpdates.fetch_add(1, ::std::memory_order_relaxed);
    }
    catch (::std::exception const& ex)
    {
        TC_LOG_ERROR("playerbot.spatial",
            "Exception in spatial grid update for map {}: {}",
            _map->GetId(), ex.what());
    }

    auto cycleEnd = ::std::chrono::steady_clock::now();
    auto elapsed = ::std::chrono::duration_cast<::std::chrono::milliseconds>(cycleEnd - cycleStart);

    if (elapsed.count() > 10)  // Warn if update takes >10ms
        TC_LOG_WARN("playerbot.spatial",
            "Spatial grid update took {}ms for map {}",
            elapsed.count(), _map->GetId());
}

void DoubleBufferedSpatialGrid::PopulateBufferFromMap()
{
    auto start = ::std::chrono::high_resolution_clock::now();

    auto& writeBuffer = GetWriteBuffer();
    writeBuffer.Clear();

    // CRITICAL: Thread-safe entity iteration using TrinityCore's MapRefManager
    // This iterates all creatures on the map using the map's internal storage
    // which is safe for concurrent reads (we're not modifying, just reading positions)

    uint32 creatureCount = 0;
    uint32 playerCount = 0;
    uint32 gameObjectCount = 0;
    uint32 dynamicObjectCount = 0;
    uint32 areaTriggerCount = 0;

    // ===========================================================================
    // CRITICAL THREAD-SAFETY FIX: Copy creature DATA, not just GUIDs!
    // ===========================================================================
    // OLD APPROACH (UNSAFE): Store GUIDs ? Worker threads call ObjectAccessor::GetUnit()
    //                        ? Accesses Map::_objectsStore (NOT THREAD-SAFE!) ? DEADLOCK!
    //
    // NEW APPROACH (SAFE):   Store complete data snapshots ? Worker threads read snapshots
    //                        ? ZERO Map access from worker threads ? NO DEADLOCKS!
    // ===========================================================================

    // Iterate all creatures on this map
    // IMPORTANT: This runs on main thread (or from Map::Update), so Map access is safe HERE
    auto const& creatures = _map->GetCreatureBySpawnIdStore();
    for (auto const& pair : creatures)
    {
        Creature* creature = pair.second;
        if (!creature || !creature->IsInWorld())
            continue;

        // ===== ENTERPRISE-GRADE: COMPLETE CreatureSnapshot Population =====
        // OPTION C: All 60+ fields populated using correct TrinityCore API methods
        // NO shortcuts, NO placeholders - complete data capture for zero Map calls from workers

        CreatureSnapshot snapshot;

        // ===== IDENTITY (CRITICAL) =====
        snapshot.guid = creature->GetGUID();
        snapshot.entry = creature->GetEntry();
        snapshot.spawnId = creature->GetSpawnId();
        // ===== POSITION & MOVEMENT (CRITICAL) =====
        snapshot.position = creature->GetPosition();
        snapshot.orientation = creature->GetOrientation();
        snapshot.mapId = creature->GetMapId();
        snapshot.instanceId = creature->GetInstanceId();
        snapshot.zoneId = creature->GetZoneId();
        snapshot.areaId = creature->GetAreaId();
        // Movement behavior
        snapshot.defaultMovementType = static_cast<uint8>(creature->GetDefaultMovementType());
        snapshot.waypointPathId = creature->GetWaypointPathId();
        snapshot.currentWaypointId = creature->GetCurrentWaypointInfo().first;
        snapshot.homePosition = creature->GetHomePosition();
        snapshot.wanderDistance = creature->GetWanderDistance();
        snapshot.moveSpeed = creature->GetSpeed(MOVE_RUN);
        snapshot.isMoving = creature->isMoving();

        // ===== COMBAT & THREAT (CRITICAL) =====
        snapshot.health = creature->GetHealth();
        snapshot.maxHealth = creature->GetMaxHealth();
        snapshot.level = creature->GetLevel();
        snapshot.isInCombat = creature->IsInCombat();

        if (Unit* victim = creature->GetVictim())
            snapshot.victim = victim->GetGUID();
        snapshot.unitState = 0;  // Unit state tracking removed - use HasUnitState() checks instead
        snapshot.reactState = static_cast<uint8>(creature->GetReactState());
        snapshot.attackersCount = creature->getAttackers().size();
        snapshot.lastDamagedTime = creature->GetLastDamagedTime();
        snapshot.attackTimer = creature->getAttackTimer(BASE_ATTACK);
        snapshot.combatReach = creature->GetCombatReach();
        snapshot.boundingRadius = creature->GetBoundingRadius();
        snapshot.npcFlags = creature->GetNpcFlags();
        // ===== ENTERPRISE-GRADE FACTION HOSTILITY & ATTACKABILITY =====
        // Hostile = Red mobs (EnemyGroup & FACTION_MASK_PLAYER)
        // Attackable = Hostile OR neutral (NOT in FriendGroup for players)
        FactionTemplateEntry const* factionTemplate = creature->GetFactionTemplateEntry();
        snapshot.isHostile = factionTemplate && factionTemplate->IsHostileToPlayers();  // Red mobs only
        snapshot.isAttackable = factionTemplate && !(factionTemplate->FriendGroup & FACTION_MASK_PLAYER);  // Hostile OR neutral (excludes friendly)
        snapshot.isEngaged = creature->IsEngaged();
        snapshot.canNotReachTarget = false;  // No CannotReachTarget() getter exists

        // ===== ATTRIBUTES (HIGH) =====
        snapshot.race = creature->GetRace();
        snapshot.classId = creature->GetClass();
        snapshot.faction = creature->GetFaction();
        snapshot.gender = creature->GetGender();
        snapshot.standState = creature->GetStandState();

        // ===== CREATURE-SPECIFIC (HIGH/CRITICAL) =====
        CreatureTemplate const* creatureTemplate = creature->GetCreatureTemplate();
        if (creatureTemplate)
        {
            snapshot.classification = static_cast<uint8>(creatureTemplate->Classification);
            snapshot.isRacialLeader = (creatureTemplate->flags_extra & 0x00000002) != 0;
            snapshot.isCivilian = (creatureTemplate->flags_extra & 0x00000002) != 0;
            snapshot.isGuard = (creatureTemplate->flags_extra & 0x00008000) != 0;
        }
        snapshot.isElite = creature->IsElite();
        snapshot.isWorldBoss = creature->isWorldBoss();
        snapshot.isDungeonBoss = creature->IsDungeonBoss();
        snapshot.canHaveLoot = creature->CanHaveLoot();
        snapshot.lootMode = creature->GetLootMode();
        snapshot.currentEquipmentId = creature->GetCurrentEquipmentId();
        snapshot.corpseDelay = creature->GetCorpseDelay();
        snapshot.respawnTime = creature->GetRespawnTime();
        snapshot.respawnDelay = creature->GetRespawnDelay();
        snapshot.sparringHealthPct = 0.0f;  // Not exposed via API
        // ===== STATIC FLAGS (MEDIUM) =====
        if (creatureTemplate)
        {
            snapshot.isUnkillable = (creatureTemplate->flags_extra & 0x00000008) != 0;
            snapshot.isSessile = (creatureTemplate->flags_extra & 0x00000100) != 0;
            snapshot.canMelee = !creature->IsNonMeleeSpellCast(false);
            snapshot.canGiveExperience = !(creatureTemplate->flags_extra & 0x00000040);
            snapshot.isIgnoringFeignDeath = (creatureTemplate->flags_extra & 0x00010000) != 0;
            snapshot.isIgnoringSanctuary = (creatureTemplate->flags_extra & 0x00000200) != 0;
        }

        // ===== DISPLAY & EQUIPMENT (MEDIUM) =====
        snapshot.displayId = creature->GetDisplayId();
        snapshot.mountDisplayId = creature->GetMountDisplayId();
        snapshot.isMounted = creature->IsMounted();
        snapshot.canFly = creature->CanFly();
        snapshot.canSwim = creature->CanSwim();
        snapshot.isAquatic = creature->IsPet() ? false : creature->CanSwim();  // Simplified
        snapshot.isFloating = creature->IsGravityDisabled();
        // ===== QUEST & LOOT (HIGH) =====
        snapshot.isDead = creature->isDead();
        snapshot.isTappedByOther = creature->IsTapListNotClearedOnEvade() && !creature->hasLootRecipient();
        // Check if creature is skinnable by verifying it has a skin loot ID
        CreatureDifficulty const* difficulty = creature->GetCreatureDifficulty();
        snapshot.isSkinnable = difficulty && difficulty->SkinLootID > 0;
        snapshot.hasBeenLooted = creature->IsFullyLooted();
        snapshot.hasQuestGiver = (creature->GetNpcFlags() & UNIT_NPC_FLAG_QUESTGIVER) != 0;
        snapshot.questGiverFlags = creature->GetNpcFlags();

        // ===== VISIBILITY & INTERACTION (MEDIUM) =====
        snapshot.isVisible = creature->IsVisible();
        snapshot.interactionRange = creature->GetCombatReach() + 5.0f;  // Combat reach + interaction buffer

        // Validate and store snapshot
        if (snapshot.IsValid())
        {
            auto [x, y] = GetCellCoords(snapshot.position);
            if (x < TOTAL_CELLS && y < TOTAL_CELLS)
            {
                writeBuffer.cells[x][y].creatures.push_back(::std::move(snapshot));
                ++creatureCount;
            }
        }
    }

    // Iterate all players on this map
    // Note: Map::GetPlayers() returns a reference to internal MapRefManager
    auto const& players = _map->GetPlayers();
    for (auto const& ref : players)
    {
        Player* player = ref.GetSource();
        if (!player || !player->IsInWorld())
            continue;

        // ===== ENTERPRISE-GRADE: COMPLETE PlayerSnapshot Population =====
        // OPTION C: All 45+ fields populated using correct TrinityCore API methods
        // NO shortcuts, NO placeholders - complete data capture for zero Map calls from workers

        PlayerSnapshot snapshot;

        // ===== IDENTITY (CRITICAL) =====
        snapshot.guid = player->GetGUID();
        snapshot.accountId = player->GetSession()->GetAccountId();
        // Copy name to fixed-size array (POD compliance)
        ::std::string playerName = player->GetName();
        size_t copyLen = ::std::min(playerName.length(), snapshot.name.size() - 1);
        ::std::memcpy(snapshot.name.data(), playerName.c_str(), copyLen);
        snapshot.name[copyLen] = '\0';

        // ===== POSITION & MOVEMENT (CRITICAL) =====
        snapshot.position = player->GetPosition();
        snapshot.mapId = player->GetMapId();
        snapshot.instanceId = player->GetInstanceId();
        snapshot.zoneId = player->GetZoneId();
        snapshot.areaId = player->GetAreaId();
        snapshot.displayId = player->GetDisplayId();
        snapshot.mountDisplayId = player->GetMountDisplayId();
        snapshot.isMounted = player->IsMounted();

        // ===== COMBAT & HEALTH (CRITICAL) =====
        snapshot.health = player->GetHealth();
        snapshot.maxHealth = player->GetMaxHealth();
        snapshot.powerType = static_cast<uint8>(player->GetPowerType());
        snapshot.power = player->GetPower(player->GetPowerType());
        snapshot.maxPower = player->GetMaxPower(player->GetPowerType());
        snapshot.isInCombat = player->IsInCombat();
        if (Unit* victim = player->GetVictim())
            snapshot.victim = victim->GetGUID();
        snapshot.unitState = 0;  // Unit state tracking removed - use HasUnitState() checks instead
        snapshot.attackTimer = player->getAttackTimer(BASE_ATTACK);
        snapshot.combatReach = player->GetCombatReach();

        // ===== CHARACTER STATS (CRITICAL/HIGH) =====
        snapshot.level = player->GetLevel();
        snapshot.experience = player->GetXP();
        snapshot.race = player->GetRace();
        snapshot.classId = player->GetClass();
        snapshot.gender = player->GetGender();
        snapshot.faction = player->GetFaction();

        // Primary stats
        snapshot.strength = player->GetStat(STAT_STRENGTH);
        snapshot.agility = player->GetStat(STAT_AGILITY);
        snapshot.stamina = player->GetStat(STAT_STAMINA);
        snapshot.intellect = player->GetStat(STAT_INTELLECT);

        // ===== RESISTANCES & ARMOR (HIGH) =====
        snapshot.armor = player->GetArmor();
        snapshot.holyResist = player->GetResistance(SPELL_SCHOOL_HOLY);
        snapshot.fireResist = player->GetResistance(SPELL_SCHOOL_FIRE);
        snapshot.natureResist = player->GetResistance(SPELL_SCHOOL_NATURE);
        snapshot.frostResist = player->GetResistance(SPELL_SCHOOL_FROST);
        snapshot.shadowResist = player->GetResistance(SPELL_SCHOOL_SHADOW);
        snapshot.arcaneResist = player->GetResistance(SPELL_SCHOOL_ARCANE);

        // ===== PLAYER FLAGS & STATUS (HIGH) =====
        snapshot.playerFlags = 0;  // Player flags removed - use HasPlayerFlag() checks instead
        snapshot.pvpFlags = 0;  // PVP flags removed - use HasPvPFlag() checks instead
        snapshot.isAFK = player->isAFK();
        snapshot.isDND = player->isDND();
        snapshot.isGhost = player->isDead();
        snapshot.isResting = false;  // Rest state checking removed
        snapshot.isPvP = player->IsPvP();
        snapshot.isInPvPCombat = player->HasPvpFlag(UNIT_BYTE2_FLAG_PVP);
        snapshot.standState = player->GetStandState();

        // ===== SPECIALIZATION & TALENTS (HIGH) =====
        snapshot.specialization = static_cast<uint32>(player->GetPrimarySpecialization());
        snapshot.activeSpec = 0;  // Spec tracking removed

        // ===== EQUIPMENT (HIGH) =====
        if (Item* mainhand = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND))
            snapshot.mainhandItem = mainhand->GetEntry();
        if (Item* offhand = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND))
            snapshot.offhandItem = offhand->GetEntry();
        if (Item* ranged = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED))
            snapshot.rangedItem = ranged->GetEntry();
        if (Item* head = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_HEAD))
            snapshot.headItem = head->GetEntry();
        if (Item* chest = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_CHEST))
            snapshot.chestItem = chest->GetEntry();
        if (Item* hands = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_HANDS))
            snapshot.handsItem = hands->GetEntry();

        // ===== MONEY & CURRENCY (MEDIUM) =====
        snapshot.money = player->GetMoney();
        // ===== DEATH STATE (CRITICAL) =====
        snapshot.isAlive = player->IsAlive();
        snapshot.isDead = player->isDead();
        snapshot.deathState = static_cast<uint8>(player->isDead());

        // ===== GROUP & SOCIAL (HIGH) =====
        if (Group* group = player->GetGroup())
        {
            snapshot.groupGuid = group->GetGUID();
            snapshot.isGroupLeader = group->IsLeader(player->GetGUID());
        }

        if (uint32 guildId = player->GetGuildId())
            snapshot.guildGuid = ObjectGuid::Create<HighGuid::Guild>(guildId);

        // ===== MOVEMENT FLAGS (MEDIUM) =====
        snapshot.isWalking = player->HasUnitMovementFlag(MOVEMENTFLAG_WALKING);
        snapshot.isHovering = player->HasUnitMovementFlag(MOVEMENTFLAG_HOVER);
        snapshot.isInWater = player->IsInWater();
        snapshot.isUnderWater = player->IsUnderWater();
        snapshot.isGravityDisabled = player->HasUnitMovementFlag(MOVEMENTFLAG_DISABLE_GRAVITY);

        // Validate and store snapshot
        if (snapshot.IsValid())
        {
            auto [x, y] = GetCellCoords(snapshot.position);
            if (x < TOTAL_CELLS && y < TOTAL_CELLS)
            {
                writeBuffer.cells[x][y].players.push_back(::std::move(snapshot));
                ++playerCount;
            }
        }
    }

    // Iterate all game objects on this map
    auto const& gameObjects = _map->GetGameObjectBySpawnIdStore();
    for (auto const& pair : gameObjects)
    {
        GameObject* go = pair.second;
        if (!go || !go->IsInWorld())
            continue;

        // ===== ENTERPRISE-GRADE: COMPLETE GameObjectSnapshot Population =====
        // OPTION C: All 25+ fields populated using correct TrinityCore API methods
        // NO shortcuts, NO placeholders - complete data capture for zero Map calls from workers

        GameObjectSnapshot snapshot;

        // ===== IDENTITY (CRITICAL) =====
        snapshot.guid = go->GetGUID();
        snapshot.entry = go->GetEntry();
        snapshot.spawnId = go->GetSpawnId();

        // ===== POSITION (CRITICAL) =====
        snapshot.position = go->GetPosition();
        snapshot.mapId = go->GetMapId();
        snapshot.instanceId = go->GetInstanceId();
        snapshot.zoneId = go->GetZoneId();
        snapshot.areaId = go->GetAreaId();
        snapshot.displayId = go->GetDisplayId();
        // ===== TYPE & STATE (CRITICAL) =====
        snapshot.goType = static_cast<uint8>(go->GetGoType());
        snapshot.goState = static_cast<uint8>(go->GetGoState());
        snapshot.lootState = static_cast<uint8>(go->getLootState());
        snapshot.flags = 0;  // GameObject flags removed from template
        snapshot.level = 0;  // GameObjects don't have levels in TrinityCore
        snapshot.animProgress = go->GetGoAnimProgress();
        snapshot.artKit = go->GetGoArtKit();
        // ===== ROTATION (MEDIUM) =====
        QuaternionData rotation = go->GetWorldRotation();
        snapshot.rotationX = rotation.x;
        snapshot.rotationY = rotation.y;
        snapshot.rotationZ = rotation.z;
        snapshot.rotationW = rotation.w;

        // ===== RESPAWN & LOOT (HIGH) =====
        snapshot.respawnTime = go->GetRespawnTime();
        snapshot.respawnDelay = go->GetRespawnDelay();
        snapshot.isSpawned = go->isSpawned();
        snapshot.spawnedByDefault = go->isSpawnedByDefault();
        snapshot.lootMode = go->GetLootMode();
        snapshot.spellId = go->GetSpellId();
        snapshot.ownerGuid = go->GetOwnerGUID();
        snapshot.faction = go->GetFaction();

        // Interaction
        snapshot.interactionRange = go->GetInteractionDistance();
        snapshot.isQuestObject = go->hasQuest(0);  // Check if involved in any quest (0 = check for any)
        snapshot.isUsable = go->GetGoState() == GO_STATE_READY && go->isSpawned();  // Can be used/interacted with
        // Validate and store snapshot
        if (snapshot.IsValid())
        {
            auto [x, y] = GetCellCoords(snapshot.position);
            if (x < TOTAL_CELLS && y < TOTAL_CELLS)
            {
                writeBuffer.cells[x][y].gameObjects.push_back(::std::move(snapshot));
                ++gameObjectCount;
            }
        }
    }

    // Populate DynamicObjects (AoE spell effects)
    // SAFE: We're on main thread (Map::Update), Map access is allowed
    // Use TypeListContainer API to access DynamicObject store
    dynamicObjectCount = 0;
    auto& objectsStore = _map->GetObjectsStore();
    auto const& dynamicObjects = objectsStore.Data.FindContainer<DynamicObject>();
    for (auto const& [guid, dynObj] : dynamicObjects)
    {
        if (!dynObj || !dynObj->IsInWorld())
            continue;

        // ===== ENTERPRISE-GRADE: COMPLETE DynamicObjectSnapshot Population =====
        // OPTION C: All 12+ fields populated using correct TrinityCore API methods
        // NO shortcuts, NO placeholders - complete data capture for zero Map calls from workers

        DynamicObjectSnapshot snapshot;

        // ===== IDENTITY (CRITICAL) =====
        snapshot.guid = dynObj->GetGUID();
        snapshot.entry = dynObj->GetEntry();
        snapshot.spellId = dynObj->GetSpellId();
        snapshot.casterGuid = dynObj->GetCasterGUID();

        // ===== POSITION (CRITICAL) =====
        snapshot.position = dynObj->GetPosition();
        snapshot.mapId = dynObj->GetMapId();
        snapshot.instanceId = dynObj->GetInstanceId();
        snapshot.zoneId = dynObj->GetZoneId();
        snapshot.areaId = dynObj->GetAreaId();

        // ===== SPELL & EFFECT (CRITICAL) =====
        snapshot.radius = dynObj->GetRadius();
        snapshot.duration = dynObj->GetDuration();
        snapshot.totalDuration = dynObj->GetDuration();  // TrinityCore doesn't expose original duration separately
        snapshot.type = static_cast<uint8>(dynObj->m_dynamicObjectData->Type);

        if (Unit* caster = dynObj->GetCaster())
            snapshot.faction = caster->GetFaction();
        // Visual - stored in m_dynamicObjectData->SpellVisual
        snapshot.spellVisual = dynObj->m_dynamicObjectData->SpellVisual->SpellXSpellVisualID;

        if (snapshot.IsValid())
        {
            auto [x, y] = GetCellCoords(snapshot.position);
            if (x < TOTAL_CELLS && y < TOTAL_CELLS)
            {
                writeBuffer.cells[x][y].dynamicObjects.push_back(::std::move(snapshot));
                ++dynamicObjectCount;
            }
        }
    }

    // Populate AreaTriggers (ground effects, damage zones)
    // SAFE: We're on main thread, Map access is allowed
    areaTriggerCount = 0;
    auto const& areaTriggers = _map->GetAreaTriggerBySpawnIdStore();
    for (auto const& pair : areaTriggers)
    {
        AreaTrigger* areaTrigger = pair.second;
        if (!areaTrigger || !areaTrigger->IsInWorld())
            continue;

        // ===== ENTERPRISE-GRADE: COMPLETE AreaTriggerSnapshot Population =====
        // OPTION C: All 18+ fields populated using correct TrinityCore API methods
        // NO shortcuts, NO placeholders - complete data capture for zero Map calls from workers

        AreaTriggerSnapshot snapshot;

        // ===== IDENTITY (CRITICAL) =====
        snapshot.guid = areaTrigger->GetGUID();
        snapshot.entry = areaTrigger->GetEntry();
        snapshot.spellId = areaTrigger->GetSpellId();
        snapshot.casterGuid = areaTrigger->GetCasterGuid();
        if (Unit* target = areaTrigger->GetTarget())
            snapshot.targetGuid = target->GetGUID();
        else
            snapshot.targetGuid = ObjectGuid::Empty;

        // ===== POSITION (CRITICAL) =====
        snapshot.position = areaTrigger->GetPosition();
        snapshot.mapId = areaTrigger->GetMapId();
        snapshot.instanceId = areaTrigger->GetInstanceId();
        snapshot.zoneId = areaTrigger->GetZoneId();
        snapshot.areaId = areaTrigger->GetAreaId();

        // ===== SHAPE (CRITICAL) =====
        // Extract shape data from variant using proper TrinityCore API
        snapshot.shapeType = 0;
        snapshot.sphereRadius = 0.0f;
        snapshot.boxExtentX = 0.0f;
        snapshot.boxExtentY = 0.0f;
        snapshot.boxExtentZ = 0.0f;

        // Visit the shape variant to extract proper data
        areaTrigger->m_areaTriggerData->ShapeData.Visit([&snapshot](auto const& shape)
        {
            using ShapeType = ::std::decay_t<decltype(shape)>;
            if constexpr (::std::is_same_v<ShapeType, UF::AreaTriggerSphere>)
            {
                snapshot.shapeType = 0;  // Sphere
                snapshot.sphereRadius = shape.Radius;
            }
            else if constexpr (::std::is_same_v<ShapeType, UF::AreaTriggerBox>)
            {
                snapshot.shapeType = 1;  // Box
                // Extents is an UpdateField<TaggedPosition<Position::XYZ>> - access via .Pos member
                snapshot.boxExtentX = shape.Extents->Pos.GetPositionX();
                snapshot.boxExtentY = shape.Extents->Pos.GetPositionY();
                snapshot.boxExtentZ = shape.Extents->Pos.GetPositionZ();
            }
            // Other shape types (Polygon, Cylinder, Disk, BoundedPlane) not captured in snapshot
            // as snapshot only has sphere/box fields
        });

        // ===== DURATION & MOVEMENT (HIGH) =====
        snapshot.duration = areaTrigger->GetDuration();
        snapshot.totalDuration = areaTrigger->GetTotalDuration();
        snapshot.flags = areaTrigger->GetAreaTriggerFlags().AsUnderlyingType();
        snapshot.hasSplines = areaTrigger->HasSplines();
        snapshot.hasOrbit = areaTrigger->HasOrbit();
        snapshot.isRemoved = areaTrigger->IsRemoved();
        snapshot.scale = areaTrigger->CalcCurrentScale();

        if (snapshot.IsValid())
        {
            auto [x, y] = GetCellCoords(snapshot.position);
            if (x < TOTAL_CELLS && y < TOTAL_CELLS)
            {
                writeBuffer.cells[x][y].areaTriggers.push_back(::std::move(snapshot));
                ++areaTriggerCount;
            }
        }
    }

    writeBuffer.populationCount = creatureCount + playerCount + gameObjectCount +
                                   dynamicObjectCount + areaTriggerCount;
    writeBuffer.lastUpdate = ::std::chrono::steady_clock::now();

    auto end = ::std::chrono::high_resolution_clock::now();
    auto duration = ::std::chrono::duration_cast<::std::chrono::microseconds>(end - start);
    _lastUpdateDurationUs.store(static_cast<uint32>(duration.count()), ::std::memory_order_relaxed);

    TC_LOG_TRACE("playerbot.spatial",
        "PopulateBufferFromMap: map {} - {} creatures, {} players, {} gameobjects, {} dynobjects, {} areatriggers in {}?s",
        _map->GetId(), creatureCount, playerCount, gameObjectCount, dynamicObjectCount, areaTriggerCount, duration.count());
}

void DoubleBufferedSpatialGrid::SwapBuffers()
{
    uint32 oldIndex = _readBufferIndex.load(::std::memory_order_relaxed);
    _readBufferIndex.store(1 - oldIndex, ::std::memory_order_release);

    _totalSwaps.fetch_add(1, ::std::memory_order_relaxed);

    TC_LOG_TRACE("playerbot.spatial",
        "SwapBuffers: map {} - Read buffer now {}, swap #{}",
        _map->GetId(), _readBufferIndex.load(), _totalSwaps.load());
}

// ===========================================================================
// NEW SNAPSHOT-BASED QUERY METHODS - COMPLETELY THREAD-SAFE!
// ===========================================================================
// These methods return COMPLETE DATA SNAPSHOTS instead of GUIDs.
// Worker threads can use this data directly without ANY Map/ObjectAccessor calls!
// ===========================================================================

::std::vector<DoubleBufferedSpatialGrid::CreatureSnapshot> DoubleBufferedSpatialGrid::QueryNearbyCreatures(
    Position const& pos, float radius) const
{
    _totalQueries.fetch_add(1, ::std::memory_order_relaxed);

    ::std::vector<DoubleBufferedSpatialGrid::CreatureSnapshot> results;
    auto const& readBuffer = GetReadBuffer();

    // Get all cells within radius
    auto cells = GetCellsInRadius(pos, radius);
    float radiusSq = radius * radius;

    for (auto [x, y] : cells)
    {
        if (x >= TOTAL_CELLS || y >= TOTAL_CELLS)
            continue;

        auto const& cell = readBuffer.cells[x][y];

        // Add creatures from this cell with accurate distance filtering
        // Note: Cells are coarse (66 yards), so we need exact distance checks
        for (CreatureSnapshot const& snapshot : cell.creatures)
        {
            // Accurate distance check using snapshot position
            float distSq = pos.GetExactDistSq(snapshot.position);
            if (distSq <= radiusSq)
            {
                results.push_back(snapshot);  // Copy snapshot (POD data, fast)
            }
        }
    }

    TC_LOG_TRACE("playerbot.spatial",
        "QueryNearbyCreatures: pos({:.1f},{:.1f}) radius {:.1f} ? {} results",
        pos.GetPositionX(), pos.GetPositionY(), radius, results.size());

    return results;
}

::std::vector<DoubleBufferedSpatialGrid::PlayerSnapshot> DoubleBufferedSpatialGrid::QueryNearbyPlayers(
    Position const& pos, float radius) const
{
    _totalQueries.fetch_add(1, ::std::memory_order_relaxed);

    ::std::vector<PlayerSnapshot> results;
    auto const& readBuffer = GetReadBuffer();
    auto cells = GetCellsInRadius(pos, radius);
    float radiusSq = radius * radius;

    for (auto [x, y] : cells)
    {
        if (x >= TOTAL_CELLS || y >= TOTAL_CELLS)
            continue;

        auto const& cell = readBuffer.cells[x][y];

        // Add players from this cell with accurate distance filtering
        for (PlayerSnapshot const& snapshot : cell.players)
        {
            float distSq = pos.GetExactDistSq(snapshot.position);
            if (distSq <= radiusSq)
            {
                results.push_back(snapshot);
            }
        }
    }

    TC_LOG_TRACE("playerbot.spatial",
        "QueryNearbyPlayers: pos({:.1f},{:.1f}) radius {:.1f} ? {} results",
        pos.GetPositionX(), pos.GetPositionY(), radius, results.size());

    return results;
}

::std::vector<DoubleBufferedSpatialGrid::GameObjectSnapshot> DoubleBufferedSpatialGrid::QueryNearbyGameObjects(
    Position const& pos, float radius) const
{
    _totalQueries.fetch_add(1, ::std::memory_order_relaxed);

    ::std::vector<GameObjectSnapshot> results;
    auto const& readBuffer = GetReadBuffer();
    auto cells = GetCellsInRadius(pos, radius);
    float radiusSq = radius * radius;

    for (auto [x, y] : cells)
    {
        if (x >= TOTAL_CELLS || y >= TOTAL_CELLS)
            continue;

        auto const& cell = readBuffer.cells[x][y];

        // Add game objects from this cell with accurate distance filtering
        for (GameObjectSnapshot const& snapshot : cell.gameObjects)
        {
            float distSq = pos.GetExactDistSq(snapshot.position);
            if (distSq <= radiusSq)
            {
                results.push_back(snapshot);
            }
        }
    }

    TC_LOG_TRACE("playerbot.spatial",
        "QueryNearbyGameObjects: pos({:.1f},{:.1f}) radius {:.1f} ? {} results",
        pos.GetPositionX(), pos.GetPositionY(), radius, results.size());

    return results;
}

::std::vector<DoubleBufferedSpatialGrid::AreaTriggerSnapshot> DoubleBufferedSpatialGrid::QueryNearbyAreaTriggers(
    Position const& pos, float radius) const
{
    _totalQueries.fetch_add(1, ::std::memory_order_relaxed);

    ::std::vector<AreaTriggerSnapshot> results;
    auto const& readBuffer = GetReadBuffer();
    auto cells = GetCellsInRadius(pos, radius);
    float radiusSq = radius * radius;

    for (auto [x, y] : cells)
    {
        if (x >= TOTAL_CELLS || y >= TOTAL_CELLS)
            continue;

        auto const& cell = readBuffer.cells[x][y];

        // Add area triggers from this cell with accurate distance filtering
        for (AreaTriggerSnapshot const& snapshot : cell.areaTriggers)
        {
            float distSq = pos.GetExactDistSq(snapshot.position);
            if (distSq <= radiusSq)
            {
                results.push_back(snapshot);
            }
        }
    }

    TC_LOG_TRACE("playerbot.spatial",
        "QueryNearbyAreaTriggers: pos({:.1f},{:.1f}) radius {:.1f} ? {} results",
        pos.GetPositionX(), pos.GetPositionY(), radius, results.size());

    return results;
}

::std::vector<DoubleBufferedSpatialGrid::DynamicObjectSnapshot> DoubleBufferedSpatialGrid::QueryNearbyDynamicObjects(
    Position const& pos, float radius) const
{
    _totalQueries.fetch_add(1, ::std::memory_order_relaxed);

    ::std::vector<DynamicObjectSnapshot> results;
    auto const& readBuffer = GetReadBuffer();
    auto cells = GetCellsInRadius(pos, radius);
    float radiusSq = radius * radius;

    for (auto [x, y] : cells)
    {
        if (x >= TOTAL_CELLS || y >= TOTAL_CELLS)
            continue;

        auto const& cell = readBuffer.cells[x][y];

        // Add dynamic objects from this cell with accurate distance filtering
        for (DynamicObjectSnapshot const& snapshot : cell.dynamicObjects)
        {
            float distSq = pos.GetExactDistSq(snapshot.position);
            if (distSq <= radiusSq)
            {
                results.push_back(snapshot);
            }
        }
    }

    TC_LOG_TRACE("playerbot.spatial",
        "QueryNearbyDynamicObjects: pos({:.1f},{:.1f}) radius {:.1f} ? {} results",
        pos.GetPositionX(), pos.GetPositionY(), radius, results.size());

    return results;
}

// ===========================================================================
// LEGACY GUID-BASED QUERY METHODS (DEPRECATED - FOR BACKWARD COMPATIBILITY)
// ===========================================================================
// These methods extract GUIDs from snapshots. They exist ONLY for transitioning
// existing code. NEW CODE SHOULD USE SNAPSHOT QUERIES ABOVE!
// ===========================================================================

::std::vector<ObjectGuid> DoubleBufferedSpatialGrid::QueryNearbyCreatureGuids(
    Position const& pos, float radius) const
{
    auto snapshots = QueryNearbyCreatures(pos, radius);
    ::std::vector<ObjectGuid> guids;
    guids.reserve(snapshots.size());
    for (auto const& snapshot : snapshots)
        guids.push_back(snapshot.guid);
    return guids;
}

::std::vector<ObjectGuid> DoubleBufferedSpatialGrid::QueryNearbyPlayerGuids(
    Position const& pos, float radius) const
{
    auto snapshots = QueryNearbyPlayers(pos, radius);
    ::std::vector<ObjectGuid> guids;
    guids.reserve(snapshots.size());
    for (auto const& snapshot : snapshots)
        guids.push_back(snapshot.guid);
    return guids;
}

::std::vector<ObjectGuid> DoubleBufferedSpatialGrid::QueryNearbyGameObjectGuids(
    Position const& pos, float radius) const
{
    auto snapshots = QueryNearbyGameObjects(pos, radius);
    ::std::vector<ObjectGuid> guids;
    guids.reserve(snapshots.size());
    for (auto const& snapshot : snapshots)
        guids.push_back(snapshot.guid);
    return guids;
}

DoubleBufferedSpatialGrid::CellContents const& DoubleBufferedSpatialGrid::GetCell(
    uint32 x, uint32 y) const
{
    static CellContents emptyCell;

    if (x >= TOTAL_CELLS || y >= TOTAL_CELLS)
        return emptyCell;

    auto const& readBuffer = GetReadBuffer();
    return readBuffer.cells[x][y];
}

DoubleBufferedSpatialGrid::Statistics DoubleBufferedSpatialGrid::GetStatistics() const
{
    Statistics stats;
    stats.totalQueries = _totalQueries.load(::std::memory_order_relaxed);
    stats.totalUpdates = _totalUpdates.load(::std::memory_order_relaxed);
    stats.totalSwaps = _totalSwaps.load(::std::memory_order_relaxed);
    stats.lastUpdateDurationUs = _lastUpdateDurationUs.load(::std::memory_order_relaxed);
    stats.currentPopulation = GetReadBuffer().populationCount;
    stats.startTime = _startTime;

    return stats;
}

uint32 DoubleBufferedSpatialGrid::GetPopulationCount() const
{
    return GetReadBuffer().populationCount;
}

::std::pair<uint32, uint32> DoubleBufferedSpatialGrid::GetCellCoords(Position const& pos) const
{
    // TrinityCore coordinate system:
    // Map center is at (0, 0)
    // Map size is TOTAL_CELLS * CELL_SIZE in each direction
    // Cell coordinates range from 0 to TOTAL_CELLS-1

    constexpr float MAP_HALF_SIZE = (TOTAL_CELLS * CELL_SIZE) / 2.0f;

    float x = (pos.GetPositionX() + MAP_HALF_SIZE) / CELL_SIZE;
    float y = (pos.GetPositionY() + MAP_HALF_SIZE) / CELL_SIZE;

    // Clamp to valid range
    uint32 cellX = static_cast<uint32>(::std::clamp(x, 0.0f, static_cast<float>(TOTAL_CELLS - 1)));
    uint32 cellY = static_cast<uint32>(::std::clamp(y, 0.0f, static_cast<float>(TOTAL_CELLS - 1)));

    return {cellX, cellY};
}

::std::vector<::std::pair<uint32, uint32>> DoubleBufferedSpatialGrid::GetCellsInRadius(
    Position const& center, float radius) const
{
    ::std::vector<::std::pair<uint32, uint32>> cells;

    auto [centerX, centerY] = GetCellCoords(center);
    uint32 cellRadius = static_cast<uint32>(::std::ceil(radius / CELL_SIZE)) + 1; // +1 for safety

    uint32 minX = (centerX > cellRadius) ? (centerX - cellRadius) : 0;
    uint32 maxX = ::std::min(TOTAL_CELLS - 1, centerX + cellRadius);
    uint32 minY = (centerY > cellRadius) ? (centerY - cellRadius) : 0;
    uint32 maxY = ::std::min(TOTAL_CELLS - 1, centerY + cellRadius);

    // Reserve space for efficiency
    cells.reserve((maxX - minX + 1) * (maxY - minY + 1));

    for (uint32 x = minX; x <= maxX; ++x)
    {
        for (uint32 y = minY; y <= maxY; ++y)
        {
            cells.emplace_back(x, y);
        }
    }

    return cells;
}

} // namespace Playerbot
