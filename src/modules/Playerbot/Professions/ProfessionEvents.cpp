/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "ProfessionEvents.h"
#include <sstream>

namespace Playerbot
{

ProfessionEvent ProfessionEvent::RecipeLearned(ObjectGuid playerGuid, ProfessionType profession, uint32 recipeId)
{
    ProfessionEvent event;
    event.type = ProfessionEventType::RECIPE_LEARNED;
    event.playerGuid = playerGuid;
    event.profession = profession;
    event.recipeId = recipeId;
    event.itemId = 0;
    event.quantity = 0;
    event.skillBefore = 0;
    event.skillAfter = 0;
    event.goldAmount = 0;
    event.reason = "Recipe learned";
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

ProfessionEvent ProfessionEvent::SkillUp(ObjectGuid playerGuid, ProfessionType profession, uint32 skillBefore, uint32 skillAfter)
{
    ProfessionEvent event;
    event.type = ProfessionEventType::SKILL_UP;
    event.playerGuid = playerGuid;
    event.profession = profession;
    event.recipeId = 0;
    event.itemId = 0;
    event.quantity = 0;
    event.skillBefore = skillBefore;
    event.skillAfter = skillAfter;
    event.goldAmount = 0;
    event.reason = "Skill increased";
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

ProfessionEvent ProfessionEvent::CraftingStarted(ObjectGuid playerGuid, ProfessionType profession, uint32 recipeId, uint32 itemId)
{
    ProfessionEvent event;
    event.type = ProfessionEventType::CRAFTING_STARTED;
    event.playerGuid = playerGuid;
    event.profession = profession;
    event.recipeId = recipeId;
    event.itemId = itemId;
    event.quantity = 0;
    event.skillBefore = 0;
    event.skillAfter = 0;
    event.goldAmount = 0;
    event.reason = "Crafting started";
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

ProfessionEvent ProfessionEvent::CraftingCompleted(ObjectGuid playerGuid, ProfessionType profession, uint32 recipeId, uint32 itemId, uint32 quantity)
{
    ProfessionEvent event;
    event.type = ProfessionEventType::CRAFTING_COMPLETED;
    event.playerGuid = playerGuid;
    event.profession = profession;
    event.recipeId = recipeId;
    event.itemId = itemId;
    event.quantity = quantity;
    event.skillBefore = 0;
    event.skillAfter = 0;
    event.goldAmount = 0;
    event.reason = "Crafting completed";
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

ProfessionEvent ProfessionEvent::CraftingFailed(ObjectGuid playerGuid, ProfessionType profession, uint32 recipeId, std::string const& reason)
{
    ProfessionEvent event;
    event.type = ProfessionEventType::CRAFTING_FAILED;
    event.playerGuid = playerGuid;
    event.profession = profession;
    event.recipeId = recipeId;
    event.itemId = 0;
    event.quantity = 0;
    event.skillBefore = 0;
    event.skillAfter = 0;
    event.goldAmount = 0;
    event.reason = reason;
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

ProfessionEvent ProfessionEvent::MaterialsNeeded(ObjectGuid playerGuid, ProfessionType profession, uint32 recipeId)
{
    ProfessionEvent event;
    event.type = ProfessionEventType::MATERIALS_NEEDED;
    event.playerGuid = playerGuid;
    event.profession = profession;
    event.recipeId = recipeId;
    event.itemId = 0;
    event.quantity = 0;
    event.skillBefore = 0;
    event.skillAfter = 0;
    event.goldAmount = 0;
    event.reason = "Materials needed for crafting";
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

ProfessionEvent ProfessionEvent::MaterialGathered(ObjectGuid playerGuid, ProfessionType profession, uint32 itemId, uint32 quantity)
{
    ProfessionEvent event;
    event.type = ProfessionEventType::MATERIAL_GATHERED;
    event.playerGuid = playerGuid;
    event.profession = profession;
    event.recipeId = 0;
    event.itemId = itemId;
    event.quantity = quantity;
    event.skillBefore = 0;
    event.skillAfter = 0;
    event.goldAmount = 0;
    event.reason = "Material gathered from node";
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

ProfessionEvent ProfessionEvent::MaterialPurchased(ObjectGuid playerGuid, uint32 itemId, uint32 quantity, uint32 goldSpent)
{
    ProfessionEvent event;
    event.type = ProfessionEventType::MATERIAL_PURCHASED;
    event.playerGuid = playerGuid;
    event.profession = ProfessionType::NONE;
    event.recipeId = 0;
    event.itemId = itemId;
    event.quantity = quantity;
    event.skillBefore = 0;
    event.skillAfter = 0;
    event.goldAmount = goldSpent;
    event.reason = "Material purchased from auction house";
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

ProfessionEvent ProfessionEvent::ItemBanked(ObjectGuid playerGuid, uint32 itemId, uint32 quantity)
{
    ProfessionEvent event;
    event.type = ProfessionEventType::ITEM_BANKED;
    event.playerGuid = playerGuid;
    event.profession = ProfessionType::NONE;
    event.recipeId = 0;
    event.itemId = itemId;
    event.quantity = quantity;
    event.skillBefore = 0;
    event.skillAfter = 0;
    event.goldAmount = 0;
    event.reason = "Item deposited to bank";
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

ProfessionEvent ProfessionEvent::ItemWithdrawn(ObjectGuid playerGuid, uint32 itemId, uint32 quantity)
{
    ProfessionEvent event;
    event.type = ProfessionEventType::ITEM_WITHDRAWN;
    event.playerGuid = playerGuid;
    event.profession = ProfessionType::NONE;
    event.recipeId = 0;
    event.itemId = itemId;
    event.quantity = quantity;
    event.skillBefore = 0;
    event.skillAfter = 0;
    event.goldAmount = 0;
    event.reason = "Item withdrawn from bank";
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

ProfessionEvent ProfessionEvent::GoldBanked(ObjectGuid playerGuid, uint32 goldAmount)
{
    ProfessionEvent event;
    event.type = ProfessionEventType::GOLD_BANKED;
    event.playerGuid = playerGuid;
    event.profession = ProfessionType::NONE;
    event.recipeId = 0;
    event.itemId = 0;
    event.quantity = 0;
    event.skillBefore = 0;
    event.skillAfter = 0;
    event.goldAmount = goldAmount;
    event.reason = "Gold deposited to bank";
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

ProfessionEvent ProfessionEvent::GoldWithdrawn(ObjectGuid playerGuid, uint32 goldAmount)
{
    ProfessionEvent event;
    event.type = ProfessionEventType::GOLD_WITHDRAWN;
    event.playerGuid = playerGuid;
    event.profession = ProfessionType::NONE;
    event.recipeId = 0;
    event.itemId = 0;
    event.quantity = 0;
    event.skillBefore = 0;
    event.skillAfter = 0;
    event.goldAmount = goldAmount;
    event.reason = "Gold withdrawn from bank";
    event.timestamp = std::chrono::steady_clock::now();
    return event;
}

bool ProfessionEvent::IsValid() const
{
    // Basic validation
    if (type >= ProfessionEventType::MAX_PROFESSION_EVENT)
        return false;

    if (!playerGuid.IsPlayer())
        return false;

    // Type-specific validation
    switch (type)
    {
        case ProfessionEventType::RECIPE_LEARNED:
            return recipeId != 0 && profession != ProfessionType::NONE;

        case ProfessionEventType::SKILL_UP:
            return skillAfter > skillBefore && profession != ProfessionType::NONE;

        case ProfessionEventType::CRAFTING_STARTED:
        case ProfessionEventType::CRAFTING_COMPLETED:
            return recipeId != 0 && itemId != 0 && profession != ProfessionType::NONE;

        case ProfessionEventType::CRAFTING_FAILED:
            return recipeId != 0 && !reason.empty();

        case ProfessionEventType::MATERIALS_NEEDED:
            return recipeId != 0 && profession != ProfessionType::NONE;

        case ProfessionEventType::MATERIAL_GATHERED:
            return itemId != 0 && quantity > 0;

        case ProfessionEventType::MATERIAL_PURCHASED:
            return itemId != 0 && quantity > 0 && goldAmount > 0;

        case ProfessionEventType::ITEM_BANKED:
        case ProfessionEventType::ITEM_WITHDRAWN:
            return itemId != 0 && quantity > 0;

        case ProfessionEventType::GOLD_BANKED:
        case ProfessionEventType::GOLD_WITHDRAWN:
            return goldAmount > 0;

        default:
            return true;
    }
}

std::string ProfessionEvent::ToString() const
{
    std::ostringstream oss;
    oss << "ProfessionEvent[";

    switch (type)
    {
        case ProfessionEventType::RECIPE_LEARNED:
            oss << "RECIPE_LEARNED, recipe=" << recipeId;
            break;
        case ProfessionEventType::SKILL_UP:
            oss << "SKILL_UP, skill=" << skillBefore << "->" << skillAfter;
            break;
        case ProfessionEventType::CRAFTING_STARTED:
            oss << "CRAFTING_STARTED, recipe=" << recipeId << ", item=" << itemId;
            break;
        case ProfessionEventType::CRAFTING_COMPLETED:
            oss << "CRAFTING_COMPLETED, recipe=" << recipeId << ", item=" << itemId << ", qty=" << quantity;
            break;
        case ProfessionEventType::CRAFTING_FAILED:
            oss << "CRAFTING_FAILED, recipe=" << recipeId << ", reason=" << reason;
            break;
        case ProfessionEventType::MATERIALS_NEEDED:
            oss << "MATERIALS_NEEDED, recipe=" << recipeId;
            break;
        case ProfessionEventType::MATERIAL_GATHERED:
            oss << "MATERIAL_GATHERED, item=" << itemId << ", qty=" << quantity;
            break;
        case ProfessionEventType::MATERIAL_PURCHASED:
            oss << "MATERIAL_PURCHASED, item=" << itemId << ", qty=" << quantity << ", gold=" << goldAmount;
            break;
        case ProfessionEventType::ITEM_BANKED:
            oss << "ITEM_BANKED, item=" << itemId << ", qty=" << quantity;
            break;
        case ProfessionEventType::ITEM_WITHDRAWN:
            oss << "ITEM_WITHDRAWN, item=" << itemId << ", qty=" << quantity;
            break;
        case ProfessionEventType::GOLD_BANKED:
            oss << "GOLD_BANKED, gold=" << goldAmount;
            break;
        case ProfessionEventType::GOLD_WITHDRAWN:
            oss << "GOLD_WITHDRAWN, gold=" << goldAmount;
            break;
        default:
            oss << "UNKNOWN";
            break;
    }

    oss << ", player=" << playerGuid.ToString() << "]";
    return oss.str();
}

} // namespace Playerbot
