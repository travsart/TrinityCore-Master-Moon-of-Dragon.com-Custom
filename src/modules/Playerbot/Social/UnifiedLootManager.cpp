/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "UnifiedLootManager.h"
#include "Core/PlayerBotHelpers.h"  // GetBotAI, GetGameSystems
#include "Core/Managers/GameSystemsManager.h"  // Complete type for IGameSystemsManager
#include "LootDistribution.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Random.h"
#include <algorithm>
#include <sstream>
#include <cmath>
#include "GameTime.h"
#include "ItemTemplate.h"
#include "Item.h"
#include "Player.h"
#include "SharedDefines.h"

// Note: LootAnalysis and LootCoordination were stub headers with no implementations.
// They have been removed during consolidation. Real implementations needed here.

namespace Playerbot
{

// ============================================================================
// Singleton Management
// ============================================================================

UnifiedLootManager* UnifiedLootManager::instance()
{
    static UnifiedLootManager instance;
    return &instance;
}

UnifiedLootManager::UnifiedLootManager()
    : _analysis(std::make_unique<AnalysisModule>())
    , _coordination(std::make_unique<CoordinationModule>())
    , _distribution(std::make_unique<DistributionModule>())
{
    TC_LOG_INFO("playerbot.loot", "UnifiedLootManager initialized");
}

UnifiedLootManager::~UnifiedLootManager()
{
    TC_LOG_INFO("playerbot.loot", "UnifiedLootManager shutting down");
}

// ============================================================================
// ANALYSIS MODULE IMPLEMENTATION
// ============================================================================

float UnifiedLootManager::AnalysisModule::CalculateItemValue(Player* player, LootItem const& item)
{
    if (!player || !item.itemTemplate)
    {
        _itemsAnalyzed++;
        return 0.0f;
    }

    ItemTemplate const* proto = item.itemTemplate;
    float value = 0.0f;

    // Base value from item level (normalized to 0-100 scale)
    // Max item level in TWW ~639, so divide by ~6.4 for rough scaling
    uint32 itemLevel = item.itemLevel > 0 ? item.itemLevel : proto->GetBaseItemLevel();
    value += static_cast<float>(itemLevel) / 6.4f;

    // Quality multiplier (0=poor, 1=common, 2=uncommon, 3=rare, 4=epic, 5=legendary)
    uint32 quality = item.itemQuality > 0 ? item.itemQuality : proto->GetQuality();
    float qualityMultiplier = 1.0f;
    switch (quality)
    {
        case ITEM_QUALITY_POOR:      qualityMultiplier = 0.1f; break;
        case ITEM_QUALITY_NORMAL:    qualityMultiplier = 0.3f; break;
        case ITEM_QUALITY_UNCOMMON:  qualityMultiplier = 0.6f; break;
        case ITEM_QUALITY_RARE:      qualityMultiplier = 0.85f; break;
        case ITEM_QUALITY_EPIC:      qualityMultiplier = 1.0f; break;
        case ITEM_QUALITY_LEGENDARY: qualityMultiplier = 1.15f; break;
        case ITEM_QUALITY_ARTIFACT:  qualityMultiplier = 1.2f; break;
        default:                     qualityMultiplier = 0.5f; break;
    }
    value *= qualityMultiplier;

    // Add value from stats using player's stat weights
    std::vector<std::pair<uint32, float>> priorities = GetStatPriorities(player);
    std::unordered_map<uint32, float> weightMap;
    for (auto const& [statType, weight] : priorities)
        weightMap[statType] = weight;

    for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
    {
        int32 statType = proto->GetStatModifierBonusStat(i);
        int32 statValue = proto->GetStatPercentEditor(i);
        if (statType >= 0 && statValue != 0)
        {
            float weight = 1.0f;
            auto it = weightMap.find(static_cast<uint32>(statType));
            if (it != weightMap.end())
                weight = it->second;
            value += std::abs(statValue) * weight * 0.1f;
        }
    }

    // Armor value for armor items
    if (proto->IsArmor())
    {
        uint32 armor = proto->GetArmor(itemLevel);
        value += armor * 0.01f;
    }

    // DPS value for weapons
    if (proto->IsWeapon())
    {
        float dps = proto->GetDPS(itemLevel);
        value += dps * 0.5f;
    }

    // Cap at 100
    value = std::min(value, 100.0f);

    _itemsAnalyzed++;
    return value;
}

float UnifiedLootManager::AnalysisModule::CalculateUpgradeValue(Player* player, LootItem const& item)
{
    if (!player || !item.itemTemplate)
    {
        _upgradesDetected++;
        return 0.0f;
    }

    ItemTemplate const* proto = item.itemTemplate;
    InventoryType invType = proto->GetInventoryType();

    // Find current equipped item for this slot
    uint8 slot = 0;
    switch (invType)
    {
        case INVTYPE_HEAD:          slot = EQUIPMENT_SLOT_HEAD; break;
        case INVTYPE_NECK:          slot = EQUIPMENT_SLOT_NECK; break;
        case INVTYPE_SHOULDERS:     slot = EQUIPMENT_SLOT_SHOULDERS; break;
        case INVTYPE_BODY:          slot = EQUIPMENT_SLOT_BODY; break;
        case INVTYPE_CHEST:
        case INVTYPE_ROBE:          slot = EQUIPMENT_SLOT_CHEST; break;
        case INVTYPE_WAIST:         slot = EQUIPMENT_SLOT_WAIST; break;
        case INVTYPE_LEGS:          slot = EQUIPMENT_SLOT_LEGS; break;
        case INVTYPE_FEET:          slot = EQUIPMENT_SLOT_FEET; break;
        case INVTYPE_WRISTS:        slot = EQUIPMENT_SLOT_WRISTS; break;
        case INVTYPE_HANDS:         slot = EQUIPMENT_SLOT_HANDS; break;
        case INVTYPE_FINGER:        slot = EQUIPMENT_SLOT_FINGER1; break;
        case INVTYPE_TRINKET:       slot = EQUIPMENT_SLOT_TRINKET1; break;
        case INVTYPE_CLOAK:         slot = EQUIPMENT_SLOT_BACK; break;
        case INVTYPE_WEAPON:
        case INVTYPE_2HWEAPON:
        case INVTYPE_WEAPONMAINHAND: slot = EQUIPMENT_SLOT_MAINHAND; break;
        case INVTYPE_WEAPONOFFHAND:
        case INVTYPE_SHIELD:
        case INVTYPE_HOLDABLE:      slot = EQUIPMENT_SLOT_OFFHAND; break;
        case INVTYPE_RANGED:
        case INVTYPE_THROWN:
        case INVTYPE_RANGEDRIGHT:   slot = EQUIPMENT_SLOT_MAINHAND; break;
        default:
            _upgradesDetected++;
            return 0.0f; // Not equippable
    }

    Item const* currentItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);

