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

#ifndef PLAYERBOT_PRESERVATIONEVOKERREFACTORED_H
#define PLAYERBOT_PRESERVATIONEVOKERREFACTORED_H

#include "../CombatSpecializationTemplates.h"
#include "Player.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Group.h"
#include <unordered_map>
#include <vector>
#include "Log.h"
#include "EvokerSpecialization.h"

namespace Playerbot
{

// WoW 11.2 (The War Within) - Preservation Evoker Spell IDs
constexpr uint32 PRES_LIVING_FLAME = 361469;
constexpr uint32 PRES_EMERALD_BLOSSOM = 355913;
constexpr uint32 PRES_VERDANT_EMBRACE = 360995;
constexpr uint32 PRES_REVERSION = 366155;
constexpr uint32 PRES_SPIRITBLOOM = 367226;
constexpr uint32 PRES_DREAM_BREATH = 355936;
constexpr uint32 PRES_ECHO = 364343;
constexpr uint32 PRES_TIME_DILATION = 357170;
constexpr uint32 PRES_REWIND = 363534;
constexpr uint32 PRES_DREAM_FLIGHT = 359816;
constexpr uint32 PRES_TEMPORAL_ANOMALY = 373862;
constexpr uint32 PRES_ESSENCE_BURST = 369299; // Buff proc
constexpr uint32 PRES_LIFEBIND = 373267; // Buff
constexpr uint32 PRES_STASIS = 370537;
constexpr uint32 PRES_OBSIDIAN_SCALES = 363916;
constexpr uint32 PRES_RENEWING_BLAZE = 374348;

// Mana and Essence resource (Evokers use dual resource)
struct ManaEssenceResourcePres
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

// HoT tracking system for Preservation
class PreservationHoTTracker
{
public:
    PreservationHoTTracker() = default;

    void ApplyReversion(ObjectGuid guid, uint32 duration = 12000)
    {
        _reversionTargets[guid] = getMSTime() + duration;
    }

    void ApplyEcho(ObjectGuid guid, uint32 duration = 15000)
    {
        _echoTargets[guid] = getMSTime() + duration;
    }

    void ApplyDreamBreath(ObjectGuid guid, uint32 duration = 20000)
    {
        _dreamBreathTargets[guid] = getMSTime() + duration;
    }

    [[nodiscard]] bool HasReversion(ObjectGuid guid) const
    {
        auto it = _reversionTargets.find(guid);
        return it != _reversionTargets.end() && getMSTime() < it->second;
    }

    [[nodiscard]] bool HasEcho(ObjectGuid guid) const
    {
        auto it = _echoTargets.find(guid);
        return it != _echoTargets.end() && getMSTime() < it->second;
    }

    [[nodiscard]] bool HasDreamBreath(ObjectGuid guid) const
    {
        auto it = _dreamBreathTargets.find(guid);
        return it != _dreamBreathTargets.end() && getMSTime() < it->second;
    }

    [[nodiscard]] uint32 GetReversionTimeRemaining(ObjectGuid guid) const
    {
        auto it = _reversionTargets.find(guid);
        if (it != _reversionTargets.end())
        {
            uint32 now = getMSTime();
            return now < it->second ? (it->second - now) : 0;
        }
        return 0;
    }

    [[nodiscard]] bool NeedsReversionRefresh(ObjectGuid guid, uint32 pandemicWindow = 3600) const
    {
        uint32 remaining = GetReversionTimeRemaining(guid);
        return remaining < pandemicWindow; // Refresh in pandemic window
    }

    [[nodiscard]] uint32 GetActiveReversionCount() const
    {
        uint32 count = 0;
        uint32 now = getMSTime();
        for (const auto& pair : _reversionTargets)
        {
            if (now < pair.second)
                ++count;
        }
        return count;
    }

