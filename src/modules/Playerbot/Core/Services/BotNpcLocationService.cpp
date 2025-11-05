/**
 * @file BotNpcLocationService.cpp
 * @brief Implementation of enterprise-grade NPC location resolution service
 *
 * @created 2025-10-27
 * @part-of PlayerBot Core Infrastructure
 */

#include "BotNpcLocationService.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "QuestDef.h"
#include "Creature.h"
#include "GameObject.h"
#include "Log.h"
#include "Map.h"
#include "World.h"
#include "DatabaseEnv.h"
#include "Spatial/SpatialGridQueryHelpers.h"
#include "Spatial/SpatialGridManager.h"
#include <algorithm>
#include <chrono>

namespace Playerbot
{

BotNpcLocationService* BotNpcLocationService::instance()
{
    static BotNpcLocationService instance;
    return &instance;
}

bool BotNpcLocationService::Initialize()
{
    if (_initialized)
    {
        TC_LOG_WARN("module.playerbot.services", "BotNpcLocationService::Initialize() called but already initialized!");
        return true;
    }

    TC_LOG_INFO("module.playerbot.services", "========================================");
    TC_LOG_INFO("module.playerbot.services", "Initializing BotNpcLocationService...");
    TC_LOG_INFO("module.playerbot.services", "========================================");

    auto startTime = std::chrono::steady_clock::now();

    // Build all caches
    BuildCreatureSpawnCache();
    BuildGameObjectSpawnCache();
    BuildProfessionTrainerCache();
    BuildClassTrainerCache();
    BuildServiceNpcCache();
    BuildQuestPOICache();

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    // Log statistics
    CacheStats stats = GetCacheStats();
    TC_LOG_INFO("module.playerbot.services", "========================================");
    TC_LOG_INFO("module.playerbot.services", "BotNpcLocationService initialized successfully!");
    TC_LOG_INFO("module.playerbot.services", "  Creature spawns cached: {}", stats.creatureSpawnsCached);
    TC_LOG_INFO("module.playerbot.services", "  GameObject spawns cached: {}", stats.gameObjectSpawnsCached);
    TC_LOG_INFO("module.playerbot.services", "  Profession trainers: {}", stats.professionTrainersCached);
    TC_LOG_INFO("module.playerbot.services", "  Class trainers: {}", stats.classTrainersCached);
    TC_LOG_INFO("module.playerbot.services", "  Service NPCs: {}", stats.serviceNpcsCached);
    TC_LOG_INFO("module.playerbot.services", "  Quest POIs: {}", stats.questPOIsCached);
    TC_LOG_INFO("module.playerbot.services", "  Maps indexed: {}", stats.mapsIndexed);
    TC_LOG_INFO("module.playerbot.services", "  Initialization time: {} ms", duration.count());
    TC_LOG_INFO("module.playerbot.services", "========================================");

    _initialized = true;
    return true;
}

void BotNpcLocationService::Shutdown()
{
    TC_LOG_INFO("module.playerbot.services", "Shutting down BotNpcLocationService...");

    _creatureSpawnCache.clear();
    _gameObjectSpawnCache.clear();
    _professionTrainerCache.clear();
    _classTrainerCache.clear();
    _serviceNpcCache.clear();
    _questPOICache.clear();

    _initialized = false;

    TC_LOG_INFO("module.playerbot.services", "BotNpcLocationService shutdown complete.");
}

// ===== CACHE BUILDING METHODS =====

void BotNpcLocationService::BuildCreatureSpawnCache()
{
    TC_LOG_INFO("module.playerbot.services", "Building creature spawn cache...");

    auto const& spawnData = sObjectMgr->GetAllCreatureData();
    uint32 cached = 0;

    for (auto const& [guid, data] : spawnData)
    {
        SpawnLocationData location;
        location.position = data.spawnPoint;
        location.entry = data.id;
        location.mapId = data.mapId;
        location.distanceFromOrigin = data.spawnPoint.GetExactDist2d(0.0f, 0.0f);

        // Index by mapId first, then by entry
        _creatureSpawnCache[data.mapId][data.id].push_back(location);
        cached++;
    }

    TC_LOG_INFO("module.playerbot.services", "  Cached {} creature spawns across {} maps",
                cached, _creatureSpawnCache.size());
}

void BotNpcLocationService::BuildGameObjectSpawnCache()
{
    TC_LOG_INFO("module.playerbot.services", "Building GameObject spawn cache...");

    auto const& spawnData = sObjectMgr->GetAllGameObjectData();
    uint32 cached = 0;

    for (auto const& [guid, data] : spawnData)
    {
        SpawnLocationData location;
        location.position = data.spawnPoint;
        location.entry = data.id;
        location.mapId = data.mapId;
        location.distanceFromOrigin = data.spawnPoint.GetExactDist2d(0.0f, 0.0f);

        // Index by mapId first, then by entry
        _gameObjectSpawnCache[data.mapId][data.id].push_back(location);
        cached++;
    }

    TC_LOG_INFO("module.playerbot.services", "  Cached {} GameObject spawns across {} maps",
                cached, _gameObjectSpawnCache.size());
}

void BotNpcLocationService::BuildProfessionTrainerCache()
{
    TC_LOG_INFO("module.playerbot.services", "Building profession trainer cache...");

    // Iterate through creature spawns and identify trainers
    uint32 cached = 0;

    for (auto const& [mapId, entryMap] : _creatureSpawnCache)
    {
        for (auto const& [entry, locations] : entryMap)
        {
            // Check each profession skill
            for (uint32 skillId = 1; skillId < 800; ++skillId)
            {
                if (IsTrainerForSkill(entry, skillId))
                {
                    for (auto const& loc : locations)
                    {
                        _professionTrainerCache[skillId].push_back(loc);
                        cached++;
                    }
                }
            }
        }
    }

    TC_LOG_INFO("module.playerbot.services", "  Cached {} profession trainer locations for {} skills",
                cached, _professionTrainerCache.size());
}

void BotNpcLocationService::BuildClassTrainerCache()
{
    TC_LOG_INFO("module.playerbot.services", "Building class trainer cache...");

    uint32 cached = 0;

    for (auto const& [mapId, entryMap] : _creatureSpawnCache)
    {
        for (auto const& [entry, locations] : entryMap)
        {
            // Check each class
            for (uint8 classId = CLASS_WARRIOR; classId < MAX_CLASSES; ++classId)
            {
                if (IsClassTrainer(entry, classId))
                {
                    for (auto const& loc : locations)
                    {
                        _classTrainerCache[classId].push_back(loc);
                        cached++;
                    }
                }
            }
        }
    }

    TC_LOG_INFO("module.playerbot.services", "  Cached {} class trainer locations for {} classes",
                cached, _classTrainerCache.size());
}

void BotNpcLocationService::BuildServiceNpcCache()
{
    TC_LOG_INFO("module.playerbot.services", "Building service NPC cache...");

    uint32 cached = 0;

    for (auto const& [mapId, entryMap] : _creatureSpawnCache)
    {
        for (auto const& [entry, locations] : entryMap)
        {
            // Check each service type
            for (uint8 serviceType = 0; serviceType <= static_cast<uint8>(NpcServiceType::BATTLEMASTER); ++serviceType)
            {
                if (ProvidesService(entry, static_cast<NpcServiceType>(serviceType)))
                {
                    for (auto const& loc : locations)
                    {
                        _serviceNpcCache[static_cast<NpcServiceType>(serviceType)].push_back(loc);
                        cached++;
                    }
                }
            }
        }
    }

    TC_LOG_INFO("module.playerbot.services", "  Cached {} service NPC locations",
                cached);
}

void BotNpcLocationService::BuildQuestPOICache()
{
    TC_LOG_INFO("module.playerbot.services", "Building Quest POI cache...");

    uint32 cached = 0;

    // Iterate through all quest templates
    auto const& questTemplates = sObjectMgr->GetQuestTemplates();
    for (auto const& [questId, quest] : questTemplates)
    {
        QuestPOIData const* poiData = sObjectMgr->GetQuestPOIData(questId);
        if (!poiData || poiData->Blobs.empty())
            continue;

        // Cache POI positions for each objective
        for (auto const& blob : poiData->Blobs)
        {
            if (blob.Points.empty())
                continue;

            uint32 objectiveIndex = static_cast<uint32>(blob.ObjectiveIndex);

            // Use first POI point as the objective location
            QuestPOIBlobPoint const& point = blob.Points[0];
            Position poiPos;
            poiPos.Relocate(static_cast<float>(point.X), static_cast<float>(point.Y), static_cast<float>(point.Z));

            _questPOICache[questId][objectiveIndex] = poiPos;
            cached++;
        }
    }

    TC_LOG_INFO("module.playerbot.services", "  Cached {} Quest POI locations for {} quests",
                cached, _questPOICache.size());
}

// ===== QUERY METHODS =====

NpcLocationResult BotNpcLocationService::FindQuestObjectiveLocation(Player* bot, uint32 questId, uint32 objectiveIndex)
{
    if (!bot || !_initialized)
        return NpcLocationResult();

    Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
    if (!quest || objectiveIndex >= quest->Objectives.size())
        return NpcLocationResult();

    QuestObjective const& objective = quest->Objectives[objectiveIndex];

    // Try different sources in priority order
    NpcLocationResult result;

    switch (objective.Type)
    {
        case QUEST_OBJECTIVE_MONSTER:
        {
            // Try 1: Live creature in spatial grid (best quality)
            result = TryFindLiveCreature(bot, objective.ObjectID, 500.0f);
            if (result.isValid)
                return result;

            // Try 2: Spawn database cache
            result = FindNearestCreatureSpawn(bot, objective.ObjectID, 500.0f);
            if (result.isValid)
                return result;

            break;
        }

        case QUEST_OBJECTIVE_GAMEOBJECT:
        {
            // Try 1: Live GameObject in spatial grid
            result = TryFindLiveGameObject(bot, objective.ObjectID, 500.0f);
            if (result.isValid)
                return result;

            // Try 2: Spawn database cache
            result = FindNearestGameObjectSpawn(bot, objective.ObjectID, 500.0f);
            if (result.isValid)
                return result;

            break;
        }

        default:
            break;
    }

    // Try 3: Quest POI fallback (lowest quality but always provides something)
    auto questIt = _questPOICache.find(questId);
    if (questIt != _questPOICache.end())
    {
        auto objIt = questIt->second.find(objectiveIndex);
        if (objIt != questIt->second.end())
        {
            result.position = objIt->second;
            result.distance = bot->GetDistance(result.position);
            result.isValid = true;
            result.qualityScore = 60;  // POI quality
            result.sourceName = "QuestPOI";

            TC_LOG_DEBUG("module.playerbot.services", "Found quest objective via POI: Quest {} Objective {} at ({:.1f}, {:.1f}, {:.1f})",
                        questId, objectiveIndex,
                        result.position.GetPositionX(), result.position.GetPositionY(), result.position.GetPositionZ());

            return result;
        }
    }

    // No location found
    TC_LOG_WARN("module.playerbot.services", "Failed to find location for Quest {} Objective {}", questId, objectiveIndex);
    return NpcLocationResult();
}

NpcLocationResult BotNpcLocationService::FindNearestProfessionTrainer(Player* bot, uint32 skillId)
{
    if (!bot || !_initialized)
        return NpcLocationResult();

    auto it = _professionTrainerCache.find(skillId);
    if (it == _professionTrainerCache.end() || it->second.empty())
    {
        TC_LOG_DEBUG("module.playerbot.services", "No profession trainers found for skill {}", skillId);
        return NpcLocationResult();
    }

    return FindNearestFromCache(bot, it->second, 999999.0f, "ProfessionTrainerCache");
}

NpcLocationResult BotNpcLocationService::FindNearestClassTrainer(Player* bot, uint8 classId)
{
    if (!bot || !_initialized)
        return NpcLocationResult();

    auto it = _classTrainerCache.find(classId);
    if (it == _classTrainerCache.end() || it->second.empty())
    {
        TC_LOG_DEBUG("module.playerbot.services", "No class trainers found for class {}", classId);
        return NpcLocationResult();
    }

    return FindNearestFromCache(bot, it->second, 999999.0f, "ClassTrainerCache");
}

NpcLocationResult BotNpcLocationService::FindNearestService(Player* bot, NpcServiceType serviceType)
{
    if (!bot || !_initialized)
        return NpcLocationResult();

    auto it = _serviceNpcCache.find(serviceType);
    if (it == _serviceNpcCache.end() || it->second.empty())
    {
        TC_LOG_DEBUG("module.playerbot.services", "No service NPCs found for type {}", static_cast<uint8>(serviceType));
        return NpcLocationResult();
    }

    return FindNearestFromCache(bot, it->second, 999999.0f, "ServiceNpcCache");
}

NpcLocationResult BotNpcLocationService::FindNearestCreatureSpawn(Player* bot, uint32 creatureEntry, float maxRange)
{
    if (!bot || !_initialized)
        return NpcLocationResult();

    uint32 botMapId = bot->GetMapId();
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetMapId");
        return;
    }

