/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "DemonologySpecialization.h"
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

DemonologySpecialization::DemonologySpecialization(Player* bot)
    : WarlockSpecialization(bot), _demonicEmpowermentStacks(0), _lastMetamorphosis(0),
      _felguardCommands(0), _lastDemonicEmpowerment(0), _demonFormActive(false), _petEnhanced(false)
{
    _currentPet = WarlockPet::NONE;
    _petUnit = nullptr;
    _petBehavior = PetBehavior::DEFENSIVE;
    _lastPetCommand = 0;
    _lastDoTCheck = 0;
}

void DemonologySpecialization::UpdateRotation(Unit* target)
{
    if (!target || !GetBot())
        return;

    Player* bot = GetBot();
    uint32 now = getMSTime();

    // Update core mechanics
    UpdatePetManagement();
    UpdateDemonicEmpowerment();
    UpdateMetamorphosis();
    UpdateFelguardCommands();
    UpdateDoTManagement();
    UpdateCurseManagement();

    // Ensure we have optimal pet out
    if (!IsPetAlive())
    {
        SummonOptimalPet();
        return;
    }

    // Demonology rotation priority
    // 1. Metamorphosis if available and appropriate
    if (ShouldCastMetamorphosis())
    {
        CastMetamorphosis();
        return;
    }

    // 2. Demonic Empowerment after summoning pet
    if (ShouldCastDemonicEmpowerment())
    {
        CastDemonicEmpowerment();
        return;
    }

    // 3. Apply DoTs to target
    ApplyDoTsToTarget(target);

    // 4. Use demon form abilities if in metamorphosis
    if (_demonFormActive)
    {
        UseDemonFormAbilities(target);
        return;
    }

    // 5. Shadow Bolt as filler (or Incinerate if available)
    if (IsInCastingRange(target, SHADOW_BOLT) && HasEnoughMana(100))
    {
        if (bot->CastSpell(target, SHADOW_BOLT, false))
        {
            TC_LOG_DEBUG("playerbots", "DemonologySpecialization: Bot {} cast shadow bolt on target {}",
                        bot->GetName(), target->GetName());
        }
        return;
    }

    // 6. Life Tap if low on mana
    if (GetManaPercent() < 30.0f && bot->GetHealthPct() > 50.0f)
    {
        CastLifeTap();
    }
}

void DemonologySpecialization::UpdateBuffs()
{
    if (!GetBot())
        return;

    Player* bot = GetBot();

    // Maintain armor
    UpdateArmor();

    // Enhance pet if available
    if (IsPetAlive() && !_petEnhanced)
    {
        EnhancePetAbilities();
    }

    // Cast Demonic Empowerment after summoning
    if (IsPetAlive() && ShouldCastDemonicEmpowerment())
    {
        CastDemonicEmpowerment();
    }

    // Maintain immolation aura in demon form
    if (_demonFormActive && !bot->HasAura(IMMOLATION_AURA))
    {
        CastImmolationAura();
    }
}

void DemonologySpecialization::UpdateCooldowns(uint32 diff)
{
    // Update internal cooldown tracking
    for (auto& [spellId, cooldownEnd] : _cooldowns)
    {
        if (cooldownEnd > diff)
            cooldownEnd -= diff;
        else
            cooldownEnd = 0;
    }

    // Update metamorphosis state
    if (_demonFormActive)
    {
        Player* bot = GetBot();
        if (!bot || !bot->HasAura(METAMORPHOSIS))
        {
            _demonFormActive = false;
            TC_LOG_DEBUG("playerbots", "DemonologySpecialization: Bot {} exited metamorphosis",
                        bot->GetName());
        }
    }
}

bool DemonologySpecialization::CanUseAbility(uint32 spellId)
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

void DemonologySpecialization::OnCombatStart(Unit* target)
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
    _demonicEmpowermentStacks = 0;
    _petEnhanced = false;

    TC_LOG_DEBUG("playerbots", "DemonologySpecialization: Bot {} entered combat with target {}",
                bot->GetName(), target->GetName());
}

void DemonologySpecialization::OnCombatEnd()
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
    _demonicEmpowermentStacks = 0;
    _petEnhanced = false;
    _felguardCommands = 0;

    TC_LOG_DEBUG("playerbots", "DemonologySpecialization: Bot {} combat ended",
                bot->GetName());
}

