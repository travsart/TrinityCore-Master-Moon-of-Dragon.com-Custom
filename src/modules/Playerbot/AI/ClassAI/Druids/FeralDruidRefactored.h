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

#ifndef PLAYERBOT_FERALDRDUIDREFACTORED_H
#define PLAYERBOT_FERALDRDUIDREFACTORED_H

#include "../CombatSpecializationTemplates.h"
#include "Player.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include <unordered_map>
#include "Log.h"
#include "DruidSpecialization.h"

namespace Playerbot
{

// WoW 11.2 (The War Within) - Feral Druid Spell IDs
constexpr uint32 FERAL_SHRED = 5221;
constexpr uint32 FERAL_RAKE = 1822;
constexpr uint32 FERAL_RIP = 1079;
constexpr uint32 FERAL_FEROCIOUS_BITE = 22568;
constexpr uint32 FERAL_SWIPE_CAT = 106830;
constexpr uint32 FERAL_THRASH_CAT = 106832;
constexpr uint32 FERAL_BRUTAL_SLASH = 202028;
constexpr uint32 FERAL_PRIMAL_WRATH = 285381;
constexpr uint32 FERAL_MOONFIRE_CAT = 155625; // Lunar Inspiration talent
constexpr uint32 FERAL_TIGERS_FURY = 5217;
constexpr uint32 FERAL_BERSERK = 106951;
constexpr uint32 FERAL_INCARNATION_KING = 102543; // Incarnation: Avatar of Ashamane
constexpr uint32 FERAL_BLOODTALONS = 155672; // Healing touch buff
constexpr uint32 FERAL_CAT_FORM = 768;
constexpr uint32 FERAL_DASH = 1850;
constexpr uint32 FERAL_STAMPEDING_ROAR = 106898;
constexpr uint32 FERAL_SURVIVAL_INSTINCTS = 61336;
constexpr uint32 FERAL_BARKSKIN = 22812;
constexpr uint32 FERAL_RENEWAL = 108238;
constexpr uint32 FERAL_REGROWTH = 8936;

// Energy and Combo Points resource
struct EnergyComboResource
{
    uint32 energy = 0;
    uint32 maxEnergy = 100;
    uint32 comboPoints = 0;
    uint32 maxComboPoints = 5;
    bool available = true;

    void Initialize(Player* bot)
    {
        if (!bot)
            return;

        energy = bot->GetPower(POWER_ENERGY);
        maxEnergy = bot->GetMaxPower(POWER_ENERGY);
        comboPoints = bot->GetPower(POWER_COMBO_POINTS);
        maxComboPoints = bot->GetMaxPower(POWER_COMBO_POINTS);
    }

    void Update(Player* bot)
    {
        if (!bot)
            return;

        energy = bot->GetPower(POWER_ENERGY);
        comboPoints = bot->GetPower(POWER_COMBO_POINTS);
    }

    // ComplexResource interface requirements
    [[nodiscard]] bool Consume(uint32 amount)
    {
        if (energy >= amount)
        {
            energy -= amount;
            return true;
        }
        return false;
    }

    void Regenerate(uint32 /*diff*/)
    {
        available = true;
    }

    [[nodiscard]] uint32 GetAvailable() const { return energy; }
    [[nodiscard]] uint32 GetMax() const { return maxEnergy; }

    [[nodiscard]] bool HasEnergy(uint32 amount) const { return energy >= amount; }
    [[nodiscard]] bool HasComboPoints(uint32 amount) const { return comboPoints >= amount; }
    [[nodiscard]] bool IsMaxComboPoints() const { return comboPoints >= maxComboPoints; }
    [[nodiscard]] uint32 GetEnergyPercent() const { return maxEnergy > 0 ? (energy * 100) / maxEnergy : 0; }
};

// Bleed tracking system
class FeralBleedTracker
{
public:
    FeralBleedTracker() = default;

