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

#ifndef PLAYERBOT_RESTORATIONSHAMANREFACTORED_H
#define PLAYERBOT_RESTORATIONSHAMANREFACTORED_H

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

// WoW 11.2 (The War Within) - Restoration Shaman Spell IDs
constexpr uint32 REST_HEALING_WAVE = 77472;
constexpr uint32 REST_HEALING_SURGE = 8004;
constexpr uint32 REST_CHAIN_HEAL = 1064;
constexpr uint32 REST_RIPTIDE = 61295;
constexpr uint32 REST_HEALING_RAIN = 73920;
constexpr uint32 REST_WELLSPRING = 197995;
constexpr uint32 REST_HEALING_TIDE_TOTEM = 108280;
constexpr uint32 REST_CLOUDBURST_TOTEM = 157153;
constexpr uint32 REST_SPIRIT_LINK_TOTEM = 98008;
constexpr uint32 REST_EARTHEN_WALL_TOTEM = 198838;
constexpr uint32 REST_ANCESTRAL_PROTECTION_TOTEM = 207399;
constexpr uint32 REST_ASCENDANCE = 114052;
constexpr uint32 REST_UNLEASH_LIFE = 73685;
constexpr uint32 REST_EARTH_SHIELD = 974;
constexpr uint32 REST_WATER_SHIELD = 52127;
constexpr uint32 REST_PURIFY_SPIRIT = 77130;
constexpr uint32 REST_SPIRITWALKERS_GRACE = 79206;
constexpr uint32 REST_ASTRAL_SHIFT = 108271;
constexpr uint32 REST_WIND_SHEAR = 57994;

// ManaResource is already defined in CombatSpecializationTemplates.h
// No need to redefine it here

// Riptide HoT tracker
class RiptideTracker
{
public:
    RiptideTracker() = default;

    void ApplyRiptide(ObjectGuid guid, uint32 duration = 18000)
    {
        _riptideTargets[guid] = getMSTime() + duration;
    }

    void RemoveRiptide(ObjectGuid guid)
    {
        _riptideTargets.erase(guid);
    }

    [[nodiscard]] bool HasRiptide(ObjectGuid guid) const
    {
        auto it = _riptideTargets.find(guid);
        if (it == _riptideTargets.end())
            return false;
        return getMSTime() < it->second;
    }

    [[nodiscard]] uint32 GetRiptideTimeRemaining(ObjectGuid guid) const
    {
        auto it = _riptideTargets.find(guid);
        if (it == _riptideTargets.end())
            return 0;

        uint32 now = getMSTime();
        if (now >= it->second)
            return 0;

        return it->second - now;
    }

    [[nodiscard]] bool NeedsRiptideRefresh(ObjectGuid guid, uint32 pandemicWindow = 5400) const
    {
        uint32 remaining = GetRiptideTimeRemaining(guid);
        return remaining < pandemicWindow;
    }

    [[nodiscard]] uint32 GetActiveRiptideCount() const
    {
        uint32 count = 0;
        uint32 now = getMSTime();
        for (const auto& pair : _riptideTargets)
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

        // Clean up expired Riptides
        uint32 now = getMSTime();
        for (auto it = _riptideTargets.begin(); it != _riptideTargets.end();)
        {
            if (now >= it->second)
                it = _riptideTargets.erase(it);
            else
                ++it;
        }
    }

private:
    std::unordered_map<ObjectGuid, uint32> _riptideTargets; // GUID -> expiration time
};

// Earth Shield tracker
class EarthShieldTracker
{
public:
    EarthShieldTracker() = default;

    void ApplyEarthShield(ObjectGuid guid, uint32 duration = 600000)
    {
        _earthShieldTarget = guid;
        _earthShieldEndTime = getMSTime() + duration;
    }

    void RemoveEarthShield()
    {
        _earthShieldTarget = ObjectGuid::Empty;
    }

    [[nodiscard]] bool HasEarthShield(ObjectGuid guid) const
    {
        return _earthShieldTarget == guid && getMSTime() < _earthShieldEndTime;
    }

    [[nodiscard]] ObjectGuid GetEarthShieldTarget() const { return _earthShieldTarget; }

    [[nodiscard]] bool NeedsEarthShieldRefresh(uint32 refreshWindow = 60000) const
    {
        if (_earthShieldTarget.IsEmpty())
            return true;

        uint32 now = getMSTime();
        if (now >= _earthShieldEndTime)
            return true;

        return (_earthShieldEndTime - now) < refreshWindow;
    }

