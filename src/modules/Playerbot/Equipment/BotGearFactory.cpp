/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * BOT GEAR FACTORY - IMPLEMENTATION
 *
 * High-performance gear generation using immutable cache pattern
 */

#include "BotGearFactory.h"
#include "EquipmentManager.h"
#include "Player.h"
#include "Item.h"
#include "Bag.h"
#include "ItemTemplate.h"
#include "ObjectMgr.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "SharedDefines.h"
#include <algorithm>
#include <random>

namespace Playerbot
{

BotGearFactory* BotGearFactory::instance()
{
    static BotGearFactory instance;
    return &instance;
}

BotGearFactory::BotGearFactory()
{
    TC_LOG_INFO("playerbot.gear", "BotGearFactory: Initializing...");
}

void BotGearFactory::Initialize()
{
    std::lock_guard<std::mutex> lock(_initMutex);

    if (_cacheReady.load(std::memory_order_acquire))
    {
        TC_LOG_WARN("playerbot.gear", "BotGearFactory: Already initialized");
        return;
    }

    TC_LOG_INFO("playerbot.gear", "BotGearFactory: Building gear cache...");

    InitializeQualityDistributions();
    BuildGearCache();

    _cacheReady.store(true, std::memory_order_release);

    TC_LOG_INFO("playerbot.gear", "BotGearFactory: Initialization complete. Cache size: {} items",
                _stats.cacheSize.load(std::memory_order_relaxed));
}

void BotGearFactory::InitializeQualityDistributions()
{
    _qualityDistributions.clear();

    // Leveling (L1-59): 50% Green, 50% Blue
    _qualityDistributions.emplace_back(1, 59, 50.0f, 50.0f, 0.0f);

    // Pre-Endgame (L60-69): 30% Green, 70% Blue
    _qualityDistributions.emplace_back(60, 69, 30.0f, 70.0f, 0.0f);

    // Endgame (L70-80): 60% Blue, 40% Purple
    _qualityDistributions.emplace_back(70, 80, 0.0f, 60.0f, 40.0f);

    TC_LOG_DEBUG("playerbot.gear", "BotGearFactory: Initialized {} quality distributions",
                 _qualityDistributions.size());
}

void BotGearFactory::BuildGearCache()
{
    LoadItemsFromDatabase();
    PrecomputeItemScores();

    TC_LOG_INFO("playerbot.gear", "BotGearFactory: Cache build complete");
}

void BotGearFactory::LoadItemsFromDatabase()
{
    TC_LOG_INFO("playerbot.gear", "BotGearFactory: Loading equippable items from ObjectMgr...");

    _rawItems.clear();
    _rawItems.reserve(100000); // Pre-allocate for performance

    // CRITICAL DISCOVERY: TrinityCore already loads ALL item templates into memory at startup
    // Using sObjectMgr->GetItemTemplateStore() is INSTANT and bypasses MySQL entirely
    // This is the CORRECT approach - no database queries during module initialization!
    // ObjectMgr::LoadItemTemplates() runs during WorldDatabase loading, before our module initializes

    ItemTemplateContainer const& itemStore = sObjectMgr->GetItemTemplateStore();
    uint32 totalItems = 0;
    uint32 filteredItems = 0;

    for (auto const& [itemEntry, itemTemplate] : itemStore)
    {
        ++totalItems;

        // Filter: Quality >= 2 (Uncommon+), ItemLevel >= 5, InventoryType > 0 (equippable only)
        if (itemTemplate.GetQuality() < 2)
            continue;
        if (itemTemplate.GetBaseItemLevel() < 5)
            continue;
        if (itemTemplate.GetInventoryType() == INVTYPE_NON_EQUIP)
            continue;

        CachedItem item;
        item.itemEntry = itemEntry;
        item.itemLevel = itemTemplate.GetBaseItemLevel();
        item.requiredLevel = itemTemplate.GetBaseRequiredLevel();
        item.quality = itemTemplate.GetQuality();
        item.inventoryType = itemTemplate.GetInventoryType();
        item.itemClass = itemTemplate.GetClass();
        item.itemSubClass = itemTemplate.GetSubClass();
        item.statScore = 0.0f; // Will be computed in PrecomputeItemScores()
        item.armorType = 0;    // Will be set in PrecomputeItemScores()

        _rawItems.push_back(item);
        ++filteredItems;
    }

    TC_LOG_INFO("playerbot.gear", "BotGearFactory: Loaded {} equippable items from {} total items in ObjectMgr (filtered in memory)",
                filteredItems, totalItems);
}

void BotGearFactory::PrecomputeItemScores()
{
    TC_LOG_INFO("playerbot.gear", "BotGearFactory: Organizing {} items into cache...", _rawItems.size());

    uint32 itemsOrganized = 0;

    // Organize all items into cache by inventory slot
    for (CachedItem const& item : _rawItems)
    {
        // Store item in cache for all classes that can use it
        // For now, store items generically by slot without class/spec filtering
        // Class/spec filtering will be done when building gear sets

        // Determine level bracket (group by 5-level increments)
        uint32 levelBracket = (item.requiredLevel / 5) * 5;
        if (levelBracket == 0)
            levelBracket = 1;

        // Store in cache for ALL classes (class 0 = generic)
        // Spec 0 = generic, applies to all specs
        _gearCache[0][0][levelBracket][item.inventoryType].push_back(item);
        ++itemsOrganized;
        ++_stats.cacheSize;
    }

    TC_LOG_INFO("playerbot.gear", "BotGearFactory: Organized {} items into cache", itemsOrganized);

    // Clear raw items to free memory
    _rawItems.clear();
    _rawItems.shrink_to_fit();
}

GearSet BotGearFactory::BuildGearSet(uint8 cls, uint32 specId, uint32 level, TeamId faction)
{
    if (!IsReady())
    {
        TC_LOG_ERROR("playerbot.gear", "BotGearFactory: Not ready, cache not built");
        return GearSet();
    }

    GearSet gearSet;
    gearSet.setLevel = level;
    gearSet.specId = specId;

    // Generate items for all equipment slots
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        uint32 targetQuality = SelectQuality(level, slot);
        uint32 itemEntry = SelectBestItem(cls, specId, level, slot, targetQuality);

        if (itemEntry > 0)
        {
            gearSet.items[slot] = itemEntry;
            ++_stats.itemsSelected;
        }
    }

