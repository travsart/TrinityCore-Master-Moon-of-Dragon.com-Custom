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
#include "SharedDefines.h"
#include <map>
#include <vector>
#include <atomic>

class Player;

namespace Playerbot
{

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
    ::std::map<uint8, uint32> items;           // slot -> itemEntry
    ::std::vector<uint32> bags;                // 4 bag slots
    ::std::map<uint32, uint32> consumables;    // itemEntry -> quantity

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
    ::std::atomic<uint64> setsGenerated{0};
    ::std::atomic<uint64> itemsSelected{0};
    ::std::atomic<uint64> itemsApplied{0};     // Items successfully equipped to players
    ::std::atomic<uint64> cacheLookups{0};
    ::std::atomic<uint64> qualityRolls{0};
    ::std::atomic<uint32> cacheSize{0};

    void Reset()
    {
        setsGenerated.store(0, ::std::memory_order_relaxed);
        itemsSelected.store(0, ::std::memory_order_relaxed);
        itemsApplied.store(0, ::std::memory_order_relaxed);
        cacheLookups.store(0, ::std::memory_order_relaxed);
        qualityRolls.store(0, ::std::memory_order_relaxed);
    }
};

/**
 * @brief Interface for Bot Gear Factory
 *
 * Automated gear generation system for instant bot level-up.
 * Uses immutable cache for lock-free, high-performance item selection.
 *
 * **Responsibilities:**
 * - Generate complete gear sets for bots during instant level-up
 * - Lock-free item selection from pre-built cache
 * - Integration with EquipmentManager for stat weight calculations
 * - Apply gear sets to player characters
 * - Track performance metrics
 */
class TC_GAME_API IBotGearFactory
{
public:
    virtual ~IBotGearFactory() = default;

    /**
     * Initialize the gear factory and build immutable cache
     * Called once at server startup
     */
    virtual void Initialize() = 0;

    /**
     * Check if factory is ready to generate gear
     */
    virtual bool IsReady() const = 0;

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
    virtual GearSet BuildGearSet(uint8 cls, uint32 specId, uint32 level, TeamId faction) = 0;

    /**
     * Apply gear set to player (create items and equip)
     * Must be called from main thread (uses Player API)
     *
     * @param player    Player object to equip
     * @param gearSet   Pre-generated gear set
     * @return true if successfully equipped
     */
    virtual bool ApplyGearSet(Player* player, GearSet const& gearSet) = 0;

    /**
     * Get statistics for monitoring performance
     * Fills the provided stats struct with current values
     */
    virtual void GetStats(GearFactoryStats& stats) const = 0;

    /**
     * Get item level for character level (mapping)
     * L1 -> ilvl 5, L80 -> ilvl 593
     */
    virtual uint32 GetItemLevelForCharLevel(uint32 charLevel) = 0;

    /**
     * Get appropriate bag item entries for level range
     */
    virtual ::std::vector<uint32> GetBagItemsForLevel(uint32 level) = 0;

    /**
     * Get class-appropriate consumables
     */
    virtual ::std::map<uint32, uint32> GetConsumablesForClass(uint8 cls, uint32 level) = 0;
};

} // namespace Playerbot
