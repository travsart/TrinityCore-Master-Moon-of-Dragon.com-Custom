/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "ShamanAI.h"
#include "ElementalSpecialization.h"
#include "EnhancementSpecialization.h"
#include "RestorationSpecialization.h"
#include "../BaselineRotationManager.h"
#include "../../Combat/CombatBehaviorIntegration.h"
#include "Player.h"
#include "Unit.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "SpellAuras.h"
#include "Map.h"
#include "Group.h"
#include "Item.h"
#include "MotionMaster.h"
#include "Log.h"
#include <algorithm>
#include <chrono>

namespace Playerbot
{

// Totem spell definitions for quick reference
enum TotemSpells
{
    // Fire Totems
    SPELL_SEARING_TOTEM = 3599,
    SPELL_FIRE_NOVA_TOTEM = 1535,
    SPELL_MAGMA_TOTEM = 8190,
    SPELL_FLAMETONGUE_TOTEM = 8227,
    SPELL_TOTEM_OF_WRATH = 30706,
    SPELL_FIRE_ELEMENTAL_TOTEM = 2894,

    // Earth Totems
    SPELL_EARTHBIND_TOTEM = 2484,
    SPELL_STONESKIN_TOTEM = 8071,
    SPELL_STONECLAW_TOTEM = 5730,
    SPELL_STRENGTH_OF_EARTH_TOTEM = 8075,
    SPELL_TREMOR_TOTEM = 8143,
    SPELL_EARTH_ELEMENTAL_TOTEM = 2062,
    SPELL_EARTHGRAB_TOTEM = 51485,

    // Water Totems
    SPELL_HEALING_STREAM_TOTEM = 5394,
    SPELL_MANA_SPRING_TOTEM = 5675,
    SPELL_POISON_CLEANSING_TOTEM = 8166,
    SPELL_DISEASE_CLEANSING_TOTEM = 8170,
    SPELL_FIRE_RESISTANCE_TOTEM = 8184,
    SPELL_MANA_TIDE_TOTEM = 16190,
    SPELL_HEALING_TIDE_TOTEM = 108280,

    // Air Totems
    SPELL_GROUNDING_TOTEM = 8177,
    SPELL_NATURE_RESISTANCE_TOTEM = 10595,
    SPELL_WINDFURY_TOTEM = 8512,
    SPELL_GRACE_OF_AIR_TOTEM = 8835,
    SPELL_WRATH_OF_AIR_TOTEM = 3738,
    SPELL_SENTRY_TOTEM = 6495,
    SPELL_SPIRIT_LINK_TOTEM = 98008,
    SPELL_CAPACITOR_TOTEM = 192058
};

// Shock spell definitions
enum ShockSpells
{
    SPELL_EARTH_SHOCK = 8042,
    SPELL_FLAME_SHOCK = 8050,
    SPELL_FROST_SHOCK = 8056,
    SPELL_WIND_SHEAR = 57994  // Interrupt
};

// Shield spell definitions
enum ShieldSpells
{
    SPELL_LIGHTNING_SHIELD = 192106,
    SPELL_WATER_SHIELD = 52127,
    SPELL_EARTH_SHIELD = 974
};

// Weapon imbue spell definitions
enum WeaponImbues
{
    SPELL_ROCKBITER_WEAPON = 8017,
    SPELL_FLAMETONGUE_WEAPON = 8024,
    SPELL_FROSTBRAND_WEAPON = 8033,
    SPELL_WINDFURY_WEAPON = 8232,
    SPELL_EARTHLIVING_WEAPON = 51730
};

// Utility spell definitions
enum UtilitySpells
{
    SPELL_PURGE = 370,
    SPELL_CLEANSE_SPIRIT = 51886,
    SPELL_HEX = 51514,
    SPELL_BLOODLUST = 2825,
    SPELL_HEROISM = 32182,
    SPELL_GHOST_WOLF = 2645,
    SPELL_ANCESTRAL_SPIRIT = 2008,
    SPELL_WATER_WALKING = 546,
    SPELL_WATER_BREATHING = 131,
    SPELL_ASTRAL_RECALL = 556,
    SPELL_ASTRAL_SHIFT = 108271,
    SPELL_SHAMANISTIC_RAGE = 30823,
    SPELL_SPIRIT_WALK = 58875
};

// Healing spell definitions
enum HealingSpells
{
    SPELL_HEALING_WAVE = 331,
    SPELL_LESSER_HEALING_WAVE = 8004,
    SPELL_CHAIN_HEAL = 1064,
    SPELL_RIPTIDE = 61295,
    SPELL_HEALING_RAIN = 73920,
    SPELL_HEALING_SURGE = 8004,
    SPELL_ANCESTRAL_GUIDANCE = 108281,
    SPELL_SPIRIT_LINK = 98021
};

// Damage spell definitions
enum DamageSpells
{
    SPELL_LIGHTNING_BOLT = 403,
    SPELL_CHAIN_LIGHTNING = 421,
    SPELL_LAVA_BURST = 51505,
    SPELL_THUNDERSTORM = 51490,
    SPELL_EARTHQUAKE = 61882,
    SPELL_ELEMENTAL_BLAST = 117014,
    SPELL_LAVA_BEAM = 114074
};

// Enhancement-specific spells
enum EnhancementSpells
{
    SPELL_STORMSTRIKE = 17364,
    SPELL_LAVA_LASH = 60103,
    SPELL_FERAL_SPIRIT = 51533,
    SPELL_CRASH_LIGHTNING = 187874,
    SPELL_WINDSTRIKE = 115356,
    SPELL_SUNDERING = 197214,
    SPELL_DOOM_WINDS = 335903
};

// Elemental-specific spells
enum ElementalSpells
{
    SPELL_ELEMENTAL_MASTERY = 16166,
    SPELL_ASCENDANCE = 114049,
    SPELL_STORMKEEPER = 191634,
    SPELL_LIQUID_MAGMA_TOTEM = 192222,
    SPELL_ICEFURY = 210714,
    SPELL_PRIMORDIAL_WAVE = 375982
};

// Talent IDs for specialization detection
enum ShamanTalents
{
    TALENT_ELEMENTAL_FOCUS = 16164,
    TALENT_ELEMENTAL_MASTERY = 16166,
    TALENT_LIGHTNING_OVERLOAD = 30675,
    TALENT_TOTEM_OF_WRATH_TALENT = 30706,
    TALENT_LAVA_BURST_TALENT = 51505,

    TALENT_DUAL_WIELD = 30798,
    TALENT_STORMSTRIKE_TALENT = 17364,
    TALENT_SHAMANISTIC_RAGE_TALENT = 30823,
    TALENT_MAELSTROM_WEAPON = 51530,
    TALENT_LAVA_LASH_TALENT = 60103,