    // Add bags
    gearSet.bags = GetBagItemsForLevel(level);

    // Add consumables
    gearSet.consumables = GetConsumablesForClass(cls, level);

    // Calculate average item level
    float totalIlvl = 0.0f;
    for (auto const& [slot, itemEntry] : gearSet.items)
    {
        if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemEntry))
            totalIlvl += proto->GetBaseItemLevel();
    }
    gearSet.averageIlvl = gearSet.items.empty() ? 0.0f : totalIlvl / gearSet.items.size();

    ++_stats.setsGenerated;

    TC_LOG_DEBUG("playerbot.gear", "BotGearFactory: Generated gear set for class {} spec {} level {} ({} items, avg ilvl {:.1f})",
                 cls, specId, level, gearSet.GetItemCount(), gearSet.averageIlvl);

    return gearSet;
}

bool BotGearFactory::ApplyGearSet(Player* player, GearSet const& gearSet)
{
    if (!player)
    {
        TC_LOG_ERROR("playerbot.gear", "BotGearFactory: Null player pointer");
        return false;
    }

    TC_LOG_DEBUG("playerbot.gear", "BotGearFactory: Applying gear set to player {} (class {} level {})",
                 player->GetName(), player->GetClass(), player->GetLevel());

    uint32 itemsEquipped = 0;
    uint32 itemsFailed = 0;

    // Phase 1: Equip main gear (armor, weapons, trinkets)
    for (auto const& [slot, itemEntry] : gearSet.items)
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemEntry);
        if (!proto)
        {
            TC_LOG_ERROR("playerbot.gear", "BotGearFactory: Invalid item entry {} for slot {}", itemEntry, slot);
            ++itemsFailed;
            continue;
        }

        // Use EquipNewItem for direct equipping (most efficient for bot initialization)
        // This creates the item and equips it in one operation
        uint16 equipDest = 0;
        InventoryResult equipResult = player->CanEquipNewItem(slot, equipDest, itemEntry, false);

        if (equipResult != EQUIP_ERR_OK)
        {
            TC_LOG_WARN("playerbot.gear", "BotGearFactory: Cannot equip item {} (entry {}) in slot {} for player {}: error {}",
                        proto->GetDefaultLocaleName(), itemEntry, slot, player->GetName(), uint32(equipResult));
            ++itemsFailed;
            continue;
        }

        // Create and equip item with WoW 11.2 ItemContext system
        // Use ITEM_CONTEXT_NONE for standard bot gear (not from dungeons/raids)
        Item* newItem = player->EquipNewItem(equipDest, itemEntry, ItemContext::NONE, true);

        if (newItem)
        {
            TC_LOG_DEBUG("playerbot.gear", "BotGearFactory: Equipped {} (entry {}, ilvl {}) in slot {} for player {}",
                         proto->GetDefaultLocaleName(), itemEntry, proto->GetBaseItemLevel(), slot, player->GetName());
            ++itemsEquipped;
        }
        else
        {
            TC_LOG_ERROR("playerbot.gear", "BotGearFactory: EquipNewItem failed for item {} in slot {} for player {}",
                         itemEntry, slot, player->GetName());
            ++itemsFailed;
        }
    }

    // Phase 2: Add bags to inventory
    uint32 bagsAdded = 0;
    uint8 bagSlot = INVENTORY_SLOT_BAG_START;

    for (uint32 bagEntry : gearSet.bags)
    {
        if (bagSlot >= INVENTORY_SLOT_BAG_END)
        {
            TC_LOG_WARN("playerbot.gear", "BotGearFactory: No more bag slots available for player {}", player->GetName());
            break;
        }

        ItemTemplate const* bagProto = sObjectMgr->GetItemTemplate(bagEntry);
        if (!bagProto)
        {
            TC_LOG_ERROR("playerbot.gear", "BotGearFactory: Invalid bag entry {}", bagEntry);
            continue;
        }

        // Check if bag can be equipped
        uint16 bagDest = 0;
        InventoryResult bagResult = player->CanEquipNewItem(bagSlot, bagDest, bagEntry, false);

        if (bagResult == EQUIP_ERR_OK)
        {
            Item* newBag = player->EquipNewItem(bagDest, bagEntry, ItemContext::NONE, true);
            if (newBag)
            {
                TC_LOG_DEBUG("playerbot.gear", "BotGearFactory: Equipped bag {} (entry {}, {} slots) in slot {} for player {}",
                             bagProto->GetDefaultLocaleName(), bagEntry, bagProto->GetContainerSlots(), bagSlot, player->GetName());
                ++bagsAdded;
            }
        }

        ++bagSlot;
    }

    // Phase 3: Add consumables to inventory (food, water, reagents)
    uint32 consumablesAdded = 0;

    for (auto const& [consumableEntry, quantity] : gearSet.consumables)
    {
        ItemTemplate const* consumableProto = sObjectMgr->GetItemTemplate(consumableEntry);
        if (!consumableProto)
        {
            TC_LOG_ERROR("playerbot.gear", "BotGearFactory: Invalid consumable entry {}", consumableEntry);
            continue;
        }

        // Find empty bag slots for consumables
        ItemPosCountVec dest;
        InventoryResult storeResult = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, consumableEntry, quantity);

        if (storeResult == EQUIP_ERR_OK)
        {
            // Create and store consumable items
            Item* newConsumable = player->StoreNewItem(dest, consumableEntry, true, ItemRandomBonusListId(), GuidSet(), ItemContext::NONE, false);

            if (newConsumable)
            {
                TC_LOG_DEBUG("playerbot.gear", "BotGearFactory: Added {} x{} (entry {}) to inventory of player {}",
                             consumableProto->GetDefaultLocaleName(), quantity, consumableEntry, player->GetName());
                ++consumablesAdded;
            }
        }
        else
        {
            TC_LOG_WARN("playerbot.gear", "BotGearFactory: Cannot store consumable {} x{} for player {}: error {}",
                        consumableEntry, quantity, player->GetName(), uint32(storeResult));
        }
    }

    // Phase 4: Save to database
    player->SaveToDB();

    // Log summary
    TC_LOG_INFO("playerbot.gear", "BotGearFactory: Applied gear set to player {} (class {} level {}): {} items equipped, {} bags, {} consumables ({}failed)",
                player->GetName(), player->GetClass(), player->GetLevel(),
                itemsEquipped, bagsAdded, consumablesAdded, itemsFailed);

    // Update statistics
    _stats.itemsApplied.fetch_add(itemsEquipped, std::memory_order_relaxed);

    return itemsFailed == 0;
}

