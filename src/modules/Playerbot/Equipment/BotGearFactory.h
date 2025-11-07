/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * BOT GEAR FACTORY - Automated Gear Generation System
 *
 * Purpose: Generate complete gear sets for bots during instant level-up
 * Uses immutable cache for lock-free, high-performance item selection
 *
 * INTEGRATION: Uses EquipmentManager for stat weight calculations
 * - GetStatPriorityByClassSpec() provides class/spec stat weights
 * - NO duplicate stat scoring logic - delegates to EquipmentManager
 * - Ensures consistency between gear generation and inventory management
 */

#pragma once

#include "Define.h"
#include "SharedDefines.h"
#include "Player.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>

namespace Playerbot
{

// Forward declarations
class EquipmentManager;

/**
 * @brief Quality distribution configuration for level ranges
 */
struct QualityDistribution
{
    uint32 minLevel;
    uint32 maxLevel;
    float greenPercent;    // Uncommon (Quality 2)
    float bluePercent;     // Rare (Quality 3)
    float purplePercent;   // Epic (Quality 4)

    QualityDistribution(uint32 min, uint32 max, float green, float blue, float purple)
        : minLevel(min), maxLevel(max), greenPercent(green), bluePercent(blue), purplePercent(purple) {}
};

/**
 * @brief Cached item data for fast lookup
 */
struct CachedItem
{
    uint32 itemEntry;
    uint32 itemLevel;
    uint32 requiredLevel;
    uint32 quality;
    uint8 inventoryType;
    uint8 itemClass;
    uint8 itemSubClass;
    float statScore;      // Pre-computed score for spec
    uint8 armorType;

    CachedItem() : itemEntry(0), itemLevel(0), requiredLevel(0), quality(0),
                   inventoryType(0), itemClass(0), itemSubClass(0), statScore(0.0f), armorType(0) {}
};

/**
 * @brief Complete gear set for a bot (14 slots + bags)
 */
struct GearSet
{
    std::map<uint8, uint32> items;           // slot -> itemEntry
    std::vector<uint32> bags;                // 4 bag slots
    std::map<uint32, uint32> consumables;    // itemEntry -> quantity

    float totalScore{0.0f};
    float averageIlvl{0.0f};
    uint32 setLevel{0};
    uint32 specId{0};

    bool HasWeapon() const { return items.count(15) > 0; }  // EQUIPMENT_SLOT_MAINHAND
    bool IsComplete() const { return items.size() >= 6; }
    uint32 GetItemCount() const { return static_cast<uint32>(items.size()); }
};

/**
 * @brief Statistics for gear generation performance tracking
 */
struct GearFactoryStats
{
    std::atomic<uint64> setsGenerated{0};
    std::atomic<uint64> itemsSelected{0};
    std::atomic<uint64> itemsApplied{0};     // Items successfully equipped to players
    std::atomic<uint64> cacheLookups{0};
    std::atomic<uint64> qualityRolls{0};
    std::atomic<uint32> cacheSize{0};

    void Reset()
    {
        setsGenerated.store(0, std::memory_order_relaxed);
        itemsSelected.store(0, std::memory_order_relaxed);
        itemsApplied.store(0, std::memory_order_relaxed);
        cacheLookups.store(0, std::memory_order_relaxed);
        qualityRolls.store(0, std::memory_order_relaxed);
    }
};

/**
 * @brief Immutable gear cache for lock-free item generation
 *
 * Cache Structure:
 * - class -> spec -> level -> slot -> [CachedItem]
 * - Built once at startup from item_template database
 * - Never modified after initialization (lock-free reads)
 * - Pre-scored items for each class/spec combination
 */
class TC_GAME_API BotGearFactory
{
public:
    static BotGearFactory* instance();

    /**
     * Initialize the gear factory and build immutable cache
     * Called once at server startup
     */
    void Initialize();

    /**
     * Check if factory is ready to generate gear
     */
    bool IsReady() const { return _cacheReady.load(std::memory_order_acquire); }

    /**
     * Generate complete gear set for bot
     * Thread-safe (lock-free cache reads)
     *
     * @param cls       Class ID (1-13)
     * @param specId    Specialization ID (0-3)
     * @param level     Character level (1-80)
     * @param faction   Faction for faction-specific items
     * @return Complete gear set with items, bags, consumables
     */
    GearSet BuildGearSet(uint8 cls, uint32 specId, uint32 level, TeamId faction);

