/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "WarlockAI.h"
#include "../../Combat/CombatBehaviorIntegration.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "SpellAuras.h"
#include "Pet.h"
#include "ObjectAccessor.h"
#include "Map.h"
#include "Group.h"
#include "Log.h"
#include "World.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Cell.h"
#include "CellImpl.h"
#include "SpellHistory.h"
#include "CreatureAI.h"
#include "MotionMaster.h"
#include "../../../AI/Combat/BotThreatManager.h"
#include "../../../AI/Combat/TargetSelector.h"
#include "../../../AI/Combat/PositionManager.h"
#include "../../../AI/Combat/InterruptManager.h"
#include "AfflictionSpecialization.h"
#include "DemonologySpecialization.h"
#include "DestructionSpecialization.h"
#include "../BaselineRotationManager.h"

namespace Playerbot
{

// Warlock spell IDs
enum WarlockSpells
{
    // Generic/Utility
    SPELL_LOCK = 19647,         // Felhunter interrupt
    FEAR = 5782,
    BANISH = 710,
    HOWL_OF_TERROR = 5484,
    DEATH_COIL = 6789,
    SHADOWFURY = 30283,

    // Defensive
    UNENDING_RESOLVE = 104773,
    DARK_PACT = 108416,
    SOUL_LEECH = 108370,
    DEMON_ARMOR = 47889,
    FEL_ARMOR = 47893,
    SHADOW_WARD = 47891,
    NETHER_WARD = 91711,
    SOULBURN = 74434,

    // Pet Management
    SUMMON_IMP = 688,
    SUMMON_VOIDWALKER = 697,
    SUMMON_SUCCUBUS = 712,
    SUMMON_FELHUNTER = 691,
    SUMMON_FELGUARD = 30146,
    SUMMON_INFERNAL = 1122,
    SUMMON_DOOMGUARD = 18540,
    HEALTH_FUNNEL = 755,
    CONSUME_SHADOWS = 17767,    // Voidwalker heal
    SOUL_LINK = 19028,
    DEMONIC_EMPOWERMENT = 47193,

    // Offensive Cooldowns
    DARK_SOUL_INSTABILITY = 113858,  // Destruction
    DARK_SOUL_KNOWLEDGE = 113861,    // Affliction
    DARK_SOUL_MISERY = 113860,       // Demonology
    METAMORPHOSIS = 103958,

    // AoE
    SEED_OF_CORRUPTION = 27243,
    RAIN_OF_FIRE = 5740,
    CATACLYSM = 152108,
    FIRE_AND_BRIMSTONE = 108683,
    MANNOROTH_FURY = 108508,

    // Curses
    CURSE_OF_AGONY = 980,
    CURSE_OF_ELEMENTS = 1490,
    CURSE_OF_TONGUES = 1714,
    CURSE_OF_WEAKNESS = 702,
    CURSE_OF_EXHAUSTION = 18223,

    // Affliction
    CORRUPTION = 172,
    UNSTABLE_AFFLICTION = 30108,
    HAUNT = 48181,
    DRAIN_SOUL = 1120,
    SIPHON_LIFE = 63106,
    SOUL_SWAP = 86121,

    // Demonology
    HAND_OF_GULDAN = 105174,
    SHADOWBOLT = 686,
    TOUCH_OF_CHAOS = 103964,
    CHAOS_WAVE = 124916,
    IMMOLATION_AURA = 104025,
    CARRION_SWARM = 103967,
    DEMONIC_LEAP = 104205,
    WRATHSTORM = 89751,

    // Destruction
    IMMOLATE = 348,
    CONFLAGRATE = 17962,
    CHAOS_BOLT = 116858,
    INCINERATE = 29722,
    SHADOWBURN = 17877,
    HAVOC = 80240,
    BACKDRAFT = 117828,

    // Resources
    LIFE_TAP = 1454,
    DARK_INTENT = 109773,
    DRAIN_LIFE = 689,
    DRAIN_MANA = 5138,

