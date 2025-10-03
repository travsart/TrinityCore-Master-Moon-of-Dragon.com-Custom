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

#ifndef PLAYERBOT_DISCIPLINEPRIESTREFACTORED_H
#define PLAYERBOT_DISCIPLINEPRIESTREFACTORED_H

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

// WoW 11.2 (The War Within) - Discipline Priest Spell IDs
constexpr uint32 DISC_SMITE = 585;
constexpr uint32 DISC_PENANCE = 47540;
constexpr uint32 DISC_POWER_WORD_SHIELD = 17;
constexpr uint32 DISC_SHADOW_MEND = 186263;
constexpr uint32 DISC_PURGE_WICKED = 204197;
constexpr uint32 DISC_POWER_WORD_RADIANCE = 194509;
constexpr uint32 DISC_RAPTURE = 47536;
constexpr uint32 DISC_PAIN_SUPPRESSION = 33206;
constexpr uint32 DISC_BARRIER = 62618;
constexpr uint32 DISC_EVANGELISM = 246287;
constexpr uint32 DISC_SCHISM = 214621;
constexpr uint32 DISC_MINDGAMES = 323673;
constexpr uint32 DISC_SHADOW_COVENANT = 314867;
constexpr uint32 DISC_POWER_WORD_LIFE = 373481;
constexpr uint32 DISC_PURIFY = 527;
constexpr uint32 DISC_SHADOW_WORD_PAIN = 589;
constexpr uint32 DISC_FADE = 586;
constexpr uint32 DISC_DESPERATE_PRAYER = 19236;
constexpr uint32 DISC_POWER_WORD_FORTITUDE = 21562;

// Atonement tracker - tracks which allies have Atonement buff for damage-to-healing conversion
class AtonementTracker
{
public:
    AtonementTracker() = default;

    void ApplyAtonement(ObjectGuid guid, uint32 duration = 15000)
    {
        _atonementTargets[guid] = getMSTime() + duration;
    }

    void RemoveAtonement(ObjectGuid guid)
    {
        _atonementTargets.erase(guid);
    }

    [[nodiscard]] bool HasAtonement(ObjectGuid guid) const
    {
        auto it = _atonementTargets.find(guid);
        if (it == _atonementTargets.end())
            return false;
        return getMSTime() < it->second;
    }

    [[nodiscard]] uint32 GetAtonementTimeRemaining(ObjectGuid guid) const
    {
        auto it = _atonementTargets.find(guid);
        if (it == _atonementTargets.end())
            return 0;

        uint32 now = getMSTime();
        if (now >= it->second)
            return 0;

        return it->second - now;
    }

    [[nodiscard]] bool NeedsAtonementRefresh(ObjectGuid guid, uint32 refreshWindow = 3000) const
    {
        uint32 remaining = GetAtonementTimeRemaining(guid);
        return remaining < refreshWindow;
    }

    [[nodiscard]] uint32 GetActiveAtonementCount() const
    {
        uint32 count = 0;
        uint32 now = getMSTime();
        for (const auto& pair : _atonementTargets)
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

        // Clean up expired Atonements
        uint32 now = getMSTime();
        for (auto it = _atonementTargets.begin(); it != _atonementTargets.end();)
        {
            if (now >= it->second)
                it = _atonementTargets.erase(it);
            else
                ++it;
        }
    }

private:
    std::unordered_map<ObjectGuid, uint32> _atonementTargets; // GUID -> expiration time
};

// Power Word: Shield tracker
class PowerWordShieldTracker
{
public:
    PowerWordShieldTracker() = default;

    void ApplyShield(ObjectGuid guid, uint32 duration = 15000)
    {
        _shieldTargets[guid] = getMSTime() + duration;
    }

    void RemoveShield(ObjectGuid guid)
    {
        _shieldTargets.erase(guid);
    }

    [[nodiscard]] bool HasShield(ObjectGuid guid) const
    {
        auto it = _shieldTargets.find(guid);
        if (it == _shieldTargets.end())
            return false;
        return getMSTime() < it->second;
    }