    void Update(Player* bot)
    {
        if (!bot)
            return;

        // Check if Earth Shield expired
        if (!_earthShieldTarget.IsEmpty() && getMSTime() >= _earthShieldEndTime)
        {
            _earthShieldTarget = ObjectGuid::Empty;
        }
    }

private:
    ObjectGuid _earthShieldTarget;
    uint32 _earthShieldEndTime = 0;
};

class RestorationShamanRefactored : public HealerSpecialization<ManaResource>, public ShamanSpecialization
{
public:
    using Base = HealerSpecialization<ManaResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::_resource;
    explicit RestorationShamanRefactored(Player* bot)
        : HealerSpecialization<ManaResource>(bot)
        , ShamanSpecialization(bot)
        , _riptideTracker()
        , _earthShieldTracker()
        , _ascendanceActive(false)
        , _ascendanceEndTime(0)
        , _lastAscendanceTime(0)
        , _lastHealingTideTotemTime(0)
        , _lastSpiritLinkTotemTime(0)
        , _lastCloudburstTotemTime(0)
        , _lastEarthenWallTotemTime(0)
        , _lastAncestralProtectionTotemTime(0)
    {
        // Resource initialization handled by base class CombatSpecializationTemplate
        InitializeCooldowns();
        TC_LOG_DEBUG("playerbot", "RestorationShamanRefactored initialized for {}", bot->GetName());
    }

    void UpdateRotation(::Unit* target) override
    {
        Player* bot = this->GetBot();
        if (!target || !bot)
            return;

        UpdateRestorationState();

        // Restoration is a healer - check group health first
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

        // Deal damage when no healing is needed (maintain mana)
        ExecuteDamageRotation(target);
    }

    void UpdateBuffs() override
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        // Water Shield (mana regeneration)
        if (!bot->HasAura(REST_WATER_SHIELD))
        {
            if (this->CanCastSpell(REST_WATER_SHIELD, bot))
            {
                this->CastSpell(bot, REST_WATER_SHIELD);
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
        if (healthPct < 40.0f && this->CanCastSpell(REST_ASTRAL_SHIFT, bot))
        {
            this->CastSpell(bot, REST_ASTRAL_SHIFT);
            return;
        }

        // Spiritwalker's Grace (heal while moving)
        if (healthPct < 60.0f && bot->HasUnitMovementFlag(MOVEMENTFLAG_FORWARD))
        {
            if (this->CanCastSpell(REST_SPIRITWALKERS_GRACE, bot))
            {
                this->CastSpell(bot, REST_SPIRITWALKERS_GRACE);
                return;
            }
        }
    }

private:
    void InitializeCooldowns()
    {
        _lastAscendanceTime = 0;
        _lastHealingTideTotemTime = 0;
        _lastSpiritLinkTotemTime = 0;
        _lastCloudburstTotemTime = 0;
        _lastEarthenWallTotemTime = 0;
        _lastAncestralProtectionTotemTime = 0;
    }

    void UpdateRestorationState()
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        // ManaResource (uint32) doesn't have Update method - handled by base class
        _riptideTracker.Update(bot);
        _earthShieldTracker.Update(bot);
        UpdateCooldownStates();
    }

    void UpdateCooldownStates()
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        // Ascendance state (transform into Water Ascendant)
        if (_ascendanceActive && getMSTime() >= _ascendanceEndTime)
            _ascendanceActive = false;