    void ApplyRake(ObjectGuid guid, uint32 duration)
    {
        _rakeTargets[guid] = getMSTime() + duration;
    }

    void ApplyRip(ObjectGuid guid, uint32 duration)
    {
        _ripTargets[guid] = getMSTime() + duration;
    }

    void ApplyThrash(ObjectGuid guid, uint32 duration)
    {
        _thrashTargets[guid] = getMSTime() + duration;
    }

    void ApplyMoonfire(ObjectGuid guid, uint32 duration)
    {
        _moonfireTargets[guid] = getMSTime() + duration;
    }

    [[nodiscard]] bool HasRake(ObjectGuid guid) const
    {
        auto it = _rakeTargets.find(guid);
        return it != _rakeTargets.end() && getMSTime() < it->second;
    }

    [[nodiscard]] bool HasRip(ObjectGuid guid) const
    {
        auto it = _ripTargets.find(guid);
        return it != _ripTargets.end() && getMSTime() < it->second;
    }

    [[nodiscard]] bool HasThrash(ObjectGuid guid) const
    {
        auto it = _thrashTargets.find(guid);
        return it != _thrashTargets.end() && getMSTime() < it->second;
    }

    [[nodiscard]] bool HasMoonfire(ObjectGuid guid) const
    {
        auto it = _moonfireTargets.find(guid);
        return it != _moonfireTargets.end() && getMSTime() < it->second;
    }

    [[nodiscard]] uint32 GetRakeTimeRemaining(ObjectGuid guid) const
    {
        auto it = _rakeTargets.find(guid);
        if (it != _rakeTargets.end())
        {
            uint32 now = getMSTime();
            return now < it->second ? (it->second - now) : 0;
        }
        return 0;
    }

    [[nodiscard]] uint32 GetRipTimeRemaining(ObjectGuid guid) const
    {
        auto it = _ripTargets.find(guid);
        if (it != _ripTargets.end())
        {
            uint32 now = getMSTime();
            return now < it->second ? (it->second - now) : 0;
        }
        return 0;
    }

    [[nodiscard]] bool NeedsRakeRefresh(ObjectGuid guid, uint32 pandemicWindow = 7200) const
    {
        uint32 remaining = GetRakeTimeRemaining(guid);
        return remaining < pandemicWindow; // Refresh in pandemic window (30% of 24 sec)
    }

    [[nodiscard]] bool NeedsRipRefresh(ObjectGuid guid, uint32 pandemicWindow = 7200) const
    {
        uint32 remaining = GetRipTimeRemaining(guid);
        return remaining < pandemicWindow; // Refresh in pandemic window
    }

    void Update(Unit* target)
    {
        if (!target)
            return;

        ObjectGuid guid = target->GetGUID();

        // Sync with actual auras
        if (Aura* rake = target->GetAura(FERAL_RAKE))
            _rakeTargets[guid] = getMSTime() + rake->GetDuration();
        else
            _rakeTargets.erase(guid);

        if (Aura* rip = target->GetAura(FERAL_RIP))
            _ripTargets[guid] = getMSTime() + rip->GetDuration();
        else
            _ripTargets.erase(guid);

        if (Aura* thrash = target->GetAura(FERAL_THRASH_CAT))
            _thrashTargets[guid] = getMSTime() + thrash->GetDuration();
        else
            _thrashTargets.erase(guid);

        if (Aura* moonfire = target->GetAura(FERAL_MOONFIRE_CAT))
            _moonfireTargets[guid] = getMSTime() + moonfire->GetDuration();
        else
            _moonfireTargets.erase(guid);
    }

