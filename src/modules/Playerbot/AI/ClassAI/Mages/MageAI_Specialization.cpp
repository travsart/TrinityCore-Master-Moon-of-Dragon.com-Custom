/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "MageAI.h"
#include "ArcaneMageRefactored.h"
#include "FireMageRefactored.h"
#include "FrostMageRefactored.h"
#include "Player.h"
#include "SpellMgr.h"
#include "Log.h"
#include "Timer.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"

namespace Playerbot
{

void MageAI::InitializeSpecialization()
{
    _currentSpec = DetectCurrentSpecialization();
    SwitchSpecialization(_currentSpec);
}

void MageAI::UpdateSpecialization()
{
    MageSpec detectedSpec = DetectCurrentSpecialization();
    if (detectedSpec != _currentSpec)
    {
        TC_LOG_DEBUG("playerbot.mage", "MageAI specialization changed from {} to {} for {}",
                     static_cast<uint32>(_currentSpec), static_cast<uint32>(detectedSpec), GetBot()->GetName());
        SwitchSpecialization(detectedSpec);
    }
}

MageSpec MageAI::DetectCurrentSpecialization()
{
    // Detect based on primary talents or highest skilled spec
    Player* bot = GetBot();
    if (!bot)
        return MageSpec::ARCANE; // Default fallback

    // Check for key specialization spells or talents
    // This is a simplified detection - real implementation would check talent trees

    // Check for Fire specialization indicators
    if (bot->HasSpell(COMBUSTION) || bot->HasSpell(PYROBLAST))
        return MageSpec::FIRE;

    // Check for Frost specialization indicators
    if (bot->HasSpell(ICY_VEINS) || bot->HasSpell(WATER_ELEMENTAL))
        return MageSpec::FROST;

    // Check for Arcane specialization indicators
    if (bot->HasSpell(ARCANE_POWER) || bot->HasSpell(ARCANE_BARRAGE))
        return MageSpec::ARCANE;

    // Default to Arcane if no clear specialization detected
    return MageSpec::ARCANE;
}

void MageAI::SwitchSpecialization(MageSpec newSpec)
{
    _currentSpec = newSpec;

    // Create appropriate specialization instance
    switch (newSpec)
    {
        case MageSpec::ARCANE:
        {
            auto arcane = new ArcaneMageRefactored(GetBot());
            _specialization = std::unique_ptr<MageSpecialization>(static_cast<Playerbot::MageSpecialization*>(arcane));
            TC_LOG_DEBUG("module.playerbot.mage", "Mage {} switched to Arcane specialization",
                         GetBot()->GetName());
            break;
        }
        case MageSpec::FIRE:
        {
            auto fire = new FireMageRefactored(GetBot());
            _specialization = std::unique_ptr<MageSpecialization>(static_cast<Playerbot::MageSpecialization*>(fire));
            TC_LOG_DEBUG("module.playerbot.mage", "Mage {} switched to Fire specialization",
                         GetBot()->GetName());
            break;
        }
        case MageSpec::FROST:
        {
            auto frost = new FrostMageRefactored(GetBot());
            _specialization = std::unique_ptr<MageSpecialization>(static_cast<Playerbot::MageSpecialization*>(frost));
            TC_LOG_DEBUG("module.playerbot.mage", "Mage {} switched to Frost specialization",
                         GetBot()->GetName());
            break;
        }
        default:
        {
            auto arcane = new ArcaneMageRefactored(GetBot());
            _specialization = std::unique_ptr<MageSpecialization>(static_cast<Playerbot::MageSpecialization*>(arcane));
            TC_LOG_DEBUG("module.playerbot.mage", "Mage {} defaulted to Arcane specialization",
                         GetBot()->GetName());
            break;
        }
    }
}

void MageAI::DelegateToSpecialization(::Unit* target)
{
    if (!_specialization)
    {
        TC_LOG_ERROR("playerbot.mage", "MageAI specialization not initialized for {}", GetBot()->GetName());
        return;
    }

    // Delegate rotation to current specialization
    _specialization->UpdateRotation(target);
}

// Override UpdateBuffs to use specialization
void MageAI::UpdateBuffs()
{
    // Update shared mage buffs
    UpdateMageBuffs();

    // Delegate specialization-specific buffs
    if (_specialization)
    {
        _specialization->UpdateBuffs();
    }
}

// Override UpdateCooldowns to use specialization
void MageAI::UpdateCooldowns(uint32 diff)
{
    // Update shared cooldowns
    ClassAI::UpdateCooldowns(diff);

    // Delegate specialization-specific cooldowns
    if (_specialization)
    {
        _specialization->UpdateCooldowns(diff);
    }
}

// Override CanUseAbility to check specialization
bool MageAI::CanUseAbility(uint32 spellId)
{
    // Check base requirements first
    if (!ClassAI::CanUseAbility(spellId))
        return false;

    // Delegate to specialization for additional checks
    if (_specialization)
    {
        return _specialization->CanUseAbility(spellId);
    }

    return true;
}

// Override OnCombatStart to notify specialization
void MageAI::OnCombatStart(::Unit* target)
{
    ClassAI::OnCombatStart(target);

    if (_specialization)
    {
        _specialization->OnCombatStart(target);
    }
}

// Override OnCombatEnd to notify specialization
void MageAI::OnCombatEnd()
{
    ClassAI::OnCombatEnd();

    if (_specialization)
    {
        _specialization->OnCombatEnd();
    }
}

// Override HasEnoughResource to use specialization
bool MageAI::HasEnoughResource(uint32 spellId)
{
    if (_specialization)
    {
        return _specialization->HasEnoughResource(spellId);
    }

    // Fallback to base mana check
    return HasEnoughMana(100); // Basic mana check
}

// Override ConsumeResource to use specialization
void MageAI::ConsumeResource(uint32 spellId)
{
    if (_specialization)
    {
        _specialization->ConsumeResource(spellId);
    }
}

// Override GetOptimalPosition to use specialization
Position MageAI::GetOptimalPosition(::Unit* target)
{
    if (_specialization)
    {
        return _specialization->GetOptimalPosition(target);
    }

    // Fallback to base position
    return GetBot()->GetPosition();
}

// Override GetOptimalRange to use specialization
float MageAI::GetOptimalRange(::Unit* target)
{
    if (_specialization)
    {
        return _specialization->GetOptimalRange(target);
    }

    // Fallback to default casting range
    return OPTIMAL_CASTING_RANGE;
}

// Override UpdateRotation to use specialization
void MageAI::UpdateRotation(::Unit* target)
{
    if (!target)
        return;

    // Update specialization if needed
    UpdateSpecialization();

    // Delegate to current specialization
    DelegateToSpecialization(target);
}

// Constructor implementation
MageAI::MageAI(Player* bot)
    : ClassAI(bot)
    , _lastPolymorph(0)
    , _lastCounterspell(0)
    , _lastBlink(0)
    , _lastManaShield(0)
    , _lastIceBarrier(0)
{
    // Initialize combat system components - skip for now to avoid constructor issues
    // _threatManager = std::make_unique<BotThreatManager>(bot);
    // _targetSelector = std::make_unique<TargetSelector>(bot);
    // _positionManager = std::make_unique<PositionManager>(bot);
    // _interruptManager = std::make_unique<InterruptManager>(bot);

    InitializeSpecialization();
    _combatMetrics.Reset();

    TC_LOG_DEBUG("playerbot.mage", "MageAI initialized for {} with specialization {}",
                 GetBot()->GetName(), static_cast<uint32>(_currentSpec));
}

// Destructor implementation
MageAI::~MageAI() = default;

// Implementation of missing methods that were in MageAI.cpp
bool MageAI::HasEnoughMana(uint32 amount)
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    return bot->GetPower(POWER_MANA) >= amount;
}