    // If nothing equipped, any item is an upgrade
    if (!currentItem)
    {
        _upgradesDetected++;
        return 100.0f; // Max upgrade value - empty slot
    }

    // Compare item scores
    float newScore = CalculateItemScore(player, item);

    // Create a temporary LootItem for current item
    LootItem currentLootItem;
    currentLootItem.itemId = currentItem->GetEntry();
    currentLootItem.itemTemplate = currentItem->GetTemplate();
    currentLootItem.itemLevel = currentItem->GetItemLevel(player);
    currentLootItem.itemQuality = currentItem->GetQuality();

    float currentScore = CalculateItemScore(player, currentLootItem);

    // Calculate percentage improvement
    float upgradePercent = 0.0f;
    if (currentScore > 0.0f)
        upgradePercent = ((newScore - currentScore) / currentScore) * 100.0f;
    else if (newScore > 0.0f)
        upgradePercent = 100.0f;

    if (upgradePercent > 0.0f)
        _upgradesDetected++;

    return upgradePercent;
}

bool UnifiedLootManager::AnalysisModule::IsSignificantUpgrade(Player* player, LootItem const& item)
{
    // A significant upgrade is defined as >5% improvement
    float upgradeValue = CalculateUpgradeValue(player, item);
    return upgradeValue > 5.0f;
}

float UnifiedLootManager::AnalysisModule::CalculateStatWeight(Player* player, uint32 statType)
{
    if (!player)
        return 1.0f;

    // Get class and spec for stat weight determination
    uint8 playerClass = player->GetClass();
    uint32 spec = static_cast<uint32>(player->GetPrimarySpecialization());

    // WoW 11.x (The War Within) stat priorities by role
    // These are approximate weights - 1.0 = baseline, higher = more valuable
    // Based on common theorycrafting priorities

    // Determine role from spec
    bool isTank = false;
    bool isHealer = false;
    bool isMelee = false;
    bool isCaster = false;

    // Map spec ID to role (simplified - TWW spec IDs)
    switch (playerClass)
    {
        case CLASS_WARRIOR:
            isTank = (spec == 73); // Protection
            isMelee = !isTank;
            break;
        case CLASS_PALADIN:
            isTank = (spec == 66);  // Protection
            isHealer = (spec == 65); // Holy
            isMelee = !isTank && !isHealer;
            break;
        case CLASS_HUNTER:
            isMelee = (spec == 255); // Survival
            isCaster = !isMelee; // BM/MM are more ranged
            break;
        case CLASS_ROGUE:
            isMelee = true;
            break;
        case CLASS_PRIEST:
            isHealer = (spec == 256 || spec == 257); // Discipline/Holy
            isCaster = !isHealer; // Shadow
            break;
        case CLASS_DEATH_KNIGHT:
            isTank = (spec == 250); // Blood
            isMelee = !isTank;
            break;
        case CLASS_SHAMAN:
            isHealer = (spec == 264); // Restoration
            isMelee = (spec == 263); // Enhancement
            isCaster = !isHealer && !isMelee; // Elemental
            break;
        case CLASS_MAGE:
            isCaster = true;
            break;
        case CLASS_WARLOCK:
            isCaster = true;
            break;
        case CLASS_MONK:
            isTank = (spec == 268); // Brewmaster
            isHealer = (spec == 270); // Mistweaver
            isMelee = !isTank && !isHealer; // Windwalker
            break;
        case CLASS_DRUID:
            isTank = (spec == 104); // Guardian
            isHealer = (spec == 105); // Restoration
            isMelee = (spec == 103); // Feral
            isCaster = (spec == 102); // Balance
            break;
        case CLASS_DEMON_HUNTER:
            isTank = (spec == 581); // Vengeance
            isMelee = !isTank; // Havoc
            break;
        case CLASS_EVOKER:
            isHealer = (spec == 1468); // Preservation
            isCaster = !isHealer; // Devastation/Augmentation
            break;
        default:
            isMelee = true; // Default to melee DPS
            break;
    }

    // Primary stats (class-specific)
    switch (statType)
    {
        // Primary stats
        case ITEM_MOD_AGILITY:
            if (playerClass == CLASS_ROGUE || playerClass == CLASS_HUNTER ||
                playerClass == CLASS_MONK || playerClass == CLASS_DEMON_HUNTER ||
                (playerClass == CLASS_DRUID && (spec == 103 || spec == 104)) ||
                (playerClass == CLASS_SHAMAN && spec == 263))
                return 1.5f;
            return 0.0f;

        case ITEM_MOD_STRENGTH:
            if (playerClass == CLASS_WARRIOR || playerClass == CLASS_DEATH_KNIGHT ||
                (playerClass == CLASS_PALADIN && spec != 65))
                return 1.5f;
            return 0.0f;

        case ITEM_MOD_INTELLECT:
            if (isHealer || isCaster ||
                (playerClass == CLASS_PALADIN && spec == 65) ||
                (playerClass == CLASS_SHAMAN && (spec == 262 || spec == 264)) ||
                (playerClass == CLASS_DRUID && (spec == 102 || spec == 105)) ||
                (playerClass == CLASS_MONK && spec == 270) ||
                playerClass == CLASS_MAGE || playerClass == CLASS_WARLOCK ||
                playerClass == CLASS_PRIEST || playerClass == CLASS_EVOKER)
                return 1.5f;
            return 0.0f;

        case ITEM_MOD_STAMINA:
            if (isTank) return 1.3f;
            return 0.8f;

        // Secondary stats
        case ITEM_MOD_CRIT_RATING:
            if (isTank) return 0.8f;
            if (isHealer) return 1.0f;
            return 1.2f; // DPS love crit

        case ITEM_MOD_HASTE_RATING:
            if (isHealer) return 1.3f; // Healers often prioritize haste
            if (isCaster) return 1.2f;
            return 1.1f;

        case ITEM_MOD_MASTERY_RATING:
            if (isTank) return 1.2f; // Tanks often value mastery
            return 1.0f;

        case ITEM_MOD_VERSATILITY:
            if (isTank) return 1.4f; // Tanks love versatility
            if (isHealer) return 1.1f;
            return 0.9f; // Usually lower priority for DPS

        // Tertiary/special stats
        case ITEM_MOD_DODGE_RATING:
        case ITEM_MOD_PARRY_RATING:
        case ITEM_MOD_BLOCK_RATING:
            if (isTank) return 1.3f;
            return 0.0f; // Useless for non-tanks

        case ITEM_MOD_HIT_RATING:
        case ITEM_MOD_EXPERTISE_RATING:
            return 0.0f; // Deprecated stats in modern WoW

        case ITEM_MOD_SPELL_POWER:
            if (isHealer || isCaster) return 1.3f;
            return 0.0f;

        case ITEM_MOD_ATTACK_POWER:
            if (isMelee && !isTank) return 1.2f;
            return 0.0f;

        default:
            return 1.0f;
    }
}

