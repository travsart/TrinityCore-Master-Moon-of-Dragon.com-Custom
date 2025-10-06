/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "GatheringManager.h"
#include "AI/BotAI.h"
#include "Player.h"
#include "GameObject.h"
#include "Creature.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "World.h"
#include "Map.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "GameObjectAI.h"
#include "MotionMaster.h"
#include "MovementDefines.h"
#include "UpdateFields.h"
#include <algorithm>

namespace Playerbot
{

GatheringManager::GatheringManager(Player* bot, BotAI* ai)
    : BehaviorManager(bot, ai, 1000, "GatheringManager")  // 1 second update interval
{
    // Configuration will be loaded in OnInitialize
}

bool GatheringManager::OnInitialize()
{
    if (!GetBot() || !GetBot()->IsInWorld())
        return false;

    // Check which gathering professions the bot has
    _gatherMining = GetProfessionSkill(GatheringNodeType::MINING_VEIN) > 0;
    _gatherHerbalism = GetProfessionSkill(GatheringNodeType::HERB_NODE) > 0;
    _gatherSkinning = GetProfessionSkill(GatheringNodeType::CREATURE_CORPSE) > 0;
    _gatherFishing = GetProfessionSkill(GatheringNodeType::FISHING_POOL) > 0;

    TC_LOG_DEBUG("bot.playerbot", "GatheringManager initialized for bot %s - Mining: %s, Herbalism: %s, Skinning: %s, Fishing: %s",
        GetBot()->GetName().c_str(),
        _gatherMining ? "Yes" : "No",
        _gatherHerbalism ? "Yes" : "No",
        _gatherSkinning ? "Yes" : "No",
        _gatherFishing ? "Yes" : "No");

    return true;
}

void GatheringManager::OnShutdown()
{
    // Clean up any ongoing gathering
    if (_isGathering.load())
    {
        StopGathering();
    }

    // Clear detected nodes
    {
        std::lock_guard<std::mutex> lock(_nodeMutex);
        _detectedNodes.clear();
    }

    _detectedNodeCount.store(0, std::memory_order_release);
    _hasNearbyResources.store(false, std::memory_order_release);
}

void GatheringManager::OnUpdate(uint32 elapsed)
{
    if (!GetBot() || !GetBot()->IsInWorld() || !_gatheringEnabled)
        return;

    // Update node detection every few seconds
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastScanTime).count() >= NODE_SCAN_INTERVAL)
    {
        UpdateDetectedNodes();
        _lastScanTime = now;
    }

    // Process current gathering action
    if (_isGathering.load())
    {
        ProcessCurrentGathering();
    }
    // If not gathering and nodes are available, select best node
    else if (_hasNearbyResources.load() && !GetBot()->IsInCombat())
    {
        GatheringNode const* bestNode = SelectBestNode();
        if (bestNode && CanGatherFromNode(*bestNode))
        {
            if (IsInGatheringRange(*bestNode))
            {
                GatherFromNode(*bestNode);
            }
            else if (_gatherWhileMoving)
            {
                PathToNode(*bestNode);
            }
        }
    }

    // Clean up expired nodes
    CleanupExpiredNodes();

    // Update state flags
    _detectedNodeCount.store(static_cast<uint32>(_detectedNodes.size()), std::memory_order_release);
    _hasNearbyResources.store(!_detectedNodes.empty(), std::memory_order_release);
}

std::vector<GatheringNode> GatheringManager::ScanForNodes(float range)
{
    std::vector<GatheringNode> nodes;

    if (!GetBot() || !GetBot()->IsInWorld())
        return nodes;

    // Scan for different node types based on professions
    if (_gatherMining)
    {
        auto miningNodes = DetectMiningNodes(range);
        nodes.insert(nodes.end(), miningNodes.begin(), miningNodes.end());
    }

    if (_gatherHerbalism)
    {
        auto herbNodes = DetectHerbNodes(range);
        nodes.insert(nodes.end(), herbNodes.begin(), herbNodes.end());
    }

    if (_gatherFishing)
    {
        auto fishingPools = DetectFishingPools(range);
        nodes.insert(nodes.end(), fishingPools.begin(), fishingPools.end());
    }

    if (_gatherSkinning)
    {
        auto skinnableCreatures = DetectSkinnableCreatures(range);
        nodes.insert(nodes.end(), skinnableCreatures.begin(), skinnableCreatures.end());
    }

    // Sort by distance
    std::sort(nodes.begin(), nodes.end(),
        [](GatheringNode const& a, GatheringNode const& b) { return a.distance < b.distance; });

    return nodes;
}