    void CleanupExpired()
    {
        uint32 now = getMSTime();

        for (auto it = _rakeTargets.begin(); it != _rakeTargets.end();)
        {
            if (now >= it->second)
                it = _rakeTargets.erase(it);
            else
                ++it;
        }

        for (auto it = _ripTargets.begin(); it != _ripTargets.end();)
        {
            if (now >= it->second)
                it = _ripTargets.erase(it);
            else
                ++it;
        }

        for (auto it = _thrashTargets.begin(); it != _thrashTargets.end();)
        {
            if (now >= it->second)
                it = _thrashTargets.erase(it);
            else
                ++it;
        }

        for (auto it = _moonfireTargets.begin(); it != _moonfireTargets.end();)
        {
            if (now >= it->second)
                it = _moonfireTargets.erase(it);
            else
                ++it;
        }
    }

private:
    std::unordered_map<ObjectGuid, uint32> _rakeTargets;
    std::unordered_map<ObjectGuid, uint32> _ripTargets;
    std::unordered_map<ObjectGuid, uint32> _thrashTargets;
    std::unordered_map<ObjectGuid, uint32> _moonfireTargets;
};

// Bloodtalons proc tracker
class FeralBloodtalonsTracker
{
public:
    FeralBloodtalonsTracker() : _bloodtalonsActive(false), _bloodtalonsEndTime(0), _bloodtalonsStacks(0) {}

    void ActivateProc(uint32 stacks = 2)
    {
        _bloodtalonsActive = true;
        _bloodtalonsStacks = stacks;
        _bloodtalonsEndTime = getMSTime() + 30000; // 30 sec duration
    }

    void ConsumeStack()
    {
        if (_bloodtalonsStacks > 0)
            _bloodtalonsStacks--;

        if (_bloodtalonsStacks == 0)
            _bloodtalonsActive = false;
    }

    [[nodiscard]] bool IsActive() const { return _bloodtalonsActive && getMSTime() < _bloodtalonsEndTime; }
    [[nodiscard]] uint32 GetStacks() const { return _bloodtalonsStacks; }

    void Update(Player* bot)
    {
        if (!bot)
            return;

        if (Aura* aura = bot->GetAura(FERAL_BLOODTALONS))
        {
            _bloodtalonsActive = true;
            _bloodtalonsStacks = aura->GetStackAmount();
            _bloodtalonsEndTime = getMSTime() + aura->GetDuration();
        }
        else
        {
            _bloodtalonsActive = false;
            _bloodtalonsStacks = 0;
        }
    }

private:
    bool _bloodtalonsActive;
    uint32 _bloodtalonsEndTime;
    uint32 _bloodtalonsStacks;
};

class FeralDruidRefactored : public MeleeDpsSpecialization<EnergyComboResource>, public DruidSpecialization
{
public:
    // Use base class members with type alias for cleaner syntax
    using Base = MeleeDpsSpecialization<EnergyComboResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::GetEnemiesInRange;
    using Base::_resource;
    explicit FeralDruidRefactored(Player* bot)
        : MeleeDpsSpecialization<EnergyComboResource>(bot)
        , DruidSpecialization(bot)
        , _bleedTracker()
        , _bloodtalonsTracker()
        , _tigersFuryActive(false)
        , _tigersFuryEndTime(0)
        , _berserkActive(false)
        , _berserkEndTime(0)
        , _lastTigersFuryTime(0)
        , _lastBerserkTime(0)
    {
        this->_resource.Initialize(bot);
        InitializeCooldowns();
        TC_LOG_DEBUG("playerbot", "FeralDruidRefactored initialized for {}", bot->GetName());
    }

    void UpdateRotation(::Unit* target) override
    {
        if (!target || !bot)
            return;

        UpdateFeralState(target);
        MaintainCatForm();

        uint32 enemyCount = GetEnemiesInRange(8.0f);

        if (enemyCount >= 3)
            ExecuteAoERotation(target, enemyCount);
        else
            ExecuteSingleTargetRotation(target);
    }

    void UpdateBuffs() override
    {
        if (!bot)
            return;

        MaintainCatForm();
    }

