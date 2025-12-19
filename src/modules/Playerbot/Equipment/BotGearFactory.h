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
#include "Threading/LockHierarchy.h"
#include "SharedDefines.h"
#include "Player.h"
#include "Core/DI/Interfaces/IBotGearFactory.h"
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
 * @brief Immutable gear cache for lock-free item generation
 *
 * Cache Structure:
 * - class -> spec -> level -> slot -> [CachedItem]
 * - Built once at startup from item_template database
 * - Never modified after initialization (lock-free reads)
 * - Pre-scored items for each class/spec combination
 */
class TC_GAME_API BotGearFactory final : public IBotGearFactory
{
public:
    static BotGearFactory* instance();

    // IBotGearFactory interface implementation

    /**
     * Initialize the gear factory and build immutable cache
     * Called once at server startup
     */
    void Initialize() override;

    /**
     * Check if factory is ready to generate gear
     */
    bool IsReady() const override { return _cacheReady.load(::std::memory_order_acquire); }

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
    GearSet BuildGearSet(uint8 cls, uint32 specId, uint32 level, TeamId faction) override;

    /**
     * Apply gear set to player (create items and equip)
     * Must be called from main thread (uses Player API)
     *
     * @param player    Player object to equip
     * @param gearSet   Pre-generated gear set
     * @return true if successfully equipped
     */
    bool ApplyGearSet(Player* player, GearSet const& gearSet) override;

    /**
     * Get statistics for monitoring performance
     * Fills the provided stats struct with current values
     */
    void GetStats(GearFactoryStats& stats) const override
    {
        stats.setsGenerated.store(_stats.setsGenerated.load(::std::memory_order_relaxed), ::std::memory_order_relaxed);
        stats.itemsSelected.store(_stats.itemsSelected.load(::std::memory_order_relaxed), ::std::memory_order_relaxed);
        stats.cacheLookups.store(_stats.cacheLookups.load(::std::memory_order_relaxed), ::std::memory_order_relaxed);
        stats.qualityRolls.store(_stats.qualityRolls.load(::std::memory_order_relaxed), ::std::memory_order_relaxed);
        stats.cacheSize.store(_stats.cacheSize.load(::std::memory_order_relaxed), ::std::memory_order_relaxed);
    }

    /**
     * Get item level for character level (mapping)
     * L1 -> ilvl 5, L80 -> ilvl 593
     */
    uint32 GetItemLevelForCharLevel(uint32 charLevel) override;

    /**
     * Get appropriate bag item entries for level range
     */
    ::std::vector<uint32> GetBagItemsForLevel(uint32 level) override;

    /**
     * Get class-appropriate consumables
     */
    ::std::map<uint32, uint32> GetConsumablesForClass(uint8 cls, uint32 level) override;

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
    ::std::vector<CachedItem> const* GetItemsForSlot(uint8 cls, uint32 specId, uint32 level, uint8 slot) const;

    /**
     * Filter items by quality
     */
    ::std::vector<CachedItem> FilterByQuality(::std::vector<CachedItem> const& items, uint32 quality) const;

    /**
     * Get allowed armor types for class
     */
    ::std::vector<uint8> GetAllowedArmorTypes(uint8 cls) const;

    /**
     * Get allowed weapon subtypes for class
     * Returns vector of ITEM_SUBCLASS_WEAPON_* values
     */
    ::std::vector<uint8> GetAllowedWeaponTypes(uint8 cls) const;

    /**
     * Verify item is appropriate for class/spec
     */
    bool IsItemAppropriate(CachedItem const& item, uint8 cls, uint32 specId, uint32 level) const;

    // Cache: class -> spec -> level -> slot -> [items]
    using SlotCache = ::std::unordered_map<uint8, ::std::vector<CachedItem>>;
    using LevelCache = ::std::unordered_map<uint32, SlotCache>;
    using SpecCache = ::std::unordered_map<uint32, LevelCache>;
    using ClassCache = ::std::unordered_map<uint8, SpecCache>;

    ClassCache _gearCache;
    ::std::vector<QualityDistribution> _qualityDistributions;
    ::std::vector<CachedItem> _rawItems;  // Temporary storage for items before organizing into cache
    ::std::atomic<bool> _cacheReady{false};
    mutable GearFactoryStats _stats;
    Playerbot::OrderedMutex<Playerbot::LockOrder::BEHAVIOR_MANAGER> _initMutex;

    // Configuration
    uint32 _minItemLevel{5};
    uint32 _minQuality{2};  // Uncommon (Green)
    bool _useStatWeighting{true};
};

#define sBotGearFactory BotGearFactory::instance()

} // namespace Playerbot
