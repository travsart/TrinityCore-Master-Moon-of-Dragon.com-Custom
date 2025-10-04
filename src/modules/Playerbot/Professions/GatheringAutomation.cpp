/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "GatheringAutomation.h"
#include "Player.h"
#include "GameObject.h"
#include "Creature.h"
#include "Item.h"
#include "Bag.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Log.h"
#include "DB2Stores.h"
#include "GameObjectData.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "GridNotifiers.h"
#include "CellImpl.h"
#include "MotionMaster.h"
#include "CreatureData.h"
#include <algorithm>

namespace Playerbot
{

// Singleton instance
GatheringAutomation* GatheringAutomation::instance()
{
    static GatheringAutomation instance;
    return &instance;
}

GatheringAutomation::GatheringAutomation()
{
}

// ============================================================================
// CORE GATHERING MANAGEMENT
// ============================================================================

void GatheringAutomation::Initialize()
{
    TC_LOG_INFO("playerbots", "GatheringAutomation: Initializing gathering automation system...");

    LoadGatheringSpells();
    LoadGatheringNodes();
    InitializeNodeDatabase();

    TC_LOG_INFO("playerbots", "GatheringAutomation: Loaded {} gathering spells, {} node types",
        _gatheringSpells.size(), _nodeTypes.size());
}

void GatheringAutomation::Update(::Player* player, uint32 diff)
{
    if (!player || !IsEnabled(player))
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    uint32 currentTime = getMSTime();

    // Check if enough time has passed since last scan
    if (currentTime - _lastScanTimes[playerGuid] < NODE_SCAN_INTERVAL)
        return;

    _lastScanTimes[playerGuid] = currentTime;

    std::lock_guard<std::mutex> lock(_mutex);

    // Get automation profile
    GatheringAutomationProfile const& profile = GetAutomationProfile(playerGuid);

    // Check inventory space
    if (!HasInventorySpace(player, 1))
    {
        TC_LOG_DEBUG("playerbots", "GatheringAutomation: Player {} has no inventory space", player->GetName());
        return;
    }

    // Scan for nodes based on enabled professions
    std::vector<GatheringNodeInfo> nodes = ScanForNodes(player, profile.detectionRange);

    if (nodes.empty())
        return;

    // Cache detected nodes
    _detectedNodes[playerGuid] = nodes;

    // Find best node to gather
    GatheringNodeInfo const* bestNode = nullptr;
    float bestScore = 0.0f;

    for (auto const& node : nodes)
    {
        if (!CanGatherFromNode(player, node))
            continue;

        // Score based on distance and skill-up chance
        float distance = node.distance;
        float skillUpChance = GetSkillUpChance(player, node);
        float score = skillUpChance / (distance + 1.0f); // Closer + higher skill-up = better

        if (profile.prioritizeSkillUps)
            score *= (1.0f + skillUpChance); // Bonus for skill-ups

        if (score > bestScore)
        {
            bestScore = score;
            bestNode = &node;
        }
    }

    if (bestNode)
    {
        _currentTarget[playerGuid] = *bestNode;
        GatherFromNode(player, *bestNode);
    }
}

void GatheringAutomation::SetEnabled(::Player* player, bool enabled)
{
    if (!player)
        return;

    std::lock_guard<std::mutex> lock(_mutex);
    uint32 playerGuid = player->GetGUID().GetCounter();

    if (_profiles.find(playerGuid) == _profiles.end())
        _profiles[playerGuid] = GatheringAutomationProfile();

    // Enabling/disabling is handled by profile existence check in IsEnabled
    // For explicit enable/disable, we could add a flag to the profile
}

bool GatheringAutomation::IsEnabled(::Player* player) const
{
    if (!player)
        return false;

    std::lock_guard<std::mutex> lock(_mutex);
    uint32 playerGuid = player->GetGUID().GetCounter();

    auto it = _profiles.find(playerGuid);
    if (it == _profiles.end())
        return false;

    return it->second.autoGather;
}

void GatheringAutomation::SetAutomationProfile(uint32 playerGuid, GatheringAutomationProfile const& profile)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _profiles[playerGuid] = profile;
}

GatheringAutomationProfile GatheringAutomation::GetAutomationProfile(uint32 playerGuid) const
{
    std::lock_guard<std::mutex> lock(_mutex);

    auto it = _profiles.find(playerGuid);
    if (it != _profiles.end())
        return it->second;

    return GatheringAutomationProfile(); // Return default profile
}