    TALENT_NATURES_SWIFTNESS = 16188,
    TALENT_MANA_TIDE_TOTEM_TALENT = 16190,
    TALENT_EARTH_SHIELD_TALENT = 974,
    TALENT_RIPTIDE_TALENT = 61295,
    TALENT_HEALING_RAIN_TALENT = 73920
};

// Combat constants
static const float OPTIMAL_CASTER_RANGE = 30.0f;
static const float OPTIMAL_MELEE_RANGE = 5.0f;
static const float TOTEM_PLACEMENT_RANGE = 20.0f;
static const float TOTEM_EFFECT_RANGE = 40.0f;
static const uint32 SHOCK_GLOBAL_COOLDOWN = 1500;
static const uint32 TOTEM_UPDATE_INTERVAL = 2000;
static const uint32 SHIELD_REFRESH_TIME = 540000; // 9 minutes
static const uint32 WEAPON_IMBUE_DURATION = 1800000; // 30 minutes
static const uint32 FLAME_SHOCK_DURATION = 30000; // 30 seconds
static const uint32 MAELSTROM_WEAPON_MAX = 5;

ShamanAI::ShamanAI(Player* bot) : ClassAI(bot),
    _currentSpec(ShamanSpec::ELEMENTAL),
    _manaSpent(0),
    _damageDealt(0),
    _healingDone(0),
    _totemsDeploy(0),
    _shocksUsed(0),
    _lastTotemUpdate(0),
    _lastTotemCheck(0),
    _lastShockTime(0),
    _flameshockTarget(0),
    _flameshockExpiry(0),
    _maelstromWeaponStacks(0),
    _lastMaelstromCheck(0),
    _elementalMaelstrom(0),
    _lastWindShear(0),
    _lastBloodlust(0),
    _lastElementalMastery(0),
    _lastAscendance(0),
    _lastFireElemental(0),
    _lastEarthElemental(0),
    _lastSpiritWalk(0),
    _lastShamanisticRage(0),
    _hasFlameShockUp(false),
    _lavaBurstCharges(2),
    _hasLavaSurgeProc(false),
    _healingStreamTotemTime(0),
    _chainHealBounceCount(3)
{
    InitializeSpecialization();

    TC_LOG_DEBUG("module.playerbot.ai", "ShamanAI created for player {} with specialization {}",
                 bot ? bot->GetName() : "null",
                 _specialization ? _specialization->GetSpecializationName() : "none");
}

ShamanAI::~ShamanAI() = default;

void ShamanAI::UpdateRotation(::Unit* target)
{
    if (!GetBot() || !target)
        return;

    Player* bot = GetBot();

    // Check if bot should use baseline rotation (levels 1-9 or no spec)
    if (BaselineRotationManager::ShouldUseBaselineRotation(bot))
    {
        TC_LOG_DEBUG("module.playerbot.shaman", "Shaman {} using BASELINE rotation (level {})",
                     bot->GetName(), bot->GetLevel());

        static BaselineRotationManager baselineManager;
        baselineManager.HandleAutoSpecialization(bot);

        bool executed = baselineManager.ExecuteBaselineRotation(bot, target);
        TC_LOG_DEBUG("module.playerbot.shaman", "BaselineRotation result: {}", executed ? "SUCCESS" : "FAILED");

        // No fallback for casters - if rotation failed, just return
        // Do NOT use AttackerStateUpdate (melee) for a caster class
        return;
    }

    // Check if we need to switch specialization
    ShamanSpec newSpec = DetectCurrentSpecialization();
    if (newSpec != _currentSpec)
    {
        SwitchSpecialization(newSpec);
    }

    // ========================================================================
    // COMBAT BEHAVIOR INTEGRATION - Priority-based decision system
    // Following MageAI reference pattern with Shaman-specific mechanics
    // ========================================================================
    auto* behaviors = GetCombatBehaviors();

    // Priority 1: Interrupts (Wind Shear)
    if (HandleInterrupts(target))
        return;

    // Priority 2: Defensive abilities
    if (HandleDefensives())
        return;

    // Priority 3: Positioning (range management)
    if (HandlePositioning(target))
        return;

    // Priority 4: Totem management (unique to Shaman)
    if (HandleTotemManagement(target))
        return;

    // Priority 5: Target switching for priority targets
    if (HandleTargetSwitching(target))
        return;

    // Priority 6: Purge/Dispel
    if (HandlePurgeDispel(target))
        return;

    // Priority 7: AoE decisions
    if (HandleAoEDecisions(target))
        return;

    // Priority 8: Offensive cooldowns
    if (HandleOffensiveCooldowns(target))
        return;

    // Priority 9: Resource management (Maelstrom/Mana)
    if (HandleResourceManagement())
        return;

    // Priority 10: Normal rotation (spec-specific)
    HandleNormalRotation(target);

    // Track combat metrics
    if (GetBot()->IsInCombat())
    {
        _damageDealt += CalculateDamageDealt(target);
        _healingDone += CalculateHealingDone();
        _manaSpent += CalculateManaUsage();
    }
}

bool ShamanAI::HandleInterrupts(::Unit* target)
{
    auto* behaviors = GetCombatBehaviors();
    if (!behaviors || !target)
        return false;

    // Check if we should interrupt
    if (behaviors->ShouldInterrupt(target))
    {
        Unit* interruptTarget = behaviors->GetInterruptTarget();
        if (!interruptTarget)
            interruptTarget = target;

        // Wind Shear is our primary interrupt
        if (CanUseAbility(SPELL_WIND_SHEAR))
        {
            uint32 currentTime = getMSTime();
            if (currentTime - _lastWindShear > 12000) // 12 sec cooldown
            {
                if (interruptTarget->IsNonMeleeSpellCast(false))
                {
                    if (CastSpell(interruptTarget, SPELL_WIND_SHEAR))
                    {
                        _lastWindShear = currentTime;
                        TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} interrupted {} with Wind Shear",
                                     GetBot()->GetName(), interruptTarget->GetName());
                        return true;
                    }
                }
            }
        }

        // Grounding Totem as backup interrupt mechanism
        if (CanUseAbility(SPELL_GROUNDING_TOTEM) && !_activeTotems[static_cast<size_t>(TotemType::AIR)].isActive)
        {
            if (DeployTotem(SPELL_GROUNDING_TOTEM, TotemType::AIR))
            {
                TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} deployed Grounding Totem for spell protection",
                             GetBot()->GetName());
                return true;
            }
        }

        // Capacitor Totem for AoE stun interrupt
        if (_currentSpec == ShamanSpec::ELEMENTAL && CanUseAbility(SPELL_CAPACITOR_TOTEM))
        {
            if (GetBot()->GetDistance(target) <= 8.0f)
            {
                if (CastSpell(SPELL_CAPACITOR_TOTEM))
                {
                    TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} using Capacitor Totem for stun",
                                 GetBot()->GetName());
                    return true;
                }
            }
        }
    }

    return false;
}

bool ShamanAI::HandleDefensives()
{
    auto* behaviors = GetCombatBehaviors();
    if (!behaviors)
        return false;

    if (!behaviors->NeedsDefensive())
        return false;

    float healthPct = GetBot()->GetHealthPct();

    // Critical health - use major defensives
    if (healthPct < 25.0f)
    {
        // Astral Shift - 40% damage reduction
        if (CanUseAbility(SPELL_ASTRAL_SHIFT))
        {
            if (CastSpell(SPELL_ASTRAL_SHIFT))
            {
                TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} using Astral Shift at {}% health",
                             GetBot()->GetName(), healthPct);
                return true;
            }
        }

        // Earth Elemental Totem for tanking
        if (CanUseAbility(SPELL_EARTH_ELEMENTAL_TOTEM))
        {
            uint32 currentTime = getMSTime();
            if (currentTime - _lastEarthElemental > 300000) // 5 min cooldown
            {
                if (DeployTotem(SPELL_EARTH_ELEMENTAL_TOTEM, TotemType::EARTH))
                {
                    _lastEarthElemental = currentTime;
                    TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} summoning Earth Elemental for protection",
                                 GetBot()->GetName());
                    return true;
                }
            }
        }
    }

    // Low health - use moderate defensives
    if (healthPct < 40.0f)
    {
        // Shamanistic Rage for Enhancement
        if (_currentSpec == ShamanSpec::ENHANCEMENT && CanUseAbility(SPELL_SHAMANISTIC_RAGE))
        {
            uint32 currentTime = getMSTime();
            if (currentTime - _lastShamanisticRage > 60000) // 1 min cooldown
            {
                if (CastSpell(SPELL_SHAMANISTIC_RAGE))
                {
                    _lastShamanisticRage = currentTime;
                    TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} using Shamanistic Rage",
                                 GetBot()->GetName());
                    return true;
                }
            }
        }

        // Healing Stream Totem for passive healing
        if (!_activeTotems[static_cast<size_t>(TotemType::WATER)].isActive ||
            _activeTotems[static_cast<size_t>(TotemType::WATER)].spellId != SPELL_HEALING_STREAM_TOTEM)
        {
            if (DeployTotem(SPELL_HEALING_STREAM_TOTEM, TotemType::WATER))
            {
                _healingStreamTotemTime = getMSTime();
                TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} deploying Healing Stream Totem",
                             GetBot()->GetName());
                return true;
            }
        }

        // Stoneclaw Totem for damage absorption
        if (CanUseAbility(SPELL_STONECLAW_TOTEM))
        {
            if (CastSpell(SPELL_STONECLAW_TOTEM))
            {
                TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} using Stoneclaw Totem for shield",
                             GetBot()->GetName());
                return true;
            }
        }
    }

    // Spirit Walk for root/snare removal
    if (GetBot()->HasUnitState(UNIT_STATE_ROOT) || GetBot()->GetSpeedRate(MOVE_RUN) < 1.0f)
    {
        if (CanUseAbility(SPELL_SPIRIT_WALK))
        {
            uint32 currentTime = getMSTime();
            if (currentTime - _lastSpiritWalk > 120000) // 2 min cooldown
            {
                if (CastSpell(SPELL_SPIRIT_WALK))
                {
                    _lastSpiritWalk = currentTime;
                    TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} using Spirit Walk to break roots",
                                 GetBot()->GetName());
                    return true;
                }
            }
        }

        // Ghost Wolf as backup escape
        if (!GetBot()->HasAura(SPELL_GHOST_WOLF))
        {
            if (CastSpell(SPELL_GHOST_WOLF))
            {
                TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} shifting to Ghost Wolf form",
                             GetBot()->GetName());
                return true;
            }
        }
    }

    return false;
}

bool ShamanAI::HandlePositioning(::Unit* target)
{
    auto* behaviors = GetCombatBehaviors();
    if (!behaviors || !target)
        return false;

    if (!behaviors->NeedsRepositioning())
        return false;

    float currentDistance = GetBot()->GetDistance(target);
    float optimalRange = GetOptimalRange(target);

    // Enhancement needs to be in melee range
    if (_currentSpec == ShamanSpec::ENHANCEMENT)
    {
        if (currentDistance > OPTIMAL_MELEE_RANGE)
        {
            // Use Ghost Wolf for gap closing
            if (currentDistance > 15.0f && !GetBot()->HasAura(SPELL_GHOST_WOLF))
            {
                if (CastSpell(SPELL_GHOST_WOLF))
                {
                    TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} using Ghost Wolf to close gap",
                                 GetBot()->GetName());
                    return true;
                }
            }

            // Feral Spirit for additional damage while closing
            if (CanUseAbility(SPELL_FERAL_SPIRIT))
            {
                if (CastSpell(SPELL_FERAL_SPIRIT))
                {
                    TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} summoning Feral Spirits",
                                 GetBot()->GetName());
                    return true;
                }
            }
        }
    }
    // Elemental and Restoration need to maintain range
    else
    {
        if (currentDistance < 8.0f)
        {
            // Thunderstorm for knockback
            if (_currentSpec == ShamanSpec::ELEMENTAL && CanUseAbility(SPELL_THUNDERSTORM))
            {
                if (CastSpell(SPELL_THUNDERSTORM))
                {
                    TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} using Thunderstorm for knockback",
                                 GetBot()->GetName());
                    return true;
                }
            }

            // Earthbind Totem for slowing
            if (!_activeTotems[static_cast<size_t>(TotemType::EARTH)].isActive ||
                _activeTotems[static_cast<size_t>(TotemType::EARTH)].spellId != SPELL_EARTHBIND_TOTEM)
            {
                if (DeployTotem(SPELL_EARTHBIND_TOTEM, TotemType::EARTH))
                {
                    TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} placing Earthbind Totem for kiting",
                                 GetBot()->GetName());
                    return true;
                }
            }

            // Frost Shock for slowing while kiting
            if (CanUseAbility(SPELL_FROST_SHOCK))
            {
                if (CastSpell(target, SPELL_FROST_SHOCK))
                {
                    TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} using Frost Shock to slow target",
                                 GetBot()->GetName());
                    return true;
                }
            }
        }
    }

    return false;
}

