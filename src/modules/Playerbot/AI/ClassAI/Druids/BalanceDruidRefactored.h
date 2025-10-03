/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * Balance Druid Refactored - Template-Based Implementation
 *
 * This file provides a complete, template-based implementation of Balance Druid
 * using the RangedDpsSpecialization with dual resource system (Mana + Astral Power).
 */

#pragma once

#include "../CombatSpecializationTemplates.h"
#include "../ResourceTypes.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellAuraEffects.h"
#include "Log.h"
#include "DruidSpecialization.h"

namespace Playerbot
{

// ============================================================================
// BALANCE DRUID SPELL IDs (WoW 11.2 - The War Within)
// ============================================================================

enum BalanceDruidSpells
{
    // Astral Power Generators
    WRATH                    = 190984,  // 40 Astral Power, single target
    STARFIRE                 = 194153,  // 60 Astral Power, single target
    STELLAR_FLARE            = 202347,  // DoT, generates 8 AP per tick (talent)
    FORCE_OF_NATURE          = 205636,  // 20 AP, summons treants (talent)

    // Astral Power Spenders
    STARSURGE                = 78674,   // 30 AP, single target nuke
    STARFALL                 = 191034,  // 50 AP, AoE ground effect
    NEW_MOON                 = 274281,  // 10 AP, first stage (talent)
    HALF_MOON                = 274282,  // 20 AP, second stage (talent)
    FULL_MOON                = 274283,  // 40 AP, third stage (talent)

    // DoTs
    MOONFIRE                 = 164812,  // DoT, applies from Wrath during eclipse
    SUNFIRE                  = 164815,  // DoT, applies from Starfire during eclipse

    // Major Cooldowns
    INCARNATION_CHOSEN       = 102560,  // 3 min CD, major burst (talent)
    CELESTIAL_ALIGNMENT      = 194223,  // 3 min CD, burst damage
    WARRIOR_OF_ELUNE         = 202425,  // 45 sec CD, 3 free Starfires (talent)
    FURY_OF_ELUNE            = 202770,  // 1 min CD, channeled AoE (talent)
    CONVOKE_THE_SPIRITS      = 391528,  // 2 min CD, random spell burst (talent)

    // Utility
    MOONKIN_FORM             = 24858,   // Shapeshift
    SOLAR_BEAM               = 78675,   // Interrupt/silence
    TYPHOON                  = 132469,  // Knockback (talent)
    MIGHTY_BASH              = 5211,    // Stun (talent)
    MASS_ENTANGLEMENT        = 102359,  // Root (talent)
    REMOVE_CORRUPTION        = 2782,    // Dispel poison/curse
    SOOTHE                   = 2908,    // Enrage dispel
    INNERVATE                = 29166,   // Mana regen

    // Defensives
    BARKSKIN                 = 22812,   // 1 min CD, damage reduction
    RENEWAL                  = 108238,  // 1.5 min CD, self-heal (talent)
    REGROWTH                 = 8936,    // Self-heal
    BEAR_FORM                = 5487,    // Emergency tank form
    FRENZIED_REGENERATION    = 22842,   // Self-heal in bear form

    // Eclipse System
    ECLIPSE_SOLAR            = 48517,   // Solar Eclipse buff
    ECLIPSE_LUNAR            = 48518,   // Lunar Eclipse buff
    BALANCE_OF_ALL_THINGS    = 394048,  // Stacking crit buff (talent)

    // Procs and Buffs
    SHOOTING_STARS           = 202342,  // Proc: free Starsurge (talent)
    STARWEAVERS_WARP         = 393942,  // Starsurge increases Starfall damage
    STARWEAVERS_WEFT         = 393944,  // Starfall increases Starsurge damage

    // Talents
    WILD_MUSHROOM            = 88747,   // Ground AoE (talent)
    TWIN_MOONS               = 279620,  // Moonfire hits extra target
    SOUL_OF_THE_FOREST       = 114107   // Reduced Starsurge cost after Starfall
};

// Dual resource type for Balance Druid (Mana + Astral Power)
struct ManaAstralPowerResource
{
    uint32 mana{0};
    uint32 astralPower{0};
    uint32 maxMana{100000};
    uint32 maxAstralPower{100};

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
        astralPower = 0;
    }
};

// ============================================================================
// BALANCE ECLIPSE TRACKER
// ============================================================================

class BalanceEclipseTracker
{
public:
    enum EclipseState
    {
        NONE,
        SOLAR,
        LUNAR
    };

    BalanceEclipseTracker()
        : _currentEclipse(NONE)
        , _eclipseEndTime(0)
    {
    }

    void EnterSolarEclipse()
    {
        _currentEclipse = SOLAR;
        _eclipseEndTime = getMSTime() + 15000; // 15 sec duration
    }

