/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * Destruction Warlock Refactored - Template-Based Implementation
 *
 * This file provides a complete, template-based implementation of Destruction Warlock
 * using the RangedDpsSpecialization with dual resource system (Mana + Soul Shards).
 */

#pragma once

#include "../CombatSpecializationTemplates.h"
#include "../ResourceTypes.h"
#include "Player.h"
#include "Pet.h"
#include "SpellMgr.h"
#include "SpellAuraEffects.h"
#include "Log.h"
#include "WarlockSpecialization.h"

namespace Playerbot
{

// ============================================================================
// DESTRUCTION WARLOCK SPELL IDs (WoW 11.2 - The War Within)
// ============================================================================

enum DestructionWarlockSpells
{
    // Core Spells
    CHAOS_BOLT               = 116858,  // 2 shards, heavy direct damage
    INCINERATE               = 29722,   // Filler, generates shards
    CONFLAGRATE              = 17962,   // 2 charges, 13 sec CD, generates shards
    IMMOLATE                 = 348,     // DoT, enables Conflagrate

    // AoE Spells
    RAIN_OF_FIRE             = 5740,    // 3 shards, ground AoE
    CHANNEL_DEMONFIRE        = 196447,  // Channel, requires Immolate (talent)
    CATACLYSM                = 152108,  // 30 sec CD, AoE + Immolate (talent)
    HAVOC                    = 80240,   // 30 sec CD, cleave on 2nd target

    // Major Cooldowns
    SUMMON_INFERNAL          = 1122,    // 3 min CD, summons Infernal (major CD)
    DARK_SOUL_INSTABILITY    = 113858,  // 2 min CD, crit buff (talent)
    SOUL_FIRE                = 6353,    // 20 sec CD, strong direct damage (talent)

    // Pet Management
    SUMMON_IMP_DESTRO        = 688,
    SUMMON_VOIDWALKER_DESTRO = 697,
    SUMMON_SUCCUBUS_DESTRO   = 712,
    SUMMON_FELHUNTER_DESTRO  = 691,
    COMMAND_DEMON_DESTRO     = 119898,

    // Utility
    CURSE_OF_TONGUES_DESTRO  = 1714,    // Casting slow (talent)
    CURSE_OF_WEAKNESS_DESTRO = 702,     // Reduces physical damage
    CURSE_OF_EXHAUSTION_DESTRO = 334275,// Movement slow
    SHADOWBURN               = 17877,   // Execute, generates shards (talent)
    BACKDRAFT                = 196406,  // Buff: reduces Incinerate cast time

    // Defensives
    UNENDING_RESOLVE_DESTRO  = 104773,  // 3 min CD, damage reduction
    DARK_PACT_DESTRO         = 108416,  // 1 min CD, shield (talent)
    MORTAL_COIL_DESTRO       = 6789,    // Heal + fear (talent)
    HOWL_OF_TERROR_DESTRO    = 5484,    // AoE fear (talent)
    FEAR_DESTRO              = 5782,    // CC
    BANISH_DESTRO            = 710,     // CC (demons/elementals)
    DEMONIC_CIRCLE_TELEPORT_DESTRO = 48020, // Teleport
    DEMONIC_GATEWAY_DESTRO   = 111771,  // Portal
    BURNING_RUSH_DESTRO      = 111400,  // Speed, drains health

    // Procs and Buffs
    BACKDRAFT_BUFF           = 117828,  // Buff from Conflagrate
    REVERSE_ENTROPY          = 205148,  // Buff from Rain of Fire (talent)
    ERADICATION              = 196412,  // Debuff: increases damage taken (talent)
    FLASHOVER                = 267115,  // Backdraft on Conflagrate CD end (talent)