GatheringNode const* GatheringManager::FindNearestNode(GatheringNodeType nodeType) const
{
    std::lock_guard<std::mutex> lock(_nodeMutex);

    GatheringNode const* nearest = nullptr;
    float minDistance = std::numeric_limits<float>::max();

    for (auto const& node : _detectedNodes)
    {
        if (nodeType != GatheringNodeType::NONE && node.nodeType != nodeType)
            continue;

        if (node.distance < minDistance && node.isActive)
        {
            minDistance = node.distance;
            nearest = &node;
        }
    }

    return nearest;
}

bool GatheringManager::CanGatherFromNode(GatheringNode const& node) const
{
    if (!GetBot())
        return false;

    // Check if we have the required skill level
    uint16 currentSkill = GetProfessionSkill(node.nodeType);
    if (currentSkill < node.requiredSkill)
        return false;

    // Check if node type is enabled
    if (!IsProfessionEnabled(node.nodeType))
        return false;

    return true;
}

uint16 GatheringManager::GetRequiredSkillForNode(GatheringNode const& node) const
{
    return node.requiredSkill;
}

bool GatheringManager::GatherFromNode(GatheringNode const& node)
{
    if (!GetBot() || !CanGatherFromNode(node))
        return false;

    // Set gathering state
    _isGathering.store(true, std::memory_order_release);
    _currentNodeGuid = node.guid;
    _currentTarget = &node;
    _gatheringStartTime = std::chrono::steady_clock::now();
    _gatheringAttempts++;

    // Cast appropriate gathering spell
    return CastGatheringSpell(node);
}

bool GatheringManager::CastGatheringSpell(GatheringNode const& node)
{
    if (!GetBot())
        return false;

    uint32 spellId = GetGatheringSpellId(node.nodeType, GetProfessionSkill(node.nodeType));
    if (!spellId)
    {
        TC_LOG_DEBUG("bot.playerbot", "GatheringManager: No gathering spell for node type %u", static_cast<uint32>(node.nodeType));
        return false;
    }

    // Stop any current casting
    if (GetBot()->IsNonMeleeSpellCast(false))
        GetBot()->InterruptNonMeleeSpells(false);

    // Get the target object
    Unit* target = nullptr;
    if (node.nodeType == GatheringNodeType::CREATURE_CORPSE)
    {
        target = ObjectAccessor::GetCreature(*GetBot(), node.guid);
    }

    // Cast the gathering spell
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
    {
        TC_LOG_ERROR("bot.playerbot", "GatheringManager: Invalid spell ID %u for gathering", spellId);
        return false;
    }

    if (node.nodeType == GatheringNodeType::CREATURE_CORPSE && target)
    {
        GetBot()->CastSpell(target, spellId, false);
    }
    else
    {
        // For game objects, we need to use a different approach
        GameObject* gameObject = ObjectAccessor::GetGameObject(*GetBot(), node.guid);
        if (gameObject)
        {
            gameObject->Use(GetBot());
            return true;
        }
    }

    _currentSpellId = spellId;
    return true;
}

bool GatheringManager::LootNode(GameObject* gameObject)
{
    if (!GetBot() || !gameObject)
        return false;

    // The game object's loot is handled by the core
    // We just need to mark that we're looting
    _isLooting = true;

    // Record statistics
    _statistics.nodesGathered++;

    return true;
}