    // Check if we have spawn data for this map
    auto mapIt = _creatureSpawnCache.find(botMapId);
    if (mapIt == _creatureSpawnCache.end())
        return NpcLocationResult();

    // Check if we have this creature on this map
    auto entryIt = mapIt->second.find(creatureEntry);
    if (entryIt == mapIt->second.end() || entryIt->second.empty())
        return NpcLocationResult();

    return FindNearestFromCache(bot, entryIt->second, maxRange, "CreatureSpawnCache");
}

NpcLocationResult BotNpcLocationService::FindNearestGameObjectSpawn(Player* bot, uint32 objectEntry, float maxRange)
{
    if (!bot || !_initialized)
        return NpcLocationResult();

    uint32 botMapId = bot->GetMapId();
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetMapId");
        return;
    }

    // Check if we have spawn data for this map
    auto mapIt = _gameObjectSpawnCache.find(botMapId);
    if (mapIt == _gameObjectSpawnCache.end())
        return NpcLocationResult();

    // Check if we have this object on this map
    auto entryIt = mapIt->second.find(objectEntry);
    if (entryIt == mapIt->second.end() || entryIt->second.empty())
        return NpcLocationResult();

    return FindNearestFromCache(bot, entryIt->second, maxRange, "GameObjectSpawnCache");
}

