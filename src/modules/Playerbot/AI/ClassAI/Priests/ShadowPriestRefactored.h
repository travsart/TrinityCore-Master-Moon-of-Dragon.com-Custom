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

#ifndef PLAYERBOT_SHADOWPRIESTREFACTORED_H
#define PLAYERBOT_SHADOWPRIESTREFACTORED_H

#include "PriestSpecialization.h"
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

// WoW 11.2 (The War Within) - Shadow Priest Spell IDs
constexpr uint32 SHADOW_MIND_BLAST = 8092;
constexpr uint32 SHADOW_MIND_FLAY = 15407;
constexpr uint32 SHADOW_VAMPIRIC_TOUCH = 34914;
constexpr uint32 SHADOW_SHADOW_WORD_PAIN = 589;
constexpr uint32 SHADOW_DEVOURING_PLAGUE = 335467;
constexpr uint32 SHADOW_VOID_ERUPTION = 228260;
constexpr uint32 SHADOW_VOID_BOLT = 205448;
constexpr uint32 SHADOW_MIND_SEAR = 48045;
constexpr uint32 SHADOW_SHADOW_CRASH = 205385;
constexpr uint32 SHADOW_VOID_TORRENT = 263165;
constexpr uint32 SHADOW_DARK_ASCENSION = 391109;
constexpr uint32 SHADOW_MINDGAMES = 375901;
constexpr uint32 SHADOW_SHADOW_WORD_DEATH = 32379;
constexpr uint32 SHADOW_VAMPIRIC_EMBRACE = 15286;
constexpr uint32 SHADOW_DISPERSION = 47585;
constexpr uint32 SHADOW_FADE = 586;
constexpr uint32 SHADOW_DESPERATE_PRAYER = 19236;
constexpr uint32 SHADOW_POWER_WORD_FORTITUDE = 21562;
constexpr uint32 SHADOW_SHADOWFORM = 232698;

// Insanity tracker (Shadow Priest secondary resource - primary is still Mana)
class InsanityTracker
{
public:
    InsanityTracker() : _insanity(0), _maxInsanity(100) {}

    void GenerateInsanity(uint32 amount)
    {
        _insanity = std::min(_insanity + amount, _maxInsanity);
    }

    void SpendInsanity(uint32 amount)
    {
        if (_insanity >= amount)
            _insanity -= amount;
        else
            _insanity = 0;
    }

    [[nodiscard]] bool HasInsanity(uint32 amount) const { return _insanity >= amount; }
    [[nodiscard]] uint32 GetInsanity() const { return _insanity; }
    [[nodiscard]] uint32 GetInsanityPercent() const { return (_insanity * 100) / _maxInsanity; }

    void Reset() { _insanity = 0; }

private:
    uint32 _insanity;
    uint32 _maxInsanity;
};

// Voidform tracker (Shadow's burst mode)
class VoidformTracker
{
public:
    VoidformTracker() : _voidformActive(false), _voidformStacks(0), _voidformEndTime(0) {}

    void ActivateVoidform()
    {
        _voidformActive = true;
        _voidformStacks = 1;
        _voidformEndTime = getMSTime() + 15000; // Base duration
    }

    void DeactivateVoidform()
    {
        _voidformActive = false;
        _voidformStacks = 0;
    }

    void IncrementStack()
    {
        if (_voidformActive)
        {
            _voidformStacks++;
            // Each stack increases drain rate, but also extends slightly
            _voidformEndTime = getMSTime() + 1000; // 1 sec extension per stack
        }
    }

    [[nodiscard]] bool IsActive() const
    {
        return _voidformActive && getMSTime() < _voidformEndTime;
    }

    [[nodiscard]] uint32 GetStacks() const { return _voidformStacks; }

    void Update(Player* bot)
    {
        if (!bot)
            return;

        // Check if Voidform buff is active
        if (bot->HasAura(SHADOW_VOID_ERUPTION) || bot->HasAura(194249)) // Voidform buff ID
        {
            if (!_voidformActive)
                ActivateVoidform();
        }
        else
        {
            if (_voidformActive)
                DeactivateVoidform();
        }

        // Voidform expires over time
        if (_voidformActive && getMSTime() >= _voidformEndTime)
            DeactivateVoidform();
    }

private:
    bool _voidformActive;
    uint32 _voidformStacks;
    uint32 _voidformEndTime;
};

