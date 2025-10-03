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
#include "Timer.h"
#include "ObjectDefines.h"

namespace Playerbot
{

// HunterSpecialization constructor - base interface for all hunter specializations
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
}

// Utility methods implementation
bool HunterSpecialization::IsRangedWeaponEquipped() const
{
    // This is a pure interface class used by refactored template-based specializations
    // The actual implementation is in the derived classes
    return _bot != nullptr;
}

bool HunterSpecialization::HasAmmo() const
{
    // This is a pure interface class used by refactored template-based specializations
    // The actual implementation is in the derived classes
    return _bot != nullptr;
}

uint32 HunterSpecialization::GetAmmoCount() const
{
    // This is a pure interface class used by refactored template-based specializations
    // The actual implementation is in the derived classes
    return 0;
}

float HunterSpecialization::GetRangedAttackSpeed() const
{
    // This is a pure interface class used by refactored template-based specializations
    // The actual implementation is in the derived classes
    return 2.0f;
}

bool HunterSpecialization::CanCastWhileMoving() const
{
    // This is a pure interface class used by refactored template-based specializations
    // The actual implementation is in the derived classes
    return false;
}

// Common helper methods
void HunterSpecialization::UpdateCooldown(uint32 spellId, uint32 cooldown)
{
    _cooldowns[spellId] = getMSTime() + cooldown;
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
    auto it = _cooldowns.find(spellId);
    if (it == _cooldowns.end())
        return 0;
    uint32 now = getMSTime();
    return now >= it->second ? 0 : (it->second - now);
}

void HunterSpecialization::SetRotationPhase(HunterRotationPhase phase)
{
    _rotationPhase = phase;
}

// Pet helper methods
void HunterSpecialization::UpdatePetInfo()
{
    // This is a pure interface class used by refactored template-based specializations
    // The actual implementation is in the derived classes
}

bool HunterSpecialization::IsPetAlive() const
{
    return !_petInfo.isDead && _petInfo.health > 0;
}

bool HunterSpecialization::IsPetHappy() const
{
    return _petInfo.happiness >= 50; // Simple threshold
}

uint32 HunterSpecialization::GetPetHappiness() const
{
    return _petInfo.happiness;
}

// Range helper methods
float HunterSpecialization::GetDistanceToTarget(::Unit* target) const
{
    if (!_bot || !target)
        return 0.0f;
    return _bot->GetDistance(target);
}

bool HunterSpecialization::IsInOptimalRange(::Unit* target) const
{
    float distance = GetDistanceToTarget(target);
    return distance >= 8.0f && distance <= HUNTER_OPTIMAL_RANGE;
}

bool HunterSpecialization::IsInMeleeRange(::Unit* target) const
{
    return GetDistanceToTarget(target) <= MELEE_RANGE; // From ObjectDefines.h
}

bool HunterSpecialization::IsInRangedRange(::Unit* target) const
{
    float distance = GetDistanceToTarget(target);
    return distance >= HUNTER_DEAD_ZONE_MAX && distance <= HUNTER_RANGED_ATTACK_RANGE;
}

} // namespace Playerbot
