/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "WarlockSpecialization.h"
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

// Pet management methods
void WarlockSpecialization::SummonPet(WarlockPet petType)
{
    if (!_bot)
        return;

    // Don't summon if we already have the right pet
    if (_currentPet == petType && IsPetAlive())
        return;

    uint32 summonSpell = 0;
    switch (petType)
    {
        case WarlockPet::IMP:
            summonSpell = SUMMON_IMP;
            break;
        case WarlockPet::VOIDWALKER:
            summonSpell = SUMMON_VOIDWALKER;
            break;
        case WarlockPet::SUCCUBUS:
            summonSpell = SUMMON_SUCCUBUS;
            break;
        case WarlockPet::FELHUNTER:
            summonSpell = SUMMON_FELHUNTER;
            break;
        case WarlockPet::FELGUARD:
            summonSpell = SUMMON_FELGUARD;
            break;
        default:
            return;
    }

    // Check if spell is known
    if (!_bot->HasSpell(summonSpell))
        return;

    // Check if we have soul shards for higher level pets
    if ((petType == WarlockPet::VOIDWALKER || petType == WarlockPet::SUCCUBUS ||
         petType == WarlockPet::FELHUNTER || petType == WarlockPet::FELGUARD) &&
        !HasSoulShardsAvailable(1))
        return;

    // Dismiss current pet if different
    if (_petUnit && _currentPet != petType)
    {
        _petUnit->ToTempSummon()->UnSummon();
        _petUnit = nullptr;
    }

    // Cast summon spell
    if (_bot->CastSpell(_bot, summonSpell, false))
    {
        _currentPet = petType;
        _lastPetCommand = getMSTime();

        // Use soul shard if required
        if (petType != WarlockPet::IMP)
        {
            UseSoulShard(summonSpell);
        }

        TC_LOG_DEBUG("playerbots", "WarlockSpecialization: Bot {} summoned {} pet",
                    _bot->GetName(), static_cast<uint32>(petType));
    }
}

void WarlockSpecialization::PetAttackTarget(Unit* target)
{
    if (!target || !_bot)
        return;

    Pet* pet = _bot->GetPet();
    if (!pet || !pet->IsAlive())
        return;

    uint32 now = getMSTime();
    if (now - _lastPetCommand < 1000) // 1 second cooldown between commands
        return;

    if (pet->GetVictim() == target)
        return; // Already attacking this target

    // Command pet to attack
    _bot->GetPetMgr()->GetPetByEntry(pet->GetEntry())->GetCharmInfo()->SetIsCommandAttack(true);
    _bot->GetPetMgr()->GetPetByEntry(pet->GetEntry())->GetCharmInfo()->SetCommandState(COMMAND_ATTACK);

    _petUnit = pet;
    _lastPetCommand = now;

    TC_LOG_DEBUG("playerbots", "WarlockSpecialization: Bot {} commanded pet to attack {}",
                _bot->GetName(), target->GetName());
}

void WarlockSpecialization::PetFollow()
{
    if (!_bot)
        return;

    Pet* pet = _bot->GetPet();
    if (!pet || !pet->IsAlive())
        return;

    uint32 now = getMSTime();
    if (now - _lastPetCommand < 1000) // 1 second cooldown
        return;

    // Command pet to follow
    pet->GetCharmInfo()->SetIsCommandAttack(false);
    pet->GetCharmInfo()->SetCommandState(COMMAND_FOLLOW);

    _lastPetCommand = now;
    _petBehavior = PetBehavior::DEFENSIVE;

    TC_LOG_DEBUG("playerbots", "WarlockSpecialization: Bot {} commanded pet to follow",
                _bot->GetName());
}

bool WarlockSpecialization::IsPetAlive()
{
    if (!_bot)
        return false;

    Pet* pet = _bot->GetPet();
    if (!pet)
        return false;

    _petUnit = pet;
    return pet->IsAlive();
}

// DoT management methods
bool WarlockSpecialization::IsDoTActive(Unit* target, uint32 spellId)
{
    if (!target)
        return false;

    return target->HasAura(spellId, _bot->GetGUID());
}

