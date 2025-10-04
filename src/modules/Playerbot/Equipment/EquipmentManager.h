/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * COMPLETE EQUIPMENT MANAGEMENT SYSTEM
 *
 * This system provides comprehensive equipment analysis, comparison, and
 * auto-equip functionality for all 13 WoW classes across all specializations.
 *
 * Key Features:
 * - Class/spec stat priority evaluation
 * - Item level + stat weight comparison
 * - Auto-equip better gear
 * - Junk item identification
 * - Set bonus tracking
 * - Weapon DPS comparison
 */

#pragma once

#include "Define.h"
#include "Player.h"
#include "Item.h"
#include "ItemTemplate.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>

namespace Playerbot
{

/**
 * @brief Stat types for item comparison based on WoW 11.2 itemMods
 */
enum class StatType : uint8
{
    STRENGTH = 0,
    AGILITY = 1,
    STAMINA = 2,
    INTELLECT = 3,
    SPIRIT = 4,
    CRITICAL_STRIKE = 5,
    HASTE = 6,
    VERSATILITY = 7,
    MASTERY = 8,
    ARMOR = 9,
    WEAPON_DPS = 10,
    ITEM_LEVEL = 11
};

/**
 * @brief Equipment quality levels
 */
enum class ItemQualityLevel : uint8
{
    POOR = 0,       // Grey - Junk, always sell
    COMMON = 1,     // White - Low value
    UNCOMMON = 2,   // Green - Quest rewards
    RARE = 3,       // Blue - Dungeon drops
    EPIC = 4,       // Purple - Raid/High-end
    LEGENDARY = 5,  // Orange - Legendary items
    ARTIFACT = 6,   // Gold - Artifact weapons
    HEIRLOOM = 7    // Blue with gold border
};

/**
 * @brief Item categories for organization
 */
enum class ItemCategory : uint8
{
    WEAPON,
    ARMOR,
    CONSUMABLE,
    TRADE_GOODS,
    QUEST_ITEM,
    JUNK,
    VALUABLE_BIND_ON_EQUIP,
    UNKNOWN
};

/**
 * @brief Stat priority configuration for class/spec combinations
 */
struct StatPriority
{
    uint8 classId;
    uint8 specId;
    std::vector<std::pair<StatType, float>> statWeights; // stat -> weight (0.0-1.0)

    StatPriority() : classId(0), specId(0) {}
    StatPriority(uint8 cls, uint8 spec) : classId(cls), specId(spec) {}

    float GetStatWeight(StatType stat) const
    {
        for (const auto& [statType, weight] : statWeights)
        {
            if (statType == stat)
                return weight;
        }
        return 0.0f;
    }
};

/**
 * @brief Item comparison result
 */
struct ItemComparisonResult
{
    bool isUpgrade = false;
    float scoreDifference = 0.0f;
    float currentItemScore = 0.0f;
    float newItemScore = 0.0f;
    uint32 currentItemLevel = 0;
    uint32 newItemLevel = 0;
    std::string upgradeReason;

    ItemComparisonResult() = default;
};

/**
 * @brief Complete equipment manager for all bot equipment operations
 */
class TC_GAME_API EquipmentManager
{
public:
    static EquipmentManager* instance();

    // ============================================================================
    // CORE EQUIPMENT EVALUATION
    // ============================================================================

    /**
     * Scan all inventory items and auto-equip better gear
     * This is the main entry point called by TradeAutomation
     */
    void AutoEquipBestGear(::Player* player);

    /**
     * Compare two items for the same equipment slot
     * Returns true if newItem is better than currentItem for this player
     */
    ItemComparisonResult CompareItems(::Player* player, ::Item* currentItem, ::Item* newItem);

    /**
     * Calculate item score based on class/spec stat priorities
     */
    float CalculateItemScore(::Player* player, ::Item* item);

