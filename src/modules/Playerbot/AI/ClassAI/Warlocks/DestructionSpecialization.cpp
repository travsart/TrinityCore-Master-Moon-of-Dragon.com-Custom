/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "DestructionSpecialization.h"
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
#include <algorithm>

namespace Playerbot
{

DestructionSpecialization::DestructionSpecialization(Player* bot)
    : WarlockSpecialization(bot), _shadowBurnCharges(0), _backdraftStacks(0),
      _conflagrateCharges(0), _lastImmolate(0), _lastConflagrate(0), _lastShadowBurn(0)
{
    _currentPet = WarlockPet::NONE;
    _petUnit = nullptr;
    _petBehavior = PetBehavior::DEFENSIVE;
    _lastPetCommand = 0;
    _lastDoTCheck = 0;
}

void DestructionSpecialization::UpdateRotation(Unit* target)
{
    if (!target || !GetBot())
        return;

    Player* bot = GetBot();
    uint32 now = getMSTime();

    // Update core mechanics
    UpdatePetManagement();
    UpdateBackdraft();
    UpdateConflagrate();
    UpdateShadowBurn();
    UpdateDoTManagement();
    UpdateCurseManagement();

    // Ensure we have a pet out (Imp for destruction)
    if (!IsPetAlive())
    {
        SummonOptimalPet();
        return;
    }

    // Destruction rotation priority
    // 1. Immolate if not active on target
    if (ShouldCastImmolate(target))
    {
        CastImmolate(target);
        return;
    }

    // 2. Conflagrate if immolate is active
    if (ShouldCastConflagrate(target))
    {
        CastConflagrate(target);
        return;
    }

    // 3. Chaos Bolt if available and target has high health
    if (ShouldCastChaosBolt(target))
    {
        CastChaosBolt(target);
        return;
    }

    // 4. Shadow Burn for execute phase
    if (ShouldCastShadowBurn(target))
    {
        CastShadowBurn(target);
        return;
    }

    // 5. Incinerate as main filler spell
    if (ShouldCastIncinerate(target))
    {
        CastIncinerate(target);
        return;
    }

    // 6. Shadow Bolt as fallback
    if (IsInCastingRange(target, SHADOW_BOLT) && HasEnoughMana(100))
    {
        if (bot->CastSpell(target, SHADOW_BOLT, false))
        {
            TC_LOG_DEBUG("playerbots", "DestructionSpecialization: Bot {} cast shadow bolt on target {}",
                        bot->GetName(), target->GetName());
        }
        return;
    }

    // 7. Life Tap if low on mana
    if (GetManaPercent() < 30.0f && bot->GetHealthPct() > 50.0f)
    {
        CastLifeTap();
    }
}

void DestructionSpecialization::UpdateBuffs()
{
    if (!GetBot())
        return;

    Player* bot = GetBot();

    // Maintain armor
    UpdateArmor();

    // Ensure pet is out
    if (!IsPetAlive())
    {
        SummonOptimalPet();
    }
}

void DestructionSpecialization::UpdateCooldowns(uint32 diff)
{
    // Update internal cooldown tracking
    for (auto& [spellId, cooldownEnd] : _cooldowns)
    {
        if (cooldownEnd > diff)
            cooldownEnd -= diff;
        else
            cooldownEnd = 0;
    }

    // Update backdraft stacks
    Player* bot = GetBot();
    if (bot)
    {
        if (Aura* backdraftAura = bot->GetAura(BACKDRAFT))
        {
            _backdraftStacks = backdraftAura->GetStackAmount();
        }
        else
        {
            _backdraftStacks = 0;
        }
    }
}

bool DestructionSpecialization::CanUseAbility(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    if (bot->HasSpellCooldown(spellId))
        return false;

    if (!HasEnoughResource(spellId))
        return false;

    return CanCast();
}

void DestructionSpecialization::OnCombatStart(Unit* target)
{
    if (!target || !GetBot())
        return;

    Player* bot = GetBot();

    // Ensure we have optimal pet
    if (!IsPetAlive())
    {
        SummonOptimalPet();
    }

    // Command pet to attack
    if (IsPetAlive())
    {
        CommandPet(PET_ATTACK, target);
    }

    // Reset state
    _shadowBurnCharges = 3; // Shadow burn typically has 3 charges
    _backdraftStacks = 0;
    _conflagrateCharges = 2; // Conflagrate typically has 2 charges

    TC_LOG_DEBUG("playerbots", "DestructionSpecialization: Bot {} entered combat with target {}",
                bot->GetName(), target->GetName());
}

void DestructionSpecialization::OnCombatEnd()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Command pet to follow
    if (IsPetAlive())
    {
        CommandPet(PET_FOLLOW);
    }

