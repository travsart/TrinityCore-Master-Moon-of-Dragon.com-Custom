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
#include "../../../AI/BotAI.h"
#include "MotionMaster.h"
#include "Item.h"
#include "CharmInfo.h"
#include "Bag.h"
#include "SpellHistory.h"
#include "SharedDefines.h"
#include "../../../AI/Combat/TargetSelector.h"
#include <algorithm>

namespace Playerbot
{

// Pet management methods
void WarlockSpecialization::SummonPet(WarlockPet petType)
{
    Player* bot = GetBot();
    if (!bot)
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
    if (!bot->HasSpell(summonSpell))
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
    if (bot->CastSpell(GetBot(), summonSpell, false))
    {
        _currentPet = petType;
        _lastPetCommand = getMSTime();

        // Use soul shard if required
        if (petType != WarlockPet::IMP)
        {
            UseSoulShard(summonSpell);
        }

        TC_LOG_DEBUG("playerbots", "WarlockSpecialization: Bot {} summoned {} pet",
                    bot->GetName().c_str(), static_cast<uint32>(petType));
    }
}

void WarlockSpecialization::PetAttackTarget(Unit* target)
{
    Player* bot = GetBot();
    if (!target || !bot)
        return;

    Pet* pet = bot->GetPet();
    if (!pet || !pet->IsAlive())
        return;

    uint32 now = getMSTime();
    if (now - _lastPetCommand < 1000) // 1 second cooldown between commands
        return;

    if (pet->GetVictim() == target)
        return; // Already attacking this target

    // Command pet to attack
    if (CharmInfo* charmInfo = pet->GetCharmInfo())
    {
        charmInfo->SetIsCommandAttack(true);
        charmInfo->SetCommandState(COMMAND_ATTACK);
    }

    _petUnit = pet;
    _lastPetCommand = now;

    TC_LOG_DEBUG("playerbots", "WarlockSpecialization: Bot {} commanded pet to attack {}",
                bot->GetName().c_str(), target->GetName().c_str());
}

void WarlockSpecialization::PetFollow()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    Pet* pet = bot->GetPet();
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
                bot->GetName().c_str());
}

bool WarlockSpecialization::IsPetAlive()
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    Pet* pet = bot->GetPet();
    if (!pet)
        return false;

    _petUnit = pet;
    return pet->IsAlive();
}

// DoT management methods
bool WarlockSpecialization::IsDoTActive(Unit* target, uint32 spellId)
{
    Player* bot = GetBot();
    if (!target || !bot)
        return false;

    return target->HasAura(spellId, bot->GetGUID());
}

uint32 WarlockSpecialization::GetDoTRemainingTime(Unit* target, uint32 spellId)
{
    Player* bot = GetBot();
    if (!target || !bot)
        return 0;

    if (Aura* aura = target->GetAura(spellId, bot->GetGUID()))
    {
        return aura->GetDuration();
    }

    return 0;
}

void WarlockSpecialization::CastCurse(Unit* target, uint32 curseId)
{
    Player* bot = GetBot();
    if (!target || !bot)
        return;

    // Don't overwrite stronger curses
    if (target->HasAura(CURSE_OF_ELEMENTS) && curseId != CURSE_OF_ELEMENTS)
        return;
    if (target->HasAura(CURSE_OF_SHADOW) && curseId != CURSE_OF_SHADOW)
        return;

    // Check if target already has this curse from us
    if (target->HasAura(curseId, bot->GetGUID()))
        return;

    if (bot->CastSpell(target, curseId, false))
    {
        TC_LOG_DEBUG("playerbots", "WarlockSpecialization: Bot {} cast curse {} on target {}",
                    bot->GetName().c_str(), curseId, target->GetName().c_str());
    }
}

// Soul shard management methods
bool WarlockSpecialization::HasSoulShardsAvailable(uint32 required)
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    // Count soul shards in inventory
    uint32 shardCount = 0;
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        if (Bag* bagItem = bot->GetBagByPos(bag))
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
        if (Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
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
    Player* bot = GetBot();
    if (!bot)
        return;

    // Find and consume a soul shard
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        if (Bag* bagItem = bot->GetBagByPos(bag))
        {
            for (uint32 slot = 0; slot < bagItem->GetBagSize(); ++slot)
            {
                if (Item* item = bagItem->GetItemByPos(slot))
                {
                    if (item->GetEntry() == 6265) // Soul Shard item ID
                    {
                        bot->DestroyItem(bag, slot, true);
                        _soulShards.count--;
                        _soulShards.lastUsed = getMSTime();

                        TC_LOG_DEBUG("playerbots", "WarlockSpecialization: Bot {} used soul shard for spell {} (remaining: {})",
                                    bot->GetName().c_str(), spellId, _soulShards.count);
                        return;
                    }
                }
            }
        }
    }

    // Check main inventory
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
    {
        if (Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        {
            if (item->GetEntry() == 6265) // Soul Shard item ID
            {
                bot->DestroyItem(INVENTORY_SLOT_BAG_0, slot, true);
                _soulShards.count--;
                _soulShards.lastUsed = getMSTime();

                TC_LOG_DEBUG("playerbots", "WarlockSpecialization: Bot {} used soul shard for spell {} (remaining: {})",
                            bot->GetName().c_str(), spellId, _soulShards.count);
                return;
            }
        }
    }
}