bool DemonologySpecialization::HasEnoughResource(uint32 spellId)
{
    switch (spellId)
    {
        case SHADOW_BOLT:
            return HasEnoughMana(100);
        case CORRUPTION:
            return HasEnoughMana(75);
        case CURSE_OF_AGONY:
            return HasEnoughMana(60);
        case IMMOLATE:
            return HasEnoughMana(125);
        case DEMONIC_EMPOWERMENT:
            return HasEnoughMana(200);
        case METAMORPHOSIS:
            return true; // No mana cost
        case SOUL_BURN:
            return HasEnoughMana(150) && HasSoulShardsAvailable(1);
        case LIFE_TAP:
            return GetBot() && GetBot()->GetHealthPct() > 30.0f;
        default:
            return HasEnoughMana(100); // Default mana cost
    }
}

void DemonologySpecialization::ConsumeResource(uint32 spellId)
{
    // Resources are consumed automatically by spell casting
    // This method is for tracking purposes
    uint32 now = getMSTime();
    _cooldowns[spellId] = now + 1500; // 1.5s global cooldown
}

Position DemonologySpecialization::GetOptimalPosition(Unit* target)
{
    if (!target || !GetBot())
        return GetBot()->GetPosition();

    return GetOptimalCastingPosition(target);
}

float DemonologySpecialization::GetOptimalRange(Unit* target)
{
    return OPTIMAL_CASTING_RANGE;
}

void DemonologySpecialization::UpdatePetManagement()
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

void DemonologySpecialization::SummonOptimalPet()
{
    WarlockPet optimalPet = GetOptimalPetForSituation();
    SummonPet(optimalPet);
}

WarlockPet DemonologySpecialization::GetOptimalPetForSituation()
{
    Player* bot = GetBot();
    if (!bot)
        return WarlockPet::IMP;

    // Demonology prioritizes Felguard if available
    if (bot->HasSpell(SUMMON_FELGUARD))
        return WarlockPet::FELGUARD;

    // Otherwise use Felhunter for utility
    if (bot->HasSpell(SUMMON_FELHUNTER))
        return WarlockPet::FELHUNTER;

    // Fall back to Voidwalker for tanking
    if (bot->HasSpell(SUMMON_VOIDWALKER))
        return WarlockPet::VOIDWALKER;

    // Default to Imp
    return WarlockPet::IMP;
}

void DemonologySpecialization::CommandPet(uint32 action, Unit* target)
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

void DemonologySpecialization::UpdateDoTManagement()
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

void DemonologySpecialization::ApplyDoTsToTarget(Unit* target)
{
    if (!target || !GetBot())
        return;

    Player* bot = GetBot();

    // Apply Corruption
    if (ShouldApplyDoT(target, CORRUPTION))
    {
        if (bot->CastSpell(target, CORRUPTION, false))
        {
            TC_LOG_DEBUG("playerbots", "DemonologySpecialization: Bot {} cast corruption on target {}",
                        bot->GetName(), target->GetName());
        }
    }

    // Apply Curse of Agony
    if (ShouldApplyDoT(target, CURSE_OF_AGONY))
    {
        if (bot->CastSpell(target, CURSE_OF_AGONY, false))
        {
            TC_LOG_DEBUG("playerbots", "DemonologySpecialization: Bot {} cast curse of agony on target {}",
                        bot->GetName(), target->GetName());
        }
    }

    // Apply Immolate
    if (ShouldApplyDoT(target, IMMOLATE))
    {
        if (bot->CastSpell(target, IMMOLATE, false))
        {
            TC_LOG_DEBUG("playerbots", "DemonologySpecialization: Bot {} cast immolate on target {}",
                        bot->GetName(), target->GetName());
        }
    }
}

bool DemonologySpecialization::ShouldApplyDoT(Unit* target, uint32 spellId)
{
    if (!target || !GetBot())
        return false;

    // Don't apply if target already has the DoT from us
    if (IsDoTActive(target, spellId))
        return false;

    // Don't apply if target is low health (DoTs won't have time to tick)
    if (target->GetHealthPct() < 25.0f)
        return false;

    // Check if we have enough mana
    if (!HasEnoughResource(spellId))
        return false;

    // Check range
    if (!IsInCastingRange(target, spellId))
        return false;

    return true;
}

void DemonologySpecialization::UpdateCurseManagement()
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