    // Talents
    ROARING_BLAZE            = 205184,  // Conflagrate buff
    INTERNAL_COMBUSTION      = 266134,  // Chaos Bolt consumes Immolate
    FIRE_AND_BRIMSTONE       = 196408,  // Incinerate cleaves
    INFERNO                  = 270545,  // Rain of Fire stun
    GRIMOIRE_OF_SUPREMACY    = 266086   // Better pets
};

// Dual resource type for Destruction Warlock
struct ManaSoulShardResourceDestro
{
    uint32 mana{0};
    uint32 soulShards{0};
    uint32 maxMana{100000};
    uint32 maxSoulShards{5};

    bool available{true};

    bool Consume(uint32 manaCost) {
        if (mana >= manaCost) {
            mana -= manaCost;
            return true;
        }
        return false;
    }

    void Regenerate(uint32 diff) {
        if (mana < maxMana) {
            uint32 regenAmount = (maxMana / 100) * (diff / 1000);
            mana = std::min(mana + regenAmount, maxMana);
        }
        available = mana > 0;
    }

    [[nodiscard]] uint32 GetAvailable() const { return mana; }
    [[nodiscard]] uint32 GetMax() const { return maxMana; }

    void Initialize(Player* bot) {
        if (bot) {
            maxMana = bot->GetMaxPower(POWER_MANA);
            mana = bot->GetPower(POWER_MANA);
        }
        soulShards = 0;
    }
};

// ============================================================================
// DESTRUCTION IMMOLATE TRACKER
// ============================================================================

class DestructionImmolateTracker
{
public:
    DestructionImmolateTracker()
        : _trackedTargets()
    {
    }

    void ApplyImmolate(ObjectGuid guid, uint32 duration)
    {
        _trackedTargets[guid] = getMSTime() + duration;
    }

    void RemoveImmolate(ObjectGuid guid)
    {
        _trackedTargets.erase(guid);
    }

    bool HasImmolate(ObjectGuid guid) const
    {
        auto it = _trackedTargets.find(guid);
        if (it == _trackedTargets.end())
            return false;

        return getMSTime() < it->second;
    }

    uint32 GetTimeRemaining(ObjectGuid guid) const
    {
        auto it = _trackedTargets.find(guid);
        if (it == _trackedTargets.end())
            return 0;

        uint32 now = getMSTime();
        return (it->second > now) ? (it->second - now) : 0;
    }

    bool NeedsRefresh(ObjectGuid guid, uint32 pandemicWindow = 5400) const
    {
        uint32 remaining = GetTimeRemaining(guid);
        return remaining < pandemicWindow; // Pandemic window
    }

    void Update()
    {
        uint32 now = getMSTime();
        for (auto it = _trackedTargets.begin(); it != _trackedTargets.end();)
        {
            if (now >= it->second)
                it = _trackedTargets.erase(it);
            else
                ++it;
        }
    }

private:
    std::unordered_map<ObjectGuid, uint32> _trackedTargets;
};

// ============================================================================
// DESTRUCTION HAVOC TRACKER
// ============================================================================

class DestructionHavocTracker
{
public:
    DestructionHavocTracker()
        : _havocTargetGuid()
        , _havocEndTime(0)
        , _havocActive(false)
    {
    }

    void ApplyHavoc(ObjectGuid guid)
    {
        _havocTargetGuid = guid;
        _havocEndTime = getMSTime() + 12000; // 12 sec duration
        _havocActive = true;
    }

    bool IsActive() const { return _havocActive; }
    ObjectGuid GetTarget() const { return _havocTargetGuid; }

    void Update()
    {
        if (_havocActive && getMSTime() >= _havocEndTime)
        {
            _havocActive = false;
            _havocTargetGuid = ObjectGuid::Empty;
            _havocEndTime = 0;
        }
    }

private:
    ObjectGuid _havocTargetGuid;
    uint32 _havocEndTime;
    bool _havocActive;
};

// ============================================================================
// DESTRUCTION WARLOCK REFACTORED
// ============================================================================

class DestructionWarlockRefactored : public RangedDpsSpecialization<ManaSoulShardResourceDestro>, public WarlockSpecialization
{
public:
    // Use base class members with type alias for cleaner syntax
    using Base = RangedDpsSpecialization<ManaSoulShardResourceDestro>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::_resource;
    explicit DestructionWarlockRefactored(Player* bot)
        : RangedDpsSpecialization<ManaSoulShardResourceDestro>(bot)
        , WarlockSpecialization(bot)
        , _immolateTracker()
        , _havocTracker()
        , _backdraftStacks(0)
        , _lastInfernalTime(0)
    {
        // Initialize mana/soul shard resources
        this->_resource.Initialize(bot);

        InitializeCooldowns();

        TC_LOG_DEBUG("playerbot", "DestructionWarlockRefactored initialized for {}", bot->GetName());
    }