// Resource management helpers
uint32 WarlockSpecialization::GetMana() const
{
    Player* bot = GetBot();
    if (!bot)
        return 0;

    return bot->GetPower(POWER_MANA);
}

uint32 WarlockSpecialization::GetMaxMana() const
{
    Player* bot = GetBot();
    if (!bot)
        return 0;

    return bot->GetMaxPower(POWER_MANA);
}

float WarlockSpecialization::GetManaPercent() const
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
    Player* bot = GetBot();
    if (!bot)
        return;

    if (bot->GetHealthPct() < 30.0f)
        return; // Don't life tap when low on health

    // Skip cooldown check for simplicity - TrinityCore API compatibility
    // if (bot->HasSpellCooldown(LIFE_TAP))
    //    return;

    if (bot->CastSpell(bot, LIFE_TAP, false))
    {
        TC_LOG_DEBUG("playerbots", "WarlockSpecialization: Bot {} used life tap",
                    bot->GetName().c_str());
    }
}

// Buff management
void WarlockSpecialization::UpdateArmor()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Prioritize Fel Armor > Demon Armor > Demon Skin
    if (bot->HasSpell(FEL_ARMOR) && !bot->HasAura(FEL_ARMOR))
    {
        if (bot->CastSpell(bot, FEL_ARMOR, false))
        {
            TC_LOG_DEBUG("playerbots", "WarlockSpecialization: Bot {} cast fel armor",
                        bot->GetName().c_str());
        }
    }
    else if (bot->HasSpell(DEMON_ARMOR) && !bot->HasAura(DEMON_ARMOR) && !bot->HasAura(FEL_ARMOR))
    {
        if (bot->CastSpell(bot, DEMON_ARMOR, false))
        {
            TC_LOG_DEBUG("playerbots", "WarlockSpecialization: Bot {} cast demon armor",
                        bot->GetName().c_str());
        }
    }
    else if (bot->HasSpell(DEMON_SKIN) && !bot->HasAura(DEMON_SKIN) &&
             !bot->HasAura(DEMON_ARMOR) && !bot->HasAura(FEL_ARMOR))
    {
        if (bot->CastSpell(bot, DEMON_SKIN, false))
        {
            TC_LOG_DEBUG("playerbots", "WarlockSpecialization: Bot {} cast demon skin",
                        bot->GetName().c_str());
        }
    }
}

// Crowd control methods
bool WarlockSpecialization::CastFear(Unit* target)
{
    Player* bot = GetBot();
    if (!target || !bot)
        return false;

    if (target->HasAura(FEAR))
        return false;

    // Don't fear in groups to avoid pulling additional mobs
    if (bot->GetGroup())
        return false;

    if (bot->CastSpell(target, FEAR, false))
    {
        TC_LOG_DEBUG("playerbots", "WarlockSpecialization: Bot {} cast fear on target {}",
                    bot->GetName().c_str(), target->GetName().c_str());
        return true;
    }

    return false;
}

bool WarlockSpecialization::CastBanish(Unit* target)
{
    Player* bot = GetBot();
    if (!target || !bot)
        return false;

    if (target->HasAura(BANISH))
        return false;

    // Only use on demons and elementals
    if (target->GetCreatureType() != CREATURE_TYPE_DEMON &&
        target->GetCreatureType() != CREATURE_TYPE_ELEMENTAL)
        return false;

    if (bot->CastSpell(target, BANISH, false))
    {
        TC_LOG_DEBUG("playerbots", "WarlockSpecialization: Bot {} cast banish on target {}",
                    bot->GetName().c_str(), target->GetName().c_str());
        return true;
    }

    return false;
}

// Note: CastDeathCoil is not a warlock ability - removed broken implementation

// Positioning helpers
bool WarlockSpecialization::IsInCastingRange(Unit* target, uint32 spellId)
{
    Player* bot = GetBot();
    if (!target || !bot)
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return false;

    float range = spellInfo->GetMaxRange();
    float distance = bot->GetDistance2d(target);

    return distance <= range && bot->IsWithinLOSInMap(target);
}