// DoT tracker for Vampiric Touch and Shadow Word: Pain
class ShadowDoTTracker
{
public:
    ShadowDoTTracker() = default;

    void ApplyVampiricTouch(ObjectGuid guid, uint32 duration = 21000)
    {
        _vampiricTouchTargets[guid] = getMSTime() + duration;
    }

    void ApplyShadowWordPain(ObjectGuid guid, uint32 duration = 16000)
    {
        _shadowWordPainTargets[guid] = getMSTime() + duration;
    }

    [[nodiscard]] bool HasVampiricTouch(ObjectGuid guid) const
    {
        auto it = _vampiricTouchTargets.find(guid);
        if (it == _vampiricTouchTargets.end())
            return false;
        return getMSTime() < it->second;
    }

    [[nodiscard]] bool HasShadowWordPain(ObjectGuid guid) const
    {
        auto it = _shadowWordPainTargets.find(guid);
        if (it == _shadowWordPainTargets.end())
            return false;
        return getMSTime() < it->second;
    }

    [[nodiscard]] uint32 GetVampiricTouchTimeRemaining(ObjectGuid guid) const
    {
        auto it = _vampiricTouchTargets.find(guid);
        if (it == _vampiricTouchTargets.end())
            return 0;

        uint32 now = getMSTime();
        if (now >= it->second)
            return 0;

        return it->second - now;
    }

    [[nodiscard]] uint32 GetShadowWordPainTimeRemaining(ObjectGuid guid) const
    {
        auto it = _shadowWordPainTargets.find(guid);
        if (it == _shadowWordPainTargets.end())
            return 0;

        uint32 now = getMSTime();
        if (now >= it->second)
            return 0;

        return it->second - now;
    }

    [[nodiscard]] bool NeedsVampiricTouchRefresh(ObjectGuid guid, uint32 pandemicWindow = 6300) const
    {
        uint32 remaining = GetVampiricTouchTimeRemaining(guid);
        return remaining < pandemicWindow;
    }

    [[nodiscard]] bool NeedsShadowWordPainRefresh(ObjectGuid guid, uint32 pandemicWindow = 4800) const
    {
        uint32 remaining = GetShadowWordPainTimeRemaining(guid);
        return remaining < pandemicWindow;
    }

    void Update(Player* bot)
    {
        if (!bot)
            return;

        // Clean up expired DoTs
        uint32 now = getMSTime();
        for (auto it = _vampiricTouchTargets.begin(); it != _vampiricTouchTargets.end();)
        {
            if (now >= it->second)
                it = _vampiricTouchTargets.erase(it);
            else
                ++it;
        }

        for (auto it = _shadowWordPainTargets.begin(); it != _shadowWordPainTargets.end();)
        {
            if (now >= it->second)
                it = _shadowWordPainTargets.erase(it);
            else
                ++it;
        }
    }

private:
    std::unordered_map<ObjectGuid, uint32> _vampiricTouchTargets; // GUID -> expiration time
    std::unordered_map<ObjectGuid, uint32> _shadowWordPainTargets; // GUID -> expiration time
};

class ShadowPriestRefactored : public RangedDpsSpecialization<ManaResource>, public PriestSpecialization
{
public:
    using Base = RangedDpsSpecialization<ManaResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::_resource;
    explicit ShadowPriestRefactored(Player* bot)
        : RangedDpsSpecialization<ManaResource>(bot)
        , PriestSpecialization(bot)
        , _insanityTracker()
        , _voidformTracker()
        , _dotTracker()
        , _darkAscensionActive(false)
        , _darkAscensionEndTime(0)
        , _lastDarkAscensionTime(0)
        , _lastVoidTorrentTime(0)
        , _lastMindgamesTime(0)
        , _lastVampiricEmbraceTime(0)
    {
        InitializeCooldowns();
        TC_LOG_DEBUG("playerbot", "ShadowPriestRefactored initialized for {}", bot->GetName());
    }