float UnifiedLootManager::AnalysisModule::CompareItems(Player* player, LootItem const& newItem, Item const* currentItem)
{
    if (!player || !newItem.itemTemplate)
        return 0.0f;

    if (!currentItem)
        return 100.0f; // New item is infinitely better than nothing

    // Calculate scores for both items
    float newScore = CalculateItemScore(player, newItem);

    // Create temporary LootItem for current item
    LootItem currentLootItem;
    currentLootItem.itemId = currentItem->GetEntry();
    currentLootItem.itemTemplate = currentItem->GetTemplate();
    currentLootItem.itemLevel = currentItem->GetItemLevel(player);
    currentLootItem.itemQuality = currentItem->GetQuality();

    float currentScore = CalculateItemScore(player, currentLootItem);

    // Return difference (positive = new is better, negative = current is better)
    return newScore - currentScore;
}

float UnifiedLootManager::AnalysisModule::CalculateItemScore(Player* player, LootItem const& item)
{
    if (!player || !item.itemTemplate)
        return 0.0f;

    ItemTemplate const* proto = item.itemTemplate;
    float score = 0.0f;

    // Base score from item level (heavily weighted)
    uint32 itemLevel = item.itemLevel > 0 ? item.itemLevel : proto->GetBaseItemLevel();
    score += static_cast<float>(itemLevel) * 1.5f;

    // Quality bonus
    uint32 quality = item.itemQuality > 0 ? item.itemQuality : proto->GetQuality();
    switch (quality)
    {
        case ITEM_QUALITY_POOR:      score += 0.0f; break;
        case ITEM_QUALITY_NORMAL:    score += 5.0f; break;
        case ITEM_QUALITY_UNCOMMON:  score += 15.0f; break;
        case ITEM_QUALITY_RARE:      score += 30.0f; break;
        case ITEM_QUALITY_EPIC:      score += 50.0f; break;
        case ITEM_QUALITY_LEGENDARY: score += 75.0f; break;
        case ITEM_QUALITY_ARTIFACT:  score += 100.0f; break;
        default: break;
    }

    // Calculate weighted stat contribution
    for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
    {
        int32 statType = proto->GetStatModifierBonusStat(i);
        int32 statValue = proto->GetStatPercentEditor(i);
        if (statType >= 0 && statValue != 0)
        {
            float weight = CalculateStatWeight(player, static_cast<uint32>(statType));
            score += std::abs(statValue) * weight;
        }
    }

    // Armor contribution for armor items
    if (proto->IsArmor())
    {
        uint32 armor = proto->GetArmor(itemLevel);
        // Normalize armor - plate has ~4000+ at high ilvl, cloth ~1000
        score += armor * 0.02f;
    }

    // Weapon DPS contribution
    if (proto->IsWeapon())
    {
        float dps = proto->GetDPS(itemLevel);
        score += dps * 2.0f; // Weapon DPS is very important
    }

    return score;
}

std::vector<std::pair<uint32, float>> UnifiedLootManager::AnalysisModule::GetStatPriorities(Player* player)
{
    std::vector<std::pair<uint32, float>> priorities;

    if (!player)
    {
        // Return generic priorities
        priorities.push_back({ITEM_MOD_STAMINA, 1.0f});
        priorities.push_back({ITEM_MOD_CRIT_RATING, 1.0f});
        priorities.push_back({ITEM_MOD_HASTE_RATING, 1.0f});
        priorities.push_back({ITEM_MOD_MASTERY_RATING, 1.0f});
        priorities.push_back({ITEM_MOD_VERSATILITY, 1.0f});
        return priorities;
    }

    // Build priority list using CalculateStatWeight for each relevant stat
    std::vector<uint32> relevantStats = {
        ITEM_MOD_AGILITY,
        ITEM_MOD_STRENGTH,
        ITEM_MOD_INTELLECT,
        ITEM_MOD_STAMINA,
        ITEM_MOD_CRIT_RATING,
        ITEM_MOD_HASTE_RATING,
        ITEM_MOD_MASTERY_RATING,
        ITEM_MOD_VERSATILITY,
        ITEM_MOD_DODGE_RATING,
        ITEM_MOD_PARRY_RATING,
        ITEM_MOD_BLOCK_RATING,
        ITEM_MOD_SPELL_POWER,
        ITEM_MOD_ATTACK_POWER
    };

    for (uint32 stat : relevantStats)
    {
        float weight = CalculateStatWeight(player, stat);
        if (weight > 0.0f)
            priorities.push_back({stat, weight});
    }

    // Sort by weight (descending)
    std::sort(priorities.begin(), priorities.end(),
        [](std::pair<uint32, float> const& a, std::pair<uint32, float> const& b)
        {
            return a.second > b.second;
        });

    return priorities;
}

// ============================================================================
// COORDINATION MODULE IMPLEMENTATION
// ============================================================================

void UnifiedLootManager::CoordinationModule::InitiateLootSession(Group* group, Loot* loot)
{
    std::lock_guard<decltype(_sessionMutex)> lock(_sessionMutex);

    // TODO: Implement loot session initiation
    // LootCoordination was a stub interface with no implementation - removed during consolidation

    // Track in unified system
    uint32 sessionId = _nextSessionId++;
    LootSession session(sessionId, group ? group->GetGUID().GetCounter() : 0);
    _activeSessions[sessionId] = session;
    _sessionsCreated++;
}

