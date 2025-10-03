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

#ifndef PLAYERBOT_DEVASTATIONEVOKERREFACTORED_H
#define PLAYERBOT_DEVASTATIONEVOKERREFACTORED_H

#include "../CombatSpecializationTemplates.h"
#include "Player.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include <unordered_map>
#include "Log.h"
#include "EvokerSpecialization.h"

namespace Playerbot
{

// WoW 11.2 (The War Within) - Devastation Evoker Spell IDs
constexpr uint32 DEV_AZURE_STRIKE = 362969;
constexpr uint32 DEV_LIVING_FLAME = 361469;
constexpr uint32 DEV_PYRE = 357208;
constexpr uint32 DEV_DISINTEGRATE = 356995;
constexpr uint32 DEV_ETERNITY_SURGE = 359073;
constexpr uint32 DEV_FIRE_BREATH = 357208;
constexpr uint32 DEV_SHATTERING_STAR = 370452;
constexpr uint32 DEV_DRAGONRAGE = 375087;
constexpr uint32 DEV_TIP_THE_SCALES = 370553;
constexpr uint32 DEV_ESSENCE_BURST = 359618; // Buff proc
constexpr uint32 DEV_IRIDESCENCE_BLUE = 386399; // Buff
constexpr uint32 DEV_IRIDESCENCE_RED = 386353; // Buff
constexpr uint32 DEV_HOVER = 358267;
constexpr uint32 DEV_DEEP_BREATH = 357210;
constexpr uint32 DEV_OBSIDIAN_SCALES = 363916;
constexpr uint32 DEV_RENEWING_BLAZE = 374348;
constexpr uint32 DEV_VERDANT_EMBRACE = 360995;

// Mana and Essence resource (Evokers use dual resource)
struct ManaEssenceResource
{
    uint32 mana = 0;
    uint32 maxMana = 100;
    uint32 essence = 0;
    uint32 maxEssence = 6;

    void Initialize(Player* bot)
    {
        if (!bot)
            return;

        mana = bot->GetPower(POWER_MANA);
        maxMana = bot->GetMaxPower(POWER_MANA);
        essence = bot->GetPower(POWER_ESSENCE);
        maxEssence = bot->GetMaxPower(POWER_ESSENCE);
    }

    void Update(Player* bot)
    {
        if (!bot)
            return;

        mana = bot->GetPower(POWER_MANA);
        essence = bot->GetPower(POWER_ESSENCE);
    }

    [[nodiscard]] bool HasMana(uint32 amount) const { return mana >= amount; }
    [[nodiscard]] bool HasEssence(uint32 amount) const { return essence >= amount; }
    [[nodiscard]] uint32 GetManaPercent() const { return maxMana > 0 ? (mana * 100) / maxMana : 0; }
};

// Essence Burst proc tracker (free Disintegrate)
class DevastationEssenceBurstTracker
{
public:
    DevastationEssenceBurstTracker() : _essenceBurstStacks(0), _essenceBurstEndTime(0) {}

    void ActivateProc(uint32 stacks = 1)
    {
        _essenceBurstStacks = std::min(_essenceBurstStacks + stacks, 2u); // Max 2 stacks
        _essenceBurstEndTime = getMSTime() + 15000; // 15 sec duration
    }

    void ConsumeProc()
    {
        if (_essenceBurstStacks > 0)
            _essenceBurstStacks--;
    }

    [[nodiscard]] bool IsActive() const
    {
        return _essenceBurstStacks > 0 && getMSTime() < _essenceBurstEndTime;
    }

    [[nodiscard]] uint32 GetStacks() const { return _essenceBurstStacks; }

    void Update(Player* bot)
    {
        if (!bot)
            return;

        if (Aura* aura = bot->GetAura(DEV_ESSENCE_BURST))
        {
            _essenceBurstStacks = aura->GetStackAmount();
            _essenceBurstEndTime = getMSTime() + aura->GetDuration();
        }
        else
        {
            _essenceBurstStacks = 0;
            _essenceBurstEndTime = 0;
        }
    }

private:
    uint32 _essenceBurstStacks;
    uint32 _essenceBurstEndTime;
};

// Iridescence buff tracker (alternating blue/red magic)
class DevastationIridescenceTracker
{
public:
    enum IridescenceType
    {
        NONE,
        BLUE,   // Azure/Arcane magic
        RED     // Red/Fire magic
    };