    void Update(Player* bot)
    {
        if (!bot)
            return;

        // Clean up expired shields
        uint32 now = getMSTime();
        for (auto it = _shieldTargets.begin(); it != _shieldTargets.end();)
        {
            if (now >= it->second)
                it = _shieldTargets.erase(it);
            else
                ++it;
        }
    }

private:
    std::unordered_map<ObjectGuid, uint32> _shieldTargets; // GUID -> expiration time
};

class DisciplinePriestRefactored : public HealerSpecialization<ManaResource>, public PriestSpecialization
{
public:
    using Base = HealerSpecialization<ManaResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::_resource;
    explicit DisciplinePriestRefactored(Player* bot)
        : HealerSpecialization<ManaResource>(bot)
        , PriestSpecialization(bot)
        , _atonementTracker()
        , _shieldTracker()
        , _raptureActive(false)
        , _raptureEndTime(0)
        , _lastRaptureTime(0)
        , _lastEvangelismTime(0)
        , _lastBarrierTime(0)
        , _lastPainSuppressionTime(0)
    {
        InitializeCooldowns();
        TC_LOG_DEBUG("playerbot", "DisciplinePriestRefactored initialized for {}", this->GetBot()->GetName());
    }

    void UpdateRotation(::Unit* target) override
    {
        Player* bot = this->GetBot();
        if (!target || !bot)
            return;

        UpdateDisciplineState();

        // Discipline is a healer - check group health first
        if (Group* group = bot->GetGroup())
        {
            std::vector<Unit*> groupMembers;
            for (GroupReference const& ref : group->GetMembers())
            {
                if (Player* member = ref.GetSource())
                {
                    if (member->IsAlive() && bot->IsInMap(member))
                        groupMembers.push_back(member);
                }
            }

            if (!groupMembers.empty())
            {
                if (HandleGroupHealing(groupMembers))
                    return;
            }
        }

        // Solo healing (self)
        if (bot->GetHealthPct() < 80.0f)
        {
            if (HandleSelfHealing())
                return;
        }

        // Deal damage to trigger Atonement healing
        ExecuteAtonementDamageRotation(target);
    }

    void UpdateBuffs() override
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        // Power Word: Fortitude (group buff)
        if (!bot->HasAura(DISC_POWER_WORD_FORTITUDE))
        {
            if (this->CanCastSpell(DISC_POWER_WORD_FORTITUDE, bot))
            {
                this->CastSpell(bot, DISC_POWER_WORD_FORTITUDE);
            }
        }
    }

    void UpdateDefensives()
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        float healthPct = bot->GetHealthPct();

        // Desperate Prayer (self-heal + damage reduction)
        if (healthPct < 30.0f && this->CanCastSpell(DISC_DESPERATE_PRAYER, bot))
        {
            this->CastSpell(bot, DISC_DESPERATE_PRAYER);
            return;
        }

        // Fade (threat reduction)
        if (healthPct < 50.0f && bot->GetThreatManager().GetThreatListSize() > 0)
        {
            if (this->CanCastSpell(DISC_FADE, bot))
            {
                this->CastSpell(bot, DISC_FADE);
                return;
            }
        }

        // Power Word: Shield (self)
        if (healthPct < 60.0f && !_shieldTracker.HasShield(bot->GetGUID()))
        {
            if (this->CanCastSpell(DISC_POWER_WORD_SHIELD, bot))
            {
                this->CastSpell(bot, DISC_POWER_WORD_SHIELD);
                _shieldTracker.ApplyShield(bot->GetGUID(), 15000);
                _atonementTracker.ApplyAtonement(bot->GetGUID(), 15000);
                return;
            }
        }
    }