uint32 WarlockSpecialization::GetDoTRemainingTime(Unit* target, uint32 spellId)
{
    if (!target)
        return 0;

    if (Aura* aura = target->GetAura(spellId, _bot->GetGUID()))
    {
        return aura->GetDuration();
    }

    return 0;
}

void WarlockSpecialization::CastCurse(Unit* target, uint32 curseId)
{
    if (!target || !_bot)
        return;

    // Don't overwrite stronger curses
    if (target->HasAura(CURSE_OF_ELEMENTS) && curseId != CURSE_OF_ELEMENTS)
        return;
    if (target->HasAura(CURSE_OF_SHADOW) && curseId != CURSE_OF_SHADOW)
        return;

    // Check if target already has this curse from us
    if (target->HasAura(curseId, _bot->GetGUID()))
        return;

    if (_bot->CastSpell(target, curseId, false))
    {
        TC_LOG_DEBUG("playerbots", "WarlockSpecialization: Bot {} cast curse {} on target {}",
                    _bot->GetName(), curseId, target->GetName());
    }
}

// Soul shard management methods
bool WarlockSpecialization::HasSoulShardsAvailable(uint32 required)
{
    if (!_bot)
        return false;

    // Count soul shards in inventory
    uint32 shardCount = 0;
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        if (Bag* bagItem = _bot->GetBagByPos(bag))
        {
            for (uint32 slot = 0; slot < bagItem->GetBagSize(); ++slot)
            {
                if (Item* item = bagItem->GetItemByPos(slot))
                {
                    if (item->GetEntry() == 6265) // Soul Shard item ID
                    {
                        shardCount += item->GetCount();
                    }
                }
            }
        }
    }

    // Also check main inventory
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
    {
        if (Item* item = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        {
            if (item->GetEntry() == 6265) // Soul Shard item ID
            {
                shardCount += item->GetCount();
            }
        }
    }

    _soulShards.count = shardCount;
    return shardCount >= required;
}

void WarlockSpecialization::UseSoulShard(uint32 spellId)
{
    if (!_bot)
        return;

    // Find and consume a soul shard
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        if (Bag* bagItem = _bot->GetBagByPos(bag))
        {
            for (uint32 slot = 0; slot < bagItem->GetBagSize(); ++slot)
            {
                if (Item* item = bagItem->GetItemByPos(slot))
                {
                    if (item->GetEntry() == 6265) // Soul Shard item ID
                    {
                        _bot->DestroyItem(bag, slot, true);
                        _soulShards.count--;
                        _soulShards.lastUsed = getMSTime();

                        TC_LOG_DEBUG("playerbots", "WarlockSpecialization: Bot {} used soul shard for spell {} (remaining: {})",
                                    _bot->GetName(), spellId, _soulShards.count);
                        return;
                    }
                }
            }
        }
    }

    // Check main inventory
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
    {
        if (Item* item = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        {
            if (item->GetEntry() == 6265) // Soul Shard item ID
            {
                _bot->DestroyItem(INVENTORY_SLOT_BAG_0, slot, true);
                _soulShards.count--;
                _soulShards.lastUsed = getMSTime();

                TC_LOG_DEBUG("playerbots", "WarlockSpecialization: Bot {} used soul shard for spell {} (remaining: {})",
                            _bot->GetName(), spellId, _soulShards.count);
                return;
            }
        }
    }
}

// Resource management helpers
uint32 WarlockSpecialization::GetMana()
{
    if (!_bot)
        return 0;

    return _bot->GetPower(POWER_MANA);
}

uint32 WarlockSpecialization::GetMaxMana()
{
    if (!_bot)
        return 0;

    return _bot->GetMaxPower(POWER_MANA);
}

float WarlockSpecialization::GetManaPercent()
{
    uint32 maxMana = GetMaxMana();
    if (maxMana == 0)
        return 0.0f;

    return static_cast<float>(GetMana()) / static_cast<float>(maxMana) * 100.0f;
}

bool WarlockSpecialization::HasEnoughMana(uint32 amount)
{
    return GetMana() >= amount;
}

void WarlockSpecialization::CastLifeTap()
{
    if (!_bot)
        return;

    if (_bot->GetHealthPct() < 30.0f)
        return; // Don't life tap when low on health

    if (_bot->HasSpellCooldown(LIFE_TAP))
        return;

    if (_bot->CastSpell(_bot, LIFE_TAP, false))
    {
        TC_LOG_DEBUG("playerbots", "WarlockSpecialization: Bot {} used life tap",
                    _bot->GetName());
    }
}

