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

#ifndef PLAYERBOT_FIREMAGEREFACTORED_H
#define PLAYERBOT_FIREMAGEREFACTORED_H

#include "Player.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"
#include <unordered_map>
#include "../CombatSpecializationTemplates.h"
#include "MageSpecialization.h"

namespace Playerbot
{

// WoW 11.2 (The War Within) - Fire Mage Spell IDs
constexpr uint32 FIRE_FIREBALL = 133;
constexpr uint32 FIRE_PYROBLAST = 11366;
constexpr uint32 FIRE_FIRE_BLAST = 108853;
constexpr uint32 FIRE_SCORCH = 2948;
constexpr uint32 FIRE_FLAMESTRIKE = 2120;
constexpr uint32 FIRE_PHOENIX_FLAMES = 257541;
constexpr uint32 FIRE_COMBUSTION = 190319;
constexpr uint32 FIRE_DRAGON_BREATH = 31661;
constexpr uint32 FIRE_METEOR = 153561;
constexpr uint32 FIRE_LIVING_BOMB = 44457;
constexpr uint32 FIRE_BLAZING_BARRIER = 235313;
constexpr uint32 FIRE_ICE_BLOCK = 45438;
constexpr uint32 FIRE_MIRROR_IMAGE = 55342;
constexpr uint32 FIRE_SHIFTING_POWER = 382440;
constexpr uint32 FIRE_TIME_WARP = 80353;

// Hot Streak proc tracker (2 consecutive crits = free instant Pyroblast)
class HotStreakTracker
{
public:
    HotStreakTracker() : _hotStreakActive(false), _heatingUpActive(false), _hotStreakEndTime(0) {}

    void ActivateHeatingUp()
    {
        _heatingUpActive = true;
    }

    void ActivateHotStreak()
    {
        _hotStreakActive = true;
        _heatingUpActive = false;
        _hotStreakEndTime = getMSTime() + 15000; // 15 sec duration
    }

    void ConsumeHotStreak()
    {
        _hotStreakActive = false;
        _heatingUpActive = false;
    }

    [[nodiscard]] bool IsHotStreakActive() const
    {
        return _hotStreakActive && getMSTime() < _hotStreakEndTime;
    }

    [[nodiscard]] bool IsHeatingUpActive() const { return _heatingUpActive; }

    void Update(Player* bot)
    {
        if (!bot)
            return;

        // Hot Streak buff (instant cast Pyroblast)
        if (bot->HasAura(48108)) // Hot Streak buff ID
        {
            _hotStreakActive = true;
            if (Aura* aura = bot->GetAura(48108))
                _hotStreakEndTime = getMSTime() + aura->GetDuration();
        }
        else
        {
            _hotStreakActive = false;
        }

        // Heating Up buff (1 crit, need 1 more for Hot Streak)
        _heatingUpActive = bot->HasAura(48107); // Heating Up buff ID
    }

private:
    bool _hotStreakActive;
    bool _heatingUpActive;
    uint32 _hotStreakEndTime;
};

// Fire Blast charge tracker
class FireBlastChargeTracker
{
public:
    FireBlastChargeTracker() : _charges(0), _maxCharges(3), _lastChargeTime(0) {}

    void ConsumeCharge()
    {
        if (_charges > 0)
        {
            _charges--;
            _lastChargeTime = getMSTime();
        }
    }

    void RegenerateCharge()
    {
        if (_charges < _maxCharges)
            _charges++;
    }

    [[nodiscard]] bool HasCharge() const { return _charges > 0; }
    [[nodiscard]] uint32 GetCharges() const { return _charges; }

    void Update(Player* bot)
    {
        if (!bot)
            return;

        // Fire Blast has 3 charges with 10 sec recharge
        // Simplified tracking - check spell charges
        _charges = 3; // Placeholder - real implementation would track charges properly
    }

private:
    uint32 _charges;
    uint32 _maxCharges;
    uint32 _lastChargeTime;
};

class FireMageRefactored : public RangedDpsSpecialization<ManaResource>, public MageSpecialization
{
public:
    // Use base class members directly with this-> or explicit qualification
    using Base = RangedDpsSpecialization<ManaResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::IsSpellReady;
    using Base::GetEnemiesInRange;
    using Base::_resource;

    explicit FireMageRefactored(Player* bot)
        : RangedDpsSpecialization<ManaResource>(bot)
        , MageSpecialization(bot)
        , _hotStreakTracker()
        , _fireBlastTracker()
        , _combustionActive(false)
        , _combustionEndTime(0)
        , _lastCombustionTime(0)
        , _lastPhoenixFlamesTime(0)
    {
        InitializeCooldowns();
        TC_LOG_DEBUG("playerbot", "FireMageRefactored initialized for {}", bot->GetName());
    }