    /**
     * Apply gear set to player (create items and equip)
     * Must be called from main thread (uses Player API)
     *
     * @param player    Player object to equip
     * @param gearSet   Pre-generated gear set
     * @return true if successfully equipped
     */
    bool ApplyGearSet(Player* player, GearSet const& gearSet);

    /**
     * Get statistics for monitoring performance
     * Fills the provided stats struct with current values
     */
    void GetStats(GearFactoryStats& stats) const
    {
        stats.setsGenerated.store(_stats.setsGenerated.load(std::memory_order_relaxed), std::memory_order_relaxed);
        stats.itemsSelected.store(_stats.itemsSelected.load(std::memory_order_relaxed), std::memory_order_relaxed);
        stats.cacheLookups.store(_stats.cacheLookups.load(std::memory_order_relaxed), std::memory_order_relaxed);
        stats.qualityRolls.store(_stats.qualityRolls.load(std::memory_order_relaxed), std::memory_order_relaxed);
        stats.cacheSize.store(_stats.cacheSize.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }

    /**
     * Get item level for character level (mapping)
     * L1 -> ilvl 5, L80 -> ilvl 593
     */
    static uint32 GetItemLevelForCharLevel(uint32 charLevel);

    /**
     * Get appropriate bag item entries for level range
     */
    std::vector<uint32> GetBagItemsForLevel(uint32 level);

    /**
     * Get class-appropriate consumables
     */
    std::map<uint32, uint32> GetConsumablesForClass(uint8 cls, uint32 level);

private:
    BotGearFactory();
    ~BotGearFactory() = default;
    BotGearFactory(BotGearFactory const&) = delete;
    BotGearFactory& operator=(BotGearFactory const&) = delete;

    /**
     * Build immutable item cache from database
     */
    void BuildGearCache();

    /**
     * Initialize quality distribution rules
     */
    void InitializeQualityDistributions();

    /**
     * Query item_template for eligible items
     */
    void LoadItemsFromDatabase();

    /**
     * Pre-compute stat scores for all cached items
     * INTEGRATION: Calls EquipmentManager::GetStatPriorityByClassSpec()
     * Uses the SAME stat weights as inventory management (zero duplication)
     */
    void PrecomputeItemScores();

    /**
     * Select quality tier based on level and slot
     */
    uint32 SelectQuality(uint32 level, uint8 slot);

    /**
     * Get quality distribution for level range
     */
    QualityDistribution const* GetQualityDistribution(uint32 level) const;

    /**
     * Select best item from candidates
     */
    uint32 SelectBestItem(uint8 cls, uint32 specId, uint32 level, uint8 slot, uint32 targetQuality);

    /**
     * Get cached items for specific parameters
     */
    std::vector<CachedItem> const* GetItemsForSlot(uint8 cls, uint32 specId, uint32 level, uint8 slot) const;

    /**
     * Filter items by quality
     */
    std::vector<CachedItem> FilterByQuality(std::vector<CachedItem> const& items, uint32 quality) const;

    /**
     * Get allowed armor types for class
     */
    std::vector<uint8> GetAllowedArmorTypes(uint8 cls) const;

    /**
     * Verify item is appropriate for class/spec
     */
    bool IsItemAppropriate(CachedItem const& item, uint8 cls, uint32 specId, uint32 level) const;

    // Cache: class -> spec -> level -> slot -> [items]
    using SlotCache = std::unordered_map<uint8, std::vector<CachedItem>>;
    using LevelCache = std::unordered_map<uint32, SlotCache>;
    using SpecCache = std::unordered_map<uint32, LevelCache>;
    using ClassCache = std::unordered_map<uint8, SpecCache>;

    ClassCache _gearCache;
    std::vector<QualityDistribution> _qualityDistributions;
    std::vector<CachedItem> _rawItems;  // Temporary storage for items before organizing into cache
    std::atomic<bool> _cacheReady{false};
    mutable GearFactoryStats _stats;
    std::mutex _initMutex;

    // Configuration
    uint32 _minItemLevel{5};
    uint32 _minQuality{2};  // Uncommon (Green)
    bool _useStatWeighting{true};
};

#define sBotGearFactory BotGearFactory::instance()

} // namespace Playerbot
