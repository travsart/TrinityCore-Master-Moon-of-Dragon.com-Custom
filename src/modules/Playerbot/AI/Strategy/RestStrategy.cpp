/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "RestStrategy.h"
#include "../BotAI.h"
#include "Player.h"
#include "Item.h"
#include "Bag.h"
#include "ItemTemplate.h"
#include "SpellInfo.h"
#include "Log.h"
#include "SpellMgr.h"

namespace Playerbot
{

RestStrategy::RestStrategy()
    : Strategy("rest")
{
    TC_LOG_DEBUG("module.playerbot.strategy", "RestStrategy: Initialized");
}

void RestStrategy::InitializeActions()
{
    TC_LOG_DEBUG("module.playerbot.strategy", "RestStrategy: No actions (direct rest control)");
}

void RestStrategy::InitializeTriggers()
{
    TC_LOG_DEBUG("module.playerbot.strategy", "RestStrategy: No triggers (using relevance system)");
}

void RestStrategy::InitializeValues()
{
    TC_LOG_DEBUG("module.playerbot.strategy", "RestStrategy: No values");
}

void RestStrategy::OnActivate(BotAI* ai)
{
    if (!ai || !ai->GetBot())
        return;

    TC_LOG_INFO("module.playerbot.strategy", "Rest strategy activated for bot {}", ai->GetBot()->GetName());
    SetActive(true);
}

void RestStrategy::OnDeactivate(BotAI* ai)
{
    if (!ai || !ai->GetBot())
        return;

    TC_LOG_INFO("module.playerbot.strategy", "Rest strategy deactivated for bot {}", ai->GetBot()->GetName());
    SetActive(false);

    _isEating = false;
    _isDrinking = false;
    _restStartTime = 0;
}

bool RestStrategy::IsActive(BotAI* ai) const
{
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();

    // NOT active during combat (can't eat/drink in combat)
    if (bot->IsInCombat())
        return false;

    // Active if explicitly activated and not in a group
    return _active && !bot->GetGroup();
}

float RestStrategy::GetRelevance(BotAI* ai) const
{
    if (!ai || !ai->GetBot())
        return 0.0f;

    Player* bot = ai->GetBot();

    // Can't rest in combat
    if (bot->IsInCombat())
        return 0.0f;

    // Check if bot needs to rest
    bool needsRest = NeedsFood(ai) || NeedsDrink(ai);

    // HIGHEST priority when needs rest (even higher than quest=70)
    // Bot must rest before doing anything else
    return needsRest ? 90.0f : 0.0f;
}

void RestStrategy::UpdateBehavior(BotAI* ai, uint32 diff)
{
    if (!ai || !ai->GetBot())
        return;

    Player* bot = ai->GetBot();

    // Can't rest in combat
    if (bot->IsInCombat())
    {
        _isEating = false;
        _isDrinking = false;
        _restStartTime = 0;
        return;
    }

    // DEBUG: Log current status
    float healthPct = bot->GetHealthPct();
    float manaPct = bot->GetMaxPower(POWER_MANA) > 0
        ? (bot->GetPower(POWER_MANA) * 100.0f / bot->GetMaxPower(POWER_MANA))
        : 100.0f;

    TC_LOG_DEBUG("module.playerbot.strategy", "RestStrategy::UpdateBehavior: Bot {} health={:.1f}%, mana={:.1f}%, needsFood={}, needsDrink={}",
                 bot->GetName(), healthPct, manaPct, NeedsFood(ai), NeedsDrink(ai));

    uint32 currentTime = getMSTime();

    // Check for rest timeout (prevent infinite resting)
    if (_restStartTime > 0 && (currentTime - _restStartTime) > _maxRestTime)
    {
        TC_LOG_WARN("module.playerbot.strategy", "RestStrategy: Bot {} rest timeout after 30s",
                    bot->GetName());
        _isEating = false;
        _isDrinking = false;
        _restStartTime = 0;
        return;
    }

    // Check if rest is complete (healthPct and manaPct already declared above)

    if (_isEating && healthPct >= _restCompleteHealth)
    {
        TC_LOG_DEBUG("module.playerbot.strategy", "RestStrategy: Bot {} finished eating ({:.1f}% health)",
                     bot->GetName(), healthPct);
        _isEating = false;
    }

    if (_isDrinking && manaPct >= _restCompleteMana)
    {
        TC_LOG_DEBUG("module.playerbot.strategy", "RestStrategy: Bot {} finished drinking ({:.1f}% mana)",
                     bot->GetName(), manaPct);
        _isDrinking = false;
    }

    // If both complete, stop resting
    if (!_isEating && !_isDrinking && _restStartTime > 0)
    {
        _restStartTime = 0;
        return;
    }

    // Start eating if needed
    if (NeedsFood(ai) && !_isEating)
    {
        Item* food = FindFood(ai);
        if (food)
        {
            TC_LOG_DEBUG("module.playerbot.strategy", "RestStrategy: Bot {} found food item {}, attempting to eat",
                         bot->GetName(), food->GetTemplate()->GetName(DEFAULT_LOCALE));

            if (EatFood(ai, food))
            {
                _isEating = true;
                if (_restStartTime == 0)
                    _restStartTime = currentTime;

                TC_LOG_DEBUG("module.playerbot.strategy", "RestStrategy: Bot {} started eating ({:.1f}% health)",
                             bot->GetName(), healthPct);
            }
            else
            {
                TC_LOG_WARN("module.playerbot.strategy", "RestStrategy: Bot {} failed to eat food {}",
                            bot->GetName(), food->GetTemplate()->GetName(DEFAULT_LOCALE));
            }
        }
        else
        {
            TC_LOG_DEBUG("module.playerbot.strategy", "RestStrategy: Bot {} needs food but none found in inventory",
                         bot->GetName());
        }
    }

    // Start drinking if needed
    if (NeedsDrink(ai) && !_isDrinking)
    {
        Item* drink = FindDrink(ai);
        if (drink)
        {
            TC_LOG_DEBUG("module.playerbot.strategy", "RestStrategy: Bot {} found drink item {}, attempting to drink",
                         bot->GetName(), drink->GetTemplate()->GetName(DEFAULT_LOCALE));

            if (DrinkWater(ai, drink))
            {
                _isDrinking = true;
                if (_restStartTime == 0)
                    _restStartTime = currentTime;

                TC_LOG_DEBUG("module.playerbot.strategy", "RestStrategy: Bot {} started drinking ({:.1f}% mana)",
                             bot->GetName(), manaPct);
            }
            else
            {
                TC_LOG_WARN("module.playerbot.strategy", "RestStrategy: Bot {} failed to drink {}",
                            bot->GetName(), drink->GetTemplate()->GetName(DEFAULT_LOCALE));
            }
        }
        else
        {
            TC_LOG_DEBUG("module.playerbot.strategy", "RestStrategy: Bot {} needs drink but none found in inventory",
                         bot->GetName());
        }
    }

    // Use bandage if health critical and no food
    if (healthPct < 30.0f && !_isEating)
    {
        Item* bandage = FindBandage(ai);
        if (bandage && UseBandage(ai, bandage))
        {
            TC_LOG_DEBUG("module.playerbot.strategy", "RestStrategy: Bot {} used bandage ({:.1f}% health)",
                         bot->GetName(), healthPct);
        }
    }

    // Stop movement while resting
    if (IsResting(ai) && bot->isMoving())
    {
        bot->StopMoving();
    }
}

bool RestStrategy::NeedsFood(BotAI* ai) const
{
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();
    return bot->GetHealthPct() < _eatHealthThreshold;
}

bool RestStrategy::NeedsDrink(BotAI* ai) const
{
    if (!ai || !ai->GetBot())
        return false;

    Player* bot = ai->GetBot();

    // Only mana users need to drink
    if (bot->GetMaxPower(POWER_MANA) == 0)
        return false;

    float manaPct = (bot->GetPower(POWER_MANA) * 100.0f) / bot->GetMaxPower(POWER_MANA);
    return manaPct < _drinkManaThreshold;
}

Item* RestStrategy::FindFood(BotAI* ai) const
{
    if (!ai || !ai->GetBot())
        return nullptr;

    Player* bot = ai->GetBot();

    // Scan inventory for food
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (!item)
            continue;

        ItemTemplate const* proto = item->GetTemplate();
        if (!proto)
            continue;

        // Check if item is food
        if (proto->GetClass() == ITEM_CLASS_CONSUMABLE && proto->GetSubClass() == ITEM_SUBCLASS_FOOD_DRINK)
        {
            // Verify it's actually food by checking spell specific type
            for (ItemEffectEntry const* effect : proto->Effects)
            {
                if (effect && effect->SpellID > 0)
                {
                    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(effect->SpellID, DIFFICULTY_NONE);
                    if (spellInfo)
                    {
                        SpellSpecificType specificType = spellInfo->GetSpellSpecific();
                        if (specificType == SPELL_SPECIFIC_FOOD || specificType == SPELL_SPECIFIC_FOOD_AND_DRINK)
                            return item;
                    }
                }
            }
        }
    }