    void EnterLunarEclipse()
    {
        _currentEclipse = LUNAR;
        _eclipseEndTime = getMSTime() + 15000;
    }

    EclipseState GetCurrentEclipse() const { return _currentEclipse; }
    bool IsInEclipse() const { return _currentEclipse != NONE; }
    bool IsInSolarEclipse() const { return _currentEclipse == SOLAR; }
    bool IsInLunarEclipse() const { return _currentEclipse == LUNAR; }

    void Update(Player* bot)
    {
        if (!bot)
            return;

        uint32 now = getMSTime();

        // Check eclipse buffs
        if (bot->HasAura(ECLIPSE_SOLAR))
        {
            _currentEclipse = SOLAR;
        }
        else if (bot->HasAura(ECLIPSE_LUNAR))
        {
            _currentEclipse = LUNAR;
        }
        else if (_currentEclipse != NONE && now >= _eclipseEndTime)
        {
            _currentEclipse = NONE;
            _eclipseEndTime = 0;
        }
    }

private:
    EclipseState _currentEclipse;
    uint32 _eclipseEndTime;
};

// ============================================================================
// BALANCE DOT TRACKER
// ============================================================================

class BalanceDoTTracker
{
public:
    BalanceDoTTracker()
        : _trackedDoTs()
    {
    }

    void ApplyDoT(ObjectGuid guid, uint32 spellId, uint32 duration)
    {
        _trackedDoTs[guid][spellId] = getMSTime() + duration;
    }

    bool HasDoT(ObjectGuid guid, uint32 spellId) const
    {
        auto it = _trackedDoTs.find(guid);
        if (it == _trackedDoTs.end())
            return false;

        auto dotIt = it->second.find(spellId);
        if (dotIt == it->second.end())
            return false;

        return getMSTime() < dotIt->second;
    }

    uint32 GetTimeRemaining(ObjectGuid guid, uint32 spellId) const
    {
        auto it = _trackedDoTs.find(guid);
        if (it == _trackedDoTs.end())
            return 0;

        auto dotIt = it->second.find(spellId);
        if (dotIt == it->second.end())
            return 0;

        uint32 now = getMSTime();
        return (dotIt->second > now) ? (dotIt->second - now) : 0;
    }

    bool NeedsRefresh(ObjectGuid guid, uint32 spellId, uint32 pandemicWindow = 5400) const
    {
        return GetTimeRemaining(guid, spellId) < pandemicWindow;
    }

    void Update()
    {
        uint32 now = getMSTime();
        for (auto targetIt = _trackedDoTs.begin(); targetIt != _trackedDoTs.end();)
        {
            for (auto dotIt = targetIt->second.begin(); dotIt != targetIt->second.end();)
            {
                if (now >= dotIt->second)
                    dotIt = targetIt->second.erase(dotIt);
                else
                    ++dotIt;
            }

            if (targetIt->second.empty())
                targetIt = _trackedDoTs.erase(targetIt);
            else
                ++targetIt;
        }
    }

private:
    std::unordered_map<ObjectGuid, std::unordered_map<uint32, uint32>> _trackedDoTs;
};

// ============================================================================
// BALANCE DRUID REFACTORED
// ============================================================================

class BalanceDruidRefactored : public RangedDpsSpecialization<ManaAstralPowerResource>, public DruidSpecialization
{
public:
    using Base = RangedDpsSpecialization<ManaAstralPowerResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::_resource;
    explicit BalanceDruidRefactored(Player* bot)
        : RangedDpsSpecialization<ManaAstralPowerResource>(bot)
        , DruidSpecialization(bot)
        , _eclipseTracker()
        , _dotTracker()
        , _starfallActive(false)
        , _starfallEndTime(0)
        , _shootingStarsProc(false)
    {
        // Initialize mana/astral power resources
        this->_resource.Initialize(bot);

        InitializeCooldowns();

        TC_LOG_DEBUG("playerbot", "BalanceDruidRefactored initialized for {}", bot->GetName());
    }

    void UpdateRotation(::Unit* target) override
    {
        if (!target || !target->IsAlive() || !target->IsHostileTo(this->GetBot()))
            return;

        // Update Balance state
        UpdateBalanceState(target);

        // Ensure Moonkin Form
        EnsureMoonkinForm();

        // Use major cooldowns
        HandleCooldowns();

        // Determine if AoE or single target
        uint32 enemyCount = this->GetEnemiesInRange(40.0f);
        if (enemyCount >= 3)
        {
            ExecuteAoERotation(target, enemyCount);
        }
        else
        {
            ExecuteSingleTargetRotation(target);
        }
    }

    void UpdateBuffs() override
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        // Defensive cooldowns
        HandleDefensiveCooldowns();
    }

    // Note: GetOptimalRange is final in RangedDpsSpecialization base class (defaults to 30-40 yards)