    // Reset combat state
    _shadowBurnCharges = 3;
    _backdraftStacks = 0;
    _conflagrateCharges = 2;

    TC_LOG_DEBUG("playerbots", "DestructionSpecialization: Bot {} combat ended",
                bot->GetName());
}

bool DestructionSpecialization::HasEnoughResource(uint32 spellId)
{
    switch (spellId)
    {
        case INCINERATE:
            return HasEnoughMana(110);
        case IMMOLATE:
            return HasEnoughMana(125);
        case CONFLAGRATE:
            return HasEnoughMana(165);
        case SHADOW_BURN:
            return HasEnoughMana(135) && HasSoulShardsAvailable(1);
        case CHAOS_BOLT:
            return HasEnoughMana(300) && HasSoulShardsAvailable(1);
        case SOUL_FIRE:
            return HasEnoughMana(250) && HasSoulShardsAvailable(1);
        case SHADOW_BOLT:
            return HasEnoughMana(100);
        case CORRUPTION:
            return HasEnoughMana(75);
        case CURSE_OF_AGONY:
            return HasEnoughMana(60);
        case LIFE_TAP:
            return GetBot() && GetBot()->GetHealthPct() > 30.0f;
        default:
            return HasEnoughMana(100); // Default mana cost
    }
}

void DestructionSpecialization::ConsumeResource(uint32 spellId)
{
    // Resources are consumed automatically by spell casting
    // This method is for tracking purposes
    uint32 now = getMSTime();
    _cooldowns[spellId] = now + 1500; // 1.5s global cooldown

    // Track soul shard usage
    if (spellId == SHADOW_BURN || spellId == CHAOS_BOLT || spellId == SOUL_FIRE)
    {
        UseSoulShard(spellId);
    }
}

Position DestructionSpecialization::GetOptimalPosition(Unit* target)
{
    if (!target || !GetBot())
        return GetBot()->GetPosition();

    return GetOptimalCastingPosition(target);
}

float DestructionSpecialization::GetOptimalRange(Unit* target)
{
    return OPTIMAL_CASTING_RANGE;
}

void DestructionSpecialization::UpdatePetManagement()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    uint32 now = getMSTime();

    // Check pet status every 2 seconds
    if (now - _lastPetCommand < 2000)
        return;

    _lastPetCommand = now;

    // Ensure we have the optimal pet
    WarlockPet optimalPet = GetOptimalPetForSituation();
    if (optimalPet != _currentPet || !IsPetAlive())
    {
        SummonOptimalPet();
    }

    // Update pet AI if alive
    if (IsPetAlive())
    {
        // Ensure pet is in defensive mode by default
        if (_petBehavior != PetBehavior::DEFENSIVE)
        {
            CommandPet(PET_DEFENSIVE);
        }

        // If in combat, ensure pet is attacking current target
        if (bot->IsInCombat())
        {
            Unit* target = bot->GetSelectedUnit();
            if (target && target->IsAlive())
            {
                CommandPet(PET_ATTACK, target);
            }
        }
    }
}

void DestructionSpecialization::SummonOptimalPet()
{
    WarlockPet optimalPet = GetOptimalPetForSituation();
    SummonPet(optimalPet);
}

WarlockPet DestructionSpecialization::GetOptimalPetForSituation()
{
    Player* bot = GetBot();
    if (!bot)
        return WarlockPet::IMP;

    // Destruction typically uses Imp for damage boost
    if (bot->HasSpell(SUMMON_IMP))
        return WarlockPet::IMP;

    return WarlockPet::IMP;
}

void DestructionSpecialization::CommandPet(uint32 action, Unit* target)
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

    switch (action)
    {
        case PET_ATTACK:
            if (target)
                PetAttackTarget(target);
            break;
        case PET_FOLLOW:
            PetFollow();
            break;
        case PET_DEFENSIVE:
            _petBehavior = PetBehavior::DEFENSIVE;
            break;
        case PET_AGGRESSIVE:
            _petBehavior = PetBehavior::AGGRESSIVE;
            break;
        case PET_PASSIVE:
            _petBehavior = PetBehavior::PASSIVE;
            break;
    }

    _lastPetCommand = now;
}

void DestructionSpecialization::UpdateDoTManagement()
{
    uint32 now = getMSTime();

    if (now - _lastDoTCheck < 2000) // Check every 2 seconds
        return;

    _lastDoTCheck = now;

    Player* bot = GetBot();
    if (!bot || !bot->IsInCombat())
        return;

    // Get nearby enemies
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(bot, bot, 30.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, targets, u_check);
    Cell::VisitAllObjects(bot, searcher, 30.0f);

    // Apply DoTs to targets that need them
    for (Unit* target : targets)
    {
        if (!target || !target->IsAlive())
            continue;

        ApplyDoTsToTarget(target);
    }
}