void MageAI::UpdateMageBuffs()
{
    // Update shared mage buffs like Arcane Intellect, Mage Armor
    Player* bot = GetBot();
    if (!bot || !bot->IsAlive())
        return;

    // Check for Arcane Intellect
    if (!HasAura(ARCANE_INTELLECT) && CanUseAbility(ARCANE_INTELLECT))
    {
        CastSpell(ARCANE_INTELLECT);
    }

    // Check for appropriate armor spell
    if (!HasAura(MAGE_ARMOR) && !HasAura(FROST_ARMOR) && !HasAura(MOLTEN_ARMOR))
    {
        // Choose appropriate armor based on specialization
        uint32 armorSpell = MAGE_ARMOR; // Default

        switch (_currentSpec)
        {
            case MageSpec::FIRE:
                armorSpell = MOLTEN_ARMOR;
                break;
            case MageSpec::FROST:
                armorSpell = FROST_ARMOR;
                break;
            default:
                armorSpell = MAGE_ARMOR;
                break;
        }

        if (CanUseAbility(armorSpell))
        {
            CastSpell(armorSpell);
        }
    }
}

uint32 MageAI::GetMana()
{
    Player* bot = GetBot();
    return bot ? bot->GetPower(POWER_MANA) : 0;
}

uint32 MageAI::GetMaxMana()
{
    Player* bot = GetBot();
    return bot ? bot->GetMaxPower(POWER_MANA) : 1;
}

float MageAI::GetManaPercent()
{
    uint32 maxMana = GetMaxMana();
    if (maxMana == 0)
        return 0.0f;

    return static_cast<float>(GetMana()) / static_cast<float>(maxMana);
}

bool MageAI::ShouldConserveMana()
{
    return GetManaPercent() < MANA_CONSERVATION_THRESHOLD;
}

