/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "DruidSpecialization.h"
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

// Form management methods
void DruidSpecialization::CastShapeshift(DruidForm form)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Don't shift if already in the form
    if (IsInForm(form))
        return;

    // Check cooldown
    uint32 now = getMSTime();
    if (now - _lastFormShift < 1500) // 1.5 second global cooldown
        return;

    uint32 shiftSpell = 0;
    switch (form)
    {
        case DruidForm::BEAR:
            shiftSpell = BEAR_FORM;
            break;
        case DruidForm::CAT:
            shiftSpell = CAT_FORM;
            break;
        case DruidForm::AQUATIC:
            shiftSpell = AQUATIC_FORM;
            break;
        case DruidForm::TRAVEL:
            shiftSpell = TRAVEL_FORM;
            break;
        case DruidForm::MOONKIN:
            shiftSpell = MOONKIN_FORM;
            break;
        case DruidForm::TREE_OF_LIFE:
            shiftSpell = TREE_OF_LIFE;
            break;
        case DruidForm::FLIGHT:
            shiftSpell = FLIGHT_FORM;
            break;
        case DruidForm::HUMANOID:
            // Cancel current shapeshift to return to humanoid form
            bot->RemoveAurasByType(SPELL_AURA_MOD_SHAPESHIFT);
            _previousForm = _currentForm;
            _currentForm = DruidForm::HUMANOID;
            _lastFormShift = now;
            TC_LOG_DEBUG("playerbots", "DruidSpecialization: Bot {} shifted to humanoid form",
                        bot->GetName());
            return;
        default:
            return;
    }

    // Check if spell is known
    if (!bot->HasSpell(shiftSpell))
        return;

    // Store transition info
    _formTransition.fromForm = _currentForm;
    _formTransition.toForm = form;
    _formTransition.lastTransition = now;
    _formTransition.inProgress = true;

    if (bot->CastSpell(bot, shiftSpell, false))
    {
        _previousForm = _currentForm;
        _currentForm = form;
        _lastFormShift = now;
        _formTransition.inProgress = false;

        TC_LOG_DEBUG("playerbots", "DruidSpecialization: Bot {} shifted to form {}",
                    bot->GetName(), static_cast<uint32>(form));
    }
    else
    {
        _formTransition.inProgress = false;
    }
}

bool DruidSpecialization::IsInForm(DruidForm form)
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    switch (form)
    {
        case DruidForm::BEAR:
            return bot->HasAura(BEAR_FORM);
        case DruidForm::CAT:
            return bot->HasAura(CAT_FORM);
        case DruidForm::AQUATIC:
            return bot->HasAura(AQUATIC_FORM);
        case DruidForm::TRAVEL:
            return bot->HasAura(TRAVEL_FORM);
        case DruidForm::MOONKIN:
            return bot->HasAura(MOONKIN_FORM);
        case DruidForm::TREE_OF_LIFE:
            return bot->HasAura(TREE_OF_LIFE);
        case DruidForm::FLIGHT:
            return bot->HasAura(FLIGHT_FORM);
        case DruidForm::HUMANOID:
            return !bot->HasAuraType(SPELL_AURA_MOD_SHAPESHIFT);
        default:
            return false;
    }
}

bool DruidSpecialization::CanCastInCurrentForm(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return false;

    // Check if current form allows this spell
    ShapeshiftForm currentShapeshift = bot->GetShapeshiftForm();

    // Most spells can be cast in humanoid form
    if (currentShapeshift == FORM_NONE)
        return true;

    // Check specific form restrictions
    switch (spellId)
    {
        // Spells that can be cast in bear form
        case 5487:  // Bear Form abilities
        case 6807:  // Maul
        case 779:   // Swipe
        case 22812: // Barkskin
            return currentShapeshift == FORM_BEAR || currentShapeshift == FORM_NONE;

        // Spells that can be cast in cat form
        case 768:   // Cat Form abilities
        case 1082:  // Claw
        case 1079:  // Rip
        case 5221:  // Shred
        case 22812: // Barkskin
            return currentShapeshift == FORM_CAT || currentShapeshift == FORM_NONE;

        // Spells that can be cast in moonkin form
        case 24858: // Moonkin Form abilities
        case 8921:  // Moonfire
        case 5176:  // Wrath
        case 2912:  // Starfire
            return currentShapeshift == FORM_MOONKIN || currentShapeshift == FORM_NONE;

        // Spells that can be cast in tree form
        case 33891: // Tree of Life abilities
        case 774:   // Rejuvenation
        case 5185:  // Healing Touch
        case 33763: // Lifebloom
            return currentShapeshift == FORM_TREE || currentShapeshift == FORM_NONE;

        default:
            // Most other spells require humanoid form
            return currentShapeshift == FORM_NONE;
    }
}

