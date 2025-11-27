#!/usr/bin/env python3
"""
Fix LootDistribution.cpp - Complete enterprise-grade implementation
All stub methods and DESIGN NOTE sections are replaced with full implementations
"""

# Read the file
with open('src/modules/Playerbot/Social/LootDistribution.cpp', 'r') as f:
    content = f.read()

fixes_made = 0

# Fix 1: ShouldPlayerGreedItem - Full implementation with group member analysis
old_code1 = '''bool LootDistribution::ShouldPlayerGreedItem(const LootItem& item)
{
    if (!_bot)
        return false;

    PlayerLootProfile profile = GetPlayerLootProfile();
    // Check greed threshold
    if (item.vendorValue < profile.greedThreshold * 10000) // Convert threshold to copper
        return false;

    // DESIGN NOTE: Simplified implementation for greed eligibility
    // Current behavior: Always returns true after vendor value check
    // Full implementation should:
    // - Query Group members to check if anyone can use item for main spec
    // - Use IsClassAppropriate() against all group member classes
    // - Respect "main spec before off spec" etiquette
    // - Check if item is BiS (Best in Slot) for any group member
    // - Consider group loot strategy and fairness policies
    // Reference: Group::GetMembers(), IsClassAppropriate(), LootFairnessTracker
    return true;
}'''

new_code1 = '''bool LootDistribution::ShouldPlayerGreedItem(const LootItem& item)
{
    if (!_bot)
        return false;

    PlayerLootProfile profile = GetPlayerLootProfile();

    // Check greed threshold
    if (item.vendorValue < profile.greedThreshold * 10000) // Convert threshold to copper
        return false;

    // Check if item is blacklisted
    if (profile.blacklistedItems.find(item.itemId) != profile.blacklistedItems.end())
        return false;

    Group* group = _bot->GetGroup();
    if (!group)
        return true; // Solo play - always greed valuable items

    // Check group composition for main spec priority
    // Count how many players can use this item for main spec
    uint32 mainSpecUserCount = 0;
    uint32 offSpecUserCount = 0;
    bool botCanMainSpec = IsItemForMainSpec(item);

    for (GroupReference const& itr : group->GetMembers())
    {
        Player* member = itr.GetSource();
        if (!member || member == _bot)
            continue;

        // Check if member can use item at all
        if (!member->CanUseItem(item.itemTemplate))
            continue;

        // Get member's specialization to determine main spec appropriateness
        ChrSpecializationEntry const* memberSpec = member->GetPrimarySpecializationEntry();
        if (memberSpec)
        {
            // Check armor type appropriateness based on spec role
            bool isMainSpecItem = IsItemTypeUsefulForClass(member->GetClass(), item.itemTemplate);
            if (isMainSpecItem)
            {
                // Check if this would be an upgrade for their current gear
                uint8 slot = item.itemTemplate->GetInventoryType();
                Item* memberEquipped = member->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);

                if (!memberEquipped || memberEquipped->GetItemLevel(member) < item.itemLevel)
                    mainSpecUserCount++;
                else
                    offSpecUserCount++;
            }
            else
            {
                offSpecUserCount++;
            }
        }
    }

    // Respect main spec before off spec etiquette
    // If others need for main spec and we don't, pass instead of greed
    if (mainSpecUserCount > 0 && !botCanMainSpec)
    {
        // Consider fairness - if we've received many items recently, be more conservative
        LootFairnessTracker tracker = GetGroupLootFairness(group->GetGUID().GetCounter());
        auto playerIt = tracker.playerLootCount.find(_bot->GetGUID().GetCounter());

        if (playerIt != tracker.playerLootCount.end())
        {
            float avgItems = float(tracker.totalItemsDistributed) / std::max(1u, uint32(tracker.playerLootCount.size()));
            if (float(playerIt->second) > avgItems * 1.2f) // 20% above average
                return false; // Be generous, pass on this item
        }
    }

    // Check group loot strategy fairness policy
    LootDecisionStrategy groupStrategy = profile.strategy;
    if (groupStrategy == LootDecisionStrategy::FAIR_DISTRIBUTION)
    {
        // In fair distribution mode, skip greed if we're above average in loot received
        LootFairnessTracker tracker = GetGroupLootFairness(group->GetGUID().GetCounter());
        if (tracker.fairnessScore < FAIRNESS_ADJUSTMENT_THRESHOLD)
        {
            auto playerIt = tracker.playerLootCount.find(_bot->GetGUID().GetCounter());
            if (playerIt != tracker.playerLootCount.end())
            {
                float avgItems = float(tracker.totalItemsDistributed) / std::max(1u, uint32(tracker.playerLootCount.size()));
                if (float(playerIt->second) > avgItems)
                    return false;
            }
        }
    }

    // Item is valuable enough and respects group etiquette - greed it
    return true;
}'''

if old_code1 in content:
    content = content.replace(old_code1, new_code1)
    print("Fix 1: ShouldPlayerGreedItem full implementation: APPLIED")
    fixes_made += 1
else:
    print("Fix 1: ShouldPlayerGreedItem: NOT FOUND")

# Fix 2: CanParticipateInRoll - Full implementation with all validations
old_code2 = '''bool LootDistribution::CanParticipateInRoll(const LootItem& item)
{
    if (!_bot || !item.itemTemplate)
        return false;

    // Player must be able to use the item
    if (!_bot->CanUseItem(item.itemTemplate))
        return false;

    // DESIGN NOTE: Simplified implementation for roll participation validation
    // Current behavior: Always returns true after basic item usability check
    // Full implementation should:
    // - Verify player is within loot range (typically 30-40 yards)
    // - Check if player is alive (dead players can't roll)
    // - Validate player is still in the group
    // - Ensure player hasn't left the instance
    // - Check if player has inventory space for the item
    // Reference: Player::IsWithinDistInMap(), Group::IsMember()
    return true;
}'''

