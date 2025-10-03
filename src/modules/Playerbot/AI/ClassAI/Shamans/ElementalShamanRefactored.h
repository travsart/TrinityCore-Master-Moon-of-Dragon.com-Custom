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

#ifndef PLAYERBOT_ELEMENTALSHAMANREFACTORED_H
#define PLAYERBOT_ELEMENTALSHAMANREFACTORED_H

#include "ShamanSpecialization.h"
#include "../CombatSpecializationTemplates.h"
#include "Player.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "ObjectGuid.h"
#include <unordered_map>
#include "Log.h"

namespace Playerbot
{

// WoW 11.2 (The War Within) - Elemental Shaman Spell IDs
constexpr uint32 ELEM_LIGHTNING_BOLT = 188196;
constexpr uint32 ELEM_LAVA_BURST = 51505;
constexpr uint32 ELEM_FLAME_SHOCK = 188389;
constexpr uint32 ELEM_EARTH_SHOCK = 8042;
constexpr uint32 ELEM_FROST_SHOCK = 196840;
constexpr uint32 ELEM_CHAIN_LIGHTNING = 188443;
constexpr uint32 ELEM_EARTHQUAKE = 61882;
constexpr uint32 ELEM_FIRE_ELEMENTAL = 198067;
constexpr uint32 ELEM_STORMKEEPER = 191634;
constexpr uint32 ELEM_ASCENDANCE = 114050;
constexpr uint32 ELEM_LIQUID_MAGMA_TOTEM = 192222;
constexpr uint32 ELEM_ICEFURY = 210714;
constexpr uint32 ELEM_ELEMENTAL_BLAST = 117014;
constexpr uint32 ELEM_ECHOING_SHOCK = 320125;
constexpr uint32 ELEM_PRIMORDIAL_WAVE = 375982;
constexpr uint32 ELEM_ASTRAL_SHIFT = 108271;
constexpr uint32 ELEM_EARTH_SHIELD = 974;
constexpr uint32 ELEM_WIND_SHEAR = 57994;
constexpr uint32 ELEM_CAPACITOR_TOTEM = 192058;

// ManaResource is already defined in CombatSpecializationTemplates.h
// No need to redefine it here

// Maelstrom tracker (tracked separately, not part of resource concept)
class MaelstromTracker
{
public:
    MaelstromTracker() : _maelstrom(0), _maxMaelstrom(100) {}

    void Generate(uint32 amount)
    {
        _maelstrom = std::min(_maelstrom + amount, _maxMaelstrom);
    }

    void Spend(uint32 amount)
    {
        if (_maelstrom >= amount)
            _maelstrom -= amount;
        else
            _maelstrom = 0;
    }

    [[nodiscard]] bool Has(uint32 amount) const { return _maelstrom >= amount; }
    [[nodiscard]] uint32 GetPercent() const { return (_maelstrom * 100) / _maxMaelstrom; }
    [[nodiscard]] uint32 GetCurrent() const { return _maelstrom; }

    void Update(Player* bot)
    {
        // Maelstrom is tracked via game mechanics, this is simplified tracking
    }

private:
    uint32 _maelstrom;
    uint32 _maxMaelstrom;
};

// Flame Shock DoT tracker
class FlameShockTracker
{
public:
    FlameShockTracker() = default;

    void ApplyFlameShock(ObjectGuid guid, uint32 duration = 18000)
    {
        _flameShockTargets[guid] = getMSTime() + duration;
    }

    void RemoveFlameShock(ObjectGuid guid)
    {
        _flameShockTargets.erase(guid);
    }

    [[nodiscard]] bool HasFlameShock(ObjectGuid guid) const
    {
        auto it = _flameShockTargets.find(guid);
        if (it == _flameShockTargets.end())
            return false;
        return getMSTime() < it->second;
    }

    [[nodiscard]] uint32 GetFlameShockTimeRemaining(ObjectGuid guid) const
    {
        auto it = _flameShockTargets.find(guid);
        if (it == _flameShockTargets.end())
            return 0;

        uint32 now = getMSTime();
        if (now >= it->second)
            return 0;

        return it->second - now;
    }

    [[nodiscard]] bool NeedsFlameShockRefresh(ObjectGuid guid, uint32 pandemicWindow = 5400) const
    {
        uint32 remaining = GetFlameShockTimeRemaining(guid);
        return remaining < pandemicWindow;
    }