void UnifiedLootManager::CoordinationModule::ProcessLootSession(Group* group, uint32 lootSessionId)
{
    // TODO: Implement loot session processing
    // LootCoordination was a stub interface with no implementation - removed during consolidation
}

void UnifiedLootManager::CoordinationModule::CompleteLootSession(uint32 lootSessionId)
{
    std::lock_guard<decltype(_sessionMutex)> lock(_sessionMutex);

    // TODO: Implement loot session completion
    // LootCoordination was a stub interface with no implementation - removed during consolidation

    // Clean up from unified tracking
    auto it = _activeSessions.find(lootSessionId);
    if (it != _activeSessions.end())
    {
        _activeSessions.erase(it);
        _sessionsCompleted++;
    }
}

void UnifiedLootManager::CoordinationModule::HandleLootSessionTimeout(uint32 lootSessionId)
{
    // TODO: Implement loot session timeout handling
    // LootCoordination was a stub interface with no implementation - removed during consolidation
    CompleteLootSession(lootSessionId); // Cleanup
}

void UnifiedLootManager::CoordinationModule::OrchestrateLootDistribution(Group* group, std::vector<LootItem> const& items)
{
    // TODO: Implement loot distribution orchestration
    // LootCoordination was a stub interface with no implementation - removed during consolidation
}

void UnifiedLootManager::CoordinationModule::PrioritizeLootDistribution(Group* group, std::vector<LootItem>& items)
{
    // TODO: Implement loot distribution prioritization
    // LootCoordination was a stub interface with no implementation - removed during consolidation
}

void UnifiedLootManager::CoordinationModule::OptimizeLootSequence(Group* group, std::vector<LootItem>& items)
{
    // TODO: Implement loot sequence optimization
    // LootCoordination was a stub interface with no implementation - removed during consolidation
}

void UnifiedLootManager::CoordinationModule::FacilitateGroupLootDiscussion(Group* group, LootItem const& item)
{
    // TODO: Implement group loot discussion facilitation
    // LootCoordination was a stub interface with no implementation - removed during consolidation
}

void UnifiedLootManager::CoordinationModule::HandleLootConflictResolution(Group* group, LootItem const& item)
{
    // TODO: Implement loot conflict resolution
    // LootCoordination was a stub interface with no implementation - removed during consolidation
}

void UnifiedLootManager::CoordinationModule::BroadcastLootRecommendations(Group* group, LootItem const& item)
{
    // TODO: Implement loot recommendation broadcasting
    // LootCoordination was a stub interface with no implementation - removed during consolidation
}

void UnifiedLootManager::CoordinationModule::OptimizeLootEfficiency(Group* group)
{
    // TODO: Implement loot efficiency optimization
    // LootCoordination was a stub interface with no implementation - removed during consolidation
}

void UnifiedLootManager::CoordinationModule::MinimizeLootTime(Group* group, uint32 sessionId)
{
    // TODO: Implement loot time minimization
    // LootCoordination was a stub interface with no implementation - removed during consolidation
}

void UnifiedLootManager::CoordinationModule::MaximizeLootFairness(Group* group, uint32 sessionId)
{
    // TODO: Implement loot fairness maximization
    // LootCoordination was a stub interface with no implementation - removed during consolidation
}

// ============================================================================
// DISTRIBUTION MODULE IMPLEMENTATION
// ============================================================================

void UnifiedLootManager::DistributionModule::DistributeLoot(Group* group, LootItem const& item)
{
    if (!group)
    {
        TC_LOG_ERROR("playerbot.loot", "DistributeLoot called with null group");
        return;
    }

    LootMethod method = group->GetLootMethod();

    TC_LOG_DEBUG("playerbot.loot", "Distributing loot item {} (id: {}) for group {} using method {}",
        item.itemName, item.itemId, group->GetGUID().GetCounter(), static_cast<uint32>(method));

    switch (method)
    {
        case MASTER_LOOT:
            HandleMasterLoot(group, item);
            break;

        case GROUP_LOOT:
        case NEED_BEFORE_GREED:
            HandleGroupLoot(group, item);
            break;

        case FREE_FOR_ALL:
            // In free-for-all, first bot to loot gets it - no coordination needed
            TC_LOG_DEBUG("playerbot.loot", "Free-for-all loot - no distribution needed");
            break;

        case ROUND_ROBIN:
            // Round robin is handled by game's loot system
            TC_LOG_DEBUG("playerbot.loot", "Round-robin loot - handled by game system");
            break;

        case PERSONAL_LOOT:
            // Personal loot is auto-assigned by game
            TC_LOG_DEBUG("playerbot.loot", "Personal loot - auto-assigned by game");
            break;

        default:
            TC_LOG_WARN("playerbot.loot", "Unknown loot method: {}", static_cast<uint32>(method));
            break;
    }

    _itemsDistributed++;
}

void UnifiedLootManager::DistributionModule::HandleLootRoll(Player* player, uint32 rollId, LootRollType rollType)
{
    std::lock_guard<decltype(_rollMutex)> lock(_rollMutex);

    (GetGameSystems(player) ? GetGameSystems(player)->GetLootDistribution()->ProcessPlayerLootDecision(rollId, rollType) : decltype(GetGameSystems(player)->GetLootDistribution()->ProcessPlayerLootDecision(rollId, rollType))());
    _rollsProcessed++;
}

LootRollType UnifiedLootManager::DistributionModule::DetermineLootDecision(
    Player* player, LootItem const& item, LootDecisionStrategy strategy)
{
    // Strategy parameter is ignored - each bot uses its own profile strategy
    return (GetGameSystems(player) ? GetGameSystems(player)->GetLootDistribution()->DetermineLootDecision(item) : decltype(GetGameSystems(player)->GetLootDistribution()->DetermineLootDecision(item))());
}

LootPriority UnifiedLootManager::DistributionModule::CalculateLootPriority(Player* player, LootItem const& item)
{
    return (GetGameSystems(player) ? GetGameSystems(player)->GetLootDistribution()->AnalyzeItemPriority(item) : decltype(GetGameSystems(player)->GetLootDistribution()->AnalyzeItemPriority(item))());
}