void DruidSpecialization::ApplyDoT(Unit* target, uint32 spellId)
{
    Player* bot = GetBot();
    if (!target || !bot)
        return;

    if (!CanCastInCurrentForm(spellId))
        return;

    if (target->HasAura(spellId, bot->GetGUID()))
        return; // Already has our DoT

    if (bot->CastSpell(target, spellId, false))
    {
        uint32 now = getMSTime();
        _moonfireTimers[target->GetGUID()] = now + 12000; // Moonfire lasts 12 seconds

        TC_LOG_DEBUG("playerbots", "DruidSpecialization: Bot {} applied DoT {} to target {}",
                    bot->GetName(), spellId, target->GetName());
    }
}

void DruidSpecialization::ApplyHoT(Unit* target, uint32 spellId)
{
    Player* bot = GetBot();
    if (!target || !bot)
        return;

    if (!CanCastInCurrentForm(spellId))
        return;

    if (target->HasAura(spellId, bot->GetGUID()))
        return; // Already has our HoT

    if (bot->CastSpell(target, spellId, false))
    {
        uint32 now = getMSTime();

        // Track different HoT durations
        switch (spellId)
        {
            case REJUVENATION:
                _rejuvenationTimers[target->GetGUID()] = now + 12000; // 12 seconds
                break;
            case LIFEBLOOM:
                _lifebloomTimers[target->GetGUID()] = now + 7000; // 7 seconds
                break;
        }

        TC_LOG_DEBUG("playerbots", "DruidSpecialization: Bot {} applied HoT {} to target {}",
                    bot->GetName(), spellId, target->GetName());
    }
}

uint32 DruidSpecialization::GetDoTRemainingTime(Unit* target, uint32 spellId)
{
    if (!target)
        return 0;

    if (Aura* aura = target->GetAura(spellId, GetBot()->GetGUID()))
    {
        return aura->GetDuration();
    }

    return 0;
}

uint32 DruidSpecialization::GetHoTRemainingTime(Unit* target, uint32 spellId)
{
    if (!target)
        return 0;

    if (Aura* aura = target->GetAura(spellId, GetBot()->GetGUID()))
    {
        return aura->GetDuration();
    }

    return 0;
}

// Resource management helpers
uint32 DruidSpecialization::GetMana()
{
    Player* bot = GetBot();
    if (!bot)
        return 0;

    return bot->GetPower(POWER_MANA);
}

uint32 DruidSpecialization::GetMaxMana()
{
    Player* bot = GetBot();
    if (!bot)
        return 0;

    return bot->GetMaxPower(POWER_MANA);
}

float DruidSpecialization::GetManaPercent()
{
    uint32 maxMana = GetMaxMana();
    if (maxMana == 0)
        return 0.0f;

    return static_cast<float>(GetMana()) / static_cast<float>(maxMana) * 100.0f;
}

uint32 DruidSpecialization::GetEnergy()
{
    Player* bot = GetBot();
    if (!bot)
        return 0;

    return bot->GetPower(POWER_ENERGY);
}

uint32 DruidSpecialization::GetMaxEnergy()
{
    Player* bot = GetBot();
    if (!bot)
        return 0;

    return bot->GetMaxPower(POWER_ENERGY);
}

float DruidSpecialization::GetEnergyPercent()
{
    uint32 maxEnergy = GetMaxEnergy();
    if (maxEnergy == 0)
        return 0.0f;

    return static_cast<float>(GetEnergy()) / static_cast<float>(maxEnergy) * 100.0f;
}

