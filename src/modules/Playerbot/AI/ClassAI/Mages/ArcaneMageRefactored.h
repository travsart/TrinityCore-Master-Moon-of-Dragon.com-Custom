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

#ifndef PLAYERBOT_ARCANEMAGEREFACTORED_H
#define PLAYERBOT_ARCANEMAGEREFACTORED_H

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

// WoW 11.2 (The War Within) - Arcane Mage Spell IDs
constexpr uint32 ARCANE_BLAST = 30451;
constexpr uint32 ARCANE_MISSILES = 5143;
constexpr uint32 ARCANE_BARRAGE = 44425;
constexpr uint32 ARCANE_SURGE = 365350;
constexpr uint32 ARCANE_ORB = 153626;
constexpr uint32 EVOCATION = 12051;
constexpr uint32 TOUCH_OF_MAGE = 321507; // Arcane-specific talent
constexpr uint32 ARCANE_FAMILIAR = 205022;
constexpr uint32 PRESENCE_OF_MIND = 205025;
constexpr uint32 ARCANE_INTELLECT = 1459;
constexpr uint32 ARCANE_EXPLOSION = 1449;
constexpr uint32 SUPERNOVA = 157980;
constexpr uint32 SHIFTING_POWER = 382440;
constexpr uint32 ICE_BLOCK = 45438;
constexpr uint32 MIRROR_IMAGE = 55342;
constexpr uint32 TIME_WARP = 80353;

// Arcane Charge tracker (stacks 1-4)
class ArcaneChargeTracker
{
public:
    ArcaneChargeTracker() : _charges(0), _maxCharges(4) {}

    void AddCharge(uint32 amount = 1)
    {
        _charges = std::min(_charges + amount, _maxCharges);
    }

    void ClearCharges()
    {
        _charges = 0;
    }

    [[nodiscard]] uint32 GetCharges() const { return _charges; }
    [[nodiscard]] bool IsMaxCharges() const { return _charges >= _maxCharges; }

    void Update(Player* bot)
    {
        if (!bot)
            return;

        // Sync with actual Arcane Charges buff
        if (Aura* aura = bot->GetAura(36032)) // Arcane Charges buff ID
            _charges = aura->GetStackAmount();
        else
            _charges = 0;
    }

private:
    uint32 _charges;
    uint32 _maxCharges;
};

// Clearcasting proc tracker (free Arcane Missiles)
class ClearcastingTracker
{
public:
    ClearcastingTracker() : _clearcastingActive(false), _clearcastingStacks(0), _clearcastingEndTime(0) {}

    void ActivateProc(uint32 stacks = 1)
    {
        _clearcastingActive = true;
        _clearcastingStacks = std::min(_clearcastingStacks + stacks, 3u); // Max 3 stacks
        _clearcastingEndTime = getMSTime() + 15000; // 15 sec duration
    }

    void ConsumeProc()
    {
        if (_clearcastingStacks > 0)
            _clearcastingStacks--;

        if (_clearcastingStacks == 0)
            _clearcastingActive = false;
    }

    [[nodiscard]] bool IsActive() const
    {
        return _clearcastingActive && getMSTime() < _clearcastingEndTime;
    }

    [[nodiscard]] uint32 GetStacks() const { return _clearcastingStacks; }

    void Update(Player* bot)
    {
        if (!bot)
            return;

        if (Aura* aura = bot->GetAura(263725)) // Clearcasting buff ID
        {
            _clearcastingActive = true;
            _clearcastingStacks = aura->GetStackAmount();
            _clearcastingEndTime = getMSTime() + aura->GetDuration();
        }
        else
        {
            _clearcastingActive = false;
            _clearcastingStacks = 0;
        }
    }

private:
    bool _clearcastingActive;
    uint32 _clearcastingStacks;
    uint32 _clearcastingEndTime;
};

class ArcaneMageRefactored : public RangedDpsSpecialization<ManaResource>, public MageSpecialization
{
public:
    using Base = RangedDpsSpecialization<ManaResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::IsSpellReady;
    using Base::GetEnemiesInRange;
    using Base::_resource;

    explicit ArcaneMageRefactored(Player* bot)
        : RangedDpsSpecialization<ManaResource>(bot)
        , MageSpecialization(bot)
        , _chargeTracker()
        , _clearcastingTracker()
        , _arcaneSurgeActive(false)
        , _arcaneSurgeEndTime(0)
        , _lastArcaneSurgeTime(0)
        , _lastEvocationTime(0)
        , _lastPresenceOfMindTime(0)
    {
        InitializeCooldowns();
        TC_LOG_DEBUG("playerbot", "ArcaneMageRefactored initialized for {}", bot->GetName());
    }

