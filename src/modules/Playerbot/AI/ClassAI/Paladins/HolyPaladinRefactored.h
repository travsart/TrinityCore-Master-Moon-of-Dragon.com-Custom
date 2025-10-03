/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * Holy Paladin Refactored - Template-Based Implementation
 *
 * This file provides a complete, template-based implementation of Holy Paladin
 * using the HealerSpecialization with dual resource system (Mana + Holy Power).
 */

#pragma once

#include "../CombatSpecializationTemplates.h"
#include "../ResourceTypes.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellAuraEffects.h"
#include "Group.h"
#include "Log.h"
#include "PaladinSpecialization.h"

namespace Playerbot
{

// ============================================================================
// HOLY PALADIN SPELL IDs (WoW 11.2 - The War Within)
// ============================================================================

enum HolyPaladinSpells
{
    // Holy Power Generators
    HOLY_SHOCK               = 20473,   // 16% mana, 1 HP, 7.5 sec CD
    CRUSADER_STRIKE_HOLY     = 35395,   // 9% mana, 1 HP, generates HP for healing
    JUDGMENT_HOLY            = 275773,  // 3% mana, 12 sec CD

    // Holy Power Spenders
    WORD_OF_GLORY            = 85673,   // 3 HP, instant heal
    LIGHT_OF_DAWN            = 85222,   // 3 HP, AoE cone heal
    SHIELD_OF_THE_RIGHTEOUS_HOLY = 53600, // 3 HP, defensive

    // Direct Heals
    FLASH_OF_LIGHT           = 19750,   // 22% mana, fast heal
    HOLY_LIGHT               = 82326,   // 15% mana, efficient heal
    DIVINE_TOLL              = 375576,  // 1 min CD, instant HP generation

    // AoE Heals
    LIGHT_OF_THE_MARTYR      = 183998,  // 7% mana, self-damage heal
    BEACON_OF_LIGHT          = 53563,   // Transfer healing to beacon target
    BEACON_OF_FAITH          = 156910,  // Second beacon (talent)
    BEACON_OF_VIRTUE         = 200025,  // Short duration AoE beacon (talent)

    // Cooldowns
    AVENGING_WRATH_HOLY      = 31842,   // 2 min CD, healing/damage buff
    AVENGING_CRUSADER        = 216331,  // 2 min CD, healing/damage hybrid (talent)
    HOLY_AVENGER             = 105809,  // 2 min CD, HP generation boost (talent)
    DIVINE_PROTECTION        = 498,     // 5 min CD, 20% damage reduction
    BLESSING_OF_SACRIFICE    = 6940,    // 2 min CD, redirect damage to self

    // Utility
    CLEANSE                  = 4987,    // Dispel diseases/poisons
    BLESSING_OF_FREEDOM      = 1044,    // Remove movement impairment
    BLESSING_OF_PROTECTION   = 1022,    // Immunity to physical damage
    LAY_ON_HANDS             = 633,     // 10 min CD, full heal
    DIVINE_SHIELD            = 642,     // 5 min CD, immunity

    // Buffs/Auras
    INFUSION_OF_LIGHT        = 54149,   // Proc: reduced Flash of Light cast time
    GLIMMER_OF_LIGHT         = 325966,  // HoT from Holy Shock
    AURA_MASTERY             = 31821,   // 3 min CD, strengthen auras

    // Auras
    DEVOTION_AURA            = 465,     // Armor buff
    CONCENTRATION_AURA       = 317920,  // Interrupt resistance
    RETRIBUTION_AURA         = 183435,  // Damage reflect

    // Talents
    DIVINE_FAVOR             = 210294,  // Free, instant Holy Light proc
    AWAKENING                = 248033,  // Avenging Wrath CDR
    UNBREAKABLE_SPIRIT       = 114154   // Reduced defensive CDs
};

// Dual resource type for Paladin - defined once with include guard
#ifndef PLAYERBOT_RESOURCE_TYPES_MANA_HOLY_POWER
#define PLAYERBOT_RESOURCE_TYPES_MANA_HOLY_POWER
struct ManaHolyPowerResource
{
    uint32 mana{0};
    uint32 holyPower{0};
    uint32 maxMana{100000};
    uint32 maxHolyPower{5};

    bool available{true};
    bool Consume(uint32 manaCost) {
        if (mana >= manaCost) {
            mana -= manaCost;
            return true;
        }
        return false;
    }
    void Regenerate(uint32 diff) {
        // Resource regeneration logic (simplified)
        available = true;
    }

    [[nodiscard]] uint32 GetAvailable() const {
        return 100; // Simplified - override in specific implementations
    }

