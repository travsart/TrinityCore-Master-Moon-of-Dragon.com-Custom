/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "LootStrategy.h"
#include "../BotAI.h"
#include "Player.h"
#include "Creature.h"
#include "GameObject.h"
#include "Item.h"
#include "Bag.h"
#include "ItemTemplate.h"
#include "ObjectAccessor.h"
#include "Map.h"
#include "Log.h"
#include "GridNotifiers.h"
#include "CellImpl.h"
#include "Loot.h"
#include "LootMgr.h"
#include "MotionMaster.h"

namespace Playerbot
{

LootStrategy::LootStrategy()
    : Strategy("loot")
{
    TC_LOG_DEBUG("module.playerbot.strategy", "LootStrategy: Initialized");
}

void LootStrategy::InitializeActions()
{
    // No actions needed - loot strategy drives looting directly
    TC_LOG_DEBUG("module.playerbot.strategy", "LootStrategy: No actions (direct loot control)");
}

void LootStrategy::InitializeTriggers()
{
    // No triggers needed - relevance system handles activation
    TC_LOG_DEBUG("module.playerbot.strategy", "LootStrategy: No triggers (using relevance system)");
}

void LootStrategy::InitializeValues()
{
    // No values needed for this simple strategy
    TC_LOG_DEBUG("module.playerbot.strategy", "LootStrategy: No values");
}

void LootStrategy::OnActivate(BotAI* ai)
{
    if (!ai || !ai->GetBot())
        return;

    TC_LOG_INFO("module.playerbot.strategy", "Loot strategy activated for bot {}", ai->GetBot()->GetName());
    SetActive(true);
}

void LootStrategy::OnDeactivate(BotAI* ai)
{
    if (!ai || !ai->GetBot())
        return;

    TC_LOG_INFO("module.playerbot.strategy", "Loot strategy deactivated for bot {}", ai->GetBot()->GetName());
    SetActive(false);

    // Clear current loot target
    _currentLootTarget.Clear();
    _lootAttempts = 0;
}

bool LootStrategy::IsActive(BotAI* ai) const
{
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();

    // NOT active during combat
    if (bot->IsInCombat())
        return false;

    // Active if explicitly activated and not in a group
    return _active && !bot->GetGroup();
}

float LootStrategy::GetRelevance(BotAI* ai) const
{
    if (!ai || !ai->GetBot())
        return 0.0f;

    Player* bot = ai->GetBot();

    // Don't loot during combat
    if (bot->IsInCombat())
        return 0.0f;

    // Don't loot if inventory is full
    if (!HasInventorySpace(ai))
        return 0.0f;

    // Check if there are nearby lootable targets
    uint32 currentTime = getMSTime();
    if (currentTime - _lastLootScan < _lootScanInterval)
    {
        // Return cached relevance
        return _currentLootTarget.IsEmpty() ? 0.0f : 60.0f;
    }

    // Scan for loot
    std::vector<ObjectGuid> corpses = FindLootableCorpses(ai);
    std::vector<ObjectGuid> objects = FindLootableObjects(ai);

    // Medium-high relevance if loot available (lower than quest=70, higher than solo=10)
    return (!corpses.empty() || !objects.empty()) ? 60.0f : 0.0f;
}

void LootStrategy::UpdateBehavior(BotAI* ai, uint32 diff)
{
    if (!ai || !ai->GetBot())
        return;

    Player* bot = ai->GetBot();

    // Don't loot during combat
    if (bot->IsInCombat())
        return;

    // Don't loot if inventory is full
    if (!HasInventorySpace(ai))
    {
        TC_LOG_DEBUG("module.playerbot.strategy", "LootStrategy: Bot {} inventory full, skipping loot",
                     bot->GetName());
        return;
    }

    TC_LOG_DEBUG("module.playerbot.strategy", "LootStrategy::UpdateBehavior: Bot {} searching for loot",
                 bot->GetName());

    uint32 currentTime = getMSTime();

    // Throttle loot scanning
    if (currentTime - _lastLootScan < _lootScanInterval)
        return;

    _lastLootScan = currentTime;

    // Find lootable targets
    std::vector<ObjectGuid> corpses = FindLootableCorpses(ai);
    std::vector<ObjectGuid> objects = FindLootableObjects(ai);

    TC_LOG_DEBUG("module.playerbot.strategy", "LootStrategy: Bot {} found {} corpses and {} objects",
                 bot->GetName(), corpses.size(), objects.size());

    // Combine and prioritize targets
    std::vector<ObjectGuid> allTargets;
    allTargets.insert(allTargets.end(), corpses.begin(), corpses.end());
    allTargets.insert(allTargets.end(), objects.begin(), objects.end());

    if (allTargets.empty())
    {
        _currentLootTarget.Clear();
        return;
    }

    // Prioritize targets
    allTargets = PrioritizeLootTargets(ai, allTargets);

    // Try to loot the highest priority target
    for (ObjectGuid const& targetGuid : allTargets)
    {
        if (targetGuid.IsCreature())
        {
            if (LootCorpse(ai, targetGuid))
            {
                _currentLootTarget = targetGuid;
                _corpseLooted++;
                break;
            }
        }
        else if (targetGuid.IsGameObject())
        {
            if (LootObject(ai, targetGuid))
            {
                _currentLootTarget = targetGuid;
                break;
            }
        }
    }
}

std::vector<ObjectGuid> LootStrategy::FindLootableCorpses(BotAI* ai, float maxDistance) const
{
    std::vector<ObjectGuid> lootableCorpses;

    if (!ai || !ai->GetBot())
        return lootableCorpses;

    Player* bot = ai->GetBot();

    // Find all creatures in range
    std::list<Creature*> nearbyCreatures;
    bot->GetCreatureListWithEntryInGrid(nearbyCreatures, 0, maxDistance);

    // Filter for dead creatures with loot
    for (Creature* creature : nearbyCreatures)
    {
        if (!creature || !creature->isDead())
            continue;

        // Check if creature has loot
        if (!creature->CanHaveLoot() || !creature->hasLootRecipient())
            continue;

        // Add to lootable list
        lootableCorpses.push_back(creature->GetGUID());
    }

    return lootableCorpses;
}

std::vector<ObjectGuid> LootStrategy::FindLootableObjects(BotAI* ai, float maxDistance) const
{
    std::vector<ObjectGuid> lootableObjects;

    if (!ai || !ai->GetBot())
        return lootableObjects;

    Player* bot = ai->GetBot();
    Map* map = bot->GetMap();
    if (!map)
        return lootableObjects;

    // Scan for game objects within range
    std::list<GameObject*> nearbyObjects;
    Trinity::GameObjectInRangeCheck check(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), maxDistance);
    Trinity::GameObjectListSearcher<Trinity::GameObjectInRangeCheck> searcher(bot, nearbyObjects, check);
    Cell::VisitGridObjects(bot, searcher, maxDistance);