    void UpdateDefensives()
    {
        if (!bot)
            return;

        float healthPct = bot->GetHealthPct();

        // Survival Instincts (critical emergency)
        if (healthPct < 30.0f && this->CanCastSpell(FERAL_SURVIVAL_INSTINCTS, bot))
        {
            this->CastSpell(bot, FERAL_SURVIVAL_INSTINCTS);
            return;
        }

        // Barkskin (moderate damage reduction)
        if (healthPct < 50.0f && this->CanCastSpell(FERAL_BARKSKIN, bot))
        {
            this->CastSpell(bot, FERAL_BARKSKIN);
            return;
        }

        // Renewal (self-heal)
        if (healthPct < 60.0f && this->CanCastSpell(FERAL_RENEWAL, bot))
        {
            this->CastSpell(bot, FERAL_RENEWAL);
            return;
        }

        // Regrowth (if out of combat and low health)
        if (healthPct < 70.0f && !bot->IsInCombat() && this->CanCastSpell(FERAL_REGROWTH, bot))
        {
            this->CastSpell(bot, FERAL_REGROWTH);
        }
    }

private:
    void InitializeCooldowns()
    {
        _lastTigersFuryTime = 0;
        _lastBerserkTime = 0;
    }

    void UpdateFeralState(::Unit* target)
    {
        this->_resource.Update(bot);
        _bleedTracker.Update(target);
        _bloodtalonsTracker.Update(bot);
        UpdateCooldownStates();
    }

    void UpdateCooldownStates()
    {
        // Tiger's Fury state
        if (_tigersFuryActive && getMSTime() >= _tigersFuryEndTime)
            _tigersFuryActive = false;

        if (bot->HasAura(FERAL_TIGERS_FURY))
        {
            _tigersFuryActive = true;
            if (Aura* aura = bot->GetAura(FERAL_TIGERS_FURY))
                _tigersFuryEndTime = getMSTime() + aura->GetDuration();
        }

        // Berserk state
        if (_berserkActive && getMSTime() >= _berserkEndTime)
            _berserkActive = false;

        if (bot->HasAura(FERAL_BERSERK) || bot->HasAura(FERAL_INCARNATION_KING))
        {
            _berserkActive = true;
            Aura* aura = bot->GetAura(FERAL_BERSERK);
            if (!aura)
                aura = bot->GetAura(FERAL_INCARNATION_KING);
            if (aura)
                _berserkEndTime = getMSTime() + aura->GetDuration();
        }
    }

    void MaintainCatForm()
    {
        if (!bot->HasAura(FERAL_CAT_FORM))
        {
            if (this->CanCastSpell(FERAL_CAT_FORM, bot))
            {
                this->CastSpell(bot, FERAL_CAT_FORM);
            }
        }
    }

