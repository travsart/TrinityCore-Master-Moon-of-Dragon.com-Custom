#!/usr/bin/env python3
"""
Fix RoleAssignment.cpp gear scoring to remove deprecated ItemStat array access.
TrinityCore 11.x uses different API for item stats.
"""

import os
import re

ra_path = 'C:/TrinityBots/TrinityCore/src/modules/Playerbot/Group/RoleAssignment.cpp'

with open(ra_path, 'r', encoding='utf-8', errors='replace') as f:
    content = f.read()

# Find and replace CalculateGearScore function
old_calc_gear_score = '''float RoleAssignment::CalculateGearScore(GroupRole role)
{
    if (!_bot)
        return 0.0f;

    float gearScore = 0.0f;
    uint32 itemCount = 0;
    float totalItemLevel = 0.0f;

    // Analyze equipped items
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* item = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            continue;

        ItemTemplate const* itemTemplate = item->GetTemplate();
        if (!itemTemplate)
            continue;

        itemCount++;
        totalItemLevel += item->GetItemLevel();

        // Analyze item stats for role appropriateness
        float roleBonus = 0.0f;

        // Get item's primary stats
        for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        {
            if (itemTemplate->ItemStat[i].ItemStatValue == 0)
                continue;

            uint32 statType = itemTemplate->ItemStat[i].ItemStatType;
            int32 statValue = itemTemplate->ItemStat[i].ItemStatValue;

            switch (role)
            {
                case GroupRole::TANK:
                    // Tanks need Stamina, Armor, Avoidance
                    if (statType == ITEM_MOD_STAMINA)
                        roleBonus += statValue * 0.4f;
                    else if (statType == ITEM_MOD_DODGE_RATING || statType == ITEM_MOD_PARRY_RATING)
                        roleBonus += statValue * 0.3f;
                    else if (statType == ITEM_MOD_AGILITY)
                        roleBonus += statValue * 0.2f;
                    else if (statType == ITEM_MOD_STRENGTH || statType == ITEM_MOD_AGILITY)
                        roleBonus += statValue * 0.1f;
                    break;

                case GroupRole::HEALER:
                    // Healers need Intellect, Spirit, Mana Regen
                    if (statType == ITEM_MOD_INTELLECT)
                        roleBonus += statValue * 0.5f;
                    else if (statType == ITEM_MOD_SPIRIT || statType == ITEM_MOD_MANA_REGENERATION)
                        roleBonus += statValue * 0.3f;
                    else if (statType == ITEM_MOD_STAMINA)
                        roleBonus += statValue * 0.2f;
                    break;

                case GroupRole::MELEE_DPS:
                    // Melee DPS need Strength/Agility, Crit, Haste
                    if (statType == ITEM_MOD_STRENGTH || statType == ITEM_MOD_AGILITY)
                        roleBonus += statValue * 0.4f;
                    else if (statType == ITEM_MOD_CRIT_RATING || statType == ITEM_MOD_HASTE_RATING)
                        roleBonus += statValue * 0.3f;
                    else if (statType == ITEM_MOD_ATTACK_POWER)
                        roleBonus += statValue * 0.2f;
                    else if (statType == ITEM_MOD_STAMINA)
                        roleBonus += statValue * 0.1f;
                    break;

                case GroupRole::RANGED_DPS:
                    // Ranged DPS need Intellect/Agility, Crit, Mastery
                    if (statType == ITEM_MOD_INTELLECT || statType == ITEM_MOD_AGILITY)
                        roleBonus += statValue * 0.4f;
                    else if (statType == ITEM_MOD_CRIT_RATING || statType == ITEM_MOD_MASTERY_RATING)
                        roleBonus += statValue * 0.3f;
                    else if (statType == ITEM_MOD_SPELL_POWER || statType == ITEM_MOD_RANGED_ATTACK_POWER)
                        roleBonus += statValue * 0.2f;
                    else if (statType == ITEM_MOD_STAMINA)
                        roleBonus += statValue * 0.1f;
                    break;

                default:
                    break;
            }
        }

        gearScore += roleBonus;
    }

    if (itemCount == 0)
        return 0.0f;

    // Normalize score to 0.0 - 1.0 range
    float averageItemLevel = totalItemLevel / itemCount;
    float itemLevelScore = std::min(1.0f, averageItemLevel / 300.0f); // Normalize around ilvl 300

    float roleAppropriateness = std::min(1.0f, gearScore / (itemCount * 50.0f)); // Normalize based on stats

    // Combined score (70% item level, 30% role-appropriate stats)
    float finalScore = (itemLevelScore * 0.7f) + (roleAppropriateness * 0.3f);

    TC_LOG_DEBUG("playerbot", "RoleAssignment: Gear score for player {} in role {}: {:.2f} (iLvl: {:.1f}, appropriateness: {:.2f})",
                 _bot->GetName(), static_cast<uint8>(role), finalScore, averageItemLevel, roleAppropriateness);

    return finalScore;
}'''