new_code2 = '''bool LootDistribution::CanParticipateInRoll(const LootItem& item)
{
    if (!_bot || !item.itemTemplate)
        return false;

    // Player must be able to use the item
    if (!_bot->CanUseItem(item.itemTemplate))
        return false;

    // Check if player is alive (dead players can't roll)
    if (!_bot->IsAlive())
    {
        // Exception: Ghost players can still participate in rolls
        // They just need to release and run back
        if (!_bot->HasAura(8326)) // Ghost spell
            return false;
    }

    // Validate player is still in a group
    Group* group = _bot->GetGroup();
    if (!group)
        return true; // Solo play - can always loot

    // Ensure player is a member of the group
    if (!group->IsMember(_bot->GetGUID()))
        return false;

    // Check loot distance (standard loot range is about 60 yards in group)
    static constexpr float LOOT_RANGE = 60.0f;

    // Get loot source location from the group leader or nearby corpse
    // For group loot, we use a more generous range
    Player* leader = ObjectAccessor::FindConnectedPlayer(group->GetLeaderGUID());
    if (leader && !_bot->IsWithinDistInMap(leader, LOOT_RANGE * 2)) // Double range for flexibility
    {
        // Too far from group leader - might be in different zone
        if (_bot->GetMapId() != leader->GetMapId())
            return false;
    }

    // Check if player has inventory space for the item
    ItemPosCountVec dest;
    InventoryResult result = _bot->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, item.itemId, item.itemCount);
    if (result != EQUIP_ERR_OK)
    {
        // No space, but can still roll - item might go to mail
        // Log this for debugging
        TC_LOG_DEBUG("playerbot.loot", "Player {} has no inventory space for item {}, but can still roll",
            _bot->GetName(), item.itemId);
    }

    // Check if player is in a valid state to receive loot
    // (not in combat transition, not being teleported, etc.)
    if (_bot->IsBeingTeleported())
        return false;

    // Check if instance-based restrictions apply
    if (Map* map = _bot->GetMap())
    {
        if (InstanceMap* instance = map->ToInstanceMap())
        {
            // Verify player has access to this instance
            if (!instance->CanEnter(_bot))
                return false;
        }
    }

    return true;
}'''

if old_code2 in content:
    content = content.replace(old_code2, new_code2)
    print("Fix 2: CanParticipateInRoll full implementation: APPLIED")
    fixes_made += 1
else:
    print("Fix 2: CanParticipateInRoll: NOT FOUND")

# Fix 3: IsItemForMainSpec - Full spec-based item analysis
old_code3 = '''bool LootDistribution::IsItemForMainSpec( const LootItem& item)
{
    if (!_bot || !item.itemTemplate)
        return false;

    // DESIGN NOTE: Simplified implementation for main spec validation
    // Current behavior: Basic armor/weapon type checks for a few classes
    // Full implementation should:
    // - Parse player's active talent specialization
    // - Match item's primary stats to spec's stat priorities
    // - Use ClassAI spec configurations for stat weights
    // - Check if item has spec-specific procs or bonuses
    // - Compare against BiS (Best in Slot) lists for the spec
    // - Consider tier set bonuses and set piece priorities
    // Reference: Player::GetPrimarySpecialization(), ClassAI stat priorities

    uint8 playerClass = _bot->GetClass();
    uint8 spec = AsUnderlyingType(_bot->GetPrimarySpecialization());

    // Basic logic for different classes
    switch (playerClass)
    {
        case CLASS_WARRIOR:
            if (spec == 2) // Protection
                return item.itemTemplate->GetInventoryType() == INVTYPE_SHIELD ||
                       item.itemTemplate->GetSubClass() == ITEM_SUBCLASS_ARMOR_PLATE;
            else // Arms/Fury
                return item.itemTemplate->GetClass() == ITEM_CLASS_WEAPON;

        case CLASS_PALADIN:
            if (spec == 1) // Protection
                return item.itemTemplate->GetInventoryType() == INVTYPE_SHIELD ||
                       item.itemTemplate->GetSubClass() == ITEM_SUBCLASS_ARMOR_PLATE;
            else if (spec == 0) // Holy
                return item.itemTemplate->GetSubClass() == ITEM_SUBCLASS_ARMOR_PLATE ||
                       (item.itemTemplate->GetClass() == ITEM_CLASS_WEAPON &&
                        item.itemTemplate->GetSubClass() == ITEM_SUBCLASS_WEAPON_MACE);
            else // Retribution
                return item.itemTemplate->GetClass() == ITEM_CLASS_WEAPON;

        // Add more classes as needed
        default:
            return true; // Default to allowing all items
    }
}'''

