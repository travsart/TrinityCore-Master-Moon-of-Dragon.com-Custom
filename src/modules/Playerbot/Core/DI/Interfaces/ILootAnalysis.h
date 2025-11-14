/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "Define.h"
#include <vector>
#include <utility>

// Forward declarations
class Player;
class Group;
class Item;

namespace Playerbot
{

// Forward declarations
struct LootItem;
enum class LootPriority : uint8;
enum class LootRollType : uint8;

/**
 * @brief Interface for Loot Analysis System
 *
 * Advanced loot analysis for intelligent item evaluation and decision-making.
 *
 * Features:
 * - Item value calculation and comparison
 * - Upgrade analysis for players
 * - Class/spec specific evaluation
 * - Group loot coordination
 * - Market value estimation
 * - Learning and adaptation
 *
 * Thread Safety: All methods are thread-safe
 */
class TC_GAME_API ILootAnalysis
{
public:
    virtual ~ILootAnalysis() = default;

    /**
     * @brief Core item value calculation
     * @param player Player to evaluate for
     * @param item Loot item to analyze
     * @return Item value score (0.0-1.0)
     */
    virtual float CalculateItemValue(Player* player, const LootItem& item) = 0;

    /**
     * @brief Calculate upgrade value for player
     * @param player Player to check upgrade for
     * @param item Loot item to analyze
     * @return Upgrade value score (0.0-1.0)
     */
    virtual float CalculateUpgradeValue(Player* player, const LootItem& item) = 0;

    /**
     * @brief Check if item is significant upgrade
     * @param player Player to check for
     * @param item Loot item
     * @return true if significant upgrade
     */
    virtual bool IsSignificantUpgrade(Player* player, const LootItem& item) = 0;

    /**
     * @brief Calculate stat weight for player
     * @param player Player context
     * @param statType Stat type identifier
     * @return Stat weight value
     */
    virtual float CalculateStatWeight(Player* player, uint32 statType) = 0;

    /**
     * @brief Compare new item vs currently equipped
     * @param player Player context
     * @param newItem New loot item
     * @param currentItem Currently equipped item
     * @return Comparison score (negative = worse, positive = better)
     */
    virtual float CompareItems(Player* player, const LootItem& newItem, const Item* currentItem) = 0;

    /**
     * @brief Calculate overall item score
     * @param player Player context
     * @param item Loot item
     * @return Overall item score
     */
    virtual float CalculateItemScore(Player* player, const LootItem& item) = 0;

    /**
     * @brief Get stat priorities for player's class/spec
     * @param player Player context
     * @return Vector of (statType, priority) pairs
     */
    virtual ::std::vector<::std::pair<uint32, float>> GetStatPriorities(Player* player) = 0;

    /**
     * @brief Calculate item level weight
     * @param player Player context
     * @param itemLevel Item level
     * @return Item level weight factor
     */
    virtual float GetItemLevelWeight(Player* player, uint32 itemLevel) = 0;

    /**
     * @brief Check if player can equip item
     * @param player Player to check
     * @param item Loot item
     * @return true if player can equip
     */
    virtual bool CanEquipItem(Player* player, const LootItem& item) = 0;

    /**
     * @brief Get equipment slot for item
     * @param item Loot item
     * @return Equipment slot index
     */
    virtual uint32 GetEquipmentSlot(const LootItem& item) = 0;

    /**
     * @brief Get currently equipped item in slot
     * @param player Player context
     * @param slot Equipment slot
     * @return Currently equipped item or nullptr
     */
    virtual Item* GetCurrentEquippedItem(Player* player, uint32 slot) = 0;

    /**
     * @brief Calculate vendor sell value
     * @param item Loot item
     * @return Vendor value in copper
     */
    virtual float CalculateVendorValue(const LootItem& item) = 0;

    /**
     * @brief Calculate auction house value
     * @param item Loot item
     * @return AH value estimate in copper
     */
    virtual float CalculateAuctionHouseValue(const LootItem& item) = 0;

    /**
     * @brief Calculate disenchant value
     * @param item Loot item
     * @return Disenchant value estimate
     */
    virtual float CalculateDisenchantValue(const LootItem& item) = 0;

    /**
     * @brief Check if item valuable for vendoring
     * @param item Loot item
     * @return true if should vendor
     */
    virtual bool IsValuableForVendoring(const LootItem& item) = 0;

    /**
     * @brief Analyze group loot needs for item
     * @param group Group context
     * @param item Loot item
     */
    virtual void AnalyzeGroupLootNeeds(Group* group, const LootItem& item) = 0;

    /**
     * @brief Rank players in group for item
     * @param group Group context
     * @param item Loot item
     * @return Vector of (playerGuid, score) pairs, sorted by score
     */
    virtual ::std::vector<::std::pair<uint32, float>> RankPlayersForItem(Group* group, const LootItem& item) = 0;

    /**
     * @brief Check if item contested in group
     * @param group Group context
     * @param item Loot item
     * @return true if multiple players want item
     */
    virtual bool IsItemContestedInGroup(Group* group, const LootItem& item) = 0;

    /**
     * @brief Get best candidate for item in group
     * @param group Group context
     * @param item Loot item
     * @return Best player for item or nullptr
     */
    virtual Player* GetBestCandidateForItem(Group* group, const LootItem& item) = 0;
};

} // namespace Playerbot