uint32 BotGearFactory::GetItemLevelForCharLevel(uint32 charLevel)
{
    // Linear progression: L1 -> ilvl 5, L80 -> ilvl 593
    if (charLevel <= 1)
        return 5;
    if (charLevel >= 80)
        return 593;

    // Linear interpolation
    return 5 + ((charLevel - 1) * (593 - 5)) / (80 - 1);
}

std::vector<uint32> BotGearFactory::GetBagItemsForLevel(uint32 level)
{
    std::vector<uint32> bags;

    // Simple bag progression
    if (level < 10)
    {
        // 6-slot bags
        bags = {828, 828, 828, 828};  // Small Brown Pouch
    }
    else if (level < 30)
    {
        // 10-slot bags
        bags = {4496, 4496, 4496, 4496};  // Small Brown Pouch (10 slot)
    }
    else if (level < 60)
    {
        // 14-slot bags
        bags = {4500, 4500, 4500, 4500};  // Traveler's Backpack
    }
    else
    {
        // 16-slot bags
        bags = {14155, 14155, 14155, 14155};  // Mooncloth Bag
    }

    return bags;
}

std::map<uint32, uint32> BotGearFactory::GetConsumablesForClass(uint8 cls, uint32 level)
{
    std::map<uint32, uint32> consumables;

    // Food (all classes)
    if (level < 25)
        consumables[117] = 20;  // Tough Jerky
    else if (level < 45)
        consumables[4599] = 20;  // Cured Ham Steak
    else if (level < 65)
        consumables[8932] = 20;  // Alterac Swiss
    else
        consumables[27859] = 20;  // Zangarmarsh Shrooms

    // Water (mana users)
    if (cls == CLASS_MAGE || cls == CLASS_PRIEST || cls == CLASS_WARLOCK ||
        cls == CLASS_DRUID || cls == CLASS_SHAMAN || cls == CLASS_PALADIN)
    {
        if (level < 25)
            consumables[159] = 20;  // Refreshing Spring Water
        else if (level < 45)
            consumables[1179] = 20;  // Ice Cold Milk
        else if (level < 65)
            consumables[8766] = 20;  // Morning Glory Dew
        else
            consumables[28399] = 20;  // Filtered Draenic Water
    }

    // Class-specific reagents
    switch (cls)
    {
        case CLASS_MAGE:
            consumables[17031] = 20;  // Rune of Teleportation
            consumables[17032] = 20;  // Rune of Portals
            break;
        case CLASS_ROGUE:
            consumables[5140] = 20;   // Flash Powder
            consumables[3775] = 20;   // Blinding Powder
            break;
        case CLASS_WARLOCK:
            consumables[6265] = 20;   // Soul Shard
            break;
        case CLASS_DRUID:
            consumables[17058] = 20;  // Fish Oil
            break;
        case CLASS_SHAMAN:
            consumables[17030] = 20;  // Ankh
            break;
    }

    return consumables;
}

