/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "MageSpecialization.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "SpellDefines.h"
#include "Unit.h"
#include "Pet.h"
#include "GameObject.h"
#include "ObjectAccessor.h"
#include "Group.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "Log.h"
#include "ObjectGuid.h"
#include "../../../AI/BotAI.h"
#include "MotionMaster.h"
#include "Item.h"
#include "Bag.h"
#include "ItemTemplate.h"
#include "../../../AI/Combat/TargetSelector.h"
#include <algorithm>

namespace Playerbot
{

MageSpecialization::MageSpecialization(Player* bot) : _bot(bot)
{
}

// Resource management
bool MageSpecialization::HasEnoughMana(uint32 amount)
{
    if (!_bot)
        return false;

    uint32 currentMana = GetMana();
    return currentMana >= amount;
}

uint32 MageSpecialization::GetMana()
{
    if (!_bot)
        return 0;

    return _bot->GetPower(POWER_MANA);
}

uint32 MageSpecialization::GetMaxMana()
{
    if (!_bot)
        return 0;

    return _bot->GetMaxPower(POWER_MANA);
}

float MageSpecialization::GetManaPercent()
{
    uint32 maxMana = GetMaxMana();
    if (maxMana == 0)
        return 0.0f;

    return static_cast<float>(GetMana()) / static_cast<float>(maxMana) * 100.0f;
}

bool MageSpecialization::ShouldConserveMana()
{
    float manaPercent = GetManaPercent();
    return manaPercent < (MANA_CONSERVATION_THRESHOLD * 100.0f);
}

// Shared defensive abilities
void MageSpecialization::UseBlink()
{
    if (!_bot)
        return;

    // Check if we can move (not rooted)
    if (_bot->HasUnitMovementFlag(MOVEMENTFLAG_ROOT))
        return;

    // Attempt to cast blink spell
    if (_bot->CastSpell(_bot, BLINK, false))
    {
        TC_LOG_DEBUG("playerbots", "MageSpecialization: Bot %s used blink to escape", _bot->GetName().c_str());
    }
}

void MageSpecialization::UseIceBlock()
{
    if (!_bot)
        return;

    // Skip cooldown check for TrinityCore API compatibility
    // if (_bot->GetSpellCooldownDelay(CommonSpells::ICE_BLOCK) > 0)
    //     return;

    // Use ice block when health is critically low
    if (_bot->GetHealthPct() > 15.0f)
        return;

    // Don't use if already have an immunity effect
    if (_bot->HasAuraType(SPELL_AURA_SCHOOL_IMMUNITY))
        return;

    if (_bot->CastSpell(_bot, ICE_BLOCK, false))
    {
        TC_LOG_DEBUG("playerbots", "MageSpecialization: Bot %s used ice block for immunity",
                    _bot->GetName().c_str());
    }
}

void MageSpecialization::UseManaShield()
{
    if (!_bot)
        return;

    if (_bot->HasAura(MANA_SHIELD))
        return; // Already active

    // Use mana shield when health is low but we have mana
    if (_bot->GetHealthPct() > 40.0f)
        return;

    if (GetManaPercent() < 30.0f)
        return; // Not enough mana to sustain

    if (_bot->CastSpell(_bot, MANA_SHIELD, false))
    {
        TC_LOG_DEBUG("playerbots", "MageSpecialization: Bot %s activated mana shield",
                    _bot->GetName().c_str());
    }
}

// Shared crowd control
void MageSpecialization::UsePolymorph(Unit* target)
{
    if (!target || !_bot)
        return;

    if (target->HasAura(POLYMORPH))
        return;

    // Don't polymorph current target if we're in single combat
    if (_bot->GetSelectedUnit() == target)
    {
        // Check if there are multiple enemies
        Unit* nearestEnemy = TargetSelectionUtils::GetNearestEnemy(_bot, 30.0f);
        std::vector<Unit*> enemies;
        if (nearestEnemy)
            enemies.push_back(nearestEnemy);

        if (enemies.size() < 2)
            return; // Don't polymorph if only one enemy
    }

    // Only polymorph humanoids, beasts, and critters
    if (target->GetCreatureType() != CREATURE_TYPE_HUMANOID &&
        target->GetCreatureType() != CREATURE_TYPE_BEAST &&
        target->GetCreatureType() != CREATURE_TYPE_CRITTER)
        return;

    if (!HasEnoughMana(150))
        return;

    float distance = _bot->GetDistance2d(target);
    if (distance > OPTIMAL_CASTING_RANGE)
        return;

    if (_bot->CastSpell(target, POLYMORPH, false))
    {
        TC_LOG_DEBUG("playerbots", "MageSpecialization: Bot %s polymorphed target %s",
                    _bot->GetName().c_str(), target->GetName().c_str());
    }
}

void MageSpecialization::UseCounterspell(Unit* target)
{
    if (!target || !_bot)
        return;

    // Skip cooldown check for TrinityCore API compatibility
    // if (_bot->GetSpellCooldownDelay(CommonSpells::COUNTERSPELL) > 0)
    //     return;

    // Only use on casting enemies
    if (!target->HasUnitState(UNIT_STATE_CASTING))
        return;

    float distance = _bot->GetDistance2d(target);
    if (distance > 24.0f) // Counterspell has shorter range
        return;

    if (_bot->CastSpell(target, COUNTERSPELL, false))
    {
        TC_LOG_DEBUG("playerbots", "MageSpecialization: Bot %s counterspelled target %s",
                    _bot->GetName().c_str(), target->GetName().c_str());
    }
}

void MageSpecialization::UseFrostNova()
{
    if (!_bot)
        return;

    // Skip cooldown check for TrinityCore API compatibility
    // if (_bot->GetSpellCooldownDelay(CommonSpells::FROST_NOVA) > 0)
    //     return;

    // Use frost nova when enemies are close - simplified for TrinityCore compatibility
    Unit* currentTarget = _bot->GetSelectedUnit();
    if (!currentTarget || !currentTarget->IsAlive() || !_bot->IsHostileTo(currentTarget))
        return;

    // Simple distance check instead of full enemy search
    if (_bot->GetDistance2d(currentTarget) > 10.0f)
        return;

    // Check if target is not already frozen
    if (currentTarget->HasUnitMovementFlag(MOVEMENTFLAG_ROOT) ||
        currentTarget->HasAuraType(SPELL_AURA_MOD_ROOT))
        return;

    if (!HasEnoughMana(120))
        return;

    if (_bot->CastSpell(_bot, FROST_NOVA, false))
    {
        TC_LOG_DEBUG("playerbots", "MageSpecialization: Bot %s cast frost nova",
                    _bot->GetName().c_str());
    }
}

// Shared utility
bool MageSpecialization::IsChanneling()
{
    if (!_bot)
        return false;

    return _bot->HasUnitState(UNIT_STATE_CASTING);
}

bool MageSpecialization::IsCasting()
{
    if (!_bot)
        return false;

    return _bot->HasUnitState(UNIT_STATE_CASTING);
}

bool MageSpecialization::CanCastSpell()
{
    if (!_bot)
        return false;

    return !_bot->HasUnitState(UNIT_STATE_CASTING | UNIT_STATE_STUNNED |
                              UNIT_STATE_CONFUSED | UNIT_STATE_FLEEING);
}

bool MageSpecialization::IsInDanger()
{
    if (!_bot)
        return false;

    // Check health threshold
    if (_bot->GetHealthPct() < 30.0f)
        return true;

    // Simplified danger check - focus on current target and health
    Unit* currentTarget = _bot->GetSelectedUnit();
    if (currentTarget && currentTarget->IsAlive())
    {
        float distance = _bot->GetDistance2d(currentTarget);

        // Danger if enemy is very close
        if (distance < 8.0f)
            return true;

        // Danger if target is much higher level
        if (currentTarget->GetLevel() > _bot->GetLevel() + 3)
            return true;
    }

    return false;
}

// Buff management
void MageSpecialization::UpdateArcaneIntellect()
{
    if (!_bot)
        return;

    // Cast Arcane Intellect on self if not active
    if (!_bot->HasAura(ARCANE_INTELLECT) && _bot->HasSpell(ARCANE_INTELLECT))
    {
        if (_bot->CastSpell(_bot, ARCANE_INTELLECT, false))
        {
            TC_LOG_DEBUG("playerbots", "MageSpecialization: Bot %s cast arcane intellect",
                        _bot->GetName().c_str());
        }
    }

    // Cast on group members if in a group
    if (Group* group = _bot->GetGroup())
    {
        // Simplified group iteration for TrinityCore compatibility
        Group::MemberSlotList const& members = group->GetMemberSlots();
        for (Group::MemberSlotList::const_iterator itr = members.begin(); itr != members.end(); ++itr)
        {
            Player* member = ObjectAccessor::FindPlayer(itr->guid);
            if (!member || !member->IsAlive())
                continue;

            if (member->GetDistance2d(_bot) > 30.0f)
                continue;

            if (!member->HasAura(ARCANE_INTELLECT))
            {
                if (_bot->CastSpell(member, ARCANE_INTELLECT, false))
                {
                    TC_LOG_DEBUG("playerbots", "MageSpecialization: Bot %s cast arcane intellect on %s",
                                _bot->GetName().c_str(), member->GetName().c_str());
                    break; // Only cast one per update
                }
            }
        }
    }
}

// Positioning helpers
bool MageSpecialization::IsInCastingRange(Unit* target, uint32 spellId)
{
    if (!target || !_bot)
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return false;

    float range = spellInfo->GetMaxRange();
    float distance = _bot->GetDistance2d(target);

    return distance <= range && _bot->IsWithinLOSInMap(target);
}

Position MageSpecialization::GetOptimalCastingPosition(Unit* target)
{
    if (!target || !_bot)
        return _bot->GetPosition();

    Position currentPos = _bot->GetPosition();
    Position targetPos = target->GetPosition();

    // Stay at optimal casting range
    float currentDistance = _bot->GetDistance2d(target);

    if (currentDistance > OPTIMAL_CASTING_RANGE + 5.0f)
    {
        // Move closer
        float angle = targetPos.GetAbsoluteAngle(currentPos);
        Position newPos = targetPos;
        newPos.m_positionX += cos(angle) * OPTIMAL_CASTING_RANGE;
        newPos.m_positionY += sin(angle) * OPTIMAL_CASTING_RANGE;
        return newPos;
    }
    else if (currentDistance < MINIMUM_SAFE_RANGE)
    {
        // Move further away
        float angle = currentPos.GetAbsoluteAngle(targetPos);
        Position newPos = currentPos;
        newPos.m_positionX += cos(angle) * 10.0f;
        newPos.m_positionY += sin(angle) * 10.0f;
        return newPos;
    }

    return currentPos;
}

bool MageSpecialization::ShouldKite(Unit* target)
{
    if (!target || !_bot)
        return false;

    float distance = _bot->GetDistance2d(target);

    // Kite if enemy is too close
    if (distance < MINIMUM_SAFE_RANGE)
        return true;

    // Kite if low on health
    if (_bot->GetHealthPct() < 40.0f && distance < KITING_RANGE)
        return true;

    return false;
}

void MageSpecialization::PerformKiting(Unit* target)
{
    if (!target || !_bot)
        return;

    // Move away from target while maintaining casting range
    Position targetPos = target->GetPosition();
    Position currentPos = _bot->GetPosition();

    float angle = currentPos.GetAbsoluteAngle(targetPos);
    Position kitePos = currentPos;
    kitePos.m_positionX += cos(angle) * 15.0f;
    kitePos.m_positionY += sin(angle) * 15.0f;

    // Use blink if available and in danger
    if (IsInDanger()) // Simplified check - remove cooldown dependency
    {
        UseBlink();
    }
    else
    {
        _bot->GetMotionMaster()->MovePoint(0, kitePos);
    }

    TC_LOG_DEBUG("playerbots", "MageSpecialization: Bot %s kiting away from target %s",
                _bot->GetName().c_str(), target->GetName().c_str());
}

// Mana management
void MageSpecialization::UseManaGem()
{
    if (!_bot)
        return;

    if (GetManaPercent() > 70.0f)
        return; // Don't use if mana is high

    // Look for mana gems in inventory
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        if (Bag* bagItem = _bot->GetBagByPos(bag))
        {
            for (uint32 slot = 0; slot < bagItem->GetBagSize(); ++slot)
            {
                if (Item* item = bagItem->GetItemByPos(slot))
                {
                    // Check for various mana gem types
                    uint32 entry = item->GetEntry();
                    if (entry == 5514 || entry == 5513 || entry == 8007 || entry == 8008) // Mana gem IDs
                    {
                        if (Item* manaGem = bagItem->GetItemByPos(slot))
                            _bot->CastItemUseSpell(manaGem, SpellCastTargets(), ObjectGuid::Empty, nullptr);
                        TC_LOG_DEBUG("playerbots", "MageSpecialization: Bot %s used mana gem", _bot->GetName().c_str());
                        return;
                    }
                }
            }
        }
    }