    [[nodiscard]] uint32 GetActiveFlameShockCount() const
    {
        uint32 count = 0;
        uint32 now = getMSTime();
        for (const auto& pair : _flameShockTargets)
        {
            if (now < pair.second)
                ++count;
        }
        return count;
    }

    void Update(Player* bot)
    {
        if (!bot)
            return;

        // Clean up expired Flame Shocks
        uint32 now = getMSTime();
        for (auto it = _flameShockTargets.begin(); it != _flameShockTargets.end();)
        {
            if (now >= it->second)
                it = _flameShockTargets.erase(it);
            else
                ++it;
        }
    }

private:
    std::unordered_map<ObjectGuid, uint32> _flameShockTargets; // GUID -> expiration time
};

// Lava Surge proc tracker (instant Lava Burst)
class LavaSurgeTracker
{
public:
    LavaSurgeTracker() : _lavaSurgeActive(false), _lavaSurgeEndTime(0) {}

    void ActivateProc()
    {
        _lavaSurgeActive = true;
        _lavaSurgeEndTime = getMSTime() + 15000; // 15 sec duration
    }

    void ConsumeProc()
    {
        _lavaSurgeActive = false;
    }

    [[nodiscard]] bool IsActive() const
    {
        return _lavaSurgeActive && getMSTime() < _lavaSurgeEndTime;
    }

    void Update(Player* bot)
    {
        if (!bot)
            return;

        // Check if Lava Surge buff is active
        if (bot->HasAura(77762)) // Lava Surge buff ID
        {
            _lavaSurgeActive = true;
            if (Aura* aura = bot->GetAura(77762))
                _lavaSurgeEndTime = getMSTime() + aura->GetDuration();
        }
        else
        {
            _lavaSurgeActive = false;
        }

        // Expire if time is up
        if (_lavaSurgeActive && getMSTime() >= _lavaSurgeEndTime)
            _lavaSurgeActive = false;
    }

private:
    bool _lavaSurgeActive;
    uint32 _lavaSurgeEndTime;
};

// Stormkeeper proc tracker (instant Lightning Bolts)
class StormkeeperTracker
{
public:
    StormkeeperTracker() : _stormkeeperStacks(0), _stormkeeperEndTime(0) {}

    void ActivateProc(uint32 stacks = 2)
    {
        _stormkeeperStacks = stacks;
        _stormkeeperEndTime = getMSTime() + 15000; // 15 sec duration
    }

    void ConsumeStack()
    {
        if (_stormkeeperStacks > 0)
            _stormkeeperStacks--;
    }

    [[nodiscard]] bool HasStack() const
    {
        return _stormkeeperStacks > 0 && getMSTime() < _stormkeeperEndTime;
    }

    [[nodiscard]] uint32 GetStacks() const { return _stormkeeperStacks; }

    void Update(Player* bot)
    {
        if (!bot)
            return;

        // Check if Stormkeeper buff is active
        if (Aura* aura = bot->GetAura(ELEM_STORMKEEPER))
        {
            _stormkeeperStacks = aura->GetStackAmount();
            _stormkeeperEndTime = getMSTime() + aura->GetDuration();
        }
        else
        {
            _stormkeeperStacks = 0;
        }

        // Expire if time is up
        if (_stormkeeperStacks > 0 && getMSTime() >= _stormkeeperEndTime)
            _stormkeeperStacks = 0;
    }

private:
    uint32 _stormkeeperStacks;
    uint32 _stormkeeperEndTime;
};

class ElementalShamanRefactored : public RangedDpsSpecialization<ManaResource>, public ShamanSpecialization
{
public:
    // Use base class members with type alias for cleaner syntax
    using Base = RangedDpsSpecialization<ManaResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::_resource;
    explicit ElementalShamanRefactored(Player* bot)
        : RangedDpsSpecialization<ManaResource>(bot)
        , ShamanSpecialization(bot)
        , _maelstromTracker()
        , _flameShockTracker()
        , _lavaSurgeTracker()
        , _stormkeeperTracker()
        , _ascendanceActive(false)
        , _ascendanceEndTime(0)
        , _lastAscendanceTime(0)
        , _lastFireElementalTime(0)
        , _lastStormkeeperTime(0)
        , _lastEchoingShockTime(0)
        , _lastPrimordialWaveTime(0)
    {
        // Resource initialization handled by base class CombatSpecializationTemplate
        InitializeCooldowns();
        TC_LOG_DEBUG("playerbot", "ElementalShamanRefactored initialized for {}", bot->GetName());
    }