uint32 DruidSpecialization::GetRage()
{
    Player* bot = GetBot();
    if (!bot)
        return 0;

    return bot->GetPower(POWER_RAGE) / 10; // Rage is stored in 10ths
}

uint32 DruidSpecialization::GetMaxRage()
{
    Player* bot = GetBot();
    if (!bot)
        return 0;

    return bot->GetMaxPower(POWER_RAGE) / 10; // Rage is stored in 10ths
}

float DruidSpecialization::GetRagePercent()
{
    uint32 maxRage = GetMaxRage();
    if (maxRage == 0)
        return 0.0f;

    return static_cast<float>(GetRage()) / static_cast<float>(maxRage) * 100.0f;
}

// Combat state helpers
bool DruidSpecialization::IsChanneling()
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    return bot->HasUnitState(UNIT_STATE_CASTING);
}

bool DruidSpecialization::IsCasting()
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    return bot->HasUnitState(UNIT_STATE_CASTING);
}

bool DruidSpecialization::CanCast()
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    return !bot->HasUnitState(UNIT_STATE_CASTING | UNIT_STATE_STUNNED |
                              UNIT_STATE_CONFUSED | UNIT_STATE_FLEEING |
                              UNIT_STATE_SILENCED);
}

bool DruidSpecialization::IsInDanger()
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    // Check health threshold
    if (bot->GetHealthPct() < 30.0f)
        return true;

    // Check for multiple attackers
    std::list<Unit*> attackers;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(bot, bot, 15.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, attackers, u_check);
    Cell::VisitAllObjects(bot, searcher, 15.0f);

    if (attackers.size() >= 3)
        return true;

    // Check for strong enemies
    for (Unit* attacker : attackers)
    {
        if (attacker && attacker->GetLevel() > bot->GetLevel() + 2)
            return true;
    }

    return false;
}

// Buff management
void DruidSpecialization::UpdateMarkOfTheWild()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (!bot->HasSpell(MARK_OF_THE_WILD))
        return;

    // Cast on self if not active
    if (!bot->HasAura(MARK_OF_THE_WILD))
    {
        if (bot->CastSpell(bot, MARK_OF_THE_WILD, false))
        {
            TC_LOG_DEBUG("playerbots", "DruidSpecialization: Bot {} cast mark of the wild on self",
                        bot->GetName());
        }
    }

    // Cast on group members if in a group
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (!member || !member->IsAlive())
                continue;

            if (member->GetDistance2d(bot) > 30.0f)
                continue;

            if (!member->HasAura(MARK_OF_THE_WILD))
            {
                if (bot->CastSpell(member, MARK_OF_THE_WILD, false))
                {
                    TC_LOG_DEBUG("playerbots", "DruidSpecialization: Bot {} cast mark of the wild on {}",
                                bot->GetName(), member->GetName());
                    break; // Only cast one per update
                }
            }
        }
    }
}

void DruidSpecialization::UpdateThorns()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (!bot->HasSpell(THORNS))
        return;

    // Don't cast thorns on casters (it doesn't help much)
    if (IsInForm(DruidForm::MOONKIN) || IsInForm(DruidForm::TREE_OF_LIFE) ||
        IsInForm(DruidForm::HUMANOID))
        return;

    // Cast on self if not active and in melee form
    if (!bot->HasAura(THORNS) && (IsInForm(DruidForm::BEAR) || IsInForm(DruidForm::CAT)))
    {
        if (bot->CastSpell(bot, THORNS, false))
        {
            TC_LOG_DEBUG("playerbots", "DruidSpecialization: Bot {} cast thorns on self",
                        bot->GetName());
        }
    }
}

void DruidSpecialization::UpdateOmenOfClarity()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (!bot->HasSpell(OMEN_OF_CLARITY))
        return;

    // Cast if not active
    if (!bot->HasAura(OMEN_OF_CLARITY))
    {
        if (bot->CastSpell(bot, OMEN_OF_CLARITY, false))
        {
            TC_LOG_DEBUG("playerbots", "DruidSpecialization: Bot {} cast omen of clarity",
                        bot->GetName());
        }
    }
}