uint32 DemonologySpecialization::GetOptimalCurseForTarget(Unit* target)
{
    if (!target || !GetBot())
        return 0;

    Player* bot = GetBot();

    // Check what curses we know and what the target needs
    if (bot->HasSpell(CURSE_OF_ELEMENTS) && !target->HasAura(CURSE_OF_ELEMENTS))
    {
        // Casters prefer Curse of Elements
        if (target->GetPowerType() == POWER_MANA)
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

void DemonologySpecialization::UpdateSoulShardManagement()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Update soul shard count
    HasSoulShardsAvailable(0); // This updates the count

    // Manage soul shard conservation
    if (_soulShards.count < 5)
    {
        _soulShards.conserveMode = true;
    }
    else if (_soulShards.count > 15)
    {
        _soulShards.conserveMode = false;
    }
}

bool DemonologySpecialization::HasSoulShardsAvailable(uint32 required)
{
    return WarlockSpecialization::HasSoulShardsAvailable(required);
}

void DemonologySpecialization::UseSoulShard(uint32 spellId)
{
    WarlockSpecialization::UseSoulShard(spellId);
}

// Private methods

void DemonologySpecialization::UpdateDemonicEmpowerment()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    uint32 now = getMSTime();

    // Check if we have a pet and demonic empowerment is available
    if (IsPetAlive() && ShouldCastDemonicEmpowerment())
    {
        if (now - _lastDemonicEmpowerment > DEMONIC_EMPOWERMENT_COOLDOWN)
        {
            CastDemonicEmpowerment();
        }
    }
}

void DemonologySpecialization::UpdateMetamorphosis()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Update demon form state
    _demonFormActive = bot->HasAura(METAMORPHOSIS);

    // Use metamorphosis if appropriate
    if (ShouldCastMetamorphosis())
    {
        CastMetamorphosis();
    }
}

void DemonologySpecialization::UpdateFelguardCommands()
{
    Player* bot = GetBot();
    if (!bot || _currentPet != WarlockPet::FELGUARD)
        return;

    uint32 now = getMSTime();

    if (now - _felguardCommands < FELGUARD_COMMAND_INTERVAL)
        return;

    _felguardCommands = now;

    Unit* target = bot->GetSelectedUnit();
    if (target && bot->IsInCombat())
    {
        CommandFelguard(target);
    }
}

bool DemonologySpecialization::ShouldCastDemonicEmpowerment()
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    if (!IsPetAlive())
        return false;

    if (bot->HasSpellCooldown(DEMONIC_EMPOWERMENT))
        return false;

    if (!HasEnoughResource(DEMONIC_EMPOWERMENT))
        return false;

    // Cast after summoning pet or if buff is missing
    Pet* pet = bot->GetPet();
    if (pet && !pet->HasAura(DEMONIC_EMPOWERMENT))
        return true;

    return false;
}

bool DemonologySpecialization::ShouldCastMetamorphosis()
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    if (_demonFormActive)
        return false;

    if (bot->HasSpellCooldown(METAMORPHOSIS))
        return false;

    if (!bot->IsInCombat())
        return false;

    // Use during challenging fights
    std::list<Unit*> enemies;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(bot, bot, 30.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, enemies, u_check);
    Cell::VisitAllObjects(bot, searcher, 30.0f);

    // Use if facing multiple enemies or strong enemy
    if (enemies.size() >= 3)
        return true;

    for (Unit* enemy : enemies)
    {
        if (enemy && enemy->GetLevel() > bot->GetLevel() + 2)
            return true;
    }

    return false;
}

bool DemonologySpecialization::ShouldSummonFelguard()
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    return bot->HasSpell(SUMMON_FELGUARD) && HasSoulShardsAvailable(1);
}

void DemonologySpecialization::CastDemonicEmpowerment()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (bot->CastSpell(bot, DEMONIC_EMPOWERMENT, false))
    {
        _lastDemonicEmpowerment = getMSTime();
        _petEnhanced = true;
        TC_LOG_DEBUG("playerbots", "DemonologySpecialization: Bot {} cast demonic empowerment",
                    bot->GetName());
    }
}

void DemonologySpecialization::EnhancePetAbilities()
{
    Player* bot = GetBot();
    if (!bot || !IsPetAlive())
        return;

    // This would enhance pet abilities
    // For now, just cast demonic empowerment
    if (ShouldCastDemonicEmpowerment())
    {
        CastDemonicEmpowerment();
    }
}

void DemonologySpecialization::OptimizePetDamage()
{
    // Ensure pet is attacking the right target
    Player* bot = GetBot();
    if (!bot || !IsPetAlive())
        return;

    Unit* target = bot->GetSelectedUnit();
    if (target && target->IsAlive())
    {
        CommandPet(PET_ATTACK, target);
    }
}