// ============================================================================
// NODE DETECTION
// ============================================================================

std::vector<GatheringNodeInfo> GatheringAutomation::ScanForNodes(::Player* player, float range)
{
    std::vector<GatheringNodeInfo> allNodes;

    if (!player)
        return allNodes;

    GatheringAutomationProfile const& profile = GetAutomationProfile(player->GetGUID().GetCounter());

    // Scan for mining nodes
    if (profile.gatherMining && ProfessionManager::instance()->HasProfession(player, ProfessionType::MINING))
    {
        auto miningNodes = DetectMiningNodes(player, range);
        allNodes.insert(allNodes.end(), miningNodes.begin(), miningNodes.end());
    }

    // Scan for herb nodes
    if (profile.gatherHerbalism && ProfessionManager::instance()->HasProfession(player, ProfessionType::HERBALISM))
    {
        auto herbNodes = DetectHerbNodes(player, range);
        allNodes.insert(allNodes.end(), herbNodes.begin(), herbNodes.end());
    }

    // Scan for skinnable creatures
    if (profile.gatherSkinning && ProfessionManager::instance()->HasProfession(player, ProfessionType::SKINNING))
    {
        auto skinnableCreatures = DetectSkinnableCreatures(player, range);
        allNodes.insert(allNodes.end(), skinnableCreatures.begin(), skinnableCreatures.end());
    }

    // Scan for fishing pools
    if (profile.gatherFishing && ProfessionManager::instance()->HasProfession(player, ProfessionType::FISHING))
    {
        auto fishingPools = DetectFishingPools(player, range);
        allNodes.insert(allNodes.end(), fishingPools.begin(), fishingPools.end());
    }

    // Sort by distance
    std::sort(allNodes.begin(), allNodes.end(),
        [](GatheringNodeInfo const& a, GatheringNodeInfo const& b)
        {
            return a.distance < b.distance;
        });

    return allNodes;
}

GatheringNodeInfo const* GatheringAutomation::FindNearestNode(::Player* player, ProfessionType profession)
{
    if (!player)
        return nullptr;

    std::lock_guard<std::mutex> lock(_mutex);
    uint32 playerGuid = player->GetGUID().GetCounter();

    auto it = _detectedNodes.find(playerGuid);
    if (it == _detectedNodes.end() || it->second.empty())
        return nullptr;

    // Find nearest node for profession
    for (auto const& node : it->second)
    {
        if (node.profession == profession && CanGatherFromNode(player, node))
            return &node;
    }

    return nullptr;
}

bool GatheringAutomation::CanGatherFromNode(::Player* player, GatheringNodeInfo const& node) const
{
    if (!player || !node.isActive)
        return false;

    // Check profession
    if (!ProfessionManager::instance()->HasProfession(player, node.profession))
        return false;

    // Check skill requirement
    uint16 currentSkill = ProfessionManager::instance()->GetProfessionSkill(player, node.profession);
    if (currentSkill < node.requiredSkill)
        return false;

    // Check distance
    if (node.distance > GATHERING_RANGE * 2.0f) // Allow slightly more than gathering range for pathfinding
        return false;

    return true;
}

uint16 GatheringAutomation::GetRequiredSkillForNode(GatheringNodeInfo const& node) const
{
    return node.requiredSkill;
}

ProfessionType GatheringAutomation::GetProfessionForNode(GatheringNodeInfo const& node) const
{
    return node.profession;
}

// ============================================================================
// GATHERING ACTIONS
// ============================================================================

bool GatheringAutomation::GatherFromNode(::Player* player, GatheringNodeInfo const& node)
{
    if (!player || !ValidateNode(player, node))
        return false;

    // Check if already in gathering range
    if (!IsInGatheringRange(player, node))
    {
        // Path to node
        if (!PathToNode(player, node))
        {
            TC_LOG_DEBUG("playerbots", "GatheringAutomation: Failed to path to node for player {}", player->GetName());
            return false;
        }
        return true; // Pathfinding in progress
    }

    // Cast gathering spell
    if (!CastGatheringSpell(player, node))
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _playerStatistics[player->GetGUID().GetCounter()].failedAttempts++;
        _globalStatistics.failedAttempts++;
        return false;
    }

    // Note: Loot collection happens via loot handler, not here
    TC_LOG_DEBUG("playerbots", "GatheringAutomation: Player {} started gathering from node {}",
        player->GetName(), node.gameObjectEntry);

    return true;
}

