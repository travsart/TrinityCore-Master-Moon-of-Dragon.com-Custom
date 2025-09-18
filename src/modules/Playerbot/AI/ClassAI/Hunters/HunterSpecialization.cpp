/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "HunterSpecialization.h"
#include "Player.h"
#include "Pet.h"
#include "Item.h"
#include "Log.h"

namespace Playerbot
{

HunterSpecialization::HunterSpecialization(Player* bot)
    : _bot(bot)
    , _currentAspect(0)
    , _currentTracking(0)
    , _rotationPhase(HunterRotationPhase::OPENING)
    , _lastPetCheck(0)
    , _lastTrapCheck(0)
    , _lastAspectCheck(0)
    , _lastTrackingUpdate(0)
    , _lastRangeCheck(0)
    , _lastAutoShot(0)
    , _shotsFired(0)
    , _petsLost(0)
    , _trapsPlaced(0)
    , _manaConsumed(0)
    , _totalDamageDealt(0)
{
    TC_LOG_DEBUG("playerbot", "HunterSpecialization: Base initialization for bot {}", bot->GetName());

    // Initialize pet info
    _petInfo = PetInfo();

    // Initialize aspect and tracking
    _currentAspect = 0;
    _currentTracking = 0;

    // Clear active traps
    _activeTraps.clear();

    // Initialize cooldowns
    _cooldowns.clear();

    // Set initial rotation phase
    _rotationPhase = HunterRotationPhase::OPENING;

    TC_LOG_DEBUG("playerbot", "HunterSpecialization: Base initialization complete for bot {}", bot->GetName());
}

bool HunterSpecialization::IsRangedWeaponEquipped() const
{
    if (!_bot)
        return false;

    Item* rangedWeapon = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED);
    if (!rangedWeapon)
        return false;

    uint32 weaponClass = rangedWeapon->GetTemplate()->Class;
    uint32 weaponSubclass = rangedWeapon->GetTemplate()->SubClass;

    // Check for bows, guns, crossbows
    return (weaponClass == ITEM_CLASS_WEAPON &&
            (weaponSubclass == ITEM_SUBCLASS_WEAPON_BOW ||
             weaponSubclass == ITEM_SUBCLASS_WEAPON_GUN ||
             weaponSubclass == ITEM_SUBCLASS_WEAPON_CROSSBOW));
}

bool HunterSpecialization::HasAmmo() const
{
    if (!_bot)
        return false;

    // Check for ammo in ammo slot
    Item* ammo = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_AMMO);
    return ammo && ammo->GetCount() > 0;
}

uint32 HunterSpecialization::GetAmmoCount() const
{
    if (!_bot)
        return 0;

    Item* ammo = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_AMMO);
    return ammo ? ammo->GetCount() : 0;
}

float HunterSpecialization::GetRangedAttackSpeed() const
{
    if (!_bot)
        return 2.0f;

    Item* rangedWeapon = _bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED);
    if (!rangedWeapon)
        return 2.0f;

    return rangedWeapon->GetTemplate()->Delay / 1000.0f;
}

bool HunterSpecialization::CanCastWhileMoving() const
{
    // Hunters can use instant shots while moving
    return true;
}

void HunterSpecialization::UpdateCooldown(uint32 spellId, uint32 cooldown)
{
    if (cooldown > 0)
        _cooldowns[spellId] = getMSTime() + cooldown;
    else
        _cooldowns.erase(spellId);
}

bool HunterSpecialization::IsCooldownReady(uint32 spellId) const
{
    auto it = _cooldowns.find(spellId);
    if (it == _cooldowns.end())
        return true;

    return getMSTime() >= it->second;
}

uint32 HunterSpecialization::GetSpellCooldown(uint32 spellId) const
{
    switch (spellId)
    {
        case AIMED_SHOT:
            return 6000; // 6 seconds
        case CONCUSSIVE_SHOT:
            return 8000; // 8 seconds
        case MULTI_SHOT:
            return 10000; // 10 seconds
        case RAPID_FIRE:
            return 300000; // 5 minutes
        case BESTIAL_WRATH:
            return 120000; // 2 minutes
        case INTIMIDATION:
            return 60000; // 1 minute
        case READINESS:
            return 180000; // 3 minutes
        case DETERRENCE:
            return 90000; // 1.5 minutes
        case DISENGAGE:
            return 30000; // 30 seconds
        case FEIGN_DEATH:
            return 30000; // 30 seconds
        case FREEZING_TRAP:
        case EXPLOSIVE_TRAP:
        case IMMOLATION_TRAP:
        case FROST_TRAP:
        case SNAKE_TRAP:
            return 30000; // 30 seconds
        case KILL_SHOT:
            return 10000; // 10 seconds
        case CHIMERA_SHOT:
            return 10000; // 10 seconds
        case EXPLOSIVE_SHOT:
            return 6000; // 6 seconds
        case BLACK_ARROW:
            return 30000; // 30 seconds
        case WYVERN_STING:
            return 60000; // 1 minute
        default:
            return 1500; // Default GCD
    }
}

void HunterSpecialization::SetRotationPhase(HunterRotationPhase phase)
{
    if (_rotationPhase != phase)
    {
        TC_LOG_DEBUG("playerbot", "HunterSpecialization: Phase transition for bot {} from {} to {}",
                     _bot->GetName(), static_cast<int>(_rotationPhase), static_cast<int>(phase));
        _rotationPhase = phase;
    }
}

void HunterSpecialization::UpdatePetInfo()
{
    if (!_bot)
        return;

    Pet* pet = _bot->GetPet();
    if (pet && pet->IsAlive())
    {
        _petInfo.guid = pet->GetGUID();
        _petInfo.health = pet->GetHealth();
        _petInfo.maxHealth = pet->GetMaxHealth();
        _petInfo.happiness = pet->GetHappinessState();
        _petInfo.isDead = false;

        // Determine pet type based on pet family/template
        // This is a simplified implementation
        _petInfo.type = PetType::FEROCITY; // Default to ferocity
    }
    else
    {
        _petInfo.guid = ObjectGuid::Empty;
        _petInfo.health = 0;
        _petInfo.maxHealth = 0;
        _petInfo.happiness = 0;
        _petInfo.isDead = true;
        _petInfo.type = PetType::NONE;
    }
}

bool HunterSpecialization::IsPetAlive() const
{
    return !_petInfo.isDead && _petInfo.health > 0;
}

bool HunterSpecialization::IsPetHappy() const
{
    return _petInfo.happiness >= 2; // Happy or content
}

uint32 HunterSpecialization::GetPetHappiness() const
{
    return _petInfo.happiness;
}

float HunterSpecialization::GetDistanceToTarget(::Unit* target) const
{
    if (!_bot || !target)
        return 0.0f;

    return _bot->GetDistance(target);
}

bool HunterSpecialization::IsInOptimalRange(::Unit* target) const
{
    float distance = GetDistanceToTarget(target);
    return distance >= 8.0f && distance <= OPTIMAL_RANGE;
}

bool HunterSpecialization::IsInMeleeRange(::Unit* target) const
{
    float distance = GetDistanceToTarget(target);
    return distance <= MELEE_RANGE;
}

bool HunterSpecialization::IsInRangedRange(::Unit* target) const
{
    float distance = GetDistanceToTarget(target);
    return distance >= DEAD_ZONE_MAX && distance <= RANGED_ATTACK_RANGE;
}

} // namespace Playerbot