bool ShamanAI::HandleTotemManagement(::Unit* target)
{
    if (!target)
        return false;

    uint32 currentTime = getMSTime();

    // Only update totems periodically
    if (currentTime - _lastTotemUpdate < TOTEM_UPDATE_INTERVAL)
        return false;

    _lastTotemUpdate = currentTime;

    // Check each totem type and deploy if needed
    for (uint8 i = 0; i < 4; ++i) // 4 totem types: FIRE, EARTH, WATER, AIR
    {
        TotemType type = static_cast<TotemType>(i);

        if (NeedsTotemRefresh(type))
        {
            uint32 totemSpell = GetOptimalTotem(type, target);
            if (totemSpell && CanUseAbility(totemSpell))
            {
                if (DeployTotem(totemSpell, type))
                {
                    TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} deploying totem {} for slot {}",
                                 GetBot()->GetName(), totemSpell, i);
                    return true;
                }
            }
        }
    }

    return false;
}

bool ShamanAI::HandleTargetSwitching(::Unit* target)
{
    auto* behaviors = GetCombatBehaviors();
    if (!behaviors || !target)
        return false;

    if (!behaviors->ShouldSwitchTarget())
        return false;

    Unit* priorityTarget = behaviors->GetPriorityTarget();
    if (!priorityTarget || priorityTarget == target)
        return false;

    // Hex the current target if it's not the priority
    if (CanUseAbility(SPELL_HEX))
    {
        if (!target->HasAura(SPELL_HEX) && target->GetTypeId() == TYPEID_UNIT)
        {
            if (CastSpell(target, SPELL_HEX))
            {
                TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} hexing {} to switch targets",
                             GetBot()->GetName(), target->GetName());

                // Update target
                SetTarget(priorityTarget->GetGUID());
                return true;
            }
        }
    }

    // Apply Flame Shock to new target for Elemental
    if (_currentSpec == ShamanSpec::ELEMENTAL)
    {
        if (!HasFlameShockOnTarget(priorityTarget))
        {
            if (HandleFlameShock(priorityTarget))
            {
                SetTarget(priorityTarget->GetGUID());
                return true;
            }
        }
    }

    return false;
}

bool ShamanAI::HandlePurgeDispel(::Unit* target)
{
    if (!target)
        return false;

    // Purge enemy buffs
    if (target->IsHostileTo(GetBot()))
    {
        if (CanUseAbility(SPELL_PURGE))
        {
            // Check if target has purgeable buffs
            bool hasPurgeableBuff = false;
            Unit::AuraApplicationMap const& auras = target->GetAppliedAuras();
            for (auto const& [auraId, aurApp] : auras)
            {
                if (aurApp->GetBase()->IsPositive() && aurApp->GetBase()->GetSpellInfo()->Dispel == DISPEL_MAGIC)
                {
                    hasPurgeableBuff = true;
                    break;
                }
            }

            if (hasPurgeableBuff)
            {
                if (CastSpell(target, SPELL_PURGE))
                {
                    TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} purging buffs from {}",
                                 GetBot()->GetName(), target->GetName());
                    return true;
                }
            }
        }
    }

    // Cleanse Spirit for friendly dispels (Restoration)
    if (_currentSpec == ShamanSpec::RESTORATION)
    {
        // Check group members for debuffs
        if (Group* group = GetBot()->GetGroup())
        {
            for (GroupReference const& itr : group->GetMembers())
            {
                if (Player* member = itr.GetSource())
                {
                    if (!member->IsAlive() || member->GetDistance(GetBot()) > 40.0f)
                        continue;

                    // Check for dispellable debuffs
                    bool hasDispellableDebuff = false;
                    Unit::AuraApplicationMap const& auras = member->GetAppliedAuras();
                    for (auto const& [auraId, aurApp] : auras)
                    {
                        if (!aurApp->GetBase()->IsPositive())
                        {
                            uint32 dispelType = aurApp->GetBase()->GetSpellInfo()->Dispel;
                            if (dispelType == DISPEL_CURSE)
                            {
                                hasDispellableDebuff = true;
                                break;
                            }
                        }
                    }

                    if (hasDispellableDebuff && CanUseAbility(SPELL_CLEANSE_SPIRIT))
                    {
                        if (CastSpell(member, SPELL_CLEANSE_SPIRIT))
                        {
                            TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} cleansing {} with Cleanse Spirit",
                                         GetBot()->GetName(), member->GetName());
                            return true;
                        }
                    }
                }
            }
        }
    }

    return false;
}

bool ShamanAI::HandleAoEDecisions(::Unit* target)
{
    auto* behaviors = GetCombatBehaviors();
    if (!behaviors || !target)
        return false;

    if (!behaviors->ShouldAOE())
        return false;

    // Count nearby enemies
    std::list<Unit*> enemies;
    GetBot()->GetAttackableUnitListInRange(enemies, 40.0f);

    if (enemies.size() < 3)
        return false;

    switch (_currentSpec)
    {
        case ShamanSpec::ELEMENTAL:
        {
            // Earthquake for ground AoE
            if (CanUseAbility(SPELL_EARTHQUAKE))
            {
                // Note: Ground-targeted abilities need special handling
                TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} casting Earthquake for AoE",
                             GetBot()->GetName());
                // Would need actual ground targeting implementation
                return true;
            }

            // Chain Lightning for cleave
            if (HandleChainLightning(target))
                return true;

            // Lava Beam during Ascendance
            if (GetBot()->HasAura(SPELL_ASCENDANCE) && CanUseAbility(SPELL_LAVA_BEAM))
            {
                if (CastSpell(target, SPELL_LAVA_BEAM))
                {
                    TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} using Lava Beam in Ascendance",
                                 GetBot()->GetName());
                    return true;
                }
            }

            // Liquid Magma Totem
            if (CanUseAbility(SPELL_LIQUID_MAGMA_TOTEM))
            {
                if (DeployTotem(SPELL_LIQUID_MAGMA_TOTEM, TotemType::FIRE))
                {
                    TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} deploying Liquid Magma Totem",
                                 GetBot()->GetName());
                    return true;
                }
            }
            break;
        }

        case ShamanSpec::ENHANCEMENT:
        {
            // Crash Lightning for melee AoE
            if (HandleCrashLightning())
                return true;

            // Fire Nova with Flame Shock spread
            if (_hasFlameShockUp && CanUseAbility(SPELL_FIRE_NOVA_TOTEM))
            {
                if (CastSpell(SPELL_FIRE_NOVA_TOTEM))
                {
                    TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} using Fire Nova",
                                 GetBot()->GetName());
                    return true;
                }
            }

            // Chain Lightning with Maelstrom Weapon
            if (ShouldUseInstantLightningBolt() && CanUseAbility(SPELL_CHAIN_LIGHTNING))
            {
                if (CastSpell(target, SPELL_CHAIN_LIGHTNING))
                {
                    TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} instant Chain Lightning with Maelstrom",
                                 GetBot()->GetName());
                    _maelstromWeaponStacks = 0;
                    return true;
                }
            }

            // Sundering for cone AoE
            if (CanUseAbility(SPELL_SUNDERING))
            {
                if (CastSpell(SPELL_SUNDERING))
                {
                    TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} using Sundering",
                                 GetBot()->GetName());
                    return true;
                }
            }
            break;
        }

        case ShamanSpec::RESTORATION:
        {
            // Chain Heal for group healing
            if (CountInjuredGroupMembers(80.0f) >= 3)
            {
                if (HandleChainHeal())
                    return true;
            }

            // Healing Rain for area healing
            if (HandleHealingRain())
                return true;

            // Spirit Link Totem for health redistribution
            if (CountInjuredGroupMembers(50.0f) >= 2)
            {
                if (HandleSpiritLink())
                    return true;
            }
            break;
        }
    }

    return false;
}