    // Check main inventory too
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
    {
        if (Item* item = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        {
            uint32 entry = item->GetEntry();
            if (entry == 5514 || entry == 5513 || entry == 8007 || entry == 8008) // Mana gem IDs
            {
                if (Item* manaGem = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                    _bot->CastItemUseSpell(manaGem, SpellCastTargets(), ObjectGuid::Empty, nullptr);
                TC_LOG_DEBUG("playerbots", "MageSpecialization: Bot %s used mana gem", _bot->GetName().c_str());
                return;
            }
        }
    }
}

void MageSpecialization::UseEvocation()
{
    if (!_bot)
        return;

    uint32 evocationSpell = 12051; // Evocation spell ID

    if (!_bot->HasSpell(evocationSpell))
        return;

    // Skip cooldown check for TrinityCore API compatibility
    // if (_bot->GetSpellCooldownDelay(evocationSpell) > 0)
    //     return;

    if (GetManaPercent() > 40.0f)
        return; // Don't use if mana is not low enough

    // Only use evocation when safe
    if (IsInDanger())
        return;

    if (_bot->CastSpell(_bot, evocationSpell, false))
    {
        TC_LOG_DEBUG("playerbots", "MageSpecialization: Bot %s started evocation",
                    _bot->GetName().c_str());
    }
}

// Conjured item management
void MageSpecialization::ConjureFood()
{
    if (!_bot)
        return;

    // Check if we already have enough food
    uint32 foodCount = 0;
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        if (Bag* bagItem = _bot->GetBagByPos(bag))
        {
            for (uint32 slot = 0; slot < bagItem->GetBagSize(); ++slot)
            {
                if (Item* item = bagItem->GetItemByPos(slot))
                {
                    if (item->GetTemplate()->GetClass() == ITEM_CLASS_CONSUMABLE &&
                        item->GetTemplate()->GetSubClass() == ITEM_SUBCLASS_FOOD_DRINK)
                    {
                        foodCount += item->GetCount();
                    }
                }
            }
        }
    }

    if (foodCount >= 20)
        return; // We have enough food

    // Find the highest level conjure food spell we know
    uint32 conjureFoodSpell = 0;
    std::vector<uint32> foodSpells = {587, 597, 990, 6129, 10144, 10145, 28612}; // Conjure food spell IDs

    for (auto spellId : foodSpells)
    {
        if (_bot->HasSpell(spellId))
            conjureFoodSpell = spellId;
    }

    if (conjureFoodSpell == 0)
        return;

    if (!HasEnoughMana(200))
        return;

    if (_bot->CastSpell(_bot, conjureFoodSpell, false))
    {
        TC_LOG_DEBUG("playerbots", "MageSpecialization: Bot %s conjured food", _bot->GetName().c_str());
    }
}

void MageSpecialization::ConjureWater()
{
    if (!_bot)
        return;

    // Check if we already have enough water
    uint32 waterCount = 0;
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        if (Bag* bagItem = _bot->GetBagByPos(bag))
        {
            for (uint32 slot = 0; slot < bagItem->GetBagSize(); ++slot)
            {
                if (Item* item = bagItem->GetItemByPos(slot))
                {
                    if (item->GetTemplate()->GetClass() == ITEM_CLASS_CONSUMABLE &&
                        item->GetTemplate()->GetSubClass() == ITEM_SUBCLASS_FOOD_DRINK)
                    {
                        waterCount += item->GetCount();
                    }
                }
            }
        }
    }