    void Update(const std::vector<Unit*>& group)
    {
        for (Unit* member : group)
        {
            if (!member)
                continue;

            ObjectGuid guid = member->GetGUID();

            // Sync with actual auras
            if (Aura* reversion = member->GetAura(PRES_REVERSION))
                _reversionTargets[guid] = getMSTime() + reversion->GetDuration();
            else
                _reversionTargets.erase(guid);

            if (Aura* echo = member->GetAura(PRES_ECHO))
                _echoTargets[guid] = getMSTime() + echo->GetDuration();
            else
                _echoTargets.erase(guid);

            if (Aura* dreamBreath = member->GetAura(PRES_DREAM_BREATH))
                _dreamBreathTargets[guid] = getMSTime() + dreamBreath->GetDuration();
            else
                _dreamBreathTargets.erase(guid);
        }
    }

private:
    std::unordered_map<ObjectGuid, uint32> _reversionTargets;
    std::unordered_map<ObjectGuid, uint32> _echoTargets;
    std::unordered_map<ObjectGuid, uint32> _dreamBreathTargets;
};

// Essence Burst proc tracker (free healing spells)
class PreservationEssenceBurstTracker
{
public:
    PreservationEssenceBurstTracker() : _essenceBurstStacks(0), _essenceBurstEndTime(0) {}

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

        if (Aura* aura = bot->GetAura(PRES_ESSENCE_BURST))
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

class PreservationEvokerRefactored : public HealerSpecialization<ManaEssenceResourcePres>, public EvokerSpecialization
{
public:
    using Base = HealerSpecialization<ManaEssenceResourcePres>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::_resource;
    explicit PreservationEvokerRefactored(Player* bot)
        : HealerSpecialization<ManaEssenceResourcePres>(bot)
        , EvokerSpecialization(bot)
        , _hotTracker()
        , _essenceBurstTracker()
        , _lastDreamFlightTime(0)
        , _lastRewindTime(0)
        , _lastTimeDilationTime(0)
    {
        this->_resource.Initialize(bot);
        InitializeCooldowns();
        TC_LOG_DEBUG("playerbot", "PreservationEvokerRefactored initialized for {}", bot->GetName());
    }

    void UpdateRotation(::Unit* target) override
    {
        if (!bot)
            return;

        UpdatePreservationState();

        std::vector<Unit*> group = GetGroupMembers();
        if (group.empty())
            group.push_back(bot); // Solo healing (self)

        ExecuteHealingRotation(group);
    }

    void UpdateBuffs() override
    {
        if (!bot)
            return;

        // Echo on tank (buff that duplicates healing)
        Unit* tank = GetMainTank(GetGroupMembers());
        if (tank && !_hotTracker.HasEcho(tank->GetGUID()))
        {
            if (this->CanCastSpell(PRES_ECHO, tank))
            {
                this->CastSpell(PRES_ECHO, tank);
                _hotTracker.ApplyEcho(tank->GetGUID(), 15000);
            }
        }
    }

    void UpdateDefensives() override
    {
        if (!bot)
            return;

        float healthPct = bot->GetHealthPct();

        // Obsidian Scales (personal DR)
        if (healthPct < 50.0f && this->CanCastSpell(PRES_OBSIDIAN_SCALES, bot))
        {
            this->CastSpell(bot, PRES_OBSIDIAN_SCALES);
            return;
        }

        // Renewing Blaze (self-heal)
        if (healthPct < 40.0f && this->CanCastSpell(PRES_RENEWING_BLAZE, bot))
        {
            this->CastSpell(bot, PRES_RENEWING_BLAZE);
            return;
        }
    }

private:
    void InitializeCooldowns()
    {
        _lastDreamFlightTime = 0;
        _lastRewindTime = 0;
        _lastTimeDilationTime = 0;
    }

    void UpdatePreservationState()
    {
        this->_resource.Update(bot);
        _essenceBurstTracker.Update(bot);
    }

    void ExecuteHealingRotation(const std::vector<Unit*>& group)
    {
        _hotTracker.Update(group);

        // Emergency group-wide healing
        if (HandleEmergencyHealing(group))
            return;

        // Maintain Reversion HoTs
        if (HandleReversion(group))
            return;

        // Dream Breath for AoE healing
        if (HandleDreamBreath(group))
            return;

        // Spiritbloom for single-target healing
        if (HandleSpiritbloom(group))
            return;

        // Emerald Blossom for AoE spot healing
        if (HandleEmeraldBlossom(group))
            return;

        // Verdant Embrace for quick single-target heal
        if (HandleVerdantEmbrace(group))
            return;

        // Time Dilation (extend HoT durations)
        if (HandleTimeDilation(group))
            return;

        // DPS rotation when no healing needed
        HandleDPSRotation();
    }