bool GatheringAutomation::CastGatheringSpell(::Player* player, GatheringNodeInfo const& node)
{
    if (!player)
        return false;

    uint16 skillLevel = ProfessionManager::instance()->GetProfessionSkill(player, node.profession);
    uint32 spellId = GetGatheringSpellId(node.profession, skillLevel);

    if (spellId == 0)
    {
        TC_LOG_ERROR("playerbots", "GatheringAutomation: No gathering spell found for profession {} skill {}",
            static_cast<uint16>(node.profession), skillLevel);
        return false;
    }

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
    {
        TC_LOG_ERROR("playerbots", "GatheringAutomation: Invalid spell ID {} for gathering", spellId);
        return false;
    }

    // Get target GameObject
    ::GameObject* gameObject = nullptr;
    if (node.nodeType != GatheringNodeType::CREATURE_CORPSE)
    {
        gameObject = ObjectAccessor::GetGameObject(*player, node.guid);
        if (!gameObject)
        {
            TC_LOG_DEBUG("playerbots", "GatheringAutomation: GameObject not found for node");
            return false;
        }
    }

    // Cast spell
    if (gameObject)
        player->CastSpell(gameObject, spellId, false);
    else
    {
        // For skinning, we need the creature
        ::Creature* creature = ObjectAccessor::GetCreature(*player, node.guid);
        if (creature)
            player->CastSpell(creature, spellId, false);
        else
            return false;
    }

    return true;
}

bool GatheringAutomation::LootNode(::Player* player, ::GameObject* node)
{
    if (!player || !node)
        return false;

    // Loot handling is managed by TrinityCore's loot system
    // This method is called after spell cast completes

    std::lock_guard<std::mutex> lock(_mutex);
    uint32 playerGuid = player->GetGUID().GetCounter();

    _playerStatistics[playerGuid].nodesGathered++;
    _globalStatistics.nodesGathered++;

    return true;
}

bool GatheringAutomation::SkinCreature(::Player* player, ::Creature* creature)
{
    if (!player || !creature)
        return false;

    // Check if creature is dead and has loot
    if (creature->IsAlive())
        return false;

    GatheringNodeInfo nodeInfo = CreateNodeInfo(creature);
    return CastGatheringSpell(player, nodeInfo);
}

uint32 GatheringAutomation::GetGatheringSpellId(ProfessionType profession, uint16 skillLevel) const
{
    std::lock_guard<std::mutex> lock(_mutex);

    // Find appropriate spell for skill level
    for (auto const& spellInfo : _gatheringSpells)
    {
        if (spellInfo.profession == profession &&
            skillLevel >= spellInfo.minSkill &&
            skillLevel <= spellInfo.maxSkill)
        {
            return spellInfo.spellId;
        }
    }

    return 0;
}

// ============================================================================
// PATHFINDING INTEGRATION
// ============================================================================

bool GatheringAutomation::PathToNode(::Player* player, GatheringNodeInfo const& node)
{
    if (!player)
        return false;

    // Simple movement - in full implementation, integrate with bot pathfinding system
    // For now, just move towards the node position
    player->GetMotionMaster()->MovePoint(0, node.position);

    return true;
}

bool GatheringAutomation::IsInGatheringRange(::Player* player, GatheringNodeInfo const& node) const
{
    if (!player)
        return false;

    float distance = player->GetDistance(node.position);
    return distance <= GATHERING_RANGE;
}

float GatheringAutomation::GetDistanceToNode(::Player* player, GatheringNodeInfo const& node) const
{
    if (!player)
        return 0.0f;

    return player->GetDistance(node.position);
}

// ============================================================================
// INVENTORY MANAGEMENT
// ============================================================================

bool GatheringAutomation::HasInventorySpace(::Player* player, uint32 requiredSlots) const
{
    if (!player)
        return false;

    return GetFreeBagSlots(player) >= requiredSlots;
}

uint32 GatheringAutomation::GetFreeBagSlots(::Player* player) const
{
    if (!player)
        return 0;

    uint32 freeSlots = 0;

    // Check all bags
    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (::Bag* bag = player->GetBagByPos(i))
            freeSlots += bag->GetFreeSlots();
    }

    // Check main backpack
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        if (!player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            freeSlots++;
    }

    return freeSlots;
}