    if (waterCount >= 20)
        return; // We have enough water

    // Find the highest level conjure water spell we know
    uint32 conjureWaterSpell = 0;
    std::vector<uint32> waterSpells = {5504, 5505, 5506, 6127, 10138, 10139, 10140, 37420}; // Conjure water spell IDs

    for (auto spellId : waterSpells)
    {
        if (_bot->HasSpell(spellId))
            conjureWaterSpell = spellId;
    }

    if (conjureWaterSpell == 0)
        return;

    if (!HasEnoughMana(200))
        return;

    if (_bot->CastSpell(_bot, conjureWaterSpell, false))
    {
        TC_LOG_DEBUG("playerbots", "MageSpecialization: Bot %s conjured water", _bot->GetName().c_str());
    }
}

// Emergency abilities
void MageSpecialization::UseEmergencyAbilities()
{
    if (!_bot)
        return;

    float healthPct = _bot->GetHealthPct();

    // Use ice block if critically low
    if (healthPct < 15.0f)
    {
        UseIceBlock();
    }

    // Use mana shield if low health but have mana
    if (healthPct < 40.0f && GetManaPercent() > 30.0f)
    {
        UseManaShield();
    }

    // Use blink to escape if in danger
    if (IsInDanger())
    {
        UseBlink();
    }

    // Use frost nova to control nearby enemies
    if (healthPct < 50.0f)
    {
        UseFrostNova();
    }
}