    DevastationIridescenceTracker() : _currentType(NONE), _iridescenceEndTime(0) {}

    void ActivateBlue()
    {
        _currentType = BLUE;
        _iridescenceEndTime = getMSTime() + 10000; // 10 sec
    }

    void ActivateRed()
    {
        _currentType = RED;
        _iridescenceEndTime = getMSTime() + 10000; // 10 sec
    }

    [[nodiscard]] bool IsBlueActive() const
    {
        return _currentType == BLUE && getMSTime() < _iridescenceEndTime;
    }

    [[nodiscard]] bool IsRedActive() const
    {
        return _currentType == RED && getMSTime() < _iridescenceEndTime;
    }

    void Update(Player* bot)
    {
        if (!bot)
            return;

        if (bot->HasAura(DEV_IRIDESCENCE_BLUE))
        {
            _currentType = BLUE;
            if (Aura* aura = bot->GetAura(DEV_IRIDESCENCE_BLUE))
                _iridescenceEndTime = getMSTime() + aura->GetDuration();
        }
        else if (bot->HasAura(DEV_IRIDESCENCE_RED))
        {
            _currentType = RED;
            if (Aura* aura = bot->GetAura(DEV_IRIDESCENCE_RED))
                _iridescenceEndTime = getMSTime() + aura->GetDuration();
        }
        else if (getMSTime() >= _iridescenceEndTime)
        {
            _currentType = NONE;
        }
    }

private:
    IridescenceType _currentType;
    uint32 _iridescenceEndTime;
};

class DevastationEvokerRefactored : public RangedDpsSpecialization<ManaEssenceResource>, public EvokerSpecialization
{
public:
    using Base = RangedDpsSpecialization<ManaEssenceResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::_resource;
    explicit DevastationEvokerRefactored(Player* bot)
        : RangedDpsSpecialization<ManaEssenceResource>(bot)
        , EvokerSpecialization(bot)
        , _essenceBurstTracker()
        , _iridescenceTracker()
        , _dragonrageActive(false)
        , _dragonrageEndTime(0)
        , _lastDragonrageTime(0)
        , _lastFireBreathTime(0)
        , _lastEternitySurgeTime(0)
    {
        this->_resource.Initialize(bot);
        InitializeCooldowns();
        TC_LOG_DEBUG("playerbot", "DevastationEvokerRefactored initialized for {}", bot->GetName());
    }

    void UpdateRotation(::Unit* target) override
    {
        if (!target || !bot)
            return;

        UpdateDevastationState();

        uint32 enemyCount = this->GetEnemiesInRange(25.0f);

        if (enemyCount >= 3)
            ExecuteAoERotation(target, enemyCount);
        else
            ExecuteSingleTargetRotation(target);
    }

    void UpdateBuffs() override
    {
        if (!bot)
            return;

        // Hover for mobility (optional)
        if (bot->IsInCombat() && NeedsMobility())
        {
            if (this->CanCastSpell(DEV_HOVER, bot))
            {
                this->CastSpell(bot, DEV_HOVER);
            }
        }
    }

    void UpdateDefensives() override
    {
        if (!bot)
            return;

        float healthPct = bot->GetHealthPct();

        // Obsidian Scales (major defensive - 30% DR)
        if (healthPct < 50.0f && this->CanCastSpell(DEV_OBSIDIAN_SCALES, bot))
        {
            this->CastSpell(bot, DEV_OBSIDIAN_SCALES);
            return;
        }

        // Renewing Blaze (self-heal)
        if (healthPct < 40.0f && this->CanCastSpell(DEV_RENEWING_BLAZE, bot))
        {
            this->CastSpell(bot, DEV_RENEWING_BLAZE);
            return;
        }

        // Verdant Embrace (emergency self-heal)
        if (healthPct < 30.0f && this->CanCastSpell(DEV_VERDANT_EMBRACE, bot))
        {
            this->CastSpell(bot, DEV_VERDANT_EMBRACE);
            return;
        }
    }

private:
    void InitializeCooldowns()
    {
        _lastDragonrageTime = 0;
        _lastFireBreathTime = 0;
        _lastEternitySurgeTime = 0;
    }