uint32 BotGearFactory::SelectQuality(uint32 level, uint8 slot)
{
    QualityDistribution const* dist = GetQualityDistribution(level);
    if (!dist)
        return 2;  // Default to Green

    ++_stats.qualityRolls;

    // Random quality selection based on distribution
    float roll = frand(0.0f, 100.0f);
    float cumulative = 0.0f;

    cumulative += dist->greenPercent;
    if (roll < cumulative && dist->greenPercent > 0.0f)
        return 2;  // ITEM_QUALITY_UNCOMMON (Green)

    cumulative += dist->bluePercent;
    if (roll < cumulative && dist->bluePercent > 0.0f)
        return 3;  // ITEM_QUALITY_RARE (Blue)

    if (dist->purplePercent > 0.0f)
        return 4;  // ITEM_QUALITY_EPIC (Purple)

    return 2;  // Default to Green
}

QualityDistribution const* BotGearFactory::GetQualityDistribution(uint32 level) const
{
    for (auto const& dist : _qualityDistributions)
    {
        if (level >= dist.minLevel && level <= dist.maxLevel)
            return &dist;
    }
    return nullptr;
}

uint32 BotGearFactory::SelectBestItem(uint8 cls, uint32 specId, uint32 level, uint8 slot, uint32 targetQuality)
{
    auto const* items = GetItemsForSlot(cls, specId, level, slot);
    if (!items || items->empty())
        return 0;

    ++_stats.cacheLookups;

    // Filter by quality
    auto qualityFiltered = FilterByQuality(*items, targetQuality);
    if (qualityFiltered.empty())
        qualityFiltered = *items;  // Fallback to all items

    // Select highest scored item
    if (!qualityFiltered.empty())
    {
        auto best = std::max_element(qualityFiltered.begin(), qualityFiltered.end(),
            [](CachedItem const& a, CachedItem const& b)
            {
                return a.statScore < b.statScore;
            });

        return best->itemEntry;
    }

    return 0;
}