bool UnifiedLootManager::DistributionModule::ShouldRollNeed(Player* player, LootItem const& item)
{
    return (GetGameSystems(player) ? GetGameSystems(player)->GetLootDistribution()->CanPlayerNeedItem(item) : decltype(GetGameSystems(player)->GetLootDistribution()->CanPlayerNeedItem(item))());
}

bool UnifiedLootManager::DistributionModule::ShouldRollGreed(Player* player, LootItem const& item)
{
    return (GetGameSystems(player) ? GetGameSystems(player)->GetLootDistribution()->ShouldPlayerGreedItem(item) : decltype(GetGameSystems(player)->GetLootDistribution()->ShouldPlayerGreedItem(item))());
}

bool UnifiedLootManager::DistributionModule::IsItemForClass(Player* player, LootItem const& item)
{
    return (GetGameSystems(player) ? GetGameSystems(player)->GetLootDistribution()->IsClassAppropriate(item) : decltype(GetGameSystems(player)->GetLootDistribution()->IsClassAppropriate(item))());
}

bool UnifiedLootManager::DistributionModule::IsItemForMainSpec(Player* player, LootItem const& item)
{
    return (GetGameSystems(player) ? GetGameSystems(player)->GetLootDistribution()->IsItemForMainSpec(item) : decltype(GetGameSystems(player)->GetLootDistribution()->IsItemForMainSpec(item))());
}

bool UnifiedLootManager::DistributionModule::IsItemForOffSpec(Player* player, LootItem const& item)
{
    return (GetGameSystems(player) ? GetGameSystems(player)->GetLootDistribution()->IsItemForOffSpec(item) : decltype(GetGameSystems(player)->GetLootDistribution()->IsItemForOffSpec(item))());
}

// ============================================================================
// HELPER METHODS - Group Loot Coordination
// ============================================================================

void UnifiedLootManager::DistributionModule::HandleMasterLoot(Group* group, LootItem const& item)
{
    if (!group)
        return;

    // Get master looter
    ObjectGuid masterLooterGuid = group->GetMasterLooterGuid();
    Player* masterLooter = ObjectAccessor::FindPlayer(masterLooterGuid);

    if (!masterLooter)
    {
        TC_LOG_ERROR("playerbot.loot", "Master looter not found for group {}", group->GetGUID().GetCounter());
        return;
    }

    // Evaluate all bots in the group
    std::vector<BotRollEvaluation> evaluations;

    for (GroupReference const& ref : group->GetMembers())
    {
        Player* member = ref.GetSource();
        if (!member || !GetBotAI(member))
            continue; // Skip non-bots and human players

        IGameSystemsManager* systems = GetGameSystems(member);
        if (!systems)
            continue;

        // Calculate upgrade value for this bot
        float upgradeValue = UnifiedLootManager::instance()->CalculateUpgradeValue(member, item);
        LootPriority priority = CalculateLootPriority(member, item);
        LootRollType recommendedRoll = DetermineLootDecision(member, item, LootDecisionStrategy::UPGRADE_PRIORITY);

        BotRollEvaluation eval;
        eval.bot = member;
        eval.rollType = recommendedRoll;
        eval.rollValue = 0; // Master loot doesn't use random rolls
        eval.upgradeValue = upgradeValue;
        eval.priority = priority;

        evaluations.push_back(eval);
    }

    if (evaluations.empty())
    {
        TC_LOG_DEBUG("playerbot.loot", "No eligible bots for master loot item {}", item.itemId);
        return;
    }

    // Sort by priority (lower enum value = higher priority) then by upgrade value
    std::sort(evaluations.begin(), evaluations.end(),
        [](BotRollEvaluation const& a, BotRollEvaluation const& b)
        {
            if (a.priority != b.priority)
                return a.priority < b.priority; // Higher priority (lower enum)
            return a.upgradeValue > b.upgradeValue; // Higher upgrade value
        });

    // Award to highest priority bot
    Player* winner = evaluations[0].bot;
    TC_LOG_INFO("playerbot.loot", "Master loot: Awarding item {} to {} (priority: {}, upgrade: {:.1f}%)",
        item.itemId, winner->GetName(), static_cast<uint32>(evaluations[0].priority), evaluations[0].upgradeValue);

    // TODO: Actually award the item via game's loot system
    // This requires integration with TrinityCore's loot distribution
}

void UnifiedLootManager::DistributionModule::HandleGroupLoot(Group* group, LootItem const& item)
{
    if (!group)
        return;

    std::lock_guard<decltype(_rollMutex)> lock(_rollMutex);

    // Create a new loot roll session
    uint32 rollId = _nextRollId++;
    LootRoll roll(rollId);
    roll.itemId = item.itemId;
    roll.lootSlot = item.lootSlot;
    roll.groupId = static_cast<uint32>(group->GetGUID().GetCounter());

    // Collect rolls from all bots in the group
    std::vector<BotRollEvaluation> evaluations;

    for (GroupReference const& ref : group->GetMembers())
    {
        Player* member = ref.GetSource();
        if (!member || !GetBotAI(member))
            continue; // Skip non-bots and human players

        IGameSystemsManager* systems = GetGameSystems(member);
        if (!systems)
            continue;

        // Determine bot's roll decision
        LootRollType rollType = DetermineLootDecision(member, item,
            group->GetLootMethod() == NEED_BEFORE_GREED ?
                LootDecisionStrategy::NEED_BEFORE_GREED :
                LootDecisionStrategy::UPGRADE_PRIORITY);

        // Generate roll value (1-100) for Need/Greed
        uint32 rollValue = 0;
        if (rollType == LootRollType::NEED || rollType == LootRollType::GREED)
        {
            rollValue = urand(1, 100);
        }

        // Calculate upgrade metrics
        float upgradeValue = UnifiedLootManager::instance()->CalculateUpgradeValue(member, item);
        LootPriority priority = CalculateLootPriority(member, item);

        BotRollEvaluation eval;
        eval.bot = member;
        eval.rollType = rollType;
        eval.rollValue = rollValue;
        eval.upgradeValue = upgradeValue;
        eval.priority = priority;

        evaluations.push_back(eval);

        // Record in roll tracking
        roll.playerRolls[member->GetGUID().GetCounter()] = rollType;
        roll.rollValues[member->GetGUID().GetCounter()] = rollValue;

        TC_LOG_DEBUG("playerbot.loot", "Bot {} rolled {} (value: {}) for item {}",
            member->GetName(), static_cast<uint32>(rollType), rollValue, item.itemId);
    }

    if (evaluations.empty())
    {
        TC_LOG_DEBUG("playerbot.loot", "No eligible bots for group loot item {}", item.itemId);
        return;
    }

    // Store roll for potential tie resolution
    _activeRolls[rollId] = roll;

    // Determine winner
    Player* winner = DetermineGroupLootWinner(evaluations);
    if (winner)
    {
        uint32 winnerGuid = winner->GetGUID().GetCounter();
        roll.isComplete = true;

        TC_LOG_INFO("playerbot.loot", "Group loot: {} won item {} with roll type {}",
            winner->GetName(), item.itemId,
            static_cast<uint32>(roll.playerRolls[winnerGuid]));

        // TODO: Actually award the item via game's loot system
    }

    _rollsProcessed++;
}

