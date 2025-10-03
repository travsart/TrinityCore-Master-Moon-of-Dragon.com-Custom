/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * Mistweaver Monk Refactored - Template-Based Implementation
 *
 * This file provides a complete, template-based implementation of Mistweaver Monk
 * using the HealerSpecialization with Mana resource system.
 */

#pragma once

#include "../CombatSpecializationTemplates.h"
#include "../ResourceTypes.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellAuraEffects.h"
#include "Group.h"
#include "Log.h"
#include "MonkSpecialization.h"

namespace Playerbot
{

// ============================================================================
// MISTWEAVER MONK SPELL IDs (WoW 11.2 - The War Within)
// ============================================================================

enum MistweaverMonkSpells
{
    // Direct Heals
    VIVIFY                   = 116670,  // 5% mana, smart heal (cleave)
    SOOTHING_MIST            = 115175,  // Channel, enables instant Vivify
    ENVELOPING_MIST          = 124682,  // 6% mana, HoT
    EXPEL_HARM_MIST          = 322101,  // Self-heal
    LIFE_COCOON              = 116849,  // 2 min CD, absorb shield

    // HoT Management
    RENEWING_MIST            = 115151,  // 8.5 sec CD, 2 charges, bouncing HoT
    ESSENCE_FONT             = 191837,  // 5% mana, 12 sec CD, AoE HoT + heal
    REVIVAL                  = 115310,  // 3 min CD, raid-wide instant heal

    // AoE Healing
    REFRESHING_JADE_WIND     = 196725,  // 25% mana, AoE HoT (talent)
    CHI_BURST_MIST           = 123986,  // 30 sec CD, AoE heal (talent)

    // Cooldowns
    INVOKE_YULON             = 322118,  // 3 min CD, summon celestial (talent)
    INVOKE_CHI_JI            = 325197,  // 3 min CD, summon celestial (talent)
    INVOKE_SHEILUN           = 399491,  // 3 min CD, summon weapon (talent)

    // Utility
    THUNDER_FOCUS_TEA        = 116680,  // 30 sec CD, empowers next spell
    MANA_TEA                 = 197908,  // Mana regen channel (talent)
    FORTIFYING_BREW_MIST     = 243435,  // 6 min CD, damage reduction
    DIFFUSE_MAGIC_MIST       = 122783,  // 1.5 min CD, magic immunity
    DETOX_MIST               = 115450,  // Dispel poison/disease
    PARALYSIS                = 115078,  // CC

    // DPS Abilities (Fistweaving)
    RISING_SUN_KICK_MIST     = 107428,  // 2 Chi, damage
    BLACKOUT_KICK_MIST       = 100784,  // 1 Chi, damage
    TIGER_PALM_MIST          = 100780,  // Energy, generates Chi
    SPINNING_CRANE_KICK_MIST = 101546,  // Chi, AoE damage + healing

    // Procs and Buffs
    TEACHINGS_OF_THE_MONASTERY = 202090, // Buff from Blackout Kick
    UPWELLING                = 274963,  // Essence Font stacks
    ANCIENT_TEACHINGS        = 388023,  // Fistweaving healing conversion

    // Talents
    INVOKE_YULON_THE_JADE_SERPENT = 322118,
    LIFECYCLES               = 197915,  // Mana reduction rotation
    SPIRIT_OF_THE_CRANE      = 210802,  // Mana regen from fistweaving
    CLOUDED_FOCUS            = 388047   // Soothing Mist cost reduction
};

// ============================================================================
// MISTWEAVER RENEWING MIST TRACKER
// ============================================================================

class MistweaverReNewingMistTracker
{
public:
    MistweaverReNewingMistTracker()
        : _trackedTargets()
    {
    }

    void AddTarget(ObjectGuid guid)
    {
        _trackedTargets[guid] = getMSTime() + 20000; // 20 sec duration
    }

    void RemoveTarget(ObjectGuid guid)
    {
        _trackedTargets.erase(guid);
    }

    bool HasReNewingMist(ObjectGuid guid) const
    {
        auto it = _trackedTargets.find(guid);
        if (it == _trackedTargets.end())
            return false;

        return getMSTime() < it->second;
    }