new_calc_gear_score = '''float RoleAssignment::CalculateGearScore(GroupRole role)
{
    if (!_bot)
        return 0.0f;

    uint32 itemCount = 0;
    float totalItemLevel = 0.0f;

    // Analyze equipped items - using item level based scoring
    // TrinityCore 11.x doesn't expose ItemStat array directly
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* item = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            continue;

        itemCount++;
        totalItemLevel += static_cast<float>(item->GetItemLevel());
    }

    if (itemCount == 0)
        return 0.0f;

    // Normalize score to 0.0 - 1.0 range based on item level
    float averageItemLevel = totalItemLevel / static_cast<float>(itemCount);

    // Normalize around TWW item levels (roughly 400-600 range)
    float itemLevelScore = std::min(1.0f, averageItemLevel / 500.0f);

    // Role-based adjustment using player stats
    float roleModifier = 1.0f;

    switch (role)
    {
        case GroupRole::TANK:
            // Tanks benefit from high stamina
            roleModifier = 1.0f + (static_cast<float>(_bot->GetStat(STAT_STAMINA)) / 50000.0f);
            break;

        case GroupRole::HEALER:
            // Healers benefit from intellect
            roleModifier = 1.0f + (static_cast<float>(_bot->GetStat(STAT_INTELLECT)) / 30000.0f);
            break;

        case GroupRole::MELEE_DPS:
            // Melee DPS benefits from strength/agility
            roleModifier = 1.0f + (static_cast<float>(std::max(_bot->GetStat(STAT_STRENGTH), _bot->GetStat(STAT_AGILITY))) / 30000.0f);
            break;

        case GroupRole::RANGED_DPS:
            // Ranged DPS benefits from intellect/agility
            roleModifier = 1.0f + (static_cast<float>(std::max(_bot->GetStat(STAT_INTELLECT), _bot->GetStat(STAT_AGILITY))) / 30000.0f);
            break;

        default:
            break;
    }

    // Cap role modifier at 1.5x
    roleModifier = std::min(1.5f, roleModifier);

    float finalScore = itemLevelScore * roleModifier;
    finalScore = std::min(1.0f, finalScore); // Cap at 1.0

    TC_LOG_DEBUG("playerbot", "RoleAssignment: Gear score for {} in role {}: {:.2f} (iLvl: {:.1f})",
                 _bot->GetName(), static_cast<uint8>(role), finalScore, averageItemLevel);

    return finalScore;
}'''

content = content.replace(old_calc_gear_score, new_calc_gear_score)
print("Replaced CalculateGearScore function")

# Now fix AnalyzePlayerGear function - remove ItemStat references
# Find the problematic section and simplify it
old_analyze_gear = '''    // Analyze overall gear quality
    uint32 totalItemLevel = 0;
    uint32 itemCount = 0;
    bool hasTankGear = false;
    bool hasHealerGear = false;
    bool hasDPSGear = false;

    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* item = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            continue;

        ItemTemplate const* itemTemplate = item->GetTemplate();
        if (!itemTemplate)
            continue;

        itemCount++;
        totalItemLevel += item->GetItemLevel();

        // Detect gear type based on primary stats
        for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        {
            if (itemTemplate->ItemStat[i].ItemStatValue == 0)
                continue;

            uint32 statType = itemTemplate->ItemStat[i].ItemStatType;

            // Tank stats
            if (statType == ITEM_MOD_DODGE_RATING || statType == ITEM_MOD_PARRY_RATING)
                hasTankGear = true;

            // Healer stats
            if (statType == ITEM_MOD_SPIRIT && _bot->GetPowerType() == POWER_MANA)
                hasHealerGear = true;

            // DPS stats
            if (statType == ITEM_MOD_CRIT_RATING || statType == ITEM_MOD_HASTE_RATING ||
                statType == ITEM_MOD_MASTERY_RATING)
                hasDPSGear = true;
        }
    }

    if (itemCount > 0)
    {
        float averageItemLevel = static_cast<float>(totalItemLevel) / itemCount;
        profile.overallRating = averageItemLevel / 30.0f; // Normalize to 0-10 scale

        TC_LOG_DEBUG("playerbot", "RoleAssignment: Player {} average item level: {:.1f}, overall rating: {:.1f}",
                     _bot->GetName(), averageItemLevel, profile.overallRating);
    }

    // Update role capabilities based on gear
    if (hasTankGear)
    {
        if (profile.roleCapabilities[GroupRole::TANK] == RoleCapability::INCAPABLE)
            profile.roleCapabilities[GroupRole::TANK] = RoleCapability::EMERGENCY;
    }

    if (hasHealerGear)
    {
        if (profile.roleCapabilities[GroupRole::HEALER] == RoleCapability::INCAPABLE)
            profile.roleCapabilities[GroupRole::HEALER] = RoleCapability::EMERGENCY;
    }

    if (hasDPSGear)
    {
        if (profile.roleCapabilities[GroupRole::MELEE_DPS] == RoleCapability::INCAPABLE)
            profile.roleCapabilities[GroupRole::MELEE_DPS] = RoleCapability::EMERGENCY;
        if (profile.roleCapabilities[GroupRole::RANGED_DPS] == RoleCapability::INCAPABLE)
            profile.roleCapabilities[GroupRole::RANGED_DPS] = RoleCapability::EMERGENCY;
    }

    profile.lastRoleUpdate = GameTime::GetGameTimeMS();
}'''