    void UpdateRotation(::Unit* target) override
    {
        Player* bot = this->GetBot();
        if (!target || !bot)
            return;

        UpdateElementalState();

        uint32 enemyCount = this->GetEnemiesInRange(40.0f);

        if (enemyCount >= 3)
            ExecuteAoERotation(target, enemyCount);
        else
            ExecuteSingleTargetRotation(target);
    }

    void UpdateBuffs() override
    {
        if (!bot)
            return;

        // Earth Shield (self-protection)
        if (!bot->HasAura(ELEM_EARTH_SHIELD))
        {
            if (this->CanCastSpell(ELEM_EARTH_SHIELD, bot))
            {
                this->CastSpell(bot, ELEM_EARTH_SHIELD);
            }
        }
    }

    void UpdateDefensives()
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        float healthPct = bot->GetHealthPct();

        // Astral Shift (damage reduction)
        if (healthPct < 40.0f && this->CanCastSpell(ELEM_ASTRAL_SHIFT, bot))
        {
            this->CastSpell(bot, ELEM_ASTRAL_SHIFT);
            return;
        }

        // Capacitor Totem (AoE stun for escape)
        if (healthPct < 50.0f && bot->GetThreatManager().GetThreatListSize() >= 2)
        {
            if (this->CanCastSpell(ELEM_CAPACITOR_TOTEM, bot))
            {
                this->CastSpell(bot, ELEM_CAPACITOR_TOTEM);
                return;
            }
        }
    }

private:
    void InitializeCooldowns()
    {
        _lastAscendanceTime = 0;
        _lastFireElementalTime = 0;
        _lastStormkeeperTime = 0;
        _lastEchoingShockTime = 0;
        _lastPrimordialWaveTime = 0;
    }