    void UpdateDevastationState()
    {
        this->_resource.Update(bot);
        _essenceBurstTracker.Update(bot);
        _iridescenceTracker.Update(bot);
        UpdateCooldownStates();
    }

    void UpdateCooldownStates()
    {
        // Dragonrage state
        if (_dragonrageActive && getMSTime() >= _dragonrageEndTime)
            _dragonrageActive = false;

        if (bot->HasAura(DEV_DRAGONRAGE))
        {
            _dragonrageActive = true;
            if (Aura* aura = bot->GetAura(DEV_DRAGONRAGE))
                _dragonrageEndTime = getMSTime() + aura->GetDuration();
        }
    }

    void ExecuteSingleTargetRotation(::Unit* target)
    {
        uint32 essence = this->_resource.essence;

        // Dragonrage (major DPS cooldown)
        if (essence >= 3 && !_dragonrageActive)
        {
            if (this->CanCastSpell(DEV_DRAGONRAGE, bot))
            {
                this->CastSpell(bot, DEV_DRAGONRAGE);
                _dragonrageActive = true;
                _dragonrageEndTime = getMSTime() + 18000; // 18 sec
                _lastDragonrageTime = getMSTime();
                return;
            }
        }

        // Shattering Star (amplify damage taken)
        if (this->CanCastSpell(DEV_SHATTERING_STAR, target))
        {
            this->CastSpell(target, DEV_SHATTERING_STAR);
            return;
        }

        // Fire Breath (powerful DoT and damage)
        if (essence >= 3 && (getMSTime() - _lastFireBreathTime) >= 30000) // 30 sec CD
        {
            if (this->CanCastSpell(DEV_FIRE_BREATH, target))
            {
                this->CastSpell(target, DEV_FIRE_BREATH);
                _lastFireBreathTime = getMSTime();
                ConsumeEssence(3);
                _iridescenceTracker.ActivateRed(); // Gain red buff
                return;
            }
        }

        // Eternity Surge (high damage AoE cone)
        if (essence >= 3 && (getMSTime() - _lastEternitySurgeTime) >= 30000) // 30 sec CD
        {
            if (this->CanCastSpell(DEV_ETERNITY_SURGE, target))
            {
                this->CastSpell(target, DEV_ETERNITY_SURGE);
                _lastEternitySurgeTime = getMSTime();
                ConsumeEssence(3);
                _iridescenceTracker.ActivateBlue(); // Gain blue buff
                return;
            }
        }

        // Disintegrate (with Essence Burst proc for free cast)
        if (_essenceBurstTracker.IsActive())
        {
            if (this->CanCastSpell(DEV_DISINTEGRATE, target))
            {
                this->CastSpell(target, DEV_DISINTEGRATE);
                _essenceBurstTracker.ConsumeProc();
                return;
            }
        }

        // Disintegrate (empowered by Iridescence)
        if (essence >= 3 && _iridescenceTracker.IsBlueActive())
        {
            if (this->CanCastSpell(DEV_DISINTEGRATE, target))
            {
                this->CastSpell(target, DEV_DISINTEGRATE);
                ConsumeEssence(3);
                return;
            }
        }

        // Pyre (empowered by red Iridescence - single target filler)
        if (essence >= 3 && _iridescenceTracker.IsRedActive())
        {
            if (this->CanCastSpell(DEV_PYRE, target))
            {
                this->CastSpell(target, DEV_PYRE);
                ConsumeEssence(3);
                return;
            }
        }

        // Living Flame (filler builder)
        if (this->CanCastSpell(DEV_LIVING_FLAME, target))
        {
            this->CastSpell(target, DEV_LIVING_FLAME);
            GenerateEssence(1);
            return;
        }

        // Azure Strike (melee range filler)
        if (bot->GetDistance(target) <= 8.0f)
        {
            if (this->CanCastSpell(DEV_AZURE_STRIKE, target))
            {
                this->CastSpell(target, DEV_AZURE_STRIKE);
                GenerateEssence(1);
                return;
            }
        }
    }