    void UpdateRotation(::Unit* target) override
    {
        if (!target || !this->GetBot())
            return;

        UpdateFireState();

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

        // Blazing Barrier for absorb shield
        if (!bot->HasAura(FIRE_BLAZING_BARRIER))
        {
            if (this->CanCastSpell(FIRE_BLAZING_BARRIER, bot))
            {
                this->CastSpell(bot, FIRE_BLAZING_BARRIER);
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
        if (healthPct < 20.0f && this->CanCastSpell(FIRE_ICE_BLOCK, bot))
        {
            this->CastSpell(bot, FIRE_ICE_BLOCK);
            return;
        }

        // Mirror Image (defensive decoy)
        if (healthPct < 40.0f && this->CanCastSpell(FIRE_MIRROR_IMAGE, bot))
        {
            this->CastSpell(bot, FIRE_MIRROR_IMAGE);
            return;
        }

        // Shifting Power (reset cooldowns) - self-cast
        if (healthPct < 50.0f && this->CanCastSpell(FIRE_SHIFTING_POWER, bot))
        {
            this->CastSpell(bot, FIRE_SHIFTING_POWER);
            return;
        }
    }

private:
    void InitializeCooldowns()
    {
        _lastCombustionTime = 0;
        _lastPhoenixFlamesTime = 0;
    }

    void UpdateFireState()
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        // Resource (mana) is managed by the base template class automatically
        _hotStreakTracker.Update(bot);
        _fireBlastTracker.Update(bot);
        UpdateCooldownStates();
    }

    void UpdateCooldownStates()
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        // Combustion state
        if (_combustionActive && getMSTime() >= _combustionEndTime)
            _combustionActive = false;

        if (bot->HasAura(FIRE_COMBUSTION))
        {
            _combustionActive = true;
            if (Aura* aura = bot->GetAura(FIRE_COMBUSTION))
                _combustionEndTime = getMSTime() + aura->GetDuration();
        }
    }

    void ExecuteSingleTargetRotation(::Unit* target)
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        // Combustion (major DPS cooldown - crits guaranteed)
        if (!_combustionActive && _hotStreakTracker.IsHotStreakActive())
        {
            if (this->CanCastSpell(FIRE_COMBUSTION, bot))
            {
                this->CastSpell(bot, FIRE_COMBUSTION);
                _combustionActive = true;
                _combustionEndTime = getMSTime() + 10000; // 10 sec
                _lastCombustionTime = getMSTime();
                return;
            }
        }

        // Pyroblast with Hot Streak proc (instant cast, high damage)
        if (_hotStreakTracker.IsHotStreakActive())
        {
            if (this->CanCastSpell(FIRE_PYROBLAST, target))
            {
                this->CastSpell(target, FIRE_PYROBLAST);
                _hotStreakTracker.ConsumeHotStreak();
                return;
            }
        }

        // Fire Blast (generate Hot Streak if Heating Up is active)
        if (_hotStreakTracker.IsHeatingUpActive() && _fireBlastTracker.HasCharge())
        {
            if (this->CanCastSpell(FIRE_FIRE_BLAST, target))
            {
                this->CastSpell(target, FIRE_FIRE_BLAST);
                _fireBlastTracker.ConsumeCharge();
                _hotStreakTracker.ActivateHotStreak(); // Heating Up + crit = Hot Streak
                return;
            }
        }

        // Phoenix Flames (high damage, generates Heating Up)
        if (bot->HasSpell(FIRE_PHOENIX_FLAMES) && (getMSTime() - _lastPhoenixFlamesTime) >= 30000)
        {
            if (this->CanCastSpell(FIRE_PHOENIX_FLAMES, target))
            {
                this->CastSpell(target, FIRE_PHOENIX_FLAMES);
                _lastPhoenixFlamesTime = getMSTime();
                _hotStreakTracker.ActivateHeatingUp();
                return;
            }
        }

        // Meteor (if talented - big AoE damage)
        if (bot->HasSpell(FIRE_METEOR))
        {
            if (this->CanCastSpell(FIRE_METEOR, target))
            {
                this->CastSpell(target, FIRE_METEOR);
                return;
            }
        }

        // Scorch during movement (instant cast filler)
        if (bot->HasUnitMovementFlag(MOVEMENTFLAG_FORWARD))
        {
            if (this->CanCastSpell(FIRE_SCORCH, target))
            {
                this->CastSpell(target, FIRE_SCORCH);
                return;
            }
        }