bool GatheringAutomation::ShouldDepositMaterials(::Player* player) const
{
    if (!player)
        return false;

    GatheringAutomationProfile const& profile = GetAutomationProfile(player->GetGUID().GetCounter());
    uint32 freeSlots = GetFreeBagSlots(player);

    // Deposit if less than reserved slots available
    return freeSlots < profile.maxInventorySlots;
}

// ============================================================================
// STATISTICS
// ============================================================================

GatheringStatistics const& GatheringAutomation::GetPlayerStatistics(uint32 playerGuid) const
{
    std::lock_guard<std::mutex> lock(_mutex);

    static GatheringStatistics emptyStats;
    auto it = _playerStatistics.find(playerGuid);
    if (it != _playerStatistics.end())
        return it->second;

    return emptyStats;
}

GatheringStatistics const& GatheringAutomation::GetGlobalStatistics() const
{
    return _globalStatistics;
}

void GatheringAutomation::ResetStatistics(uint32 playerGuid)
{
    std::lock_guard<std::mutex> lock(_mutex);

    auto it = _playerStatistics.find(playerGuid);
    if (it != _playerStatistics.end())
        it->second.Reset();
}

// ============================================================================
// INITIALIZATION HELPERS
// ============================================================================

void GatheringAutomation::LoadGatheringSpells()
{
    _gatheringSpells.clear();

    // Mining spells
    // Note: These spell IDs should be validated against actual DB2 data
    // This is a simplified implementation - full version would load from SkillLineAbility
    _gatheringSpells.push_back({2575, 1, 75, ProfessionType::MINING});      // Mining (Apprentice)
    _gatheringSpells.push_back({2576, 75, 150, ProfessionType::MINING});    // Mining (Journeyman)
    _gatheringSpells.push_back({3564, 150, 225, ProfessionType::MINING});   // Mining (Expert)
    _gatheringSpells.push_back({10248, 225, 300, ProfessionType::MINING});  // Mining (Artisan)

    // Herbalism spells
    _gatheringSpells.push_back({2366, 1, 75, ProfessionType::HERBALISM});     // Herbalism (Apprentice)
    _gatheringSpells.push_back({2368, 75, 150, ProfessionType::HERBALISM});   // Herbalism (Journeyman)
    _gatheringSpells.push_back({3570, 150, 225, ProfessionType::HERBALISM});  // Herbalism (Expert)
    _gatheringSpells.push_back({11993, 225, 300, ProfessionType::HERBALISM}); // Herbalism (Artisan)

    // Skinning spells
    _gatheringSpells.push_back({8613, 1, 75, ProfessionType::SKINNING});      // Skinning (Apprentice)
    _gatheringSpells.push_back({8617, 75, 150, ProfessionType::SKINNING});    // Skinning (Journeyman)
    _gatheringSpells.push_back({8618, 150, 225, ProfessionType::SKINNING});   // Skinning (Expert)
    _gatheringSpells.push_back({10768, 225, 300, ProfessionType::SKINNING});  // Skinning (Artisan)

    TC_LOG_INFO("playerbots", "GatheringAutomation: Loaded {} gathering spell entries", _gatheringSpells.size());
}

void GatheringAutomation::LoadGatheringNodes()
{
    // This would load from gameobject_template in full implementation
    // For now, we rely on runtime detection via GAMEOBJECT_TYPE_GATHERING_NODE
    TC_LOG_INFO("playerbots", "GatheringAutomation: Node detection will use GAMEOBJECT_TYPE_GATHERING_NODE (50)");
}

void GatheringAutomation::InitializeNodeDatabase()
{
    _nodeTypes.clear();

    // Mining nodes (example entries - should be loaded from database)
    _nodeTypes[2770] = {ProfessionType::MINING, 1, GatheringNodeType::MINING_VEIN};    // Copper Vein
    _nodeTypes[2771] = {ProfessionType::MINING, 65, GatheringNodeType::MINING_VEIN};   // Tin Vein
    _nodeTypes[2772] = {ProfessionType::MINING, 125, GatheringNodeType::MINING_VEIN};  // Iron Deposit

    // Herb nodes (example entries)
    _nodeTypes[1618] = {ProfessionType::HERBALISM, 1, GatheringNodeType::HERB_NODE};   // Peacebloom
    _nodeTypes[1619] = {ProfessionType::HERBALISM, 1, GatheringNodeType::HERB_NODE};   // Silverleaf
    _nodeTypes[1620] = {ProfessionType::HERBALISM, 70, GatheringNodeType::HERB_NODE};  // Mageroyal

    TC_LOG_INFO("playerbots", "GatheringAutomation: Initialized {} node type entries", _nodeTypes.size());
}