    for (GameObject* object : nearbyObjects)
    {
        if (!object)
            continue;

        // Check if object is lootable (chests, herbs, mining nodes)
        GameobjectTypes type = object->GetGoType();
        if (type != GAMEOBJECT_TYPE_CHEST &&
            type != GAMEOBJECT_TYPE_GOOBER &&
            type != GAMEOBJECT_TYPE_FISHINGHOLE)
            continue;

        // Check if object is ready to use
        if (!object->isSpawned())
            continue;

        // Add to lootable list
        lootableObjects.push_back(object->GetGUID());
    }

    return lootableObjects;
}

bool LootStrategy::LootCorpse(BotAI* ai, ObjectGuid corpseGuid)
{
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();

    Creature* creature = ObjectAccessor::GetCreature(*bot, corpseGuid);
    if (!creature || !creature->isDead())
        return false;

    // Check distance
    float distance = bot->GetDistance(creature);
    if (distance > INTERACTION_DISTANCE)
    {
        // Move closer
        Position pos;
        pos.Relocate(creature->GetPositionX(), creature->GetPositionY(), creature->GetPositionZ());
        bot->GetMotionMaster()->MovePoint(0, pos);

        TC_LOG_DEBUG("module.playerbot.strategy", "LootStrategy: Bot {} moving to corpse at distance {:.1f}",
                     bot->GetName(), distance);
        return false;
    }

    // Get the loot and send it to bot
    if (creature->m_loot)
    {
        bot->SendLoot(*creature->m_loot, false);

        TC_LOG_DEBUG("module.playerbot.strategy", "LootStrategy: Bot {} looting corpse {}",
                     bot->GetName(), creature->GetEntry());
        return true;
    }

    return false;
}

bool LootStrategy::LootObject(BotAI* ai, ObjectGuid objectGuid)
{
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();

    GameObject* object = ObjectAccessor::GetGameObject(*bot, objectGuid);
    if (!object)
        return false;

    // Check distance
    float distance = bot->GetDistance(object);
    if (distance > INTERACTION_DISTANCE)
    {
        // Move closer
        Position pos;
        pos.Relocate(object->GetPositionX(), object->GetPositionY(), object->GetPositionZ());
        bot->GetMotionMaster()->MovePoint(0, pos);

        TC_LOG_DEBUG("module.playerbot.strategy", "LootStrategy: Bot {} moving to object at distance {:.1f}",
                     bot->GetName(), distance);
        return false;
    }

    // Use the object (opens loot)
    object->Use(bot);

    TC_LOG_DEBUG("module.playerbot.strategy", "LootStrategy: Bot {} looting object {}",
                 bot->GetName(), object->GetEntry());

    return true;
}

bool LootStrategy::HasInventorySpace(BotAI* ai) const
{
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();

    // Check if bot has at least 5 free bag slots
    uint32 freeSlots = 0;
    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (Bag* bag = bot->GetBagByPos(i))
        {
            freeSlots += bag->GetFreeSlots();
        }
    }

    // Add main backpack slots
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        if (!bot->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            freeSlots++;
    }

    return freeSlots >= 5;
}

std::vector<ObjectGuid> LootStrategy::PrioritizeLootTargets(BotAI* ai, std::vector<ObjectGuid> const& targets) const
{
    if (!ai || !ai->GetBot())
        return targets;

    Player* bot = ai->GetBot();
    std::vector<ObjectGuid> prioritized = targets;

    // Sort by distance (closest first)
    std::sort(prioritized.begin(), prioritized.end(),
        [bot](ObjectGuid const& a, ObjectGuid const& b) -> bool
        {
            WorldObject* objA = nullptr;
            WorldObject* objB = nullptr;

            if (a.IsCreature())
                objA = ObjectAccessor::GetCreature(*bot, a);
            else if (a.IsGameObject())
                objA = ObjectAccessor::GetGameObject(*bot, a);

            if (b.IsCreature())
                objB = ObjectAccessor::GetCreature(*bot, b);
            else if (b.IsGameObject())
                objB = ObjectAccessor::GetGameObject(*bot, b);

            if (!objA || !objB)
                return false;

            float distA = bot->GetDistance(objA);
            float distB = bot->GetDistance(objB);

            return distA < distB;
        });

    return prioritized;
}

} // namespace Playerbot