    // Utility
    CREATE_SOULSTONE = 20707,
    CREATE_HEALTHSTONE = 6201,
    RITUAL_OF_SUMMONING = 698,
    EYE_OF_KILROGG = 126,
    ENSLAVE_DEMON = 1098,
    UNENDING_BREATH = 5697,
    DETECT_INVISIBILITY = 2970
};

// Constructor with proper member initialization matching the header
WarlockAI::WarlockAI(Player* bot) :
    ClassAI(bot),
    _currentSpec(WarlockSpec::AFFLICTION),
    _specialization(nullptr),
    _warlockMetrics{},
    _threatManager(std::make_unique<BotThreatManager>(bot)),
    _targetSelector(std::make_unique<TargetSelector>(bot, _threatManager.get())),
    _positionManager(std::make_unique<PositionManager>(bot, _threatManager.get())),
    _interruptManager(std::make_unique<InterruptManager>(bot)),
    _currentSoulShards(0),
    _petActive(false),
    _petHealthPercent(0),
    _lastPetCheck(std::chrono::steady_clock::now()),
    _optimalManaThreshold(0.4f),
    _lowManaMode(false),
    _lastLifeTapTime(0),
    _manaSpent(0),
    _damageDealt(0),
    _soulshardsUsed(0),
    _fearsUsed(0),
    _petsSpawned(0),
    _lastFear(0),
    _lastPetSummon(0)
{
    // Initialize specialization based on talent analysis
    InitializeSpecialization();

    // Reset metrics
    _warlockMetrics.Reset();

    // Initialize tracking maps
    _dotTracker.clear();
    _petAbilityCooldowns.clear();

    TC_LOG_DEBUG("playerbot.warlock", "WarlockAI initialized for {} with specialization {}",
                 GetBot()->GetName(), static_cast<uint32>(_currentSpec));
}

// Destructor (required because of forward declarations)
WarlockAI::~WarlockAI() = default;

void WarlockAI::InitializeSpecialization()
{
    // Detect current specialization from talents
    _currentSpec = DetectCurrentSpecialization();

    // Create appropriate specialization handler
    SwitchSpecialization(_currentSpec);
}

WarlockSpec WarlockAI::DetectCurrentSpecialization()
{
    Player* bot = GetBot();
    if (!bot)
        return WarlockSpec::AFFLICTION;

    // Count talent points in each tree
    uint32 afflictionPoints = 0;
    uint32 demonologyPoints = 0;
    uint32 destructionPoints = 0;

    // Check for key Affliction talents
    if (bot->HasSpell(UNSTABLE_AFFLICTION) || bot->HasSpell(HAUNT))
    {
        afflictionPoints += 51;
    }

    // Check for key Demonology talents
    if (bot->HasSpell(METAMORPHOSIS) || bot->HasSpell(DEMONIC_EMPOWERMENT))
    {
        demonologyPoints += 51;
    }

    // Check for key Destruction talents
    if (bot->HasSpell(CHAOS_BOLT) || bot->HasSpell(CONFLAGRATE))
    {
        destructionPoints += 51;
    }

    // Determine specialization based on highest point investment
    if (demonologyPoints > afflictionPoints && demonologyPoints > destructionPoints)
        return WarlockSpec::DEMONOLOGY;
    else if (destructionPoints > afflictionPoints && destructionPoints > demonologyPoints)
        return WarlockSpec::DESTRUCTION;
    else
        return WarlockSpec::AFFLICTION; // Default to Affliction
}

void WarlockAI::SwitchSpecialization(WarlockSpec newSpec)
{
    if (_currentSpec == newSpec && _specialization)
        return;

    _currentSpec = newSpec;

    // Create new specialization handler
    switch (_currentSpec)
    {
        case WarlockSpec::AFFLICTION:
            _specialization = std::make_unique<AfflictionSpecialization>(GetBot());
            TC_LOG_DEBUG("playerbot.warlock", "Switched to Affliction specialization");
            break;
        case WarlockSpec::DEMONOLOGY:
            _specialization = std::make_unique<DemonologySpecialization>(GetBot());
            TC_LOG_DEBUG("playerbot.warlock", "Switched to Demonology specialization");
            break;
        case WarlockSpec::DESTRUCTION:
            _specialization = std::make_unique<DestructionSpecialization>(GetBot());
            TC_LOG_DEBUG("playerbot.warlock", "Switched to Destruction specialization");
            break;
    }
}

void WarlockAI::UpdateRotation(::Unit* target)
{
    // CRITICAL: Use module.playerbot logger to prove function entry
    TC_LOG_ERROR("module.playerbot", "🔥🔥🔥 WARLOCK UpdateRotation() ENTERED! 🔥🔥🔥");

    if (!target)
    {
        TC_LOG_ERROR("module.playerbot", "❌ WarlockAI::UpdateRotation - target is NULL");
        return;
    }

    Player* bot = GetBot();
    if (!bot)
    {
        TC_LOG_ERROR("module.playerbot", "❌ WarlockAI::UpdateRotation - bot is NULL");
        return;
    }

    TC_LOG_ERROR("module.playerbot", "🎯 WarlockAI::UpdateRotation - Bot {} (level {}) attacking {} at {:.1f}yd",
                 bot->GetName(), bot->GetLevel(), target->GetName(), bot->GetDistance(target));

    // Check if bot should use baseline rotation (levels 1-9 or no spec)
    if (BaselineRotationManager::ShouldUseBaselineRotation(bot))
    {
        TC_LOG_ERROR("module.playerbot", "📋 Bot {} using BASELINE rotation (level {})",
                     bot->GetName(), bot->GetLevel());

        static BaselineRotationManager baselineManager;
        baselineManager.HandleAutoSpecialization(bot);

        bool executed = baselineManager.ExecuteBaselineRotation(bot, target);
        TC_LOG_ERROR("module.playerbot", "📋 BaselineRotation result: {}", executed ? "SUCCESS" : "FAILED");

        // No fallback for casters - if rotation failed, just return
        // Do NOT use AttackerStateUpdate (melee) for a caster class
        return;
    }

    // ========================================================================
    // COMBAT BEHAVIOR INTEGRATION - Full 10 Priority System
    // ========================================================================
    auto* behaviors = GetCombatBehaviors();

    // Update combat metrics
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - _warlockMetrics.lastUpdate).count() > COMBAT_METRICS_UPDATE_INTERVAL)
    {
        _warlockMetrics.lastUpdate = now;
        UpdateCombatMetrics();
    }

    // Priority 1: Interrupts - Spell Lock (pet ability)
    if (behaviors && behaviors->ShouldInterrupt(target))
    {
        Unit* interruptTarget = behaviors->GetInterruptTarget();
        if (interruptTarget && HandleInterrupt(interruptTarget))
        {
            TC_LOG_DEBUG("playerbot.warlock", "Warlock {} interrupted {} with Spell Lock",
                         bot->GetName(), interruptTarget->GetName());
            return;
        }
    }

    // Priority 2: Defensives - Unending Resolve, Dark Pact, Soul Leech
    if (behaviors && behaviors->NeedsDefensive())
    {
        if (HandleDefensives())
        {
            TC_LOG_DEBUG("playerbot.warlock", "Warlock {} using defensive abilities", bot->GetName());
            return;
        }
    }

    // Priority 3: Positioning - Maintain max range
    if (behaviors && behaviors->NeedsRepositioning())
    {
        Position optimalPos = behaviors->GetOptimalPosition();
        // Movement is handled by BotAI strategies, but we can cast instant spells while moving
        if (bot->isMoving())
        {
            // Use instant casts while repositioning
            if (HandleInstantCasts(target))
                return;
        }
    }

    // Priority 4: Pet Management - Summon, heal, command
    if (HandlePetManagement())
    {
        TC_LOG_DEBUG("playerbot.warlock", "Warlock {} managing pet", bot->GetName());
        return;
    }

    // Priority 5: Target Switching - Priority targets
    if (behaviors && behaviors->ShouldSwitchTarget())
    {
        Unit* priorityTarget = behaviors->GetPriorityTarget();
        if (priorityTarget && priorityTarget != target)
        {
            // Apply a DoT/Curse to old target before switching
            if (ApplyDoTToTarget(target))
            {
                TC_LOG_DEBUG("playerbot.warlock", "Applied DoT to {} before switching", target->GetName());
            }

            // Switch to priority target
            OnTargetChanged(priorityTarget);
            target = priorityTarget;
            TC_LOG_DEBUG("playerbot.warlock", "Warlock {} switching to priority target {}",
                         bot->GetName(), priorityTarget->GetName());
        }
    }

    // Priority 6: Crowd Control - Fear, Banish for secondary targets
    if (behaviors && behaviors->ShouldUseCrowdControl())
    {
        Unit* ccTarget = behaviors->GetCrowdControlTarget();
        if (ccTarget && ccTarget != target && HandleCrowdControl(ccTarget))
        {
            TC_LOG_DEBUG("playerbot.warlock", "Warlock {} crowd controlling {}",
                         bot->GetName(), ccTarget->GetName());
            return;
        }
    }

    // Priority 7: AoE Decisions - Seed of Corruption, Rain of Fire, Cataclysm
    if (behaviors && behaviors->ShouldAOE())
    {
        if (HandleAoERotation(target))
        {
            TC_LOG_DEBUG("playerbot.warlock", "Warlock {} executing AoE rotation", bot->GetName());
            return;
        }
    }

    // Priority 8: Offensive Cooldowns - Dark Soul, Summon Infernal/Doomguard
    if (behaviors && behaviors->ShouldUseCooldowns())
    {
        if (HandleOffensiveCooldowns(target))
        {
            TC_LOG_DEBUG("playerbot.warlock", "Warlock {} using offensive cooldowns", bot->GetName());
        }
    }

    // Priority 9: Soul Shard Management - Efficient shard generation and spending
    HandleSoulShardManagement();

    // Priority 10: Normal Rotation - Delegate to specialization
    if (_specialization)
    {
        _specialization->UpdateRotation(target);
        _warlockMetrics.spellsCast++;
    }
}