void DestructionSpecialization::ApplyDoTsToTarget(Unit* target)
{
    if (!target || !GetBot())
        return;

    Player* bot = GetBot();

    // Apply Immolate - primary DoT for destruction
    if (ShouldApplyDoT(target, IMMOLATE))
    {
        CastImmolate(target);
    }

    // Apply Corruption if we have spare GCDs
    if (ShouldApplyDoT(target, CORRUPTION))
    {
        if (bot->CastSpell(target, CORRUPTION, false))
        {
            TC_LOG_DEBUG("playerbots", "DestructionSpecialization: Bot {} cast corruption on target {}",
                        bot->GetName(), target->GetName());
        }
    }
}

bool DestructionSpecialization::ShouldApplyDoT(Unit* target, uint32 spellId)
{
    if (!target || !GetBot())
        return false;

    // Don't apply if target already has the DoT from us
    if (IsDoTActive(target, spellId))
        return false;

    // Don't apply if target is low health (DoTs won't have time to tick)
    if (target->GetHealthPct() < 25.0f && spellId != IMMOLATE)
        return false;

    // Immolate should always be applied for destruction (conflagrate requirement)
    if (spellId == IMMOLATE && target->GetHealthPct() < 15.0f)
        return false;

    // Check if we have enough mana
    if (!HasEnoughResource(spellId))
        return false;

    // Check range
    if (!IsInCastingRange(target, spellId))
        return false;

    return true;
}

void DestructionSpecialization::UpdateCurseManagement()
{
    Player* bot = GetBot();
    if (!bot || !bot->IsInCombat())
        return;

    Unit* target = bot->GetSelectedUnit();
    if (!target || !target->IsAlive())
        return;

    uint32 optimalCurse = GetOptimalCurseForTarget(target);
    if (optimalCurse != 0)
    {
        CastCurse(target, optimalCurse);
    }
}

uint32 DestructionSpecialization::GetOptimalCurseForTarget(Unit* target)
{
    if (!target || !GetBot())
        return 0;

    Player* bot = GetBot();

    // Check what curses we know and what the target needs
    if (bot->HasSpell(CURSE_OF_ELEMENTS) && !target->HasAura(CURSE_OF_ELEMENTS))
    {
        return CURSE_OF_ELEMENTS;
    }

    if (bot->HasSpell(CURSE_OF_SHADOW) && !target->HasAura(CURSE_OF_SHADOW))
    {
        return CURSE_OF_SHADOW;
    }

    if (bot->HasSpell(CURSE_OF_AGONY) && !target->HasAura(CURSE_OF_AGONY))
    {
        return CURSE_OF_AGONY;
    }

    return 0;
}

void DestructionSpecialization::UpdateSoulShardManagement()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Update soul shard count
    HasSoulShardsAvailable(0); // This updates the count

    // Manage soul shard conservation
    if (_soulShards.count < 3)
    {
        _soulShards.conserveMode = true;
    }
    else if (_soulShards.count > 10)
    {
        _soulShards.conserveMode = false;
    }
}

bool DestructionSpecialization::HasSoulShardsAvailable(uint32 required)
{
    return WarlockSpecialization::HasSoulShardsAvailable(required);
}

void DestructionSpecialization::UseSoulShard(uint32 spellId)
{
    WarlockSpecialization::UseSoulShard(spellId);
}

// Private methods

void DestructionSpecialization::UpdateBackdraft()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Check backdraft stacks
    if (Aura* backdraftAura = bot->GetAura(BACKDRAFT))
    {
        _backdraftStacks = backdraftAura->GetStackAmount();
    }
    else
    {
        _backdraftStacks = 0;
    }
}

void DestructionSpecialization::UpdateConflagrate()
{
    // Conflagrate charge management is handled by the core system
    // This method updates our internal tracking
}

void DestructionSpecialization::UpdateShadowBurn()
{
    // Shadow burn charge management
    Player* bot = GetBot();
    if (!bot)
        return;

    // Check if shadow burn is off cooldown
    if (!bot->HasSpellCooldown(SHADOW_BURN))
    {
        _shadowBurnCharges = std::min(_shadowBurnCharges + 1, 3u);
    }
}

bool DestructionSpecialization::ShouldCastImmolate(Unit* target)
{
    if (!target || !GetBot())
        return false;

    // Always maintain immolate for conflagrate
    if (!IsDoTActive(target, IMMOLATE))
        return true;

    // Refresh if low duration
    uint32 remaining = GetDoTRemainingTime(target, IMMOLATE);
    if (remaining < 3000) // Less than 3 seconds
        return true;

    return false;
}