std::vector<CachedItem> const* BotGearFactory::GetItemsForSlot(uint8 cls, uint32 specId, uint32 level, uint8 slot) const
{
    // Lookup in cache: _gearCache[cls][specId][level][slot]
    auto clsIt = _gearCache.find(cls);
    if (clsIt == _gearCache.end())
        return nullptr;

    auto specIt = clsIt->second.find(specId);
    if (specIt == clsIt->second.end())
        return nullptr;

    // Round level to nearest 5 for cache lookup
    uint32 levelBracket = (level / 5) * 5;
    if (levelBracket == 0)
        levelBracket = 1;

    auto levelIt = specIt->second.find(levelBracket);
    if (levelIt == specIt->second.end())
        return nullptr;

    auto slotIt = levelIt->second.find(slot);
    if (slotIt == levelIt->second.end())
        return nullptr;

    return &slotIt->second;
}

std::vector<CachedItem> BotGearFactory::FilterByQuality(std::vector<CachedItem> const& items, uint32 quality) const
{
    std::vector<CachedItem> filtered;
    filtered.reserve(items.size() / 3);  // Estimate

    for (auto const& item : items)
    {
        if (item.quality == quality)
            filtered.push_back(item);
    }

    return filtered;
}

std::vector<uint8> BotGearFactory::GetAllowedArmorTypes(uint8 cls) const
{
    switch (cls)
    {
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
        case CLASS_DEATH_KNIGHT:
            return {4, 3};  // Plate, Mail (for leveling)

        case CLASS_HUNTER:
        case CLASS_SHAMAN:
        case CLASS_EVOKER:
            return {3, 2};  // Mail, Leather (for leveling)

        case CLASS_ROGUE:
        case CLASS_DRUID:
        case CLASS_MONK:
        case CLASS_DEMON_HUNTER:
            return {2};  // Leather only

        case CLASS_MAGE:
        case CLASS_PRIEST:
        case CLASS_WARLOCK:
            return {1};  // Cloth only

        default:
            return {1};  // Default to Cloth
    }
}

bool BotGearFactory::IsItemAppropriate(CachedItem const& item, uint8 cls, uint32 specId, uint32 level) const
{
    // Check level requirement
    if (item.requiredLevel > level)
        return false;

    // Check armor type for armor items
    if (item.itemClass == ITEM_CLASS_ARMOR)
    {
        auto allowedTypes = GetAllowedArmorTypes(cls);
        bool armorMatch = false;
        for (uint8 allowedType : allowedTypes)
        {
            if (item.armorType == allowedType)
            {
                armorMatch = true;
                break;
            }
        }
        if (!armorMatch)
            return false;
    }

    // Additional checks can be added here (weapon types, etc.)

    return true;
}

} // namespace Playerbot