bool WarlockAI::HandleInterrupt(Unit* target)
{
    if (!target || !target->IsNonMeleeSpellCast(false))
        return false;

    Player* bot = GetBot();
    if (!bot)
        return false;

    // Spell Lock - Felhunter ability
    Pet* pet = bot->GetPet();
    if (pet && pet->IsAlive())
    {
        // Check if pet is a Felhunter (entry 417)
        if (pet->GetEntry() == 417 || pet->GetEntry() == 17252) // Felhunter or Fel Guard
        {
            // Command pet to use Spell Lock (19647)
            if (!_petAbilityCooldowns[SPELL_LOCK] || getMSTime() - _petAbilityCooldowns[SPELL_LOCK] > 24000)
            {
                pet->CastSpell(target, SPELL_LOCK, false);
                _petAbilityCooldowns[SPELL_LOCK] = getMSTime();
                return true;
            }
        }
    }

    // Shadowfury - stun interrupt
    if (bot->HasSpell(SHADOWFURY) && !bot->GetSpellHistory()->HasCooldown(SHADOWFURY))
    {
        float distance = bot->GetDistance(target);
        if (distance <= 30.0f)
        {
            bot->CastSpell(target, SHADOWFURY, false);
            return true;
        }
    }

    return false;
}

bool WarlockAI::HandleDefensives()
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    float healthPct = bot->GetHealthPct();

    // Critical health - use strongest defensives
    if (healthPct < 20.0f)
    {
        // Unending Resolve
        if (bot->HasSpell(UNENDING_RESOLVE) && !bot->GetSpellHistory()->HasCooldown(UNENDING_RESOLVE))
        {
            bot->CastSpell(bot, UNENDING_RESOLVE, false);
            return true;
        }

        // Dark Pact
        if (bot->HasSpell(DARK_PACT) && !bot->GetSpellHistory()->HasCooldown(DARK_PACT))
        {
            bot->CastSpell(bot, DARK_PACT, false);
            return true;
        }

        // Healthstone
        if (UseHealthstone())
            return true;
    }

    // Low health - use moderate defensives
    if (healthPct < 40.0f)
    {
        // Shadow Ward/Nether Ward
        if (bot->HasSpell(NETHER_WARD) && !bot->GetSpellHistory()->HasCooldown(NETHER_WARD))
        {
            bot->CastSpell(bot, NETHER_WARD, false);
            return true;
        }
        else if (bot->HasSpell(SHADOW_WARD) && !bot->GetSpellHistory()->HasCooldown(SHADOW_WARD))
        {
            bot->CastSpell(bot, SHADOW_WARD, false);
            return true;
        }

        // Death Coil for heal + fear
        if (bot->HasSpell(DEATH_COIL) && !bot->GetSpellHistory()->HasCooldown(DEATH_COIL))
        {
            Unit* nearestEnemy = GetNearestEnemy(8.0f);
            if (nearestEnemy)
            {
                bot->CastSpell(nearestEnemy, DEATH_COIL, false);
                return true;
            }
        }

        // Howl of Terror for AoE fear
        if (bot->HasSpell(HOWL_OF_TERROR) && !bot->GetSpellHistory()->HasCooldown(HOWL_OF_TERROR))
        {
            if (GetNearbyEnemyCount(10.0f) >= 2)
            {
                bot->CastSpell(bot, HOWL_OF_TERROR, false);
                return true;
            }
        }

        // Drain Life for healing
        Unit* target = bot->GetVictim();
        if (target && bot->HasSpell(DRAIN_LIFE) && !bot->IsNonMeleeSpellCast(false))
        {
            bot->CastSpell(target, DRAIN_LIFE, false);
            return true;
        }
    }

    return false;
}

bool WarlockAI::HandlePetManagement()
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    Pet* pet = bot->GetPet();

    // Check if we need to summon a pet
    if (!pet || !pet->IsAlive())
    {
        return SummonPet();
    }

    // Update pet status
    _petActive = true;
    _petHealthPercent = static_cast<uint32>(pet->GetHealthPct());

    // Heal pet if needed
    if (_petHealthPercent < 50)
    {
        // Health Funnel
        if (bot->HasSpell(HEALTH_FUNNEL) && !bot->GetSpellHistory()->HasCooldown(HEALTH_FUNNEL))
        {
            bot->CastSpell(pet, HEALTH_FUNNEL, false);
            return true;
        }

        // Consume Shadows (Voidwalker self-heal)
        if (pet->GetEntry() == 1860) // Voidwalker
        {
            if (!_petAbilityCooldowns[CONSUME_SHADOWS] || getMSTime() - _petAbilityCooldowns[CONSUME_SHADOWS] > 180000)
            {
                pet->CastSpell(pet, CONSUME_SHADOWS, false);
                _petAbilityCooldowns[CONSUME_SHADOWS] = getMSTime();
            }
        }
    }

    // Demonic Empowerment for Demonology
    if (_currentSpec == WarlockSpec::DEMONOLOGY)
    {
        if (bot->HasSpell(DEMONIC_EMPOWERMENT) && !bot->GetSpellHistory()->HasCooldown(DEMONIC_EMPOWERMENT))
        {
            bot->CastSpell(bot, DEMONIC_EMPOWERMENT, false);
            return true;
        }
    }

    // Command pet to attack if idle
    Unit* target = bot->GetVictim();
    if (target && (!pet->GetVictim() || pet->GetVictim() != target))
    {
        pet->AI()->AttackStart(target);
    }

    return false;
}

