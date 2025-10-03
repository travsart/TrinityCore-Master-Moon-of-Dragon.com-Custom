/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * Affliction Warlock Refactored - Template-Based Implementation
 *
 * This file provides a complete, template-based implementation of Affliction Warlock
 * using the RangedDpsSpecialization with dual resource system (Mana + Soul Shards).
 */

#pragma once
#include "Pet.h"

#include "../CombatSpecializationTemplates.h"
#include "../ResourceTypes.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellAuraEffects.h"
#include "Log.h"
#include "WarlockSpecialization.h"

namespace Playerbot
{

// ============================================================================
// AFFLICTION WARLOCK SPELL IDs (WoW 11.2 - The War Within)
// ============================================================================

enum AfflictionWarlockSpells
{
    // DoT Spells
    AGONY                    = 980,     // Core DoT, stacks up to 10
    CORRUPTION               = 172,     // Core DoT
    UNSTABLE_AFFLICTION      = 316099,  // Strong DoT, generates shards on refresh
    SIPHON_LIFE              = 63106,   // DoT + heal (talent)

    // Direct Damage
    SHADOW_BOLT_AFF          = 686,     // Filler, generates shards
    DRAIN_SOUL               = 198590,  // Execute damage (< 20% HP)
    MALEFIC_RAPTURE          = 324536,  // Shard spender, AoE burst

    // Major Cooldowns
    PHANTOM_SINGULARITY      = 205179,  // 45 sec CD, AoE DoT (talent)
    VILE_TAINT               = 278350,  // 20 sec CD, AoE DoT (talent)
    SOUL_ROT                 = 386997,  // 1 min CD, AoE DoT (talent)
    SUMMON_DARKGLARE         = 205180,  // 2 min CD, extends DoTs

    // AoE
    SEED_OF_CORRUPTION       = 27243,   // AoE DoT spread
    SOULBURN                 = 385899,  // Instant Seed of Corruption (talent)

    // Pet Management
    SUMMON_IMP_AFF           = 688,
    SUMMON_VOIDWALKER_AFF    = 697,
    SUMMON_FELHUNTER_AFF     = 691,
    SUMMON_SUCCUBUS_AFF      = 712,
    COMMAND_DEMON_AFF        = 119898,

    // Utility
    CURSE_OF_WEAKNESS        = 702,     // Reduces physical damage
    CURSE_OF_TONGUES         = 1714,    // Casting slow (talent)
    CURSE_OF_EXHAUSTION      = 334275,  // Movement slow
    UNENDING_RESOLVE         = 104773,  // 3 min CD, damage reduction
    DARK_PACT                = 108416,  // 1 min CD, shield (talent)
    MORTAL_COIL              = 6789,    // Heal + fear (talent)
    HOWL_OF_TERROR           = 5484,    // AoE fear (talent)
    FEAR                     = 5782,    // CC
    BANISH                   = 710,     // CC (demons/elementals)
    SOULSTONE                = 20707,   // Battle res

    // Defensives
    HEALTH_FUNNEL            = 755,     // Channel, heals pet
    DEMONIC_CIRCLE_TELEPORT  = 48020,   // Teleport
    DEMONIC_GATEWAY          = 111771,  // Portal
    BURNING_RUSH             = 111400,  // Speed buff, drains health

    // Procs and Buffs
    NIGHTFALL                = 108558,  // Proc: free Shadow Bolt
    INEVITABLE_DEMISE        = 334319,  // Stacking drain life buff
    TORMENTED_CRESCENDO      = 387079,  // Stacking Malefic Rapture buff

    // Talents
    GRIMOIRE_OF_SACRIFICE    = 108503,  // Sacrifice pet for damage buff
    SOUL_CONDUIT             = 215941,  // Chance to refund soul shards
    CREEPING_DEATH           = 264000,  // DoT speed increase
    WRITHE_IN_AGONY          = 196102   // Agony damage increase
};

// Dual resource type for Warlock (Mana + Soul Shards)
struct ManaSoulShardResource
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
        // Mana regenerates naturally over time
        // This is a simplified implementation
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
        available = mana > 0;
    }
};