        // Fireball (builder - chance to proc Heating Up on crit)
        if (this->CanCastSpell(FIRE_FIREBALL, target))
        {
            this->CastSpell(target, FIRE_FIREBALL);

            // Simulate crit chance for proc
            if (rand() % 100 < 30) // 30% crit chance (simplified)
            {
                if (!_hotStreakTracker.IsHeatingUpActive())
                    _hotStreakTracker.ActivateHeatingUp();
                else
                    _hotStreakTracker.ActivateHotStreak(); // 2 crits = Hot Streak
            }

            return;
        }
    }

    void ExecuteAoERotation(::Unit* target, uint32 enemyCount)
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        // Combustion for AoE burst
        if (!_combustionActive && _hotStreakTracker.IsHotStreakActive() && enemyCount >= 4)
        {
            if (this->CanCastSpell(FIRE_COMBUSTION, bot))
            {
                this->CastSpell(bot, FIRE_COMBUSTION);
                _combustionActive = true;
                _combustionEndTime = getMSTime() + 10000;
                _lastCombustionTime = getMSTime();
                return;
            }
        }

        // Meteor (massive AoE damage)
        if (bot->HasSpell(FIRE_METEOR) && enemyCount >= 4)
        {
            if (this->CanCastSpell(FIRE_METEOR, target))
            {
                this->CastSpell(target, FIRE_METEOR);
                return;
            }
        }

        // Dragon's Breath (cone AoE)
        if (enemyCount >= 3 && GetNearbyEnemies(12.0f) >= 3)
        {
            if (this->CanCastSpell(FIRE_DRAGON_BREATH, target))
            {
                this->CastSpell(target, FIRE_DRAGON_BREATH);
                return;
            }
        }

        // Flamestrike (ground AoE with Hot Streak proc for instant cast)
        if (_hotStreakTracker.IsHotStreakActive() && enemyCount >= 3)
        {
            if (this->CanCastSpell(FIRE_FLAMESTRIKE, target))
            {
                this->CastSpell(target, FIRE_FLAMESTRIKE);
                _hotStreakTracker.ConsumeHotStreak();
                return;
            }
        }

        // Fire Blast to generate Hot Streak
        if (_hotStreakTracker.IsHeatingUpActive() && _fireBlastTracker.HasCharge())
        {
            if (this->CanCastSpell(FIRE_FIRE_BLAST, target))
            {
                this->CastSpell(target, FIRE_FIRE_BLAST);
                _fireBlastTracker.ConsumeCharge();
                _hotStreakTracker.ActivateHotStreak();
                return;
            }
        }

        // Phoenix Flames for AoE damage
        if (bot->HasSpell(FIRE_PHOENIX_FLAMES) && (getMSTime() - _lastPhoenixFlamesTime) >= 30000)
        {
            if (this->CanCastSpell(FIRE_PHOENIX_FLAMES, target))
            {
                this->CastSpell(target, FIRE_PHOENIX_FLAMES);
                _lastPhoenixFlamesTime = getMSTime();
                _hotStreakTracker.ActivateHeatingUp();
                return;
            }
        }

        // Living Bomb (if talented - DoT that spreads on death)
        if (bot->HasSpell(FIRE_LIVING_BOMB) && enemyCount >= 3)
        {
            if (this->CanCastSpell(FIRE_LIVING_BOMB, target))
            {
                this->CastSpell(target, FIRE_LIVING_BOMB);
                return;
            }
        }

        // Flamestrike (hardcast if no Hot Streak)
        if (enemyCount >= 3)
        {
            if (this->CanCastSpell(FIRE_FLAMESTRIKE, target))
            {
                this->CastSpell(target, FIRE_FLAMESTRIKE);
                return;
            }
        }

        // Fireball as filler
        if (this->CanCastSpell(FIRE_FIREBALL, target))
        {
            this->CastSpell(target, FIRE_FIREBALL);

            // Simulate crit for proc
            if (rand() % 100 < 30)
            {
                if (!_hotStreakTracker.IsHeatingUpActive())
                    _hotStreakTracker.ActivateHeatingUp();
                else
                    _hotStreakTracker.ActivateHotStreak();
            }

            return;
        }
    }

    [[nodiscard]] uint32 GetNearbyEnemies(float range) const
    {
        return this->GetEnemiesInRange(range);
    }

    // Member variables
    HotStreakTracker _hotStreakTracker;
    FireBlastChargeTracker _fireBlastTracker;

    bool _combustionActive;
    uint32 _combustionEndTime;

    uint32 _lastCombustionTime;
    uint32 _lastPhoenixFlamesTime;
};

} // namespace Playerbot

#endif // PLAYERBOT_FIREMAGEREFACTORED_H