bool WarlockAI::SummonPet()
{
    Player* bot = GetBot();
    if (!bot || bot->IsNonMeleeSpellCast(false))
        return false;

    // Don't summon in combat unless we have enough time
    if (bot->IsInCombat() && GetNearbyEnemyCount(10.0f) > 0)
        return false;

    uint32 summonSpell = 0;

    // Choose pet based on spec and situation
    switch (_currentSpec)
    {
        case WarlockSpec::AFFLICTION:
            // Felhunter for interrupt and dispel
            summonSpell = SUMMON_FELHUNTER;
            break;

        case WarlockSpec::DEMONOLOGY:
            // Felguard if available, otherwise Voidwalker
            if (bot->HasSpell(SUMMON_FELGUARD))
                summonSpell = SUMMON_FELGUARD;
            else
                summonSpell = SUMMON_VOIDWALKER;
            break;

        case WarlockSpec::DESTRUCTION:
            // Imp for damage
            summonSpell = SUMMON_IMP;
            break;
    }

    // Fallback to basic pets if specialized ones aren't available
    if (!bot->HasSpell(summonSpell))
    {
        if (bot->HasSpell(SUMMON_IMP))
            summonSpell = SUMMON_IMP;
        else if (bot->HasSpell(SUMMON_VOIDWALKER))
            summonSpell = SUMMON_VOIDWALKER;
    }

    if (summonSpell && !bot->GetSpellHistory()->HasCooldown(summonSpell))
    {
        // Check soul shard requirement
        uint32 soulShards = bot->GetItemCount(6265);
        if (soulShards > 0)
        {
            bot->CastSpell(bot, summonSpell, false);
            _lastPetSummon = getMSTime();
            _petsSpawned++;
            TC_LOG_DEBUG("playerbot.warlock", "Summoning pet with spell {}", summonSpell);
            return true;
        }
    }

    return false;
}

bool WarlockAI::HandleCrowdControl(Unit* target)
{
    if (!target)
        return false;

    Player* bot = GetBot();
    if (!bot)
        return false;

    uint32 now = getMSTime();

    // Fear - primary CC
    if (bot->HasSpell(FEAR) && (now - _lastFear > 5000))
    {
        if (!target->HasAura(FEAR) && bot->GetDistance(target) <= 20.0f)
        {
            bot->CastSpell(target, FEAR, false);
            _lastFear = now;
            _fearsUsed++;
            return true;
        }
    }

    // Banish - for demons/elementals
    if (target->GetCreatureType() == CREATURE_TYPE_DEMON ||
        target->GetCreatureType() == CREATURE_TYPE_ELEMENTAL)
    {
        if (bot->HasSpell(BANISH) && !target->HasAura(BANISH))
        {
            bot->CastSpell(target, BANISH, false);
            return true;
        }
    }

    // Curse of Exhaustion - slow for kiting
    if (bot->HasSpell(CURSE_OF_EXHAUSTION) && !target->HasAura(CURSE_OF_EXHAUSTION))
    {
        bot->CastSpell(target, CURSE_OF_EXHAUSTION, false);
        return true;
    }

    return false;
}

bool WarlockAI::HandleAoERotation(Unit* target)
{
    if (!target)
        return false;

    Player* bot = GetBot();
    if (!bot)
        return false;

    uint32 nearbyEnemies = GetNearbyEnemyCount(15.0f);
    if (nearbyEnemies < 3)
        return false;

    // Seed of Corruption for Affliction
    if (_currentSpec == WarlockSpec::AFFLICTION || nearbyEnemies >= 4)
    {
        if (bot->HasSpell(SEED_OF_CORRUPTION) && !target->HasAura(SEED_OF_CORRUPTION))
        {
            bot->CastSpell(target, SEED_OF_CORRUPTION, false);
            return true;
        }
    }

    // Rain of Fire
    if (bot->HasSpell(RAIN_OF_FIRE) && !bot->GetSpellHistory()->HasCooldown(RAIN_OF_FIRE))
    {
        // Note: Ground-targeted spell, needs special handling
        bot->CastSpell(target, RAIN_OF_FIRE, false);
        return true;
    }

    // Cataclysm (if available)
    if (bot->HasSpell(CATACLYSM) && !bot->GetSpellHistory()->HasCooldown(CATACLYSM))
    {
        bot->CastSpell(target, CATACLYSM, false);
        return true;
    }

    // Fire and Brimstone for Destruction
    if (_currentSpec == WarlockSpec::DESTRUCTION)
    {
        if (bot->HasSpell(FIRE_AND_BRIMSTONE) && !bot->HasAura(FIRE_AND_BRIMSTONE))
        {
            bot->CastSpell(bot, FIRE_AND_BRIMSTONE, false);
            return true;
        }
    }

    // Mannoroth's Fury
    if (bot->HasSpell(MANNOROTH_FURY) && !bot->GetSpellHistory()->HasCooldown(MANNOROTH_FURY))
    {
        bot->CastSpell(bot, MANNOROTH_FURY, false);
        return true;
    }

    return false;
}

bool WarlockAI::HandleOffensiveCooldowns(Unit* target)
{
    if (!target)
        return false;

    Player* bot = GetBot();
    if (!bot)
        return false;

    // Check if target is worth using cooldowns on
    if (target->GetHealthPct() < 20.0f)
        return false;

    bool usedCooldown = false;

    // Dark Soul variants based on spec
    switch (_currentSpec)
    {
        case WarlockSpec::AFFLICTION:
            if (bot->HasSpell(DARK_SOUL_MISERY) && !bot->GetSpellHistory()->HasCooldown(DARK_SOUL_MISERY))
            {
                bot->CastSpell(bot, DARK_SOUL_MISERY, false);
                usedCooldown = true;
            }
            break;

        case WarlockSpec::DEMONOLOGY:
            // Metamorphosis
            if (bot->HasSpell(METAMORPHOSIS) && !bot->GetSpellHistory()->HasCooldown(METAMORPHOSIS))
            {
                bot->CastSpell(bot, METAMORPHOSIS, false);
                usedCooldown = true;
            }
            // Dark Soul: Knowledge
            if (bot->HasSpell(DARK_SOUL_KNOWLEDGE) && !bot->GetSpellHistory()->HasCooldown(DARK_SOUL_KNOWLEDGE))
            {
                bot->CastSpell(bot, DARK_SOUL_KNOWLEDGE, false);
                usedCooldown = true;
            }
            break;

        case WarlockSpec::DESTRUCTION:
            if (bot->HasSpell(DARK_SOUL_INSTABILITY) && !bot->GetSpellHistory()->HasCooldown(DARK_SOUL_INSTABILITY))
            {
                bot->CastSpell(bot, DARK_SOUL_INSTABILITY, false);
                usedCooldown = true;
            }
            break;
    }

    // Summon Infernal/Doomguard
    if (GetNearbyEnemyCount(30.0f) >= 3 || target->GetHealthPct() > 80.0f)
    {
        // Infernal for AoE
        if (GetNearbyEnemyCount(10.0f) >= 3 && bot->HasSpell(SUMMON_INFERNAL))
        {
            if (!bot->GetSpellHistory()->HasCooldown(SUMMON_INFERNAL))
            {
                uint32 soulShards = bot->GetItemCount(6265);
                if (soulShards > 0)
                {
                    bot->CastSpell(target, SUMMON_INFERNAL, false);
                    usedCooldown = true;
                }
            }
        }
        // Doomguard for single target
        else if (bot->HasSpell(SUMMON_DOOMGUARD))
        {
            if (!bot->GetSpellHistory()->HasCooldown(SUMMON_DOOMGUARD))
            {
                uint32 soulShards = bot->GetItemCount(6265);
                if (soulShards > 0)
                {
                    bot->CastSpell(target, SUMMON_DOOMGUARD, false);
                    usedCooldown = true;
                }
            }
        }
    }

    return usedCooldown;
}