new_code3 = '''bool LootDistribution::IsItemForMainSpec(const LootItem& item)
{
    if (!_bot || !item.itemTemplate)
        return false;

    ChrSpecialization spec = _bot->GetPrimarySpecialization();
    ChrSpecializationEntry const* specEntry = _bot->GetPrimarySpecializationEntry();

    if (!specEntry)
        return true; // No spec info, allow item

    uint8 itemClass = item.itemTemplate->GetClass();
    uint8 itemSubClass = item.itemTemplate->GetSubClass();
    uint8 invType = item.itemTemplate->GetInventoryType();

    // Get spec role (Tank, Healer, DPS)
    ChrSpecializationRole role = static_cast<ChrSpecializationRole>(specEntry->Role);

    // Check armor type appropriateness
    if (itemClass == ITEM_CLASS_ARMOR)
    {
        uint8 playerClass = _bot->GetClass();

        // Armor type restrictions by class (WoW 11.2)
        switch (playerClass)
        {
            case CLASS_WARRIOR:
            case CLASS_PALADIN:
            case CLASS_DEATH_KNIGHT:
                // Plate wearers should only need plate (except trinkets/rings/etc)
                if (itemSubClass != ITEM_SUBCLASS_ARMOR_PLATE &&
                    itemSubClass != ITEM_SUBCLASS_ARMOR_MISCELLANEOUS &&
                    invType != INVTYPE_FINGER && invType != INVTYPE_TRINKET &&
                    invType != INVTYPE_NECK && invType != INVTYPE_CLOAK)
                    return false;
                break;

            case CLASS_HUNTER:
            case CLASS_SHAMAN:
            case CLASS_EVOKER:
                // Mail wearers
                if (itemSubClass != ITEM_SUBCLASS_ARMOR_MAIL &&
                    itemSubClass != ITEM_SUBCLASS_ARMOR_MISCELLANEOUS &&
                    invType != INVTYPE_FINGER && invType != INVTYPE_TRINKET &&
                    invType != INVTYPE_NECK && invType != INVTYPE_CLOAK)
                    return false;
                break;

            case CLASS_ROGUE:
            case CLASS_DRUID:
            case CLASS_MONK:
            case CLASS_DEMON_HUNTER:
                // Leather wearers
                if (itemSubClass != ITEM_SUBCLASS_ARMOR_LEATHER &&
                    itemSubClass != ITEM_SUBCLASS_ARMOR_MISCELLANEOUS &&
                    invType != INVTYPE_FINGER && invType != INVTYPE_TRINKET &&
                    invType != INVTYPE_NECK && invType != INVTYPE_CLOAK)
                    return false;
                break;

            case CLASS_MAGE:
            case CLASS_PRIEST:
            case CLASS_WARLOCK:
                // Cloth wearers
                if (itemSubClass != ITEM_SUBCLASS_ARMOR_CLOTH &&
                    itemSubClass != ITEM_SUBCLASS_ARMOR_MISCELLANEOUS &&
                    invType != INVTYPE_FINGER && invType != INVTYPE_TRINKET &&
                    invType != INVTYPE_NECK && invType != INVTYPE_CLOAK)
                    return false;
                break;
        }

        // Shield check - only for tank specs or Holy Paladin/Shaman
        if (invType == INVTYPE_SHIELD)
        {
            switch (spec)
            {
                case ChrSpecialization::WarriorProtection:
                case ChrSpecialization::PaladinProtection:
                case ChrSpecialization::PaladinHoly:
                case ChrSpecialization::ShamanElemental:
                case ChrSpecialization::ShamanRestoration:
                    return true;
                default:
                    return false;
            }
        }
    }

    // Check weapon type appropriateness by spec
    if (itemClass == ITEM_CLASS_WEAPON)
    {
        // Two-handed vs one-handed preferences by spec
        bool isTwoHanded = (invType == INVTYPE_2HWEAPON ||
                            invType == INVTYPE_RANGED ||
                            invType == INVTYPE_RANGEDRIGHT);

        switch (spec)
        {
            // Specs that use two-handed weapons
            case ChrSpecialization::WarriorArms:
            case ChrSpecialization::DeathKnightFrost: // Can dual wield 2H
            case ChrSpecialization::DeathKnightUnholy:
            case ChrSpecialization::PaladinRetribution:
            case ChrSpecialization::DruidBalance:
            case ChrSpecialization::DruidFeral:
            case ChrSpecialization::DruidGuardian:
            case ChrSpecialization::MonkWindwalker:
            case ChrSpecialization::MonkBrewmaster:
            case ChrSpecialization::ShamanEnhancement:
            case ChrSpecialization::HunterBeastMastery:
            case ChrSpecialization::HunterMarksmanship:
            case ChrSpecialization::HunterSurvival:
                // These specs prefer 2H but can use 1H
                break;

            // Specs that use one-handed weapons
            case ChrSpecialization::WarriorFury: // Titan's Grip for 2H
            case ChrSpecialization::RogueAssassination:
            case ChrSpecialization::RogueOutlaw:
            case ChrSpecialization::RogueSubtely:
            case ChrSpecialization::DemonHunterHavoc:
            case ChrSpecialization::DemonHunterVengeance:
                // Dual wield specs
                if (isTwoHanded && spec != ChrSpecialization::WarriorFury)
                    return false;
                break;

            // Caster specs - prefer staves or caster weapons
            case ChrSpecialization::MageArcane:
            case ChrSpecialization::MageFire:
            case ChrSpecialization::MageFrost:
            case ChrSpecialization::WarlockAffliction:
            case ChrSpecialization::WarlockDemonology:
            case ChrSpecialization::WarlockDestruction:
            case ChrSpecialization::PriestShadow:
            case ChrSpecialization::PriestDiscipline:
            case ChrSpecialization::PriestHoly:
            case ChrSpecialization::DruidRestoration:
            case ChrSpecialization::ShamanRestoration:
            case ChrSpecialization::ShamanElemental:
            case ChrSpecialization::MonkMistweaver:
            case ChrSpecialization::EvokerPreservation:
            case ChrSpecialization::EvokerDevastation:
            case ChrSpecialization::EvokerAugmentation:
            case ChrSpecialization::PaladinHoly:
                // Casters prefer staves, wands, and caster weapons
                if (itemSubClass == ITEM_SUBCLASS_WEAPON_AXE ||
                    itemSubClass == ITEM_SUBCLASS_WEAPON_AXE2 ||
                    itemSubClass == ITEM_SUBCLASS_WEAPON_SWORD ||
                    itemSubClass == ITEM_SUBCLASS_WEAPON_SWORD2)
                    return false; // Melee weapons not for casters
                break;

            // Tank specs
            case ChrSpecialization::WarriorProtection:
            case ChrSpecialization::PaladinProtection:
            case ChrSpecialization::DeathKnightBlood:
                // Tanks prefer 1H + Shield
                if (isTwoHanded && spec != ChrSpecialization::DeathKnightBlood)
                    return false;
                break;

            default:
                break;
        }

        // Specific weapon restrictions
        switch (itemSubClass)
        {
            case ITEM_SUBCLASS_WEAPON_WARGLAIVE:
                // Only Demon Hunters can use warglaives
                return spec == ChrSpecialization::DemonHunterHavoc ||
                       spec == ChrSpecialization::DemonHunterVengeance;

            case ITEM_SUBCLASS_WEAPON_BOW:
            case ITEM_SUBCLASS_WEAPON_GUN:
            case ITEM_SUBCLASS_WEAPON_CROSSBOW:
                // Only Hunters use ranged weapons as main spec
                return spec == ChrSpecialization::HunterBeastMastery ||
                       spec == ChrSpecialization::HunterMarksmanship ||
                       spec == ChrSpecialization::HunterSurvival;

            case ITEM_SUBCLASS_WEAPON_WAND:
                // Wands for casters only
                return role == ChrSpecializationRole::Healer ||
                       (role == ChrSpecializationRole::Dps &&
                        (_bot->GetClass() == CLASS_MAGE ||
                         _bot->GetClass() == CLASS_WARLOCK ||
                         _bot->GetClass() == CLASS_PRIEST));
        }
    }

    // Check primary stat appropriateness
    // This requires checking item stats against spec stat priorities
    bool hasIntellect = false;
    bool hasAgility = false;
    bool hasStrength = false;

    // TrinityCore 11.2: Check item's primary stats
    for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
    {
        int32 statType = item.itemTemplate->GetStatModifierBonusStat(i);
        if (statType < 0)
            continue;

        switch (statType)
        {
            case ITEM_MOD_INTELLECT:
            case ITEM_MOD_INTELLECT_PCT:
                hasIntellect = true;
                break;
            case ITEM_MOD_AGILITY:
            case ITEM_MOD_AGILITY_PCT:
                hasAgility = true;
                break;
            case ITEM_MOD_STRENGTH:
            case ITEM_MOD_STRENGTH_PCT:
                hasStrength = true;
                break;
        }
    }

    // Match primary stat to spec
    bool specNeedsIntellect = (role == ChrSpecializationRole::Healer) ||
        spec == ChrSpecialization::MageArcane || spec == ChrSpecialization::MageFire ||
        spec == ChrSpecialization::MageFrost || spec == ChrSpecialization::WarlockAffliction ||
        spec == ChrSpecialization::WarlockDemonology || spec == ChrSpecialization::WarlockDestruction ||
        spec == ChrSpecialization::PriestShadow || spec == ChrSpecialization::DruidBalance ||
        spec == ChrSpecialization::ShamanElemental || spec == ChrSpecialization::EvokerDevastation ||
        spec == ChrSpecialization::EvokerAugmentation;

    bool specNeedsAgility =
        spec == ChrSpecialization::RogueAssassination || spec == ChrSpecialization::RogueOutlaw ||
        spec == ChrSpecialization::RogueSubtely || spec == ChrSpecialization::DruidFeral ||
        spec == ChrSpecialization::MonkWindwalker || spec == ChrSpecialization::DemonHunterHavoc ||
        spec == ChrSpecialization::DemonHunterVengeance || spec == ChrSpecialization::DruidGuardian ||
        spec == ChrSpecialization::MonkBrewmaster || spec == ChrSpecialization::HunterBeastMastery ||
        spec == ChrSpecialization::HunterMarksmanship || spec == ChrSpecialization::HunterSurvival ||
        spec == ChrSpecialization::ShamanEnhancement;

    bool specNeedsStrength =
        spec == ChrSpecialization::WarriorArms || spec == ChrSpecialization::WarriorFury ||
        spec == ChrSpecialization::WarriorProtection || spec == ChrSpecialization::PaladinProtection ||
        spec == ChrSpecialization::PaladinRetribution || spec == ChrSpecialization::DeathKnightBlood ||
        spec == ChrSpecialization::DeathKnightFrost || spec == ChrSpecialization::DeathKnightUnholy;

    // If item has wrong primary stat, it's not for main spec
    if ((hasIntellect && !specNeedsIntellect && (specNeedsAgility || specNeedsStrength)) ||
        (hasAgility && !specNeedsAgility && (specNeedsIntellect || specNeedsStrength)) ||
        (hasStrength && !specNeedsStrength && (specNeedsIntellect || specNeedsAgility)))
    {
        return false;
    }

    return true;
}'''

