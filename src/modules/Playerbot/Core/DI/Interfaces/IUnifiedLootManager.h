/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "Define.h"
#include <vector>
#include <string>
#include <memory>

class Player;
class Group;
class Item;
struct Loot;

namespace Playerbot
{

// Forward declarations from LootDistribution.h
struct LootItem;
enum class LootRollType : uint8;
enum class LootDecisionStrategy : uint8;
enum class LootPriority : uint8;

/**
 * @brief Unified interface for all loot management operations
 *
 * This interface consolidates functionality from:
 * - LootAnalysis: Item value calculation and upgrade analysis
 * - LootCoordination: Loot session management and orchestration
 * - LootDistribution: Roll management and distribution execution
 *
 * **Design Pattern:** Facade Pattern
 * - Single entry point for all loot operations
 * - Simplifies loot system interactions
 * - Reduces coupling between components
 *
 * **Benefits:**
 * - Easier to test (single mock instead of 3)
 * - Clear API surface
 * - Reduced complexity
 * - Better encapsulation
 */
class TC_GAME_API IUnifiedLootManager
{
public:
    virtual ~IUnifiedLootManager() = default;

    // ========================================================================
    // ANALYSIS MODULE (from LootAnalysis)
    // ========================================================================

    /**
     * @brief Calculate the overall value of an item for a player
     * @param player Player to evaluate for
     * @param item Item to evaluate
     * @return Value score (0.0 - 100.0)
     */
    virtual float CalculateItemValue(Player* player, LootItem const& item) = 0;

    /**
     * @brief Calculate how much of an upgrade this item is
     * @param player Player to evaluate for
     * @param item Item to evaluate
     * @return Upgrade value (0.0 = no upgrade, 100.0 = massive upgrade)
     */
    virtual float CalculateUpgradeValue(Player* player, LootItem const& item) = 0;

    /**
     * @brief Check if item is a significant upgrade
     * @param player Player to check for
     * @param item Item to check
     * @return True if upgrade is significant (>= 15% improvement)
     */
    virtual bool IsSignificantUpgrade(Player* player, LootItem const& item) = 0;

    /**
     * @brief Calculate stat weight for a specific stat type
     * @param player Player to calculate for
     * @param statType Stat type enum
     * @return Stat weight value
     */
    virtual float CalculateStatWeight(Player* player, uint32 statType) = 0;

    /**
     * @brief Compare two items for a player
     * @param player Player context
     * @param newItem New item being considered
     * @param currentItem Currently equipped item
     * @return Comparison score (positive = new item better)
     */
    virtual float CompareItems(Player* player, LootItem const& newItem, Item const* currentItem) = 0;

    /**
     * @brief Calculate comprehensive item score
     * @param player Player context
     * @param item Item to score
     * @return Overall score (0-100)
     */
    virtual float CalculateItemScore(Player* player, LootItem const& item) = 0;

    /**
     * @brief Get stat priorities for a player's class/spec
     * @param player Player to get priorities for
     * @return Vector of (statType, weight) pairs, sorted by priority
     */
    virtual ::std::vector<::std::pair<uint32, float>> GetStatPriorities(Player* player) = 0;

    // ========================================================================
    // COORDINATION MODULE (from LootCoordination)
    // ========================================================================

    /**
     * @brief Start a new loot session for a group
     * @param group Group that triggered loot
     * @param loot Loot object containing items
     */
    virtual void InitiateLootSession(Group* group, Loot* loot) = 0;

    /**
     * @brief Process an active loot session
     * @param group Group context
     * @param lootSessionId Session to process
     */
    virtual void ProcessLootSession(Group* group, uint32 lootSessionId) = 0;

    /**
     * @brief Complete and cleanup a loot session
     * @param lootSessionId Session to complete
     */
    virtual void CompleteLootSession(uint32 lootSessionId) = 0;

    /**
     * @brief Handle loot session timeout
     * @param lootSessionId Session that timed out
     */
    virtual void HandleLootSessionTimeout(uint32 lootSessionId) = 0;

    /**
     * @brief Orchestrate intelligent loot distribution
     * @param group Group context
     * @param items Items to distribute
     */
    virtual void OrchestrateLootDistribution(Group* group, ::std::vector<LootItem> const& items) = 0;

    /**
     * @brief Prioritize loot distribution order
     * @param group Group context
     * @param items Items to prioritize (modified in-place)
     */
    virtual void PrioritizeLootDistribution(Group* group, ::std::vector<LootItem>& items) = 0;

    /**
     * @brief Optimize loot sequence for efficiency
     * @param group Group context
     * @param items Items to optimize (modified in-place)
     */
    virtual void OptimizeLootSequence(Group* group, ::std::vector<LootItem>& items) = 0;

    /**
     * @brief Facilitate group discussion about loot
     * @param group Group context
     * @param item Item being discussed
     */
    virtual void FacilitateGroupLootDiscussion(Group* group, LootItem const& item) = 0;