    bool HandleEmergencyHealing(const std::vector<Unit*>& group)
    {
        // Count critically injured allies
        uint32 criticalCount = 0;
        for (Unit* member : group)
        {
            if (member && member->GetHealthPct() < 40.0f)
                ++criticalCount;
        }

        // Dream Flight (raid-wide emergency healing)
        if (criticalCount >= 3 && (getMSTime() - _lastDreamFlightTime) >= 120000) // 2 min CD
        {
            if (this->CanCastSpell(PRES_DREAM_FLIGHT, bot))
            {
                this->CastSpell(bot, PRES_DREAM_FLIGHT);
                _lastDreamFlightTime = getMSTime();
                return true;
            }
        }

        // Rewind (heal allies back in time)
        if (criticalCount >= 2 && (getMSTime() - _lastRewindTime) >= 240000) // 4 min CD
        {
            Unit* target = GetGroupMemberNeedingHealing(group, 40.0f);
            if (target && this->CanCastSpell(PRES_REWIND, target))
            {
                this->CastSpell(target, PRES_REWIND);
                _lastRewindTime = getMSTime();
                return true;
            }
        }

        // Stasis (store health, release later)
        if (criticalCount >= 1)
        {
            Unit* target = GetGroupMemberNeedingHealing(group, 30.0f);
            if (target && this->CanCastSpell(PRES_STASIS, target))
            {
                this->CastSpell(target, PRES_STASIS);
                return true;
            }
        }

        return false;
    }

    bool HandleReversion(const std::vector<Unit*>& group)
    {
        uint32 activeReversions = _hotTracker.GetActiveReversionCount();

        // Maintain Reversion on 3-4 allies
        if (activeReversions < 4)
        {
            for (Unit* member : group)
            {
                if (member && member->GetHealthPct() < 95.0f && _hotTracker.NeedsReversionRefresh(member->GetGUID()))
                {
                    if (this->CanCastSpell(PRES_REVERSION, member))
                    {
                        this->CastSpell(PRES_REVERSION, member);
                        _hotTracker.ApplyReversion(member->GetGUID(), 12000);
                        GenerateEssence(1);
                        return true;
                    }
                }
            }
        }

        return false;
    }

    bool HandleDreamBreath(const std::vector<Unit*>& group)
    {
        // Count injured allies
        uint32 needsHealing = 0;
        for (Unit* member : group)
        {
            if (member && member->GetHealthPct() < 80.0f)
                ++needsHealing;
        }

        // Use Dream Breath if 3+ allies need healing
        if (needsHealing >= 3 && this->_resource.HasEssence(3))
        {
            Unit* target = GetGroupMemberNeedingHealing(group, 80.0f);
            if (target && this->CanCastSpell(PRES_DREAM_BREATH, target))
            {
                this->CastSpell(target, PRES_DREAM_BREATH);
                ConsumeEssence(3);
                // Apply to all nearby allies (simplified)
                for (Unit* member : group)
                {
                    if (member)
                        _hotTracker.ApplyDreamBreath(member->GetGUID(), 20000);
                }
                return true;
            }
        }

        return false;
    }

    bool HandleSpiritbloom(const std::vector<Unit*>& group)
    {
        // Use Spiritbloom for heavy single-target healing
        Unit* target = GetGroupMemberNeedingHealing(group, 60.0f);
        if (target && this->_resource.HasEssence(3))
        {
            // Empowered with Essence Burst for free cast
            if (_essenceBurstTracker.IsActive())
            {
                if (this->CanCastSpell(PRES_SPIRITBLOOM, target))
                {
                    this->CastSpell(target, PRES_SPIRITBLOOM);
                    _essenceBurstTracker.ConsumeProc();
                    return true;
                }
            }
            // Normal cast
            else if (this->CanCastSpell(PRES_SPIRITBLOOM, target))
            {
                this->CastSpell(target, PRES_SPIRITBLOOM);
                ConsumeEssence(3);
                return true;
            }
        }

        return false;
    }

