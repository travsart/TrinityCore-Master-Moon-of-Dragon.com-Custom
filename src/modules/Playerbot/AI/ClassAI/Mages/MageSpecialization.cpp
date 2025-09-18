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
#include "Unit.h"
#include "Pet.h"
#include "GameObject.h"
#include "ObjectAccessor.h"
#include "Group.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "Log.h"
#include "PlayerbotAI.h"
#include "MotionMaster.h"
#include "Item.h"
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

    if (_bot->HasSpellCooldown(BLINK))
        return;

    // Use blink when in danger and need to escape
    if (!IsInDanger())
        return;

    // Check if we can move (not rooted)
    if (_bot->HasUnitMovementFlag(MOVEMENTFLAG_ROOT))
        return;

    if (_bot->CastSpell(_bot, BLINK, false))
    {
        TC_LOG_DEBUG("playerbots", "MageSpecialization: Bot {} used blink to escape",
                    _bot->GetName());
    }
}

void MageSpecialization::UseIceBlock()
{
    if (!_bot)
        return;

    if (_bot->HasSpellCooldown(ICE_BLOCK))
        return;

    // Use ice block when health is critically low
    if (_bot->GetHealthPct() > 15.0f)
        return;

    // Don't use if already have an immunity effect
    if (_bot->HasAuraType(SPELL_AURA_SCHOOL_IMMUNITY))
        return;

    if (_bot->CastSpell(_bot, ICE_BLOCK, false))
    {
        TC_LOG_DEBUG("playerbots", "MageSpecialization: Bot {} used ice block for immunity",
                    _bot->GetName());
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
        TC_LOG_DEBUG("playerbots", "MageSpecialization: Bot {} activated mana shield",
                    _bot->GetName());
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
        std::list<Unit*> enemies;
        Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(_bot, _bot, 30.0f);
        Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(_bot, enemies, u_check);
        Cell::VisitAllObjects(_bot, searcher, 30.0f);

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
        TC_LOG_DEBUG("playerbots", "MageSpecialization: Bot {} polymorphed target {}",
                    _bot->GetName(), target->GetName());
    }
}

void MageSpecialization::UseCounterspell(Unit* target)
{
    if (!target || !_bot)
        return;

    if (_bot->HasSpellCooldown(COUNTERSPELL))
        return;

    // Only use on casting enemies
    if (!target->HasUnitState(UNIT_STATE_CASTING))
        return;

    float distance = _bot->GetDistance2d(target);
    if (distance > 24.0f) // Counterspell has shorter range
        return;

    if (_bot->CastSpell(target, COUNTERSPELL, false))
    {
        TC_LOG_DEBUG("playerbots", "MageSpecialization: Bot {} counterspelled target {}",
                    _bot->GetName(), target->GetName());
    }
}

