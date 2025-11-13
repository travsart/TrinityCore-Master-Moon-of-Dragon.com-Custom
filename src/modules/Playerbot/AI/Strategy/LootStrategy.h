/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "Strategy.h"
#include "Define.h"
#include "ObjectGuid.h"
#include <vector>

class Creature;
class GameObject;

namespace Playerbot
{

// Forward declarations
class BotAI;

/**
 * @class LootStrategy
 * @brief Handles corpse looting and item pickup for solo bots
 *
 * This strategy drives bots to:
 * - Loot nearby corpses after combat
 * - Pick up quest items and valuables
 * - Manage inventory space during looting
 * - Prioritize loot based on value and need
 *
 * Priority: Medium (runs after combat ends, before other activities)
 * Performance: <0.1ms per update (only scans when needed)
 */
class TC_GAME_API LootStrategy : public Strategy
{
public:
    LootStrategy();
    ~LootStrategy() override = default;

    // Strategy interface
    void InitializeActions() override;
    void InitializeTriggers() override;
    void InitializeValues() override;

    void OnActivate(BotAI* ai) override;
    void OnDeactivate(BotAI* ai) override;

    bool IsActive(BotAI* ai) const override;
    float GetRelevance(BotAI* ai) const override;
    void UpdateBehavior(BotAI* ai, uint32 diff) override;

private:
    /**
     * @brief Find nearby lootable corpses
     * @param ai Bot AI instance
     * @param maxDistance Maximum search distance (default: 30 yards)
     * @return List of lootable creature GUIDs
     */
    std::vector<ObjectGuid> FindLootableCorpses(BotAI* ai, float maxDistance = 30.0f) const;

    /**
     * @brief Find nearby lootable game objects (chests, herb nodes, etc.)
     * @param ai Bot AI instance
     * @param maxDistance Maximum search distance (default: 20 yards)
     * @return List of lootable GameObject GUIDs
     */
    std::vector<ObjectGuid> FindLootableObjects(BotAI* ai, float maxDistance = 20.0f) const;

    /**
     * @brief Move to and loot a corpse
     * @param ai Bot AI instance
     * @param corpseGuid GUID of corpse to loot
     * @return true if looting initiated
     */
    bool LootCorpse(BotAI* ai, ObjectGuid corpseGuid);

    /**
     * @brief Move to and loot a game object
     * @param ai Bot AI instance
     * @param objectGuid GUID of object to loot
     * @return true if looting initiated
     */
    bool LootObject(BotAI* ai, ObjectGuid objectGuid);

    /**
     * @brief Check if bot has inventory space
     * @param ai Bot AI instance
     * @return true if bot can pick up more items
     */
    bool HasInventorySpace(BotAI* ai) const;

    /**
     * @brief Prioritize loot targets by distance and value
     * @param ai Bot AI instance
     * @param targets List of loot target GUIDs
     * @return Sorted list (highest priority first)
     */
    std::vector<ObjectGuid> PrioritizeLootTargets(BotAI* ai, std::vector<ObjectGuid> const& targets) const;

private:
    // Loot scan throttling
    uint32 _lastLootScan = 0;
    uint32 _lootScanInterval = 1000; // Scan every 1 second

    // Current loot target
    ObjectGuid _currentLootTarget;
    uint32 _lootAttempts = 0;
    uint32 _maxLootAttempts = 3;

    // Performance tracking
    uint32 _itemsLooted = 0;
    uint32 _corpseLooted = 0;
    uint32 _goldLooted = 0;
};

} // namespace Playerbot
