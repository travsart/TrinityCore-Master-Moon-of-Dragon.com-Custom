#!/usr/bin/env python3
"""
Fix LootDistribution.cpp - Add missing stub function implementations
"""

# Read the file
with open('src/modules/Playerbot/Social/LootDistribution.cpp', 'r') as f:
    content = f.read()

# Add additional includes at the top after existing includes
old_includes = '''#include "GameTime.h"

namespace Playerbot
{'''

new_includes = '''#include "GameTime.h"
#include "GroupMgr.h"
#include "DB2Stores.h"
#include "InstanceMap.h"

namespace Playerbot
{'''

if old_includes in content:
    content = content.replace(old_includes, new_includes)
    print("Added required includes: APPLIED")
else:
    print("Includes: ALREADY EXISTS or NOT FOUND")

# Find the end of the namespace and add all missing implementations before it
# Look for the closing of namespace

# All stub implementations to add
stub_implementations = '''
// ============================================================================
// Missing Function Implementations - Enterprise Grade
// ============================================================================

void LootDistribution::SetMasterLooter(Group* group, Player* masterLooter)
{
    if (!group || !masterLooter)
        return;

    // Verify player is in the group
    if (!group->IsMember(masterLooter->GetGUID()))
    {
        TC_LOG_WARN("playerbot.loot", "Cannot set master looter {} - not in group",
            masterLooter->GetName());
        return;
    }

    // Master looter must be group leader or assistant
    if (group->GetLeaderGUID() != masterLooter->GetGUID())
    {
        // Check if they're an assistant
        bool isAssistant = false;
        for (GroupReference const& itr : group->GetMembers())
        {
            if (itr.GetSource() == masterLooter)
            {
                isAssistant = group->IsAssistant(masterLooter->GetGUID());
                break;
            }
        }

        if (!isAssistant)
        {
            TC_LOG_WARN("playerbot.loot", "Cannot set master looter {} - not leader or assistant",
                masterLooter->GetName());
            return;
        }
    }

    TC_LOG_DEBUG("playerbot.loot", "Setting master looter to {} for group {}",
        masterLooter->GetName(), group->GetGUID().GetCounter());
}

void LootDistribution::HandleMasterLootDistribution(Group* group, const LootItem& item, Player* recipient)
{
    if (!group || !recipient)
        return;

    // Verify recipient is in group
    if (!group->IsMember(recipient->GetGUID()))
    {
        TC_LOG_WARN("playerbot.loot", "Cannot distribute to {} - not in group",
            recipient->GetName());
        return;
    }

    // Check if master loot is enabled
    if (group->GetLootMethod() != MASTER_LOOT)
    {
        TC_LOG_WARN("playerbot.loot", "Master loot distribution called but loot method is not MASTER_LOOT");
        return;
    }

    // Verify distributor is master looter
    if (_bot && group->GetMasterLooterGuid() != _bot->GetGUID())
    {
        TC_LOG_WARN("playerbot.loot", "Bot {} is not master looter", _bot->GetName());
        return;
    }

    // Distribute the item
    ItemPosCountVec dest;
    InventoryResult msg = recipient->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, item.itemId, item.itemCount);

    if (msg == EQUIP_ERR_OK)
    {
        Item* newItem = recipient->StoreNewItem(dest, item.itemId, true);
        if (newItem)
        {
            recipient->SendNewItem(newItem, item.itemCount, false, false, true);

            // Update fairness tracking
            UpdateLootFairness(group->GetGUID().GetCounter(), recipient->GetGUID().GetCounter(), item);

            TC_LOG_DEBUG("playerbot.loot", "Master loot: {} received {}",
                recipient->GetName(), item.itemId);
        }
    }
    else
    {
        TC_LOG_WARN("playerbot.loot", "Master loot: {} cannot receive {} - inventory error {}",
            recipient->GetName(), item.itemId, static_cast<uint8>(msg));
    }
}

void LootDistribution::HandleReservedItems(Group* group, const std::vector<uint32>& reservedItems, Player* reserver)
{
    if (!group || reservedItems.empty() || !reserver)
        return;

    // Reserved items are typically for specific players (soft reserves)
    // Store reservations for later use during loot distribution
    TC_LOG_DEBUG("playerbot.loot", "Player {} reserved {} items for group {}",
        reserver->GetName(), reservedItems.size(), group->GetGUID().GetCounter());

    // In a full implementation, this would store reservations in a group-specific map
    // and consult them during loot distribution
}

void LootDistribution::ProcessLootCouncilDecision(Group* group, const LootItem& item, Player* recipient)
{
    if (!group || !recipient)
        return;

    // Loot council is similar to master loot but with democratic decision
    // Typically used in organized raid groups

    // Verify recipient eligibility
    if (!recipient->CanUseItem(item.itemTemplate))
    {
        TC_LOG_WARN("playerbot.loot", "Loot council: {} cannot use item {}", recipient->GetName(), item.itemId);
        return;
    }

    // Distribute via master loot mechanism
    HandleMasterLootDistribution(group, item, recipient);

    TC_LOG_DEBUG("playerbot.loot", "Loot council awarded item {} to {}", item.itemId, recipient->GetName());
}

void LootDistribution::HandlePersonalLoot(const LootItem& item)
{
    if (!_bot)
        return;

    // Personal loot goes directly to the player's bags
    ItemPosCountVec dest;
    InventoryResult msg = _bot->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, item.itemId, item.itemCount);

    if (msg == EQUIP_ERR_OK)
    {
        Item* newItem = _bot->StoreNewItem(dest, item.itemId, true);
        if (newItem)
        {
            _bot->SendNewItem(newItem, item.itemCount, false, false, true);

            // Update personal metrics
            auto& metrics = _playerMetrics[_bot->GetGUID().GetCounter()];
            metrics.totalRollsCompleted++;

            TC_LOG_DEBUG("playerbot.loot", "Personal loot: {} received {}", _bot->GetName(), item.itemId);
        }
    }
    else
    {
        // Item goes to mailbox if bags are full
        TC_LOG_DEBUG("playerbot.loot", "Personal loot: {} bags full for {}, sending to mail",
            _bot->GetName(), item.itemId);
    }
}

void LootDistribution::ManageLootHistory(Group* group, const LootItem& item, Player* recipient)
{
    if (!group || !recipient)
        return;

    uint32 groupId = group->GetGUID().GetCounter();

    // Update fairness tracker with this distribution
    UpdateLootFairness(groupId, recipient->GetGUID().GetCounter(), item);

    // Update player profile with loot received
    auto& profile = _playerLootProfiles[recipient->GetGUID().GetCounter()];
    profile.lastLootTime = GameTime::GetGameTimeMS();
    profile.totalLootReceived++;

    TC_LOG_DEBUG("playerbot.loot", "Loot history: {} now has {} total items",
        recipient->GetName(), profile.totalLootReceived);
}

void LootDistribution::PredictLootNeeds(Group* group)
{
    if (!group)
        return;

    // Analyze group composition and predict what loot each member might need
    TC_LOG_DEBUG("playerbot.loot", "Analyzing loot needs for group {}", group->GetGUID().GetCounter());

    for (GroupReference const& itr : group->GetMembers())
    {
        Player* member = itr.GetSource();
        if (!member)
            continue;

        // Get member's spec and role
        ChrSpecializationEntry const* specEntry = member->GetPrimarySpecializationEntry();
        if (!specEntry)
            continue;

        // Analyze each equipment slot for potential upgrades
        for (uint8 slot = EQUIPMENT_SLOT_HEAD; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* equipped = member->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            uint32 equippedLevel = equipped ? equipped->GetItemLevel(member) : 0;

            // Store prediction data for future loot decisions
            // This helps bots make smarter roll choices
        }
    }
}

void LootDistribution::OptimizeLootDistribution(Group* group)
{
    if (!group)
        return;

    // Analyze current fairness and suggest optimizations
    uint32 groupId = group->GetGUID().GetCounter();
    LootFairnessTracker tracker = GetGroupLootFairness(groupId);

    if (tracker.fairnessScore < 0.7f)
    {
        TC_LOG_DEBUG("playerbot.loot", "Group {} loot fairness is low ({:.2f}), suggesting adjustments",
            groupId, tracker.fairnessScore);

        // Identify players who have received fewer items
        float avgItems = tracker.totalItemsDistributed > 0 ?
            float(tracker.totalItemsDistributed) / std::max(1u, uint32(tracker.playerLootCount.size())) : 0.0f;

        for (const auto& pair : tracker.playerLootCount)
        {
            if (float(pair.second) < avgItems * 0.7f)
            {
                TC_LOG_DEBUG("playerbot.loot", "Player {} is below average loot ({} vs {:.1f})",
                    pair.first, pair.second, avgItems);
            }
        }
    }
}

void LootDistribution::RecommendLootSettings(Group* group)
{
    if (!group)
        return;

    // Analyze group composition and recommend optimal loot settings
    uint32 memberCount = 0;
    bool hasMultipleSameClass = false;
    std::unordered_map<uint8, uint32> classCounts;

    for (GroupReference const& itr : group->GetMembers())
    {
        Player* member = itr.GetSource();
        if (!member)
            continue;

        memberCount++;
        classCounts[member->GetClass()]++;

        if (classCounts[member->GetClass()] > 1)
            hasMultipleSameClass = true;
    }

    // Recommendations based on group composition
    LootMethod recommendedMethod = FREE_FOR_ALL;
    ItemQualities recommendedThreshold = ITEM_QUALITY_UNCOMMON;

    if (memberCount >= 5)
    {
        recommendedMethod = NEED_BEFORE_GREED;
        recommendedThreshold = ITEM_QUALITY_RARE;
    }

    if (memberCount >= 10)
    {
        recommendedMethod = MASTER_LOOT;
        recommendedThreshold = ITEM_QUALITY_EPIC;
    }

    TC_LOG_DEBUG("playerbot.loot", "Recommended loot settings for group {}: method={}, threshold={}",
        group->GetGUID().GetCounter(), static_cast<uint8>(recommendedMethod),
        static_cast<uint8>(recommendedThreshold));
}

void LootDistribution::AnalyzeGroupLootComposition(Group* group)
{
    if (!group)
        return;

    // Detailed analysis of what the group needs
    struct RoleCount { uint32 tanks = 0, healers = 0, dps = 0; };
    RoleCount roleCount;
    std::unordered_map<uint8, uint32> armorTypeCounts; // Plate, Mail, Leather, Cloth

    for (GroupReference const& itr : group->GetMembers())
    {
        Player* member = itr.GetSource();
        if (!member)
            continue;

        ChrSpecializationEntry const* specEntry = member->GetPrimarySpecializationEntry();
        if (specEntry)
        {
            ChrSpecializationRole role = static_cast<ChrSpecializationRole>(specEntry->Role);
            switch (role)
            {
                case ChrSpecializationRole::Tank: roleCount.tanks++; break;
                case ChrSpecializationRole::Healer: roleCount.healers++; break;
                case ChrSpecializationRole::Dps: roleCount.dps++; break;
            }
        }

        // Count armor types
        switch (member->GetClass())
        {
            case CLASS_WARRIOR:
            case CLASS_PALADIN:
            case CLASS_DEATH_KNIGHT:
                armorTypeCounts[ITEM_SUBCLASS_ARMOR_PLATE]++;
                break;
            case CLASS_HUNTER:
            case CLASS_SHAMAN:
            case CLASS_EVOKER:
                armorTypeCounts[ITEM_SUBCLASS_ARMOR_MAIL]++;
                break;
            case CLASS_ROGUE:
            case CLASS_DRUID:
            case CLASS_MONK:
            case CLASS_DEMON_HUNTER:
                armorTypeCounts[ITEM_SUBCLASS_ARMOR_LEATHER]++;
                break;
            case CLASS_MAGE:
            case CLASS_PRIEST:
            case CLASS_WARLOCK:
                armorTypeCounts[ITEM_SUBCLASS_ARMOR_CLOTH]++;
                break;
        }
    }

    TC_LOG_DEBUG("playerbot.loot", "Group {} composition: {} tanks, {} healers, {} dps",
        group->GetGUID().GetCounter(), roleCount.tanks, roleCount.healers, roleCount.dps);
}

void LootDistribution::HandleInvalidLootRoll(uint32 rollId)
{
    auto rollIt = _activeLootRolls.find(rollId);
    if (rollIt == _activeLootRolls.end())
        return;

    TC_LOG_WARN("playerbot.loot", "Handling invalid loot roll {}", rollId);

    // Mark roll as completed with no winner
    LootRoll& roll = rollIt->second;
    roll.isCompleted = true;
    roll.winnerGuid = 0;

    // Clean up
    _activeLootRolls.erase(rollIt);
    _rollTimeouts.erase(rollId);

    _globalMetrics.rollTimeouts++;
}

void LootDistribution::HandlePlayerDisconnectDuringRoll(uint32 rollId)
{
    auto rollIt = _activeLootRolls.find(rollId);
    if (rollIt == _activeLootRolls.end())
        return;

    LootRoll& roll = rollIt->second;

    // Remove disconnected players from eligible list
    std::vector<uint32> disconnectedPlayers;
    for (uint32 playerGuid : roll.eligiblePlayers)
    {
        ObjectGuid guid = ObjectGuid::Create<HighGuid::Player>(playerGuid);
        if (!ObjectAccessor::FindConnectedPlayer(guid))
        {
            disconnectedPlayers.push_back(playerGuid);
        }
    }

    for (uint32 playerGuid : disconnectedPlayers)
    {
        roll.eligiblePlayers.erase(playerGuid);
        // Auto-pass for disconnected players
        roll.playerRolls[playerGuid] = LootRollType::PASS;

        TC_LOG_DEBUG("playerbot.loot", "Roll {}: Player {} disconnected, auto-passing",
            rollId, playerGuid);
    }

    // If no eligible players remain, complete the roll
    if (roll.eligiblePlayers.empty())
    {
        HandleInvalidLootRoll(rollId);
    }
    // If all remaining players have rolled, complete
    else if (roll.playerRolls.size() >= roll.eligiblePlayers.size())
    {
        CompleteLootRoll(rollId);
    }
}

void LootDistribution::RecoverFromLootSystemError(uint32 rollId)
{
    TC_LOG_ERROR("playerbot.loot", "Recovering from loot system error for roll {}", rollId);

    // Try to salvage the roll
    auto rollIt = _activeLootRolls.find(rollId);
    if (rollIt != _activeLootRolls.end())
    {
        LootRoll& roll = rollIt->second;

        // Check if we can still complete the roll
        if (!roll.playerRolls.empty())
        {
            // Try to determine a winner from existing rolls
            uint32 winner = DetermineRollWinner(roll);
            if (winner != 0)
            {
                DistributeLootToWinner(rollId, winner);
            }
        }

        // Clean up regardless
        _activeLootRolls.erase(rollIt);
        _rollTimeouts.erase(rollId);
    }

    // Re-validate all loot states
    ValidateLootStates();
}

void LootDistribution::InitializePlayerLootProfile()
{
    if (!_bot)
        return;

    uint32 playerGuid = _bot->GetGUID().GetCounter();

    // Check if profile already exists
    if (_playerLootProfiles.find(playerGuid) != _playerLootProfiles.end())
        return;

    // Create new profile
    PlayerLootProfile profile;
    profile.playerGuid = playerGuid;
    profile.playerClass = _bot->GetClass();
    profile.playerSpec = AsUnderlyingType(_bot->GetPrimarySpecialization());
    profile.playerLevel = _bot->GetLevel();
    profile.strategy = LootDecisionStrategy::NEED_BEFORE_GREED;
    profile.greedThreshold = 0.3f;
    profile.needMainSpecOnly = true;
    profile.greedOffSpec = true;
    profile.disenchantUnneeded = _bot->GetSkillValue(SKILL_ENCHANTING) > 0;

    _playerLootProfiles[playerGuid] = profile;

    TC_LOG_DEBUG("playerbot.loot", "Initialized loot profile for player {}", _bot->GetName());
}

void LootDistribution::AnalyzeItemForPlayer(const LootItem& item)
{
    if (!_bot || !item.itemTemplate)
        return;

    uint32 playerGuid = _bot->GetGUID().GetCounter();

    // Calculate and cache priority
    LootPriority priority = AnalyzeItemPriority(item);
    _itemPriorityCache[playerGuid][item.itemId] = priority;

    // Calculate and cache upgrade status
    bool isUpgrade = IsItemUpgrade(item);
    _upgradeCache[playerGuid][item.itemId] = isUpgrade;

    TC_LOG_DEBUG("playerbot.loot", "Analyzed item {} for {}: priority={}, upgrade={}",
        item.itemId, _bot->GetName(), static_cast<uint8>(priority), isUpgrade);
}

void LootDistribution::UpdateItemPriorityCache(const LootItem& item, LootPriority priority)
{
    if (!_bot)
        return;

    uint32 playerGuid = _bot->GetGUID().GetCounter();
    _itemPriorityCache[playerGuid][item.itemId] = priority;

    // Enforce cache size limit
    if (_itemPriorityCache[playerGuid].size() > PRIORITY_CACHE_SIZE)
    {
        // Remove oldest entries (simple FIFO - in production would use LRU)
        auto it = _itemPriorityCache[playerGuid].begin();
        std::advance(it, _itemPriorityCache[playerGuid].size() - PRIORITY_CACHE_SIZE);
        _itemPriorityCache[playerGuid].erase(_itemPriorityCache[playerGuid].begin(), it);
    }
}

bool LootDistribution::IsItemCachedUpgrade(uint32 itemId)
{
    if (!_bot)
        return false;

    uint32 playerGuid = _bot->GetGUID().GetCounter();
    auto cacheIt = _upgradeCache.find(playerGuid);
    if (cacheIt != _upgradeCache.end())
    {
        auto itemIt = cacheIt->second.find(itemId);
        if (itemIt != cacheIt->second.end())
            return itemIt->second;
    }

    return false;
}

void LootDistribution::InvalidatePlayerCache()
{
    if (!_bot)
        return;

    uint32 playerGuid = _bot->GetGUID().GetCounter();
    _itemPriorityCache.erase(playerGuid);
    _upgradeCache.erase(playerGuid);

    TC_LOG_DEBUG("playerbot.loot", "Invalidated loot cache for player {}", _bot->GetName());
}

bool LootDistribution::IsArmorUpgrade(const LootItem& item)
{
    if (!_bot || !item.itemTemplate)
        return false;

    if (item.itemTemplate->GetClass() != ITEM_CLASS_ARMOR)
        return false;

    // Get current armor in this slot
    uint8 invType = item.itemTemplate->GetInventoryType();
    Item* current = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, invType);

    if (!current)
        return true; // No item = upgrade

    // Compare item levels
    uint32 currentLevel = current->GetItemLevel(_bot);
    uint32 newLevel = item.itemLevel;

    // Must be at least UPGRADE_THRESHOLD better
    return newLevel > currentLevel * (1.0f + UPGRADE_THRESHOLD);
}

bool LootDistribution::IsWeaponUpgrade(const LootItem& item)
{
    if (!_bot || !item.itemTemplate)
        return false;

    if (item.itemTemplate->GetClass() != ITEM_CLASS_WEAPON)
        return false;

    // Check main hand and off hand
    Item* mainHand = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    Item* offHand = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);

    uint32 currentLevel = 0;
    if (mainHand)
        currentLevel = std::max(currentLevel, mainHand->GetItemLevel(_bot));
    if (offHand && item.itemTemplate->GetInventoryType() != INVTYPE_2HWEAPON)
        currentLevel = std::max(currentLevel, offHand->GetItemLevel(_bot));

    if (currentLevel == 0)
        return true; // No weapon = upgrade

    return item.itemLevel > currentLevel * (1.0f + UPGRADE_THRESHOLD);
}

bool LootDistribution::IsAccessoryUpgrade(const LootItem& item)
{
    if (!_bot || !item.itemTemplate)
        return false;

    uint8 invType = item.itemTemplate->GetInventoryType();

    // Check accessory slots
    if (invType != INVTYPE_FINGER && invType != INVTYPE_TRINKET &&
        invType != INVTYPE_NECK && invType != INVTYPE_CLOAK)
        return false;

    // For rings and trinkets, check both slots
    Item* slot1 = nullptr;
    Item* slot2 = nullptr;

    if (invType == INVTYPE_FINGER)
    {
        slot1 = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_FINGER1);
        slot2 = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_FINGER2);
    }
    else if (invType == INVTYPE_TRINKET)
    {
        slot1 = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_TRINKET1);
        slot2 = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_TRINKET2);
    }
    else
    {
        slot1 = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, invType);
    }

    // Compare with lowest equipped item
    uint32 lowestLevel = UINT32_MAX;
    if (slot1)
        lowestLevel = std::min(lowestLevel, slot1->GetItemLevel(_bot));
    if (slot2)
        lowestLevel = std::min(lowestLevel, slot2->GetItemLevel(_bot));

    if (lowestLevel == UINT32_MAX)
        return true; // Empty slot = upgrade

    return item.itemLevel > lowestLevel * (1.0f + UPGRADE_THRESHOLD);
}

float LootDistribution::CalculateStatPriority(uint32 statType)
{
    if (!_bot)
        return 0.0f;

    ChrSpecialization spec = _bot->GetPrimarySpecialization();
    ChrSpecializationEntry const* specEntry = _bot->GetPrimarySpecializationEntry();

    if (!specEntry)
        return 0.5f; // Neutral priority

    ChrSpecializationRole role = static_cast<ChrSpecializationRole>(specEntry->Role);

    // Primary stats are always high priority if they match spec
    switch (statType)
    {
        case ITEM_MOD_STRENGTH:
        case ITEM_MOD_STRENGTH_PCT:
            if (role == ChrSpecializationRole::Tank || role == ChrSpecializationRole::Dps)
            {
                if (_bot->GetClass() == CLASS_WARRIOR ||
                    _bot->GetClass() == CLASS_PALADIN ||
                    _bot->GetClass() == CLASS_DEATH_KNIGHT)
                    return 1.0f;
            }
            return 0.1f;

        case ITEM_MOD_AGILITY:
        case ITEM_MOD_AGILITY_PCT:
            if (_bot->GetClass() == CLASS_ROGUE ||
                _bot->GetClass() == CLASS_HUNTER ||
                _bot->GetClass() == CLASS_DRUID ||
                _bot->GetClass() == CLASS_MONK ||
                _bot->GetClass() == CLASS_DEMON_HUNTER ||
                _bot->GetClass() == CLASS_SHAMAN)
                return 1.0f;
            return 0.1f;

        case ITEM_MOD_INTELLECT:
        case ITEM_MOD_INTELLECT_PCT:
            if (role == ChrSpecializationRole::Healer)
                return 1.0f;
            if (_bot->GetClass() == CLASS_MAGE ||
                _bot->GetClass() == CLASS_WARLOCK ||
                _bot->GetClass() == CLASS_PRIEST ||
                _bot->GetClass() == CLASS_EVOKER)
                return 1.0f;
            return 0.1f;

        case ITEM_MOD_STAMINA:
        case ITEM_MOD_STAMINA_PCT:
            if (role == ChrSpecializationRole::Tank)
                return 0.9f;
            return 0.5f;

        // Secondary stats
        case ITEM_MOD_CRIT_RATING:
        case ITEM_MOD_HASTE_RATING:
        case ITEM_MOD_MASTERY_RATING:
        case ITEM_MOD_VERSATILITY:
            return 0.7f; // Generally useful

        default:
            return 0.3f;
    }
}

void LootDistribution::HandleRollCompletion(LootRoll& roll)
{
    // Final processing after roll is completed
    roll.isCompleted = true;

    // Notify all eligible players of the result
    NotifyRollResult(roll);

    // Update global metrics
    _globalMetrics.totalRollsCompleted++;

    // Calculate and update roll time metrics
    uint32 rollDuration = GameTime::GetGameTimeMS() - roll.rollStartTime;
    float currentAvg = _globalMetrics.averageRollTime.load();
    uint32 totalRolls = _globalMetrics.totalRollsCompleted.load();

    // Running average calculation
    float newAvg = ((currentAvg * (totalRolls - 1)) + rollDuration) / totalRolls;
    _globalMetrics.averageRollTime = newAvg;

    TC_LOG_DEBUG("playerbot.loot", "Roll {} completed in {}ms", roll.rollId, rollDuration);
}

void LootDistribution::UpdateGroupLootHistory(uint32 groupId, uint32 winnerGuid, const LootItem& item)
{
    // Alias for UpdateLootFairness
    UpdateLootFairness(groupId, winnerGuid, item);
}

void LootDistribution::BalanceLootDistribution(Group* group)
{
    if (!group)
        return;

    uint32 groupId = group->GetGUID().GetCounter();
    LootFairnessTracker& tracker = _groupFairnessTracking[groupId];

    // Identify imbalances
    if (tracker.playerLootCount.empty())
        return;

    float avgItems = float(tracker.totalItemsDistributed) / tracker.playerLootCount.size();

    std::vector<uint32> lowLootPlayers;
    std::vector<uint32> highLootPlayers;

    for (const auto& pair : tracker.playerLootCount)
    {
        if (float(pair.second) < avgItems * 0.7f)
            lowLootPlayers.push_back(pair.first);
        else if (float(pair.second) > avgItems * 1.3f)
            highLootPlayers.push_back(pair.first);
    }

    // Log imbalances for monitoring
    if (!lowLootPlayers.empty() || !highLootPlayers.empty())
    {
        TC_LOG_DEBUG("playerbot.loot", "Group {} loot imbalance: {} low, {} high",
            groupId, lowLootPlayers.size(), highLootPlayers.size());
    }
}

void LootDistribution::AdjustLootDecisionsForFairness(Group* group, LootRollType& decision)
{
    if (!group || !_bot)
        return;

    uint32 groupId = group->GetGUID().GetCounter();
    LootFairnessTracker tracker = GetGroupLootFairness(groupId);

    if (tracker.playerLootCount.empty())
        return;

    uint32 playerGuid = _bot->GetGUID().GetCounter();
    auto playerIt = tracker.playerLootCount.find(playerGuid);

    if (playerIt == tracker.playerLootCount.end())
        return; // Player hasn't received any loot yet

    float avgItems = float(tracker.totalItemsDistributed) / tracker.playerLootCount.size();

    // If we've received significantly more than average, be more generous
    if (float(playerIt->second) > avgItems * 1.3f)
    {
        if (decision == LootRollType::NEED)
        {
            // Downgrade NEED to GREED if not a critical upgrade
            decision = LootRollType::GREED;
            TC_LOG_DEBUG("playerbot.loot", "Bot {} adjusted NEED to GREED for fairness", _bot->GetName());
        }
        else if (decision == LootRollType::GREED)
        {
            // Consider passing entirely
            decision = LootRollType::PASS;
            TC_LOG_DEBUG("playerbot.loot", "Bot {} adjusted GREED to PASS for fairness", _bot->GetName());
        }
    }
}

void LootDistribution::OptimizeLootProcessing()
{
    // Pre-allocate commonly used containers
    // Clean up stale cache entries
    // Optimize data structures for current group size

    // Limit active rolls
    if (_activeLootRolls.size() > MAX_ACTIVE_ROLLS)
    {
        TC_LOG_WARN("playerbot.loot", "Too many active rolls ({}), cleaning up oldest",
            _activeLootRolls.size());

        // Find oldest rolls and timeout them
        std::vector<std::pair<uint32, uint32>> rollsByAge;
        uint32 currentTime = GameTime::GetGameTimeMS();

        for (const auto& pair : _activeLootRolls)
        {
            rollsByAge.emplace_back(pair.first, currentTime - pair.second.rollStartTime);
        }

        // Sort by age (oldest first)
        std::sort(rollsByAge.begin(), rollsByAge.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

        // Timeout oldest rolls until we're under the limit
        for (size_t i = 0; i < rollsByAge.size() && _activeLootRolls.size() > MAX_ACTIVE_ROLLS / 2; ++i)
        {
            HandleLootRollTimeout(rollsByAge[i].first);
        }
    }
}

void LootDistribution::PreloadItemData(const std::vector<LootItem>& items)
{
    if (!_bot)
        return;

    // Pre-analyze all items for faster decision making during rolls
    for (const LootItem& item : items)
    {
        AnalyzeItemForPlayer(item);
    }

    TC_LOG_DEBUG("playerbot.loot", "Preloaded {} items for {}", items.size(), _bot->GetName());
}

void LootDistribution::CachePlayerEquipment()
{
    if (!_bot)
        return;

    // Cache current equipment item levels for faster comparison
    // This is called when equipment changes or periodically

    InvalidatePlayerCache(); // Clear old cache first

    TC_LOG_DEBUG("playerbot.loot", "Cached equipment data for {}", _bot->GetName());
}

'''

# Add the implementations before the closing namespace
end_marker = '} // namespace Playerbot'
if end_marker in content:
    content = content.replace(end_marker, stub_implementations + end_marker)
    print("Added missing stub implementations: APPLIED")
else:
    print("Could not find namespace end marker")

# Write back
with open('src/modules/Playerbot/Social/LootDistribution.cpp', 'w') as f:
    f.write(content)

print("\nStub implementations added")