void MageSpecialization::UseFrostNova()
{
    if (!_bot)
        return;

    if (_bot->HasSpellCooldown(FROST_NOVA))
        return;

    // Use frost nova when enemies are close
    std::list<Unit*> nearbyEnemies;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(_bot, _bot, 10.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(_bot, nearbyEnemies, u_check);
    Cell::VisitAllObjects(_bot, searcher, 10.0f);

    if (nearbyEnemies.empty())
        return;

    // Check if any nearby enemies are not frozen
    bool shouldCast = false;
    for (Unit* enemy : nearbyEnemies)
    {
        if (enemy && !enemy->HasUnitMovementFlag(MOVEMENTFLAG_ROOT) &&
            !enemy->HasAuraType(SPELL_AURA_MOD_ROOT))
        {
            shouldCast = true;
            break;
        }
    }

    if (!shouldCast)
        return;

    if (!HasEnoughMana(120))
        return;

    if (_bot->CastSpell(_bot, FROST_NOVA, false))
    {
        TC_LOG_DEBUG("playerbots", "MageSpecialization: Bot {} cast frost nova on {} enemies",
                    _bot->GetName(), nearbyEnemies.size());
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
                              UNIT_STATE_CONFUSED | UNIT_STATE_FLEEING |
                              UNIT_STATE_SILENCED);
}

bool MageSpecialization::IsInDanger()
{
    if (!_bot)
        return false;

    // Check health threshold
    if (_bot->GetHealthPct() < 30.0f)
        return true;

    // Check for nearby enemies
    std::list<Unit*> nearbyEnemies;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(_bot, _bot, 15.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(_bot, nearbyEnemies, u_check);
    Cell::VisitAllObjects(_bot, searcher, 15.0f);

    if (nearbyEnemies.size() >= 3)
        return true;

    // Check for strong enemies
    for (Unit* enemy : nearbyEnemies)
    {
        if (enemy && enemy->GetLevel() > _bot->GetLevel() + 2)
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
            TC_LOG_DEBUG("playerbots", "MageSpecialization: Bot {} cast arcane intellect",
                        _bot->GetName());
        }
    }

    // Cast on group members if in a group
    if (Group* group = _bot->GetGroup())
    {
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (!member || !member->IsAlive())
                continue;

            if (member->GetDistance2d(_bot) > 30.0f)
                continue;

            if (!member->HasAura(ARCANE_INTELLECT))
            {
                if (_bot->CastSpell(member, ARCANE_INTELLECT, false))
                {
                    TC_LOG_DEBUG("playerbots", "MageSpecialization: Bot {} cast arcane intellect on {}",
                                _bot->GetName(), member->GetName());
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
        float angle = targetPos.GetAngle(currentPos);
        Position newPos = targetPos;
        newPos.m_positionX += cos(angle) * OPTIMAL_CASTING_RANGE;
        newPos.m_positionY += sin(angle) * OPTIMAL_CASTING_RANGE;
        return newPos;
    }
    else if (currentDistance < MINIMUM_SAFE_RANGE)
    {
        // Move further away
        float angle = currentPos.GetAngle(targetPos);
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

    float angle = currentPos.GetAngle(targetPos);
    Position kitePos = currentPos;
    kitePos.m_positionX += cos(angle) * 15.0f;
    kitePos.m_positionY += sin(angle) * 15.0f;

    // Use blink if available and in danger
    if (IsInDanger() && !_bot->HasSpellCooldown(BLINK))
    {
        UseBlink();
    }
    else
    {
        _bot->GetMotionMaster()->MovePoint(0, kitePos);
    }

    TC_LOG_DEBUG("playerbots", "MageSpecialization: Bot {} kiting away from target {}",
                _bot->GetName(), target->GetName());
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
                        _bot->UseItem(bag, slot);
                        TC_LOG_DEBUG("playerbots", "MageSpecialization: Bot {} used mana gem",
                                    _bot->GetName());
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
                _bot->UseItem(INVENTORY_SLOT_BAG_0, slot);
                TC_LOG_DEBUG("playerbots", "MageSpecialization: Bot {} used mana gem",
                            _bot->GetName());
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

    if (_bot->HasSpellCooldown(evocationSpell))
        return;

    if (GetManaPercent() > 40.0f)
        return; // Don't use if mana is not low enough

    // Only use evocation when safe
    if (IsInDanger())
        return;

    if (_bot->CastSpell(_bot, evocationSpell, false))
    {
        TC_LOG_DEBUG("playerbots", "MageSpecialization: Bot {} started evocation",
                    _bot->GetName());
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
                    if (item->GetTemplate()->Class == ITEM_CLASS_CONSUMABLE &&
                        item->GetTemplate()->SubClass == ITEM_SUBCLASS_FOOD)
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
        TC_LOG_DEBUG("playerbots", "MageSpecialization: Bot {} conjured food",
                    _bot->GetName());
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
                    if (item->GetTemplate()->Class == ITEM_CLASS_CONSUMABLE &&
                        item->GetTemplate()->SubClass == ITEM_SUBCLASS_DRINK)
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
        TC_LOG_DEBUG("playerbots", "MageSpecialization: Bot {} conjured water",
                    _bot->GetName());
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

    std::list<Unit*> enemies;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(_bot, _bot, 30.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(_bot, enemies, u_check);
    Cell::VisitAllObjects(_bot, searcher, 30.0f);

    Unit* bestTarget = nullptr;
    float bestScore = 0.0f;

    for (Unit* enemy : enemies)
    {
        if (!enemy || !enemy->IsAlive())
            continue;

        // Skip current target
        if (enemy == _bot->GetSelectedUnit())
            continue;

        // Only polymorph appropriate creature types
        if (enemy->GetCreatureType() != CREATURE_TYPE_HUMANOID &&
            enemy->GetCreatureType() != CREATURE_TYPE_BEAST &&
            enemy->GetCreatureType() != CREATURE_TYPE_CRITTER)
            continue;

        float score = 0.0f;

        // Prefer targets not attacking us
        if (enemy->GetVictim() != _bot)
            score += 10.0f;

        // Prefer closer targets
        float distance = _bot->GetDistance2d(enemy);
        score += (30.0f - distance);

        // Prefer higher health targets (polymorph lasts longer value)
        score += enemy->GetHealthPct() * 0.1f;

        if (score > bestScore)
        {
            bestScore = score;
            bestTarget = enemy;
        }
    }

    return bestTarget;
}

} // namespace Playerbot