bool ShamanAI::HandleOffensiveCooldowns(::Unit* target)
{
    auto* behaviors = GetCombatBehaviors();
    if (!behaviors || !target)
        return false;

    if (!behaviors->ShouldUseCooldowns())
        return false;

    uint32 currentTime = getMSTime();

    // Bloodlust/Heroism - raid-wide haste
    if (ShouldUseBloodlust())
    {
        uint32 spellId = GetBot()->GetTeamId() == TEAM_ALLIANCE ? SPELL_HEROISM : SPELL_BLOODLUST;
        if (CanUseAbility(spellId) && currentTime - _lastBloodlust > 600000) // 10 min debuff
        {
            if (CastSpell(spellId))
            {
                _lastBloodlust = currentTime;
                TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} casting Bloodlust/Heroism",
                             GetBot()->GetName());
                return true;
            }
        }
    }

    switch (_currentSpec)
    {
        case ShamanSpec::ELEMENTAL:
        {
            // Ascendance for Lava Beam
            if (ShouldUseAscendance() && CanUseAbility(SPELL_ASCENDANCE))
            {
                if (currentTime - _lastAscendance > 180000) // 3 min cooldown
                {
                    if (CastSpell(SPELL_ASCENDANCE))
                    {
                        _lastAscendance = currentTime;
                        TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} activating Elemental Ascendance",
                                     GetBot()->GetName());
                        return true;
                    }
                }
            }

            // Elemental Mastery for instant cast
            if (ShouldUseElementalMastery() && CanUseAbility(SPELL_ELEMENTAL_MASTERY))
            {
                if (currentTime - _lastElementalMastery > 90000) // 1.5 min cooldown
                {
                    if (CastSpell(SPELL_ELEMENTAL_MASTERY))
                    {
                        _lastElementalMastery = currentTime;
                        TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} using Elemental Mastery",
                                     GetBot()->GetName());
                        return true;
                    }
                }
            }

            // Fire Elemental Totem
            if (CanUseAbility(SPELL_FIRE_ELEMENTAL_TOTEM))
            {
                if (currentTime - _lastFireElemental > 300000) // 5 min cooldown
                {
                    if (DeployTotem(SPELL_FIRE_ELEMENTAL_TOTEM, TotemType::FIRE))
                    {
                        _lastFireElemental = currentTime;
                        TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} summoning Fire Elemental",
                                     GetBot()->GetName());
                        return true;
                    }
                }
            }

            // Stormkeeper for empowered Lightning Bolts
            if (CanUseAbility(SPELL_STORMKEEPER))
            {
                if (CastSpell(SPELL_STORMKEEPER))
                {
                    TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} casting Stormkeeper",
                                 GetBot()->GetName());
                    return true;
                }
            }
            break;
        }

        case ShamanSpec::ENHANCEMENT:
        {
            // Ascendance for Windstrike
            if (ShouldUseAscendance() && CanUseAbility(SPELL_ASCENDANCE))
            {
                if (currentTime - _lastAscendance > 180000)
                {
                    if (CastSpell(SPELL_ASCENDANCE))
                    {
                        _lastAscendance = currentTime;
                        TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} activating Enhancement Ascendance",
                                     GetBot()->GetName());
                        return true;
                    }
                }
            }

            // Doom Winds for Windfury procs
            if (CanUseAbility(SPELL_DOOM_WINDS))
            {
                if (CastSpell(SPELL_DOOM_WINDS))
                {
                    TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} activating Doom Winds",
                                 GetBot()->GetName());
                    return true;
                }
            }

            // Feral Spirit wolves
            if (CanUseAbility(SPELL_FERAL_SPIRIT))
            {
                if (CastSpell(SPELL_FERAL_SPIRIT))
                {
                    TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} summoning Feral Spirits",
                                 GetBot()->GetName());
                    return true;
                }
            }
            break;
        }

        case ShamanSpec::RESTORATION:
        {
            // Ascendance for spreading heals
            if (CountInjuredGroupMembers(60.0f) >= 3 && CanUseAbility(SPELL_ASCENDANCE))
            {
                if (currentTime - _lastAscendance > 180000)
                {
                    if (CastSpell(SPELL_ASCENDANCE))
                    {
                        _lastAscendance = currentTime;
                        TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} activating Restoration Ascendance",
                                     GetBot()->GetName());
                        return true;
                    }
                }
            }

            // Healing Tide Totem for major healing
            if (CountInjuredGroupMembers(50.0f) >= 3 && CanUseAbility(SPELL_HEALING_TIDE_TOTEM))
            {
                if (DeployTotem(SPELL_HEALING_TIDE_TOTEM, TotemType::WATER))
                {
                    TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} deploying Healing Tide Totem",
                                 GetBot()->GetName());
                    return true;
                }
            }

            // Ancestral Guidance for healing while dealing damage
            if (CanUseAbility(SPELL_ANCESTRAL_GUIDANCE))
            {
                if (CastSpell(SPELL_ANCESTRAL_GUIDANCE))
                {
                    TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} activating Ancestral Guidance",
                                 GetBot()->GetName());
                    return true;
                }
            }

            // Mana Tide Totem for mana restoration
            if (GetBot()->GetPowerPct(POWER_MANA) < 30.0f && CanUseAbility(SPELL_MANA_TIDE_TOTEM))
            {
                if (DeployTotem(SPELL_MANA_TIDE_TOTEM, TotemType::WATER))
                {
                    TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} deploying Mana Tide Totem",
                                 GetBot()->GetName());
                    return true;
                }
            }
            break;
        }
    }

    return false;
}

bool ShamanAI::HandleResourceManagement()
{
    auto* behaviors = GetCombatBehaviors();

    switch (_currentSpec)
    {
        case ShamanSpec::ELEMENTAL:
        {
            // Manage Elemental Maelstrom resource
            uint32 maelstrom = GetElementalMaelstrom();

            // Spend maelstrom if capped
            if (maelstrom >= 90)
            {
                // Earth Shock to dump maelstrom
                if (CanUseAbility(SPELL_EARTH_SHOCK))
                {
                    if (CastSpell(_currentTarget, SPELL_EARTH_SHOCK))
                    {
                        _elementalMaelstrom -= 60;
                        TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} spending maelstrom with Earth Shock",
                                     GetBot()->GetName());
                        return true;
                    }
                }
            }
            break;
        }

        case ShamanSpec::ENHANCEMENT:
        {
            // Check Maelstrom Weapon stacks
            _maelstromWeaponStacks = GetMaelstromWeaponStacks();

            // Use instant cast at 5 stacks
            if (_maelstromWeaponStacks >= MAELSTROM_WEAPON_MAX)
            {
                // Instant Lightning Bolt for single target
                if (CanUseAbility(SPELL_LIGHTNING_BOLT))
                {
                    if (CastSpell(_currentTarget, SPELL_LIGHTNING_BOLT))
                    {
                        _maelstromWeaponStacks = 0;
                        TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} instant Lightning Bolt with Maelstrom",
                                     GetBot()->GetName());
                        return true;
                    }
                }
            }
            break;
        }

        case ShamanSpec::RESTORATION:
        {
            // Mana management for Restoration
            float manaPct = GetBot()->GetPowerPct(POWER_MANA);

            if (behaviors && behaviors->ShouldConserveMana())
            {
                // Use more efficient heals when low on mana
                if (manaPct < 30.0f)
                {
                    // Mana Spring Totem for regeneration
                    if (!_activeTotems[static_cast<size_t>(TotemType::WATER)].isActive ||
                        _activeTotems[static_cast<size_t>(TotemType::WATER)].spellId != SPELL_MANA_SPRING_TOTEM)
                    {
                        if (DeployTotem(SPELL_MANA_SPRING_TOTEM, TotemType::WATER))
                        {
                            TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} deploying Mana Spring Totem",
                                         GetBot()->GetName());
                            return true;
                        }
                    }
                }
            }
            break;
        }
    }

    return false;
}

bool ShamanAI::HandleNormalRotation(::Unit* target)
{
    if (!target)
        return false;

    switch (_currentSpec)
    {
        case ShamanSpec::ELEMENTAL:
            return UpdateElementalRotation(target);

        case ShamanSpec::ENHANCEMENT:
            return UpdateEnhancementRotation(target);

        case ShamanSpec::RESTORATION:
            return UpdateRestorationRotation(target);

        default:
            return false;
    }
}

// Elemental rotation implementation
bool ShamanAI::UpdateElementalRotation(::Unit* target)
{
    if (!target)
        return false;

    // Maintain Flame Shock
    if (HandleFlameShock(target))
        return true;

    // Lava Burst on cooldown (especially with Flame Shock up)
    if (HandleLavaBurst(target))
        return true;

    // Elemental Blast if talented
    if (HandleElementalBlast(target))
        return true;

    // Chain Lightning for cleave
    if (HandleChainLightning(target))
        return true;

    // Lightning Bolt as filler
    if (CanUseAbility(SPELL_LIGHTNING_BOLT))
    {
        if (CastSpell(target, SPELL_LIGHTNING_BOLT))
        {
            _elementalMaelstrom += 8;
            TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} casting Lightning Bolt",
                         GetBot()->GetName());
            return true;
        }
    }

    return false;
}

