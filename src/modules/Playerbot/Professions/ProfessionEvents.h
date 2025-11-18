/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef PLAYERBOT_PROFESSION_EVENTS_H
#define PLAYERBOT_PROFESSION_EVENTS_H

#include "Define.h"
#include "ObjectGuid.h"
#include "ProfessionManager.h"
#include <chrono>
#include <string>

namespace Playerbot
{

enum class ProfessionEventType : uint8
{
    RECIPE_LEARNED = 0,         // Bot learned new recipe
    SKILL_UP,                   // Profession skill increased
    CRAFTING_STARTED,           // Bot started crafting item
    CRAFTING_COMPLETED,         // Bot completed crafting item
    CRAFTING_FAILED,            // Crafting attempt failed
    MATERIALS_NEEDED,           // Bot needs materials for recipe
    MATERIAL_GATHERED,          // Bot gathered material from node
    MATERIAL_PURCHASED,         // Bot bought material from AH
    ITEM_BANKED,                // Bot deposited item to bank
    ITEM_WITHDRAWN,             // Bot withdrew item from bank
    GOLD_BANKED,                // Bot deposited gold to bank
    GOLD_WITHDRAWN,             // Bot withdrew gold from bank
    MAX_PROFESSION_EVENT
};

enum class ProfessionEventPriority : uint8
{
    CRITICAL = 0,
    HIGH = 1,
    MEDIUM = 2,
    LOW = 3,
    BATCH = 4
};

struct ProfessionEvent
{
    using EventType = ProfessionEventType;
    using Priority = ProfessionEventPriority;

    ProfessionEventType type;
    ObjectGuid playerGuid;
    ProfessionType profession;
    uint32 recipeId;
    uint32 itemId;
    uint32 quantity;
    uint32 skillBefore;
    uint32 skillAfter;
    uint32 goldAmount;              // For banking events
    std::string reason;             // Human-readable event reason
    std::chrono::steady_clock::time_point timestamp;

    bool IsValid() const;
    std::string ToString() const;

    // Factory methods for each event type
    static ProfessionEvent RecipeLearned(ObjectGuid playerGuid, ProfessionType profession, uint32 recipeId);
    static ProfessionEvent SkillUp(ObjectGuid playerGuid, ProfessionType profession, uint32 skillBefore, uint32 skillAfter);
    static ProfessionEvent CraftingStarted(ObjectGuid playerGuid, ProfessionType profession, uint32 recipeId, uint32 itemId);
    static ProfessionEvent CraftingCompleted(ObjectGuid playerGuid, ProfessionType profession, uint32 recipeId, uint32 itemId, uint32 quantity);
    static ProfessionEvent CraftingFailed(ObjectGuid playerGuid, ProfessionType profession, uint32 recipeId, std::string const& reason);
    static ProfessionEvent MaterialsNeeded(ObjectGuid playerGuid, ProfessionType profession, uint32 recipeId);
    static ProfessionEvent MaterialGathered(ObjectGuid playerGuid, ProfessionType profession, uint32 itemId, uint32 quantity);
    static ProfessionEvent MaterialPurchased(ObjectGuid playerGuid, uint32 itemId, uint32 quantity, uint32 goldSpent);
    static ProfessionEvent ItemBanked(ObjectGuid playerGuid, uint32 itemId, uint32 quantity);
    static ProfessionEvent ItemWithdrawn(ObjectGuid playerGuid, uint32 itemId, uint32 quantity);
    static ProfessionEvent GoldBanked(ObjectGuid playerGuid, uint32 goldAmount);
    static ProfessionEvent GoldWithdrawn(ObjectGuid playerGuid, uint32 goldAmount);
};

} // namespace Playerbot

#endif // PLAYERBOT_PROFESSION_EVENTS_H