    [[nodiscard]] uint32 GetMax() const {
        return 100; // Simplified - override in specific implementations
    }


    void Initialize(Player* bot) {
        if (bot) {
            maxMana = bot->GetMaxPower(POWER_MANA);
            mana = bot->GetPower(POWER_MANA);
        }
        holyPower = 0;
    }
};
#endif // PLAYERBOT_RESOURCE_TYPES_MANA_HOLY_POWER

// ============================================================================
// HOLY PALADIN BEACON TRACKER
// ============================================================================

class HolyPaladinBeaconTracker
{
public:
    HolyPaladinBeaconTracker()
        : _primaryBeaconGuid()
        , _secondaryBeaconGuid()
        , _hasBeaconOfFaith(false)
    {
    }

    void SetPrimaryBeacon(ObjectGuid guid)
    {
        _primaryBeaconGuid = guid;
    }

    void SetSecondaryBeacon(ObjectGuid guid)
    {
        if (_hasBeaconOfFaith)
            _secondaryBeaconGuid = guid;
    }

    bool HasPrimaryBeacon() const { return !_primaryBeaconGuid.IsEmpty(); }
    bool HasSecondaryBeacon() const { return _hasBeaconOfFaith && !_secondaryBeaconGuid.IsEmpty(); }

    ObjectGuid GetPrimaryBeacon() const { return _primaryBeaconGuid; }
    ObjectGuid GetSecondaryBeacon() const { return _secondaryBeaconGuid; }

    void EnableBeaconOfFaith() { _hasBeaconOfFaith = true; }

    bool NeedsBeaconRefresh(Player* bot, Unit* target) const
    {
        if (!target || !bot)
            return false;

        // Check if target has beacon aura from this bot
        return !target->HasAura(BEACON_OF_LIGHT, bot->GetGUID());
    }

private:
    ObjectGuid _primaryBeaconGuid;
    ObjectGuid _secondaryBeaconGuid;
    bool _hasBeaconOfFaith;
};

// ============================================================================
// HOLY PALADIN REFACTORED
// ============================================================================

class HolyPaladinRefactored : public HealerSpecialization<ManaHolyPowerResource>, public PaladinSpecialization
{
public:
    // Use base class members with type alias for cleaner syntax
    using Base = HealerSpecialization<ManaHolyPowerResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::_resource;
    explicit HolyPaladinRefactored(Player* bot)
        : HealerSpecialization<ManaHolyPowerResource>(bot)
        , PaladinSpecialization(bot)
        , _beaconTracker()
        , _avengingWrathActive(false)
        , _avengingWrathEndTime(0)
        , _infusionOfLightActive(false)
        , _lastHolyShockTime(0)
    {
        // Initialize mana/holy power resources
        this->_resource.Initialize(bot);

        // Check if bot has Beacon of Faith talent
        if (bot->HasSpell(BEACON_OF_FAITH))
            _beaconTracker.EnableBeaconOfFaith();

        InitializeCooldowns();

        TC_LOG_DEBUG("playerbot", "HolyPaladinRefactored initialized for {}", bot->GetName());
    }

    void UpdateRotation(::Unit* target) override
    {
        if (!this->GetBot())
            return;

        // Update Holy Paladin state
        UpdateHolyPaladinState();

        // Healers focus on group healing, not target damage
        ExecuteHealingRotation();
    }

    void UpdateBuffs() override
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        // Maintain Devotion Aura
        if (!bot->HasAura(DEVOTION_AURA) && this->CanCastSpell(DEVOTION_AURA, bot))
        {
            this->CastSpell(bot, DEVOTION_AURA);
        }

        // Emergency defensive
        if (bot->GetHealthPct() < 20.0f && this->CanCastSpell(DIVINE_SHIELD, bot))
        {
            this->CastSpell(bot, DIVINE_SHIELD);
        }
    }

protected:
    void ExecuteHealingRotation()
    {
        Player* bot = this->GetBot();
        Group* group = bot->GetGroup();

        // Update beacon targets
        UpdateBeacons(group);

        // Emergency healing
        if (HandleEmergencyHealing(group))
            return;

        // Use Holy Power spenders
        if (this->_resource.holyPower >= 3)
        {
            if (ExecuteHolyPowerSpender(group))
                return;
        }

        // Use major cooldowns
        if (ShouldUseAvengingWrath(group))
        {
            if (this->CanCastSpell(AVENGING_WRATH_HOLY, bot))
            {
                this->CastSpell(bot, AVENGING_WRATH_HOLY);
                _avengingWrathActive = true;
                _avengingWrathEndTime = getMSTime() + 20000;
                return;
            }
        }

        // Generate Holy Power with Holy Shock
        if (this->_resource.holyPower < 5)
        {
            Unit* healTarget = SelectHealingTarget(group);
            if (healTarget && this->CanCastSpell(HOLY_SHOCK, healTarget))
            {
                this->CastSpell(healTarget, HOLY_SHOCK);
                _lastHolyShockTime = getMSTime();
                GenerateHolyPower(1);
                return;
            }
        }

        // Direct healing
        Unit* healTarget = SelectHealingTarget(group);
        if (healTarget)
        {
            ExecuteDirectHealing(healTarget);
        }
    }