bool ShamanAI::HandleLavaBurst(::Unit* target)
{
    if (!target || !CanUseAbility(SPELL_LAVA_BURST))
        return false;

    // Check for Lava Surge proc
    CheckLavaSurgeProc();

    // Always use if we have charges or proc
    if (_lavaBurstCharges > 0 || _hasLavaSurgeProc)
    {
        // Guaranteed crit if Flame Shock is up
        if (HasFlameShockOnTarget(target))
        {
            if (CastSpell(target, SPELL_LAVA_BURST))
            {
                if (_hasLavaSurgeProc)
                    _hasLavaSurgeProc = false;
                else
                    _lavaBurstCharges--;

                _elementalMaelstrom += 10;
                TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} casting Lava Burst (charges: {})",
                             GetBot()->GetName(), _lavaBurstCharges);
                return true;
            }
        }
    }

    return false;
}

bool ShamanAI::HandleFlameShock(::Unit* target)
{
    if (!target || !CanUseAbility(SPELL_FLAME_SHOCK))
        return false;

    // Check if Flame Shock needs refresh
    if (!HasFlameShockOnTarget(target) ||
        (getMSTime() - _flameshockExpiry < 9000)) // Refresh at <9 seconds
    {
        if (CastSpell(target, SPELL_FLAME_SHOCK))
        {
            _flameshockTarget = target->GetGUID().GetCounter();
            _flameshockExpiry = getMSTime() + FLAME_SHOCK_DURATION;
            _hasFlameShockUp = true;
            _elementalMaelstrom += 20;
            TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} applying Flame Shock to {}",
                         GetBot()->GetName(), target->GetName());
            return true;
        }
    }

    return false;
}

bool ShamanAI::HandleChainLightning(::Unit* target)
{
    if (!target || !CanUseAbility(SPELL_CHAIN_LIGHTNING))
        return false;

    // Use Chain Lightning if there are multiple targets
    std::list<Unit*> enemies;
    GetBot()->GetAttackableUnitListInRange(enemies, 30.0f);

    if (enemies.size() >= 2)
    {
        if (CastSpell(target, SPELL_CHAIN_LIGHTNING))
        {
            _elementalMaelstrom += 4 * std::min(static_cast<size_t>(5), enemies.size());
            TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} casting Chain Lightning",
                         GetBot()->GetName());
            return true;
        }
    }

    return false;
}

// Enhancement rotation implementation
bool ShamanAI::UpdateEnhancementRotation(::Unit* target)
{
    if (!target || !IsInMeleeRange(target))
        return false;

    // Maintain Flame Shock for Lava Lash
    if (!HasFlameShockOnTarget(target))
    {
        if (HandleFlameShock(target))
            return true;
    }

    // Stormstrike on cooldown
    if (HandleStormstrike(target))
        return true;

    // Windstrike during Ascendance
    if (GetBot()->HasAura(SPELL_ASCENDANCE))
    {
        if (HandleWindstrike(target))
            return true;
    }

    // Lava Lash with Flame Shock up
    if (_hasFlameShockUp)
    {
        if (HandleLavaLash(target))
            return true;
    }

    // Crash Lightning for AoE or buff
    if (HandleCrashLightning())
        return true;

    // Maelstrom Weapon instant casts
    if (HandleMaelstromWeapon())
        return true;

    // Auto attack
    if (!GetBot()->IsAutoAttackTarget(target))
    {
        GetBot()->Attack(target, true);
    }

    return false;
}

bool ShamanAI::HandleStormstrike(::Unit* target)
{
    if (!target || !CanUseAbility(SPELL_STORMSTRIKE))
        return false;

    if (IsInMeleeRange(target))
    {
        if (CastSpell(target, SPELL_STORMSTRIKE))
        {
            TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} using Stormstrike on {}",
                         GetBot()->GetName(), target->GetName());
            return true;
        }
    }

    return false;
}

bool ShamanAI::HandleLavaLash(::Unit* target)
{
    if (!target || !CanUseAbility(SPELL_LAVA_LASH))
        return false;

    if (IsInMeleeRange(target))
    {
        if (CastSpell(target, SPELL_LAVA_LASH))
        {
            TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} using Lava Lash on {}",
                         GetBot()->GetName(), target->GetName());
            return true;
        }
    }

    return false;
}

bool ShamanAI::HandleMaelstromWeapon()
{
    if (_maelstromWeaponStacks < MAELSTROM_WEAPON_MAX)
        return false;

    // Use instant Lightning Bolt at 5 stacks
    if (CanUseAbility(SPELL_LIGHTNING_BOLT))
    {
        if (CastSpell(_currentTarget, SPELL_LIGHTNING_BOLT))
        {
            _maelstromWeaponStacks = 0;
            TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} instant Lightning Bolt with Maelstrom Weapon",
                         GetBot()->GetName());
            return true;
        }
    }

    return false;
}

// Restoration rotation implementation
bool ShamanAI::UpdateRestorationRotation(::Unit* target)
{
    // Priority healing for group members
    Player* lowestHealth = GetLowestHealthGroupMember();

    if (lowestHealth)
    {
        float healthPct = lowestHealth->GetHealthPct();

        // Emergency healing
        if (healthPct < 30.0f)
        {
            // Healing Surge for fast healing
            if (CanUseAbility(SPELL_HEALING_SURGE))
            {
                if (CastSpell(lowestHealth, SPELL_HEALING_SURGE))
                {
                    TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} emergency Healing Surge on {}",
                                 GetBot()->GetName(), lowestHealth->GetName());
                    return true;
                }
            }
        }

        // Riptide for instant heal + HoT
        if (healthPct < 80.0f && !lowestHealth->HasAura(SPELL_RIPTIDE))
        {
            if (HandleRiptide(lowestHealth))
                return true;
        }

        // Chain Heal for group healing
        if (CountInjuredGroupMembers(70.0f) >= 2)
        {
            if (HandleChainHeal())
                return true;
        }

        // Healing Wave for efficient healing
        if (healthPct < 70.0f)
        {
            if (HandleHealingWave(lowestHealth))
                return true;
        }
    }

    // Maintain Healing Stream Totem
    if (HandleHealingStreamTotem())
        return true;

    // Damage if no healing needed
    if (target && target->IsHostileTo(GetBot()))
    {
        // Lightning Bolt for damage
        if (CanUseAbility(SPELL_LIGHTNING_BOLT))
        {
            if (CastSpell(target, SPELL_LIGHTNING_BOLT))
            {
                TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} dealing damage with Lightning Bolt",
                             GetBot()->GetName());
                return true;
            }
        }
    }

    return false;
}

bool ShamanAI::HandleRiptide(Player* target)
{
    if (!target || !CanUseAbility(SPELL_RIPTIDE))
        return false;

    if (!target->HasAura(SPELL_RIPTIDE))
    {
        if (CastSpell(target, SPELL_RIPTIDE))
        {
            TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} casting Riptide on {}",
                         GetBot()->GetName(), target->GetName());
            return true;
        }
    }

    return false;
}

bool ShamanAI::HandleChainHeal()
{
    if (!CanUseAbility(SPELL_CHAIN_HEAL))
        return false;

    Player* target = GetLowestHealthGroupMember();
    if (target)
    {
        if (CastSpell(target, SPELL_CHAIN_HEAL))
        {
            TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} casting Chain Heal starting from {}",
                         GetBot()->GetName(), target->GetName());
            return true;
        }
    }

    return false;
}

// Totem management helpers
bool ShamanAI::NeedsTotemRefresh(TotemType type) const
{
    const TotemInfo& totem = _activeTotems[static_cast<size_t>(type)];

    // No totem active
    if (!totem.isActive)
        return true;

    // Totem expired (most totems last 2 minutes)
    uint32 currentTime = getMSTime();
    if (currentTime - totem.deployTime > 120000)
        return true;

    // Totem out of range
    if (!IsTotemInRange(type, _currentTarget))
        return true;

    return false;
}

uint32 ShamanAI::GetOptimalTotem(TotemType type, ::Unit* target) const
{
    if (!target)
        return 0;

    switch (type)
    {
        case TotemType::FIRE:
        {
            switch (_currentSpec)
            {
                case ShamanSpec::ELEMENTAL:
                    // Searing Totem for single target damage
                    return SPELL_SEARING_TOTEM;

                case ShamanSpec::ENHANCEMENT:
                    // Magma Totem for AoE
                    if (GetBot()->GetDistance(target) <= 8.0f)
                        return SPELL_MAGMA_TOTEM;
                    return SPELL_SEARING_TOTEM;

                case ShamanSpec::RESTORATION:
                    // Flametongue Totem for spell power buff
                    return SPELL_FLAMETONGUE_TOTEM;
            }
            break;
        }

        case TotemType::EARTH:
        {
            // Stoneskin Totem for physical mitigation
            if (target->GetTypeId() == TYPEID_UNIT && target->ToCreature()->IsDungeonBoss())
                return SPELL_STONESKIN_TOTEM;

            // Strength of Earth for melee
            if (_currentSpec == ShamanSpec::ENHANCEMENT)
                return SPELL_STRENGTH_OF_EARTH_TOTEM;

            // Earthbind for kiting
            if (_currentSpec != ShamanSpec::ENHANCEMENT && GetBot()->GetDistance(target) < 15.0f)
                return SPELL_EARTHBIND_TOTEM;

            return SPELL_STONESKIN_TOTEM;
        }

        case TotemType::WATER:
        {
            switch (_currentSpec)
            {
                case ShamanSpec::RESTORATION:
                    // Healing Stream for constant healing
                    return SPELL_HEALING_STREAM_TOTEM;

                default:
                    // Mana Spring for mana regen
                    if (GetBot()->GetPowerPct(POWER_MANA) < 70.0f)
                        return SPELL_MANA_SPRING_TOTEM;
                    return SPELL_HEALING_STREAM_TOTEM;
            }
            break;
        }

        case TotemType::AIR:
        {
            switch (_currentSpec)
            {
                case ShamanSpec::ENHANCEMENT:
                    // Windfury for attack speed
                    return SPELL_WINDFURY_TOTEM;

                case ShamanSpec::ELEMENTAL:
                    // Wrath of Air for spell haste
                    return SPELL_WRATH_OF_AIR_TOTEM;

                case ShamanSpec::RESTORATION:
                    // Grace of Air for agility
                    return SPELL_GRACE_OF_AIR_TOTEM;
            }
            break;
        }
    }

    return 0;
}