    void UpdateElementalState()
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        // ManaResource (uint32) doesn't have Update method - handled by base class
        _maelstromTracker.Update(bot);
        _flameShockTracker.Update(bot);
        _lavaSurgeTracker.Update(bot);
        _stormkeeperTracker.Update(bot);
        UpdateCooldownStates();
    }

    void UpdateCooldownStates()
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        // Ascendance state
        if (_ascendanceActive && getMSTime() >= _ascendanceEndTime)
            _ascendanceActive = false;

        if (bot->HasAura(ELEM_ASCENDANCE))
        {
            _ascendanceActive = true;
            if (Aura* aura = bot->GetAura(ELEM_ASCENDANCE))
                _ascendanceEndTime = getMSTime() + aura->GetDuration();
        }
    }

    void ExecuteSingleTargetRotation(::Unit* target)
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        uint32 maelstrom = _maelstromTracker.GetCurrent();

        // Fire Elemental (major DPS cooldown)
        if ((getMSTime() - _lastFireElementalTime) >= 150000) // 2.5 min CD
        {
            if (this->CanCastSpell(ELEM_FIRE_ELEMENTAL, bot))
            {
                this->CastSpell(bot, ELEM_FIRE_ELEMENTAL);
                _lastFireElementalTime = getMSTime();
                return;
            }
        }

        // Ascendance (burst mode)
        if (maelstrom >= 60 && (getMSTime() - _lastAscendanceTime) >= 180000) // 3 min CD
        {
            if (bot->HasSpell(ELEM_ASCENDANCE))
            {
                if (this->CanCastSpell(ELEM_ASCENDANCE, bot))
                {
                    this->CastSpell(bot, ELEM_ASCENDANCE);
                    _ascendanceActive = true;
                    _ascendanceEndTime = getMSTime() + 15000;
                    _lastAscendanceTime = getMSTime();
                    return;
                }
            }
        }

        // Stormkeeper (instant Lightning Bolts)
        if ((getMSTime() - _lastStormkeeperTime) >= 60000) // 60 sec CD
        {
            if (this->CanCastSpell(ELEM_STORMKEEPER, bot))
            {
                this->CastSpell(bot, ELEM_STORMKEEPER);
                _stormkeeperTracker.ActivateProc(2);
                _lastStormkeeperTime = getMSTime();
                return;
            }
        }

        // Primordial Wave (buff + Flame Shock application)
        if ((getMSTime() - _lastPrimordialWaveTime) >= 45000) // 45 sec CD
        {
            if (bot->HasSpell(ELEM_PRIMORDIAL_WAVE))
            {
                if (this->CanCastSpell(ELEM_PRIMORDIAL_WAVE, target))
                {
                    this->CastSpell(target, ELEM_PRIMORDIAL_WAVE);
                    _flameShockTracker.ApplyFlameShock(target->GetGUID(), 18000);
                    _lastPrimordialWaveTime = getMSTime();
                    return;
                }
            }
        }

        // Flame Shock (maintain DoT)
        if (!_flameShockTracker.HasFlameShock(target->GetGUID()) ||
            _flameShockTracker.NeedsFlameShockRefresh(target->GetGUID()))
        {
            if (this->CanCastSpell(ELEM_FLAME_SHOCK, target))
            {
                this->CastSpell(target, ELEM_FLAME_SHOCK);
                _flameShockTracker.ApplyFlameShock(target->GetGUID(), 18000);
                _maelstromTracker.Generate(5);
                return;
            }
        }

        // Lava Burst (with Lava Surge proc for instant cast)
        if (_lavaSurgeTracker.IsActive())
        {
            if (this->CanCastSpell(ELEM_LAVA_BURST, target))
            {
                this->CastSpell(target, ELEM_LAVA_BURST);
                _lavaSurgeTracker.ConsumeProc();
                _maelstromTracker.Generate(10);
                return;
            }
        }

        // Lava Burst (normal cast on Flame Shock target)
        if (_flameShockTracker.HasFlameShock(target->GetGUID()))
        {
            if (this->CanCastSpell(ELEM_LAVA_BURST, target))
            {
                this->CastSpell(target, ELEM_LAVA_BURST);
                _maelstromTracker.Generate(10);

                // Chance to proc Lava Surge
                if (rand() % 100 < 15) // 15% chance (simplified)
                    _lavaSurgeTracker.ActivateProc();

                return;
            }
        }

        // Earth Shock (Maelstrom spender - high damage)
        if (maelstrom >= 60)
        {
            if (this->CanCastSpell(ELEM_EARTH_SHOCK, target))
            {
                this->CastSpell(target, ELEM_EARTH_SHOCK);
                _maelstromTracker.Spend(60);
                return;
            }
        }

        // Elemental Blast (talent - generates Maelstrom + random buff)
        if (bot->HasSpell(ELEM_ELEMENTAL_BLAST))
        {
            if (this->CanCastSpell(ELEM_ELEMENTAL_BLAST, target))
            {
                this->CastSpell(target, ELEM_ELEMENTAL_BLAST);
                _maelstromTracker.Generate(12);
                return;
            }
        }

        // Icefury (talent - empowers Frost Shock)
        if (bot->HasSpell(ELEM_ICEFURY))
        {
            if (this->CanCastSpell(ELEM_ICEFURY, target))
            {
                this->CastSpell(target, ELEM_ICEFURY);
                _maelstromTracker.Generate(15);
                return;
            }
        }

        // Lightning Bolt with Stormkeeper proc (instant cast)
        if (_stormkeeperTracker.HasStack())
        {
            if (this->CanCastSpell(ELEM_LIGHTNING_BOLT, target))
            {
                this->CastSpell(target, ELEM_LIGHTNING_BOLT);
                _stormkeeperTracker.ConsumeStack();
                _maelstromTracker.Generate(8);
                return;
            }
        }

        // Echoing Shock (duplicates next spell)
        if ((getMSTime() - _lastEchoingShockTime) >= 30000) // 30 sec CD
        {
            if (bot->HasSpell(ELEM_ECHOING_SHOCK))
            {
                if (this->CanCastSpell(ELEM_ECHOING_SHOCK, bot))
                {
                    this->CastSpell(bot, ELEM_ECHOING_SHOCK);
                    _lastEchoingShockTime = getMSTime();
                    return;
                }
            }
        }

        // Lightning Bolt (builder)
        if (this->CanCastSpell(ELEM_LIGHTNING_BOLT, target))
        {
            this->CastSpell(target, ELEM_LIGHTNING_BOLT);
            _maelstromTracker.Generate(8);
            return;
        }
    }

    void ExecuteAoERotation(::Unit* target, uint32 enemyCount)
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        uint32 maelstrom = _maelstromTracker.GetCurrent();

        // Fire Elemental for AoE burst
        if ((getMSTime() - _lastFireElementalTime) >= 150000 && enemyCount >= 4)
        {
            if (this->CanCastSpell(ELEM_FIRE_ELEMENTAL, bot))
            {
                this->CastSpell(bot, ELEM_FIRE_ELEMENTAL);
                _lastFireElementalTime = getMSTime();
                return;
            }
        }

        // Ascendance for AoE burst
        if (maelstrom >= 60 && (getMSTime() - _lastAscendanceTime) >= 180000 && enemyCount >= 5)
        {
            if (bot->HasSpell(ELEM_ASCENDANCE))
            {
                if (this->CanCastSpell(ELEM_ASCENDANCE, bot))
                {
                    this->CastSpell(bot, ELEM_ASCENDANCE);
                    _ascendanceActive = true;
                    _ascendanceEndTime = getMSTime() + 15000;
                    _lastAscendanceTime = getMSTime();
                    return;
                }
            }
        }

        // Liquid Magma Totem (AoE DoT)
        if (bot->HasSpell(ELEM_LIQUID_MAGMA_TOTEM) && enemyCount >= 3)
        {
            if (this->CanCastSpell(ELEM_LIQUID_MAGMA_TOTEM, bot))
            {
                this->CastSpell(bot, ELEM_LIQUID_MAGMA_TOTEM);
                return;
            }
        }

        // Stormkeeper for AoE (empowered Chain Lightning)
        if ((getMSTime() - _lastStormkeeperTime) >= 60000 && enemyCount >= 3)
        {
            if (this->CanCastSpell(ELEM_STORMKEEPER, bot))
            {
                this->CastSpell(bot, ELEM_STORMKEEPER);
                _stormkeeperTracker.ActivateProc(2);
                _lastStormkeeperTime = getMSTime();
                return;
            }
        }

        // Flame Shock on multiple targets (up to 3)
        if (enemyCount <= 3 && _flameShockTracker.GetActiveFlameShockCount() < 3)
        {
            if (!_flameShockTracker.HasFlameShock(target->GetGUID()))
            {
                if (this->CanCastSpell(ELEM_FLAME_SHOCK, target))
                {
                    this->CastSpell(target, ELEM_FLAME_SHOCK);
                    _flameShockTracker.ApplyFlameShock(target->GetGUID(), 18000);
                    _maelstromTracker.Generate(5);
                    return;
                }
            }
        }

        // Earthquake (AoE Maelstrom spender)
        if (maelstrom >= 60 && enemyCount >= 3)
        {
            if (this->CanCastSpell(ELEM_EARTHQUAKE, target))
            {
                this->CastSpell(target, ELEM_EARTHQUAKE);
                _maelstromTracker.Spend(60);
                return;
            }
        }

        // Chain Lightning with Stormkeeper proc
        if (_stormkeeperTracker.HasStack() && enemyCount >= 2)
        {
            if (this->CanCastSpell(ELEM_CHAIN_LIGHTNING, target))
            {
                this->CastSpell(target, ELEM_CHAIN_LIGHTNING);
                _stormkeeperTracker.ConsumeStack();
                _maelstromTracker.Generate(4 * std::min(enemyCount, 5u)); // 4 per target hit
                return;
            }
        }

        // Chain Lightning (AoE builder)
        if (enemyCount >= 2)
        {
            if (this->CanCastSpell(ELEM_CHAIN_LIGHTNING, target))
            {
                this->CastSpell(target, ELEM_CHAIN_LIGHTNING);
                _maelstromTracker.Generate(4 * std::min(enemyCount, 5u));
                return;
            }
        }

        // Lightning Bolt (single-target filler)
        if (this->CanCastSpell(ELEM_LIGHTNING_BOLT, target))
        {
            this->CastSpell(target, ELEM_LIGHTNING_BOLT);
            _maelstromTracker.Generate(8);
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

    // Member variables
    MaelstromTracker _maelstromTracker;
    FlameShockTracker _flameShockTracker;
    LavaSurgeTracker _lavaSurgeTracker;
    StormkeeperTracker _stormkeeperTracker;

    bool _ascendanceActive;
    uint32 _ascendanceEndTime;

    uint32 _lastAscendanceTime;
    uint32 _lastFireElementalTime;
    uint32 _lastStormkeeperTime;
    uint32 _lastEchoingShockTime;
    uint32 _lastPrimordialWaveTime;
};

} // namespace Playerbot

#endif // PLAYERBOT_ELEMENTALSHAMANREFACTORED_H