    void UpdateRotation(::Unit* target) override
    {
        if (!target || !this->GetBot())
            return;

        UpdateArcaneState();

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

        // Arcane Intellect buff
        if (!bot->HasAura(ARCANE_INTELLECT))
        {
            if (this->CanCastSpell(ARCANE_INTELLECT, bot))
            {
                this->CastSpell(bot, ARCANE_INTELLECT);
            }
        }

        // Arcane Familiar (if talented)
        if (bot->HasSpell(ARCANE_FAMILIAR) && !bot->HasAura(ARCANE_FAMILIAR))
        {
            if (this->CanCastSpell(ARCANE_FAMILIAR, bot))
            {
                this->CastSpell(bot, ARCANE_FAMILIAR);
            }
        }
    }

    void UpdateDefensives()
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        float healthPct = bot->GetHealthPct();

        // Ice Block (critical emergency - immune)
        if (healthPct < 20.0f && this->CanCastSpell(ICE_BLOCK, bot))
        {
            this->CastSpell(bot, ICE_BLOCK);
            return;
        }

        // Mirror Image (defensive decoy)
        if (healthPct < 40.0f && this->CanCastSpell(MIRROR_IMAGE, bot))
        {
            this->CastSpell(bot, MIRROR_IMAGE);
            return;
        }

        // Shifting Power (reset cooldowns in emergency) - self-cast
        if (healthPct < 50.0f && this->CanCastSpell(SHIFTING_POWER, bot))
        {
            this->CastSpell(bot, SHIFTING_POWER);
            return;
        }
    }