bool ShamanAI::DeployTotem(uint32 spellId, TotemType type)
{
    if (!CanUseAbility(spellId))
        return false;

    if (CastSpell(spellId))
    {
        TotemInfo& totem = _activeTotems[static_cast<size_t>(type)];
        totem.spellId = spellId;
        totem.deployTime = getMSTime();
        totem.position = GetBot()->GetPosition();
        totem.isActive = true;

        _totemsDeploy++;

        TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} deployed totem {} in slot {}",
                     GetBot()->GetName(), spellId, static_cast<uint32>(type));
        return true;
    }

    return false;
}

bool ShamanAI::IsTotemInRange(TotemType type, ::Unit* target) const
{
    if (!target)
        return false;

    const TotemInfo& totem = _activeTotems[static_cast<size_t>(type)];
    if (!totem.isActive)
        return false;

    // Check distance from totem position to target
    float distance = target->GetDistance(totem.position);
    return distance <= TOTEM_EFFECT_RANGE;
}

// Helper methods
bool ShamanAI::IsInMeleeRange(::Unit* target) const
{
    return target && GetBot()->GetDistance(target) <= OPTIMAL_MELEE_RANGE;
}

bool ShamanAI::HasFlameShockOnTarget(::Unit* target) const
{
    if (!target)
        return false;

    return target->HasAura(SPELL_FLAME_SHOCK, GetBot()->GetGUID());
}

void ShamanAI::RefreshFlameShock(::Unit* target)
{
    if (!target)
        return;

    HandleFlameShock(target);
}

bool ShamanAI::CheckLavaSurgeProc()
{
    // Check for Lava Surge buff that gives instant Lava Burst
    // This would check for the actual proc buff ID
    _hasLavaSurgeProc = false; // GetBot()->HasAura(LAVA_SURGE_PROC_BUFF);
    return _hasLavaSurgeProc;
}

uint32 ShamanAI::GetElementalMaelstrom() const
{
    // In real implementation, this would read the actual maelstrom power
    return _elementalMaelstrom;
}

uint32 ShamanAI::GetMaelstromWeaponStacks() const
{
    // In real implementation, this would check the Maelstrom Weapon buff stacks
    return _maelstromWeaponStacks;
}

bool ShamanAI::ShouldUseInstantLightningBolt() const
{
    return _maelstromWeaponStacks >= MAELSTROM_WEAPON_MAX;
}

bool ShamanAI::ShouldUseAscendance() const
{
    // Use on boss fights or when multiple enemies
    if (_currentTarget && _currentTarget->GetMaxHealth() > 1000000)
        return true;

    std::list<Unit*> enemies;
    GetBot()->GetAttackableUnitListInRange(enemies, 40.0f);
    return enemies.size() >= 3;
}

bool ShamanAI::ShouldUseElementalMastery() const
{
    // Use when we need burst damage
    return _currentTarget && _currentTarget->GetHealthPct() < 30.0f;
}

Player* ShamanAI::GetLowestHealthGroupMember() const
{
    Player* lowest = nullptr;
    float lowestPct = 100.0f;

    if (Group* group = GetBot()->GetGroup())
    {
        for (GroupReference const& itr : group->GetMembers())
        {
            if (Player* member = itr.GetSource())
            {
                if (!member->IsAlive() || member->GetDistance(GetBot()) > 40.0f)
                    continue;

                float healthPct = member->GetHealthPct();
                if (healthPct < lowestPct)
                {
                    lowestPct = healthPct;
                    lowest = member;
                }
            }
        }
    }

    // Check self if not in group or lowest
    if (!lowest || GetBot()->GetHealthPct() < lowestPct)
        lowest = GetBot();

    return lowest;
}

uint32 ShamanAI::CountInjuredGroupMembers(float healthThreshold) const
{
    uint32 count = 0;

    if (Group* group = GetBot()->GetGroup())
    {
        for (GroupReference const& itr : group->GetMembers())
        {
            if (Player* member = itr.GetSource())
            {
                if (member->IsAlive() && member->GetHealthPct() < healthThreshold &&
                    member->GetDistance(GetBot()) <= 40.0f)
                {
                    count++;
                }
            }
        }
    }
    else if (GetBot()->GetHealthPct() < healthThreshold)
    {
        count = 1;
    }

    return count;
}

bool ShamanAI::HandleCrashLightning()
{
    if (!CanUseAbility(SPELL_CRASH_LIGHTNING))
        return false;

    // Use if multiple enemies nearby
    std::list<Unit*> enemies;
    GetBot()->GetAttackableUnitListInRange(enemies, 8.0f);

    if (enemies.size() >= 2 || (_currentTarget && IsInMeleeRange(_currentTarget)))
    {
        if (CastSpell(SPELL_CRASH_LIGHTNING))
        {
            TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} using Crash Lightning",
                         GetBot()->GetName());
            return true;
        }
    }

    return false;
}

bool ShamanAI::HandleWindstrike(::Unit* target)
{
    if (!target || !CanUseAbility(SPELL_WINDSTRIKE) || !GetBot()->HasAura(SPELL_ASCENDANCE))
        return false;

    if (IsInMeleeRange(target))
    {
        if (CastSpell(target, SPELL_WINDSTRIKE))
        {
            TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} using Windstrike during Ascendance",
                         GetBot()->GetName());
            return true;
        }
    }

    return false;
}

bool ShamanAI::HandleEarthquake()
{
    if (!CanUseAbility(SPELL_EARTHQUAKE))
        return false;

    // Use if enough maelstrom and multiple enemies
    if (_elementalMaelstrom >= 60)
    {
        std::list<Unit*> enemies;
        GetBot()->GetAttackableUnitListInRange(enemies, 40.0f);

        if (enemies.size() >= 3)
        {
            // Note: Ground-targeted spell, needs special handling
            TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} casting Earthquake",
                         GetBot()->GetName());
            _elementalMaelstrom -= 60;
            return true;
        }
    }

    return false;
}

bool ShamanAI::HandleElementalBlast(::Unit* target)
{
    if (!target || !CanUseAbility(SPELL_ELEMENTAL_BLAST))
        return false;

    if (CastSpell(target, SPELL_ELEMENTAL_BLAST))
    {
        _elementalMaelstrom += 25;
        TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} casting Elemental Blast",
                     GetBot()->GetName());
        return true;
    }

    return false;
}

bool ShamanAI::HandleHealingWave(Player* target)
{
    if (!target || !CanUseAbility(SPELL_HEALING_WAVE))
        return false;

    if (CastSpell(target, SPELL_HEALING_WAVE))
    {
        TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} casting Healing Wave on {}",
                     GetBot()->GetName(), target->GetName());
        return true;
    }

    return false;
}

bool ShamanAI::HandleHealingRain()
{
    if (!CanUseAbility(SPELL_HEALING_RAIN))
        return false;

    // Use if multiple injured group members
    if (CountInjuredGroupMembers(70.0f) >= 3)
    {
        // Note: Ground-targeted spell
        TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} casting Healing Rain",
                     GetBot()->GetName());
        return true;
    }

    return false;
}

bool ShamanAI::HandleHealingStreamTotem()
{
    // Check if we need healing stream totem
    if (_activeTotems[static_cast<size_t>(TotemType::WATER)].spellId == SPELL_HEALING_STREAM_TOTEM &&
        _activeTotems[static_cast<size_t>(TotemType::WATER)].isActive)
    {
        return false;
    }

    if (CountInjuredGroupMembers(90.0f) >= 1)
    {
        if (DeployTotem(SPELL_HEALING_STREAM_TOTEM, TotemType::WATER))
        {
            _healingStreamTotemTime = getMSTime();
            TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} deploying Healing Stream Totem",
                         GetBot()->GetName());
            return true;
        }
    }

    return false;
}

bool ShamanAI::HandleSpiritLink()
{
    if (!CanUseAbility(SPELL_SPIRIT_LINK_TOTEM))
        return false;

    // Use for health redistribution when tank is low
    if (Group* group = GetBot()->GetGroup())
    {
        Player* tank = FindGroupTank(group);
        if (tank && tank->GetHealthPct() < 30.0f)
        {
            if (CastSpell(SPELL_SPIRIT_LINK_TOTEM))
            {
                TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} deploying Spirit Link Totem",
                             GetBot()->GetName());
                return true;
            }
        }
    }

    return false;
}