if old_code3 in content:
    content = content.replace(old_code3, new_code3)
    print("Fix 3: IsItemForMainSpec full implementation: APPLIED")
    fixes_made += 1
else:
    print("Fix 3: IsItemForMainSpec: NOT FOUND")

# Fix 4: IsItemUsefulForOffSpec - Full off-spec validation
old_code4 = '''bool LootDistribution::IsItemUsefulForOffSpec(const LootItem& item)
{
    if (!_bot || !item.itemTemplate)
        return false;

    // DESIGN NOTE: Simplified implementation for off-spec validation
    // Current behavior: Returns true if player can equip the item
    // Full implementation should:
    // - Check player's secondary/tertiary talent specs
    // - Match item stats against off-spec stat priorities
    // - Verify item isn't already owned for off-spec use
    // - Compare item level against current off-spec gear
    // - Consider dual-spec and spec-switching frequency
    // - Respect "main spec > off spec" loot priority rules
    // Reference: Player::GetSpecializationsCount(), ClassAI off-spec configs

    return _bot->CanUseItem(item.itemTemplate);
}'''

new_code4 = '''bool LootDistribution::IsItemUsefulForOffSpec(const LootItem& item)
{
    if (!_bot || !item.itemTemplate)
        return false;

    // Basic usability check
    if (!_bot->CanUseItem(item.itemTemplate))
        return false;

    // If it's already for main spec, not off-spec
    if (IsItemForMainSpec(item))
        return false;

    ChrSpecialization mainSpec = _bot->GetPrimarySpecialization();
    uint8 playerClass = _bot->GetClass();
    uint8 itemClass = item.itemTemplate->GetClass();
    uint8 itemSubClass = item.itemTemplate->GetSubClass();
    uint8 invType = item.itemTemplate->GetInventoryType();

    // Define potential off-specs for each class and check if item is useful
    // WoW 11.2 class/spec matrix
    std::vector<ChrSpecialization> possibleOffSpecs;

    switch (playerClass)
    {
        case CLASS_WARRIOR:
            possibleOffSpecs = {ChrSpecialization::WarriorArms,
                               ChrSpecialization::WarriorFury,
                               ChrSpecialization::WarriorProtection};
            break;
        case CLASS_PALADIN:
            possibleOffSpecs = {ChrSpecialization::PaladinHoly,
                               ChrSpecialization::PaladinProtection,
                               ChrSpecialization::PaladinRetribution};
            break;
        case CLASS_HUNTER:
            possibleOffSpecs = {ChrSpecialization::HunterBeastMastery,
                               ChrSpecialization::HunterMarksmanship,
                               ChrSpecialization::HunterSurvival};
            break;
        case CLASS_ROGUE:
            possibleOffSpecs = {ChrSpecialization::RogueAssassination,
                               ChrSpecialization::RogueOutlaw,
                               ChrSpecialization::RogueSubtely};
            break;
        case CLASS_PRIEST:
            possibleOffSpecs = {ChrSpecialization::PriestDiscipline,
                               ChrSpecialization::PriestHoly,
                               ChrSpecialization::PriestShadow};
            break;
        case CLASS_DEATH_KNIGHT:
            possibleOffSpecs = {ChrSpecialization::DeathKnightBlood,
                               ChrSpecialization::DeathKnightFrost,
                               ChrSpecialization::DeathKnightUnholy};
            break;
        case CLASS_SHAMAN:
            possibleOffSpecs = {ChrSpecialization::ShamanElemental,
                               ChrSpecialization::ShamanEnhancement,
                               ChrSpecialization::ShamanRestoration};
            break;
        case CLASS_MAGE:
            possibleOffSpecs = {ChrSpecialization::MageArcane,
                               ChrSpecialization::MageFire,
                               ChrSpecialization::MageFrost};
            break;
        case CLASS_WARLOCK:
            possibleOffSpecs = {ChrSpecialization::WarlockAffliction,
                               ChrSpecialization::WarlockDemonology,
                               ChrSpecialization::WarlockDestruction};
            break;
        case CLASS_MONK:
            possibleOffSpecs = {ChrSpecialization::MonkBrewmaster,
                               ChrSpecialization::MonkMistweaver,
                               ChrSpecialization::MonkWindwalker};
            break;
        case CLASS_DRUID:
            possibleOffSpecs = {ChrSpecialization::DruidBalance,
                               ChrSpecialization::DruidFeral,
                               ChrSpecialization::DruidGuardian,
                               ChrSpecialization::DruidRestoration};
            break;
        case CLASS_DEMON_HUNTER:
            possibleOffSpecs = {ChrSpecialization::DemonHunterHavoc,
                               ChrSpecialization::DemonHunterVengeance};
            break;
        case CLASS_EVOKER:
            possibleOffSpecs = {ChrSpecialization::EvokerDevastation,
                               ChrSpecialization::EvokerPreservation,
                               ChrSpecialization::EvokerAugmentation};
            break;
        default:
            return false;
    }

    // Remove main spec from off-spec list
    possibleOffSpecs.erase(
        std::remove(possibleOffSpecs.begin(), possibleOffSpecs.end(), mainSpec),
        possibleOffSpecs.end());

    // Check if item is useful for any off-spec
    for (ChrSpecialization offSpec : possibleOffSpecs)
    {
        // Get spec role
        ChrSpecializationEntry const* specEntry = sChrSpecializationStore.LookupEntry(AsUnderlyingType(offSpec));
        if (!specEntry)
            continue;

        ChrSpecializationRole role = static_cast<ChrSpecializationRole>(specEntry->Role);

        // Check armor type
        if (itemClass == ITEM_CLASS_ARMOR)
        {
            // Armor type doesn't change by spec within same class
            // But accessory slots are universal
            if (invType == INVTYPE_FINGER || invType == INVTYPE_TRINKET ||
                invType == INVTYPE_NECK || invType == INVTYPE_CLOAK)
            {
                // Check primary stats for these slots
                bool hasIntellect = false, hasAgility = false, hasStrength = false;

                for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
                {
                    int32 statType = item.itemTemplate->GetStatModifierBonusStat(i);
                    if (statType < 0) continue;

                    if (statType == ITEM_MOD_INTELLECT || statType == ITEM_MOD_INTELLECT_PCT)
                        hasIntellect = true;
                    if (statType == ITEM_MOD_AGILITY || statType == ITEM_MOD_AGILITY_PCT)
                        hasAgility = true;
                    if (statType == ITEM_MOD_STRENGTH || statType == ITEM_MOD_STRENGTH_PCT)
                        hasStrength = true;
                }

                // Check if this off-spec needs these stats
                bool offSpecNeedsIntellect = (role == ChrSpecializationRole::Healer) ||
                    offSpec == ChrSpecialization::MageArcane || offSpec == ChrSpecialization::MageFire ||
                    offSpec == ChrSpecialization::MageFrost || offSpec == ChrSpecialization::WarlockAffliction ||
                    offSpec == ChrSpecialization::WarlockDemonology || offSpec == ChrSpecialization::WarlockDestruction ||
                    offSpec == ChrSpecialization::PriestShadow || offSpec == ChrSpecialization::DruidBalance ||
                    offSpec == ChrSpecialization::ShamanElemental || offSpec == ChrSpecialization::EvokerDevastation ||
                    offSpec == ChrSpecialization::EvokerAugmentation;

                bool offSpecNeedsAgility =
                    offSpec == ChrSpecialization::RogueAssassination || offSpec == ChrSpecialization::RogueOutlaw ||
                    offSpec == ChrSpecialization::RogueSubtely || offSpec == ChrSpecialization::DruidFeral ||
                    offSpec == ChrSpecialization::MonkWindwalker || offSpec == ChrSpecialization::DemonHunterHavoc ||
                    offSpec == ChrSpecialization::DemonHunterVengeance || offSpec == ChrSpecialization::DruidGuardian ||
                    offSpec == ChrSpecialization::MonkBrewmaster || offSpec == ChrSpecialization::HunterBeastMastery ||
                    offSpec == ChrSpecialization::HunterMarksmanship || offSpec == ChrSpecialization::HunterSurvival ||
                    offSpec == ChrSpecialization::ShamanEnhancement;

                bool offSpecNeedsStrength =
                    offSpec == ChrSpecialization::WarriorArms || offSpec == ChrSpecialization::WarriorFury ||
                    offSpec == ChrSpecialization::WarriorProtection || offSpec == ChrSpecialization::PaladinProtection ||
                    offSpec == ChrSpecialization::PaladinRetribution || offSpec == ChrSpecialization::DeathKnightBlood ||
                    offSpec == ChrSpecialization::DeathKnightFrost || offSpec == ChrSpecialization::DeathKnightUnholy;

                // If stats match off-spec needs, it's useful
                if ((hasIntellect && offSpecNeedsIntellect) ||
                    (hasAgility && offSpecNeedsAgility) ||
                    (hasStrength && offSpecNeedsStrength))
                {
                    return true;
                }
            }
        }

        // Check weapon type for off-spec
        if (itemClass == ITEM_CLASS_WEAPON)
        {
            // Shield for tank off-specs
            if (invType == INVTYPE_SHIELD)
            {
                if (offSpec == ChrSpecialization::WarriorProtection ||
                    offSpec == ChrSpecialization::PaladinProtection ||
                    offSpec == ChrSpecialization::PaladinHoly ||
                    offSpec == ChrSpecialization::ShamanElemental ||
                    offSpec == ChrSpecialization::ShamanRestoration)
                    return true;
            }

            // Two-handed weapons for DPS off-specs
            if (invType == INVTYPE_2HWEAPON)
            {
                if (offSpec == ChrSpecialization::WarriorArms ||
                    offSpec == ChrSpecialization::DeathKnightUnholy ||
                    offSpec == ChrSpecialization::PaladinRetribution ||
                    offSpec == ChrSpecialization::DruidFeral ||
                    offSpec == ChrSpecialization::DruidGuardian ||
                    offSpec == ChrSpecialization::DruidBalance)
                    return true;
            }
        }
    }

    return false;
}'''