    /**
     * @brief Handle loot conflict resolution
     * @param group Group context
     * @param item Contested item
     */
    virtual void HandleLootConflictResolution(Group* group, LootItem const& item) = 0;

    /**
     * @brief Broadcast loot recommendations to group
     * @param group Group context
     * @param item Item to recommend on
     */
    virtual void BroadcastLootRecommendations(Group* group, LootItem const& item) = 0;

    /**
     * @brief Optimize loot efficiency for group
     * @param group Group to optimize for
     */
    virtual void OptimizeLootEfficiency(Group* group) = 0;

    /**
     * @brief Minimize time spent looting
     * @param group Group context
     * @param sessionId Session to optimize
     */
    virtual void MinimizeLootTime(Group* group, uint32 sessionId) = 0;

    /**
     * @brief Maximize loot fairness
     * @param group Group context
     * @param sessionId Session to optimize
     */
    virtual void MaximizeLootFairness(Group* group, uint32 sessionId) = 0;

    // ========================================================================
    // DISTRIBUTION MODULE (from LootDistribution)
    // ========================================================================

    /**
     * @brief Execute loot distribution for an item
     * @param group Group context
     * @param item Item to distribute
     */
    virtual void DistributeLoot(Group* group, LootItem const& item) = 0;

    /**
     * @brief Handle a player's loot roll
     * @param player Player rolling
     * @param rollId Roll identifier
     * @param rollType Type of roll (NEED, GREED, PASS)
     */
    virtual void HandleLootRoll(Player* player, uint32 rollId, LootRollType rollType) = 0;

    /**
     * @brief Determine optimal loot decision for a player
     * @param player Player context
     * @param item Item being considered
     * @param strategy Decision strategy to use
     * @return Recommended roll type
     */
    virtual LootRollType DetermineLootDecision(Player* player, LootItem const& item,
                                                 LootDecisionStrategy strategy) = 0;

    /**
     * @brief Calculate loot priority for a player
     * @param player Player context
     * @param item Item being evaluated
     * @return Priority level
     */
    virtual LootPriority CalculateLootPriority(Player* player, LootItem const& item) = 0;

    /**
     * @brief Evaluate if a player should roll NEED
     * @param player Player context
     * @param item Item being evaluated
     * @return True if NEED is appropriate
     */
    virtual bool ShouldRollNeed(Player* player, LootItem const& item) = 0;

    /**
     * @brief Evaluate if a player should roll GREED
     * @param player Player context
     * @param item Item being evaluated
     * @return True if GREED is appropriate
     */
    virtual bool ShouldRollGreed(Player* player, LootItem const& item) = 0;

    /**
     * @brief Check if item is class-appropriate
     * @param player Player context
     * @param item Item to check
     * @return True if item is appropriate for player's class
     */
    virtual bool IsItemForClass(Player* player, LootItem const& item) = 0;

    /**
     * @brief Check if item is for main spec
     * @param player Player context
     * @param item Item to check
     * @return True if item is for main spec
     */
    virtual bool IsItemForMainSpec(Player* player, LootItem const& item) = 0;

    /**
     * @brief Check if item is for off spec
     * @param player Player context
     * @param item Item to check
     * @return True if item is for off spec
     */
    virtual bool IsItemForOffSpec(Player* player, LootItem const& item) = 0;

    /**
     * @brief Execute loot distribution based on rolls
     * @param group Group context
     * @param rollId Roll identifier
     */
    virtual void ExecuteLootDistribution(Group* group, uint32 rollId) = 0;

    /**
     * @brief Resolve roll ties
     * @param group Group context
     * @param rollId Roll with tied results
     */
    virtual void ResolveRollTies(Group* group, uint32 rollId) = 0;

    /**
     * @brief Handle loot ninja detection
     * @param group Group context
     * @param suspectedPlayer Player suspected of ninjaing
     */
    virtual void HandleLootNinja(Group* group, uint32 suspectedPlayer) = 0;

    // ========================================================================
    // UNIFIED OPERATIONS (combining all modules)
    // ========================================================================

    /**
     * @brief Complete end-to-end loot processing
     *
     * This method orchestrates the entire loot flow:
     * 1. Analyze items (Analysis module)
     * 2. Coordinate distribution (Coordination module)
     * 3. Execute rolls and distribution (Distribution module)
     *
     * @param group Group that looted
     * @param loot Loot object
     */
    virtual void ProcessCompleteLootFlow(Group* group, Loot* loot) = 0;

    /**
     * @brief Get comprehensive loot recommendation
     *
     * Combines analysis and decision logic to provide:
     * - Item value score
     * - Upgrade assessment
     * - Recommended action
     * - Reasoning
     *
     * @param player Player context
     * @param item Item to evaluate
     * @return Detailed recommendation string
     */
    virtual ::std::string GetLootRecommendation(Player* player, LootItem const& item) = 0;

    /**
     * @brief Get statistics for loot operations
     * @return Statistics string (for debugging/monitoring)
     */
    virtual ::std::string GetLootStatistics() const = 0;
};

} // namespace Playerbot