// Keep all the existing helper methods from the original implementation
void ShamanAI::UpdateBuffs()
{
    if (!GetBot())
        return;

    // Update shields first
    UpdateShamanBuffs();

    // Check weapon imbues
    UpdateWeaponImbues();

    // Delegate additional buffs to specialization
    if (_specialization)
    {
        _specialization->UpdateBuffs();
    }

    // Water walking/breathing utility
    if (!GetBot()->IsInCombat())
    {
        UpdateUtilityBuffs();
    }
}

void ShamanAI::UpdateCooldowns(uint32 diff)
{
    if (!GetBot())
        return;

    // Update ability cooldowns
    for (auto& [spellId, lastUse] : _abilityUsage)
    {
        if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE))
        {
            // Update cooldown tracking
            if (lastUse > 0 && lastUse < diff)
            {
                lastUse = 0;
            }
            else if (lastUse > 0)
            {
                lastUse -= diff;
            }
        }
    }

    // Update Lava Burst charges
    static uint32 lavaBurstRecharge = 0;
    lavaBurstRecharge += diff;
    if (lavaBurstRecharge >= 8000 && _lavaBurstCharges < 2) // 8 sec recharge
    {
        _lavaBurstCharges++;
        lavaBurstRecharge = 0;
    }

    // Delegate to specialization
    if (_specialization)
    {
        _specialization->UpdateCooldowns(diff);
    }
}

bool ShamanAI::CanUseAbility(uint32 spellId)
{
    if (!GetBot())
        return false;

    // Check if spell is learned
    if (!GetBot()->HasSpell(spellId))
        return false;

    // Check if spell is ready
    if (!IsSpellReady(spellId))
        return false;

    // Check resource requirements
    if (!HasEnoughResource(spellId))
        return false;

    // Check specialization-specific requirements
    if (_specialization)
    {
        return _specialization->CanUseAbility(spellId);
    }

    return true;
}

void ShamanAI::OnCombatStart(::Unit* target)
{
    if (!GetBot() || !target)
        return;

    TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} entering combat with {}",
                 GetBot()->GetName(), target->GetName());

    // Deploy initial totems
    DeployInitialTotems(target);

    // Apply initial buffs
    ApplyCombatBuffs();

    // Reset combat tracking
    _hasFlameShockUp = false;
    _flameshockTarget = 0;
    _flameshockExpiry = 0;
    _maelstromWeaponStacks = 0;
    _elementalMaelstrom = 0;
    _lavaBurstCharges = 2;
    _hasLavaSurgeProc = false;

    // Delegate to specialization
    if (_specialization)
    {
        _specialization->OnCombatStart(target);
    }

    // Initialize combat tracking
    _combatTime = 0;
    _inCombat = true;
    _currentTarget = target;
}

void ShamanAI::OnCombatEnd()
{
    if (!GetBot())
        return;

    TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} leaving combat. Metrics - Damage: {}, Healing: {}, Mana Used: {}, Totems: {}, Shocks: {}",
                 GetBot()->GetName(), _damageDealt, _healingDone, _manaSpent, _totemsDeploy, _shocksUsed);

    // Recall unnecessary totems
    RecallCombatTotems();

    // Reset totem tracking
    for (auto& totem : _activeTotems)
    {
        totem.isActive = false;
    }

    // Delegate to specialization
    if (_specialization)
    {
        _specialization->OnCombatEnd();
    }

    // Reset combat tracking
    _inCombat = false;
    _currentTarget = nullptr;

    // Log performance metrics
    LogCombatMetrics();
}

bool ShamanAI::HasEnoughResource(uint32 spellId)
{
    if (!GetBot())
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return false;

    // Check mana cost
    auto powerCosts = spellInfo->CalcPowerCost(GetBot(), spellInfo->GetSchoolMask());
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA && GetBot()->GetPower(POWER_MANA) < int32(cost.Amount))
            return false;
    }

    // Check maelstrom cost for Elemental
    if (_currentSpec == ShamanSpec::ELEMENTAL)
    {
        if (spellId == SPELL_EARTH_SHOCK && _elementalMaelstrom < 60)
            return false;
        if (spellId == SPELL_EARTHQUAKE && _elementalMaelstrom < 60)
            return false;
    }

    // Delegate additional checks to specialization
    if (_specialization)
    {
        return _specialization->HasEnoughResource(spellId);
    }

    return true;
}

void ShamanAI::ConsumeResource(uint32 spellId)
{
    if (!GetBot())
        return;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return;

    // Track mana consumption
    auto powerCosts = spellInfo->CalcPowerCost(GetBot(), spellInfo->GetSchoolMask());
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_MANA)
            _manaSpent += cost.Amount;
    }

    // Track ability usage
    _abilityUsage[spellId] = getMSTime();

    // Track specific spell categories
    if (IsShockSpell(spellId))
    {
        _shocksUsed++;
        _lastShockTime = getMSTime();
    }
    else if (IsTotemSpell(spellId))
    {
        _totemsDeploy++;
    }

    // Delegate to specialization
    if (_specialization)
    {
        _specialization->ConsumeResource(spellId);
    }
}

Position ShamanAI::GetOptimalPosition(::Unit* target)
{
    if (!GetBot() || !target)
        return Position();

    // Use CombatBehaviorIntegration for positioning
    auto* behaviors = GetCombatBehaviors();
    if (behaviors)
    {
        return behaviors->GetOptimalPosition();
    }

    // Fallback to spec-based positioning
    float optimalRange = GetOptimalRange(target);
    float angle = GetBot()->GetAbsoluteAngle(target);
    float x = target->GetPositionX() - optimalRange * std::cos(angle);
    float y = target->GetPositionY() - optimalRange * std::sin(angle);
    float z = target->GetPositionZ();

    return Position(x, y, z);
}

float ShamanAI::GetOptimalRange(::Unit* target)
{
    if (!GetBot() || !target)
        return OPTIMAL_CASTER_RANGE;

    // Enhancement needs melee range
    if (_currentSpec == ShamanSpec::ENHANCEMENT)
        return OPTIMAL_MELEE_RANGE;

    // Elemental and Restoration maintain caster range
    return OPTIMAL_CASTER_RANGE;
}

void ShamanAI::InitializeSpecialization()
{
    _currentSpec = DetectCurrentSpecialization();
    SwitchSpecialization(_currentSpec);
}

ShamanSpec ShamanAI::DetectCurrentSpecialization()
{
    if (!GetBot())
        return ShamanSpec::ELEMENTAL;

    // Check for key Restoration talents
    if (GetBot()->HasSpell(TALENT_EARTH_SHIELD_TALENT) ||
        GetBot()->HasSpell(TALENT_RIPTIDE_TALENT) ||
        GetBot()->HasSpell(TALENT_HEALING_RAIN_TALENT))
    {
        return ShamanSpec::RESTORATION;
    }

    // Check for key Enhancement talents
    if (GetBot()->HasSpell(TALENT_STORMSTRIKE_TALENT) ||
        GetBot()->HasSpell(TALENT_LAVA_LASH_TALENT) ||
        GetBot()->HasSpell(TALENT_MAELSTROM_WEAPON))
    {
        return ShamanSpec::ENHANCEMENT;
    }

    // Check for key Elemental talents
    if (GetBot()->HasSpell(TALENT_LAVA_BURST_TALENT) ||
        GetBot()->HasSpell(TALENT_ELEMENTAL_MASTERY) ||
        GetBot()->HasSpell(TALENT_LIGHTNING_OVERLOAD))
    {
        return ShamanSpec::ELEMENTAL;
    }

    // Default to Elemental if no clear specialization
    return ShamanSpec::ELEMENTAL;
}

void ShamanAI::SwitchSpecialization(ShamanSpec newSpec)
{
    if (_currentSpec == newSpec && _specialization)
        return;

    _currentSpec = newSpec;
    _specialization.reset();

    switch (newSpec)
    {
        case ShamanSpec::ELEMENTAL:
            _specialization = std::make_unique<ElementalSpecialization>(GetBot());
            TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} switching to Elemental specialization", GetBot()->GetName());
            break;
        case ShamanSpec::ENHANCEMENT:
            _specialization = std::make_unique<EnhancementSpecialization>(GetBot());
            TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} switching to Enhancement specialization", GetBot()->GetName());
            break;
        case ShamanSpec::RESTORATION:
            _specialization = std::make_unique<RestorationSpecialization>(GetBot());
            TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} switching to Restoration specialization", GetBot()->GetName());
            break;
    }
}

void ShamanAI::DelegateToSpecialization(::Unit* target)
{
    if (!_specialization || !target)
        return;

    _specialization->UpdateRotation(target);
}

void ShamanAI::UpdateShamanBuffs()
{
    if (!GetBot())
        return;

    // Lightning Shield for Elemental/Enhancement
    if (_currentSpec != ShamanSpec::RESTORATION)
    {
        if (!HasAura(SPELL_LIGHTNING_SHIELD, GetBot()))
        {
            if (CastSpell(SPELL_LIGHTNING_SHIELD))
            {
                TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} casting Lightning Shield", GetBot()->GetName());
            }
        }
    }
    // Water Shield for Restoration
    else
    {
        if (!HasAura(SPELL_WATER_SHIELD, GetBot()))
        {
            if (CastSpell(SPELL_WATER_SHIELD))
            {
                TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} casting Water Shield", GetBot()->GetName());
            }
        }
    }

    // Earth Shield on tank in group
    if (_currentSpec == ShamanSpec::RESTORATION && GetBot()->HasSpell(SPELL_EARTH_SHIELD))
    {
        if (Group* group = GetBot()->GetGroup())
        {
            Player* tank = FindGroupTank(group);
            if (tank && !HasAura(SPELL_EARTH_SHIELD, tank))
            {
                if (CastSpell(tank, SPELL_EARTH_SHIELD))
                {
                    TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} casting Earth Shield on tank {}",
                                 GetBot()->GetName(), tank->GetName());
                }
            }
        }
    }
}