// ============================================================================
// AFFLICTION DOT TRACKER
// ============================================================================

class AfflictionDoTTracker
{
public:
    AfflictionDoTTracker()
        : _trackedDoTs()
    {
    }

    struct DoTInfo
    {
        uint32 spellId;
        uint32 endTime;
        uint32 stacks;
    };

    void ApplyDoT(ObjectGuid targetGuid, uint32 spellId, uint32 duration, uint32 stacks = 1)
    {
        auto& dotsOnTarget = _trackedDoTs[targetGuid];
        dotsOnTarget[spellId] = { spellId, getMSTime() + duration, stacks };
    }

    void RemoveDoT(ObjectGuid targetGuid, uint32 spellId)
    {
        auto it = _trackedDoTs.find(targetGuid);
        if (it != _trackedDoTs.end())
        {
            it->second.erase(spellId);
            if (it->second.empty())
                _trackedDoTs.erase(it);
        }
    }

    bool HasDoT(ObjectGuid targetGuid, uint32 spellId) const
    {
        auto it = _trackedDoTs.find(targetGuid);
        if (it == _trackedDoTs.end())
            return false;

        auto dotIt = it->second.find(spellId);
        if (dotIt == it->second.end())
            return false;

        return getMSTime() < dotIt->second.endTime;
    }

    uint32 GetDoTTimeRemaining(ObjectGuid targetGuid, uint32 spellId) const
    {
        auto it = _trackedDoTs.find(targetGuid);
        if (it == _trackedDoTs.end())
            return 0;

        auto dotIt = it->second.find(spellId);
        if (dotIt == it->second.end())
            return 0;

        uint32 now = getMSTime();
        return (dotIt->second.endTime > now) ? (dotIt->second.endTime - now) : 0;
    }

    bool NeedsRefresh(ObjectGuid targetGuid, uint32 spellId, uint32 pandemicWindow = 5400) const
    {
        uint32 remaining = GetDoTTimeRemaining(targetGuid, spellId);
        return remaining < pandemicWindow; // Refresh if < 5.4 sec (pandemic window)
    }

    uint32 GetDoTCount(ObjectGuid targetGuid) const
    {
        auto it = _trackedDoTs.find(targetGuid);
        if (it == _trackedDoTs.end())
            return 0;

        uint32 count = 0;
        uint32 now = getMSTime();
        for (const auto& pair : it->second)
        {
            if (now < pair.second.endTime)
                ++count;
        }
        return count;
    }

    void Update()
    {
        uint32 now = getMSTime();
        for (auto targetIt = _trackedDoTs.begin(); targetIt != _trackedDoTs.end();)
        {
            for (auto dotIt = targetIt->second.begin(); dotIt != targetIt->second.end();)
            {
                if (now >= dotIt->second.endTime)
                    dotIt = targetIt->second.erase(dotIt);
                else
                    ++dotIt;
            }

            if (targetIt->second.empty())
                targetIt = _trackedDoTs.erase(targetIt);
            else
                ++targetIt;
        }
    }

private:
    std::unordered_map<ObjectGuid, std::unordered_map<uint32, DoTInfo>> _trackedDoTs;
};

// ============================================================================
// AFFLICTION WARLOCK REFACTORED
// ============================================================================

class AfflictionWarlockRefactored : public RangedDpsSpecialization<ManaSoulShardResource>, public WarlockSpecialization
{
public:
    using Base = RangedDpsSpecialization<ManaSoulShardResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::_resource;
    explicit AfflictionWarlockRefactored(Player* bot)
        : RangedDpsSpecialization<ManaSoulShardResource>(bot)
        , WarlockSpecialization(bot)
        , _dotTracker()
        , _nightfallProc(false)
        , _lastDarkglareTime(0)
    {
        // Initialize mana/soul shard resources
        this->_resource.Initialize(bot);

        InitializeCooldowns();

        TC_LOG_DEBUG("playerbot", "AfflictionWarlockRefactored initialized for {}", bot->GetName());
    }