    void UpdateRotation(::Unit* target) override
    {
        Player* bot = this->GetBot();
        if (!target || !bot)
            return;

        UpdateShadowState();

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

        // Shadowform (mandatory buff - 10% damage increase)
        if (!bot->HasAura(SHADOW_SHADOWFORM))
        {
            if (this->CanCastSpell(SHADOW_SHADOWFORM, bot))
            {
                this->CastSpell(bot, SHADOW_SHADOWFORM);
            }
        }

        // Power Word: Fortitude (group buff)
        if (!bot->HasAura(SHADOW_POWER_WORD_FORTITUDE))
        {
            if (this->CanCastSpell(SHADOW_POWER_WORD_FORTITUDE, bot))
            {
                this->CastSpell(bot, SHADOW_POWER_WORD_FORTITUDE);
            }
        }

        // Vampiric Embrace (healing for group)
        if ((getMSTime() - _lastVampiricEmbraceTime) >= 120000) // 2 min CD
        {
            if (bot->GetHealthPct() < 70.0f || (bot->GetGroup() && bot->GetGroup()->GetMembersCount() > 1))
            {
                if (this->CanCastSpell(SHADOW_VAMPIRIC_EMBRACE, bot))
                {
                    this->CastSpell(bot, SHADOW_VAMPIRIC_EMBRACE);
                    _lastVampiricEmbraceTime = getMSTime();
                }
            }
        }
    }

    void UpdateDefensives()
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        float healthPct = bot->GetHealthPct();

        // Dispersion (immune + heal)
        if (healthPct < 25.0f && this->CanCastSpell(SHADOW_DISPERSION, bot))
        {
            this->CastSpell(bot, SHADOW_DISPERSION);
            return;
        }

        // Desperate Prayer (self-heal)
        if (healthPct < 40.0f && this->CanCastSpell(SHADOW_DESPERATE_PRAYER, bot))
        {
            this->CastSpell(bot, SHADOW_DESPERATE_PRAYER);
            return;
        }

        // Fade (threat reduction)
        if (healthPct < 60.0f && bot->GetThreatManager().GetThreatListSize() > 0)
        {
            if (this->CanCastSpell(SHADOW_FADE, bot))
            {
                this->CastSpell(bot, SHADOW_FADE);
                return;
            }
        }
    }