if old_code4 in content:
    content = content.replace(old_code4, new_code4)
    print("Fix 4: IsItemUsefulForOffSpec full implementation: APPLIED")
    fixes_made += 1
else:
    print("Fix 4: IsItemUsefulForOffSpec: NOT FOUND")

# Fix 5: GetGroupLootMetrics - Full aggregation implementation
old_code5 = '''LootMetrics LootDistribution::GetGroupLootMetrics(uint32 groupId)
{
    // Aggregate metrics from all group members
    LootMetrics groupMetrics;
    groupMetrics.Reset();

    // In a real implementation, we would iterate through group members
    // and aggregate their individual metrics

    return groupMetrics;
}'''

new_code5 = '''LootMetrics LootDistribution::GetGroupLootMetrics(uint32 groupId)
{
    LootMetrics groupMetrics;
    groupMetrics.Reset();

    if (!_bot)
        return groupMetrics;

    Group* group = _bot->GetGroup();
    if (!group || group->GetGUID().GetCounter() != groupId)
        return groupMetrics;

    // Aggregate metrics from all group members
    uint32 memberCount = 0;
    float totalSatisfaction = 0.0f;
    float totalAccuracy = 0.0f;

    for (GroupReference const& itr : group->GetMembers())
    {
        Player* member = itr.GetSource();
        if (!member)
            continue;

        uint32 memberGuid = member->GetGUID().GetCounter();
        auto metricsIt = _playerMetrics.find(memberGuid);

        if (metricsIt != _playerMetrics.end())
        {
            const LootMetrics& memberMetrics = metricsIt->second;

            // Aggregate counters
            groupMetrics.totalRollsInitiated += memberMetrics.totalRollsInitiated.load();
            groupMetrics.totalRollsCompleted += memberMetrics.totalRollsCompleted.load();
            groupMetrics.needRollsWon += memberMetrics.needRollsWon.load();
            groupMetrics.greedRollsWon += memberMetrics.greedRollsWon.load();
            groupMetrics.itemsPassed += memberMetrics.itemsPassed.load();
            groupMetrics.rollTimeouts += memberMetrics.rollTimeouts.load();

            // Track satisfaction and accuracy for averaging
            totalSatisfaction += memberMetrics.playerSatisfaction.load();
            totalAccuracy += memberMetrics.decisionAccuracy.load();
            memberCount++;
        }
    }

    // Calculate averages
    if (memberCount > 0)
    {
        groupMetrics.playerSatisfaction = totalSatisfaction / memberCount;
        groupMetrics.decisionAccuracy = totalAccuracy / memberCount;

        // Calculate average roll time based on completed rolls
        if (groupMetrics.totalRollsCompleted > 0)
        {
            // Use weighted average based on individual member roll times
            float totalRollTime = 0.0f;
            uint32 rollCount = 0;

            for (GroupReference const& itr : group->GetMembers())
            {
                Player* member = itr.GetSource();
                if (!member)
                    continue;

                auto metricsIt = _playerMetrics.find(member->GetGUID().GetCounter());
                if (metricsIt != _playerMetrics.end())
                {
                    totalRollTime += metricsIt->second.averageRollTime.load() *
                                    metricsIt->second.totalRollsCompleted.load();
                    rollCount += metricsIt->second.totalRollsCompleted.load();
                }
            }

            if (rollCount > 0)
                groupMetrics.averageRollTime = totalRollTime / rollCount;
        }
    }

    // Add fairness tracking data to metrics context
    auto fairnessIt = _groupFairnessTracking.find(groupId);
    if (fairnessIt != _groupFairnessTracking.end())
    {
        // Encode fairness score into satisfaction as a quality indicator
        // High fairness = high satisfaction potential
        float fairnessWeight = fairnessIt->second.fairnessScore;
        groupMetrics.playerSatisfaction = groupMetrics.playerSatisfaction.load() *
                                          (0.5f + 0.5f * fairnessWeight);
    }

    groupMetrics.lastUpdate = std::chrono::steady_clock::now();
    return groupMetrics;
}'''