    /**
     * Determine if item is an upgrade for any equipment slot
     */
    bool IsItemUpgrade(::Player* player, ::Item* item);

    // ============================================================================
    // JUNK IDENTIFICATION
    // ============================================================================

    /**
     * Identify all junk items in player's inventory
     * Returns item GUIDs that should be sold to vendor
     */
    std::vector<ObjectGuid> IdentifyJunkItems(::Player* player);

    /**
     * Check if specific item is junk (grey quality, low ilvl, wrong stats)
     */
    bool IsJunkItem(::Player* player, ::Item* item);

    /**
     * Check if item should NEVER be sold (quest items, valuables, set items)
     */
    bool IsProtectedItem(::Player* player, ::Item* item);

    /**
     * Evaluate if BoE item is valuable enough for AH vs vendor selling
     */
    bool IsValuableBoE(::Item* item);

    // ============================================================================
    // CONSUMABLE MANAGEMENT
    // ============================================================================

    /**
     * Get list of consumables this player needs to restock
     * Returns itemId -> quantity needed
     */
    std::unordered_map<uint32, uint32> GetConsumableNeeds(::Player* player);

    /**
     * Check if player has sufficient consumables for their class
     */
    bool NeedsConsumableRestocking(::Player* player);

    /**
     * Get class-specific consumable requirements (food, potions, reagents)
     */
    std::vector<uint32> GetClassConsumables(uint8 classId);

    /**
     * Check current consumable quantities
     */
    uint32 GetConsumableCount(::Player* player, uint32 itemId);

    // ============================================================================
    // STAT PRIORITY SYSTEM
    // ============================================================================

    /**
     * Get stat priority configuration for player's current class/spec
     */
    StatPriority const& GetStatPriority(::Player* player);

    /**
     * Initialize all class/spec stat priorities (called on startup)
     */
    void InitializeStatPriorities();

    /**
     * Update stat priorities when player changes spec
     */
    void UpdatePlayerStatPriority(::Player* player);

    // ============================================================================
    // ITEM CATEGORIZATION
    // ============================================================================

    /**
     * Determine item category for organization and selling decisions
     */
    ItemCategory GetItemCategory(::Item* item);

    /**
     * Check if item can be equipped by this player (class/level restrictions)
     */
    bool CanPlayerEquipItem(::Player* player, ItemTemplate const* itemTemplate);

    /**
     * Get equipment slot for this item (EQUIPMENT_SLOT_HEAD, etc.)
     */
    uint8 GetItemEquipmentSlot(ItemTemplate const* itemTemplate);

    // ============================================================================
    // ADVANCED FEATURES
    // ============================================================================

    /**
     * Check if item is part of a set and evaluate set bonuses
     */
    bool IsSetItem(::Item* item);

    /**
     * Count equipped set pieces for set bonus calculation
     */
    uint32 GetEquippedSetPieceCount(::Player* player, uint32 setId);

    /**
     * Evaluate weapon DPS (important for melee classes)
     */
    float GetWeaponDPS(::Item* item);

    /**
     * Get total armor value from item
     */
    uint32 GetItemArmor(::Item* item);

    /**
     * Extract stat value from item (Strength, Agility, etc.)
     */
    int32 GetItemStatValue(::Item* item, StatType stat);

    // ============================================================================
    // AUTOMATION CONTROL
    // ============================================================================

    struct EquipmentAutomationProfile
    {
        bool autoEquipEnabled = true;
        bool autoSellJunkEnabled = true;
        bool considerSetBonuses = true;
        bool preferHigherItemLevel = true;
        float minUpgradeThreshold = 5.0f; // Minimum % improvement to equip
        uint32 minItemLevelToKeep = 1;    // Sell items below this ilvl
        bool keepValuableBoE = true;
        std::unordered_set<uint32> neverSellItems; // Item IDs to always keep

        EquipmentAutomationProfile() = default;
    };