void WarlockAI::HandleSoulShardManagement()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Update soul shard count
    _currentSoulShards = bot->GetItemCount(6265);

    // Track soul shard history for optimization
    _soulShardHistory.push(_currentSoulShards);
    if (_soulShardHistory.size() > 10)
        _soulShardHistory.pop();

    // Determine if we should conserve shards
    bool shouldConserve = (_currentSoulShards < 3);

    if (shouldConserve)
    {
        TC_LOG_DEBUG("playerbot.warlock", "Soul shard conservation mode active - {} shards remaining",
                     _currentSoulShards.load());
    }

    // Create healthstone if we don't have one
    if (!HasHealthstone() && _currentSoulShards > 5)
    {
        if (bot->HasSpell(CREATE_HEALTHSTONE) && !bot->GetSpellHistory()->HasCooldown(CREATE_HEALTHSTONE))
        {
            bot->CastSpell(bot, CREATE_HEALTHSTONE, false);
        }
    }

    // Create soulstone if needed
    if (!HasSoulstone() && _currentSoulShards > 3)
    {
        if (bot->HasSpell(CREATE_SOULSTONE) && !bot->GetSpellHistory()->HasCooldown(CREATE_SOULSTONE))
        {
            bot->CastSpell(bot, CREATE_SOULSTONE, false);
        }
    }
}

bool WarlockAI::HandleInstantCasts(Unit* target)
{
    if (!target)
        return false;

    Player* bot = GetBot();
    if (!bot)
        return false;

    // Corruption - instant with talent
    if (bot->HasSpell(CORRUPTION) && !target->HasAura(CORRUPTION))
    {
        bot->CastSpell(target, CORRUPTION, false);
        return true;
    }

    // Curse application
    if (ApplyCurse(target))
        return true;

    // Conflagrate for Destruction
    if (_currentSpec == WarlockSpec::DESTRUCTION)
    {
        if (bot->HasSpell(CONFLAGRATE) && target->HasAura(IMMOLATE))
        {
            if (!bot->GetSpellHistory()->HasCooldown(CONFLAGRATE))
            {
                bot->CastSpell(target, CONFLAGRATE, false);
                return true;
            }
        }
    }

    // Shadowburn for low health targets
    if (bot->HasSpell(SHADOWBURN) && target->GetHealthPct() < 20.0f)
    {
        if (!bot->GetSpellHistory()->HasCooldown(SHADOWBURN))
        {
            bot->CastSpell(target, SHADOWBURN, false);
            return true;
        }
    }

    return false;
}

bool WarlockAI::ApplyDoTToTarget(Unit* target)
{
    if (!target)
        return false;

    Player* bot = GetBot();
    if (!bot)
        return false;

    // Track DoT application time
    ObjectGuid targetGuid = target->GetGUID();
    uint32 now = getMSTime();

    // Corruption - primary DoT
    if (bot->HasSpell(CORRUPTION) && !target->HasAura(CORRUPTION))
    {
        bot->CastSpell(target, CORRUPTION, false);
        _dotTracker[targetGuid][CORRUPTION] = now;
        return true;
    }

    // Spec-specific DoTs
    switch (_currentSpec)
    {
        case WarlockSpec::AFFLICTION:
            // Unstable Affliction
            if (bot->HasSpell(UNSTABLE_AFFLICTION) && !target->HasAura(UNSTABLE_AFFLICTION))
            {
                bot->CastSpell(target, UNSTABLE_AFFLICTION, false);
                _dotTracker[targetGuid][UNSTABLE_AFFLICTION] = now;
                return true;
            }
            // Haunt
            if (bot->HasSpell(HAUNT) && !bot->GetSpellHistory()->HasCooldown(HAUNT))
            {
                bot->CastSpell(target, HAUNT, false);
                _dotTracker[targetGuid][HAUNT] = now;
                return true;
            }
            break;

        case WarlockSpec::DESTRUCTION:
            // Immolate
            if (bot->HasSpell(IMMOLATE) && !target->HasAura(IMMOLATE))
            {
                bot->CastSpell(target, IMMOLATE, false);
                _dotTracker[targetGuid][IMMOLATE] = now;
                return true;
            }
            break;

        case WarlockSpec::DEMONOLOGY:
            // Corruption is usually enough, Hand of Gul'dan for AoE
            if (bot->HasSpell(HAND_OF_GULDAN) && !bot->GetSpellHistory()->HasCooldown(HAND_OF_GULDAN))
            {
                bot->CastSpell(target, HAND_OF_GULDAN, false);
                return true;
            }
            break;
    }

    return false;
}

bool WarlockAI::ApplyCurse(Unit* target)
{
    if (!target)
        return false;

    Player* bot = GetBot();
    if (!bot)
        return false;

    // Check if target already has a curse
    if (target->HasAura(CURSE_OF_AGONY) || target->HasAura(CURSE_OF_ELEMENTS) ||
        target->HasAura(CURSE_OF_TONGUES) || target->HasAura(CURSE_OF_WEAKNESS))
        return false;

    uint32 curseSpell = 0;

    // Choose appropriate curse based on target and situation
    if (target->GetPowerType() == POWER_MANA)
    {
        // Curse of Tongues for casters
        curseSpell = CURSE_OF_TONGUES;
    }
    else if (_currentSpec == WarlockSpec::AFFLICTION)
    {
        // Curse of Agony for Affliction
        curseSpell = CURSE_OF_AGONY;
    }
    else
    {
        // Curse of the Elements for damage increase
        curseSpell = CURSE_OF_ELEMENTS;
    }

    // Apply curse if available
    if (curseSpell && bot->HasSpell(curseSpell))
    {
        bot->CastSpell(target, curseSpell, false);
        return true;
    }

    return false;
}

Unit* WarlockAI::GetNearestEnemy(float range)
{
    Player* bot = GetBot();
    if (!bot)
        return nullptr;

    std::list<Unit*> enemies;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(bot, bot, range);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, enemies, check);
    Cell::VisitAllObjects(bot, searcher, range);

    Unit* nearest = nullptr;
    float minDist = range;

    for (Unit* enemy : enemies)
    {
        if (!enemy->IsAlive() || !bot->CanSeeOrDetect(enemy))
            continue;

        float dist = bot->GetDistance(enemy);
        if (dist < minDist)
        {
            minDist = dist;
            nearest = enemy;
        }
    }

    return nearest;
}