    uint32 GetActiveCount() const
    {
        uint32 count = 0;
        uint32 now = getMSTime();
        for (const auto& pair : _trackedTargets)
        {
            if (now < pair.second)
                ++count;
        }
        return count;
    }

    void Update()
    {
        uint32 now = getMSTime();
        for (auto it = _trackedTargets.begin(); it != _trackedTargets.end();)
        {
            if (now >= it->second)
                it = _trackedTargets.erase(it);
            else
                ++it;
        }
    }

private:
    std::unordered_map<ObjectGuid, uint32> _trackedTargets;
};

// ============================================================================
// MISTWEAVER SOOTHING MIST TRACKER
// ============================================================================

class MistweaverSoothingMistTracker
{
public:
    MistweaverSoothingMistTracker()
        : _currentTargetGuid()
        , _channelStartTime(0)
        , _isChanneling(false)
    {
    }

    void StartChannel(ObjectGuid guid)
    {
        _currentTargetGuid = guid;
        _channelStartTime = getMSTime();
        _isChanneling = true;
    }

    void StopChannel()
    {
        _currentTargetGuid = ObjectGuid::Empty;
        _channelStartTime = 0;
        _isChanneling = false;
    }

    bool IsChanneling() const { return _isChanneling; }
    ObjectGuid GetTarget() const { return _currentTargetGuid; }

    bool CanInstantCast() const
    {
        // Soothing Mist enables instant Vivify/Enveloping Mist
        return _isChanneling && (getMSTime() - _channelStartTime) > 500;
    }

    void Update(Player* bot)
    {
        if (!bot)
            return;

        // Check if still channeling
        if (_isChanneling)
        {
            if (!bot->HasUnitState(UNIT_STATE_CASTING) || !bot->HasAura(SOOTHING_MIST))
            {
                StopChannel();
            }
        }
    }

private:
    ObjectGuid _currentTargetGuid;
    uint32 _channelStartTime;
    bool _isChanneling;
};

// ============================================================================
// MISTWEAVER MONK REFACTORED
// ============================================================================

class MistweaverMonkRefactored : public HealerSpecialization<ManaResource>, public MonkSpecialization
{
public:
    using Base = HealerSpecialization<ManaResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::_resource;
    explicit MistweaverMonkRefactored(Player* bot)
        : HealerSpecialization<ManaResource>(bot)
        , MonkSpecialization(bot)
        , _renewingMistTracker()
        , _soothingMistTracker()
        , _thunderFocusTeaActive(false)
        , _lastEssenceFontTime(0)
    {
        // Initialize mana resources
        this->_resource.Initialize(bot);

        InitializeCooldowns();

        TC_LOG_DEBUG("playerbot", "MistweaverMonkRefactored initialized for {}", bot->GetName());
    }

    void UpdateRotation(::Unit* target) override
    {
        // Mistweaver focuses on healing, not DPS rotation
        // Use UpdateBuffs() for healing logic
    }

    void UpdateBuffs() override
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        // Update Mistweaver state
        UpdateMistweaverState();

        // Get group members
        std::vector<Unit*> group = GetGroupMembers();
        if (group.empty())
            return;

        // Handle healing rotation
        ExecuteHealingRotation(group);
    }

    float GetOptimalRange(::Unit* target) override
    {
        return 40.0f; // Ranged healer
    }

protected:
    void ExecuteHealingRotation(const std::vector<Unit*>& group)
    {
        // Priority 1: Emergency healing
        if (HandleEmergencyHealing(group))
            return;

        // Priority 2: Thunder Focus Tea empowerment
        if (HandleThunderFocusTea(group))
            return;

        // Priority 3: Spread Renewing Mist
        if (HandleReNewingMist(group))
            return;

        // Priority 4: Essence Font for AoE healing
        if (HandleEssenceFont(group))
            return;

        // Priority 5: Single target healing
        if (HandleSingleTargetHealing(group))
            return;

        // Priority 6: Maintain Soothing Mist channel
        HandleSoothingMist(group);
    }