    // Check bags
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        if (Bag* pBag = bot->GetBagByPos(bag))
        {
            for (uint32 slot = 0; slot < pBag->GetBagSize(); ++slot)
            {
                Item* item = pBag->GetItemByPos(slot);
                if (!item)
                    continue;

                ItemTemplate const* proto = item->GetTemplate();
                if (!proto)
                    continue;

                if (proto->GetClass() == ITEM_CLASS_CONSUMABLE && proto->GetSubClass() == ITEM_SUBCLASS_FOOD_DRINK)
                {
                    // Verify it's actually food by checking spell specific type
                    for (ItemEffectEntry const* effect : proto->Effects)
                    {
                        if (effect && effect->SpellID > 0)
                        {
                            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(effect->SpellID, DIFFICULTY_NONE);
                            if (spellInfo)
                            {
                                SpellSpecificType specificType = spellInfo->GetSpellSpecific();
                                if (specificType == SPELL_SPECIFIC_FOOD || specificType == SPELL_SPECIFIC_FOOD_AND_DRINK)
                                    return item;
                            }
                        }
                    }
                }
            }
        }
    }

    return nullptr;
}

Item* RestStrategy::FindDrink(BotAI* ai) const
{
    if (!ai || !ai->GetBot())
        return nullptr;

    Player* bot = ai->GetBot();

    // Scan inventory for drink
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (!item)
            continue;

        ItemTemplate const* proto = item->GetTemplate();
        if (!proto)
            continue;

        // Check if item is drink (restores mana)
        if (proto->GetClass() == ITEM_CLASS_CONSUMABLE)
        {
            // Check if spell is drink-specific
            for (ItemEffectEntry const* effect : proto->Effects)
            {
                if (effect && effect->SpellID > 0)
                {
                    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(effect->SpellID, DIFFICULTY_NONE);
                    if (spellInfo)
                    {
                        SpellSpecificType specificType = spellInfo->GetSpellSpecific();
                        if (specificType == SPELL_SPECIFIC_DRINK || specificType == SPELL_SPECIFIC_FOOD_AND_DRINK)
                            return item;
                    }
                }
            }
        }
    }

    // Check bags
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        if (Bag* pBag = bot->GetBagByPos(bag))
        {
            for (uint32 slot = 0; slot < pBag->GetBagSize(); ++slot)
            {
                Item* item = pBag->GetItemByPos(slot);
                if (!item)
                    continue;

                ItemTemplate const* proto = item->GetTemplate();
                if (!proto)
                    continue;

                if (proto->GetClass() == ITEM_CLASS_CONSUMABLE)
                {
                    for (ItemEffectEntry const* effect : proto->Effects)
                    {
                        if (effect && effect->SpellID > 0)
                        {
                            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(effect->SpellID, DIFFICULTY_NONE);
                            if (spellInfo)
                            {
                                SpellSpecificType specificType = spellInfo->GetSpellSpecific();
                                if (specificType == SPELL_SPECIFIC_DRINK || specificType == SPELL_SPECIFIC_FOOD_AND_DRINK)
                                    return item;
                            }
                        }
                    }
                }
            }
        }
    }

    return nullptr;
}