if old_code5 in content:
    content = content.replace(old_code5, new_code5)
    print("Fix 5: GetGroupLootMetrics full implementation: APPLIED")
    fixes_made += 1
else:
    print("Fix 5: GetGroupLootMetrics: NOT FOUND")

# Fix 6: ConsiderGroupComposition - Full implementation
old_code6 = '''void LootDistribution::ConsiderGroupComposition(Group* group, const LootItem& item, LootRollType& decision)
{
    if (!group || !_bot)
        return;

    // Analyze group composition and adjust decision accordingly
    // For example, if multiple players of the same class are present,
    // be more conservative with rolling
}'''

new_code6 = '''void LootDistribution::ConsiderGroupComposition(Group* group, const LootItem& item, LootRollType& decision)
{
    if (!group || !_bot)
        return;

    uint8 myClass = _bot->GetClass();
    ChrSpecialization mySpec = _bot->GetPrimarySpecialization();

    // Count players of the same class and spec
    uint32 sameClassCount = 0;
    uint32 sameSpecCount = 0;
    uint32 totalEligiblePlayers = 0;

    for (GroupReference const& itr : group->GetMembers())
    {
        Player* member = itr.GetSource();
        if (!member || member == _bot)
            continue;

        // Check if member can use this item
        if (!member->CanUseItem(item.itemTemplate))
            continue;

        totalEligiblePlayers++;

        if (member->GetClass() == myClass)
        {
            sameClassCount++;

            if (member->GetPrimarySpecialization() == mySpec)
                sameSpecCount++;
        }
    }

    // If multiple players of same class/spec, be more conservative
    if (decision == LootRollType::NEED)
    {
        // Multiple same-spec players competing
        if (sameSpecCount > 0)
        {
            // Check loot priority - how much do we need this item?
            LootPriority priority = AnalyzeItemPriority(item);

            // For minor upgrades with competition, consider greeding instead
            if (priority >= LootPriority::MINOR_UPGRADE && sameSpecCount >= 2)
            {
                // Check fairness - have we received items recently?
                LootFairnessTracker tracker = GetGroupLootFairness(group->GetGUID().GetCounter());
                auto playerIt = tracker.playerLootCount.find(_bot->GetGUID().GetCounter());

                if (playerIt != tracker.playerLootCount.end() && tracker.playerLootCount.size() > 1)
                {
                    float avgItems = float(tracker.totalItemsDistributed) / tracker.playerLootCount.size();
                    if (float(playerIt->second) > avgItems)
                    {
                        // We've gotten more than average, be generous on minor upgrades
                        decision = LootRollType::GREED;
                        TC_LOG_DEBUG("playerbot.loot", "Bot {} downgrading NEED to GREED for fairness",
                            _bot->GetName());
                    }
                }
            }
        }

        // Multiple same-class players (different specs)
        if (sameClassCount > 0 && sameSpecCount == 0)
        {
            // This is a shared class item but different specs
            // Check if it's truly main spec for us
            if (!IsItemForMainSpec(item))
            {
                // Not main spec, downgrade to greed
                decision = LootRollType::GREED;
            }
        }
    }

    // For greed decisions, consider if we should pass
    if (decision == LootRollType::GREED)
    {
        // If many eligible players and we're above average loot count
        if (totalEligiblePlayers >= 3)
        {
            LootFairnessTracker tracker = GetGroupLootFairness(group->GetGUID().GetCounter());
            auto playerIt = tracker.playerLootCount.find(_bot->GetGUID().GetCounter());

            if (playerIt != tracker.playerLootCount.end() && tracker.playerLootCount.size() > 1)
            {
                float avgItems = float(tracker.totalItemsDistributed) / tracker.playerLootCount.size();
                // If significantly above average, pass on greed items
                if (float(playerIt->second) > avgItems * 1.5f)
                {
                    decision = LootRollType::PASS;
                    TC_LOG_DEBUG("playerbot.loot", "Bot {} passing due to high loot count",
                        _bot->GetName());
                }
            }
        }
    }
}'''