    bool HandleEmeraldBlossom(const std::vector<Unit*>& group)
    {
        // Count injured allies
        uint32 needsHealing = 0;
        for (Unit* member : group)
        {
            if (member && member->GetHealthPct() < 85.0f)
                ++needsHealing;
        }

        // Use Emerald Blossom if 2+ allies need healing
        if (needsHealing >= 2 && this->_resource.HasEssence(2))
        {
            if (this->CanCastSpell(PRES_EMERALD_BLOSSOM, bot))
            {
                this->CastSpell(bot, PRES_EMERALD_BLOSSOM);
                ConsumeEssence(2);
                return true;
            }
        }

        return false;
    }

    bool HandleVerdantEmbrace(const std::vector<Unit*>& group)
    {
        // Quick single-target heal
        Unit* target = GetGroupMemberNeedingHealing(group, 70.0f);
        if (target && this->CanCastSpell(PRES_VERDANT_EMBRACE, target))
        {
            this->CastSpell(target, PRES_VERDANT_EMBRACE);
            return true;
        }

        return false;
    }

    bool HandleTimeDilation(const std::vector<Unit*>& group)
    {
        // Extend HoT durations if multiple HoTs are active
        if (_hotTracker.GetActiveReversionCount() >= 3 && (getMSTime() - _lastTimeDilationTime) >= 60000) // 1 min CD
        {
            if (this->CanCastSpell(PRES_TIME_DILATION, bot))
            {
                this->CastSpell(bot, PRES_TIME_DILATION);
                _lastTimeDilationTime = getMSTime();
                return true;
            }
        }

        return false;
    }

    void HandleDPSRotation()
    {
        // When no healing is needed, contribute damage
        Unit* target = bot->GetVictim();
        if (!target)
            target = FindNearbyEnemy();

        if (target)
        {
            // Living Flame for damage
            if (this->CanCastSpell(PRES_LIVING_FLAME, target))
            {
                this->CastSpell(target, PRES_LIVING_FLAME);
                GenerateEssence(1);
            }
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

    [[nodiscard]] std::vector<Unit*> GetGroupMembers() const
    {
        std::vector<Unit*> members;

        if (!bot)
            return members;

        Group* group = bot->GetGroup();
        if (!group)
            return members;

        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            if (Player* member = ref->GetSource())
            {
                if (member->IsInWorld() && bot->GetDistance(member) <= 40.0f)
                    members.push_back(member);
            }
        }

        return members;
    }

    [[nodiscard]] Unit* GetGroupMemberNeedingHealing(const std::vector<Unit*>& group, float healthThreshold) const
    {
        Unit* mostInjured = nullptr;
        float lowestHealth = 100.0f;

        for (Unit* member : group)
        {
            if (member && member->GetHealthPct() < healthThreshold && member->GetHealthPct() < lowestHealth)
            {
                mostInjured = member;
                lowestHealth = member->GetHealthPct();
            }
        }

        return mostInjured;
    }

    [[nodiscard]] Unit* GetMainTank(const std::vector<Unit*>& group) const
    {
        // Find the group member with tank role or highest threat
        for (Unit* member : group)
        {
            if (member && IsTank(member))
                return member;
        }

        return !group.empty() ? group[0] : nullptr;
    }

    [[nodiscard]] bool IsTank(Unit* unit) const
    {
        if (!unit)
            return false;

        // Check if player has tank spec/role
        if (Player* player = unit->ToPlayer())
        {
            // Simplified tank detection - in real implementation, check spec and role
            return player->GetPrimaryTalentTree(player->GetActiveSpec()) == TALENT_TREE_PROTECTION;
        }

        return false;
    }

    [[nodiscard]] Unit* FindNearbyEnemy() const
    {
        // Simplified enemy finding - in real implementation, query nearby hostile units
        return bot ? bot->GetVictim() : nullptr;
    }

    // Member variables
    PreservationHoTTracker _hotTracker;
    PreservationEssenceBurstTracker _essenceBurstTracker;

    uint32 _lastDreamFlightTime;
    uint32 _lastRewindTime;
    uint32 _lastTimeDilationTime;
};

} // namespace Playerbot

#endif // PLAYERBOT_PRESERVATIONEVOKERREFACTORED_H