void ShamanAI::UpdateTotemCheck()
{
    if (!GetBot())
        return;

    static uint32 lastTotemCheck = 0;
    uint32 currentTime = getMSTime();

    if (currentTime - lastTotemCheck < TOTEM_UPDATE_INTERVAL)
        return;

    lastTotemCheck = currentTime;

    // Check if totems need refreshing
    if (_specialization)
    {
        _specialization->UpdateTotemManagement();
    }
}

void ShamanAI::UpdateShockRotation()
{
    if (!GetBot() || !_currentTarget)
        return;

    // Delegate shock rotation to specialization
    if (_specialization)
    {
        _specialization->UpdateShockRotation(_currentTarget);
    }
}

// Private helper methods
void ShamanAI::UpdateWeaponImbues()
{
    if (!GetBot())
        return;

    // Check main hand weapon imbue
    if (!HasWeaponImbue(true))
    {
        uint32 imbueSpell = GetOptimalWeaponImbue(true);
        if (imbueSpell && CastSpell(imbueSpell))
        {
            TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} applying weapon imbue {} to main hand",
                         GetBot()->GetName(), imbueSpell);
        }
    }

    // Check off-hand weapon imbue for Enhancement
    if (_currentSpec == ShamanSpec::ENHANCEMENT && !HasWeaponImbue(false))
    {
        uint32 imbueSpell = GetOptimalWeaponImbue(false);
        if (imbueSpell && CastSpell(imbueSpell))
        {
            TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} applying weapon imbue {} to off-hand",
                         GetBot()->GetName(), imbueSpell);
        }
    }
}

void ShamanAI::UpdateUtilityBuffs()
{
    if (!GetBot())
        return;

    // Water walking when near water
    if (NearWater() && !HasAura(SPELL_WATER_WALKING, GetBot()))
    {
        CastSpell(SPELL_WATER_WALKING);
    }

    // Ghost Wolf for movement speed when traveling
    if (GetBot()->isMoving() && !GetBot()->IsInCombat() && !HasAura(SPELL_GHOST_WOLF, GetBot()))
    {
        // Use Ghost Wolf for long-distance travel
        CastSpell(SPELL_GHOST_WOLF);
    }
}

void ShamanAI::DeployInitialTotems(::Unit* target)
{
    if (!GetBot() || !target || !_specialization)
        return;

    _specialization->DeployOptimalTotems();
}

void ShamanAI::RecallCombatTotems()
{
    // Totems automatically despawn, but we can track their removal
    for (auto& totem : _activeTotems)
    {
        totem.isActive = false;
    }

    TC_LOG_DEBUG("module.playerbot.ai", "Shaman {} combat ended, totems will expire naturally",
                 GetBot()->GetName());
}

void ShamanAI::ApplyCombatBuffs()
{
    if (!GetBot())
        return;

    // Already handled in HandleOffensiveCooldowns
}

void ShamanAI::LogCombatMetrics()
{
    TC_LOG_DEBUG("module.playerbot.ai",
                 "Shaman {} combat metrics - Duration: {}s, Damage: {}, Healing: {}, Mana: {}, Totems: {}, Shocks: {}",
                 GetBot()->GetName(), _combatTime / 1000, _damageDealt, _healingDone,
                 _manaSpent, _totemsDeploy, _shocksUsed);

    // Reset metrics for next combat
    _damageDealt = 0;
    _healingDone = 0;
    _manaSpent = 0;
    _totemsDeploy = 0;
    _shocksUsed = 0;
}

bool ShamanAI::IsShockSpell(uint32 spellId) const
{
    return spellId == SPELL_EARTH_SHOCK ||
           spellId == SPELL_FLAME_SHOCK ||
           spellId == SPELL_FROST_SHOCK;
}

bool ShamanAI::IsTotemSpell(uint32 spellId) const
{
    // Check if spell is a totem spell
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return false;

    // Check spell effects for totem summoning
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (spellInfo->GetEffect(SpellEffIndex(i)).Effect == SPELL_EFFECT_SUMMON)
        {
            // Additional check for totem-specific summons could be added here
            return true;
        }
    }

    // Check against known totem spell IDs
    return (spellId >= SPELL_SEARING_TOTEM && spellId <= SPELL_SENTRY_TOTEM) ||
           spellId == SPELL_FIRE_ELEMENTAL_TOTEM ||
           spellId == SPELL_EARTH_ELEMENTAL_TOTEM ||
           spellId == SPELL_MANA_TIDE_TOTEM ||
           spellId == SPELL_HEALING_TIDE_TOTEM ||
           spellId == SPELL_SPIRIT_LINK_TOTEM ||
           spellId == SPELL_CAPACITOR_TOTEM;
}

uint32 ShamanAI::GetOptimalWeaponImbue(bool mainHand) const
{
    if (!GetBot())
        return 0;

    switch (_currentSpec)
    {
        case ShamanSpec::ELEMENTAL:
            return SPELL_FLAMETONGUE_WEAPON;
        case ShamanSpec::ENHANCEMENT:
            return mainHand ? SPELL_WINDFURY_WEAPON : SPELL_FLAMETONGUE_WEAPON;
        case ShamanSpec::RESTORATION:
            return SPELL_EARTHLIVING_WEAPON;
        default:
            return SPELL_ROCKBITER_WEAPON;
    }
}

bool ShamanAI::HasWeaponImbue(bool mainHand) const
{
    if (!GetBot())
        return false;

    // Check for active weapon enchantment
    Item* weapon = GetBot()->GetItemByPos(INVENTORY_SLOT_BAG_0, mainHand ? EQUIPMENT_SLOT_MAINHAND : EQUIPMENT_SLOT_OFFHAND);
    if (!weapon)
        return false;

    // Check if weapon has temporary enchantment (imbue)
    return weapon->GetEnchantmentId(EnchantmentSlot(TEMP_ENCHANTMENT_SLOT)) != 0;
}

bool ShamanAI::NearWater() const
{
    if (!GetBot())
        return false;

    // Check if bot is near water
    return GetBot()->GetMap()->IsInWater(GetBot()->GetPhaseShift(), GetBot()->GetPositionX(), GetBot()->GetPositionY(), GetBot()->GetPositionZ());
}

bool ShamanAI::ShouldUseBloodlust() const
{
    if (!GetBot())
        return false;

    // Check if already has exhaustion debuff
    if (GetBot()->HasAura(57723) || GetBot()->HasAura(57724)) // Exhaustion/Sated
        return false;

    // Use in boss fights or when health is critical
    if (_currentTarget && _currentTarget->GetHealthPct() < 30.0f && _currentTarget->GetMaxHealth() > 100000)
        return true;

    // Use when multiple group members are low
    if (Group* group = GetBot()->GetGroup())
    {
        uint32 lowHealthCount = 0;
        for (GroupReference const& itr : group->GetMembers())
        {
            if (Player* member = itr.GetSource())
            {
                if (member->GetHealthPct() < 40.0f)
                    lowHealthCount++;
            }
        }
        return lowHealthCount >= 3;
    }

    return false;
}

Player* ShamanAI::FindGroupTank(Group* group) const
{
    if (!group)
        return nullptr;

    Player* tank = nullptr;
    uint32 highestHealth = 0;

    for (GroupReference const& itr : group->GetMembers())
    {
        if (Player* member = itr.GetSource())
        {
            // Simple tank detection - highest health or warrior/paladin/death knight
            if (member->GetClass() == CLASS_WARRIOR ||
                member->GetClass() == CLASS_PALADIN ||
                member->GetClass() == CLASS_DEATH_KNIGHT)
            {
                if (member->GetMaxHealth() > highestHealth)
                {
                    highestHealth = member->GetMaxHealth();
                    tank = member;
                }
            }
        }
    }

    return tank;
}

uint32 ShamanAI::CalculateDamageDealt(::Unit* target) const
{
    // Simplified damage calculation for metrics
    // In a real implementation, this would track actual damage events
    return 100;
}

uint32 ShamanAI::CalculateHealingDone() const
{
    // Simplified healing calculation for metrics
    // In a real implementation, this would track actual healing events
    return _currentSpec == ShamanSpec::RESTORATION ? 200 : 0;
}

uint32 ShamanAI::CalculateManaUsage() const
{
    // Simplified mana usage calculation
    // In a real implementation, this would track actual mana consumption
    return 50;
}

void ShamanAI::RecallTotem(TotemType type)
{
    TotemInfo& totem = _activeTotems[static_cast<size_t>(type)];
    totem.isActive = false;
    totem.spellId = 0;
}

void ShamanAI::UpdateTotemPositions()
{
    // Update totem positions if they moved (shouldn't happen for totems)
    // This is mainly for tracking purposes
}

} // namespace Playerbot