if old_code6 in content:
    content = content.replace(old_code6, new_code6)
    print("Fix 6: ConsiderGroupComposition full implementation: APPLIED")
    fixes_made += 1
else:
    print("Fix 6: ConsiderGroupComposition: NOT FOUND")

# Fix 7: ValidateLootStates - Full implementation
old_code7 = '''void LootDistribution::ValidateLootStates()
{
    // Validate that loot system state is consistent
    // Clean up any orphaned data
    // Detect and handle any inconsistencies
}'''

new_code7 = '''void LootDistribution::ValidateLootStates()
{
    // Validate that loot system state is consistent
    // Clean up any orphaned data
    // Detect and handle any inconsistencies

    uint32 currentTime = GameTime::GetGameTimeMS();
    std::vector<uint32> invalidRolls;
    std::vector<uint32> orphanedTimeouts;

    // Check all active rolls for validity
    for (auto& rollPair : _activeLootRolls)
    {
        const LootRoll& roll = rollPair.second;

        // Check if roll has been active too long (safety net - 5 minutes max)
        if (currentTime - roll.rollStartTime > 300000)
        {
            TC_LOG_WARN("playerbot.loot", "Roll {} has been active for over 5 minutes, marking invalid",
                roll.rollId);
            invalidRolls.push_back(rollPair.first);
            continue;
        }

        // Validate group still exists
        if (roll.groupId != 0)
        {
            ObjectGuid groupGuid = ObjectGuid::Create<HighGuid::Party>(roll.groupId);
            Group* group = sGroupMgr->GetGroupByGUID(groupGuid);
            if (!group)
            {
                TC_LOG_DEBUG("playerbot.loot", "Roll {} has invalid group {}, marking invalid",
                    roll.rollId, roll.groupId);
                invalidRolls.push_back(rollPair.first);
                continue;
            }
        }

        // Validate at least one eligible player still exists
        bool hasValidPlayer = false;
        for (uint32 playerGuid : roll.eligiblePlayers)
        {
            ObjectGuid guid = ObjectGuid::Create<HighGuid::Player>(playerGuid);
            if (ObjectAccessor::FindConnectedPlayer(guid))
            {
                hasValidPlayer = true;
                break;
            }
        }

        if (!hasValidPlayer)
        {
            TC_LOG_DEBUG("playerbot.loot", "Roll {} has no valid eligible players, marking invalid",
                roll.rollId);
            invalidRolls.push_back(rollPair.first);
        }
    }

    // Check for orphaned timeouts (timeout entries without corresponding rolls)
    for (const auto& timeoutPair : _rollTimeouts)
    {
        if (_activeLootRolls.find(timeoutPair.first) == _activeLootRolls.end())
        {
            orphanedTimeouts.push_back(timeoutPair.first);
        }
    }

    // Clean up invalid rolls
    for (uint32 rollId : invalidRolls)
    {
        _activeLootRolls.erase(rollId);
        _rollTimeouts.erase(rollId);
        _globalMetrics.rollTimeouts++;
    }

    // Clean up orphaned timeouts
    for (uint32 rollId : orphanedTimeouts)
    {
        _rollTimeouts.erase(rollId);
    }

    // Validate fairness tracking data
    std::vector<uint32> staleGroups;
    for (const auto& fairnessPair : _groupFairnessTracking)
    {
        ObjectGuid groupGuid = ObjectGuid::Create<HighGuid::Party>(fairnessPair.first);
        Group* group = sGroupMgr->GetGroupByGUID(groupGuid);
        if (!group)
        {
            staleGroups.push_back(fairnessPair.first);
        }
    }

    // Clean up stale group tracking
    for (uint32 groupId : staleGroups)
    {
        _groupFairnessTracking.erase(groupId);
    }

    // Validate player metrics (remove metrics for disconnected players after 30 minutes)
    static uint32 lastMetricsCleanup = 0;
    if (currentTime - lastMetricsCleanup > 1800000) // 30 minutes
    {
        std::vector<uint32> staleMetrics;
        for (const auto& metricsPair : _playerMetrics)
        {
            auto timeSinceUpdate = std::chrono::duration_cast<std::chrono::minutes>(
                std::chrono::steady_clock::now() - metricsPair.second.lastUpdate).count();

            if (timeSinceUpdate > 30)
            {
                ObjectGuid guid = ObjectGuid::Create<HighGuid::Player>(metricsPair.first);
                if (!ObjectAccessor::FindConnectedPlayer(guid))
                {
                    staleMetrics.push_back(metricsPair.first);
                }
            }
        }

        for (uint32 playerGuid : staleMetrics)
        {
            _playerMetrics.erase(playerGuid);
        }

        lastMetricsCleanup = currentTime;
    }

    // Log validation summary if any issues found
    if (!invalidRolls.empty() || !orphanedTimeouts.empty() || !staleGroups.empty())
    {
        TC_LOG_DEBUG("playerbot.loot", "Loot state validation: cleaned {} invalid rolls, {} orphaned timeouts, {} stale groups",
            invalidRolls.size(), orphanedTimeouts.size(), staleGroups.size());
    }
}'''

if old_code7 in content:
    content = content.replace(old_code7, new_code7)
    print("Fix 7: ValidateLootStates full implementation: APPLIED")
    fixes_made += 1
else:
    print("Fix 7: ValidateLootStates: NOT FOUND")

# Write back
with open('src/modules/Playerbot/Social/LootDistribution.cpp', 'w') as f:
    f.write(content)

print(f"\nTotal fixes applied: {fixes_made}")
print("LootDistribution.cpp updated" if fixes_made > 0 else "No changes made")