bool DestructionSpecialization::ShouldCastIncinerate(Unit* target)
{
    if (!target || !GetBot())
        return false;

    Player* bot = GetBot();

    if (bot->HasSpellCooldown(INCINERATE))
        return false;

    if (!IsInCastingRange(target, INCINERATE))
        return false;

    if (!HasEnoughResource(INCINERATE))
        return false;

    return true; // Incinerate is the main filler spell for destruction
}

bool DestructionSpecialization::ShouldCastConflagrate(Unit* target)
{
    if (!target || !GetBot())
        return false;

    Player* bot = GetBot();

    if (bot->HasSpellCooldown(CONFLAGRATE))
        return false;

    if (!IsInCastingRange(target, CONFLAGRATE))
        return false;

    if (!HasEnoughResource(CONFLAGRATE))
        return false;

    // Conflagrate requires immolate to be active
    if (!IsDoTActive(target, IMMOLATE))
        return false;

    return true;
}

bool DestructionSpecialization::ShouldCastShadowBurn(Unit* target)
{
    if (!target || !GetBot())
        return false;

    Player* bot = GetBot();

    if (bot->HasSpellCooldown(SHADOW_BURN))
        return false;

    if (!IsInCastingRange(target, SHADOW_BURN))
        return false;

    if (!HasEnoughResource(SHADOW_BURN))
        return false;

    // Use shadow burn for execute phase or when we have many charges
    if (target->GetHealthPct() < 25.0f || _shadowBurnCharges >= 3)
        return true;

    return false;
}

bool DestructionSpecialization::ShouldCastChaosBolt(Unit* target)
{
    if (!target || !GetBot())
        return false;

    Player* bot = GetBot();

    if (bot->HasSpellCooldown(CHAOS_BOLT))
        return false;

    if (!IsInCastingRange(target, CHAOS_BOLT))
        return false;

    if (!HasEnoughResource(CHAOS_BOLT))
        return false;

    // Use chaos bolt on high health targets or when we have many soul shards
    if (target->GetHealthPct() > 50.0f || _soulShards.count > 10)
        return true;

    return false;
}

void DestructionSpecialization::CastImmolate(Unit* target)
{
    if (!target || !GetBot())
        return;

    Player* bot = GetBot();

    if (bot->CastSpell(target, IMMOLATE, false))
    {
        _lastImmolate = getMSTime();
        ConsumeResource(IMMOLATE);
        TC_LOG_DEBUG("playerbots", "DestructionSpecialization: Bot {} cast immolate on target {}",
                    bot->GetName(), target->GetName());
    }
}

void DestructionSpecialization::CastIncinerate(Unit* target)
{
    if (!target || !GetBot())
        return;

    Player* bot = GetBot();

    if (bot->CastSpell(target, INCINERATE, false))
    {
        ConsumeResource(INCINERATE);
        TC_LOG_DEBUG("playerbots", "DestructionSpecialization: Bot {} cast incinerate on target {}",
                    bot->GetName(), target->GetName());
    }
}

void DestructionSpecialization::CastConflagrate(Unit* target)
{
    if (!target || !GetBot())
        return;

    Player* bot = GetBot();

    if (bot->CastSpell(target, CONFLAGRATE, false))
    {
        _lastConflagrate = getMSTime();
        _conflagrateCharges = std::max(0, static_cast<int>(_conflagrateCharges) - 1);
        ConsumeResource(CONFLAGRATE);
        TC_LOG_DEBUG("playerbots", "DestructionSpecialization: Bot {} cast conflagrate on target {}",
                    bot->GetName(), target->GetName());
    }
}

void DestructionSpecialization::CastShadowBurn(Unit* target)
{
    if (!target || !GetBot())
        return;

    Player* bot = GetBot();

    if (bot->CastSpell(target, SHADOW_BURN, false))
    {
        _lastShadowBurn = getMSTime();
        _shadowBurnCharges = std::max(0, static_cast<int>(_shadowBurnCharges) - 1);
        ConsumeResource(SHADOW_BURN);
        TC_LOG_DEBUG("playerbots", "DestructionSpecialization: Bot {} cast shadow burn on target {}",
                    bot->GetName(), target->GetName());
    }
}

void DestructionSpecialization::CastChaosBolt(Unit* target)
{
    if (!target || !GetBot())
        return;

    Player* bot = GetBot();

    if (bot->CastSpell(target, CHAOS_BOLT, false))
    {
        ConsumeResource(CHAOS_BOLT);
        TC_LOG_DEBUG("playerbots", "DestructionSpecialization: Bot {} cast chaos bolt on target {}",
                    bot->GetName(), target->GetName());
    }
}

} // namespace Playerbot