// Crowd control and utility
void DruidSpecialization::CastEntanglingRoots(Unit* target)
{
    Player* bot = GetBot();
    if (!target || !bot)
        return;

    if (!bot->HasSpell(ENTANGLING_ROOTS))
        return;

    if (target->HasAura(ENTANGLING_ROOTS))
        return;

    // Don't root in groups to avoid pulling additional mobs
    if (bot->GetGroup())
        return;

    // Only use in humanoid form or moonkin form
    if (!IsInForm(DruidForm::HUMANOID) && !IsInForm(DruidForm::MOONKIN))
        return;

    if (!CanCastInCurrentForm(ENTANGLING_ROOTS))
        return;

    if (bot->CastSpell(target, ENTANGLING_ROOTS, false))
    {
        TC_LOG_DEBUG("playerbots", "DruidSpecialization: Bot {} cast entangling roots on target {}",
                    bot->GetName(), target->GetName());
    }
}

void DruidSpecialization::CastCyclone(Unit* target)
{
    Player* bot = GetBot();
    if (!target || !bot)
        return;

    if (!bot->HasSpell(CYCLONE))
        return;

    if (target->HasAura(CYCLONE))
        return;

    // Don't cyclone current target if we're in single combat
    if (bot->GetSelectedUnit() == target)
    {
        // Check if there are multiple enemies
        std::list<Unit*> enemies;
        Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(bot, bot, 30.0f);
        Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, enemies, u_check);
        Cell::VisitAllObjects(bot, searcher, 30.0f);

        if (enemies.size() < 2)
            return; // Don't cyclone if only one enemy
    }

    if (!CanCastInCurrentForm(CYCLONE))
        return;

    if (bot->CastSpell(target, CYCLONE, false))
    {
        TC_LOG_DEBUG("playerbots", "DruidSpecialization: Bot {} cast cyclone on target {}",
                    bot->GetName(), target->GetName());
    }
}

void DruidSpecialization::CastHibernate(Unit* target)
{
    Player* bot = GetBot();
    if (!target || !bot)
        return;

    if (!bot->HasSpell(HIBERNATE))
        return;

    if (target->HasAura(HIBERNATE))
        return;

    // Only use on beasts and dragonkin
    if (target->GetCreatureType() != CREATURE_TYPE_BEAST &&
        target->GetCreatureType() != CREATURE_TYPE_DRAGONKIN)
        return;

    if (!CanCastInCurrentForm(HIBERNATE))
        return;

    if (bot->CastSpell(target, HIBERNATE, false))
    {
        TC_LOG_DEBUG("playerbots", "DruidSpecialization: Bot {} cast hibernate on target {}",
                    bot->GetName(), target->GetName());
    }
}

// Defensive abilities
void DruidSpecialization::CastBarkskin()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    if (!bot->HasSpell(BARKSKIN))
        return;

    if (bot->HasSpellCooldown(BARKSKIN))
        return;

    if (bot->HasAura(BARKSKIN))
        return;

    // Use when health is low or under heavy attack
    if (bot->GetHealthPct() < 40.0f || IsInDanger())
    {
        if (bot->CastSpell(bot, BARKSKIN, false))
        {
            TC_LOG_DEBUG("playerbots", "DruidSpecialization: Bot {} cast barkskin",
                        bot->GetName());
        }
    }
}

// Healing utilities
void DruidSpecialization::CastEmergencyHeal(Unit* target)
{
    Player* bot = GetBot();
    if (!target || !bot)
        return;

    if (target->GetHealthPct() > 50.0f)
        return; // Not an emergency

    // Try to shift to form that can heal if needed
    if (!CanCastInCurrentForm(HEALING_TOUCH))
    {
        if (IsInForm(DruidForm::CAT) || IsInForm(DruidForm::BEAR))
        {
            CastShapeshift(DruidForm::HUMANOID);
            return; // Will cast heal next update
        }
    }

    // Use fastest heal available
    if (bot->HasSpell(HEALING_TOUCH) && CanCastInCurrentForm(HEALING_TOUCH))
    {
        if (bot->CastSpell(target, HEALING_TOUCH, false))
        {
            TC_LOG_DEBUG("playerbots", "DruidSpecialization: Bot {} cast emergency healing touch on {}",
                        bot->GetName(), target->GetName());
        }
    }
}

