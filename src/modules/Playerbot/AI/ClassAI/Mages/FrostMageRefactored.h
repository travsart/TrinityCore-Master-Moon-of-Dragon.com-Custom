/*
 * Copyright (C) 2025 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PLAYERBOT_FROSTMAGEREFACTORED_H
#define PLAYERBOT_FROSTMAGEREFACTORED_H

#include "Player.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include <unordered_map>
#include "Log.h"
#include "../CombatSpecializationTemplates.h"
#include "MageSpecialization.h"

namespace Playerbot
{

// WoW 11.2 (The War Within) - Frost Mage Spell IDs
constexpr uint32 FROST_FROSTBOLT = 116;
constexpr uint32 FROST_ICE_LANCE = 30455;
constexpr uint32 FROST_FLURRY = 44614;
constexpr uint32 FROST_FROZEN_ORB = 84714;
constexpr uint32 FROST_BLIZZARD = 190356;
constexpr uint32 FROST_COMET_STORM = 153595;
constexpr uint32 FROST_RAY_OF_FROST = 205021;
constexpr uint32 FROST_GLACIAL_SPIKE = 199786;
constexpr uint32 FROST_ICY_VEINS = 12472;
constexpr uint32 FROST_CONE_OF_COLD = 120;
constexpr uint32 FROST_FREEZE = 33395; // Water Elemental ability
constexpr uint32 FROST_SUMMON_WATER_ELEMENTAL = 31687;
constexpr uint32 FROST_ICE_BARRIER = 11426;
constexpr uint32 FROST_ICE_BLOCK = 45438;
constexpr uint32 FROST_MIRROR_IMAGE = 55342;
constexpr uint32 FROST_SHIFTING_POWER = 382440;

// Fingers of Frost proc tracker (2 free Ice Lance charges)
class FingersOfFrostTracker
{
public:
    FingersOfFrostTracker() : _fofStacks(0), _fofEndTime(0) {}

    void ActivateProc(uint32 stacks = 1)
    {
        _fofStacks = std::min(_fofStacks + stacks, 2u); // Max 2 stacks
        _fofEndTime = getMSTime() + 15000; // 15 sec duration
    }

    void ConsumeProc()
    {
        if (_fofStacks > 0)
            _fofStacks--;
    }

    [[nodiscard]] bool IsActive() const
    {
        return _fofStacks > 0 && getMSTime() < _fofEndTime;
    }

    [[nodiscard]] uint32 GetStacks() const { return _fofStacks; }

    void Update(Player* bot)
    {
        if (!bot)
            return;

        if (Aura* aura = bot->GetAura(44544)) // Fingers of Frost buff ID
        {
            _fofStacks = aura->GetStackAmount();
            _fofEndTime = getMSTime() + aura->GetDuration();
        }
        else
        {
            _fofStacks = 0;
        }
    }

private:
    uint32 _fofStacks;
    uint32 _fofEndTime;
};

// Brain Freeze proc tracker (free instant Flurry)
class BrainFreezeTracker
{
public:
    BrainFreezeTracker() : _brainFreezeActive(false), _brainFreezeEndTime(0) {}

    void ActivateProc()
    {
        _brainFreezeActive = true;
        _brainFreezeEndTime = getMSTime() + 15000; // 15 sec duration
    }

    void ConsumeProc()
    {
        _brainFreezeActive = false;
    }

    [[nodiscard]] bool IsActive() const
    {
        return _brainFreezeActive && getMSTime() < _brainFreezeEndTime;
    }

    void Update(Player* bot)
    {
        if (!bot)
            return;

        if (bot->HasAura(190446)) // Brain Freeze buff ID
        {
            _brainFreezeActive = true;
            if (Aura* aura = bot->GetAura(190446))
                _brainFreezeEndTime = getMSTime() + aura->GetDuration();
        }
        else
        {
            _brainFreezeActive = false;
        }
    }

private:
    bool _brainFreezeActive;
    uint32 _brainFreezeEndTime;
};

// Icicle tracker for Glacial Spike (requires 5 icicles)
class IcicleTracker
{
public:
    IcicleTracker() : _icicles(0), _maxIcicles(5) {}

    void AddIcicle(uint32 amount = 1)
    {
        _icicles = std::min(_icicles + amount, _maxIcicles);
    }

    void ConsumeIcicles()
    {
        _icicles = 0;
    }

    [[nodiscard]] uint32 GetIcicles() const { return _icicles; }
    [[nodiscard]] bool IsMaxIcicles() const { return _icicles >= _maxIcicles; }

    void Update(Player* bot)
    {
        if (!bot)
            return;

        // Track icicles from Frostbolt casts
        // Real implementation would track Icicles aura stacks
        // Placeholder: maintain current count
    }

private:
    uint32 _icicles;
    uint32 _maxIcicles;
};

class FrostMageRefactored : public RangedDpsSpecialization<ManaResource>, public MageSpecialization
{
public:
    using Base = RangedDpsSpecialization<ManaResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::_resource;
    explicit FrostMageRefactored(Player* bot)
        : RangedDpsSpecialization<ManaResource>(bot)
        , MageSpecialization(bot)
        , _fofTracker()
        , _brainFreezeTracker()
        , _icicleTracker()
        , _icyVeinsActive(false)
        , _icyVeinsEndTime(0)
        , _lastIcyVeinsTime(0)
        , _lastFrozenOrbTime(0)
    {
        InitializeCooldowns();
        TC_LOG_DEBUG("playerbot", "FrostMageRefactored initialized for {}", bot->GetName());
    }

    void UpdateRotation(::Unit* target) override
    {
        Player* bot = this->GetBot();
        if (!target || !bot)
            return;

        UpdateFrostState();

        uint32 enemyCount = this->GetEnemiesInRange(40.0f);

        if (enemyCount >= 3)
            ExecuteAoERotation(target, enemyCount);
        else
            ExecuteSingleTargetRotation(target);
    }

    void UpdateBuffs() override
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        // Ice Barrier for absorb shield
        if (!bot->HasAura(FROST_ICE_BARRIER))
        {
            if (this->CanCastSpell(FROST_ICE_BARRIER, bot))
            {
                this->CastSpell(bot, FROST_ICE_BARRIER);
            }
        }

        // Summon Water Elemental (permanent pet)
        if (!bot->GetPet())
        {
            if (this->CanCastSpell(FROST_SUMMON_WATER_ELEMENTAL, bot))
            {
                this->CastSpell(bot, FROST_SUMMON_WATER_ELEMENTAL);
            }
        }
    }

    void UpdateDefensives()
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        float healthPct = bot->GetHealthPct();

        // Ice Block (critical emergency)
        if (healthPct < 20.0f && this->CanCastSpell(FROST_ICE_BLOCK, bot))
        {
            this->CastSpell(bot, FROST_ICE_BLOCK);
            return;
        }

        // Mirror Image (defensive decoy)
        if (healthPct < 40.0f && this->CanCastSpell(FROST_MIRROR_IMAGE, bot))
        {
            this->CastSpell(bot, FROST_MIRROR_IMAGE);
            return;
        }

        // Shifting Power (reset cooldowns)
        if (healthPct < 50.0f && this->CanCastSpell(FROST_SHIFTING_POWER, bot))
        {
            this->CastSpell(bot, FROST_SHIFTING_POWER);
            return;
        }
    }

private:
    void InitializeCooldowns()
    {
        _lastIcyVeinsTime = 0;
        _lastFrozenOrbTime = 0;
    }

    void UpdateFrostState()
    {
        Player* bot = this->GetBot();
        // Resource (mana) is managed by the base template class automatically
        _fofTracker.Update(bot);
        _brainFreezeTracker.Update(bot);
        _icicleTracker.Update(bot);
        UpdateCooldownStates();
    }

    void UpdateCooldownStates()
    {
        Player* bot = this->GetBot();
        // Icy Veins state
        if (_icyVeinsActive && getMSTime() >= _icyVeinsEndTime)
            _icyVeinsActive = false;

        if (bot->HasAura(FROST_ICY_VEINS))
        {
            _icyVeinsActive = true;
            if (Aura* aura = bot->GetAura(FROST_ICY_VEINS))
                _icyVeinsEndTime = getMSTime() + aura->GetDuration();
        }
    }

    void ExecuteSingleTargetRotation(::Unit* target)
    {
        Player* bot = this->GetBot();
        // Icy Veins (major DPS cooldown)
        if (!_icyVeinsActive)
        {
            if (this->CanCastSpell(FROST_ICY_VEINS, bot))
            {
                this->CastSpell(bot, FROST_ICY_VEINS);
                _icyVeinsActive = true;
                _icyVeinsEndTime = getMSTime() + 20000; // 20 sec
                _lastIcyVeinsTime = getMSTime();
                return;
            }
        }

        // Frozen Orb (generates Fingers of Frost procs)
        if ((getMSTime() - _lastFrozenOrbTime) >= 60000) // 60 sec CD
        {
            if (this->CanCastSpell(FROST_FROZEN_ORB, target))
            {
                this->CastSpell(target, FROST_FROZEN_ORB);
                _lastFrozenOrbTime = getMSTime();
                _fofTracker.ActivateProc(2); // Generate 2 FoF procs
                return;
            }
        }

        // Glacial Spike with 5 icicles (if talented)
        if (bot->HasSpell(FROST_GLACIAL_SPIKE) && _icicleTracker.IsMaxIcicles())
        {
            if (this->CanCastSpell(FROST_GLACIAL_SPIKE, target))
            {
                this->CastSpell(target, FROST_GLACIAL_SPIKE);
                _icicleTracker.ConsumeIcicles();
                return;
            }
        }

        // Ray of Frost with Icy Veins (if talented - channeled damage)
        if (bot->HasSpell(FROST_RAY_OF_FROST) && _icyVeinsActive)
        {
            if (this->CanCastSpell(FROST_RAY_OF_FROST, target))
            {
                this->CastSpell(target, FROST_RAY_OF_FROST);
                return;
            }
        }

        // Flurry with Brain Freeze proc (instant cast, Winter's Chill debuff)
        if (_brainFreezeTracker.IsActive())
        {
            if (this->CanCastSpell(FROST_FLURRY, target))
            {
                this->CastSpell(target, FROST_FLURRY);
                _brainFreezeTracker.ConsumeProc();
                // Follow up with Ice Lance while target has Winter's Chill
                if (this->CanCastSpell(FROST_ICE_LANCE, target))
                {
                    this->CastSpell(target, FROST_ICE_LANCE);
                }
                return;
            }
        }

        // Ice Lance with Fingers of Frost proc (free shatter damage)
        if (_fofTracker.IsActive())
        {
            if (this->CanCastSpell(FROST_ICE_LANCE, target))
            {
                this->CastSpell(target, FROST_ICE_LANCE);
                _fofTracker.ConsumeProc();
                return;
            }
        }

        // Comet Storm (if talented - burst damage)
        if (bot->HasSpell(FROST_COMET_STORM))
        {
            if (this->CanCastSpell(FROST_COMET_STORM, target))
            {
                this->CastSpell(target, FROST_COMET_STORM);
                return;
            }
        }

        // Frostbolt (builder - generates icicles and procs)
        if (this->CanCastSpell(FROST_FROSTBOLT, target))
        {
            this->CastSpell(target, FROST_FROSTBOLT);
            _icicleTracker.AddIcicle();

            // Chance to proc Brain Freeze
            if (rand() % 100 < 15) // 15% chance (simplified)
                _brainFreezeTracker.ActivateProc();

            return;
        }
    }

    void ExecuteAoERotation(::Unit* target, uint32 enemyCount)
    {
        Player* bot = this->GetBot();
        // Icy Veins for AoE burst
        if (!_icyVeinsActive && enemyCount >= 4)
        {
            if (this->CanCastSpell(FROST_ICY_VEINS, bot))
            {
                this->CastSpell(bot, FROST_ICY_VEINS);
                _icyVeinsActive = true;
                _icyVeinsEndTime = getMSTime() + 20000;
                _lastIcyVeinsTime = getMSTime();
                return;
            }
        }

        // Frozen Orb (AoE damage and FoF procs)
        if ((getMSTime() - _lastFrozenOrbTime) >= 60000)
        {
            if (this->CanCastSpell(FROST_FROZEN_ORB, target))
            {
                this->CastSpell(target, FROST_FROZEN_ORB);
                _lastFrozenOrbTime = getMSTime();
                _fofTracker.ActivateProc(2);
                return;
            }
        }

        // Comet Storm for AoE damage
        if (bot->HasSpell(FROST_COMET_STORM) && enemyCount >= 3)
        {
            if (this->CanCastSpell(FROST_COMET_STORM, target))
            {
                this->CastSpell(target, FROST_COMET_STORM);
                return;
            }
        }

        // Blizzard (ground AoE)
        if (enemyCount >= 3)
        {
            if (this->CanCastSpell(FROST_BLIZZARD, target))
            {
                this->CastSpell(target, FROST_BLIZZARD);
                return;
            }
        }

        // Cone of Cold (close-range AoE)
        if (GetNearbyEnemies(12.0f) >= 3)
        {
            if (this->CanCastSpell(FROST_CONE_OF_COLD, target))
            {
                this->CastSpell(target, FROST_CONE_OF_COLD);
                return;
            }
        }

        // Flurry with Brain Freeze
        if (_brainFreezeTracker.IsActive())
        {
            if (this->CanCastSpell(FROST_FLURRY, target))
            {
                this->CastSpell(target, FROST_FLURRY);
                _brainFreezeTracker.ConsumeProc();
                return;
            }
        }

        // Ice Lance with Fingers of Frost (AoE shatter)
        if (_fofTracker.IsActive())
        {
            if (this->CanCastSpell(FROST_ICE_LANCE, target))
            {
                this->CastSpell(target, FROST_ICE_LANCE);
                _fofTracker.ConsumeProc();
                return;
            }
        }

        // Frostbolt as filler
        if (this->CanCastSpell(FROST_FROSTBOLT, target))
        {
            this->CastSpell(target, FROST_FROSTBOLT);
            _icicleTracker.AddIcicle();

            // Chance to proc Brain Freeze
            if (rand() % 100 < 15)
                _brainFreezeTracker.ActivateProc();

            return;
        }
    }

    [[nodiscard]] uint32 GetEnemiesInRange(float range) const
    {
        Player* bot = this->GetBot();
        if (!bot)
            return 0;

        uint32 count = 0;
        // Simplified enemy counting
        return std::min(count, 10u);
    }

    [[nodiscard]] uint32 GetNearbyEnemies(float range) const
    {
        return GetEnemiesInRange(range);
    }

    // Member variables
    FingersOfFrostTracker _fofTracker;
    BrainFreezeTracker _brainFreezeTracker;
    IcicleTracker _icicleTracker;

    bool _icyVeinsActive;
    uint32 _icyVeinsEndTime;

    uint32 _lastIcyVeinsTime;
    uint32 _lastFrozenOrbTime;
};

} // namespace Playerbot

#endif // PLAYERBOT_FROSTMAGEREFACTORED_H