    void SetAutomationProfile(uint32 playerGuid, EquipmentAutomationProfile const& profile);
    EquipmentAutomationProfile GetAutomationProfile(uint32 playerGuid);

    // ============================================================================
    // METRICS AND MONITORING
    // ============================================================================

    struct EquipmentMetrics
    {
        std::atomic<uint32> itemsEquipped{0};
        std::atomic<uint32> upgradesFound{0};
        std::atomic<uint32> junkItemsSold{0};
        std::atomic<uint32> totalGoldFromJunk{0};
        std::atomic<float> averageItemScore{0.0f};

        void Reset()
        {
            itemsEquipped = 0;
            upgradesFound = 0;
            junkItemsSold = 0;
            totalGoldFromJunk = 0;
            averageItemScore = 0.0f;
        }
    };

    EquipmentMetrics const& GetPlayerMetrics(uint32 playerGuid);
    EquipmentMetrics const& GetGlobalMetrics();

private:
    EquipmentManager();
    ~EquipmentManager() = default;

    // Stat priority database (classId + specId -> StatPriority)
    std::unordered_map<uint16, StatPriority> _statPriorities; // key = (classId << 8) | specId

    // Player automation profiles
    std::unordered_map<uint32, EquipmentAutomationProfile> _playerProfiles;

    // Metrics tracking
    std::unordered_map<uint32, EquipmentMetrics> _playerMetrics;
    EquipmentMetrics _globalMetrics;

    mutable std::mutex _mutex;

    // ============================================================================
    // STAT PRIORITY INITIALIZATION (ALL 13 CLASSES)
    // ============================================================================

    void InitializeWarriorPriorities();
    void InitializePaladinPriorities();
    void InitializeHunterPriorities();
    void InitializeRoguePriorities();
    void InitializePriestPriorities();
    void InitializeShamanPriorities();
    void InitializeMagePriorities();
    void InitializeWarlockPriorities();
    void InitializeDruidPriorities();
    void InitializeDeathKnightPriorities();
    void InitializeMonkPriorities();
    void InitializeDemonHunterPriorities();
    void InitializeEvokerPriorities();

    // ============================================================================
    // HELPER METHODS
    // ============================================================================

    // Item score calculation helpers
    float CalculateWeaponScore(::Player* player, ::Item* weapon);
    float CalculateArmorScore(::Player* player, ::Item* armor);
    float CalculateAccessoryScore(::Player* player, ::Item* accessory);

    // Item analysis helpers
    bool IsOutdatedGear(::Player* player, ::Item* item);
    bool HasWrongPrimaryStats(::Player* player, ::Item* item);
    bool IsLowerQualityThanEquipped(::Player* player, ::Item* item);

    // Equipment slot helpers
    ::Item* GetEquippedItemInSlot(::Player* player, uint8 slot);
    bool CanEquipInSlot(::Player* player, ::Item* item, uint8 slot);
    void EquipItemInSlot(::Player* player, ::Item* item, uint8 slot);

    // Consumable helpers
    uint32 GetRecommendedFoodLevel(::Player* player);
    uint32 GetRecommendedPotionLevel(::Player* player);
    std::vector<uint32> GetClassReagents(uint8 classId);

    // Stat extraction from ItemTemplate (TrinityCore 11.2 API)
    int32 ExtractStatValue(ItemTemplate const* proto, StatType stat);
    float CalculateTotalStats(ItemTemplate const* proto, std::vector<std::pair<StatType, float>> const& weights);

    // Metrics updates
    void UpdateMetrics(uint32 playerGuid, bool wasEquipped, bool wasUpgrade, uint32 goldValue = 0);

    // Utility
    uint16 MakeStatPriorityKey(uint8 classId, uint8 specId) const { return (static_cast<uint16>(classId) << 8) | specId; }
    void LogEquipmentDecision(::Player* player, std::string const& action, std::string const& reason);
};

} // namespace Playerbot