// Buff management
void WarlockSpecialization::UpdateArmor()
{
    if (!_bot)
        return;

    // Prioritize Fel Armor > Demon Armor > Demon Skin
    if (_bot->HasSpell(FEL_ARMOR) && !_bot->HasAura(FEL_ARMOR))
    {
        if (_bot->CastSpell(_bot, FEL_ARMOR, false))
        {
            TC_LOG_DEBUG("playerbots", "WarlockSpecialization: Bot {} cast fel armor",
                        _bot->GetName());
        }
    }
    else if (_bot->HasSpell(DEMON_ARMOR) && !_bot->HasAura(DEMON_ARMOR) && !_bot->HasAura(FEL_ARMOR))
    {
        if (_bot->CastSpell(_bot, DEMON_ARMOR, false))
        {
            TC_LOG_DEBUG("playerbots", "WarlockSpecialization: Bot {} cast demon armor",
                        _bot->GetName());
        }
    }
    else if (_bot->HasSpell(DEMON_SKIN) && !_bot->HasAura(DEMON_SKIN) &&
             !_bot->HasAura(DEMON_ARMOR) && !_bot->HasAura(FEL_ARMOR))
    {
        if (_bot->CastSpell(_bot, DEMON_SKIN, false))
        {
            TC_LOG_DEBUG("playerbots", "WarlockSpecialization: Bot {} cast demon skin",
                        _bot->GetName());
        }
    }
}

// Crowd control methods
void WarlockSpecialization::CastFear(Unit* target)
{
    if (!target || !_bot)
        return;

    if (target->HasAura(FEAR))
        return;

    // Don't fear in groups to avoid pulling additional mobs
    if (_bot->GetGroup())
        return;

    if (_bot->CastSpell(target, FEAR, false))
    {
        TC_LOG_DEBUG("playerbots", "WarlockSpecialization: Bot {} cast fear on target {}",
                    _bot->GetName(), target->GetName());
    }
}

void WarlockSpecialization::CastBanish(Unit* target)
{
    if (!target || !_bot)
        return;

    if (target->HasAura(BANISH))
        return;

    // Only use on demons and elementals
    if (target->GetCreatureType() != CREATURE_TYPE_DEMON &&
        target->GetCreatureType() != CREATURE_TYPE_ELEMENTAL)
        return;

    if (_bot->CastSpell(target, BANISH, false))
    {
        TC_LOG_DEBUG("playerbots", "WarlockSpecialization: Bot {} cast banish on target {}",
                    _bot->GetName(), target->GetName());
    }
}

void WarlockSpecialization::CastDeathCoil(Unit* target)
{
    if (!target || !_bot)
        return;

    if (_bot->HasSpellCooldown(DEATH_COIL))
        return;

    // Use as emergency heal or damage
    if (_bot->GetHealthPct() < 40.0f || target->GetHealthPct() < 20.0f)
    {
        if (_bot->CastSpell(target, DEATH_COIL, false))
        {
            TC_LOG_DEBUG("playerbots", "WarlockSpecialization: Bot {} cast death coil on target {}",
                        _bot->GetName(), target->GetName());
        }
    }
}

// Positioning helpers
bool WarlockSpecialization::IsInCastingRange(Unit* target, uint32 spellId)
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

Position WarlockSpecialization::GetOptimalCastingPosition(Unit* target)
{
    if (!target || !_bot)
        return _bot->GetPosition();

    Position currentPos = _bot->GetPosition();
    Position targetPos = target->GetPosition();

    // Stay at max casting range
    float optimalRange = 28.0f; // Most warlock spells have 30 yard range, stay at 28
    float currentDistance = _bot->GetDistance2d(target);

    if (currentDistance > optimalRange + 5.0f)
    {
        // Move closer
        float angle = targetPos.GetAngle(currentPos);
        Position newPos = targetPos;
        newPos.m_positionX += cos(angle) * optimalRange;
        newPos.m_positionY += sin(angle) * optimalRange;
        return newPos;
    }
    else if (currentDistance < optimalRange - 5.0f)
    {
        // Move further away
        float angle = currentPos.GetAngle(targetPos);
        Position newPos = currentPos;
        newPos.m_positionX += cos(angle) * 5.0f;
        newPos.m_positionY += sin(angle) * 5.0f;
        return newPos;
    }

    return currentPos;
}