    bool HandleEmergencyHealing(Group* group)
    {
        Player* bot = this->GetBot();

        // Self emergency
        if (bot->GetHealthPct() < 25.0f)
        {
            // Lay on Hands
            if (this->CanCastSpell(LAY_ON_HANDS, bot))
            {
                this->CastSpell(bot, LAY_ON_HANDS);
                return true;
            }

            // Word of Glory
            if (this->_resource.holyPower >= 3 && this->CanCastSpell(WORD_OF_GLORY, bot))
            {
                this->CastSpell(bot, WORD_OF_GLORY);
                ConsumeHolyPower(3);
                return true;
            }
        }

        // Group emergency
        if (group)
        {
            for (GroupReference const& ref : group->GetMembers())
            {
                if (Player* member = ref.GetSource())
                {
                    if (member->IsAlive() && member->GetHealthPct() < 20.0f)
                    {
                        // Lay on Hands on critical ally
                        if (this->CanCastSpell(LAY_ON_HANDS, member))
                        {
                            this->CastSpell(member, LAY_ON_HANDS);
                            return true;
                        }

                        // Flash of Light for speed
                        if (_infusionOfLightActive && this->CanCastSpell(FLASH_OF_LIGHT, member))
                        {
                            this->CastSpell(member, FLASH_OF_LIGHT);
                            _infusionOfLightActive = false;
                            return true;
                        }
                    }
                }
            }
        }

        return false;
    }

    bool ExecuteHolyPowerSpender(Group* group)
    {
        // Check for multiple injured allies
        uint32 injuredCount = CountInjuredAllies(group, 0.7f);

        if (injuredCount >= 3)
        {
            // Light of Dawn for AoE healing
            if (this->CanCastSpell(LIGHT_OF_DAWN, this->GetBot()))
            {
                this->CastSpell(this->GetBot(), LIGHT_OF_DAWN);
                ConsumeHolyPower(3);
                return true;
            }
        }

        // Single target Word of Glory
        Unit* target = SelectHealingTarget(group);
        if (target && target->GetHealthPct() < 80.0f)
        {
            if (this->CanCastSpell(WORD_OF_GLORY, target))
            {
                this->CastSpell(target, WORD_OF_GLORY);
                ConsumeHolyPower(3);
                return true;
            }
        }

        return false;
    }

    void ExecuteDirectHealing(Unit* target)
    {
        if (!target)
            return;

        float healthPct = target->GetHealthPct();

        // Critical: Flash of Light
        if (healthPct < 50.0f)
        {
            if (this->CanCastSpell(FLASH_OF_LIGHT, target))
            {
                this->CastSpell(target, FLASH_OF_LIGHT);
                return;
            }
        }

        // Moderate: Holy Light
        if (healthPct < 80.0f)
        {
            if (this->CanCastSpell(HOLY_LIGHT, target))
            {
                this->CastSpell(target, HOLY_LIGHT);
                return;
            }
        }
    }

    void UpdateBeacons(Group* group)
    {
        if (!group)
            return;

        // Assign beacon to main tank
        Player* tank = GetMainTank(group);
        if (tank && _beaconTracker.NeedsBeaconRefresh(this->GetBot(), tank))
        {
            if (this->CanCastSpell(BEACON_OF_LIGHT, tank))
            {
                this->CastSpell(tank, BEACON_OF_LIGHT);
                _beaconTracker.SetPrimaryBeacon(tank->GetGUID());
            }
        }

        // Assign second beacon if talented
        if (_beaconTracker.HasSecondaryBeacon())
        {
            Player* secondTank = GetOffTank(group);
            if (secondTank && _beaconTracker.NeedsBeaconRefresh(this->GetBot(), secondTank))
            {
                if (this->CanCastSpell(BEACON_OF_FAITH, secondTank))
                {
                    this->CastSpell(secondTank, BEACON_OF_FAITH);
                    _beaconTracker.SetSecondaryBeacon(secondTank->GetGUID());
                }
            }
        }
    }