        if (bot->HasAura(REST_ASCENDANCE))
        {
            _ascendanceActive = true;
            if (Aura* aura = bot->GetAura(REST_ASCENDANCE))
                _ascendanceEndTime = getMSTime() + aura->GetDuration();
        }
    }

    bool HandleGroupHealing(const std::vector<Unit*>& group)
    {
        // Emergency cooldowns
        if (HandleEmergencyCooldowns(group))
            return true;

        // Maintain HoTs and shields
        if (HandleHoTs(group))
            return true;

        // AoE healing
        if (HandleAoEHealing(group))
            return true;

        // Direct healing
        if (HandleDirectHealing(group))
            return true;

        return false;
    }

    bool HandleEmergencyCooldowns(const std::vector<Unit*>& group)
    {
        Player* bot = this->GetBot();
        if (!bot)
            return false;

        // Ancestral Protection Totem (resurrect on death)
        uint32 criticalHealthCount = 0;
        for (Unit* member : group)
        {
            if (member && member->GetHealthPct() < 20.0f)
                ++criticalHealthCount;
        }

        if (criticalHealthCount >= 2 && (getMSTime() - _lastAncestralProtectionTotemTime) >= 300000) // 5 min CD
        {
            if (bot->HasSpell(REST_ANCESTRAL_PROTECTION_TOTEM))
            {
                if (this->CanCastSpell(REST_ANCESTRAL_PROTECTION_TOTEM, bot))
                {
                    this->CastSpell(bot, REST_ANCESTRAL_PROTECTION_TOTEM);
                    _lastAncestralProtectionTotemTime = getMSTime();
                    return true;
                }
            }
        }

        // Healing Tide Totem (massive raid healing)
        uint32 lowHealthCount = 0;
        for (Unit* member : group)
        {
            if (member && member->GetHealthPct() < 60.0f)
                ++lowHealthCount;
        }

        if (lowHealthCount >= 4 && (getMSTime() - _lastHealingTideTotemTime) >= 180000) // 3 min CD
        {
            if (this->CanCastSpell(REST_HEALING_TIDE_TOTEM, bot))
            {
                this->CastSpell(bot, REST_HEALING_TIDE_TOTEM);
                _lastHealingTideTotemTime = getMSTime();
                return true;
            }
        }

        // Spirit Link Totem (equalize health)
        if (lowHealthCount >= 3 && (getMSTime() - _lastSpiritLinkTotemTime) >= 180000) // 3 min CD
        {
            if (this->CanCastSpell(REST_SPIRIT_LINK_TOTEM, bot))
            {
                this->CastSpell(bot, REST_SPIRIT_LINK_TOTEM);
                _lastSpiritLinkTotemTime = getMSTime();
                return true;
            }
        }

        // Ascendance (healing burst mode)
        if (lowHealthCount >= 3 && (getMSTime() - _lastAscendanceTime) >= 180000) // 3 min CD
        {
            if (this->CanCastSpell(REST_ASCENDANCE, bot))
            {
                this->CastSpell(bot, REST_ASCENDANCE);
                _ascendanceActive = true;
                _ascendanceEndTime = getMSTime() + 15000;
                _lastAscendanceTime = getMSTime();
                return true;
            }
        }

        // Earthen Wall Totem (shield wall)
        if (lowHealthCount >= 3 && (getMSTime() - _lastEarthenWallTotemTime) >= 60000) // 60 sec CD
        {
            if (bot->HasSpell(REST_EARTHEN_WALL_TOTEM))
            {
                if (this->CanCastSpell(REST_EARTHEN_WALL_TOTEM, bot))
                {
                    this->CastSpell(bot, REST_EARTHEN_WALL_TOTEM);
                    _lastEarthenWallTotemTime = getMSTime();
                    return true;
                }
            }
        }

        return false;
    }

    bool HandleHoTs(const std::vector<Unit*>& group)
    {
        Player* bot = this->GetBot();
        if (!bot)
            return false;

        uint32 activeRiptides = _riptideTracker.GetActiveRiptideCount();

        // Earth Shield on tank
        Unit* tankTarget = nullptr;
        for (Unit* member : group)
        {
            if (member && IsTankRole(member))
            {
                tankTarget = member;
                break;
            }
        }

        if (tankTarget && !_earthShieldTracker.HasEarthShield(tankTarget->GetGUID()))
        {
            if (this->CanCastSpell(REST_EARTH_SHIELD, tankTarget))
            {
                this->CastSpell(tankTarget, REST_EARTH_SHIELD);
                _earthShieldTracker.ApplyEarthShield(tankTarget->GetGUID(), 600000);
                return true;
            }
        }

        // Riptide on injured allies
        if (activeRiptides < group.size())
        {
            for (Unit* member : group)
            {
                if (member && member->GetHealthPct() < 90.0f)
                {
                    if (_riptideTracker.NeedsRiptideRefresh(member->GetGUID()))
                    {
                        if (this->CanCastSpell(REST_RIPTIDE, member))
                        {
                            this->CastSpell(member, REST_RIPTIDE);
                            _riptideTracker.ApplyRiptide(member->GetGUID(), 18000);
                            return true;
                        }
                    }
                }
            }
        }

        return false;
    }

    bool HandleAoEHealing(const std::vector<Unit*>& group)
    {
        Player* bot = this->GetBot();
        if (!bot)
            return false;

        // Healing Rain (ground AoE HoT)
        uint32 stackedAlliesCount = 0;
        Unit* stackedTarget = nullptr;

        for (Unit* member : group)
        {
            if (member && member->GetHealthPct() < 80.0f)
            {
                // Count nearby allies to this member
                uint32 nearbyCount = 0;
                for (Unit* other : group)
                {
                    if (other && member->GetDistance(other) <= 10.0f && other->GetHealthPct() < 80.0f)
                        ++nearbyCount;
                }

                if (nearbyCount > stackedAlliesCount)
                {
                    stackedAlliesCount = nearbyCount;
                    stackedTarget = member;
                }
            }
        }

        if (stackedAlliesCount >= 3 && stackedTarget)
        {
            if (this->CanCastSpell(REST_HEALING_RAIN, stackedTarget))
            {
                this->CastSpell(stackedTarget, REST_HEALING_RAIN);
                return true;
            }
        }

        // Wellspring (instant AoE heal)
        if (stackedAlliesCount >= 4 && stackedTarget)
        {
            if (bot->HasSpell(REST_WELLSPRING))
            {
                if (this->CanCastSpell(REST_WELLSPRING, stackedTarget))
                {
                    this->CastSpell(stackedTarget, REST_WELLSPRING);
                    return true;
                }
            }
        }

        // Chain Heal (bouncing heal)
        uint32 injuredCount = 0;
        for (Unit* member : group)
        {
            if (member && member->GetHealthPct() < 80.0f)
                ++injuredCount;
        }

        if (injuredCount >= 2)
        {
            for (Unit* member : group)
            {
                if (member && member->GetHealthPct() < 75.0f)
                {
                    if (this->CanCastSpell(REST_CHAIN_HEAL, member))
                    {
                        this->CastSpell(member, REST_CHAIN_HEAL);
                        return true;
                    }
                }
            }
        }

        // Cloudburst Totem (store healing and release)
        if (injuredCount >= 3 && (getMSTime() - _lastCloudburstTotemTime) >= 30000) // 30 sec CD
        {
            if (bot->HasSpell(REST_CLOUDBURST_TOTEM))
            {
                if (this->CanCastSpell(REST_CLOUDBURST_TOTEM, bot))
                {
                    this->CastSpell(bot, REST_CLOUDBURST_TOTEM);
                    _lastCloudburstTotemTime = getMSTime();
                    return true;
                }
            }
        }

        return false;
    }

    bool HandleDirectHealing(const std::vector<Unit*>& group)
    {
        // Healing Surge for emergency
        for (Unit* member : group)
        {
            if (member && member->GetHealthPct() < 50.0f)
            {
                if (this->CanCastSpell(REST_HEALING_SURGE, member))
                {
                    this->CastSpell(member, REST_HEALING_SURGE);
                    return true;
                }
            }
        }

        // Healing Wave (efficient single-target)
        for (Unit* member : group)
        {
            if (member && member->GetHealthPct() < 80.0f)
            {
                if (this->CanCastSpell(REST_HEALING_WAVE, member))
                {
                    this->CastSpell(member, REST_HEALING_WAVE);
                    return true;
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

        // Riptide
        if (_riptideTracker.NeedsRiptideRefresh(bot->GetGUID()))
        {
            if (this->CanCastSpell(REST_RIPTIDE, bot))
            {
                this->CastSpell(bot, REST_RIPTIDE);
                _riptideTracker.ApplyRiptide(bot->GetGUID(), 18000);
                return true;
            }
        }

        // Healing Surge
        if (bot->GetHealthPct() < 60.0f)
        {
            if (this->CanCastSpell(REST_HEALING_SURGE, bot))
            {
                this->CastSpell(bot, REST_HEALING_SURGE);
                return true;
            }
        }

        // Healing Wave
        if (bot->GetHealthPct() < 80.0f)
        {
            if (this->CanCastSpell(REST_HEALING_WAVE, bot))
            {
                this->CastSpell(bot, REST_HEALING_WAVE);
                return true;
            }
        }

        return false;
    }

    void ExecuteDamageRotation(::Unit* target)
    {
        // Restoration Shaman has minimal damage rotation
        // Just maintain mana and deal minimal damage when no healing needed
    }

    [[nodiscard]] bool IsTankRole(Unit* unit) const
    {
        if (!unit)
            return false;

        if (Player* player = unit->ToPlayer())
        {
            // Check if player has tank role based on class
            // Protection Paladin, Protection Warrior, Blood DK, Guardian Druid, Brewmaster Monk, Vengeance DH
            uint8 playerClass = player->GetClass();
            return (playerClass == CLASS_WARRIOR || playerClass == CLASS_PALADIN ||
                    playerClass == CLASS_DEATH_KNIGHT || playerClass == CLASS_DRUID ||
                    playerClass == CLASS_MONK || playerClass == CLASS_DEMON_HUNTER);
        }

        return false;
    }

    // Member variables
    RiptideTracker _riptideTracker;
    EarthShieldTracker _earthShieldTracker;

    bool _ascendanceActive;
    uint32 _ascendanceEndTime;

    uint32 _lastAscendanceTime;
    uint32 _lastHealingTideTotemTime;
    uint32 _lastSpiritLinkTotemTime;
    uint32 _lastCloudburstTotemTime;
    uint32 _lastEarthenWallTotemTime;
    uint32 _lastAncestralProtectionTotemTime;
};

} // namespace Playerbot

#endif // PLAYERBOT_RESTORATIONSHAMANREFACTORED_H