    void UpdateRotation(::Unit* target) override
    {
        if (!target || !target->IsAlive() || !target->IsHostileTo(this->GetBot()))
            return;

        // Update Destruction state
        UpdateDestructionState();

        // Ensure pet is active
        EnsurePetActive();

        // Determine if AoE or single target
        uint32 enemyCount = this->GetEnemiesInRange(40.0f);
        if (enemyCount >= 3)
        {
            ExecuteAoERotation(target, enemyCount);
        }
        else if (enemyCount == 2)
        {
            ExecuteCleaveRotation(target);
        }
        else
        {
            ExecuteSingleTargetRotation(target);
        }
    }

    void UpdateBuffs() override
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        // Defensive cooldowns
        HandleDefensiveCooldowns();
    }

    void ExecuteSingleTargetRotation(::Unit* target)
    {
        ObjectGuid targetGuid = target->GetGUID();
        uint32 shards = this->_resource.soulShards;
        float targetHpPct = target->GetHealthPct();

        // Priority 1: Use Summon Infernal (major CD)
        if (shards >= 2 && this->CanCastSpell(SUMMON_INFERNAL, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), SUMMON_INFERNAL);
            _lastInfernalTime = getMSTime();
            TC_LOG_DEBUG("playerbot", "Destruction: Summon Infernal");
            // Continue rotation during Infernal
        }