private:
    void InitializeCooldowns()
    {
        _lastArcaneSurgeTime = 0;
        _lastEvocationTime = 0;
        _lastPresenceOfMindTime = 0;
    }

    void UpdateArcaneState()
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        // Resource is managed by base template class
        _chargeTracker.Update(bot);
        _clearcastingTracker.Update(bot);
        UpdateCooldownStates();
    }

    void UpdateCooldownStates()
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        // Arcane Surge state
        if (_arcaneSurgeActive && getMSTime() >= _arcaneSurgeEndTime)
            _arcaneSurgeActive = false;

        if (bot->HasAura(ARCANE_SURGE))
        {
            _arcaneSurgeActive = true;
            if (Aura* aura = bot->GetAura(ARCANE_SURGE))
                _arcaneSurgeEndTime = getMSTime() + aura->GetDuration();
        }
    }

    void ExecuteSingleTargetRotation(::Unit* target)
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        uint32 charges = _chargeTracker.GetCharges();
        uint32 manaPercent = bot->GetPowerPct(POWER_MANA);

        // Arcane Surge (major DPS cooldown at 4 charges)
        if (charges >= 4 && manaPercent >= 70 && !_arcaneSurgeActive)
        {
            if (this->CanCastSpell(ARCANE_SURGE, bot))
            {
                this->CastSpell(bot, ARCANE_SURGE);
                _arcaneSurgeActive = true;
                _arcaneSurgeEndTime = getMSTime() + 15000; // 15 sec
                _lastArcaneSurgeTime = getMSTime();
                return;
            }
        }

        // Touch of the Magi (apply damage amplification debuff at 4 charges)
        if (charges >= 4 && bot->HasSpell(TOUCH_OF_MAGE))
        {
            if (this->CanCastSpell(TOUCH_OF_MAGE, target))
            {
                this->CastSpell(target, TOUCH_OF_MAGE);
                return;
            }
        }

        // Arcane Missiles with Clearcasting proc (free cast, no mana cost)
        if (_clearcastingTracker.IsActive())
        {
            if (this->CanCastSpell(ARCANE_MISSILES, target))
            {
                this->CastSpell(target, ARCANE_MISSILES);
                _clearcastingTracker.ConsumeProc();
                return;
            }
        }

        // Arcane Barrage (spend charges when at max or low mana)
        if (charges >= 4 || (charges >= 2 && manaPercent < 30))
        {
            if (this->CanCastSpell(ARCANE_BARRAGE, target))
            {
                this->CastSpell(target, ARCANE_BARRAGE);
                _chargeTracker.ClearCharges();
                return;
            }
        }

        // Presence of Mind (instant cast Arcane Blast)
        if (charges < 4 && this->CanCastSpell(PRESENCE_OF_MIND, bot))
        {
            this->CastSpell(bot, PRESENCE_OF_MIND);
            _lastPresenceOfMindTime = getMSTime();
            // Follow up with instant Arcane Blast
            if (this->CanCastSpell(ARCANE_BLAST, target))
            {
                this->CastSpell(target, ARCANE_BLAST);
                _chargeTracker.AddCharge();
                return;
            }
        }

        // Arcane Blast (builder - generates charges)
        if (manaPercent > 20 || charges < 4)
        {
            if (this->CanCastSpell(ARCANE_BLAST, target))
            {
                this->CastSpell(target, ARCANE_BLAST);
                _chargeTracker.AddCharge();

                // Chance to proc Clearcasting
                if (rand() % 100 < 10) // 10% chance (simplified)
                    _clearcastingTracker.ActivateProc();

                return;
            }
        }

        // Evocation (emergency mana regen)
        if (manaPercent < 20 && (getMSTime() - _lastEvocationTime) >= 90000) // 90 sec CD
        {
            if (this->CanCastSpell(EVOCATION, bot))
            {
                this->CastSpell(bot, EVOCATION);
                _lastEvocationTime = getMSTime();
                return;
            }
        }
    }

    void ExecuteAoERotation(::Unit* target, uint32 enemyCount)
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        uint32 charges = _chargeTracker.GetCharges();
        uint32 manaPercent = bot->GetPowerPct(POWER_MANA);

        // Arcane Surge for burst AoE
        if (charges >= 4 && manaPercent >= 70 && !_arcaneSurgeActive && enemyCount >= 4)
        {
            if (this->CanCastSpell(ARCANE_SURGE, bot))
            {
                this->CastSpell(bot, ARCANE_SURGE);
                _arcaneSurgeActive = true;
                _arcaneSurgeEndTime = getMSTime() + 15000;
                _lastArcaneSurgeTime = getMSTime();
                return;
            }
        }

        // Arcane Orb (AoE builder)
        if (bot->HasSpell(ARCANE_ORB) && charges < 4)
        {
            if (this->CanCastSpell(ARCANE_ORB, target))
            {
                this->CastSpell(target, ARCANE_ORB);
                _chargeTracker.AddCharge(1);
                return;
            }
        }

        // Supernova (AoE damage and knockback)
        if (bot->HasSpell(SUPERNOVA) && enemyCount >= 3)
        {
            if (this->CanCastSpell(SUPERNOVA, target))
            {
                this->CastSpell(target, SUPERNOVA);
                return;
            }
        }

        // Arcane Barrage (AoE spender at max charges)
        if (charges >= 4)
        {
            if (this->CanCastSpell(ARCANE_BARRAGE, target))
            {
                this->CastSpell(target, ARCANE_BARRAGE);
                _chargeTracker.ClearCharges();
                return;
            }
        }

        // Arcane Missiles with Clearcasting
        if (_clearcastingTracker.IsActive())
        {
            if (this->CanCastSpell(ARCANE_MISSILES, target))
            {
                this->CastSpell(target, ARCANE_MISSILES);
                _clearcastingTracker.ConsumeProc();
                return;
            }
        }

        // Arcane Explosion (close-range AoE if enemies nearby)
        if (enemyCount >= 3 && GetNearbyEnemies(10.0f) >= 3)
        {
            if (this->CanCastSpell(ARCANE_EXPLOSION, bot))
            {
                this->CastSpell(bot, ARCANE_EXPLOSION);
                return;
            }
        }

        // Arcane Blast (builder)
        if (manaPercent > 20 || charges < 4)
        {
            if (this->CanCastSpell(ARCANE_BLAST, target))
            {
                this->CastSpell(target, ARCANE_BLAST);
                _chargeTracker.AddCharge();

                // Chance to proc Clearcasting
                if (rand() % 100 < 10) // 10% chance
                    _clearcastingTracker.ActivateProc();

                return;
            }
        }

        // Evocation for mana regen
        if (manaPercent < 20 && (getMSTime() - _lastEvocationTime) >= 90000)
        {
            if (this->CanCastSpell(EVOCATION, bot))
            {
                this->CastSpell(bot, EVOCATION);
                _lastEvocationTime = getMSTime();
                return;
            }
        }
    }

    [[nodiscard]] uint32 GetNearbyEnemies(float range) const
    {
        // Get enemies within melee/close range for Arcane Explosion
        return this->GetEnemiesInRange(range);
    }

    // Member variables
    ArcaneChargeTracker _chargeTracker;
    ClearcastingTracker _clearcastingTracker;

    bool _arcaneSurgeActive;
    uint32 _arcaneSurgeEndTime;

    uint32 _lastArcaneSurgeTime;
    uint32 _lastEvocationTime;
    uint32 _lastPresenceOfMindTime;
};

} // namespace Playerbot

#endif // PLAYERBOT_ARCANEMAGEREFACTORED_H