    void UpdateRotation(::Unit* target) override
    {
        if (!target || !target->IsAlive() || !target->IsHostileTo(this->GetBot()))
            return;

        // Update Affliction state
        UpdateAfflictionState();

        // Ensure pet is active
        EnsurePetActive();

        // Determine if AoE or single target
        uint32 enemyCount = this->GetEnemiesInRange(40.0f);
        if (enemyCount >= 3)
        {
            ExecuteAoERotation(target, enemyCount);
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


protected:
    void ExecuteSingleTargetRotation(::Unit* target)
    {
        ObjectGuid targetGuid = target->GetGUID();
        uint32 shards = this->_resource.soulShards;
        float targetHpPct = target->GetHealthPct();

        // Priority 1: Use Darkglare when all DoTs are up
        if (_dotTracker.GetDoTCount(targetGuid) >= 3 && shards >= 1)
        {
            if (this->CanCastSpell(SUMMON_DARKGLARE, this->GetBot()))
            {
                this->CastSpell(this->GetBot(), SUMMON_DARKGLARE);
                _lastDarkglareTime = getMSTime();
                TC_LOG_DEBUG("playerbot", "Affliction: Summon Darkglare");
                return;
            }
        }

        // Priority 2: Maintain Agony (most important DoT)
        if (_dotTracker.NeedsRefresh(targetGuid, AGONY))
        {
            if (this->CanCastSpell(AGONY, target))
            {
                this->CastSpell(target, AGONY);
                _dotTracker.ApplyDoT(targetGuid, AGONY, 18000); // 18 sec duration
                return;
            }
        }

        // Priority 3: Maintain Corruption
        if (_dotTracker.NeedsRefresh(targetGuid, CORRUPTION))
        {
            if (this->CanCastSpell(CORRUPTION, target))
            {
                this->CastSpell(target, CORRUPTION);
                _dotTracker.ApplyDoT(targetGuid, CORRUPTION, 14000); // 14 sec duration
                return;
            }
        }

        // Priority 4: Maintain Unstable Affliction
        if (_dotTracker.NeedsRefresh(targetGuid, UNSTABLE_AFFLICTION))
        {
            if (this->CanCastSpell(UNSTABLE_AFFLICTION, target))
            {
                this->CastSpell(target, UNSTABLE_AFFLICTION);
                _dotTracker.ApplyDoT(targetGuid, UNSTABLE_AFFLICTION, 8000); // 8 sec duration
                GenerateSoulShard(1);
                return;
            }
        }

        // Priority 5: Maintain Siphon Life (talent)
        if (_dotTracker.NeedsRefresh(targetGuid, SIPHON_LIFE))
        {
            if (this->CanCastSpell(SIPHON_LIFE, target))
            {
                this->CastSpell(target, SIPHON_LIFE);
                _dotTracker.ApplyDoT(targetGuid, SIPHON_LIFE, 15000); // 15 sec duration
                return;
            }
        }

        // Priority 6: Phantom Singularity (talent)
        if (this->CanCastSpell(PHANTOM_SINGULARITY, target))
        {
            this->CastSpell(target, PHANTOM_SINGULARITY);
            return;
        }

        // Priority 7: Vile Taint (talent)
        if (this->CanCastSpell(VILE_TAINT, target))
        {
            this->CastSpell(target, VILE_TAINT);
            return;
        }

        // Priority 8: Malefic Rapture (spend shards)
        if (shards >= 1 && _dotTracker.GetDoTCount(targetGuid) >= 2)
        {
            if (this->CanCastSpell(MALEFIC_RAPTURE, target))
            {
                this->CastSpell(target, MALEFIC_RAPTURE);
                ConsumeSoulShard(1);
                return;
            }
        }

        // Priority 9: Drain Soul (execute < 20%)
        if (targetHpPct < 20.0f && this->CanCastSpell(DRAIN_SOUL, target))
        {
            this->CastSpell(target, DRAIN_SOUL);
            GenerateSoulShard(1);
            return;
        }

        // Priority 10: Shadow Bolt (filler + shard gen)
        if (shards < 5)
        {
            // Use Nightfall proc if available
            if (_nightfallProc || this->CanCastSpell(SHADOW_BOLT_AFF, target))
            {
                this->CastSpell(target, SHADOW_BOLT_AFF);
                _nightfallProc = false;
                GenerateSoulShard(1);
                return;
            }
        }
    }

    void ExecuteAoERotation(::Unit* target, uint32 enemyCount)
    {
        uint32 shards = this->_resource.soulShards;

        // Priority 1: Soul Rot (AoE DoT)
        if (this->CanCastSpell(SOUL_ROT, target))
        {
            this->CastSpell(target, SOUL_ROT);
            return;
        }

        // Priority 2: Vile Taint (AoE DoT)
        if (this->CanCastSpell(VILE_TAINT, target))
        {
            this->CastSpell(target, VILE_TAINT);
            return;
        }

        // Priority 3: Seed of Corruption (AoE spread)
        if (enemyCount >= 4 && this->CanCastSpell(SEED_OF_CORRUPTION, target))
        {
            this->CastSpell(target, SEED_OF_CORRUPTION);
            return;
        }

        // Priority 4: Apply Agony on all targets
        // TODO: Implement multi-target DoT tracking

        // Priority 5: Malefic Rapture (AoE shard spender)
        if (shards >= 2 && this->CanCastSpell(MALEFIC_RAPTURE, target))
        {
            this->CastSpell(target, MALEFIC_RAPTURE);
            ConsumeSoulShard(2);
            return;
        }

        // Priority 6: Shadow Bolt filler
        if (shards < 5 && this->CanCastSpell(SHADOW_BOLT_AFF, target))
        {
            this->CastSpell(target, SHADOW_BOLT_AFF);
            GenerateSoulShard(1);
            return;
        }
    }

    void HandleDefensiveCooldowns()
    {
        Player* bot = this->GetBot();
        float healthPct = bot->GetHealthPct();

        // Unending Resolve
        if (healthPct < 40.0f && this->CanCastSpell(UNENDING_RESOLVE, bot))
        {
            this->CastSpell(bot, UNENDING_RESOLVE);
            TC_LOG_DEBUG("playerbot", "Affliction: Unending Resolve");
            return;
        }

        // Dark Pact (talent shield)
        if (healthPct < 50.0f && this->CanCastSpell(DARK_PACT, bot))
        {
            this->CastSpell(bot, DARK_PACT);
            TC_LOG_DEBUG("playerbot", "Affliction: Dark Pact");
            return;
        }

        // Mortal Coil (heal + fear)
        if (healthPct < 60.0f && this->CanCastSpell(MORTAL_COIL, bot))
        {
            this->CastSpell(bot, MORTAL_COIL);
            TC_LOG_DEBUG("playerbot", "Affliction: Mortal Coil");
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

        // Summon Felhunter (best for Affliction - interrupt + dispel)
        if (this->CanCastSpell(SUMMON_FELHUNTER_AFF, bot))
        {
            this->CastSpell(bot, SUMMON_FELHUNTER_AFF);
            TC_LOG_DEBUG("playerbot", "Affliction: Summon Felhunter");
        }
    }

private:
    void UpdateAfflictionState()
    {
        // Update DoT tracker
        _dotTracker.Update();

        // Update Nightfall proc
        if (this->GetBot() && this->GetBot()->HasAura(NIGHTFALL))
            _nightfallProc = true;
        else
            _nightfallProc = false;

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
        RegisterCooldown(SUMMON_DARKGLARE, 120000);     // 2 min CD
        RegisterCooldown(PHANTOM_SINGULARITY, 45000);   // 45 sec CD
        RegisterCooldown(VILE_TAINT, 20000);            // 20 sec CD
        RegisterCooldown(SOUL_ROT, 60000);              // 1 min CD
        RegisterCooldown(UNENDING_RESOLVE, 180000);     // 3 min CD
        RegisterCooldown(DARK_PACT, 60000);             // 1 min CD
        RegisterCooldown(MORTAL_COIL, 45000);           // 45 sec CD
        RegisterCooldown(HOWL_OF_TERROR, 40000);        // 40 sec CD
    }

private:
    AfflictionDoTTracker _dotTracker;
    bool _nightfallProc;
    uint32 _lastDarkglareTime;
};

} // namespace Playerbot