uint32 WarlockAI::GetNearbyEnemyCount(float range)
{
    Player* bot = GetBot();
    if (!bot)
        return 0;

    std::list<Unit*> enemies;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(bot, bot, range);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, enemies, check);
    Cell::VisitAllObjects(bot, searcher, range);

    uint32 count = 0;
    for (Unit* enemy : enemies)
    {
        if (enemy->IsAlive() && bot->CanSeeOrDetect(enemy))
            count++;
    }

    return count;
}

bool WarlockAI::HasHealthstone()
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    // Check for various healthstone item IDs
    uint32 healthstones[] = { 5512, 5511, 5509, 5510, 9421, 19013 };
    for (uint32 itemId : healthstones)
    {
        if (bot->GetItemCount(itemId) > 0)
            return true;
    }

    return false;
}

bool WarlockAI::UseHealthstone()
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    // Try to use healthstone
    uint32 healthstones[] = { 5512, 5511, 5509, 5510, 9421, 19013 };
    for (uint32 itemId : healthstones)
    {
        if (Item* item = bot->GetItemByEntry(itemId))
        {
            // Use modern TrinityCore CastItemUseSpell API
            SpellCastTargets targets;
            targets.SetUnitTarget(bot); // Use healthstone on self
            bot->CastItemUseSpell(item, targets, ObjectGuid::Empty, nullptr);

            TC_LOG_DEBUG("playerbot.warlock", "Warlock {} used healthstone {}", bot->GetName(), itemId);
            return true;
        }
    }

    return false;
}

bool WarlockAI::HasSoulstone()
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    // Check for soulstone buff or item
    uint32 soulstones[] = { 5232, 16892, 16893, 16895, 16896 };
    for (uint32 itemId : soulstones)
    {
        if (bot->GetItemCount(itemId) > 0)
            return true;
    }

    // Check if we have soulstone buff on us
    return bot->HasAura(20707); // Soulstone Resurrection
}

void WarlockAI::UpdateCombatMetrics()
{
    // Calculate efficiency metrics
    if (_warlockMetrics.manaSpent > 0)
    {
        _warlockMetrics.manaEfficiency = static_cast<float>(_warlockMetrics.damageDealt) / _warlockMetrics.manaSpent;
    }

    // Update pet uptime
    if (_petActive)
    {
        auto now = std::chrono::steady_clock::now();
        auto combatDuration = std::chrono::duration_cast<std::chrono::seconds>(now - _warlockMetrics.combatStartTime).count();
        if (combatDuration > 0)
        {
            _warlockMetrics.petUptime = 100.0f; // Pet is currently active
        }
    }

    // Update DoT uptime
    Player* bot = GetBot();
    if (bot && bot->GetVictim())
    {
        Unit* target = bot->GetVictim();
        int dotCount = 0;
        int totalDots = 3; // Typical number of DoTs

        if (target->HasAura(CORRUPTION)) dotCount++;
        if (target->HasAura(CURSE_OF_AGONY)) dotCount++;
        if (target->HasAura(UNSTABLE_AFFLICTION)) dotCount++;
        if (target->HasAura(IMMOLATE)) dotCount++;

        _warlockMetrics.dotUptime = (static_cast<float>(dotCount) / totalDots) * 100.0f;
    }
}

// Required virtual function implementations
void WarlockAI::UpdateBuffs()
{
    // Use baseline buffs for low-level bots
    if (BaselineRotationManager::ShouldUseBaselineRotation(GetBot()))
    {
        static BaselineRotationManager baselineManager;
        baselineManager.ApplyBaselineBuffs(GetBot());
        return;
    }

    // Update warlock-specific buffs
    UpdateWarlockBuffs();

    // Delegate to specialization for spec-specific buffs
    if (_specialization)
    {
        _specialization->UpdateBuffs();
    }
}

void WarlockAI::UpdateCooldowns(uint32 diff)
{
    // Manage warlock-specific cooldowns
    ManageWarlockCooldowns();

    // Delegate to specialization
    if (_specialization)
    {
        _specialization->UpdateCooldowns(diff);
    }
}

bool WarlockAI::CanUseAbility(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    // Check if spell is known and ready
    if (!bot->HasSpell(spellId))
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, bot->GetMap()->GetDifficultyID());
    if (!spellInfo)
        return false;

    // Check if we have enough resources
    if (!HasEnoughResource(spellId))
        return false;

    // Check cooldown
    if (bot->GetSpellHistory()->HasCooldown(spellId))
        return false;

    // Check if we're casting or channeling
    if (bot->IsNonMeleeSpellCast(false, true, true))
        return false;

    // Check soul shard requirements
    for (uint32 i = 0; i < MAX_SPELL_REAGENTS; ++i)
    {
        if (spellInfo->Reagent[i] == 6265) // Soul Shard item ID
        {
            if (bot->GetItemCount(6265) < spellInfo->ReagentCount[i])
                return false;
        }
    }

    // Delegate to specialization for additional checks
    if (_specialization)
    {
        return _specialization->CanUseAbility(spellId);
    }

    return true;
}

void WarlockAI::OnCombatStart(::Unit* target)
{
    ClassAI::OnCombatStart(target);

    // Reset combat metrics
    _warlockMetrics.Reset();

    // Update pet status
    UpdatePetCheck();

    // Clear DoT tracker for new combat
    _dotTracker.clear();

    // Initialize threat management
    if (_threatManager)
    {
        // BotThreatManager handles combat start automatically
    }

    // Delegate to specialization
    if (_specialization)
    {
        _specialization->OnCombatStart(target);
    }

    TC_LOG_DEBUG("playerbot.warlock", "Warlock {} entering combat - Spec: {}, Soul Shards: {}",
                 GetBot()->GetName(), static_cast<uint32>(_currentSpec), _currentSoulShards.load());
}

void WarlockAI::OnCombatEnd()
{
    ClassAI::OnCombatEnd();

    // Log combat metrics
    auto combatDuration = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - _warlockMetrics.combatStartTime).count();

    if (combatDuration > 0)
    {
        TC_LOG_DEBUG("playerbot.warlock", "Combat summary for {}: Duration: {}s, Damage: {}, DoT: {}, Pet: {}, Efficiency: {:.2f}",
                     GetBot()->GetName(), combatDuration,
                     _warlockMetrics.damageDealt.load(),
                     _warlockMetrics.dotDamage.load(),
                     _warlockMetrics.petDamage.load(),
                     _warlockMetrics.manaEfficiency.load());
    }

    // Life Tap if needed
    ManageLifeTapTiming();

    // Delegate to specialization
    if (_specialization)
    {
        _specialization->OnCombatEnd();
    }
}