BotNpcLocationService::CacheStats BotNpcLocationService::GetCacheStats() const
{
    CacheStats stats;

    // Count creature spawns
    for (auto const& [mapId, entryMap] : _creatureSpawnCache)
    {
        for (auto const& [entry, locations] : entryMap)
            stats.creatureSpawnsCached += static_cast<uint32>(locations.size());
    }

    // Count GameObject spawns
    for (auto const& [mapId, entryMap] : _gameObjectSpawnCache)
    {
        for (auto const& [entry, locations] : entryMap)
            stats.gameObjectSpawnsCached += static_cast<uint32>(locations.size());
    }

    // Count profession trainers
    for (auto const& [skillId, locations] : _professionTrainerCache)
        stats.professionTrainersCached += static_cast<uint32>(locations.size());

    // Count class trainers
    for (auto const& [classId, locations] : _classTrainerCache)
        stats.classTrainersCached += static_cast<uint32>(locations.size());

    // Count service NPCs
    for (auto const& [serviceType, locations] : _serviceNpcCache)
        stats.serviceNpcsCached += static_cast<uint32>(locations.size());

    // Count quest POIs
    for (auto const& [questId, objectives] : _questPOICache)
        stats.questPOIsCached += static_cast<uint32>(objectives.size());

    stats.mapsIndexed = static_cast<uint32>(_creatureSpawnCache.size());

    return stats;
}