Player* UnifiedLootManager::DistributionModule::DetermineGroupLootWinner(std::vector<BotRollEvaluation>& rolls)
{
    if (rolls.empty())
        return nullptr;

    // Separate rolls by type: Need > Greed > Disenchant > Pass
    std::vector<BotRollEvaluation*> needRolls;
    std::vector<BotRollEvaluation*> greedRolls;
    std::vector<BotRollEvaluation*> disenchantRolls;

    for (auto& roll : rolls)
    {
        if (roll.rollType == LootRollType::NEED)
            needRolls.push_back(&roll);
        else if (roll.rollType == LootRollType::GREED)
            greedRolls.push_back(&roll);
        else if (roll.rollType == LootRollType::DISENCHANT)
            disenchantRolls.push_back(&roll);
        // PASS rolls are excluded
    }

    // Determine winning pool (Need takes priority over Greed over Disenchant)
    std::vector<BotRollEvaluation*>* winningPool = nullptr;
    const char* rollTypeName = "None";

    if (!needRolls.empty())
    {
        winningPool = &needRolls;
        rollTypeName = "NEED";
    }
    else if (!greedRolls.empty())
    {
        winningPool = &greedRolls;
        rollTypeName = "GREED";
    }
    else if (!disenchantRolls.empty())
    {
        winningPool = &disenchantRolls;
        rollTypeName = "DISENCHANT";
    }

    if (!winningPool || winningPool->empty())
        return nullptr;

    // Sort by roll value (highest wins)
    std::sort(winningPool->begin(), winningPool->end(),
        [](BotRollEvaluation* a, BotRollEvaluation* b)
        {
            return a->rollValue > b->rollValue;
        });

    // Check for ties
    BotRollEvaluation* winner = (*winningPool)[0];
    std::vector<BotRollEvaluation*> tiedRolls;
    tiedRolls.push_back(winner);

    for (size_t i = 1; i < winningPool->size(); ++i)
    {
        if ((*winningPool)[i]->rollValue == winner->rollValue)
            tiedRolls.push_back((*winningPool)[i]);
        else
            break;
    }

    // If there's a tie, resolve by upgrade value
    if (tiedRolls.size() > 1)
    {
        TC_LOG_DEBUG("playerbot.loot", "Tie detected between {} bots with roll value {}, resolving by upgrade value",
            tiedRolls.size(), winner->rollValue);

        std::sort(tiedRolls.begin(), tiedRolls.end(),
            [](BotRollEvaluation* a, BotRollEvaluation* b)
            {
                return a->upgradeValue > b->upgradeValue;
            });

        winner = tiedRolls[0];
    }

    TC_LOG_DEBUG("playerbot.loot", "Winner determined: {} ({} roll: {}, upgrade: {:.1f}%)",
        winner->bot->GetName(), rollTypeName, winner->rollValue, winner->upgradeValue);

    return winner->bot;
}

void UnifiedLootManager::DistributionModule::ExecuteLootDistribution(Group* group, uint32 rollId)
{
    if (!group)
    {
        TC_LOG_ERROR("playerbot.loot", "ExecuteLootDistribution called with null group");
        return;
    }

    std::lock_guard<decltype(_rollMutex)> lock(_rollMutex);

    auto it = _activeRolls.find(rollId);
    if (it == _activeRolls.end())
    {
        TC_LOG_WARN("playerbot.loot", "ExecuteLootDistribution: Roll ID {} not found", rollId);
        return;
    }

    LootRoll& roll = it->second;

    if (roll.isComplete)
    {
        TC_LOG_DEBUG("playerbot.loot", "Roll {} already completed", rollId);
        // TODO: Award item to winner via game's loot system
        // Player* winner = ObjectAccessor::FindPlayer(ObjectGuid(HighGuid::Player, roll.winnerGuid));
        // if (winner) { /* Award item */ }
    }
    else
    {
        TC_LOG_WARN("playerbot.loot", "ExecuteLootDistribution: Roll {} not yet completed", rollId);
    }

    // Clean up completed roll
    _activeRolls.erase(it);
}

void UnifiedLootManager::DistributionModule::ResolveRollTies(Group* group, uint32 rollId)
{
    if (!group)
    {
        TC_LOG_ERROR("playerbot.loot", "ResolveRollTies called with null group");
        return;
    }

    std::lock_guard<decltype(_rollMutex)> lock(_rollMutex);

    auto it = _activeRolls.find(rollId);
    if (it == _activeRolls.end())
    {
        TC_LOG_WARN("playerbot.loot", "ResolveRollTies: Roll ID {} not found", rollId);
        return;
    }

    LootRoll& roll = it->second;

    // Find all players with the highest roll value
    uint32 highestRoll = 0;
    std::vector<uint32> tiedPlayers;

    for (auto const& [playerGuid, rollValue] : roll.rollValues)
    {
        if (rollValue > highestRoll)
        {
            highestRoll = rollValue;
            tiedPlayers.clear();
            tiedPlayers.push_back(playerGuid);
        }
        else if (rollValue == highestRoll && rollValue > 0)
        {
            tiedPlayers.push_back(playerGuid);
        }
    }

    if (tiedPlayers.size() <= 1)
    {
        TC_LOG_DEBUG("playerbot.loot", "No tie to resolve for roll {}", rollId);
        return;
    }

    TC_LOG_INFO("playerbot.loot", "Resolving tie for roll {} between {} players with value {}",
        rollId, tiedPlayers.size(), highestRoll);

    // Re-roll for tied players
    std::vector<std::pair<uint32, uint32>> rerolls; // playerGuid, rollValue
    for (uint32 playerGuid : tiedPlayers)
    {
        uint32 newRoll = urand(1, 100);
        rerolls.push_back({playerGuid, newRoll});
        TC_LOG_DEBUG("playerbot.loot", "Player {} re-rolled {}", playerGuid, newRoll);
    }

    // Find highest re-roll
    std::sort(rerolls.begin(), rerolls.end(),
        [](std::pair<uint32, uint32> const& a, std::pair<uint32, uint32> const& b)
        {
            return a.second > b.second;
        });

    // Mark roll as complete
    uint32 winnerGuid = rerolls[0].first;
    roll.isComplete = true;

    TC_LOG_INFO("playerbot.loot", "Tie resolved: Player {} won with re-roll {}",
        winnerGuid, rerolls[0].second);
}