protected:
    void ExecuteSingleTargetRotation(::Unit* target)
    {
        ObjectGuid targetGuid = target->GetGUID();
        uint32 ap = this->_resource.astralPower;

        // Priority 1: Use Shooting Stars proc (free Starsurge)
        if (_shootingStarsProc && this->CanCastSpell(STARSURGE, target))
        {
            this->CastSpell(target, STARSURGE);
            _shootingStarsProc = false;
            return;
        }

        // Priority 2: Maintain Moonfire
        if (_dotTracker.NeedsRefresh(targetGuid, MOONFIRE))
        {
            if (this->CanCastSpell(MOONFIRE, target))
            {
                this->CastSpell(target, MOONFIRE);
                _dotTracker.ApplyDoT(targetGuid, MOONFIRE, 22000); // 22 sec duration
                return;
            }
        }

        // Priority 3: Maintain Sunfire
        if (_dotTracker.NeedsRefresh(targetGuid, SUNFIRE))
        {
            if (this->CanCastSpell(SUNFIRE, target))
            {
                this->CastSpell(target, SUNFIRE);
                _dotTracker.ApplyDoT(targetGuid, SUNFIRE, 18000); // 18 sec duration
                return;
            }
        }

        // Priority 4: Maintain Stellar Flare (talent)
        if (_dotTracker.NeedsRefresh(targetGuid, STELLAR_FLARE))
        {
            if (this->CanCastSpell(STELLAR_FLARE, target))
            {
                this->CastSpell(target, STELLAR_FLARE);
                _dotTracker.ApplyDoT(targetGuid, STELLAR_FLARE, 24000);
                return;
            }
        }

        // Priority 5: Starsurge (spend AP)
        if (ap >= 30 && this->CanCastSpell(STARSURGE, target))
        {
            this->CastSpell(target, STARSURGE);
            ConsumeAstralPower(30);
            return;
        }

        // Priority 6: Starfire (Lunar Eclipse or default)
        if (_eclipseTracker.IsInLunarEclipse() || !_eclipseTracker.IsInEclipse())
        {
            if (this->CanCastSpell(STARFIRE, target))
            {
                this->CastSpell(target, STARFIRE);
                GenerateAstralPower(8);

                if (!_eclipseTracker.IsInEclipse())
                    _eclipseTracker.EnterLunarEclipse();

                return;
            }
        }

        // Priority 7: Wrath (Solar Eclipse)
        if (_eclipseTracker.IsInSolarEclipse())
        {
            if (this->CanCastSpell(WRATH, target))
            {
                this->CastSpell(target, WRATH);
                GenerateAstralPower(6);
                return;
            }
        }

        // Priority 8: Wrath filler
        if (this->CanCastSpell(WRATH, target))
        {
            this->CastSpell(target, WRATH);
            GenerateAstralPower(6);

            if (!_eclipseTracker.IsInEclipse())
                _eclipseTracker.EnterSolarEclipse();
        }
    }

    void ExecuteAoERotation(::Unit* target, uint32 enemyCount)
    {
        uint32 ap = this->_resource.astralPower;

        // Priority 1: Starfall (AoE AP spender)
        if (ap >= 50 && !_starfallActive && this->CanCastSpell(STARFALL, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), STARFALL);
            _starfallActive = true;
            _starfallEndTime = getMSTime() + 8000;
            ConsumeAstralPower(50);
            return;
        }

        // Priority 2: Sunfire (AoE DoT)
        if (this->CanCastSpell(SUNFIRE, target))
        {
            this->CastSpell(target, SUNFIRE);
            return;
        }

        // Priority 3: Moonfire (AoE DoT with Twin Moons)
        if (this->CanCastSpell(MOONFIRE, target))
        {
            this->CastSpell(target, MOONFIRE);
            return;
        }

        // Priority 4: Fury of Elune (talent)
        if (this->CanCastSpell(FURY_OF_ELUNE, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), FURY_OF_ELUNE);
            return;
        }

        // Priority 5: Starsurge
        if (ap >= 30 && this->CanCastSpell(STARSURGE, target))
        {
            this->CastSpell(target, STARSURGE);
            ConsumeAstralPower(30);
            return;
        }

        // Priority 6: Starfire filler
        if (this->CanCastSpell(STARFIRE, target))
        {
            this->CastSpell(target, STARFIRE);
            GenerateAstralPower(8);
            return;
        }
    }

    void HandleCooldowns()
    {
        Player* bot = this->GetBot();
        uint32 ap = this->_resource.astralPower;

        // Incarnation/Celestial Alignment (major burst)
        if (ap >= 40 && this->CanCastSpell(INCARNATION_CHOSEN, bot))
        {
            this->CastSpell(bot, INCARNATION_CHOSEN);
            TC_LOG_DEBUG("playerbot", "Balance: Incarnation activated");
        }
        else if (ap >= 40 && this->CanCastSpell(CELESTIAL_ALIGNMENT, bot))
        {
            this->CastSpell(bot, CELESTIAL_ALIGNMENT);
            TC_LOG_DEBUG("playerbot", "Balance: Celestial Alignment");
        }

        // Convoke the Spirits
        if (this->CanCastSpell(CONVOKE_THE_SPIRITS, bot))
        {
            this->CastSpell(bot, CONVOKE_THE_SPIRITS);
            TC_LOG_DEBUG("playerbot", "Balance: Convoke the Spirits");
        }

        // Warrior of Elune
        if (this->CanCastSpell(WARRIOR_OF_ELUNE, bot))
        {
            this->CastSpell(bot, WARRIOR_OF_ELUNE);
        }
    }

    void HandleDefensiveCooldowns()
    {
        Player* bot = this->GetBot();
        float healthPct = bot->GetHealthPct();

        // Barkskin
        if (healthPct < 50.0f && this->CanCastSpell(BARKSKIN, bot))
        {
            this->CastSpell(bot, BARKSKIN);
            TC_LOG_DEBUG("playerbot", "Balance: Barkskin");
            return;
        }

        // Renewal
        if (healthPct < 40.0f && this->CanCastSpell(RENEWAL, bot))
        {
            this->CastSpell(bot, RENEWAL);
            TC_LOG_DEBUG("playerbot", "Balance: Renewal");
            return;
        }

        // Regrowth
        if (healthPct < 60.0f && this->CanCastSpell(REGROWTH, bot))
        {
            this->CastSpell(bot, REGROWTH);
            return;
        }
    }

    void EnsureMoonkinForm()
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        if (!bot->HasAura(MOONKIN_FORM) && this->CanCastSpell(MOONKIN_FORM, bot))
        {
            this->CastSpell(bot, MOONKIN_FORM);
            TC_LOG_DEBUG("playerbot", "Balance: Moonkin Form activated");
        }
    }