    void ExecuteAoERotation(::Unit* target, uint32 enemyCount)
    {
        uint32 essence = this->_resource.essence;

        // Dragonrage for AoE burst
        if (essence >= 3 && !_dragonrageActive && enemyCount >= 4)
        {
            if (this->CanCastSpell(DEV_DRAGONRAGE, bot))
            {
                this->CastSpell(bot, DEV_DRAGONRAGE);
                _dragonrageActive = true;
                _dragonrageEndTime = getMSTime() + 18000;
                _lastDragonrageTime = getMSTime();
                return;
            }
        }

        // Deep Breath (massive AoE damage - long CD)
        if (enemyCount >= 5 && this->CanCastSpell(DEV_DEEP_BREATH, target))
        {
            this->CastSpell(target, DEV_DEEP_BREATH);
            return;
        }

        // Fire Breath (AoE DoT)
        if (essence >= 3 && (getMSTime() - _lastFireBreathTime) >= 30000)
        {
            if (this->CanCastSpell(DEV_FIRE_BREATH, target))
            {
                this->CastSpell(target, DEV_FIRE_BREATH);
                _lastFireBreathTime = getMSTime();
                ConsumeEssence(3);
                _iridescenceTracker.ActivateRed();
                return;
            }
        }

        // Eternity Surge (cone AoE)
        if (essence >= 3 && (getMSTime() - _lastEternitySurgeTime) >= 30000)
        {
            if (this->CanCastSpell(DEV_ETERNITY_SURGE, target))
            {
                this->CastSpell(target, DEV_ETERNITY_SURGE);
                _lastEternitySurgeTime = getMSTime();
                ConsumeEssence(3);
                _iridescenceTracker.ActivateBlue();
                return;
            }
        }

        // Pyre (AoE spender - empowered by red)
        if (essence >= 3)
        {
            if (this->CanCastSpell(DEV_PYRE, target))
            {
                this->CastSpell(target, DEV_PYRE);
                ConsumeEssence(3);
                return;
            }
        }

        // Disintegrate with Essence Burst
        if (_essenceBurstTracker.IsActive())
        {
            if (this->CanCastSpell(DEV_DISINTEGRATE, target))
            {
                this->CastSpell(target, DEV_DISINTEGRATE);
                _essenceBurstTracker.ConsumeProc();
                return;
            }
        }

        // Living Flame (filler builder)
        if (this->CanCastSpell(DEV_LIVING_FLAME, target))
        {
            this->CastSpell(target, DEV_LIVING_FLAME);
            GenerateEssence(1);
            return;
        }
    }

    void GenerateEssence(uint32 amount)
    {
        this->_resource.essence = std::min(this->_resource.essence + amount, this->_resource.maxEssence);

        // Chance to proc Essence Burst
        if (rand() % 100 < 15) // 15% chance (simplified)
            _essenceBurstTracker.ActivateProc();
    }

    void ConsumeEssence(uint32 amount)
    {
        if (this->_resource.essence >= amount)
            this->_resource.essence -= amount;
        else
            this->_resource.essence = 0;
    }

    [[nodiscard]] bool NeedsMobility() const
    {
        // Simplified mobility check - in real implementation, check if bot needs to move
        return false;
    }

    [[nodiscard]] uint32 this->GetEnemiesInRange(float range) const
    {
        if (!bot)
            return 0;

        uint32 count = 0;
        // Simplified enemy counting - in real implementation, query nearby hostile units
        return std::min(count, 10u); // Cap at 10 for performance
    }

    // Member variables
    DevastationEssenceBurstTracker _essenceBurstTracker;
    DevastationIridescenceTracker _iridescenceTracker;

    bool _dragonrageActive;
    uint32 _dragonrageEndTime;

    uint32 _lastDragonrageTime;
    uint32 _lastFireBreathTime;
    uint32 _lastEternitySurgeTime;
};

} // namespace Playerbot

#endif // PLAYERBOT_DEVASTATIONEVOKERREFACTORED_H