private:
    void InitializeCooldowns()
    {
        _lastDarkAscensionTime = 0;
        _lastVoidTorrentTime = 0;
        _lastMindgamesTime = 0;
        _lastVampiricEmbraceTime = 0;
    }

    void UpdateShadowState()
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        _voidformTracker.Update(bot);
        _dotTracker.Update(bot);
        UpdateCooldownStates();
    }

    void UpdateCooldownStates()
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        // Dark Ascension state (alternative to Void Eruption)
        if (_darkAscensionActive && getMSTime() >= _darkAscensionEndTime)
            _darkAscensionActive = false;

        if (bot->HasAura(SHADOW_DARK_ASCENSION))
        {
            _darkAscensionActive = true;
            if (Aura* aura = bot->GetAura(SHADOW_DARK_ASCENSION))
                _darkAscensionEndTime = getMSTime() + aura->GetDuration();
        }
    }

    void ExecuteSingleTargetRotation(::Unit* target)
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        uint32 insanity = _insanityTracker.GetInsanity();

        // Enter Voidform (or Dark Ascension)
        if (insanity >= 60 && !_voidformTracker.IsActive())
        {
            // Dark Ascension (alternative to Void Eruption)
            if (bot->HasSpell(SHADOW_DARK_ASCENSION) && (getMSTime() - _lastDarkAscensionTime) >= 60000)
            {
                if (this->CanCastSpell(SHADOW_DARK_ASCENSION, bot))
                {
                    this->CastSpell(bot, SHADOW_DARK_ASCENSION);
                    _darkAscensionActive = true;
                    _darkAscensionEndTime = getMSTime() + 15000;
                    _lastDarkAscensionTime = getMSTime();
                    _insanityTracker.SpendInsanity(50);
                    return;
                }
            }

            // Void Eruption (enter Voidform)
            if (this->CanCastSpell(SHADOW_VOID_ERUPTION, target))
            {
                this->CastSpell(target, SHADOW_VOID_ERUPTION);
                _voidformTracker.ActivateVoidform();
                _insanityTracker.Reset(); // Void Eruption consumes all Insanity
                return;
            }
        }

        // Maintain DoTs
        if (!_dotTracker.HasVampiricTouch(target->GetGUID()) ||
            _dotTracker.NeedsVampiricTouchRefresh(target->GetGUID()))
        {
            if (this->CanCastSpell(SHADOW_VAMPIRIC_TOUCH, target))
            {
                this->CastSpell(target, SHADOW_VAMPIRIC_TOUCH);
                _dotTracker.ApplyVampiricTouch(target->GetGUID(), 21000);
                _insanityTracker.GenerateInsanity(5);
                return;
            }
        }

        if (!_dotTracker.HasShadowWordPain(target->GetGUID()) ||
            _dotTracker.NeedsShadowWordPainRefresh(target->GetGUID()))
        {
            if (this->CanCastSpell(SHADOW_SHADOW_WORD_PAIN, target))
            {
                this->CastSpell(target, SHADOW_SHADOW_WORD_PAIN);
                _dotTracker.ApplyShadowWordPain(target->GetGUID(), 16000);
                _insanityTracker.GenerateInsanity(4);
                return;
            }
        }

        // Voidform rotation (in Voidform, use Void Bolt instead of Mind Blast)
        if (_voidformTracker.IsActive())
        {
            // Void Bolt (Voidform exclusive)
            if (this->CanCastSpell(SHADOW_VOID_BOLT, target))
            {
                this->CastSpell(target, SHADOW_VOID_BOLT);
                _voidformTracker.IncrementStack();
                // Void Bolt refreshes DoTs
                _dotTracker.ApplyVampiricTouch(target->GetGUID(), 21000);
                _dotTracker.ApplyShadowWordPain(target->GetGUID(), 16000);
                return;
            }
        }

        // Devouring Plague (Insanity spender)
        if (insanity >= 50)
        {
            if (this->CanCastSpell(SHADOW_DEVOURING_PLAGUE, target))
            {
                this->CastSpell(target, SHADOW_DEVOURING_PLAGUE);
                _insanityTracker.SpendInsanity(50);
                return;
            }
        }

        // Mindgames (cooldown ability)
        if (bot->HasSpell(SHADOW_MINDGAMES) && (getMSTime() - _lastMindgamesTime) >= 45000)
        {
            if (this->CanCastSpell(SHADOW_MINDGAMES, target))
            {
                this->CastSpell(target, SHADOW_MINDGAMES);
                _lastMindgamesTime = getMSTime();
                _insanityTracker.GenerateInsanity(10);
                return;
            }
        }

        // Shadow Word: Death (execute + Insanity on kill)
        if (target->GetHealthPct() < 20.0f)
        {
            if (this->CanCastSpell(SHADOW_SHADOW_WORD_DEATH, target))
            {
                this->CastSpell(target, SHADOW_SHADOW_WORD_DEATH);
                _insanityTracker.GenerateInsanity(15);
                return;
            }
        }

        // Mind Blast (Insanity generator)
        if (this->CanCastSpell(SHADOW_MIND_BLAST, target))
        {
            this->CastSpell(target, SHADOW_MIND_BLAST);
            _insanityTracker.GenerateInsanity(12);
            return;
        }

        // Void Torrent (channeled damage)
        if (bot->HasSpell(SHADOW_VOID_TORRENT) && (getMSTime() - _lastVoidTorrentTime) >= 30000)
        {
            if (this->CanCastSpell(SHADOW_VOID_TORRENT, target))
            {
                this->CastSpell(target, SHADOW_VOID_TORRENT);
                _lastVoidTorrentTime = getMSTime();
                _insanityTracker.GenerateInsanity(15);
                return;
            }
        }

        // Mind Flay (filler - channels, generates Insanity)
        if (this->CanCastSpell(SHADOW_MIND_FLAY, target))
        {
            this->CastSpell(target, SHADOW_MIND_FLAY);
            _insanityTracker.GenerateInsanity(3);
            return;
        }
    }

    void ExecuteAoERotation(::Unit* target, uint32 enemyCount)
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        uint32 insanity = _insanityTracker.GetInsanity();

        // Enter Voidform for AoE burst
        if (insanity >= 60 && !_voidformTracker.IsActive() && enemyCount >= 4)
        {
            if (bot->HasSpell(SHADOW_DARK_ASCENSION) && (getMSTime() - _lastDarkAscensionTime) >= 60000)
            {
                if (this->CanCastSpell(SHADOW_DARK_ASCENSION, bot))
                {
                    this->CastSpell(bot, SHADOW_DARK_ASCENSION);
                    _darkAscensionActive = true;
                    _darkAscensionEndTime = getMSTime() + 15000;
                    _lastDarkAscensionTime = getMSTime();
                    _insanityTracker.SpendInsanity(50);
                    return;
                }
            }

            if (this->CanCastSpell(SHADOW_VOID_ERUPTION, target))
            {
                this->CastSpell(target, SHADOW_VOID_ERUPTION);
                _voidformTracker.ActivateVoidform();
                _insanityTracker.Reset();
                return;
            }
        }

        // Shadow Crash (AoE DoT application)
        if (bot->HasSpell(SHADOW_SHADOW_CRASH) && enemyCount >= 3)
        {
            if (this->CanCastSpell(SHADOW_SHADOW_CRASH, target))
            {
                this->CastSpell(target, SHADOW_SHADOW_CRASH);
                _insanityTracker.GenerateInsanity(15);
                return;
            }
        }

        // Vampiric Touch on multiple targets
        if (enemyCount <= 5)
        {
            // Multi-dot if 5 or fewer enemies
            if (!_dotTracker.HasVampiricTouch(target->GetGUID()))
            {
                if (this->CanCastSpell(SHADOW_VAMPIRIC_TOUCH, target))
                {
                    this->CastSpell(target, SHADOW_VAMPIRIC_TOUCH);
                    _dotTracker.ApplyVampiricTouch(target->GetGUID(), 21000);
                    _insanityTracker.GenerateInsanity(5);
                    return;
                }
            }
        }

        // Devouring Plague (AoE spender)
        if (insanity >= 50 && enemyCount >= 3)
        {
            if (this->CanCastSpell(SHADOW_DEVOURING_PLAGUE, target))
            {
                this->CastSpell(target, SHADOW_DEVOURING_PLAGUE);
                _insanityTracker.SpendInsanity(50);
                return;
            }
        }

        // Mind Sear (AoE filler)
        if (enemyCount >= 3)
        {
            if (this->CanCastSpell(SHADOW_MIND_SEAR, target))
            {
                this->CastSpell(target, SHADOW_MIND_SEAR);
                _insanityTracker.GenerateInsanity(5);
                return;
            }
        }

        // Mind Blast
        if (this->CanCastSpell(SHADOW_MIND_BLAST, target))
        {
            this->CastSpell(target, SHADOW_MIND_BLAST);
            _insanityTracker.GenerateInsanity(12);
            return;
        }

        // Mind Flay (filler)
        if (this->CanCastSpell(SHADOW_MIND_FLAY, target))
        {
            this->CastSpell(target, SHADOW_MIND_FLAY);
            _insanityTracker.GenerateInsanity(3);
            return;
        }
    }

    // Member variables
    InsanityTracker _insanityTracker;
    VoidformTracker _voidformTracker;
    ShadowDoTTracker _dotTracker;

    bool _darkAscensionActive;
    uint32 _darkAscensionEndTime;

    uint32 _lastDarkAscensionTime;
    uint32 _lastVoidTorrentTime;
    uint32 _lastMindgamesTime;
    uint32 _lastVampiricEmbraceTime;
};

} // namespace Playerbot

#endif // PLAYERBOT_SHADOWPRIESTREFACTORED_H