bool GatheringManager::SkinCreature(Creature* creature)
{
    if (!GetBot() || !creature || !creature->HasUnitFlag(UNIT_FLAG_SKINNABLE))
        return false;

    // Cast skinning spell on the creature
    GetBot()->CastSpell(creature, SPELL_SKINNING, false);

    // Record statistics
    _statistics.nodesGathered++;

    return true;
}

uint32 GatheringManager::GetGatheringSpellId(GatheringNodeType nodeType, uint16 skillLevel) const
{
    switch (nodeType)
    {
        case GatheringNodeType::MINING_VEIN:
            return SPELL_MINING;
        case GatheringNodeType::HERB_NODE:
            return SPELL_HERB_GATHERING;
        case GatheringNodeType::CREATURE_CORPSE:
            return SPELL_SKINNING;
        case GatheringNodeType::FISHING_POOL:
            return SPELL_FISHING;
        default:
            return 0;
    }
}

bool GatheringManager::PathToNode(GatheringNode const& node)
{
    if (!GetBot() || !GetAI())
        return false;

    // Store return position if needed
    if (_returnToPathAfterGather)
    {
        _returnPosition = GetBot()->GetPosition();
    }

    // Move to the node
    GetAI()->MoveTo(node.position.GetPositionX(), node.position.GetPositionY(), node.position.GetPositionZ());

    _isMovingToNode.store(true, std::memory_order_release);
    _currentTarget = &node;

    TC_LOG_DEBUG("bot.playerbot", "GatheringManager: Moving to node at %.2f, %.2f, %.2f",
        node.position.GetPositionX(), node.position.GetPositionY(), node.position.GetPositionZ());

    return true;
}

bool GatheringManager::IsInGatheringRange(GatheringNode const& node) const
{
    if (!GetBot())
        return false;

    float range = (node.nodeType == GatheringNodeType::CREATURE_CORPSE) ? SKINNING_RANGE : GATHERING_RANGE;
    return GetBot()->GetDistance(node.position) <= range;
}

float GatheringManager::GetDistanceToNode(GatheringNode const& node) const
{
    if (!GetBot())
        return std::numeric_limits<float>::max();

    return GetBot()->GetDistance(node.position);
}

void GatheringManager::StopGathering()
{
    _isGathering.store(false, std::memory_order_release);
    _isMovingToNode.store(false, std::memory_order_release);
    _currentNodeGuid.Clear();
    _currentTarget = nullptr;
    _currentSpellId = 0;
    _isLooting = false;
    _gatheringAttempts = 0;

    // Stop any current casting
    if (GetBot() && GetBot()->IsNonMeleeSpellCast(false))
        GetBot()->InterruptNonMeleeSpells(false);

    // Return to original position if configured
    if (_returnToPathAfterGather && _returnPosition.IsPositionValid() && GetAI())
    {
        GetAI()->MoveTo(_returnPosition.GetPositionX(), _returnPosition.GetPositionY(), _returnPosition.GetPositionZ());
    }
}

void GatheringManager::SetProfessionEnabled(GatheringNodeType nodeType, bool enable)
{
    switch (nodeType)
    {
        case GatheringNodeType::MINING_VEIN:
            _gatherMining = enable;
            break;
        case GatheringNodeType::HERB_NODE:
            _gatherHerbalism = enable;
            break;
        case GatheringNodeType::CREATURE_CORPSE:
            _gatherSkinning = enable;
            break;
        case GatheringNodeType::FISHING_POOL:
            _gatherFishing = enable;
            break;
        default:
            break;
    }
}

bool GatheringManager::IsProfessionEnabled(GatheringNodeType nodeType) const
{
    switch (nodeType)
    {
        case GatheringNodeType::MINING_VEIN:
            return _gatherMining;
        case GatheringNodeType::HERB_NODE:
            return _gatherHerbalism;
        case GatheringNodeType::CREATURE_CORPSE:
            return _gatherSkinning;
        case GatheringNodeType::FISHING_POOL:
            return _gatherFishing;
        default:
            return false;
    }
}