void UnifiedLootManager::DistributionModule::HandleLootNinja(Group* group, uint32 suspectedPlayer)
{
    if (!group)
    {
        TC_LOG_ERROR("playerbot.loot", "HandleLootNinja called with null group");
        return;
    }

    ObjectGuid suspectGuid = ObjectGuid::Create<HighGuid::Player>(suspectedPlayer);
    Player* suspect = ObjectAccessor::FindPlayer(suspectGuid);
    if (!suspect)
    {
        TC_LOG_WARN("playerbot.loot", "HandleLootNinja: Suspected player {} not found", suspectedPlayer);
        return;
    }

    // Log the ninja loot attempt
    TC_LOG_WARN("playerbot.loot", "Potential ninja loot detected: Player {} in group {}",
        suspect->GetName(), group->GetGUID().GetCounter());

    // For bots, this is primarily for logging and detection
    // Human players would handle actual consequences

    // Check recent loot history for suspicious patterns
    std::lock_guard<decltype(_rollMutex)> lock(_rollMutex);

    uint32 recentNeedRolls = 0;
    uint32 recentWins = 0;

    for (auto const& [rollId, roll] : _activeRolls)
    {
        if (roll.groupId != group->GetGUID().GetCounter())
            continue;

        auto playerRollIt = roll.playerRolls.find(suspectedPlayer);
        if (playerRollIt != roll.playerRolls.end())
        {
            if (playerRollIt->second == LootRollType::NEED)
                ++recentNeedRolls;

            // Note: Winner tracking not implemented in simplified LootRoll struct
            // TODO: Implement winner tracking if needed for ninja detection
        }
    }

    // Simple heuristic: If player has won more than 50% of recent rolls, flag as suspicious
    if (recentNeedRolls > 0 && recentWins > (recentNeedRolls / 2))
    {
        TC_LOG_WARN("playerbot.loot", "Player {} has suspicious loot pattern: {} wins out of {} need rolls",
            suspect->GetName(), recentWins, recentNeedRolls);

        // For bots, we could implement automatic remediation:
        // - Reduce their loot priority temporarily
        // - Flag for review
        // - Notify group leader (if human)

        // For now, just log for monitoring
    }
    else
    {
        TC_LOG_DEBUG("playerbot.loot", "Player {} loot pattern appears normal: {} wins out of {} need rolls",
            suspect->GetName(), recentWins, recentNeedRolls);
    }
}

// ============================================================================
// UNIFIED MANAGER PUBLIC INTERFACE - Analysis
// ============================================================================

float UnifiedLootManager::CalculateItemValue(Player* player, LootItem const& item)
{
    return _analysis->CalculateItemValue(player, item);
}

float UnifiedLootManager::CalculateUpgradeValue(Player* player, LootItem const& item)
{
    return _analysis->CalculateUpgradeValue(player, item);
}

bool UnifiedLootManager::IsSignificantUpgrade(Player* player, LootItem const& item)
{
    return _analysis->IsSignificantUpgrade(player, item);
}

float UnifiedLootManager::CalculateStatWeight(Player* player, uint32 statType)
{
    return _analysis->CalculateStatWeight(player, statType);
}

float UnifiedLootManager::CompareItems(Player* player, LootItem const& newItem, Item const* currentItem)
{
    return _analysis->CompareItems(player, newItem, currentItem);
}

float UnifiedLootManager::CalculateItemScore(Player* player, LootItem const& item)
{
    return _analysis->CalculateItemScore(player, item);
}

std::vector<std::pair<uint32, float>> UnifiedLootManager::GetStatPriorities(Player* player)
{
    return _analysis->GetStatPriorities(player);
}

// ============================================================================
// UNIFIED MANAGER PUBLIC INTERFACE - Coordination
// ============================================================================

void UnifiedLootManager::InitiateLootSession(Group* group, Loot* loot)
{
    _coordination->InitiateLootSession(group, loot);
}

void UnifiedLootManager::ProcessLootSession(Group* group, uint32 lootSessionId)
{
    _coordination->ProcessLootSession(group, lootSessionId);
}

void UnifiedLootManager::CompleteLootSession(uint32 lootSessionId)
{
    _coordination->CompleteLootSession(lootSessionId);
}

void UnifiedLootManager::HandleLootSessionTimeout(uint32 lootSessionId)
{
    _coordination->HandleLootSessionTimeout(lootSessionId);
}

void UnifiedLootManager::OrchestrateLootDistribution(Group* group, std::vector<LootItem> const& items)
{
    _coordination->OrchestrateLootDistribution(group, items);
}

void UnifiedLootManager::PrioritizeLootDistribution(Group* group, std::vector<LootItem>& items)
{
    _coordination->PrioritizeLootDistribution(group, items);
}

void UnifiedLootManager::OptimizeLootSequence(Group* group, std::vector<LootItem>& items)
{
    _coordination->OptimizeLootSequence(group, items);
}

void UnifiedLootManager::FacilitateGroupLootDiscussion(Group* group, LootItem const& item)
{
    _coordination->FacilitateGroupLootDiscussion(group, item);
}

void UnifiedLootManager::HandleLootConflictResolution(Group* group, LootItem const& item)
{
    _coordination->HandleLootConflictResolution(group, item);
}

void UnifiedLootManager::BroadcastLootRecommendations(Group* group, LootItem const& item)
{
    _coordination->BroadcastLootRecommendations(group, item);
}