    bool HandleEmergencyHealing(const std::vector<Unit*>& group)
    {
        Player* bot = this->GetBot();

        // Critical: Life Cocoon
        for (Unit* member : group)
        {
            if (member->GetHealthPct() < 20.0f && this->CanCastSpell(LIFE_COCOON, member))
            {
                this->CastSpell(LIFE_COCOON, member);
                TC_LOG_DEBUG("playerbot", "Mistweaver: Life Cocoon on {}", member->GetName());
                return true;
            }
        }

        // Very low: Revival (raid-wide instant heal)
        uint32 lowHealthCount = 0;
        for (Unit* member : group)
        {
            if (member->GetHealthPct() < 40.0f)
                ++lowHealthCount;
        }

        if (lowHealthCount >= 3 && this->CanCastSpell(REVIVAL, bot))
        {
            this->CastSpell(bot, REVIVAL);
            TC_LOG_DEBUG("playerbot", "Mistweaver: Revival raid heal");
            return true;
        }

        // Low: Invoke Yu'lon
        if (lowHealthCount >= 2 && this->CanCastSpell(INVOKE_YULON, bot))
        {
            this->CastSpell(bot, INVOKE_YULON);
            TC_LOG_DEBUG("playerbot", "Mistweaver: Invoke Yu'lon");
            return true;
        }

        // Urgent single target heal
        for (Unit* member : group)
        {
            if (member->GetHealthPct() < 35.0f && this->CanCastSpell(VIVIFY, member))
            {
                this->CastSpell(VIVIFY, member);
                return true;
            }
        }

        return false;
    }

    bool HandleThunderFocusTea(const std::vector<Unit*>& group)
    {
        if (!_thunderFocusTeaActive)
        {
            // Use Thunder Focus Tea if we need emergency healing
            uint32 lowHealthCount = 0;
            for (Unit* member : group)
            {
                if (member->GetHealthPct() < 60.0f)
                    ++lowHealthCount;
            }

            if (lowHealthCount >= 2 && this->CanCastSpell(THUNDER_FOCUS_TEA, this->GetBot()))
            {
                this->CastSpell(THUNDER_FOCUS_TEA, this->GetBot());
                _thunderFocusTeaActive = true;
                TC_LOG_DEBUG("playerbot", "Mistweaver: Thunder Focus Tea activated");
            }
        }

        // If active, use empowered spell
        if (_thunderFocusTeaActive)
        {
            // Empowered Vivify (free + cleave)
            for (Unit* member : group)
            {
                if (member->GetHealthPct() < 70.0f && this->CanCastSpell(VIVIFY, member))
                {
                    this->CastSpell(VIVIFY, member);
                    _thunderFocusTeaActive = false;
                    return true;
                }
            }

            // Empowered Renewing Mist (instant 2 charges)
            Unit* target = SelectHealingTarget(group);
            if (target && this->CanCastSpell(RENEWING_MIST, target))
            {
                this->CastSpell(target, RENEWING_MIST);
                _renewingMistTracker.AddTarget(target->GetGUID());
                _thunderFocusTeaActive = false;
                return true;
            }
        }

        return false;
    }

    bool HandleReNewingMist(const std::vector<Unit*>& group)
    {
        // Maintain Renewing Mist on targets
        uint32 activeCount = _renewingMistTracker.GetActiveCount();

        if (activeCount < 3) // Try to maintain on 3 targets
        {
            for (Unit* member : group)
            {
                if (member->GetHealthPct() < 95.0f && !_renewingMistTracker.HasReNewingMist(member->GetGUID()))
                {
                    if (this->CanCastSpell(RENEWING_MIST, member))
                    {
                        this->CastSpell(RENEWING_MIST, member);
                        _renewingMistTracker.AddTarget(member->GetGUID());
                        return true;
                    }
                }
            }
        }

        return false;
    }

    bool HandleEssenceFont(const std::vector<Unit*>& group)
    {
        // Use Essence Font for AoE healing
        uint32 injuredCount = 0;
        for (Unit* member : group)
        {
            if (member->GetHealthPct() < 80.0f)
                ++injuredCount;
        }

        if (injuredCount >= 3 && this->CanCastSpell(ESSENCE_FONT, this->GetBot()))
        {
            this->CastSpell(ESSENCE_FONT, this->GetBot());
            _lastEssenceFontTime = getMSTime();
            return true;
        }

        return false;
    }