void DemonologySpecialization::CastMetamorphosis()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (bot->CastSpell(bot, METAMORPHOSIS, false))
    {
        _lastMetamorphosis = getMSTime();
        _demonFormActive = true;
        TC_LOG_DEBUG("playerbots", "DemonologySpecialization: Bot {} cast metamorphosis",
                    bot->GetName());
    }
}

void DemonologySpecialization::UseDemonFormAbilities(Unit* target)
{
    if (!target || !GetBot() || !_demonFormActive)
        return;

    Player* bot = GetBot();

    // Use immolation aura
    if (!bot->HasAura(IMMOLATION_AURA))
    {
        CastImmolationAura();
    }

    // Use demon charge if available
    if (bot->HasSpell(DEMON_CHARGE) && !bot->HasSpellCooldown(DEMON_CHARGE))
    {
        float distance = bot->GetDistance2d(target);
        if (distance > 10.0f && distance < 30.0f)
        {
            if (bot->CastSpell(target, DEMON_CHARGE, false))
            {
                TC_LOG_DEBUG("playerbots", "DemonologySpecialization: Bot {} used demon charge",
                            bot->GetName());
            }
        }
    }

    // Continue with normal rotation
    if (IsInCastingRange(target, SHADOW_BOLT))
    {
        bot->CastSpell(target, SHADOW_BOLT, false);
    }
}

void DemonologySpecialization::CastImmolationAura()
{
    Player* bot = GetBot();
    if (!bot || !_demonFormActive)
        return;

    if (bot->CastSpell(bot, IMMOLATION_AURA, false))
    {
        TC_LOG_DEBUG("playerbots", "DemonologySpecialization: Bot {} cast immolation aura",
                    bot->GetName());
    }
}

void DemonologySpecialization::CommandFelguard(Unit* target)
{
    if (!target || !GetBot() || _currentPet != WarlockPet::FELGUARD)
        return;

    Player* bot = GetBot();
    Pet* pet = bot->GetPet();
    if (!pet)
        return;

    // Use felguard abilities
    float distance = pet->GetDistance2d(target);

    // Use intercept if far away
    if (distance > 10.0f && !pet->HasSpellCooldown(FELGUARD_INTERCEPT))
    {
        FelguardIntercept(target);
    }
    // Use cleave if close and multiple enemies
    else if (distance < 8.0f)
    {
        std::list<Unit*> nearbyEnemies;
        Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(pet, pet, 8.0f);
        Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(pet, nearbyEnemies, u_check);
        Cell::VisitAllObjects(pet, searcher, 8.0f);

        if (nearbyEnemies.size() >= 2)
        {
            FelguardCleave();
        }
    }
}

void DemonologySpecialization::FelguardCleave()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    Pet* pet = bot->GetPet();
    if (!pet || pet->HasSpellCooldown(FELGUARD_CLEAVE))
        return;

    if (pet->CastSpell(pet, FELGUARD_CLEAVE, false))
    {
        TC_LOG_DEBUG("playerbots", "DemonologySpecialization: Felguard {} used cleave",
                    pet->GetName());
    }
}

void DemonologySpecialization::FelguardIntercept(Unit* target)
{
    if (!target || !GetBot())
        return;

    Player* bot = GetBot();
    Pet* pet = bot->GetPet();
    if (!pet)
        return;

    if (pet->CastSpell(target, FELGUARD_INTERCEPT, false))
    {
        TC_LOG_DEBUG("playerbots", "DemonologySpecialization: Felguard {} intercepted target {}",
                    pet->GetName(), target->GetName());
    }
}

void DemonologySpecialization::ManageSoulBurn(Unit* target)
{
    if (!target || !GetBot())
        return;

    Player* bot = GetBot();

    // Use soul burn on appropriate targets
    if (target->GetCreatureType() == CREATURE_TYPE_DEMON ||
        target->GetCreatureType() == CREATURE_TYPE_UNDEAD)
    {
        if (ShouldApplyDoT(target, SOUL_BURN))
        {
            CastSoulBurn(target);
        }
    }
}

void DemonologySpecialization::CastSoulBurn(Unit* target)
{
    if (!target || !GetBot())
        return;

    Player* bot = GetBot();

    if (bot->CastSpell(target, SOUL_BURN, false))
    {
        UseSoulShard(SOUL_BURN);
        TC_LOG_DEBUG("playerbots", "DemonologySpecialization: Bot {} cast soul burn on target {}",
                    bot->GetName(), target->GetName());
    }
}

} // namespace Playerbot