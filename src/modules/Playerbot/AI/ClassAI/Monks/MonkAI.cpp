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
#include "../../../Spatial/SpatialGridManager.h"  // Lock-free spatial grid for deadlock fix
#include "ObjectAccessor.h"
#include "../../../Spatial/SpatialGridQueryHelpers.h"  // PHASE 5F: Thread-safe queries

namespace Playerbot
{

MonkAI::MonkAI(Player* bot) : ClassAI(bot), _currentSpec(MonkSpec::WINDWALKER){
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

if (!interruptTarget)

{
    return 0;

}

if (!interruptTarget)

{
    return 0;

}
                                 if (!interruptTarget)
                                 {
                                     return;
                                 }
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
                                 if (!interruptTarget)
                                 {
                                     return;
                                 }
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
                         if (!priorityTarget)
                         {
                             return;
                         }
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
                if (CastSpell(2, SPINNING_CRANE_KICK))
                {
                    ConsumeChiForAbility(SPINNING_CRANE_KICK);
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
                if (CastSpell(1, RUSHING_JADE_WIND))
                {
                    ConsumeChiForAbility(RUSHING_JADE_WIND);
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
    Player* bot = GetBot();
    if (!bot)
        return;

    // Apply Legacy of the White Tiger / Legacy of the Emperor (raid-wide stat buff)
    uint32 legacyBuff = 0;
    if (bot->HasSpell(LEGACY_OF_THE_WHITE_TIGER))
        legacyBuff = LEGACY_OF_THE_WHITE_TIGER;
    else if (bot->HasSpell(LEGACY_OF_THE_EMPEROR))
        legacyBuff = LEGACY_OF_THE_EMPEROR;

    if (legacyBuff != 0 && !bot->HasAura(legacyBuff))
    {
        uint32 now = GameTime::GetGameTimeMS();
        if (now - _lastLegacyBuff > 300000) // 5 minute buff duration, recast check
        {
            if (CastSpell(bot, legacyBuff))
            {
                _lastLegacyBuff = now;
                RecordAbilityUsage(legacyBuff);
                TC_LOG_DEBUG("module.playerbot.monk", "Monk {} applied Legacy buff", bot->GetName());
            }
        }
    }

    // Specialization-specific buff management
    ChrSpecialization spec = bot->GetPrimarySpecialization();

    switch (static_cast<int>(spec))
    {
        case 268: // Brewmaster
        {
            // Maintain Ironskin Brew uptime when tanking
            if (!bot->HasAura(IRONSKIN_BREW) && bot->IsInCombat())
            {
                if (bot->HasSpell(IRONSKIN_BREW) && HasEnoughResource(IRONSKIN_BREW))
                {
                    if (CastSpell(bot, IRONSKIN_BREW))
                    {
                        ConsumeResource(IRONSKIN_BREW);
                        RecordAbilityUsage(IRONSKIN_BREW);
                        TC_LOG_DEBUG("module.playerbot.monk", "Brewmaster {} activated Ironskin Brew", bot->GetName());
                    }
                }
            }

            // Use Fortifying Brew defensively
            if (bot->GetHealthPct() < 40.0f && bot->HasSpell(FORTIFYING_BREW))
            {
                if (CastSpell(bot, FORTIFYING_BREW))
                {
                    RecordAbilityUsage(FORTIFYING_BREW);
                    TC_LOG_DEBUG("module.playerbot.monk", "Brewmaster {} used Fortifying Brew", bot->GetName());
                }
            }
            break;
        }

        case 270: // Mistweaver
        {
            // Maintain Thunder Focus Tea charges for optimal healing
            if (bot->HasSpell(THUNDER_FOCUS_TEA) && !bot->HasAura(THUNDER_FOCUS_TEA))
            {
                if (CastSpell(bot, THUNDER_FOCUS_TEA))
                {
                    RecordAbilityUsage(THUNDER_FOCUS_TEA);
                    TC_LOG_DEBUG("module.playerbot.monk", "Mistweaver {} prepared Thunder Focus Tea", bot->GetName());
                }
            }

            // Use Mana Tea when low on mana
            if (bot->GetPowerPct(POWER_MANA) < 50.0f && bot->HasSpell(MANA_TEA))
            {
                if (CastSpell(bot, MANA_TEA))
                {
                    RecordAbilityUsage(MANA_TEA);
                    TC_LOG_DEBUG("module.playerbot.monk", "Mistweaver {} used Mana Tea", bot->GetName());
                }
            }
            break;
        }

        case 269: // Windwalker
        {
            // Activate Storm, Earth, and Fire during burst windows
            if (bot->IsInCombat() && bot->HasSpell(STORM_EARTH_AND_FIRE))
            {
                Unit* target = bot->GetVictim();
                if (target && target->GetHealthPct() > 70.0f) // Boss-level target
                {
                    if (!bot->HasAura(STORM_EARTH_AND_FIRE))
                    {
                        if (CastSpell(target, STORM_EARTH_AND_FIRE))
                        {
                            RecordAbilityUsage(STORM_EARTH_AND_FIRE);
                            TC_LOG_DEBUG("module.playerbot.monk", "Windwalker {} activated Storm Earth and Fire", bot->GetName());
                        }
                    }
                }
            }

            // Use Energizing Elixir when low on resources
            if (bot->HasSpell(ENERGIZING_ELIXIR))
            {
                uint32 currentChi = _chiManager.current.load();
                uint32 currentEnergy = _energyManager.current.load();
                if (currentChi < 2 && currentEnergy < 40)
                {
                    if (CastSpell(bot, ENERGIZING_ELIXIR))
                    {
                        _chiManager.GenerateChi(2);
                        _energyManager.current.store(::std::min(_energyManager.current.load() + 50, _energyManager.maximum.load()));
                        RecordAbilityUsage(ENERGIZING_ELIXIR);
                        TC_LOG_DEBUG("module.playerbot.monk", "Windwalker {} used Energizing Elixir", bot->GetName());
                    }
                }
            }
            break;
        }

        default:
            break;
    }

    // Universal defensive buffs
    if (bot->GetHealthPct() < 35.0f)
    {
        // Touch of Karma - reflect damage back to attacker
        if (bot->HasSpell(TOUCH_OF_KARMA) && !bot->HasAura(TOUCH_OF_KARMA))
        {
            Unit* attacker = bot->GetVictim();
            if (attacker && CastSpell(TOUCH_OF_KARMA, attacker))
            {
                RecordAbilityUsage(TOUCH_OF_KARMA);
                _defensiveCooldownsUsed.store(_defensiveCooldownsUsed.load() + 1);
                TC_LOG_DEBUG("module.playerbot.monk", "Monk {} used Touch of Karma", bot->GetName());
            }
        }

        // Diffuse Magic - reduce magic damage
        if (bot->HasSpell(DIFFUSE_MAGIC) && !bot->HasAura(DIFFUSE_MAGIC))
        {
            if (CastSpell(bot, DIFFUSE_MAGIC))
            {
                RecordAbilityUsage(DIFFUSE_MAGIC);
                _defensiveCooldownsUsed.store(_defensiveCooldownsUsed.load() + 1);
                TC_LOG_DEBUG("module.playerbot.monk", "Monk {} used Diffuse Magic", bot->GetName());
            }
        }
    }
}

void MonkAI::UpdateCooldowns(uint32 diff)
{
    if (!GetBot())
        return;

    // Update Chi and Energy regeneration
    _chiManager.efficiency.store(_chiManager.CalculateEfficiency());
    ManageEnergyRegeneration(diff);

    // Update ability cooldown tracking
    uint32 now = GameTime::GetGameTimeMS();

    // Track form management cooldown
    if (_formManager.lastFormChange.load() > 0)
    {
        uint32 timeSinceChange = now - _formManager.lastFormChange.load();
        if (timeSinceChange > FORM_CHANGE_COOLDOWN)
        {
            _formManager.UpdateFormDuration(diff);
        }
    }

    // Track mobility cooldowns
    if (_lastMobilityUse > 0)
    {
        uint32 timeSinceMobility = now - _lastMobilityUse;
        if (timeSinceMobility > ROLL_COOLDOWN)
        {
            _lastMobilityUse = 0; // Reset cooldown marker
        }
    }

    // Track defensive cooldowns
    if (_lastDefensiveUse > 0)
    {
        uint32 timeSinceDefensive = now - _lastDefensiveUse;
        if (timeSinceDefensive > 60000) // 1 minute general defensive cooldown
        {
            _lastDefensiveUse = 0;
        }
    }

    // Track interrupt cooldowns
    if (_lastInterruptAttempt > 0)
    {
        uint32 timeSinceInterrupt = now - _lastInterruptAttempt;
        if (timeSinceInterrupt > 15000) // 15 second interrupt cooldown
        {
            _lastInterruptAttempt = 0;
        }
    }

    // Update Brewmaster stagger tracking
    if (_currentSpec == MonkSpec::BREWMASTER)
    {
        _staggerManager.UpdateStaggerTracking(_staggerManager.GetStaggerLevel(GetBot()->GetMaxHealth()), diff);
    }

    // Update Mistweaver healing priority
    if (_currentSpec == MonkSpec::MISTWEAVER)
    {
        // Check healing efficiency periodically
        uint32 totalHealing = _healingSystem.totalHealing.load();
        uint32 overhealingDone = _healingSystem.overhealingDone.load();
        if (totalHealing > 0)
        {
            float efficiency = 1.0f - (static_cast<float>(overhealingDone) / totalHealing);
            _healingSystem.healingEfficiency.store(efficiency);
        }
    }

    // Update Windwalker combo strike tracking
    if (_currentSpec == MonkSpec::WINDWALKER)
    {
        // Decay combo count if no abilities used recently
        uint32 lastAbilityTime = _comboTracker.lastAbility.load();
        if (lastAbilityTime > 0 && now - lastAbilityTime > COMBO_STRIKE_WINDOW)
        {
            _comboTracker.Reset();
        }
    }

    // Update metrics
    UpdateMetrics(diff);
}

void MonkAI::ManageEnergyRegeneration(uint32 diff)
{
    // Regenerate energy over time
    _energyManager.RegenerateEnergy(diff);

    // Update bot's actual energy
    if (GetBot())
    {
        uint32 currentEnergy = GetBot()->GetPower(POWER_ENERGY);
        _energyManager.current.store(currentEnergy);
    }
}

void MonkAI::UpdateMetrics(uint32 diff)
{
    // Update performance metrics
    _monkMetrics.averageChiEfficiency.store(_chiManager.CalculateEfficiency());
    _monkMetrics.averageEnergyEfficiency.store(_energyManager.efficiency.load());

    if (_currentSpec == MonkSpec::BREWMASTER)
    {
        _monkMetrics.staggerMitigationScore.store(_staggerManager.CalculateMitigationEfficiency());
    }

    if (_currentSpec == MonkSpec::MISTWEAVER)
    {
        _monkMetrics.healingEfficiencyScore.store(_healingSystem.healingEfficiency.load());
    }

    if (_currentSpec == MonkSpec::WINDWALKER)
    {
        float comboScore = _comboTracker.comboCount.load() > 0 ?
                           _comboTracker.comboDamageBonus.load() : 0.0f;
        _monkMetrics.comboStrikeScore.store(comboScore);
    }
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
    if (!GetBot())
        return false;

    // Determine resource cost based on spell
    switch (spellId)
    {
        // Chi generators (Energy cost)
        case TIGER_PALM:
            return GetBot()->GetPower(POWER_ENERGY) >= 50;
        case EXPEL_HARM:
            return GetBot()->GetPower(POWER_ENERGY) >= 15;
        case JAB:
            return GetBot()->GetPower(POWER_ENERGY) >= 40;

        // Chi spenders
        case BLACKOUT_KICK:
            return _chiManager.current.load() >= 1;
        case RISING_SUN_KICK:
            return _chiManager.current.load() >= 2;
        case FISTS_OF_FURY:
            return _chiManager.current.load() >= 3;
        case SPINNING_CRANE_KICK:
            return _chiManager.current.load() >= 2;
        case WHIRLING_DRAGON_PUNCH:
            return _chiManager.current.load() >= 2;
        case TOUCH_OF_DEATH:
            return _chiManager.current.load() >= 3;
        case RUSHING_JADE_WIND:
            return _chiManager.current.load() >= 1;

        // Brewmaster abilities
        case KEG_SMASH:
            return GetBot()->GetPower(POWER_ENERGY) >= 40;
        case BREATH_OF_FIRE:
            return _chiManager.current.load() >= 1;
        case IRONSKIN_BREW:
            return true; // Brew charges handled separately
        case PURIFYING_BREW:
            return true; // Brew charges handled separately
        case BLACK_OX_BREW:
            return true; // Cooldown based

        // Mistweaver abilities (Mana cost)
        case RENEWING_MIST:
            return GetBot()->GetPower(POWER_MANA) >= (GetBot()->GetMaxPower(POWER_MANA) * 2 / 100); // 2% mana
        case ENVELOPING_MIST:
            return _chiManager.current.load() >= 3 && GetBot()->GetPower(POWER_MANA) >= (GetBot()->GetMaxPower(POWER_MANA) * 5 / 100); // 3 Chi + 5% mana
        case VIVIFY:
            return GetBot()->GetPower(POWER_MANA) >= (GetBot()->GetMaxPower(POWER_MANA) * 4 / 100); // 4% mana
        case ESSENCE_FONT:
            return _chiManager.current.load() >= 2 && GetBot()->GetPower(POWER_MANA) >= (GetBot()->GetMaxPower(POWER_MANA) * 6 / 100); // 2 Chi + 6% mana
        case SOOTHING_MIST:
            return GetBot()->GetPower(POWER_MANA) >= (GetBot()->GetMaxPower(POWER_MANA) * 3 / 100); // 3% mana/sec
        case LIFE_COCOON:
            return GetBot()->GetPower(POWER_MANA) >= (GetBot()->GetMaxPower(POWER_MANA) * 3 / 100); // 3% mana
        case REVIVAL:
            return GetBot()->GetPower(POWER_MANA) >= (GetBot()->GetMaxPower(POWER_MANA) * 20 / 100); // 20% mana
        case SHEILUNS_GIFT:
            return true; // Stack based

        // Energy cost abilities
        case CRACKLING_JADE_LIGHTNING:
            return GetBot()->GetPower(POWER_ENERGY) >= 20;
        case FLYING_SERPENT_KICK:
            return true; // No resource cost
        case ROLL:
        case CHI_TORPEDO:
            return true; // Charge based

        // Cooldown based abilities
        case STORM_EARTH_AND_FIRE:
        case SERENITY:
        case FORTIFYING_BREW:
        case THUNDER_FOCUS_TEA:
        case MANA_TEA:
        case ENERGIZING_ELIXIR:
        case TOUCH_OF_KARMA:
        case DIFFUSE_MAGIC:
        case DAMPEN_HARM:
        case ZEN_MEDITATION:
        case PARALYSIS:
        case LEG_SWEEP:
        case SPEAR_HAND_STRIKE:
        case RING_OF_PEACE:
        case DETOX:
        case RESUSCITATE:
        case CHI_WAVE:
        case CHI_BURST:
        case TIGERS_LUST:
        case TRANSCENDENCE:
        case TRANSCENDENCE_TRANSFER:
            return true; // Cooldown and situational based

        default:
            return true; // Unknown spells assumed available
    }
}

void MonkAI::ConsumeResource(uint32 spellId)
{
    if (!GetBot())
        return;

    // Consume resources based on spell
    switch (spellId)
    {
        // Chi generators (consume Energy, generate Chi)
        case TIGER_PALM:
            if (GetBot()->GetPower(POWER_ENERGY) >= 50)
            {
                GetBot()->ModifyPower(POWER_ENERGY, -50);
                _energyManager.SpendEnergy(50);
                _energySpent.store(_energySpent.load() + 50);
            }
            break;

        case EXPEL_HARM:
            if (GetBot()->GetPower(POWER_ENERGY) >= 15)
            {
                GetBot()->ModifyPower(POWER_ENERGY, -15);
                _energyManager.SpendEnergy(15);
                _energySpent.store(_energySpent.load() + 15);
            }
            break;

        case JAB:
            if (GetBot()->GetPower(POWER_ENERGY) >= 40)
            {
                GetBot()->ModifyPower(POWER_ENERGY, -40);
                _energyManager.SpendEnergy(40);
                _energySpent.store(_energySpent.load() + 40);
            }
            break;

        // Chi spenders
        case BLACKOUT_KICK:
            if (_chiManager.ConsumeChi(1))
            {
                _chiSpent.store(_chiSpent.load() + 1);
            }
            break;

        case RISING_SUN_KICK:
            if (_chiManager.ConsumeChi(2))
            {
                _chiSpent.store(_chiSpent.load() + 2);
            }
            break;

        case FISTS_OF_FURY:
        case TOUCH_OF_DEATH:
            if (_chiManager.ConsumeChi(3))
            {
                _chiSpent.store(_chiSpent.load() + 3);
            }
            break;

        case SPINNING_CRANE_KICK:
        case WHIRLING_DRAGON_PUNCH:
            if (_chiManager.ConsumeChi(2))
            {
                _chiSpent.store(_chiSpent.load() + 2);
            }
            break;

        case RUSHING_JADE_WIND:
        case BREATH_OF_FIRE:
            if (_chiManager.ConsumeChi(1))
            {
                _chiSpent.store(_chiSpent.load() + 1);
            }
            break;

        // Brewmaster energy spenders
        case KEG_SMASH:
            if (GetBot()->GetPower(POWER_ENERGY) >= 40)
            {
                GetBot()->ModifyPower(POWER_ENERGY, -40);
                _energyManager.SpendEnergy(40);
                _energySpent.store(_energySpent.load() + 40);
            }
            break;

        // Mistweaver mana spenders
        case RENEWING_MIST:
        {
            uint32 cost = GetBot()->GetMaxPower(POWER_MANA) * 2 / 100;
            GetBot()->ModifyPower(POWER_MANA, -static_cast<int32>(cost));
            break;
        }

        case ENVELOPING_MIST:
        {
            if (_chiManager.ConsumeChi(3))
            {
                _chiSpent.store(_chiSpent.load() + 3);
                uint32 cost = GetBot()->GetMaxPower(POWER_MANA) * 5 / 100;
                GetBot()->ModifyPower(POWER_MANA, -static_cast<int32>(cost));
            }
            break;
        }

        case VIVIFY:
        {
            uint32 cost = GetBot()->GetMaxPower(POWER_MANA) * 4 / 100;
            GetBot()->ModifyPower(POWER_MANA, -static_cast<int32>(cost));
            break;
        }

        case ESSENCE_FONT:
        {
            if (_chiManager.ConsumeChi(2))
            {
                _chiSpent.store(_chiSpent.load() + 2);
                uint32 cost = GetBot()->GetMaxPower(POWER_MANA) * 6 / 100;
                GetBot()->ModifyPower(POWER_MANA, -static_cast<int32>(cost));
            }
            break;
        }

        case LIFE_COCOON:
        {
            uint32 cost = GetBot()->GetMaxPower(POWER_MANA) * 3 / 100;
            GetBot()->ModifyPower(POWER_MANA, -static_cast<int32>(cost));
            break;
        }

        case REVIVAL:
        {
            uint32 cost = GetBot()->GetMaxPower(POWER_MANA) * 20 / 100;
            GetBot()->ModifyPower(POWER_MANA, -static_cast<int32>(cost));
            break;
        }

        case CRACKLING_JADE_LIGHTNING:
            if (GetBot()->GetPower(POWER_ENERGY) >= 20)
            {
                GetBot()->ModifyPower(POWER_ENERGY, -20);
                _energyManager.SpendEnergy(20);
                _energySpent.store(_energySpent.load() + 20);
            }
            break;

        // Abilities with no resource cost
        default:
            break;
    }

    TC_LOG_DEBUG("module.playerbot.monk", "Monk {} consumed resources for spell {}",
                 GetBot()->GetName(), spellId);
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
            if (CastSpell(15, EXPEL_HARM))
            {
                ConsumeEnergyForAbility(EXPEL_HARM);
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
                             if (!healTarget)
                             {
                                 return;
                             }
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
            _lastMobilityUse = GameTime::GetGameTimeMS();
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
                _lastMobilityUse = GameTime::GetGameTimeMS();
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
    float distance = ::std::min(ROLL_DISTANCE, GetBot()->GetDistance(target) - 3.0f);

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
        if (CastSpell(1, BREATH_OF_FIRE))
        {
            ConsumeChiForAbility(BREATH_OF_FIRE);
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
            if (CastSpell(1, RUSHING_JADE_WIND))
            {
                ConsumeChiForAbility(RUSHING_JADE_WIND);
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
            if (CastSpell(2, ESSENCE_FONT))
            {
                ConsumeChiForAbility(ESSENCE_FONT);
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

    ::std::list<Unit*> allies;
    Trinity::AnyFriendlyUnitInObjectRangeCheck u_check(GetBot(), GetBot(), range);
    Trinity::UnitListSearcher<Trinity::AnyFriendlyUnitInObjectRangeCheck> searcher(GetBot(), allies, u_check);
    // DEADLOCK FIX: Use lock-free spatial grid instead of Cell::VisitGridObjects
    Map* map = GetBot()->GetMap();
    if (!map)
        return nullptr;

    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (!spatialGrid)
    {
        sSpatialGridManager.CreateGrid(map);
        spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
            return nullptr;
    }

    // Query nearby GUIDs (lock-free!)
    ::std::vector<ObjectGuid> nearbyGuids = spatialGrid->QueryNearbyCreatureGuids(
        GetBot()->GetPosition(), range);

    // Process results (replace old searcher logic)
    for (ObjectGuid guid : nearbyGuids)
    {
        // PHASE 5F: Thread-safe spatial grid validation
        auto snapshot_entity = SpatialGridQueryHelpers::FindCreatureByGuid(GetBot(), guid);

        Creature* entity = nullptr;
        if (snapshot_entity)
        {

        } snapshot_entity = SpatialGridQueryHelpers::FindCreatureByGuid(GetBot(), guid);
 entity = nullptr;
 if (snapshot_entity)
 {
 }
        if (!entity)
            continue;
        // Original filtering logic from searcher goes here
    }
    // End of spatial grid fix

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
    ::std::list<Unit*> allies;
    Trinity::AnyFriendlyUnitInObjectRangeCheck u_check(GetBot(), GetBot(), range);
    Trinity::UnitListSearcher<Trinity::AnyFriendlyUnitInObjectRangeCheck> searcher(GetBot(), allies, u_check);
    // DEADLOCK FIX: Use lock-free spatial grid instead of Cell::VisitGridObjects
    Map* map = GetBot()->GetMap();
    if (!map)
        return 0;

    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (!spatialGrid)
    {
        sSpatialGridManager.CreateGrid(map);
        spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
            return 0;
    }

    // Query nearby GUIDs (lock-free!)
    ::std::vector<ObjectGuid> nearbyGuids = spatialGrid->QueryNearbyCreatureGuids(
        GetBot()->GetPosition(), range);

    // Process results (replace old searcher logic)
    for (ObjectGuid guid : nearbyGuids)
    {
        // PHASE 5F: Thread-safe spatial grid validation
        auto snapshot_entity = SpatialGridQueryHelpers::FindCreatureByGuid(GetBot(), guid);

        Creature* entity = nullptr;
        if (snapshot_entity)
        {

        } snapshot_entity = SpatialGridQueryHelpers::FindCreatureByGuid(GetBot(), guid);
 entity = nullptr;
 if (snapshot_entity)
 {
 }
        if (!entity)
            continue;
        // Original filtering logic from searcher goes here
    }
    // End of spatial grid fix

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
    ::std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(GetBot(), GetBot(), range);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(GetBot(), targets, u_check);
    // DEADLOCK FIX: Use lock-free spatial grid instead of Cell::VisitGridObjects
    Map* map = GetBot()->GetMap();
    if (!map)
        return 0;

    DoubleBufferedSpatialGrid* spatialGrid = sSpatialGridManager.GetGrid(map);
    if (!spatialGrid)
    {
        sSpatialGridManager.CreateGrid(map);
        spatialGrid = sSpatialGridManager.GetGrid(map);
        if (!spatialGrid)
            return 0;
    }

    // Query nearby GUIDs (lock-free!)
    ::std::vector<ObjectGuid> nearbyGuids = spatialGrid->QueryNearbyCreatureGuids(
        GetBot()->GetPosition(), range);

    // Process results (replace old searcher logic)
    for (ObjectGuid guid : nearbyGuids)
    {
        // PHASE 5F: Thread-safe spatial grid validation
        auto snapshot_entity = SpatialGridQueryHelpers::FindCreatureByGuid(GetBot(), guid);

        Creature* entity = nullptr;
        if (snapshot_entity)
        {

        } snapshot_entity = SpatialGridQueryHelpers::FindCreatureByGuid(GetBot(), guid);
 entity = nullptr;
 if (snapshot_entity)
 {
 }
        if (!entity)
            continue;
        // Original filtering logic from searcher goes here
    }
    // End of spatial grid fix

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
    _monkMetrics.totalAbilitiesUsed.fetch_add(1, ::std::memory_order_relaxed);
}

} // namespace Playerbot