std::vector<GatheringNode> GatheringManager::DetectMiningNodes(float range)
{
    std::vector<GatheringNode> nodes;

    if (!GetBot() || !GetBot()->GetMap())
        return nodes;

    // Search for mining veins (GameObject type 3)
    std::list<GameObject*> gameObjects;
    Trinity::AllWorldObjectsInRange check(GetBot(), range);
    Trinity::GameObjectListSearcher searcher(GetBot(), gameObjects, check);
    Cell::VisitGridObjects(GetBot(), searcher, range);

    for (GameObject* go : gameObjects)
    {
        if (!go || go->GetGoType() != GAMEOBJECT_TYPE_CHEST)
            continue;

        // Check if it's a mining vein (simplified check - real implementation would check specific entries)
        GameObjectTemplate const* goInfo = go->GetGOInfo();
        if (!goInfo)
            continue;

        // Mining veins typically have chest loot
        if (goInfo->chest.chestLoot > 0)
        {
            GatheringNode node = CreateNodeInfo(go, GatheringNodeType::MINING_VEIN);
            if (node.isActive)
                nodes.push_back(node);
        }
    }

    return nodes;
}

std::vector<GatheringNode> GatheringManager::DetectHerbNodes(float range)
{
    std::vector<GatheringNode> nodes;

    if (!GetBot() || !GetBot()->GetMap())
        return nodes;

    // Search for herb nodes (GameObject type 3)
    std::list<GameObject*> gameObjects;
    Trinity::AllWorldObjectsInRange check(GetBot(), range);
    Trinity::GameObjectListSearcher searcher(GetBot(), gameObjects, check);
    Cell::VisitGridObjects(GetBot(), searcher, range);

    for (GameObject* go : gameObjects)
    {
        if (!go || go->GetGoType() != GAMEOBJECT_TYPE_CHEST)
            continue;

        // Check if it's an herb node (simplified check)
        GameObjectTemplate const* goInfo = go->GetGOInfo();
        if (!goInfo)
            continue;

        // Herb nodes typically have chest loot
        if (goInfo->chest.chestLoot > 0)
        {
            // Additional check could be done here based on entry ID ranges
            GatheringNode node = CreateNodeInfo(go, GatheringNodeType::HERB_NODE);
            if (node.isActive)
                nodes.push_back(node);
        }
    }

    return nodes;
}

std::vector<GatheringNode> GatheringManager::DetectFishingPools(float range)
{
    std::vector<GatheringNode> nodes;

    if (!GetBot() || !GetBot()->GetMap())
        return nodes;

    // Search for fishing pools (GameObject type 25)
    std::list<GameObject*> gameObjects;
    Trinity::AllWorldObjectsInRange check(GetBot(), range);
    Trinity::GameObjectListSearcher searcher(GetBot(), gameObjects, check);
    Cell::VisitGridObjects(GetBot(), searcher, range);

    for (GameObject* go : gameObjects)
    {
        if (!go || go->GetGoType() != GAMEOBJECT_TYPE_FISHINGHOLE)
            continue;

        GatheringNode node = CreateNodeInfo(go, GatheringNodeType::FISHING_POOL);
        if (node.isActive)
            nodes.push_back(node);
    }

    return nodes;
}

std::vector<GatheringNode> GatheringManager::DetectSkinnableCreatures(float range)
{
    std::vector<GatheringNode> nodes;

    if (!GetBot() || !GetBot()->GetMap())
        return nodes;

    // Search for dead, skinnable creatures
    std::list<Creature*> creatures;
    Trinity::AnyUnitInObjectRangeCheck check(GetBot(), range);
    Trinity::CreatureListSearcher<Trinity::AnyUnitInObjectRangeCheck> searcher(GetBot(), creatures, check);
    Cell::VisitGridObjects(GetBot(), searcher, range);

    for (Creature* creature : creatures)
    {
        if (!creature || !creature->isDead())
            continue;

        // Check if creature is skinnable
        if (creature->HasUnitFlag(UNIT_FLAG_SKINNABLE))
        {
            GatheringNode node = CreateNodeInfo(creature);
            if (node.isActive)
                nodes.push_back(node);
        }
    }

    return nodes;
}