void MageAI::UseDefensiveAbilities()
{
    Player* bot = GetBot();
    if (!bot || !bot->IsAlive())
        return;

    // Check if we're in danger and use appropriate defensive abilities
    if (IsInDanger())
    {
        // Use Blink if available and we're being attacked in melee range
        if (HasTooMuchThreat() && CanUseAbility(BLINK) &&
            (getMSTime() - _lastBlink) > BLINK_COOLDOWN)
        {
            UseBlink();
        }

        // Use Ice Barrier if we're a frost mage
        if (_currentSpec == MageSpec::FROST &&
            !HasAura(ICE_BARRIER) && CanUseAbility(ICE_BARRIER))
        {
            CastSpell(ICE_BARRIER);
        }

        // Use Mana Shield if low on health
        if (bot->GetHealthPct() < 30.0f &&
            !HasAura(MANA_SHIELD) && CanUseAbility(MANA_SHIELD))
        {
            CastSpell(MANA_SHIELD);
        }
    }
}

void MageAI::UseBlink()
{
    if (!CanUseAbility(BLINK) || (getMSTime() - _lastBlink) < BLINK_COOLDOWN)
        return;

    // Find a safe position to blink to
    Position safePos = GetSafeCastingPosition();
    if (safePos.GetExactDist(GetBot()->GetPosition()) > 5.0f)
    {
        CastSpell(BLINK);
        _lastBlink = getMSTime();
    }
}

void MageAI::FindSafeCastingPosition()
{
    // Find optimal position for casting - away from enemies but within range of targets
    Player* bot = GetBot();
    if (!bot)
        return;

    // Simple fallback positioning logic - move away from current target if too close
    if (!_currentTarget.IsEmpty())
    {
        Unit* target = ObjectAccessor::GetUnit(*bot, _currentTarget);
        if (target && bot->GetDistance(target) < MINIMUM_SAFE_RANGE)
        {
            // Move away from target - simplified approach
            float x, y, z;
            bot->GetClosePoint(x, y, z, 0.5f, OPTIMAL_CASTING_RANGE,
                              bot->GetOrientation() + M_PI);
            bot->GetMotionMaster()->MovePoint(0, x, y, z);
        }
    }
}

// Helper method for finding safe position that returns a Position
Position MageAI::GetSafeCastingPosition()
{
    Player* bot = GetBot();
    if (!bot)
        return Position();

    // Use position manager to find safe casting position - skip for now
    // if (_positionManager)
    // {
    //     return _positionManager->GetOptimalCastingPosition(OPTIMAL_CASTING_RANGE);
    // }

    // Simple fallback - current position
    return bot->GetPosition();
}

bool MageAI::IsInDanger()
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    // Check if health is low
    if (bot->GetHealthPct() < 25.0f)
        return true;

    // Check if we have too much threat
    if (HasTooMuchThreat())
        return true;

    // Simplified danger check - assume danger if health is low
    return false; // Skip complex hostile unit checking for now
}

bool MageAI::HasTooMuchThreat()
{
    // Simple threat check fallback
    // if (_threatManager)
    // {
    //     return _threatManager->HasHighThreat();
    // }

    // Simple fallback threat check
    Player* bot = GetBot();
    if (!bot || _currentTarget.IsEmpty())
        return false;

    Unit* target = ObjectAccessor::GetUnit(*bot, _currentTarget);
    if (!target)
        return false;

    return target->GetTarget() == bot->GetGUID();
}

MageSpec MageAI::DetectSpecialization()
{
    return DetectCurrentSpecialization();
}

void MageAI::OptimizeForSpecialization()
{
    // Optimization logic based on current specialization - skip for now
    // if (_specialization)
    // {
    //     _specialization->OptimizeRotation();
    // }
}

bool MageAI::HasTalent(uint32 talentId)
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    // Check if player has specific talent - this would need to be implemented
    // based on TrinityCore's talent system for the target version
    return bot->HasSpell(talentId);
}

void MageAI::ManageThreat()
{
    if (HasTooMuchThreat())
    {
        ReduceThreat();
    }
}

void MageAI::ReduceThreat()
{
    // Use threat reduction abilities
    if (CanUseAbility(INVISIBILITY))
    {
        CastSpell(INVISIBILITY);
    }
    else if (CanUseAbility(BLINK))
    {
        UseBlink();
    }
}

// Spell school mappings implementation
const std::unordered_map<uint32, MageSchool> MageAI::_spellSchools = {
    // Arcane spells
    {ARCANE_MISSILES, MageSchool::ARCANE},
    {ARCANE_BLAST, MageSchool::ARCANE},
    {ARCANE_BARRAGE, MageSchool::ARCANE},
    {ARCANE_POWER, MageSchool::ARCANE},
    {ARCANE_EXPLOSION, MageSchool::ARCANE},

    // Fire spells
    {FIREBALL, MageSchool::FIRE},
    {FIRE_BLAST, MageSchool::FIRE},
    {PYROBLAST, MageSchool::FIRE},
    {FLAMESTRIKE, MageSchool::FIRE},
    {SCORCH, MageSchool::FIRE},
    {COMBUSTION, MageSchool::FIRE},

    // Frost spells
    {FROSTBOLT, MageSchool::FROST},
    {ICE_LANCE, MageSchool::FROST},
    {FROZEN_ORB, MageSchool::FROST},
    {BLIZZARD, MageSchool::FROST},
    {CONE_OF_COLD, MageSchool::FROST},
    {ICY_VEINS, MageSchool::FROST}
};

} // namespace Playerbot