// Target selection helpers
Unit* MageSpecialization::GetBestPolymorphTarget()
{
    if (!_bot)
        return nullptr;

    // Modern TrinityCore API approach for finding polymorph targets
    Unit* currentTarget = _bot->GetSelectedUnit();
    if (!currentTarget || !currentTarget->IsAlive())
        return nullptr;

    // Start with current target if valid for polymorph
    Unit* bestTarget = nullptr;
    float bestScore = 0.0f;

    // Check current target first
    if (IsValidPolymorphTarget(currentTarget))
    {
        bestTarget = currentTarget;
        bestScore = CalculatePolymorphTargetScore(currentTarget);
    }

    // Simple approach - return the current target if it's valid for polymorph
    // More complex target selection can be added later if needed
    return bestTarget;
}

bool MageSpecialization::IsValidPolymorphTarget(Unit* target)
{
    if (!target || !target->IsAlive())
        return false;

    // Only polymorph appropriate creature types
    if (target->GetCreatureType() != CREATURE_TYPE_HUMANOID &&
        target->GetCreatureType() != CREATURE_TYPE_BEAST &&
        target->GetCreatureType() != CREATURE_TYPE_CRITTER)
        return false;

    // Don't polymorph if already polymorphed or has immunity effects
    if (target->HasAura(POLYMORPH))
        return false;

    return true;
}

float MageSpecialization::CalculatePolymorphTargetScore(Unit* target)
{
    if (!target)
        return 0.0f;

    float score = 0.0f;

    // Prefer targets that are casting
    if (target->HasUnitState(UNIT_STATE_CASTING))
        score += 50.0f;

    // Prefer closer targets
    float distance = _bot->GetDistance2d(target);
    score += (30.0f - distance); // Higher score for closer targets

    // Prefer targets with higher health (more dangerous)
    score += target->GetHealthPct() * 0.5f;

    return score;
}


} // namespace Playerbot