GatheringNode GatheringManager::CreateNodeInfo(GameObject* gameObject, GatheringNodeType nodeType)
{
    GatheringNode node;

    if (!gameObject)
        return node;

    node.guid = gameObject->GetGUID();
    node.nodeType = nodeType;
    node.entry = gameObject->GetEntry();
    node.position = gameObject->GetPosition();
    node.requiredSkill = 1;  // Would need proper skill requirement lookup
    node.detectionTime = getMSTime();
    node.isActive = gameObject->isSpawned() && gameObject->GetGoState() == GO_STATE_READY;
    node.distance = GetBot() ? GetBot()->GetDistance(gameObject) : 0.0f;

    return node;
}

GatheringNode GatheringManager::CreateNodeInfo(Creature* creature)
{
    GatheringNode node;

    if (!creature)
        return node;

    node.guid = creature->GetGUID();
    node.nodeType = GatheringNodeType::CREATURE_CORPSE;
    node.entry = creature->GetEntry();
    node.position = creature->GetPosition();
    node.requiredSkill = 1;  // Would need proper skill requirement lookup
    node.detectionTime = getMSTime();
    node.isActive = creature->isDead() && creature->HasUnitFlag(UNIT_FLAG_SKINNABLE);
    node.distance = GetBot() ? GetBot()->GetDistance(creature) : 0.0f;

    return node;
}

bool GatheringManager::ValidateNode(GatheringNode const& node)
{
    if (!GetBot() || !GetBot()->GetMap())
        return false;

    if (node.nodeType == GatheringNodeType::CREATURE_CORPSE)
    {
        Creature* creature = ObjectAccessor::GetCreature(*GetBot(), node.guid);
        return creature && creature->isDead() && creature->HasUnitFlag(UNIT_FLAG_SKINNABLE);
    }
    else
    {
        GameObject* gameObject = ObjectAccessor::GetGameObject(*GetBot(), node.guid);
        return gameObject && gameObject->isSpawned() && gameObject->GetGoState() == GO_STATE_READY;
    }
}

float GatheringManager::GetSkillUpChance(GatheringNode const& node) const
{
    uint16 currentSkill = GetProfessionSkill(node.nodeType);
    if (currentSkill >= node.requiredSkill + 100)
        return 0.0f;  // Grey - no skill-up

    if (currentSkill >= node.requiredSkill + 50)
        return 0.25f;  // Green - low chance

    if (currentSkill >= node.requiredSkill + 25)
        return 0.5f;  // Yellow - medium chance

    return 1.0f;  // Orange/Red - guaranteed or high chance
}

void GatheringManager::HandleGatheringResult(GatheringNode const& node, bool success)
{
    if (success)
    {
        _statistics.nodesGathered++;

        // Calculate skill-up chance
        float skillUpChance = GetSkillUpChance(node);
        if (skillUpChance > 0.0f && (rand_norm() < skillUpChance))
        {
            _statistics.skillPointsGained++;
        }
    }
    else
    {
        _statistics.failedAttempts++;
    }

    // Mark node as inactive if successfully gathered
    if (success)
    {
        std::lock_guard<std::mutex> lock(_nodeMutex);
        auto it = std::find_if(_detectedNodes.begin(), _detectedNodes.end(),
            [&node](GatheringNode const& n) { return n.guid == node.guid; });

        if (it != _detectedNodes.end())
        {
            it->isActive = false;
        }
    }

    StopGathering();
}

void GatheringManager::UpdateDetectedNodes()
{
    std::lock_guard<std::mutex> lock(_nodeMutex);

    // Clear old nodes
    _detectedNodes.clear();

    // Scan for new nodes
    auto newNodes = ScanForNodes(_detectionRange);
    _detectedNodes = std::move(newNodes);

    TC_LOG_DEBUG("bot.playerbot", "GatheringManager: Detected %zu gathering nodes", _detectedNodes.size());
}