// ===== HELPER METHODS =====

NpcLocationResult BotNpcLocationService::FindNearestFromCache(
    Player* bot,
    std::vector<SpawnLocationData> const& locations,
    float maxRange,
    std::string const& sourceName)
{
    NpcLocationResult result;

    float closestDistance = maxRange;
    SpawnLocationData const* closestLocation = nullptr;

    for (auto const& loc : locations)
    {
        // Filter by bot's map
        if (loc.mapId != bot->GetMapId())
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetMapId");
            return nullptr;
        }
            continue;

        float distance = bot->GetDistance(loc.position);
        if (distance < closestDistance)
        {
            closestDistance = distance;
            closestLocation = &loc;
        }
    }

    if (closestLocation)
    {
        result.position = closestLocation->position;
        result.entry = closestLocation->entry;
        result.distance = closestDistance;
        result.isValid = true;
        result.isLiveEntity = false;
        result.qualityScore = 80;  // Cached spawn quality
        result.sourceName = sourceName;

        TC_LOG_DEBUG("module.playerbot.services", "Found nearest via {}: Entry {} at distance {:.1f}",
                    sourceName, result.entry, result.distance);
    }

    return result;
}

bool BotNpcLocationService::IsTrainerForSkill(uint32 creatureEntry, uint32 skillId)
{
    // Get creature template
    CreatureTemplate const* cInfo = sObjectMgr->GetCreatureTemplate(creatureEntry);
    if (!cInfo)
        return false;

    // Check if creature has trainer flag
    if (!(cInfo->npcflag & UNIT_NPC_FLAG_TRAINER))
        return false;

    // TODO: Implement full trainer spell checking via TrainerSpell table
    // For now, return true if it's a trainer - better filtering can be added later
    return true;
}

bool BotNpcLocationService::IsClassTrainer(uint32 creatureEntry, uint8 classId)
{
    CreatureTemplate const* cInfo = sObjectMgr->GetCreatureTemplate(creatureEntry);
    if (!cInfo)
        return false;

    // Check if creature has trainer or class trainer flag
    if (!(cInfo->npcflag & UNIT_NPC_FLAG_TRAINER) && !(cInfo->npcflag & UNIT_NPC_FLAG_TRAINER_CLASS))
        return false;

    // TODO: Implement class-specific trainer checking via TrainerSpell table
    // For now, return true if it's a class trainer
    return true;
}