private:
    void UpdateBalanceState(::Unit* target)
    {
        // Update Eclipse tracker
        _eclipseTracker.Update(this->GetBot());

        // Update DoT tracker
        _dotTracker.Update();

        // Update Starfall
        if (_starfallActive && getMSTime() >= _starfallEndTime)
        {
            _starfallActive = false;
            _starfallEndTime = 0;
        }

        // Update Shooting Stars proc
        if (this->GetBot() && this->GetBot()->HasAura(SHOOTING_STARS))
            _shootingStarsProc = true;
        else
            _shootingStarsProc = false;

        // Update Astral Power from bot
        if (this->GetBot())
        {
            this->_resource.astralPower = this->GetBot()->GetPower(POWER_LUNAR_POWER); // Astral Power uses LUNAR_POWER
            this->_resource.mana = this->GetBot()->GetPower(POWER_MANA);
        }
    }

    void GenerateAstralPower(uint32 amount)
    {
        this->_resource.astralPower = std::min(this->_resource.astralPower + amount, this->_resource.maxAstralPower);
    }

    void ConsumeAstralPower(uint32 amount)
    {
        this->_resource.astralPower = (this->_resource.astralPower > amount) ? this->_resource.astralPower - amount : 0;
    }

    void InitializeCooldowns()
    {
        RegisterCooldown(WRATH, 0);                     // No CD
        RegisterCooldown(STARFIRE, 0);                  // No CD
        RegisterCooldown(STARSURGE, 0);                 // No CD, AP-gated
        RegisterCooldown(STARFALL, 0);                  // No CD, AP-gated
        RegisterCooldown(MOONFIRE, 0);                  // No CD
        RegisterCooldown(SUNFIRE, 0);                   // No CD
        RegisterCooldown(INCARNATION_CHOSEN, 180000);   // 3 min CD
        RegisterCooldown(CELESTIAL_ALIGNMENT, 180000);  // 3 min CD
        RegisterCooldown(CONVOKE_THE_SPIRITS, 120000);  // 2 min CD
        RegisterCooldown(WARRIOR_OF_ELUNE, 45000);      // 45 sec CD
        RegisterCooldown(FURY_OF_ELUNE, 60000);         // 1 min CD
        RegisterCooldown(BARKSKIN, 60000);              // 1 min CD
        RegisterCooldown(RENEWAL, 90000);               // 1.5 min CD
        RegisterCooldown(SOLAR_BEAM, 60000);            // 1 min CD
    }

private:
    BalanceEclipseTracker _eclipseTracker;
    BalanceDoTTracker _dotTracker;
    bool _starfallActive;
    uint32 _starfallEndTime;
    bool _shootingStarsProc;
};

} // namespace Playerbot