bool WarlockAI::HasEnoughResource(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, bot->GetMap()->GetDifficultyID());
    if (!spellInfo)
        return false;

    // Check mana
    auto powerCosts = spellInfo->CalcPowerCost(bot, spellInfo->GetSchoolMask());
    uint32 manaCost = 0;
    for (auto const& cost : powerCosts)
        if (cost.Power == POWER_MANA)
            manaCost = cost.Amount;
    if (bot->GetPower(POWER_MANA) < int32(manaCost))
    {
        _lowManaMode = true;
        return false;
    }

    // Check if we're in low mana mode
    if (bot->GetPowerPct(POWER_MANA) < LOW_MANA_THRESHOLD * 100)
    {
        _lowManaMode = true;
    }
    else if (bot->GetPowerPct(POWER_MANA) > 50)
    {
        _lowManaMode = false;
    }

    return true;
}

void WarlockAI::ConsumeResource(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, bot->GetMap()->GetDifficultyID());
    if (!spellInfo)
        return;

    // Track mana spent
    auto powerCosts = spellInfo->CalcPowerCost(bot, spellInfo->GetSchoolMask());
    uint32 manaCost = 0;
    for (auto const& cost : powerCosts)
        if (cost.Power == POWER_MANA)
            manaCost = cost.Amount;
    _warlockMetrics.manaSpent += manaCost;

    // Track soul shard usage
    for (uint32 i = 0; i < MAX_SPELL_REAGENTS; ++i)
    {
        if (spellInfo->Reagent[i] == 6265) // Soul Shard
        {
            _warlockMetrics.soulShardsUsed++;
            _currentSoulShards = bot->GetItemCount(6265);
            break;
        }
    }
}

Position WarlockAI::GetOptimalPosition(::Unit* target)
{
    if (!target || !_positionManager)
        return GetBot()->GetPosition();

    // Use position manager for optimal positioning
    return _positionManager->FindRangedPosition(target, GetOptimalRange(target));
}

float WarlockAI::GetOptimalRange(::Unit* target)
{
    if (!target)
        return 25.0f;

    // Warlocks prefer to stay at max casting range
    // Shadow Bolt/Corruption have 30yd max range, stay at 25yd for safety
    return 25.0f; // Safe distance within spell range, allows for movement
}

// Warlock-specific utility implementations
void WarlockAI::UpdateWarlockBuffs()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Demon Armor/Fel Armor
    if (!bot->HasAura(DEMON_ARMOR) && !bot->HasAura(FEL_ARMOR))
    {
        if (bot->HasSpell(FEL_ARMOR))
            bot->CastSpell(bot, FEL_ARMOR, false);
        else if (bot->HasSpell(DEMON_ARMOR))
            bot->CastSpell(bot, DEMON_ARMOR, false);
    }

    // Soul Link for Demonology
    if (_currentSpec == WarlockSpec::DEMONOLOGY)
    {
        if (bot->HasSpell(SOUL_LINK) && !bot->HasAura(SOUL_LINK) && _petActive)
        {
            bot->CastSpell(bot, SOUL_LINK, false);
        }
    }

    // Dark Intent buff
    if (bot->HasSpell(DARK_INTENT) && !bot->HasAura(DARK_INTENT))
    {
        bot->CastSpell(bot, DARK_INTENT, false);
    }
}

void WarlockAI::UpdatePetCheck()
{
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastPetCheck).count() < PET_CHECK_INTERVAL)
        return;

    _lastPetCheck = now;

    Player* bot = GetBot();
    if (!bot)
        return;

    Pet* pet = bot->GetPet();
    _petActive = (pet && pet->IsAlive());

    if (_petActive && pet)
    {
        _petHealthPercent = static_cast<uint32>(pet->GetHealthPct());
    }
    else
    {
        _petHealthPercent = 0;
    }
}

void WarlockAI::UpdateSoulShardCheck()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    _currentSoulShards = bot->GetItemCount(6265);

    // Track soul shard history for optimization
    _soulShardHistory.push(_currentSoulShards);
    if (_soulShardHistory.size() > 10)
        _soulShardHistory.pop();
}

void WarlockAI::OptimizeManaManagement()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    float manaPct = bot->GetPowerPct(POWER_MANA);

    // Adjust mana threshold based on combat situation
    if (bot->IsInCombat())
    {
        _optimalManaThreshold = 0.3f; // Lower threshold in combat
    }
    else
    {
        _optimalManaThreshold = 0.5f; // Higher threshold out of combat
    }

    // Update low mana mode
    _lowManaMode = (manaPct < _optimalManaThreshold * 100);
}

void WarlockAI::ManageLifeTapTiming()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    uint32 now = getMSTime();

    // Don't Life Tap too frequently
    if (now - _lastLifeTapTime < 3000)
        return;

    float healthPct = bot->GetHealthPct();
    float manaPct = bot->GetPowerPct(POWER_MANA);

    // Only Life Tap if health is good and mana is low
    if (healthPct > LIFE_TAP_THRESHOLD * 100 && manaPct < _optimalManaThreshold * 100)
    {
        if (bot->HasSpell(LIFE_TAP) && !bot->GetSpellHistory()->HasCooldown(LIFE_TAP))
        {
            bot->CastSpell(bot, LIFE_TAP, false);
            _lastLifeTapTime = now;
            _warlockMetrics.lifeTapsCast++;
            TC_LOG_DEBUG("playerbot.warlock", "Life Tap cast - Health: {:.1f}%, Mana: {:.1f}%", healthPct, manaPct);
        }
    }
}

void WarlockAI::OptimizePetPositioning()
{
    if (!_petActive || !_positionManager)
        return;

    Player* bot = GetBot();
    Pet* pet = bot ? bot->GetPet() : nullptr;
    if (!pet)
        return;

    Unit* target = bot->GetVictim();
    if (!target)
        return;

    // Position pet based on its type and combat role
    Position optimalPos;
    float distance = 0.0f;

    // Determine optimal position based on pet type
    if (_currentSpec == WarlockSpec::DEMONOLOGY && bot->HasSpell(SUMMON_FELGUARD))
    {
        // Melee pet - position in front of target
        distance = 3.0f;
        optimalPos = target->GetNearPosition(distance, 0);
    }
    else if (_currentSpec == WarlockSpec::AFFLICTION) // Felhunter
    {
        // Anti-caster pet - position near casters
        distance = 5.0f;
        optimalPos = target->GetNearPosition(distance, M_PI / 4);
    }
    else // Imp or Voidwalker
    {
        // Ranged or tank pet - position at medium range
        distance = 15.0f;
        optimalPos = target->GetNearPosition(distance, M_PI / 2);
    }

    // Command pet to move if needed
    if (pet->GetDistance(optimalPos) > 5.0f)
    {
        pet->GetMotionMaster()->MovePoint(0, optimalPos);
    }
}