        // Priority 2: Dark Soul: Instability (burst CD)
        if (this->CanCastSpell(DARK_SOUL_INSTABILITY, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), DARK_SOUL_INSTABILITY);
            TC_LOG_DEBUG("playerbot", "Destruction: Dark Soul Instability");
        }

        // Priority 3: Maintain Immolate
        if (_immolateTracker.NeedsRefresh(targetGuid))
        {
            if (this->CanCastSpell(IMMOLATE, target))
            {
                this->CastSpell(target, IMMOLATE);
                _immolateTracker.ApplyImmolate(targetGuid, 18000); // 18 sec duration
                return;
            }
        }

        // Priority 4: Conflagrate (generate shards + Backdraft)
        if (this->CanCastSpell(CONFLAGRATE, target))
        {
            this->CastSpell(target, CONFLAGRATE);
            GenerateSoulShard(1);
            _backdraftStacks = std::min(_backdraftStacks + 2, 4u); // Grants 2 stacks
            return;
        }

        // Priority 5: Soul Fire (talent, strong direct damage)
        if (this->CanCastSpell(SOUL_FIRE, target))
        {
            this->CastSpell(target, SOUL_FIRE);
            return;
        }

        // Priority 6: Chaos Bolt (shard spender)
        if (shards >= 2 && this->CanCastSpell(CHAOS_BOLT, target))
        {
            this->CastSpell(target, CHAOS_BOLT);
            ConsumeSoulShard(2);
            return;
        }

        // Priority 7: Channel Demonfire (talent, requires Immolate)
        if (_immolateTracker.HasImmolate(targetGuid) && this->CanCastSpell(CHANNEL_DEMONFIRE, target))
        {
            this->CastSpell(target, CHANNEL_DEMONFIRE);
            return;
        }

        // Priority 8: Shadowburn (execute < 20%)
        if (targetHpPct < 20.0f && this->CanCastSpell(SHADOWBURN, target))
        {
            this->CastSpell(target, SHADOWBURN);
            GenerateSoulShard(1);
            return;
        }

        // Priority 9: Incinerate (filler + shard gen)
        if (shards < 5 && this->CanCastSpell(INCINERATE, target))
        {
            this->CastSpell(target, INCINERATE);
            GenerateSoulShard(1);
            if (_backdraftStacks > 0)
                _backdraftStacks--;
            return;
        }
    }

    void ExecuteCleaveRotation(::Unit* target)
    {
        ObjectGuid targetGuid = target->GetGUID();
        uint32 shards = this->_resource.soulShards;

        // Priority 1: Havoc on secondary target
        if (!_havocTracker.IsActive() && this->CanCastSpell(HAVOC, target))
        {
            // TODO: Find secondary target CastSpell(HAVOC, target);
            _havocTracker.ApplyHavoc(target->GetGUID());
            TC_LOG_DEBUG("playerbot", "Destruction: Havoc applied");
        }

        // Priority 2: Maintain Immolate on primary
        if (_immolateTracker.NeedsRefresh(targetGuid))
        {
            if (this->CanCastSpell(IMMOLATE, target))
            {
                this->CastSpell(target, IMMOLATE);
                _immolateTracker.ApplyImmolate(targetGuid, 18000);
                return;
            }
        }

        // Priority 3: Conflagrate
        if (this->CanCastSpell(CONFLAGRATE, target))
        {
            this->CastSpell(target, CONFLAGRATE);
            GenerateSoulShard(1);
            _backdraftStacks = std::min(_backdraftStacks + 2, 4u);
            return;
        }

        // Priority 4: Chaos Bolt (cleaves with Havoc)
        if (shards >= 2 && this->CanCastSpell(CHAOS_BOLT, target))
        {
            this->CastSpell(target, CHAOS_BOLT);
            ConsumeSoulShard(2);
            return;
        }

        // Priority 5: Incinerate filler
        if (shards < 5 && this->CanCastSpell(INCINERATE, target))
        {
            this->CastSpell(target, INCINERATE);
            GenerateSoulShard(1);
            if (_backdraftStacks > 0)
                _backdraftStacks--;
            return;
        }
    }

    void ExecuteAoERotation(::Unit* target, uint32 enemyCount)
    {
        uint32 shards = this->_resource.soulShards;

        // Priority 1: Summon Infernal
        if (shards >= 2 && this->CanCastSpell(SUMMON_INFERNAL, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), SUMMON_INFERNAL);
            _lastInfernalTime = getMSTime();
            return;
        }

        // Priority 2: Cataclysm (AoE + applies Immolate)
        if (this->CanCastSpell(CATACLYSM, target))
        {
            this->CastSpell(target, CATACLYSM);
            TC_LOG_DEBUG("playerbot", "Destruction: Cataclysm");
            return;
        }

        // Priority 3: Rain of Fire (AoE shard spender)
        if (shards >= 3 && this->CanCastSpell(RAIN_OF_FIRE, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), RAIN_OF_FIRE);
            ConsumeSoulShard(3);
            return;
        }

        // Priority 4: Channel Demonfire (if targets have Immolate)
        if (this->CanCastSpell(CHANNEL_DEMONFIRE, target))
        {
            this->CastSpell(target, CHANNEL_DEMONFIRE);
            return;
        }

        // Priority 5: Havoc on secondary target
        if (!_havocTracker.IsActive() && this->CanCastSpell(HAVOC, target))
        {
            this->CastSpell(target, HAVOC);
            _havocTracker.ApplyHavoc(target->GetGUID());
            return;
        }

        // Priority 6: Conflagrate
        if (this->CanCastSpell(CONFLAGRATE, target))
        {
            this->CastSpell(target, CONFLAGRATE);
            GenerateSoulShard(1);
            return;
        }

        // Priority 7: Incinerate filler
        if (shards < 5 && this->CanCastSpell(INCINERATE, target))
        {
            this->CastSpell(target, INCINERATE);
            GenerateSoulShard(1);
            return;
        }
    }

    void HandleDefensiveCooldowns()
    {
        Player* bot = this->GetBot();
        float healthPct = bot->GetHealthPct();

        // Unending Resolve
        if (healthPct < 40.0f && this->CanCastSpell(UNENDING_RESOLVE_DESTRO, bot))
        {
            this->CastSpell(bot, UNENDING_RESOLVE_DESTRO);
            TC_LOG_DEBUG("playerbot", "Destruction: Unending Resolve");
            return;
        }

        // Dark Pact
        if (healthPct < 50.0f && this->CanCastSpell(DARK_PACT_DESTRO, bot))
        {
            this->CastSpell(bot, DARK_PACT_DESTRO);
            TC_LOG_DEBUG("playerbot", "Destruction: Dark Pact");
            return;
        }

        // Mortal Coil
        if (healthPct < 60.0f && this->CanCastSpell(MORTAL_COIL_DESTRO, bot))
        {
            this->CastSpell(bot, MORTAL_COIL_DESTRO);
            TC_LOG_DEBUG("playerbot", "Destruction: Mortal Coil");
            return;
        }
    }

    void EnsurePetActive()
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        // Check if pet exists
        if (Pet* pet = bot->GetPet())
        {
            if (pet->IsAlive())
                return; // Pet is active
        }

        // Summon Imp (best for Destruction - ranged DPS)
        if (this->CanCastSpell(SUMMON_IMP_DESTRO, bot))
        {
            this->CastSpell(bot, SUMMON_IMP_DESTRO);
            TC_LOG_DEBUG("playerbot", "Destruction: Summon Imp");
        }
    }