void UnifiedLootManager::OptimizeLootEfficiency(Group* group)
{
    _coordination->OptimizeLootEfficiency(group);
}

void UnifiedLootManager::MinimizeLootTime(Group* group, uint32 sessionId)
{
    _coordination->MinimizeLootTime(group, sessionId);
}

void UnifiedLootManager::MaximizeLootFairness(Group* group, uint32 sessionId)
{
    _coordination->MaximizeLootFairness(group, sessionId);
}

// ============================================================================
// UNIFIED MANAGER PUBLIC INTERFACE - Distribution
// ============================================================================

void UnifiedLootManager::DistributeLoot(Group* group, LootItem const& item)
{
    _distribution->DistributeLoot(group, item);
}

void UnifiedLootManager::HandleLootRoll(Player* player, uint32 rollId, LootRollType rollType)
{
    _distribution->HandleLootRoll(player, rollId, rollType);
}

LootRollType UnifiedLootManager::DetermineLootDecision(
    Player* player, LootItem const& item, LootDecisionStrategy strategy)
{
    return _distribution->DetermineLootDecision(player, item, strategy);
}

LootPriority UnifiedLootManager::CalculateLootPriority(Player* player, LootItem const& item)
{
    return _distribution->CalculateLootPriority(player, item);
}

bool UnifiedLootManager::ShouldRollNeed(Player* player, LootItem const& item)
{
    return _distribution->ShouldRollNeed(player, item);
}

bool UnifiedLootManager::ShouldRollGreed(Player* player, LootItem const& item)
{
    return _distribution->ShouldRollGreed(player, item);
}

bool UnifiedLootManager::IsItemForClass(Player* player, LootItem const& item)
{
    return _distribution->IsItemForClass(player, item);
}

bool UnifiedLootManager::IsItemForMainSpec(Player* player, LootItem const& item)
{
    return _distribution->IsItemForMainSpec(player, item);
}

bool UnifiedLootManager::IsItemForOffSpec(Player* player, LootItem const& item)
{
    return _distribution->IsItemForOffSpec(player, item);
}

void UnifiedLootManager::ExecuteLootDistribution(Group* group, uint32 rollId)
{
    _distribution->ExecuteLootDistribution(group, rollId);
}

void UnifiedLootManager::ResolveRollTies(Group* group, uint32 rollId)
{
    _distribution->ResolveRollTies(group, rollId);
}

void UnifiedLootManager::HandleLootNinja(Group* group, uint32 suspectedPlayer)
{
    _distribution->HandleLootNinja(group, suspectedPlayer);
}

// ============================================================================
// UNIFIED OPERATIONS
// ============================================================================

void UnifiedLootManager::ProcessCompleteLootFlow(Group* group, Loot* loot)
{
    std::lock_guard<decltype(_mutex)> lock(_mutex);

    auto startTime = GameTime::GetGameTimeMS();
    _totalOperations++;

    // Step 1: Initiate session (Coordination)
    InitiateLootSession(group, loot);

    // Step 2: Analyze items (Analysis)
    // This happens automatically when players evaluate items

    // Step 3: Execute distribution (Distribution)
    // This happens when players roll

    auto endTime = GameTime::GetGameTimeMS();
    _totalProcessingTimeMs += (endTime - startTime);

    TC_LOG_DEBUG("playerbot.loot", "Processed complete loot flow in {} ms", endTime - startTime);
}

std::string UnifiedLootManager::GetLootRecommendation(Player* player, LootItem const& item)
{
    std::ostringstream oss;

    // Analysis
    float itemValue = CalculateItemValue(player, item);
    float upgradeValue = CalculateUpgradeValue(player, item);
    bool isSignificantUpgrade = IsSignificantUpgrade(player, item);

    // Distribution decision
    LootPriority priority = CalculateLootPriority(player, item);
    LootRollType recommendedRoll = DetermineLootDecision(player, item, LootDecisionStrategy::UPGRADE_PRIORITY);

    // Format recommendation
    oss << "Item Value: " << itemValue << "/100\n";
    oss << "Upgrade: " << upgradeValue << "% improvement\n";
    oss << "Significant Upgrade: " << (isSignificantUpgrade ? "Yes" : "No") << "\n";
    oss << "Priority: " << static_cast<uint32>(priority) << "\n";
    oss << "Recommended Action: ";

    switch (recommendedRoll)
    {
        case LootRollType::NEED:       oss << "NEED"; break;
        case LootRollType::GREED:      oss << "GREED"; break;
        case LootRollType::PASS:       oss << "PASS"; break;
        case LootRollType::DISENCHANT: oss << "DISENCHANT"; break;
        default:                       oss << "UNKNOWN"; break;
    }

    return oss.str();
}

std::string UnifiedLootManager::GetLootStatistics() const
{
    std::ostringstream oss;

    oss << "=== UnifiedLootManager Statistics ===\n";
    oss << "Total Operations: " << _totalOperations.load() << "\n";
    oss << "Total Processing Time: " << _totalProcessingTimeMs.load() << " ms\n";

    if (_totalOperations > 0)
    {
        oss << "Average Processing Time: "
            << (_totalProcessingTimeMs.load() / _totalOperations.load()) << " ms/operation\n";
    }

    oss << "\n--- Analysis Module ---\n";
    // Note: _itemsAnalyzed is private - using placeholder
    // TODO: Add public getter method or make statistics public
    oss << "Items Analyzed: " << "(statistics unavailable)" << "\n";
    oss << "Upgrades Detected: " << "(statistics unavailable)" << "\n";

    oss << "\n--- Coordination Module ---\n";
    // Note: _sessionsCreated/_sessionsCompleted are private
    // TODO: Add public getter methods for statistics
    oss << "Sessions Created: " << "(statistics unavailable)" << "\n";
    oss << "Sessions Completed: " << "(statistics unavailable)" << "\n";
    oss << "Active Sessions: " << "(statistics unavailable)" << "\n";

    oss << "\n--- Distribution Module ---\n";
    // Note: _rollsProcessed/_itemsDistributed are private
    oss << "Rolls Processed: " << "(statistics unavailable)" << "\n";
    oss << "Items Distributed: " << "(statistics unavailable)" << "\n";

    return oss.str();
}

} // namespace Playerbot
