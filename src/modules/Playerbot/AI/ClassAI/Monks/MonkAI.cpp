/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "MonkAI.h"
#include "Player.h"
#include "SpellMgr.h"
#include "Log.h"
#include "../BaselineRotationManager.h"
#include "../../Combat/CombatBehaviorIntegration.h"
#include "SpellAuras.h"
#include "Unit.h"
#include "CellImpl.h"
#include "GridNotifiersImpl.h"

namespace Playerbot
{

MonkAI::MonkAI(Player* bot) : ClassAI(bot), _currentSpec(MonkSpec::WINDWALKER)
{
    TC_LOG_DEBUG("playerbots", "MonkAI initialized for player {}", bot->GetName());
}

void MonkAI::UpdateRotation(::Unit* target)
{
    if (!target || !GetBot())
        return;

    // Check if bot should use baseline rotation (levels 1-9 or no spec)
    if (BaselineRotationManager::ShouldUseBaselineRotation(GetBot()))
    {
        static BaselineRotationManager baselineManager;
        baselineManager.HandleAutoSpecialization(GetBot());

        if (baselineManager.ExecuteBaselineRotation(GetBot(), target))
            return;

        // Fallback: basic auto-attack
        if (!GetBot()->IsNonMeleeSpellCast(false))
        {
            if (GetBot()->GetDistance(target) <= 5.0f)
            {
                GetBot()->AttackerStateUpdate(target);
            }
        }
        return;
    }

    // ========================================================================
    // COMBAT BEHAVIOR INTEGRATION - Priority-based decision making for Monks
    // ========================================================================
    auto* behaviors = GetCombatBehaviors();

    // Priority 1: Handle interrupts (Spear Hand Strike)
    if (behaviors && behaviors->ShouldInterrupt(target))
    {
        Unit* interruptTarget = behaviors->GetInterruptTarget();
        if (interruptTarget && CanUseAbility(SPEAR_HAND_STRIKE))
        {
            // Spear Hand Strike is melee range interrupt
            if (GetBot()->GetDistance(interruptTarget) <= OPTIMAL_KICK_RANGE)
            {
                if (CastSpell(interruptTarget, SPEAR_HAND_STRIKE))
                {
                    RecordInterruptAttempt(interruptTarget, SPEAR_HAND_STRIKE, true);
                    TC_LOG_DEBUG("module.playerbot.ai", "Monk {} interrupted {} with Spear Hand Strike",
                                 GetBot()->GetName(), interruptTarget->GetName());
                    return;
                }
            }
        }

        // Paralysis as ranged interrupt for casters (if talented)
        if (interruptTarget && CanUseAbility(PARALYSIS))
        {
            if (GetBot()->GetDistance(interruptTarget) <= 20.0f)
            {
                if (CastSpell(interruptTarget, PARALYSIS))
                {
                    RecordInterruptAttempt(interruptTarget, PARALYSIS, true);
                    TC_LOG_DEBUG("module.playerbot.ai", "Monk {} paralyzed {} to interrupt",
                                 GetBot()->GetName(), interruptTarget->GetName());
                    return;
                }
            }
        }
    }

    // Priority 2: Handle defensive abilities based on spec
    if (behaviors && behaviors->NeedsDefensive())
    {
        UseDefensiveCooldowns();
        if (GetBot()->HasUnitState(UNIT_STATE_CASTING))
            return;
    }

    // Priority 3: Check for target switching
    if (behaviors && behaviors->ShouldSwitchTarget())
    {
        Unit* priorityTarget = behaviors->GetPriorityTarget();
        if (priorityTarget && priorityTarget != target)
        {
            OnTargetChanged(priorityTarget);
            target = priorityTarget;
            TC_LOG_DEBUG("module.playerbot.ai", "Monk {} switching target to {}",
                         GetBot()->GetName(), priorityTarget->GetName());
        }
    }

    // Priority 4: AoE vs Single-Target decision
    if (behaviors && behaviors->ShouldAOE())
    {
        // Spinning Crane Kick for AoE damage
        if (CanUseAbility(SPINNING_CRANE_KICK))
        {
            // Check Chi requirement
            if (HasEnoughChi(2))
            {
                if (CastSpell(SPINNING_CRANE_KICK))
                {
                    ConsumeChiForAbility(SPINNING_CRANE_KICK, 2);
                    RecordAbilityUsage(SPINNING_CRANE_KICK);
                    TC_LOG_DEBUG("module.playerbot.ai", "Monk {} using Spinning Crane Kick for AoE",
                                 GetBot()->GetName());
                    return;
                }
            }
        }

        // Rushing Jade Wind for sustained AoE (if talented)
        if (CanUseAbility(RUSHING_JADE_WIND))
        {
            if (HasEnoughChi(1))
            {
                if (CastSpell(RUSHING_JADE_WIND))
                {
                    ConsumeChiForAbility(RUSHING_JADE_WIND, 1);
                    RecordAbilityUsage(RUSHING_JADE_WIND);
                    TC_LOG_DEBUG("module.playerbot.ai", "Monk {} activated Rushing Jade Wind",
                                 GetBot()->GetName());
                    return;
                }
            }
        }

        // Fists of Fury for Windwalker AoE burst
        if (_currentSpec == MonkSpec::WINDWALKER && CanUseAbility(FISTS_OF_FURY))
        {
            if (HasEnoughChi(3))
            {
                if (CastSpell(target, FISTS_OF_FURY))
                {
                    ConsumeChiForAbility(FISTS_OF_FURY, 3);
                    RecordAbilityUsage(FISTS_OF_FURY);
                    TC_LOG_DEBUG("module.playerbot.ai", "Monk {} channeling Fists of Fury",
                                 GetBot()->GetName());
                    return;
                }
            }
        }

        // Keg Smash for Brewmaster AoE threat
        if (_currentSpec == MonkSpec::BREWMASTER && CanUseAbility(KEG_SMASH))
        {
            if (HasEnoughEnergy(40))
            {
                if (CastSpell(target, KEG_SMASH))
                {
                    ConsumeEnergyForAbility(KEG_SMASH, 40);
                    GenerateChi(2); // Keg Smash generates Chi
                    RecordAbilityUsage(KEG_SMASH);
                    TC_LOG_DEBUG("module.playerbot.ai", "Monk {} using Keg Smash for AoE threat",
                                 GetBot()->GetName());
                    return;
                }
            }
        }
    }

    // Priority 5: Use major offensive cooldowns at optimal time
    if (behaviors && behaviors->ShouldUseCooldowns())
    {
        // Windwalker cooldowns
        if (_currentSpec == MonkSpec::WINDWALKER)
        {
            // Storm, Earth, and Fire for cleave/burst
            if (CanUseAbility(STORM_EARTH_AND_FIRE))
            {
                if (CastSpell(STORM_EARTH_AND_FIRE))
                {
                    RecordAbilityUsage(STORM_EARTH_AND_FIRE);
                    TC_LOG_DEBUG("module.playerbot.ai", "Monk {} activated Storm, Earth, and Fire",
                                 GetBot()->GetName());
                }
            }

            // Serenity for single target burst (if talented)
            if (CanUseAbility(SERENITY))
            {
                if (CastSpell(SERENITY))
                {
                    RecordAbilityUsage(SERENITY);
                    TC_LOG_DEBUG("module.playerbot.ai", "Monk {} entered Serenity",
                                 GetBot()->GetName());
                }
            }

            // Touch of Death execute
            if (target->GetHealthPct() <= TOUCH_OF_DEATH_THRESHOLD && CanUseAbility(TOUCH_OF_DEATH))
            {
                if (HasEnoughChi(3))
                {
                    if (CastSpell(target, TOUCH_OF_DEATH))
                    {
                        ConsumeChiForAbility(TOUCH_OF_DEATH, 3);
                        RecordAbilityUsage(TOUCH_OF_DEATH);
                        TC_LOG_DEBUG("module.playerbot.ai", "Monk {} executed Touch of Death",
                                     GetBot()->GetName());
                        return;
                    }
                }
            }
        }

        // Brewmaster cooldowns
        if (_currentSpec == MonkSpec::BREWMASTER)
        {
            // Fortifying Brew for major mitigation
            if (GetBot()->GetHealthPct() < 60.0f && CanUseAbility(FORTIFYING_BREW))
            {
                if (CastSpell(FORTIFYING_BREW))
                {
                    RecordAbilityUsage(FORTIFYING_BREW);
                    TC_LOG_DEBUG("module.playerbot.ai", "Monk {} activated Fortifying Brew",
                                 GetBot()->GetName());
                }
            }
        }

        // Mistweaver cooldowns
        if (_currentSpec == MonkSpec::MISTWEAVER)
        {
            // Revival for raid healing
            if (CanUseAbility(REVIVAL))
            {
                // Check if multiple allies need healing
                uint32 injuredAllies = GetNearbyInjuredAlliesCount(30.0f, 50.0f);
                if (injuredAllies >= 3)
                {
                    if (CastSpell(REVIVAL))
                    {
                        RecordAbilityUsage(REVIVAL);
                        TC_LOG_DEBUG("module.playerbot.ai", "Monk {} cast Revival for group healing",
                                     GetBot()->GetName());
                        return;
                    }
                }
            }
        }
    }

    // Priority 6: Chi and Energy Management
    ManageResourceGeneration(target);

    // Priority 7: Check positioning requirements and mobility
    if (behaviors && behaviors->NeedsRepositioning())
    {
        Position optimalPos = behaviors->GetOptimalPosition();
        HandleMobilityAbilities(target, optimalPos);
    }

    // Priority 8: Execute specialization-specific rotation
    switch (_currentSpec)
    {
        case MonkSpec::WINDWALKER:
            ExecuteWindwalkerRotation(target);
            break;
        case MonkSpec::BREWMASTER:
            ExecuteBrewmasterRotation(target);
            break;
        case MonkSpec::MISTWEAVER:
            ExecuteMistweaverRotation(target);
            break;
    }

    // Update advanced combat logic
    UpdateAdvancedCombatLogic(target);
}

void MonkAI::UpdateBuffs()
{
    if (!GetBot())
        return;

    // Use baseline buffs for low-level bots
    if (BaselineRotationManager::ShouldUseBaselineRotation(GetBot()))
    {
        static BaselineRotationManager baselineManager;
        baselineManager.ApplyBaselineBuffs(GetBot());
        return;
    }

    // Specialized buff management for level 10+ with spec
    // TODO: Implement monk buff management
}

void MonkAI::UpdateCooldowns(uint32 diff)
{
    // Basic cooldown management - placeholder implementation
    // TODO: Implement monk cooldown management
}

bool MonkAI::CanUseAbility(uint32 spellId)
{
    // Basic ability check - placeholder implementation
    return GetBot() && GetBot()->HasSpell(spellId);
}

void MonkAI::OnCombatStart(::Unit* target)
{
    // Combat start logic - placeholder implementation
    ClassAI::OnCombatStart(target);
}

void MonkAI::OnCombatEnd()
{
    // Combat end logic - placeholder implementation
    ClassAI::OnCombatEnd();
}

bool MonkAI::HasEnoughResource(uint32 spellId)
{
    // Resource check - placeholder implementation
    return true; // TODO: Implement proper resource checking
}

void MonkAI::ConsumeResource(uint32 spellId)
{
    // Resource consumption - placeholder implementation
    // TODO: Implement proper resource consumption
}

Position MonkAI::GetOptimalPosition(::Unit* target)
{
    if (!target)
        return GetBot()->GetPosition();

    // Basic positioning - stay in melee range
    return target->GetNearPosition(3.0f, target->GetRelativeAngle(GetBot()));
}

float MonkAI::GetOptimalRange(::Unit* target)
{
    // Basic range - melee range for monks
    return 5.0f;
}

// Initialize specialization based on talent inspection
void MonkAI::InitializeSpecialization()
{
    _currentSpec = DetectCurrentSpecialization();
    TC_LOG_DEBUG("module.playerbot.ai", "Monk {} initialized with spec: {}",
                 GetBot()->GetName(), static_cast<uint8>(_currentSpec));
}

// Detect current specialization from talents
MonkSpec MonkAI::DetectCurrentSpecialization()
{
    // Check for key talents to determine spec
    // For now, default to Windwalker as DPS spec
    // TODO: Implement proper talent inspection
    return MonkSpec::WINDWALKER;
}

// Resource management methods
bool MonkAI::HasEnoughChi(uint32 amount) const
{
    return _chiManager.current.load() >= amount;
}

bool MonkAI::HasEnoughEnergy(uint32 amount) const
{
    return GetBot()->GetPower(POWER_ENERGY) >= amount;
}

void MonkAI::ConsumeChiForAbility(uint32 spellId, uint32 amount)
{
    _chiManager.ConsumeChi(amount);
    _comboTracker.RecordAbility(spellId);
    TC_LOG_DEBUG("module.playerbot.ai", "Monk {} consumed {} Chi for spell {}",
                 GetBot()->GetName(), amount, spellId);
}

void MonkAI::ConsumeEnergyForAbility(uint32 spellId, uint32 amount)
{
    _energyManager.SpendEnergy(amount);
    GetBot()->ModifyPower(POWER_ENERGY, -static_cast<int32>(amount));
    TC_LOG_DEBUG("module.playerbot.ai", "Monk {} spent {} Energy for spell {}",
                 GetBot()->GetName(), amount, spellId);
}

void MonkAI::GenerateChi(uint32 amount)
{
    _chiManager.GenerateChi(amount);
    TC_LOG_DEBUG("module.playerbot.ai", "Monk {} generated {} Chi",
                 GetBot()->GetName(), amount);
}

// Resource generation management
void MonkAI::ManageResourceGeneration(::Unit* target)
{
    if (!target || !GetBot())
        return;

    // Generate Chi with Tiger Palm
    if (CanUseAbility(TIGER_PALM) && HasEnoughEnergy(50))
    {
        if (_chiManager.current.load() < _chiManager.maximum.load() - 1)
        {
            if (CastSpell(target, TIGER_PALM))
            {
                ConsumeEnergyForAbility(TIGER_PALM, 50);
                GenerateChi(2);
                RecordAbilityUsage(TIGER_PALM);
                return;
            }
        }
    }

    // Expel Harm for Chi and healing
    if (CanUseAbility(EXPEL_HARM) && HasEnoughEnergy(15))
    {
        if (GetBot()->GetHealthPct() < 80.0f)
        {
            if (CastSpell(EXPEL_HARM))
            {
                ConsumeEnergyForAbility(EXPEL_HARM, 15);
                GenerateChi(1);
                RecordAbilityUsage(EXPEL_HARM);
                return;
            }
        }
    }

    // Energizing Elixir for burst resource (if talented)
    if (CanUseAbility(ENERGIZING_ELIXIR))
    {
        if (_chiManager.current.load() <= 2 && GetBot()->GetPower(POWER_ENERGY) <= 30)
        {
            if (CastSpell(ENERGIZING_ELIXIR))
            {
                GenerateChi(5);
                GetBot()->SetPower(POWER_ENERGY, GetBot()->GetMaxPower(POWER_ENERGY));
                RecordAbilityUsage(ENERGIZING_ELIXIR);
                return;
            }
        }
    }
}

// Defensive cooldowns
void MonkAI::UseDefensiveCooldowns()
{
    if (!GetBot())
        return;

    float healthPct = GetBot()->GetHealthPct();

    // Touch of Karma - reflects damage
    if (healthPct < 40.0f && CanUseAbility(TOUCH_OF_KARMA))
    {
        if (CastSpell(TOUCH_OF_KARMA))
        {
            RecordAbilityUsage(TOUCH_OF_KARMA);
            TC_LOG_DEBUG("module.playerbot.ai", "Monk {} activated Touch of Karma",
                         GetBot()->GetName());
            return;
        }
    }

    // Fortifying Brew - damage reduction
    if (healthPct < 30.0f && CanUseAbility(FORTIFYING_BREW))
    {
        if (CastSpell(FORTIFYING_BREW))
        {
            RecordAbilityUsage(FORTIFYING_BREW);
            TC_LOG_DEBUG("module.playerbot.ai", "Monk {} activated Fortifying Brew",
                         GetBot()->GetName());
            return;
        }
    }

    // Diffuse Magic - magic damage reduction
    Unit* target = GetBot()->GetSelectedUnit();
    if (target && target->HasUnitState(UNIT_STATE_CASTING))
    {
        if (CanUseAbility(DIFFUSE_MAGIC))
        {
            if (CastSpell(DIFFUSE_MAGIC))
            {
                RecordAbilityUsage(DIFFUSE_MAGIC);
                TC_LOG_DEBUG("module.playerbot.ai", "Monk {} activated Diffuse Magic",
                             GetBot()->GetName());
                return;
            }
        }
    }

    // Dampen Harm - periodic damage reduction (if talented)
    if (healthPct < 50.0f && CanUseAbility(DAMPEN_HARM))
    {
        if (CastSpell(DAMPEN_HARM))
        {
            RecordAbilityUsage(DAMPEN_HARM);
            TC_LOG_DEBUG("module.playerbot.ai", "Monk {} activated Dampen Harm",
                         GetBot()->GetName());
            return;
        }
    }

    // Zen Meditation for Brewmasters
    if (_currentSpec == MonkSpec::BREWMASTER && healthPct < 25.0f)
    {
        if (CanUseAbility(ZEN_MEDITATION))
        {
            if (CastSpell(ZEN_MEDITATION))
            {
                RecordAbilityUsage(ZEN_MEDITATION);
                TC_LOG_DEBUG("module.playerbot.ai", "Monk {} channeling Zen Meditation",
                             GetBot()->GetName());
                return;
            }
        }
    }

    // Life Cocoon for Mistweavers on self or ally
    if (_currentSpec == MonkSpec::MISTWEAVER && CanUseAbility(LIFE_COCOON))
    {
        Unit* healTarget = GetLowestHealthAlly(40.0f);
        if (healTarget && healTarget->GetHealthPct() < 30.0f)
        {
            if (CastSpell(healTarget, LIFE_COCOON))
            {
                RecordAbilityUsage(LIFE_COCOON);
                TC_LOG_DEBUG("module.playerbot.ai", "Monk {} cast Life Cocoon on {}",
                             GetBot()->GetName(), healTarget->GetName());
                return;
            }
        }
    }
}

// Mobility abilities
void MonkAI::HandleMobilityAbilities(::Unit* target, const Position& optimalPos)
{
    if (!target || !GetBot())
        return;

    float distanceToOptimal = GetBot()->GetDistance(optimalPos);
    float distanceToTarget = GetBot()->GetDistance(target);

    // Roll for quick repositioning
    if (distanceToOptimal > 10.0f && CanUseAbility(ROLL))
    {
        Position rollDest = CalculateRollDestination(target);
        if (CastSpell(ROLL))
        {
            _lastMobilityUse = getMSTime();
            RecordAbilityUsage(ROLL);
            TC_LOG_DEBUG("module.playerbot.ai", "Monk {} used Roll for positioning",
                         GetBot()->GetName());
            return;
        }
    }

    // Flying Serpent Kick for gap closing (Windwalker)
    if (_currentSpec == MonkSpec::WINDWALKER && distanceToTarget > 8.0f)
    {
        if (CanUseAbility(FLYING_SERPENT_KICK))
        {
            if (CastSpell(target, FLYING_SERPENT_KICK))
            {
                _lastMobilityUse = getMSTime();
                RecordAbilityUsage(FLYING_SERPENT_KICK);
                TC_LOG_DEBUG("module.playerbot.ai", "Monk {} used Flying Serpent Kick",
                             GetBot()->GetName());
                return;
            }
        }
    }

    // Tiger's Lust for speed boost
    if (CanUseAbility(TIGERS_LUST))
    {
        if (distanceToTarget > 15.0f || distanceToOptimal > 15.0f)
        {
            if (CastSpell(GetBot(), TIGERS_LUST))
            {
                RecordAbilityUsage(TIGERS_LUST);
                TC_LOG_DEBUG("module.playerbot.ai", "Monk {} activated Tiger's Lust",
                             GetBot()->GetName());
                return;
            }
        }
    }
}

// Calculate roll destination
Position MonkAI::CalculateRollDestination(::Unit* target)
{
    if (!target || !GetBot())
        return GetBot()->GetPosition();

    // Roll towards target but not past it
    float angle = GetBot()->GetAbsoluteAngle(target);
    float distance = std::min(ROLL_DISTANCE, GetBot()->GetDistance(target) - 3.0f);

    return GetBot()->GetFirstCollisionPosition(distance, angle);
}

// Windwalker DPS rotation
void MonkAI::ExecuteWindwalkerRotation(::Unit* target)
{
    if (!target || !GetBot())
        return;

    // Rising Sun Kick on cooldown
    if (CanUseAbility(RISING_SUN_KICK) && HasEnoughChi(2))
    {
        if (CastSpell(target, RISING_SUN_KICK))
        {
            ConsumeChiForAbility(RISING_SUN_KICK, 2);
            RecordAbilityUsage(RISING_SUN_KICK);
            return;
        }
    }

    // Whirling Dragon Punch combo finisher
    if (CanUseAbility(WHIRLING_DRAGON_PUNCH))
    {
        if (CastSpell(target, WHIRLING_DRAGON_PUNCH))
        {
            RecordAbilityUsage(WHIRLING_DRAGON_PUNCH);
            return;
        }
    }

    // Fists of Fury for burst damage
    if (CanUseAbility(FISTS_OF_FURY) && HasEnoughChi(3))
    {
        if (CastSpell(target, FISTS_OF_FURY))
        {
            ConsumeChiForAbility(FISTS_OF_FURY, 3);
            RecordAbilityUsage(FISTS_OF_FURY);
            return;
        }
    }

    // Blackout Kick for damage
    if (CanUseAbility(BLACKOUT_KICK) && HasEnoughChi(1))
    {
        if (!_comboTracker.WillBreakCombo(BLACKOUT_KICK))
        {
            if (CastSpell(target, BLACKOUT_KICK))
            {
                ConsumeChiForAbility(BLACKOUT_KICK, 1);
                RecordAbilityUsage(BLACKOUT_KICK);
                return;
            }
        }
    }

    // Chi Wave/Chi Burst for ranged damage
    if (CanUseAbility(CHI_WAVE))
    {
        if (CastSpell(target, CHI_WAVE))
        {
            RecordAbilityUsage(CHI_WAVE);
            return;
        }
    }

    // Tiger Palm to generate Chi
    if (CanUseAbility(TIGER_PALM) && HasEnoughEnergy(50))
    {
        if (_chiManager.current.load() < _chiManager.maximum.load())
        {
            if (CastSpell(target, TIGER_PALM))
            {
                ConsumeEnergyForAbility(TIGER_PALM, 50);
                GenerateChi(2);
                RecordAbilityUsage(TIGER_PALM);
                return;
            }
        }
    }

    // Crackling Jade Lightning as filler
    if (CanUseAbility(CRACKLING_JADE_LIGHTNING) && HasEnoughEnergy(20))
    {
        if (GetBot()->GetDistance(target) > 5.0f)
        {
            if (CastSpell(target, CRACKLING_JADE_LIGHTNING))
            {
                ConsumeEnergyForAbility(CRACKLING_JADE_LIGHTNING, 20);
                GenerateChi(1);
                RecordAbilityUsage(CRACKLING_JADE_LIGHTNING);
                return;
            }
        }
    }
}

// Brewmaster tank rotation
void MonkAI::ExecuteBrewmasterRotation(::Unit* target)
{
    if (!target || !GetBot())
        return;

    // Manage Stagger with Purifying Brew
    float maxHealth = GetBot()->GetMaxHealth();
    auto staggerLevel = _staggerManager.GetStaggerLevel(maxHealth);

    if (staggerLevel >= StaggerManagementSystem::MODERATE && CanUseAbility(PURIFYING_BREW))
    {
        if (CastSpell(PURIFYING_BREW))
        {
            _staggerManager.PurifyStagger(0.5f); // Purifies 50% of stagger
            RecordAbilityUsage(PURIFYING_BREW);
            TC_LOG_DEBUG("module.playerbot.ai", "Monk {} purified stagger",
                         GetBot()->GetName());
            return;
        }
    }

    // Maintain Ironskin Brew
    if (CanUseAbility(IRONSKIN_BREW))
    {
        if (!GetBot()->HasAura(IRONSKIN_BREW))
        {
            if (CastSpell(IRONSKIN_BREW))
            {
                RecordAbilityUsage(IRONSKIN_BREW);
                return;
            }
        }
    }

    // Keg Smash for threat and damage
    if (CanUseAbility(KEG_SMASH) && HasEnoughEnergy(40))
    {
        if (CastSpell(target, KEG_SMASH))
        {
            ConsumeEnergyForAbility(KEG_SMASH, 40);
            GenerateChi(2);
            RecordAbilityUsage(KEG_SMASH);
            return;
        }
    }

    // Breath of Fire for DoT
    if (CanUseAbility(BREATH_OF_FIRE) && HasEnoughChi(1))
    {
        if (CastSpell(BREATH_OF_FIRE))
        {
            ConsumeChiForAbility(BREATH_OF_FIRE, 1);
            RecordAbilityUsage(BREATH_OF_FIRE);
            return;
        }
    }

    // Blackout Strike for brew generation
    if (CanUseAbility(BLACKOUT_KICK) && HasEnoughChi(1))
    {
        if (CastSpell(target, BLACKOUT_KICK))
        {
            ConsumeChiForAbility(BLACKOUT_KICK, 1);
            RecordAbilityUsage(BLACKOUT_KICK);
            return;
        }
    }

    // Tiger Palm for threat
    if (CanUseAbility(TIGER_PALM) && HasEnoughEnergy(25))
    {
        if (CastSpell(target, TIGER_PALM))
        {
            ConsumeEnergyForAbility(TIGER_PALM, 25);
            GenerateChi(1);
            RecordAbilityUsage(TIGER_PALM);
            return;
        }
    }

    // Rushing Jade Wind for AoE threat
    if (GetNearbyEnemyCount(8.0f) > 2 && CanUseAbility(RUSHING_JADE_WIND))
    {
        if (HasEnoughChi(1))
        {
            if (CastSpell(RUSHING_JADE_WIND))
            {
                ConsumeChiForAbility(RUSHING_JADE_WIND, 1);
                RecordAbilityUsage(RUSHING_JADE_WIND);
                return;
            }
        }
    }
}

// Mistweaver healer rotation
void MonkAI::ExecuteMistweaverRotation(::Unit* target)
{
    if (!GetBot())
        return;

    // Check for healing targets
    Unit* healTarget = GetLowestHealthAlly(40.0f);

    if (healTarget)
    {
        float targetHealthPct = healTarget->GetHealthPct();

        // Emergency healing with Life Cocoon
        if (targetHealthPct < 30.0f && CanUseAbility(LIFE_COCOON))
        {
            if (CastSpell(healTarget, LIFE_COCOON))
            {
                RecordAbilityUsage(LIFE_COCOON);
                return;
            }
        }

        // Enveloping Mist for strong single target heal
        if (targetHealthPct < 50.0f && CanUseAbility(ENVELOPING_MIST))
        {
            if (HasEnoughChi(3))
            {
                if (CastSpell(healTarget, ENVELOPING_MIST))
                {
                    ConsumeChiForAbility(ENVELOPING_MIST, 3);
                    RecordAbilityUsage(ENVELOPING_MIST);
                    return;
                }
            }
        }

        // Vivify for quick heal
        if (targetHealthPct < 70.0f && CanUseAbility(VIVIFY))
        {
            if (CastSpell(healTarget, VIVIFY))
            {
                RecordAbilityUsage(VIVIFY);
                return;
            }
        }

        // Renewing Mist for HoT
        if (!healTarget->HasAura(RENEWING_MIST) && CanUseAbility(RENEWING_MIST))
        {
            if (CastSpell(healTarget, RENEWING_MIST))
            {
                RecordAbilityUsage(RENEWING_MIST);
                return;
            }
        }

        // Soothing Mist channel
        if (targetHealthPct < 80.0f && CanUseAbility(SOOTHING_MIST))
        {
            if (!GetBot()->IsNonMeleeSpellCast(false))
            {
                if (CastSpell(healTarget, SOOTHING_MIST))
                {
                    RecordAbilityUsage(SOOTHING_MIST);
                    return;
                }
            }
        }
    }

    // Essence Font for group healing
    uint32 injuredCount = GetNearbyInjuredAlliesCount(25.0f, 70.0f);
    if (injuredCount >= 3 && CanUseAbility(ESSENCE_FONT))
    {
        if (HasEnoughChi(2))
        {
            if (CastSpell(ESSENCE_FONT))
            {
                ConsumeChiForAbility(ESSENCE_FONT, 2);
                RecordAbilityUsage(ESSENCE_FONT);
                return;
            }
        }
    }

    // Thunder Focus Tea for enhanced healing
    if (CanUseAbility(THUNDER_FOCUS_TEA))
    {
        if (CastSpell(THUNDER_FOCUS_TEA))
        {
            RecordAbilityUsage(THUNDER_FOCUS_TEA);
            return;
        }
    }

    // Fistweaving - damage while healing
    if (target && _healingSystem.fistweavingMode.load())
    {
        // Tiger Palm for Teachings of the Monastery
        if (CanUseAbility(TIGER_PALM) && HasEnoughEnergy(50))
        {
            if (CastSpell(target, TIGER_PALM))
            {
                ConsumeEnergyForAbility(TIGER_PALM, 50);
                GenerateChi(1);
                RecordAbilityUsage(TIGER_PALM);
                return;
            }
        }

        // Blackout Kick for damage
        if (CanUseAbility(BLACKOUT_KICK) && HasEnoughChi(1))
        {
            if (CastSpell(target, BLACKOUT_KICK))
            {
                ConsumeChiForAbility(BLACKOUT_KICK, 1);
                RecordAbilityUsage(BLACKOUT_KICK);
                return;
            }
        }

        // Rising Sun Kick for damage
        if (CanUseAbility(RISING_SUN_KICK) && HasEnoughChi(2))
        {
            if (CastSpell(target, RISING_SUN_KICK))
            {
                ConsumeChiForAbility(RISING_SUN_KICK, 2);
                RecordAbilityUsage(RISING_SUN_KICK);
                return;
            }
        }
    }

    // Mana Tea for mana regeneration
    if (GetBot()->GetPowerPct(POWER_MANA) < 30.0f && CanUseAbility(MANA_TEA))
    {
        if (CastSpell(MANA_TEA))
        {
            RecordAbilityUsage(MANA_TEA);
            return;
        }
    }
}

// Helper method to get lowest health ally
Unit* MonkAI::GetLowestHealthAlly(float range)
{
    if (!GetBot())
        return nullptr;

    Unit* lowestAlly = nullptr;
    float lowestHealthPct = 100.0f;

    std::list<Unit*> allies;
    Trinity::AnyFriendlyUnitInObjectRangeCheck u_check(GetBot(), GetBot(), range);
    Trinity::UnitListSearcher<Trinity::AnyFriendlyUnitInObjectRangeCheck> searcher(GetBot(), allies, u_check);
    Cell::VisitAllObjects(GetBot(), searcher, range);

    for (auto* ally : allies)
    {
        if (!ally || ally->isDead())
            continue;

        float healthPct = ally->GetHealthPct();
        if (healthPct < lowestHealthPct)
        {
            lowestHealthPct = healthPct;
            lowestAlly = ally;
        }
    }

    return lowestAlly;
}

// Count nearby injured allies
uint32 MonkAI::GetNearbyInjuredAlliesCount(float range, float healthThreshold)
{
    if (!GetBot())
        return 0;

    uint32 count = 0;
    std::list<Unit*> allies;
    Trinity::AnyFriendlyUnitInObjectRangeCheck u_check(GetBot(), GetBot(), range);
    Trinity::UnitListSearcher<Trinity::AnyFriendlyUnitInObjectRangeCheck> searcher(GetBot(), allies, u_check);
    Cell::VisitAllObjects(GetBot(), searcher, range);

    for (auto* ally : allies)
    {
        if (!ally || ally->isDead())
            continue;

        if (ally->GetHealthPct() < healthThreshold)
            count++;
    }

    return count;
}

// Count nearby enemies
uint32 MonkAI::GetNearbyEnemyCount(float range) const
{
    if (!GetBot())
        return 0;

    uint32 count = 0;
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(GetBot(), GetBot(), range);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(GetBot(), targets, u_check);
    Cell::VisitAllObjects(GetBot(), searcher, range);

    for (auto* target : targets)
    {
        if (GetBot()->IsValidAttackTarget(target))
            count++;
    }

    return count;
}

// Record interrupt attempts
void MonkAI::RecordInterruptAttempt(::Unit* target, uint32 spellId, bool success)
{
    if (success)
    {
        _successfulInterrupts++;
        TC_LOG_DEBUG("module.playerbot.ai", "Monk {} successfully interrupted with spell {}",
                     GetBot()->GetName(), spellId);
    }
}

// Advanced combat logic
void MonkAI::UpdateAdvancedCombatLogic(::Unit* target)
{
    if (!target || !GetBot())
        return;

    // Update form management
    OptimizeFormManagement(target);

    // Update resource efficiency
    ManageResourceEfficiency();

    // Update combo strikes
    if (_currentSpec == MonkSpec::WINDWALKER)
    {
        OptimizeComboStrikes();
    }

    // Update stagger management
    if (_currentSpec == MonkSpec::BREWMASTER)
    {
        ManageStaggerLevel();
    }

    // Update healing priorities
    if (_currentSpec == MonkSpec::MISTWEAVER)
    {
        OptimizeHealingRotation();
    }
}

// Form management
void MonkAI::OptimizeFormManagement(::Unit* target)
{
    // Forms are passive in modern WoW for monks
    // This is kept for potential custom implementation
    _formManager.UpdateFormDuration(100); // Update with diff
}

// Resource efficiency
void MonkAI::ManageResourceEfficiency()
{
    // Calculate and track resource efficiency
    _chiManager.efficiency.store(_chiManager.CalculateEfficiency());
    _energyManager.efficiency.store(_energyManager.GetEnergyPercent());
}

// Combo strike optimization
void MonkAI::OptimizeComboStrikes()
{
    // Track combo strike efficiency
    float comboScore = _comboTracker.comboCount.load() > 0 ?
                       _comboTracker.comboDamageBonus.load() : 0.0f;
    _monkMetrics.comboStrikeScore.store(comboScore);
}

// Stagger management
void MonkAI::ManageStaggerLevel()
{
    float maxHealth = GetBot()->GetMaxHealth();
    auto level = _staggerManager.GetStaggerLevel(maxHealth);
    _staggerManager.UpdateStaggerTracking(level, 100); // Update with diff

    // Calculate mitigation efficiency
    float efficiency = _staggerManager.CalculateMitigationEfficiency();
    _monkMetrics.staggerMitigationScore.store(efficiency);
}

// Healing optimization
void MonkAI::OptimizeHealingRotation()
{
    // Update healing efficiency metrics
    float efficiency = _healingSystem.healingEfficiency.load();
    _monkMetrics.healingEfficiencyScore.store(efficiency);
}

// Placeholder implementations for the advanced methods
void MonkAI::HandleAdvancedBrewmasterManagement()
{
    // Advanced Brewmaster mechanics handled in ExecuteBrewmasterRotation
}

void MonkAI::HandleAdvancedMistweaverManagement()
{
    // Advanced Mistweaver mechanics handled in ExecuteMistweaverRotation
}

void MonkAI::HandleAdvancedWindwalkerManagement()
{
    // Advanced Windwalker mechanics handled in ExecuteWindwalkerRotation
}

void MonkAI::RecordAbilityUsage(uint32 spellId)
{
    // Track ability usage for performance monitoring
    _monkMetrics.totalAbilitiesUsed.fetch_add(1, std::memory_order_relaxed);
}

} // namespace Playerbot