// Utility methods
bool WarlockSpecialization::IsChanneling()
{
    if (!_bot)
        return false;

    return _bot->HasUnitState(UNIT_STATE_CASTING);
}

bool WarlockSpecialization::IsCasting()
{
    if (!_bot)
        return false;

    return _bot->HasUnitState(UNIT_STATE_CASTING);
}

bool WarlockSpecialization::CanCast()
{
    if (!_bot)
        return false;

    return !_bot->HasUnitState(UNIT_STATE_CASTING | UNIT_STATE_STUNNED |
                              UNIT_STATE_CONFUSED | UNIT_STATE_FLEEING);
}

// Emergency abilities
void WarlockSpecialization::UseEmergencyAbilities()
{
    if (!_bot)
        return;

    float healthPct = _bot->GetHealthPct();

    // Use Death Coil for emergency healing
    if (healthPct < 25.0f && !_bot->HasSpellCooldown(DEATH_COIL))
    {
        CastDeathCoil(_bot);
    }

    // Use Life Tap if low on mana but high on health
    if (GetManaPercent() < 20.0f && healthPct > 50.0f)
    {
        CastLifeTap();
    }

    // Use Soulshatter to drop threat
    if (_bot->HasSpell(SOULSHATTER) && !_bot->HasSpellCooldown(SOULSHATTER))
    {
        // Check if we have high threat
        Unit* target = _bot->GetSelectedUnit();
        if (target && target->GetThreatManager().GetThreat(_bot) > 0)
        {
            if (_bot->CastSpell(_bot, SOULSHATTER, false))
            {
                UseSoulShard(SOULSHATTER);
                TC_LOG_DEBUG("playerbots", "WarlockSpecialization: Bot {} used soulshatter",
                            _bot->GetName());
            }
        }
    }
}

// Target selection helpers
Unit* WarlockSpecialization::GetBestDoTTarget()
{
    if (!_bot)
        return nullptr;

    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(_bot, _bot, 30.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(_bot, targets, u_check);
    Cell::VisitAllObjects(_bot, searcher, 30.0f);

    Unit* bestTarget = nullptr;
    float bestScore = 0.0f;

    for (Unit* target : targets)
    {
        if (!target || !target->IsAlive())
            continue;

        float score = 0.0f;

        // Prefer targets without DoTs
        if (!IsDoTActive(target, CORRUPTION))
            score += 10.0f;
        if (!IsDoTActive(target, CURSE_OF_AGONY))
            score += 10.0f;
        if (!IsDoTActive(target, IMMOLATE))
            score += 8.0f;

        // Prefer higher health targets (DoTs work better on them)
        score += target->GetHealthPct() * 0.1f;

        // Prefer targets not being attacked by others
        if (!target->GetVictim())
            score += 5.0f;

        if (score > bestScore)
        {
            bestScore = score;
            bestTarget = target;
        }
    }

    return bestTarget;
}

Unit* WarlockSpecialization::GetBestDirectDamageTarget()
{
    if (!_bot)
        return nullptr;

    Unit* currentTarget = _bot->GetSelectedUnit();
    if (currentTarget && currentTarget->IsAlive() &&
        IsInCastingRange(currentTarget, SHADOW_BOLT))
        return currentTarget;

    // Find nearest enemy
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(_bot, _bot, 30.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(_bot, targets, u_check);
    Cell::VisitAllObjects(_bot, searcher, 30.0f);

    Unit* nearestTarget = nullptr;
    float nearestDistance = 1000.0f;

    for (Unit* target : targets)
    {
        if (!target || !target->IsAlive())
            continue;

        float distance = _bot->GetDistance2d(target);
        if (distance < nearestDistance && IsInCastingRange(target, SHADOW_BOLT))
        {
            nearestDistance = distance;
            nearestTarget = target;
        }
    }

    return nearestTarget;
}

} // namespace Playerbot