GatheringNode const* GatheringManager::SelectBestNode() const
{
    std::lock_guard<std::mutex> lock(_nodeMutex);

    GatheringNode const* bestNode = nullptr;
    float bestScore = 0.0f;

    for (auto const& node : _detectedNodes)
    {
        if (!node.isActive || !CanGatherFromNode(node))
            continue;

        // Calculate score based on distance and skill-up chance
        float score = 100.0f / (1.0f + node.distance);  // Distance factor

        if (_prioritizeSkillUps)
        {
            score *= (1.0f + GetSkillUpChance(node));  // Skill-up bonus
        }

        if (score > bestScore)
        {
            bestScore = score;
            bestNode = &node;
        }
    }

    return bestNode;
}

void GatheringManager::ProcessCurrentGathering()
{
    if (!GetBot() || !_currentTarget)
    {
        StopGathering();
        return;
    }

    // Check if gathering timed out
    auto elapsed = std::chrono::steady_clock::now() - _gatheringStartTime;
    if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() > GATHERING_CAST_TIME * 3)
    {
        TC_LOG_DEBUG("bot.playerbot", "GatheringManager: Gathering timed out");
        HandleGatheringResult(*_currentTarget, false);
        return;
    }

    // Check if node is still valid
    if (!ValidateNode(*_currentTarget))
    {
        TC_LOG_DEBUG("bot.playerbot", "GatheringManager: Node no longer valid");
        HandleGatheringResult(*_currentTarget, false);
        return;
    }

    // Check if we're still in range
    if (!IsInGatheringRange(*_currentTarget))
    {
        TC_LOG_DEBUG("bot.playerbot", "GatheringManager: Moved out of gathering range");
        StopGathering();
        return;
    }

    // Check if gathering is complete (simplified - real implementation would check spell cast result)
    if (!GetBot()->IsNonMeleeSpellCast(false) && !_isLooting)
    {
        HandleGatheringResult(*_currentTarget, true);
    }
}

void GatheringManager::CleanupExpiredNodes()
{
    std::lock_guard<std::mutex> lock(_nodeMutex);

    uint32 currentTime = getMSTime();

    // Remove nodes that are too old or no longer valid
    _detectedNodes.erase(
        std::remove_if(_detectedNodes.begin(), _detectedNodes.end(),
            [currentTime, this](GatheringNode const& node) {
                return !node.isActive ||
                       (currentTime - node.detectionTime > NODE_CACHE_DURATION) ||
                       !ValidateNode(node);
            }),
        _detectedNodes.end()
    );
}

void GatheringManager::RecordStatistics(GatheringNode const& node, bool success)
{
    if (success)
    {
        _statistics.nodesGathered++;
        _statistics.distanceTraveled += static_cast<uint32>(node.distance);
    }
    else
    {
        _statistics.failedAttempts++;
    }

    auto elapsed = std::chrono::steady_clock::now() - _gatheringStartTime;
    _statistics.timeSpentGathering += static_cast<uint32>(
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
}

bool GatheringManager::HasProfession(GatheringNodeType nodeType) const
{
    return GetProfessionSkill(nodeType) > 0;
}

uint16 GatheringManager::GetProfessionSkill(GatheringNodeType nodeType) const
{
    if (!GetBot())
        return 0;

    uint32 skillId = 0;
    switch (nodeType)
    {
        case GatheringNodeType::MINING_VEIN:
            skillId = SKILL_MINING;
            break;
        case GatheringNodeType::HERB_NODE:
            skillId = SKILL_HERBALISM;
            break;
        case GatheringNodeType::CREATURE_CORPSE:
            skillId = SKILL_SKINNING;
            break;
        case GatheringNodeType::FISHING_POOL:
            skillId = SKILL_FISHING;
            break;
        default:
            return 0;
    }

    return GetBot()->GetSkillValue(skillId);
}

void GatheringManager::OnSpellCastComplete(Spell const* spell)
{
    if (!spell || spell->m_spellInfo->Id != _currentSpellId)
        return;

    // Gathering spell completed
    if (_currentTarget)
    {
        // Check if the spell was executed successfully (true for successful cast)
        HandleGatheringResult(*_currentTarget, true);
    }
}

} // namespace Playerbot