new_analyze_gear = '''    // Analyze overall gear quality using item level
    uint32 totalItemLevel = 0;
    uint32 itemCount = 0;

    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* item = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            continue;

        itemCount++;
        totalItemLevel += item->GetItemLevel();
    }

    if (itemCount > 0)
    {
        float averageItemLevel = static_cast<float>(totalItemLevel) / static_cast<float>(itemCount);
        profile.overallRating = averageItemLevel / 50.0f; // Normalize to ~0-10 scale for TWW

        TC_LOG_DEBUG("playerbot", "RoleAssignment: Player {} average item level: {:.1f}, overall rating: {:.1f}",
                     _bot->GetName(), averageItemLevel, profile.overallRating);
    }

    // Determine gear type based on primary stat (using player stats, not item stats)
    // High Stamina indicates tanking potential
    bool hasTankGear = _bot->GetStat(STAT_STAMINA) > (_bot->GetLevel() * 50);
    // High Intellect with mana indicates healing potential
    bool hasHealerGear = _bot->GetPowerType() == POWER_MANA && _bot->GetStat(STAT_INTELLECT) > (_bot->GetLevel() * 30);
    // High primary DPS stat indicates damage potential
    bool hasDPSGear = std::max({_bot->GetStat(STAT_STRENGTH), _bot->GetStat(STAT_AGILITY), _bot->GetStat(STAT_INTELLECT)}) > (_bot->GetLevel() * 20);

    // Update role capabilities based on gear
    if (hasTankGear)
    {
        if (profile.roleCapabilities[GroupRole::TANK] == RoleCapability::INCAPABLE)
            profile.roleCapabilities[GroupRole::TANK] = RoleCapability::EMERGENCY;
    }

    if (hasHealerGear)
    {
        if (profile.roleCapabilities[GroupRole::HEALER] == RoleCapability::INCAPABLE)
            profile.roleCapabilities[GroupRole::HEALER] = RoleCapability::EMERGENCY;
    }

    if (hasDPSGear)
    {
        if (profile.roleCapabilities[GroupRole::MELEE_DPS] == RoleCapability::INCAPABLE)
            profile.roleCapabilities[GroupRole::MELEE_DPS] = RoleCapability::EMERGENCY;
        if (profile.roleCapabilities[GroupRole::RANGED_DPS] == RoleCapability::INCAPABLE)
            profile.roleCapabilities[GroupRole::RANGED_DPS] = RoleCapability::EMERGENCY;
    }

    profile.lastRoleUpdate = GameTime::GetGameTimeMS();
}'''

content = content.replace(old_analyze_gear, new_analyze_gear)
print("Replaced AnalyzePlayerGear gear analysis section")

# Remove MAX_ITEM_PROTO_STATS and other ItemStat references that might remain
content = re.sub(r'for \(uint32 i = 0; i < MAX_ITEM_PROTO_STATS; \+\+i\)\s*\{[^}]+itemTemplate->ItemStat\[i\][^}]+\}', '', content)

with open(ra_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("RoleAssignment.cpp gear scoring fixed!")