// ============================================================================
// NODE DETECTION HELPERS
// ============================================================================

std::vector<GatheringNodeInfo> GatheringAutomation::DetectMiningNodes(::Player* player, float range)
{
    std::vector<GatheringNodeInfo> nodes;

    if (!player)
        return nodes;

    // Find all gathering nodes in range
    std::list<::GameObject*> gameObjects;
    Trinity::AllGameObjectsWithEntryInRange checker(player, 0, range); // 0 = any entry
    Trinity::GameObjectListSearcher<Trinity::AllGameObjectsWithEntryInRange> searcher(player, gameObjects, checker);
    Cell::VisitGridObjects(player, searcher, range);

    for (auto* go : gameObjects)
    {
        if (!go || go->GetGoType() != GAMEOBJECT_TYPE_GATHERING_NODE)
            continue;

        // Check if it's a mining node based on entry
        auto it = _nodeTypes.find(go->GetEntry());
        if (it != _nodeTypes.end() && it->second.profession == ProfessionType::MINING)
        {
            GatheringNodeInfo nodeInfo = CreateNodeInfo(go, ProfessionType::MINING);
            nodes.push_back(nodeInfo);
        }
    }

    return nodes;
}

std::vector<GatheringNodeInfo> GatheringAutomation::DetectHerbNodes(::Player* player, float range)
{
    std::vector<GatheringNodeInfo> nodes;

    if (!player)
        return nodes;

    std::list<::GameObject*> gameObjects;
    Trinity::AllGameObjectsWithEntryInRange checker(player, 0, range);
    Trinity::GameObjectListSearcher<Trinity::AllGameObjectsWithEntryInRange> searcher(player, gameObjects, checker);
    Cell::VisitGridObjects(player, searcher, range);

    for (auto* go : gameObjects)
    {
        if (!go || go->GetGoType() != GAMEOBJECT_TYPE_GATHERING_NODE)
            continue;

        auto it = _nodeTypes.find(go->GetEntry());
        if (it != _nodeTypes.end() && it->second.profession == ProfessionType::HERBALISM)
        {
            GatheringNodeInfo nodeInfo = CreateNodeInfo(go, ProfessionType::HERBALISM);
            nodes.push_back(nodeInfo);
        }
    }

    return nodes;
}

std::vector<GatheringNodeInfo> GatheringAutomation::DetectFishingPools(::Player* player, float range)
{
    std::vector<GatheringNodeInfo> nodes;

    if (!player)
        return nodes;

    // Find fishing pools (GAMEOBJECT_TYPE_FISHINGHOLE)
    ::GameObject* fishingPool = player->FindNearestGameObjectOfType(GAMEOBJECT_TYPE_FISHINGHOLE, range);
    if (fishingPool)
    {
        GatheringNodeInfo nodeInfo;
        nodeInfo.guid = fishingPool->GetGUID();
        nodeInfo.nodeType = GatheringNodeType::FISHING_POOL;
        nodeInfo.gameObjectEntry = fishingPool->GetEntry();
        nodeInfo.position = fishingPool->GetPosition();
        nodeInfo.requiredSkill = 1;
        nodeInfo.profession = ProfessionType::FISHING;
        nodeInfo.detectionTime = getMSTime();
        nodeInfo.isActive = true;
        nodeInfo.distance = player->GetDistance(fishingPool);

        nodes.push_back(nodeInfo);
    }

    return nodes;
}

std::vector<GatheringNodeInfo> GatheringAutomation::DetectSkinnableCreatures(::Player* player, float range)
{
    std::vector<GatheringNodeInfo> nodes;

    if (!player)
        return nodes;

    // Find dead creatures with loot - use AllCreaturesOfEntryInRange with entry=0 for all
    std::list<::Creature*> creatures;
    Trinity::AllCreaturesOfEntryInRange checker(player, 0, range); // 0 = any entry
    Trinity::CreatureListSearcher<Trinity::AllCreaturesOfEntryInRange> searcher(player, creatures, checker);
    Cell::VisitGridObjects(player, searcher, range);

    for (auto* creature : creatures)
    {
        if (!creature || creature->IsAlive())
            continue;

        // Check if creature has skinning loot via difficulty entry
        CreatureDifficulty const* difficulty = creature->GetCreatureTemplate()->GetDifficulty(creature->GetMap()->GetDifficultyID());
        if (difficulty && difficulty->SkinLootID > 0)
        {
            GatheringNodeInfo nodeInfo = CreateNodeInfo(creature);
            nodes.push_back(nodeInfo);
        }
    }

    return nodes;
}