void WarlockAI::HandlePetSpecialAbilities()
{
    if (!_petActive)
        return;

    Player* bot = GetBot();
    Pet* pet = bot ? bot->GetPet() : nullptr;
    if (!pet)
        return;

    Unit* target = bot->GetVictim();
    if (!target)
        return;

    // Use pet abilities based on type and situation
    uint32 petEntry = pet->GetEntry();
    uint32 now = getMSTime();

    // Imp abilities
    if (petEntry == 416) // Imp
    {
        // Firebolt is auto-cast, no need to manage
    }
    // Voidwalker abilities
    else if (petEntry == 1860) // Voidwalker
    {
        // Torment for threat
        if (!_petAbilityCooldowns[17735] || now - _petAbilityCooldowns[17735] > 5000)
        {
            pet->CastSpell(target, 17735, false); // Torment
            _petAbilityCooldowns[17735] = now;
        }
    }
    // Succubus abilities
    else if (petEntry == 1863) // Succubus
    {
        // Lash of Pain
        if (!_petAbilityCooldowns[7814] || now - _petAbilityCooldowns[7814] > 6000)
        {
            pet->CastSpell(target, 7814, false); // Lash of Pain
            _petAbilityCooldowns[7814] = now;
        }
    }
    // Felhunter abilities
    else if (petEntry == 417) // Felhunter
    {
        // Devour Magic for dispel
        if (target->HasUnitState(UNIT_STATE_CASTING))
        {
            if (!_petAbilityCooldowns[19505] || now - _petAbilityCooldowns[19505] > 8000)
            {
                pet->CastSpell(target, 19505, false); // Devour Magic
                _petAbilityCooldowns[19505] = now;
            }
        }
    }
    // Felguard abilities
    else if (petEntry == 17252) // Felguard
    {
        // Cleave
        if (!_petAbilityCooldowns[30213] || now - _petAbilityCooldowns[30213] > 6000)
        {
            pet->CastSpell(target, 30213, false); // Cleave
            _petAbilityCooldowns[30213] = now;
        }
    }
}

void WarlockAI::ManageWarlockCooldowns()
{
    // Manage major warlock cooldowns
    Player* bot = GetBot();
    if (!bot || !bot->IsInCombat())
        return;

    // Demonic Empowerment for Demonology
    if (_currentSpec == WarlockSpec::DEMONOLOGY && _petActive)
    {
        if (bot->HasSpell(DEMONIC_EMPOWERMENT) && !bot->GetSpellHistory()->HasCooldown(DEMONIC_EMPOWERMENT))
        {
            bot->CastSpell(bot, DEMONIC_EMPOWERMENT, false);
        }
    }

    // Metamorphosis for Demonology
    if (_currentSpec == WarlockSpec::DEMONOLOGY)
    {
        if (bot->HasSpell(METAMORPHOSIS) && !bot->GetSpellHistory()->HasCooldown(METAMORPHOSIS))
        {
            // Use in high-pressure situations
            if (bot->GetVictim() && bot->GetVictim()->GetHealthPct() > 50)
            {
                bot->CastSpell(bot, METAMORPHOSIS, false);
            }
        }
    }
}

void WarlockAI::OptimizeSoulShardUsage()
{
    // Optimize soul shard usage based on availability and need
    std::lock_guard<std::mutex> lock(_soulShardMutex);

    // Determine conservation mode based on shard count
    bool shouldConserve = (_currentSoulShards < 5);

    if (shouldConserve)
    {
        TC_LOG_DEBUG("playerbot.warlock", "Soul shard conservation mode active - {} shards remaining",
                     _currentSoulShards.load());
    }
}

void WarlockAI::HandleAoESituations()
{
    Player* bot = GetBot();
    if (!bot || !bot->IsInCombat())
        return;

    uint32 nearbyEnemies = GetNearbyEnemyCount(30.0f);

    if (nearbyEnemies >= 3)
    {
        // Seed of Corruption for Affliction
        if (_currentSpec == WarlockSpec::AFFLICTION && bot->HasSpell(SEED_OF_CORRUPTION))
        {
            Unit* target = bot->GetVictim();
            if (target && !target->HasAura(SEED_OF_CORRUPTION) && !bot->GetSpellHistory()->HasCooldown(SEED_OF_CORRUPTION))
            {
                bot->CastSpell(target, SEED_OF_CORRUPTION, false);
            }
        }

        // Rain of Fire for all specs
        if (bot->HasSpell(RAIN_OF_FIRE) && !bot->GetSpellHistory()->HasCooldown(RAIN_OF_FIRE))
        {
            Unit* target = bot->GetVictim();
            if (target)
            {
                bot->CastSpell(target, RAIN_OF_FIRE, false);
            }
        }
    }
}

void WarlockAI::ManageCurseApplication()
{
    Player* bot = GetBot();
    if (!bot || !bot->IsInCombat())
        return;

    Unit* target = bot->GetVictim();
    if (!target)
        return;

    ApplyCurse(target);
}

void WarlockAI::OptimizeDoTRotation()
{
    if (_currentSpec != WarlockSpec::AFFLICTION)
        return;

    Player* bot = GetBot();
    if (!bot || !bot->IsInCombat())
        return;

    Unit* target = bot->GetVictim();
    if (!target)
        return;

    ApplyDoTToTarget(target);
}

// Helper method implementations
bool WarlockAI::HasEnoughMana(uint32 amount)
{
    Player* bot = GetBot();
    return bot && bot->GetPower(POWER_MANA) >= int32(amount);
}

uint32 WarlockAI::GetMana()
{
    Player* bot = GetBot();
    return bot ? bot->GetPower(POWER_MANA) : 0;
}

uint32 WarlockAI::GetMaxMana()
{
    Player* bot = GetBot();
    return bot ? bot->GetMaxPower(POWER_MANA) : 0;
}

float WarlockAI::GetManaPercent()
{
    Player* bot = GetBot();
    return bot ? bot->GetPowerPct(POWER_MANA) : 0.0f;
}

void WarlockAI::UseDefensiveAbilities()
{
    HandleDefensives();
}

void WarlockAI::UseCrowdControl(::Unit* target)
{
    HandleCrowdControl(target);
}

void WarlockAI::UpdatePetManagement()
{
    HandlePetManagement();
}

WarlockSpec WarlockAI::GetCurrentSpecialization() const
{
    return _currentSpec;
}

bool WarlockAI::ShouldConserveMana()
{
    return _lowManaMode;
}

} // namespace Playerbot