    Unit* SelectHealingTarget(Group* group)
    {
        if (!group)
            return GetBot();

        Unit* lowestHealth = nullptr;
        float lowestPct = 100.0f;

        // Priority: Self > Tank > Healers > DPS
        for (GroupReference const& ref : group->GetMembers())
        {
            if (Player* member = ref.GetSource())
            {
                if (member->IsAlive() && member->GetHealthPct() < lowestPct)
                {
                    lowestPct = member->GetHealthPct();
                    lowestHealth = member;
                }
            }
        }

        return lowestHealth ? lowestHealth : this->GetBot();
    }

private:
    void UpdateHolyPaladinState()
    {
        uint32 now = getMSTime();

        // Update Avenging Wrath
        if (_avengingWrathActive && now >= _avengingWrathEndTime)
        {
            _avengingWrathActive = false;
            _avengingWrathEndTime = 0;
        }

        // Update Infusion of Light proc
        if (_infusionOfLightActive)
        {
            // Check if buff expired
            if (!this->GetBot()->HasAura(INFUSION_OF_LIGHT))
                _infusionOfLightActive = false;
        }
        else
        {
            // Check if proc occurred
            if (this->GetBot()->HasAura(INFUSION_OF_LIGHT))
                _infusionOfLightActive = true;
        }

        // Update Holy Power from bot
        if (this->GetBot())
            this->_resource.holyPower = this->GetBot()->GetPower(POWER_HOLY_POWER);
    }

    bool ShouldUseAvengingWrath(Group* group) const
    {
        // Use when multiple allies injured
        uint32 injuredCount = CountInjuredAllies(group, 0.6f);
        return injuredCount >= 3;
    }

    uint32 CountInjuredAllies(Group* group, float threshold) const
    {
        if (!group)
            return 0;

        uint32 count = 0;
        for (GroupReference const& ref : group->GetMembers())
        {
            if (Player* member = ref.GetSource())
            {
                if (member->IsAlive() && member->GetHealthPct() < (threshold * 100.0f))
                    count++;
            }
        }
        return count;
    }

    Player* GetMainTank(Group* group) const
    {
        if (!group)
            return nullptr;

        // Find tank role
        for (GroupReference const& ref : group->GetMembers())
        {
            if (Player* member = ref.GetSource())
            {
                if (IsTank(member))
                    return member;
            }
        }
        return nullptr;
    }

    Player* GetOffTank(Group* group) const
    {
        if (!group)
            return nullptr;

        Player* mainTank = GetMainTank(group);
        for (GroupReference const& ref : group->GetMembers())
        {
            if (Player* member = ref.GetSource())
            {
                if (IsTank(member) && member != mainTank)
                    return member;
            }
        }
        return nullptr;
    }

    bool IsTank(Player* player) const
    {
        if (!player)
            return false;

        // Simplified tank detection - check if player is currently tanking
        // A more robust implementation would check spec, but talent API is deprecated
        if (Unit* victim = player->GetVictim())
            return victim->GetVictim() == player;

        return false;
    }

    void GenerateHolyPower(uint32 amount)
    {
        this->_resource.holyPower = std::min(this->_resource.holyPower + amount, this->_resource.maxHolyPower);
    }

    void ConsumeHolyPower(uint32 amount)
    {
        this->_resource.holyPower = (this->_resource.holyPower > amount) ? this->_resource.holyPower - amount : 0;
    }

    void InitializeCooldowns()
    {
        RegisterCooldown(HOLY_SHOCK, 7500);             // 7.5 sec CD
        RegisterCooldown(DIVINE_TOLL, 60000);           // 1 min CD
        RegisterCooldown(AVENGING_WRATH_HOLY, 120000);  // 2 min CD
        RegisterCooldown(AVENGING_CRUSADER, 120000);    // 2 min CD
        RegisterCooldown(HOLY_AVENGER, 120000);         // 2 min CD
        RegisterCooldown(LAY_ON_HANDS, 600000);         // 10 min CD
        RegisterCooldown(DIVINE_SHIELD, 300000);        // 5 min CD
        RegisterCooldown(DIVINE_PROTECTION, 300000);    // 5 min CD
        RegisterCooldown(BLESSING_OF_SACRIFICE, 120000); // 2 min CD
        RegisterCooldown(AURA_MASTERY, 180000);         // 3 min CD
    }

private:
    HolyPaladinBeaconTracker _beaconTracker;
    bool _avengingWrathActive;
    uint32 _avengingWrathEndTime;
    bool _infusionOfLightActive;
    uint32 _lastHolyShockTime;
};

} // namespace Playerbot