GatheringNodeInfo GatheringAutomation::CreateNodeInfo(::GameObject* gameObject, ProfessionType profession)
{
    GatheringNodeInfo nodeInfo;

    if (!gameObject)
        return nodeInfo;

    nodeInfo.guid = gameObject->GetGUID();
    nodeInfo.nodeType = (profession == ProfessionType::MINING) ? GatheringNodeType::MINING_VEIN : GatheringNodeType::HERB_NODE;
    nodeInfo.gameObjectEntry = gameObject->GetEntry();
    nodeInfo.position = gameObject->GetPosition();
    nodeInfo.profession = profession;
    nodeInfo.detectionTime = getMSTime();
    nodeInfo.isActive = gameObject->isSpawned();

    // Get required skill from node database
    auto it = _nodeTypes.find(gameObject->GetEntry());
    if (it != _nodeTypes.end())
        nodeInfo.requiredSkill = it->second.requiredSkill;
    else
        nodeInfo.requiredSkill = 1;

    return nodeInfo;
}

GatheringNodeInfo GatheringAutomation::CreateNodeInfo(::Creature* creature)
{
    GatheringNodeInfo nodeInfo;

    if (!creature)
        return nodeInfo;

    nodeInfo.guid = creature->GetGUID();
    nodeInfo.nodeType = GatheringNodeType::CREATURE_CORPSE;
    nodeInfo.creatureEntry = creature->GetEntry();
    nodeInfo.position = creature->GetPosition();
    nodeInfo.profession = ProfessionType::SKINNING;
    nodeInfo.detectionTime = getMSTime();
    nodeInfo.isActive = !creature->IsAlive();
    nodeInfo.requiredSkill = 1; // Skinning skill based on creature level

    return nodeInfo;
}

// ============================================================================
// GATHERING HELPERS
// ============================================================================

bool GatheringAutomation::ValidateNode(::Player* player, GatheringNodeInfo const& node)
{
    if (!player || !node.isActive)
        return false;

    // Check if node still exists
    if (node.nodeType == GatheringNodeType::CREATURE_CORPSE)
    {
        ::Creature* creature = ObjectAccessor::GetCreature(*player, node.guid);
        return creature && !creature->IsAlive();
    }
    else
    {
        ::GameObject* gameObject = ObjectAccessor::GetGameObject(*player, node.guid);
        return gameObject && gameObject->isSpawned();
    }
}

float GatheringAutomation::GetSkillUpChance(::Player* player, GatheringNodeInfo const& node) const
{
    if (!player)
        return 0.0f;

    uint16 currentSkill = ProfessionManager::instance()->GetProfessionSkill(player, node.profession);
    uint16 requiredSkill = node.requiredSkill;

    // Simplified skill-up calculation
    // Orange: 100% chance (skill < required + 25)
    // Yellow: 75% chance (skill < required + 50)
    // Green: 25% chance (skill < required + 75)
    // Gray: 0% chance

    if (currentSkill < requiredSkill + 25)
        return 1.0f; // Orange
    else if (currentSkill < requiredSkill + 50)
        return 0.75f; // Yellow
    else if (currentSkill < requiredSkill + 75)
        return 0.25f; // Green
    else
        return 0.0f; // Gray
}

void GatheringAutomation::HandleGatheringResult(::Player* player, GatheringNodeInfo const& node, bool success)
{
    if (!player)
        return;

    std::lock_guard<std::mutex> lock(_mutex);
    uint32 playerGuid = player->GetGUID().GetCounter();

    if (success)
    {
        _playerStatistics[playerGuid].nodesGathered++;
        _globalStatistics.nodesGathered++;

        TC_LOG_DEBUG("playerbots", "GatheringAutomation: Player {} successfully gathered from node {}",
            player->GetName(), node.gameObjectEntry);
    }
    else
    {
        _playerStatistics[playerGuid].failedAttempts++;
        _globalStatistics.failedAttempts++;
    }
}

} // namespace Playerbot