bool BotNpcLocationService::ProvidesService(uint32 creatureEntry, NpcServiceType serviceType)
{
    CreatureTemplate const* cInfo = sObjectMgr->GetCreatureTemplate(creatureEntry);
    if (!cInfo)
        return false;

    // Check NPC flags for service types
    switch (serviceType)
    {
        case NpcServiceType::INNKEEPER:
            return cInfo->npcflag & UNIT_NPC_FLAG_INNKEEPER;
        case NpcServiceType::VENDOR_GENERAL:
        case NpcServiceType::VENDOR_FOOD:
            return cInfo->npcflag & UNIT_NPC_FLAG_VENDOR;
        case NpcServiceType::VENDOR_REPAIR:
            return cInfo->npcflag & UNIT_NPC_FLAG_REPAIR;
        case NpcServiceType::BANKER:
            return cInfo->npcflag & UNIT_NPC_FLAG_BANKER;
        case NpcServiceType::AUCTIONEER:
            return cInfo->npcflag & UNIT_NPC_FLAG_AUCTIONEER;
        case NpcServiceType::FLIGHT_MASTER:
            return cInfo->npcflag & UNIT_NPC_FLAG_FLIGHTMASTER;
        case NpcServiceType::STABLE_MASTER:
            return cInfo->npcflag & UNIT_NPC_FLAG_STABLEMASTER;
        case NpcServiceType::GUILD_MASTER:
            return cInfo->npcflag & UNIT_NPC_FLAG_PETITIONER;
        case NpcServiceType::QUEST_GIVER:
            return cInfo->npcflag & UNIT_NPC_FLAG_QUESTGIVER;
        case NpcServiceType::SPIRIT_HEALER:
            return cInfo->npcflag & UNIT_NPC_FLAG_SPIRIT_HEALER;
        case NpcServiceType::BATTLEMASTER:
            return cInfo->npcflag & UNIT_NPC_FLAG_BATTLEMASTER;
        default:
            return false;
    }
}

NpcLocationResult BotNpcLocationService::TryFindLiveCreature(Player* bot, uint32 creatureEntry, float maxRange)
{
    NpcLocationResult result;

    // Use spatial grid to find live creatures (best quality - spawned and available NOW)
    std::list<Creature*> nearbyCreatures;
    bot->GetCreatureListWithEntryInGrid(nearbyCreatures, creatureEntry, maxRange);

    Creature* closest = nullptr;
    float closestDistance = maxRange;

    for (Creature* creature : nearbyCreatures)
    {
        if (!creature || !creature->IsAlive())
            continue;

        float distance = bot->GetDistance(creature);
        if (distance < closestDistance)
        {
            closestDistance = distance;
            closest = creature;
        }
    }

    if (closest)
    {
        result.position = closest->GetPosition();
        result.entry = creatureEntry;
        result.guid = closest->GetGUID();
        if (!closest)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: closest in method GetGUID");
            return;
        }
        result.distance = closestDistance;
        result.isValid = true;
        result.isLiveEntity = true;
        result.qualityScore = 100;  // Live entity - highest quality
        result.sourceName = "LiveCreature";

        TC_LOG_DEBUG("module.playerbot.services", "Found LIVE creature: Entry {} at distance {:.1f}",
                    result.entry, result.distance);
    }

    return result;
}

NpcLocationResult BotNpcLocationService::TryFindLiveGameObject(Player* bot, uint32 objectEntry, float maxRange)
{
    NpcLocationResult result;

    // Use spatial grid to find live GameObjects
    std::list<GameObject*> nearbyObjects;
    bot->GetGameObjectListWithEntryInGrid(nearbyObjects, objectEntry, maxRange);

    GameObject* closest = nullptr;
    float closestDistance = maxRange;

    for (GameObject* object : nearbyObjects)
    {
        if (!object)
            continue;

        float distance = bot->GetDistance(object);
        if (distance < closestDistance)
        {
            closestDistance = distance;
            closest = object;
        }
    }

    if (closest)
    {
        result.position = closest->GetPosition();
        result.entry = objectEntry;
        result.guid = closest->GetGUID();
        result.distance = closestDistance;
        result.isValid = true;
        result.isLiveEntity = true;
        result.qualityScore = 100;  // Live entity - highest quality
        result.sourceName = "LiveGameObject";

        TC_LOG_DEBUG("module.playerbot.services", "Found LIVE GameObject: Entry {} at distance {:.1f}",
                    result.entry, result.distance);
    }

    return result;
}

} // namespace Playerbot