    void ExecuteSingleTargetRotation(::Unit* target)
    {
        ObjectGuid targetGuid = target->GetGUID();
        uint32 cp = this->_resource.comboPoints;

        // Tiger's Fury for energy regeneration (use when low energy and not capped on combo points)
        if (this->_resource.GetEnergyPercent() < 50 && cp < this->_resource.maxComboPoints)
        {
            if (this->CanCastSpell(FERAL_TIGERS_FURY, bot))
            {
                this->CastSpell(bot, FERAL_TIGERS_FURY);
                _tigersFuryActive = true;
                _tigersFuryEndTime = getMSTime() + 15000; // 15 sec duration
                _lastTigersFuryTime = getMSTime();
                return;
            }
        }

        // Berserk/Incarnation (major cooldown burst)
        if (cp >= 4 && _bleedTracker.HasRake(targetGuid) && _bleedTracker.HasRip(targetGuid))
        {
            if (this->CanCastSpell(FERAL_INCARNATION_KING, bot))
            {
                this->CastSpell(bot, FERAL_INCARNATION_KING);
                _berserkActive = true;
                _berserkEndTime = getMSTime() + 30000;
                _lastBerserkTime = getMSTime();
                return;
            }
            else if (this->CanCastSpell(FERAL_BERSERK, bot))
            {
                this->CastSpell(bot, FERAL_BERSERK);
                _berserkActive = true;
                _berserkEndTime = getMSTime() + 15000;
                _lastBerserkTime = getMSTime();
                return;
            }
        }

        // Rip (maintain bleed at 5 combo points)
        if (cp >= 5 && _bleedTracker.NeedsRipRefresh(targetGuid))
        {
            if (this->CanCastSpell(FERAL_RIP, target))
            {
                this->CastSpell(target, FERAL_RIP);
                _bleedTracker.ApplyRip(targetGuid, 24000); // 24 sec base duration
                ConsumeComboPoints(cp);
                if (_bloodtalonsTracker.IsActive())
                    _bloodtalonsTracker.ConsumeStack();
                return;
            }
        }

        // Ferocious Bite (spend combo points if Rip is up and we're capped)
        if (cp >= 5 && _bleedTracker.HasRip(targetGuid) && _bleedTracker.GetRipTimeRemaining(targetGuid) > 10000)
        {
            if (this->CanCastSpell(FERAL_FEROCIOUS_BITE, target))
            {
                this->CastSpell(target, FERAL_FEROCIOUS_BITE);
                ConsumeComboPoints(cp);
                if (_bloodtalonsTracker.IsActive())
                    _bloodtalonsTracker.ConsumeStack();
                return;
            }
        }

        // Rake (maintain bleed)
        if (_bleedTracker.NeedsRakeRefresh(targetGuid) && this->_resource.HasEnergy(35))
        {
            if (this->CanCastSpell(FERAL_RAKE, target))
            {
                this->CastSpell(target, FERAL_RAKE);
                _bleedTracker.ApplyRake(targetGuid, 15000); // 15 sec base duration
                GenerateComboPoints(1);
                if (_bloodtalonsTracker.IsActive())
                    _bloodtalonsTracker.ConsumeStack();
                return;
            }
        }

        // Moonfire (Lunar Inspiration talent - maintain DoT)
        if (bot->HasSpell(FERAL_MOONFIRE_CAT) && !_bleedTracker.HasMoonfire(targetGuid) && this->_resource.HasEnergy(30))
        {
            if (this->CanCastSpell(FERAL_MOONFIRE_CAT, target))
            {
                this->CastSpell(target, FERAL_MOONFIRE_CAT);
                _bleedTracker.ApplyMoonfire(targetGuid, 16000);
                GenerateComboPoints(1);
                return;
            }
        }

        // Brutal Slash (if talented and available)
        if (bot->HasSpell(FERAL_BRUTAL_SLASH) && this->_resource.HasEnergy(25))
        {
            if (this->CanCastSpell(FERAL_BRUTAL_SLASH, target))
            {
                this->CastSpell(target, FERAL_BRUTAL_SLASH);
                GenerateComboPoints(1);
                if (_bloodtalonsTracker.IsActive())
                    _bloodtalonsTracker.ConsumeStack();
                return;
            }
        }

        // Shred (builder)
        if (this->_resource.HasEnergy(40))
        {
            if (this->CanCastSpell(FERAL_SHRED, target))
            {
                this->CastSpell(target, FERAL_SHRED);
                GenerateComboPoints(1);
                if (_bloodtalonsTracker.IsActive())
                    _bloodtalonsTracker.ConsumeStack();
                return;
            }
        }
    }