Item* RestStrategy::FindBandage(BotAI* ai) const
{
    if (!ai || !ai->GetBot())
        return nullptr;

    Player* bot = ai->GetBot();

    // Bandages are usually ITEM_CLASS_CONSUMABLE, ITEM_SUBCLASS_BANDAGE
    // Scan inventory
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (!item)
            continue;

        ItemTemplate const* proto = item->GetTemplate();
        if (!proto)
            continue;

        // Check item name contains "Bandage"
        std::string itemName = proto->GetName(DEFAULT_LOCALE);
        if (itemName.find("Bandage") != std::string::npos)
            return item;
    }

    // Check bags
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        if (Bag* pBag = bot->GetBagByPos(bag))
        {
            for (uint32 slot = 0; slot < pBag->GetBagSize(); ++slot)
            {
                Item* item = pBag->GetItemByPos(slot);
                if (!item)
                    continue;

                ItemTemplate const* proto = item->GetTemplate();
                if (!proto)
                    continue;

                std::string itemName = proto->GetName(DEFAULT_LOCALE);
                if (itemName.find("Bandage") != std::string::npos)
                    return item;
            }
        }
    }

    return nullptr;
}

bool RestStrategy::EatFood(BotAI* ai, Item* food)
{
    if (!ai || !ai->GetBot() || !food)
        return false;

    Player* bot = ai->GetBot();

    // Use the food item
    SpellCastTargets targets;
    targets.SetUnitTarget(bot);
    bot->CastItemUseSpell(food, targets, ObjectGuid::Empty, nullptr);

    _foodConsumed++;
    return true;
}

bool RestStrategy::DrinkWater(BotAI* ai, Item* drink)
{
    if (!ai || !ai->GetBot() || !drink)
        return false;

    Player* bot = ai->GetBot();

    // Use the drink item
    SpellCastTargets targets;
    targets.SetUnitTarget(bot);
    bot->CastItemUseSpell(drink, targets, ObjectGuid::Empty, nullptr);

    _drinkConsumed++;
    return true;
}

bool RestStrategy::UseBandage(BotAI* ai, Item* bandage)
{
    if (!ai || !ai->GetBot() || !bandage)
        return false;

    Player* bot = ai->GetBot();

    // Use bandage on self
    SpellCastTargets targets;
    targets.SetUnitTarget(bot);
    bot->CastItemUseSpell(bandage, targets, ObjectGuid::Empty, nullptr);

    _bandagesUsed++;
    return true;
}

bool RestStrategy::IsResting(BotAI* ai) const
{
    return _isEating || _isDrinking;
}

} // namespace Playerbot