// Form transition helpers
bool DruidSpecialization::CanShiftToForm(DruidForm form)
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    uint32 now = getMSTime();
    if (now - _lastFormShift < 1500) // Global cooldown
        return false;

    if (_formTransition.inProgress)
        return false;

    uint32 shiftSpell = 0;
    switch (form)
    {
        case DruidForm::BEAR:
            shiftSpell = BEAR_FORM;
            break;
        case DruidForm::CAT:
            shiftSpell = CAT_FORM;
            break;
        case DruidForm::TRAVEL:
            shiftSpell = TRAVEL_FORM;
            break;
        case DruidForm::MOONKIN:
            shiftSpell = MOONKIN_FORM;
            break;
        case DruidForm::TREE_OF_LIFE:
            shiftSpell = TREE_OF_LIFE;
            break;
        case DruidForm::HUMANOID:
            return true; // Can always shift to humanoid
        default:
            return false;
    }

    return bot->HasSpell(shiftSpell);
}

DruidForm DruidSpecialization::GetCurrentForm()
{
    Player* bot = GetBot();
    if (!bot)
        return DruidForm::HUMANOID;

    if (bot->HasAura(BEAR_FORM))
        return DruidForm::BEAR;
    else if (bot->HasAura(CAT_FORM))
        return DruidForm::CAT;
    else if (bot->HasAura(AQUATIC_FORM))
        return DruidForm::AQUATIC;
    else if (bot->HasAura(TRAVEL_FORM))
        return DruidForm::TRAVEL;
    else if (bot->HasAura(MOONKIN_FORM))
        return DruidForm::MOONKIN;
    else if (bot->HasAura(TREE_OF_LIFE))
        return DruidForm::TREE_OF_LIFE;
    else if (bot->HasAura(FLIGHT_FORM))
        return DruidForm::FLIGHT;

    return DruidForm::HUMANOID;
}

void DruidSpecialization::UpdateCurrentForm()
{
    _currentForm = GetCurrentForm();
}

// Position helpers
Position DruidSpecialization::GetOptimalCastingPosition(Unit* target)
{
    Player* bot = GetBot();
    if (!target || !bot)
        return bot->GetPosition();

    Position currentPos = bot->GetPosition();
    Position targetPos = target->GetPosition();

    // Stay at optimal casting range for caster forms
    if (IsInForm(DruidForm::MOONKIN) || IsInForm(DruidForm::HUMANOID) || IsInForm(DruidForm::TREE_OF_LIFE))
    {
        float optimalRange = 28.0f; // Most druid spells have 30 yard range
        float currentDistance = bot->GetDistance2d(target);

        if (currentDistance > optimalRange + 5.0f)
        {
            // Move closer
            float angle = targetPos.GetAngle(currentPos);
            Position newPos = targetPos;
            newPos.m_positionX += cos(angle) * optimalRange;
            newPos.m_positionY += sin(angle) * optimalRange;
            return newPos;
        }
        else if (currentDistance < 15.0f)
        {
            // Move further away
            float angle = currentPos.GetAngle(targetPos);
            Position newPos = currentPos;
            newPos.m_positionX += cos(angle) * 10.0f;
            newPos.m_positionY += sin(angle) * 10.0f;
            return newPos;
        }
    }
    // For melee forms, stay close
    else if (IsInForm(DruidForm::BEAR) || IsInForm(DruidForm::CAT))
    {
        float meleeRange = 5.0f;
        float currentDistance = bot->GetDistance2d(target);

        if (currentDistance > meleeRange + 2.0f)
        {
            // Move closer
            float angle = targetPos.GetAngle(currentPos);
            Position newPos = targetPos;
            newPos.m_positionX += cos(angle) * meleeRange;
            newPos.m_positionY += sin(angle) * meleeRange;
            return newPos;
        }
    }

    return currentPos;
}

bool DruidSpecialization::IsInMeleeRange(Unit* target)
{
    Player* bot = GetBot();
    if (!target || !bot)
        return false;

    float distance = bot->GetDistance2d(target);
    return distance <= 5.0f;
}

bool DruidSpecialization::IsInCastingRange(Unit* target, uint32 spellId)
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

} // namespace Playerbot