    void ExecuteAoERotation(::Unit* target, uint32 enemyCount)
    {
        ObjectGuid targetGuid = target->GetGUID();
        uint32 cp = this->_resource.comboPoints;

        // Tiger's Fury for energy
        if (this->_resource.GetEnergyPercent() < 50 && cp < this->_resource.maxComboPoints)
        {
            if (this->CanCastSpell(FERAL_TIGERS_FURY, bot))
            {
                this->CastSpell(bot, FERAL_TIGERS_FURY);
                _tigersFuryActive = true;
                _tigersFuryEndTime = getMSTime() + 15000;
                _lastTigersFuryTime = getMSTime();
                return;
            }
        }

        // Berserk for AoE burst
        if (cp >= 4 && enemyCount >= 4)
        {
            if (this->CanCastSpell(FERAL_INCARNATION_KING, bot))
            {
                this->CastSpell(bot, FERAL_INCARNATION_KING);
                _berserkActive = true;
                _berserkEndTime = getMSTime() + 30000;
                _lastBerserkTime = getMSTime();
                return;
            }
            else if (this->CanCastSpell(FERAL_BERSERK, bot))
            {
                this->CastSpell(bot, FERAL_BERSERK);
                _berserkActive = true;
                _berserkEndTime = getMSTime() + 15000;
                _lastBerserkTime = getMSTime();
                return;
            }
        }

        // Primal Wrath (AoE finisher - applies Rip to all nearby enemies)
        if (cp >= 5 && bot->HasSpell(FERAL_PRIMAL_WRATH) && enemyCount >= 3)
        {
            if (this->CanCastSpell(FERAL_PRIMAL_WRATH, target))
            {
                this->CastSpell(target, FERAL_PRIMAL_WRATH);
                ConsumeComboPoints(cp);
                return;
            }
        }

        // Thrash (AoE bleed builder)
        if (this->_resource.HasEnergy(45) && !_bleedTracker.HasThrash(targetGuid))
        {
            if (this->CanCastSpell(FERAL_THRASH_CAT, target))
            {
                this->CastSpell(target, FERAL_THRASH_CAT);
                _bleedTracker.ApplyThrash(targetGuid, 15000);
                GenerateComboPoints(1);
                return;
            }
        }

        // Brutal Slash (if talented - strong AoE)
        if (bot->HasSpell(FERAL_BRUTAL_SLASH) && this->_resource.HasEnergy(25))
        {
            if (this->CanCastSpell(FERAL_BRUTAL_SLASH, target))
            {
                this->CastSpell(target, FERAL_BRUTAL_SLASH);
                GenerateComboPoints(1);
                return;
            }
        }

        // Swipe (AoE filler)
        if (this->_resource.HasEnergy(35))
        {
            if (this->CanCastSpell(FERAL_SWIPE_CAT, target))
            {
                this->CastSpell(target, FERAL_SWIPE_CAT);
                GenerateComboPoints(1);
                return;
            }
        }

        // Rake on primary target
        if (_bleedTracker.NeedsRakeRefresh(targetGuid) && this->_resource.HasEnergy(35))
        {
            if (this->CanCastSpell(FERAL_RAKE, target))
            {
                this->CastSpell(target, FERAL_RAKE);
                _bleedTracker.ApplyRake(targetGuid, 15000);
                GenerateComboPoints(1);
                return;
            }
        }
    }

    void GenerateComboPoints(uint32 amount)
    {
        this->_resource.comboPoints = std::min(this->_resource.comboPoints + amount, this->_resource.maxComboPoints);
    }

    void ConsumeComboPoints(uint32 amount)
    {
        if (this->_resource.comboPoints >= amount)
            this->_resource.comboPoints -= amount;
        else
            this->_resource.comboPoints = 0;
    }

    // Note: GetEnemiesInRange is provided by the base template class
    // No need to redefine it here - use Base::GetEnemiesInRange

    // Member variables
    FeralBleedTracker _bleedTracker;
    FeralBloodtalonsTracker _bloodtalonsTracker;

    bool _tigersFuryActive;
    uint32 _tigersFuryEndTime;
    bool _berserkActive;
    uint32 _berserkEndTime;

    uint32 _lastTigersFuryTime;
    uint32 _lastBerserkTime;
};

} // namespace Playerbot

#endif // PLAYERBOT_FERALDRDUIDREFACTORED_H