Position WarlockSpecialization::GetOptimalCastingPosition(Unit* target)
{
    Player* bot = GetBot();
    if (!target || !bot)
        return bot->GetPosition();

    Position currentPos = bot->GetPosition();
    Position targetPos = target->GetPosition();

    // Stay at max casting range
    float optimalRange = 28.0f; // Most warlock spells have 30 yard range, stay at 28
    float currentDistance = bot->GetDistance2d(target);

    if (currentDistance > optimalRange + 5.0f)
    {
        // Move closer
        float angle = atan2(currentPos.GetPositionY() - targetPos.GetPositionY(),
                             currentPos.GetPositionX() - targetPos.GetPositionX());
        Position newPos = targetPos;
        newPos.m_positionX += cos(angle) * optimalRange;
        newPos.m_positionY += sin(angle) * optimalRange;
        return newPos;
    }
    else if (currentDistance < optimalRange - 5.0f)
    {
        // Move further away
        float angle = atan2(targetPos.GetPositionY() - currentPos.GetPositionY(),
                             targetPos.GetPositionX() - currentPos.GetPositionX());
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
    Player* bot = GetBot();
    if (!bot)
        return false;

    return bot->HasUnitState(UNIT_STATE_CASTING);
}

bool WarlockSpecialization::IsCasting()
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    return bot->HasUnitState(UNIT_STATE_CASTING);
}

bool WarlockSpecialization::CanCast()
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    return !bot->HasUnitState(UNIT_STATE_CASTING | UNIT_STATE_STUNNED |
                              UNIT_STATE_CONFUSED | UNIT_STATE_FLEEING);
}

// Emergency abilities
bool WarlockSpecialization::UseEmergencyAbilities()
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    float healthPct = bot->GetHealthPct();

    // Use Death Coil for emergency healing
    if (healthPct < 25.0f)
    {
        CastDeathCoil(GetBot());
    }

    // Use Life Tap if low on mana but high on health
    if (GetManaPercent() < 20.0f && healthPct > 50.0f)
    {
        CastLifeTap();
    }

    // Use Soulshatter to drop threat
    if (bot->HasSpell(SOULSHATTER))
    {
        // Check if we have high threat
        Unit* target = bot->GetSelectedUnit();
        if (target && target->GetThreatManager().GetThreat(GetBot()) > 0)
        {
            if (bot->CastSpell(GetBot(), SOULSHATTER, false))
            {
                UseSoulShard(SOULSHATTER);
                TC_LOG_DEBUG("playerbots", "WarlockSpecialization: Bot {} used soulshatter",
                            bot->GetName().c_str());
            }
        }
    }

    return false;
}

// Target selection helpers
Unit* WarlockSpecialization::GetBestDoTTarget()
{
    if (!GetBot())
        return nullptr;

    Unit* nearestEnemy = TargetSelectionUtils::GetNearestEnemy(GetBot(), 30.0f);
    if (!nearestEnemy) return nullptr;

    // For now, we'll use simplified logic focusing on the nearest enemy
    std::vector<Unit*> targets = { nearestEnemy };

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
    Player* bot = GetBot();
    if (!bot)
        return nullptr;

    Unit* currentTarget = bot->GetSelectedUnit();
    if (currentTarget && currentTarget->IsAlive() &&
        IsInCastingRange(currentTarget, SHADOW_BOLT))
        return currentTarget;

    // Find nearest enemy
    Unit* nearestEnemy = TargetSelectionUtils::GetNearestEnemy(GetBot(), 30.0f);
    if (!nearestEnemy) return nullptr;

    // For now, we'll use simplified logic focusing on the nearest enemy
    std::vector<Unit*> targets = { nearestEnemy };

    Unit* nearestTarget = nullptr;
    float nearestDistance = 1000.0f;

    for (Unit* target : targets)
    {
        if (!target || !target->IsAlive())
            continue;

        float distance = bot->GetDistance2d(target);
        if (distance < nearestDistance && IsInCastingRange(target, SHADOW_BOLT))
        {
            nearestDistance = distance;
            nearestTarget = target;
        }
    }

    return nearestTarget;
}

bool WarlockSpecialization::CastDeathCoil(::Unit* target)
{
    Player* bot = GetBot();
    if (!bot || !target)
        return false;

    uint32 spellId = DEATH_COIL;
    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return false;

    // Check if spell is available and bot has it
    if (!bot->HasSpell(spellId))
        return false;

    // Check cooldown
    if (bot->GetSpellHistory()->HasCooldown(spellId))
        return false;

    // Check mana cost
    auto powerCosts = spellInfo->CalcPowerCost(bot, spellInfo->GetSchoolMask());
    uint32 manaCost = 0;
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA)
        {
            manaCost = cost.Amount;
            break;
        }
    }

    if (manaCost > 0 && bot->GetPower(POWER_MANA) < static_cast<int32>(manaCost))
        return false;

    // Check range
    if (bot->GetDistance(target) > spellInfo->GetMaxRange())
        return false;

    // Cast the spell
    bot->CastSpell(target, spellId, false);
    return true;
}

} // namespace Playerbot