    bool HandleSingleTargetHealing(const std::vector<Unit*>& group)
    {
        Unit* target = SelectHealingTarget(group);
        if (!target)
            return false;

        float healthPct = target->GetHealthPct();

        // Priority 1: Enveloping Mist (strong single target HoT)
        if (healthPct < 70.0f && this->CanCastSpell(ENVELOPING_MIST, target))
        {
            this->CastSpell(target, ENVELOPING_MIST);
            return true;
        }

        // Priority 2: Vivify (smart heal with cleave)
        if (healthPct < 80.0f && this->CanCastSpell(VIVIFY, target))
        {
            this->CastSpell(target, VIVIFY);
            return true;
        }

        return false;
    }

    bool HandleSoothingMist(const std::vector<Unit*>& group)
    {
        // If not channeling, start on lowest target
        if (!_soothingMistTracker.IsChanneling())
        {
            Unit* target = SelectHealingTarget(group);
            if (target && target->GetHealthPct() < 90.0f)
            {
                if (this->CanCastSpell(SOOTHING_MIST, target))
                {
                    this->CastSpell(target, SOOTHING_MIST);
                    _soothingMistTracker.StartChannel(target->GetGUID());
                    return true;
                }
            }
        }
        else
        {
            // Check if channel target still needs healing
            ObjectGuid targetGuid = _soothingMistTracker.this->GetTarget();
            Unit* target = this->GetBot()->GetMap()->GetUnit(targetGuid);

            if (!target || target->GetHealthPct() > 95.0f)
            {
                // Stop channel and find new target
                _soothingMistTracker.StopChannel();
                return false;
            }

            // Use instant Vivify/Enveloping Mist while channeling
            if (_soothingMistTracker.CanInstantCast())
            {
                if (target->GetHealthPct() < 70.0f && this->CanCastSpell(VIVIFY, target))
                {
                    this->CastSpell(target, VIVIFY);
                    return true;
                }
            }
        }

        return false;
    }

private:
    void UpdateMistweaverState()
    {
        // Update Renewing Mist tracker
        _renewingMistTracker.Update();

        // Update Soothing Mist tracker
        _soothingMistTracker.Update(this->GetBot());

        // Update Thunder Focus Tea status
        if (_thunderFocusTeaActive)
        {
            if (!this->GetBot()->HasAura(THUNDER_FOCUS_TEA))
                _thunderFocusTeaActive = false;
        }

        // Update mana from bot
        if (this->GetBot())
            this->_resource.mana = this->GetBot()->GetPower(POWER_MANA);
    }

    Unit* SelectHealingTarget(const std::vector<Unit*>& group)
    {
        Unit* lowestTarget = nullptr;
        float lowestPct = 100.0f;

        for (Unit* member : group)
        {
            float pct = member->GetHealthPct();
            if (pct < lowestPct && pct < 95.0f)
            {
                lowestPct = pct;
                lowestTarget = member;
            }
        }

        return lowestTarget;
    }

    void InitializeCooldowns()
    {
        RegisterCooldown(RENEWING_MIST, 8500);          // 8.5 sec CD (2 charges)
        RegisterCooldown(ESSENCE_FONT, 12000);          // 12 sec CD
        RegisterCooldown(LIFE_COCOON, 120000);          // 2 min CD
        RegisterCooldown(REVIVAL, 180000);              // 3 min CD
        RegisterCooldown(INVOKE_YULON, 180000);         // 3 min CD
        RegisterCooldown(INVOKE_CHI_JI, 180000);        // 3 min CD
        RegisterCooldown(THUNDER_FOCUS_TEA, 30000);     // 30 sec CD
        RegisterCooldown(FORTIFYING_BREW_MIST, 360000); // 6 min CD
        RegisterCooldown(DIFFUSE_MAGIC_MIST, 90000);    // 1.5 min CD
    }

private:
    MistweaverReNewingMistTracker _renewingMistTracker;
    MistweaverSoothingMistTracker _soothingMistTracker;
    bool _thunderFocusTeaActive;
    uint32 _lastEssenceFontTime;
};

} // namespace Playerbot