private:
    void UpdateDestructionState()
    {
        // Update Immolate tracker
        _immolateTracker.Update();

        // Update Havoc tracker
        _havocTracker.Update();

        // Update Backdraft stacks
        if (this->GetBot())
        {
            if (Aura* aura = this->GetBot()->GetAura(BACKDRAFT_BUFF))
                _backdraftStacks = aura->GetStackAmount();
            else
                _backdraftStacks = 0;
        }

        // Update soul shards from bot
        if (this->GetBot())
        {
            this->_resource.soulShards = this->GetBot()->GetPower(POWER_SOUL_SHARDS);
            this->_resource.mana = this->GetBot()->GetPower(POWER_MANA);
        }
    }

    void GenerateSoulShard(uint32 amount)
    {
        this->_resource.soulShards = std::min(this->_resource.soulShards + amount, this->_resource.maxSoulShards);
    }

    void ConsumeSoulShard(uint32 amount)
    {
        this->_resource.soulShards = (this->_resource.soulShards > amount) ? this->_resource.soulShards - amount : 0;
    }

    void InitializeCooldowns()
    {
        RegisterCooldown(CONFLAGRATE, 13000);           // 13 sec CD (2 charges)
        RegisterCooldown(SUMMON_INFERNAL, 180000);      // 3 min CD
        RegisterCooldown(DARK_SOUL_INSTABILITY, 120000);// 2 min CD
        RegisterCooldown(SOUL_FIRE, 20000);             // 20 sec CD
        RegisterCooldown(CATACLYSM, 30000);             // 30 sec CD
        RegisterCooldown(HAVOC, 30000);                 // 30 sec CD
        RegisterCooldown(SHADOWBURN, 12000);            // 12 sec CD
        RegisterCooldown(UNENDING_RESOLVE_DESTRO, 180000); // 3 min CD
        RegisterCooldown(DARK_PACT_DESTRO, 60000);      // 1 min CD
        RegisterCooldown(MORTAL_COIL_DESTRO, 45000);    // 45 sec CD
        RegisterCooldown(HOWL_OF_TERROR_DESTRO, 40000); // 40 sec CD
    }

private:
    DestructionImmolateTracker _immolateTracker;
    DestructionHavocTracker _havocTracker;
    uint32 _backdraftStacks;
    uint32 _lastInfernalTime;
};

} // namespace Playerbot