private:
    void InitializeCooldowns()
    {
        _lastRaptureTime = 0;
        _lastEvangelismTime = 0;
        _lastBarrierTime = 0;
        _lastPainSuppressionTime = 0;
    }

    void UpdateDisciplineState()
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        _atonementTracker.Update(bot);
        _shieldTracker.Update(bot);
        UpdateCooldownStates();
    }

    void UpdateCooldownStates()
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        // Rapture state (free shields)
        if (_raptureActive && getMSTime() >= _raptureEndTime)
            _raptureActive = false;

        if (bot->HasAura(DISC_RAPTURE))
        {
            _raptureActive = true;
            if (Aura* aura = bot->GetAura(DISC_RAPTURE))
                _raptureEndTime = getMSTime() + aura->GetDuration();
        }
    }

    bool HandleGroupHealing(const std::vector<Unit*>& group)
    {
        // Emergency cooldowns
        if (HandleEmergencyCooldowns(group))
            return true;

        // Maintain Atonement on injured allies
        if (HandleAtonementMaintenance(group))
            return true;

        // Direct healing when Atonement isn't enough
        if (HandleDirectHealing(group))
            return true;

        // Apply shields
        if (HandleShielding(group))
            return true;

        return false;
    }

    bool HandleEmergencyCooldowns(const std::vector<Unit*>& group)
    {
        Player* bot = this->GetBot();
        if (!bot)
            return false;

        // Pain Suppression (critical tank save)
        for (Unit* member : group)
        {
            if (member && member->GetHealthPct() < 20.0f && IsTankRole(member))
            {
                if ((getMSTime() - _lastPainSuppressionTime) >= 180000) // 3 min CD
                {
                    if (this->CanCastSpell(DISC_PAIN_SUPPRESSION, member))
                    {
                        this->CastSpell(member, DISC_PAIN_SUPPRESSION);
                        _lastPainSuppressionTime = getMSTime();
                        return true;
                    }
                }
            }
        }

        // Power Barrier (raid-wide damage reduction)
        uint32 lowHealthCount = 0;
        for (Unit* member : group)
        {
            if (member && member->GetHealthPct() < 50.0f)
                ++lowHealthCount;
        }

        if (lowHealthCount >= 3 && (getMSTime() - _lastBarrierTime) >= 180000) // 3 min CD
        {
            if (this->CanCastSpell(DISC_BARRIER, bot))
            {
                this->CastSpell(bot, DISC_BARRIER); // Ground-targeted AoE
                _lastBarrierTime = getMSTime();
                return true;
            }
        }

        // Rapture (spam shields for heavy damage)
        if (lowHealthCount >= 4 && (getMSTime() - _lastRaptureTime) >= 90000) // 90 sec CD
        {
            if (this->CanCastSpell(DISC_RAPTURE, bot))
            {
                this->CastSpell(bot, DISC_RAPTURE);
                _raptureActive = true;
                _raptureEndTime = getMSTime() + 8000; // 8 sec duration
                _lastRaptureTime = getMSTime();
                return true;
            }
        }

        return false;
    }

    bool HandleAtonementMaintenance(const std::vector<Unit*>& group)
    {
        Player* bot = this->GetBot();
        if (!bot)
            return false;

        uint32 activeAtonements = _atonementTracker.GetActiveAtonementCount();

        // Evangelism (extend all Atonements by 6 sec)
        if (activeAtonements >= 4 && (getMSTime() - _lastEvangelismTime) >= 90000) // 90 sec CD
        {
            if (bot->HasSpell(DISC_EVANGELISM))
            {
                if (this->CanCastSpell(DISC_EVANGELISM, bot))
                {
                    this->CastSpell(bot, DISC_EVANGELISM);
                    _lastEvangelismTime = getMSTime();

                    // Extend all current Atonements
                    for (Unit* member : group)
                    {
                        if (member && _atonementTracker.HasAtonement(member->GetGUID()))
                        {
                            uint32 remaining = _atonementTracker.GetAtonementTimeRemaining(member->GetGUID());
                            _atonementTracker.ApplyAtonement(member->GetGUID(), remaining + 6000);
                        }
                    }
                    return true;
                }
            }
        }

        // Power Word: Radiance (AoE Atonement application)
        if (activeAtonements < 3)
        {
            for (Unit* member : group)
            {
                if (member && member->GetHealthPct() < 90.0f &&
                    !_atonementTracker.HasAtonement(member->GetGUID()))
                {
                    if (this->CanCastSpell(DISC_POWER_WORD_RADIANCE, member))
                    {
                        this->CastSpell(member, DISC_POWER_WORD_RADIANCE);
                        // Radiance applies Atonement to 5 nearby allies
                        _atonementTracker.ApplyAtonement(member->GetGUID(), 15000);
                        return true;
                    }
                }
            }
        }

        // Apply Atonement via Power Word: Shield on injured allies
        for (Unit* member : group)
        {
            if (member && member->GetHealthPct() < 85.0f)
            {
                if (_atonementTracker.NeedsAtonementRefresh(member->GetGUID()) &&
                    !_shieldTracker.HasShield(member->GetGUID()))
                {
                    if (this->CanCastSpell(DISC_POWER_WORD_SHIELD, member))
                    {
                        this->CastSpell(member, DISC_POWER_WORD_SHIELD);
                        _shieldTracker.ApplyShield(member->GetGUID(), 15000);
                        _atonementTracker.ApplyAtonement(member->GetGUID(), 15000);
                        return true;
                    }
                }
            }
        }

        return false;
    }

    bool HandleDirectHealing(const std::vector<Unit*>& group)
    {
        Player* bot = this->GetBot();
        if (!bot)
            return false;

        // Shadow Mend for emergency direct healing
        for (Unit* member : group)
        {
            if (member && member->GetHealthPct() < 50.0f)
            {
                if (this->CanCastSpell(DISC_SHADOW_MEND, member))
                {
                    this->CastSpell(member, DISC_SHADOW_MEND);
                    _atonementTracker.ApplyAtonement(member->GetGUID(), 15000);
                    return true;
                }
            }
        }

        // Power Word: Life (instant emergency heal)
        for (Unit* member : group)
        {
            if (member && member->GetHealthPct() < 35.0f)
            {
                if (bot->HasSpell(DISC_POWER_WORD_LIFE))
                {
                    if (this->CanCastSpell(DISC_POWER_WORD_LIFE, member))
                    {
                        this->CastSpell(member, DISC_POWER_WORD_LIFE);
                        return true;
                    }
                }
            }
        }

        // Penance (channeled heal/damage - use for healing if needed)
        for (Unit* member : group)
        {
            if (member && member->GetHealthPct() < 60.0f)
            {
                if (this->CanCastSpell(DISC_PENANCE, member))
                {
                    this->CastSpell(member, DISC_PENANCE);
                    return true;
                }
            }
        }

        return false;
    }

    bool HandleShielding(const std::vector<Unit*>& group)
    {
        // During Rapture, spam shields on everyone
        if (_raptureActive)
        {
            for (Unit* member : group)
            {
                if (member && !_shieldTracker.HasShield(member->GetGUID()))
                {
                    if (this->CanCastSpell(DISC_POWER_WORD_SHIELD, member))
                    {
                        this->CastSpell(member, DISC_POWER_WORD_SHIELD);
                        _shieldTracker.ApplyShield(member->GetGUID(), 15000);
                        _atonementTracker.ApplyAtonement(member->GetGUID(), 15000);
                        return true;
                    }
                }
            }
        }

        // Normal shielding for tanks and injured allies
        for (Unit* member : group)
        {
            if (member && (IsTankRole(member) || member->GetHealthPct() < 75.0f))
            {
                if (!_shieldTracker.HasShield(member->GetGUID()))
                {
                    if (this->CanCastSpell(DISC_POWER_WORD_SHIELD, member))
                    {
                        this->CastSpell(member, DISC_POWER_WORD_SHIELD);
                        _shieldTracker.ApplyShield(member->GetGUID(), 15000);
                        _atonementTracker.ApplyAtonement(member->GetGUID(), 15000);
                        return true;
                    }
                }
            }
        }

        return false;
    }

    bool HandleSelfHealing()
    {
        Player* bot = this->GetBot();
        if (!bot)
            return false;

        // Power Word: Shield
        if (!_shieldTracker.HasShield(bot->GetGUID()))
        {
            if (this->CanCastSpell(DISC_POWER_WORD_SHIELD, bot))
            {
                this->CastSpell(bot, DISC_POWER_WORD_SHIELD);
                _shieldTracker.ApplyShield(bot->GetGUID(), 15000);
                _atonementTracker.ApplyAtonement(bot->GetGUID(), 15000);
                return true;
            }
        }

        // Shadow Mend
        if (bot->GetHealthPct() < 60.0f)
        {
            if (this->CanCastSpell(DISC_SHADOW_MEND, bot))
            {
                this->CastSpell(bot, DISC_SHADOW_MEND);
                return true;
            }
        }

        // Penance (self-heal)
        if (bot->GetHealthPct() < 70.0f)
        {
            if (this->CanCastSpell(DISC_PENANCE, bot))
            {
                this->CastSpell(bot, DISC_PENANCE);
                return true;
            }
        }

        return false;
    }

    void ExecuteAtonementDamageRotation(::Unit* target)
    {
        Player* bot = this->GetBot();
        if (!bot || !target)
            return;

        // Deal damage to trigger Atonement healing (55% of damage done heals allies with Atonement)

        // Schism (damage amplification debuff)
        if (bot->HasSpell(DISC_SCHISM))
        {
            if (this->CanCastSpell(DISC_SCHISM, target))
            {
                this->CastSpell(target, DISC_SCHISM);
                return;
            }
        }

        // Mindgames (damage + healing reversal)
        if (bot->HasSpell(DISC_MINDGAMES))
        {
            if (this->CanCastSpell(DISC_MINDGAMES, target))
            {
                this->CastSpell(target, DISC_MINDGAMES);
                return;
            }
        }

        // Penance (offensive - high damage for Atonement)
        if (this->CanCastSpell(DISC_PENANCE, target))
        {
            this->CastSpell(target, DISC_PENANCE);
            return;
        }

        // Purge the Wicked (DoT - continuous Atonement healing)
        if (bot->HasSpell(DISC_PURGE_WICKED))
        {
            if (!target->HasAura(DISC_PURGE_WICKED))
            {
                if (this->CanCastSpell(DISC_PURGE_WICKED, target))
                {
                    this->CastSpell(target, DISC_PURGE_WICKED);
                    return;
                }
            }
        }
        else
        {
            // Shadow Word: Pain (alternative DoT)
            if (!target->HasAura(DISC_SHADOW_WORD_PAIN))
            {
                if (this->CanCastSpell(DISC_SHADOW_WORD_PAIN, target))
                {
                    this->CastSpell(target, DISC_SHADOW_WORD_PAIN);
                    return;
                }
            }
        }

        // Smite (filler)
        if (this->CanCastSpell(DISC_SMITE, target))
        {
            this->CastSpell(target, DISC_SMITE);
            return;
        }
    }

    [[nodiscard]] bool IsTankRole(Unit* unit) const
    {
        if (!unit)
            return false;

        if (Player* player = unit->ToPlayer())
        {
            // Simplified tank detection - check if player is currently tanking
            // A more robust implementation would check spec, but talent API is deprecated
            if (Unit* victim = player->GetVictim())
                return victim->GetVictim() == player;

            return false;
        }

        return false;
    }

    // Member variables
    AtonementTracker _atonementTracker;
    PowerWordShieldTracker _shieldTracker;

    bool _raptureActive;
    uint32 _raptureEndTime;

    uint32 _lastRaptureTime;
    uint32 _lastEvangelismTime;
    uint32 _lastBarrierTime;
    uint32 _lastPainSuppressionTime;
};

} // namespace Playerbot

#endif // PLAYERBOT_DISCIPLINEPRIESTREFACTORED_H
