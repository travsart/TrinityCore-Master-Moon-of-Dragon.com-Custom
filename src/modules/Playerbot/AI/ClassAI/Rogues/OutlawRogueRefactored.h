/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * Outlaw Rogue Refactored - Template-Based Implementation
 *
 * This file provides a complete, template-based implementation of Outlaw Rogue
 * using the MeleeDpsSpecialization with dual resource system (Energy + Combo Points).
 */

#pragma once

#include "../CombatSpecializationTemplates.h"
#include "RogueResourceTypes.h"  // Shared EnergyComboResource definition
#include "Player.h"
#include "SpellMgr.h"
#include "SpellAuraEffects.h"
#include "Log.h"
#include "RogueSpecialization.h"  // Spell IDs

namespace Playerbot
{

// NOTE: Common Rogue spell IDs (BLADE_FLURRY, ADRENALINE_RUSH, KILLING_SPREE, etc.) are in RogueSpecialization.h
// Only Outlaw-UNIQUE spell IDs defined below to avoid duplicate definition errors
// NOTE: ComboPointsOutlaw is defined in RogueResourceTypes.h (spec-specific resource type)

// ============================================================================
// OUTLAW ROGUE SPELL IDs (WoW 11.2 - The War Within) - UNIQUE ONLY
// ============================================================================

enum OutlawSpells
{
    // WoW 11.2 Outlaw-specific spells
    PISTOL_SHOT              = 185763,  // 40 Energy, ranged, 1 CP
    BETWEEN_THE_EYES         = 315341,  // Finisher, stun
    DISPATCH_OUTLAW          = 2098,    // Finisher, high damage

    // Roll the Bones System (Outlaw unique)
    ROLL_THE_BONES           = 315508,  // 25 Energy, random buff
    BUFF_RUTHLESS_PRECISION  = 193357,  // Crit buff
    BUFF_GRAND_MELEE         = 193358,  // Attack speed buff
    BUFF_BROADSIDE           = 193356,  // Extra combo point
    BUFF_TRUE_BEARING        = 193359,  // CDR buff
    BUFF_SKULL_AND_CROSSBONES = 199603, // Attack power buff
    BUFF_BURIED_TREASURE     = 199600,  // Energy regen buff

    // Talents (Outlaw specific)
    BLADE_RUSH               = 271877,  // 45 sec CD, charge + AoE (talent)
    OPPORTUNITY_PROC         = 195627,  // Free Pistol Shot proc
    GHOSTLY_STRIKE           = 196937,  // Dodge buff
    DREADBLADES              = 343142,  // Spender costs CP instead of energy

    // Outlaw-specific utility (not in shared RogueSpecialization.h)
    FEINT_OUTLAW             = 1966,    // Threat reduction
    MARKED_FOR_DEATH         = 137619   // Instant 5 CP on target
};

// ============================================================================
// ROLL THE BONES TRACKER
// ============================================================================

class RollTheBonesTracker
{
public:
    struct Buff
    {
        uint32 spellId;
        bool active;
        uint32 endTime;

        Buff(uint32 id) : spellId(id), active(false), endTime(0) {}

        bool IsActive() const { return active && getMSTime() < endTime; }
        uint32 GetTimeRemaining() const {
            if (!active) return 0;
            uint32 now = getMSTime();
            return endTime > now ? endTime - now : 0;
        }
    };

    RollTheBonesTracker()
    {
        this->_buffs.push_back(Buff(BUFF_RUTHLESS_PRECISION));
        this->_buffs.push_back(Buff(BUFF_GRAND_MELEE));
        this->_buffs.push_back(Buff(BUFF_BROADSIDE));
        this->_buffs.push_back(Buff(BUFF_TRUE_BEARING));
        this->_buffs.push_back(Buff(BUFF_SKULL_AND_CROSSBONES));
        this->_buffs.push_back(Buff(BUFF_BURIED_TREASURE));
    }

    void RollBuffs(Player* bot)
    {
        // Clear old buffs
        for (auto& buff : _buffs)
        {
            buff.active = false;
            buff.endTime = 0;
        }

        // Simulate rolling 1-2 random buffs
        uint32 buffCount = (rand() % 6 == 0) ? 2 : 1; // ~17% chance for 2 buffs

        for (uint32 i = 0; i < buffCount; ++i)
        {
            uint32 randomIndex = rand() % this->_buffs.size();
            _buffs[randomIndex].active = true;
            _buffs[randomIndex].endTime = getMSTime() + 30000; // 30 sec duration
        }
    }

    uint32 GetActiveBuffCount() const
    {
        uint32 count = 0;
        for (const auto& buff : _buffs)
        {
            if (buff.IsActive())
                count++;
        }
        return count;
    }

    bool HasAnyBuff() const
    {
        return GetActiveBuffCount() > 0;
    }

    bool HasGoodBuffs() const
    {
        // Good buffs: 2+ buffs, or 1 True Bearing/Broadside
        uint32 count = GetActiveBuffCount();
        if (count >= 2)
            return true;

        for (const auto& buff : _buffs)
        {
            if (buff.IsActive() &&
                (buff.spellId == BUFF_TRUE_BEARING || buff.spellId == BUFF_BROADSIDE))
                return true;
        }

        return false;
    }

    uint32 GetLowestBuffDuration() const
    {
        uint32 lowest = 999999;
        for (const auto& buff : _buffs)
        {
            if (buff.IsActive())
            {
                uint32 remaining = buff.GetTimeRemaining();
                if (remaining < lowest)
                    lowest = remaining;
            }
        }
        return lowest == 999999 ? 0 : lowest;
    }

    bool NeedsReroll() const
    {
        // Reroll if no buffs or low duration
        if (!HasAnyBuff())
            return true;

        if (GetLowestBuffDuration() < 3000) // Less than 3 seconds
            return true;

        // Reroll if only 1 bad buff
        if (GetActiveBuffCount() == 1 && !HasGoodBuffs())
            return true;

        return false;
    }

    void Update()
    {
        uint32 now = getMSTime();
        for (auto& buff : _buffs)
        {
            if (buff.active && now >= buff.endTime)
            {
                buff.active = false;
                buff.endTime = 0;
            }
        }
    }

private:
    std::vector<Buff> _buffs;
};

// ============================================================================
// OUTLAW ROGUE REFACTORED
// ============================================================================

class OutlawRogueRefactored : public MeleeDpsSpecialization<ComboPointsOutlaw>
{
public:
    // Use base class members with type alias for cleaner syntax
    using Base = MeleeDpsSpecialization<ComboPointsOutlaw>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::GetEnemiesInRange;
    using Base::_resource;

    explicit OutlawRogueRefactored(Player* bot)
        : MeleeDpsSpecialization<ComboPointsOutlaw>(bot)
        , _rollTheBonesTracker()
        , _bladeFlurryActive(false)
        , _bladeFlurryEndTime(0)
        , _adrenalineRushActive(false)
        , _adrenalineRushEndTime(0)
        , _inStealth(false)
        , _lastSinisterStrikeTime(0)
        , _lastDispatchTime(0)
    {
        // Initialize energy/combo resources
        this->_resource.maxEnergy = 100;
        this->_resource.maxComboPoints = bot->HasSpell(193531) ? 6 : 5; // Deeper Stratagem
        this->_resource.energy = this->_resource.maxEnergy;
        this->_resource.comboPoints = 0;

        InitializeCooldowns();

        TC_LOG_DEBUG("playerbot", "OutlawRogueRefactored initialized for {}", bot->GetName());
    }

    void UpdateRotation(::Unit* target) override
    {
        if (!target || !target->IsAlive() || !target->IsHostileTo(this->GetBot()))
            return;

        // Update tracking systems
        UpdateOutlawState();

        // Check stealth status
        _inStealth = this->GetBot()->HasAuraType(SPELL_AURA_MOD_STEALTH);

        // Stealth opener
        if (_inStealth)
        {
            ExecuteStealthOpener(target);
            return;
        }

        // Main rotation
        uint32 enemyCount = this->GetEnemiesInRange(8.0f);
        if (enemyCount >= 2)
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

        // Enter stealth out of combat
        if (!bot->IsInCombat() && !_inStealth && this->CanCastSpell(STEALTH, bot))
        {
            this->CastSpell(bot, STEALTH);
        }

        // Defensive cooldowns
        if (bot->GetHealthPct() < 30.0f && this->CanCastSpell(CLOAK_OF_SHADOWS, bot))
        {
            this->CastSpell(bot, CLOAK_OF_SHADOWS);
        }

        if (bot->GetHealthPct() < 50.0f && this->CanCastSpell(FEINT_OUTLAW, bot))
        {
            this->CastSpell(bot, FEINT_OUTLAW);
        }
    }

    // GetOptimalRange is final in MeleeDpsSpecialization, cannot override
    // Returns 5.0f melee range by default

protected:
    void ExecuteSingleTargetRotation(::Unit* target)
    {
        uint32 energy = this->_resource.energy;
        uint32 cp = this->_resource.comboPoints;
        uint32 maxCp = this->_resource.maxComboPoints;

        // Priority 1: Adrenaline Rush on cooldown
        if (this->CanCastSpell(ADRENALINE_RUSH, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), ADRENALINE_RUSH);
            _adrenalineRushActive = true;
            _adrenalineRushEndTime = getMSTime() + 20000;
            return;
        }

        // Priority 2: Roll the Bones if needed
        if (_rollTheBonesTracker.NeedsReroll() && cp >= 1 && energy >= 25)
        {
            if (this->CanCastSpell(ROLL_THE_BONES, this->GetBot()))
            {
                this->CastSpell(this->GetBot(), ROLL_THE_BONES);
                _rollTheBonesTracker.RollBuffs(this->GetBot());
                ConsumeEnergy(25);
                this->_resource.comboPoints = 0;
                return;
            }
        }

        // Priority 3: Between the Eyes at 5-6 CP
        if (cp >= maxCp && energy >= 25)
        {
            if (this->CanCastSpell(BETWEEN_THE_EYES, target))
            {
                this->CastSpell(target, BETWEEN_THE_EYES);
                ConsumeEnergy(25);
                this->_resource.comboPoints = 0;
                return;
            }
        }

        // Priority 4: Dispatch at 5-6 CP
        if (cp >= (maxCp - 1) && energy >= 35)
        {
            if (this->CanCastSpell(DISPATCH_OUTLAW, target))
            {
                this->CastSpell(target, DISPATCH_OUTLAW);
                _lastDispatchTime = getMSTime();
                ConsumeEnergy(35);
                this->_resource.comboPoints = 0;
                return;
            }
        }

        // Priority 5: Opportunity proc - free Pistol Shot
        if (this->GetBot()->HasAura(OPPORTUNITY_PROC))
        {
            if (this->CanCastSpell(PISTOL_SHOT, target))
            {
                this->CastSpell(target, PISTOL_SHOT);
                GenerateComboPoints(1);
                // No energy cost with proc
                return;
            }
        }

        // Priority 6: Blade Rush (talent)
        if (energy >= 25 && this->CanCastSpell(BLADE_RUSH, target))
        {
            this->CastSpell(target, BLADE_RUSH);
            ConsumeEnergy(25);
            GenerateComboPoints(1);
            return;
        }

        // Priority 7: Sinister Strike for combo points
        if (energy >= 45 && cp < maxCp)
        {
            if (this->CanCastSpell(SINISTER_STRIKE, target))
            {
                this->CastSpell(target, SINISTER_STRIKE);
                _lastSinisterStrikeTime = getMSTime();
                ConsumeEnergy(45);
                GenerateComboPoints(1);
                // Broadside buff gives extra CP
                if (_rollTheBonesTracker.HasGoodBuffs())
                    GenerateComboPoints(1);
                return;
            }
        }

        // Priority 8: Pistol Shot if can't melee
        if (GetDistanceToTarget(target) > 10.0f && energy >= 40)
        {
            if (this->CanCastSpell(PISTOL_SHOT, target))
            {
                this->CastSpell(target, PISTOL_SHOT);
                ConsumeEnergy(40);
                GenerateComboPoints(1);
                return;
            }
        }
    }

    void ExecuteAoERotation(::Unit* target, uint32 enemyCount)
    {
        uint32 energy = this->_resource.energy;
        uint32 cp = this->_resource.comboPoints;
        uint32 maxCp = this->_resource.maxComboPoints;

        // Priority 1: Enable Blade Flurry for cleave
        if (!_bladeFlurryActive && energy >= 15)
        {
            if (this->CanCastSpell(BLADE_FLURRY, this->GetBot()))
            {
                this->CastSpell(this->GetBot(), BLADE_FLURRY);
                _bladeFlurryActive = true;
                _bladeFlurryEndTime = getMSTime() + 12000;
                ConsumeEnergy(15);
                return;
            }
        }

        // Priority 2: Adrenaline Rush
        if (this->CanCastSpell(ADRENALINE_RUSH, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), ADRENALINE_RUSH);
            _adrenalineRushActive = true;
            _adrenalineRushEndTime = getMSTime() + 20000;
            return;
        }

        // Priority 3: Roll the Bones
        if (_rollTheBonesTracker.NeedsReroll() && cp >= 1 && energy >= 25)
        {
            if (this->CanCastSpell(ROLL_THE_BONES, this->GetBot()))
            {
                this->CastSpell(this->GetBot(), ROLL_THE_BONES);
                _rollTheBonesTracker.RollBuffs(this->GetBot());
                ConsumeEnergy(25);
                this->_resource.comboPoints = 0;
                return;
            }
        }

        // Priority 4: Between the Eyes at 5+ CP
        if (cp >= 5 && energy >= 25)
        {
            if (this->CanCastSpell(BETWEEN_THE_EYES, target))
            {
                this->CastSpell(target, BETWEEN_THE_EYES);
                ConsumeEnergy(25);
                this->_resource.comboPoints = 0;
                return;
            }
        }

        // Use single target rotation with Blade Flurry active
        ExecuteSingleTargetRotation(target);
    }

    void ExecuteStealthOpener(::Unit* target)
    {
        // Ambush from stealth for high damage
        if (this->CanCastSpell(AMBUSH, target))
        {
            this->CastSpell(target, AMBUSH);
            GenerateComboPoints(2);
            _inStealth = false;
            return;
        }

        // Cheap Shot for control
        if (this->CanCastSpell(CHEAP_SHOT, target))
        {
            this->CastSpell(target, CHEAP_SHOT);
            GenerateComboPoints(2);
            _inStealth = false;
            return;
        }
    }

private:
    void UpdateOutlawState()
    {
        uint32 now = getMSTime();

        // Update Roll the Bones buffs
        _rollTheBonesTracker.Update();

        // Check Blade Flurry expiry
        if (_bladeFlurryActive && now >= _bladeFlurryEndTime)
        {
            _bladeFlurryActive = false;
            _bladeFlurryEndTime = 0;
        }

        // Check Adrenaline Rush expiry
        if (_adrenalineRushActive && now >= _adrenalineRushEndTime)
        {
            _adrenalineRushActive = false;
            _adrenalineRushEndTime = 0;
        }

        // Regenerate energy (10 per second, 25 during Adrenaline Rush)
        static uint32 lastRegenTime = now;
        uint32 timeDiff = now - lastRegenTime;

        if (timeDiff >= 100) // Every 100ms
        {
            uint32 regenRate = _adrenalineRushActive ? 25 : 10;
            uint32 energyRegen = (timeDiff / 100) * (regenRate / 10);
            this->_resource.energy = std::min(this->_resource.energy + energyRegen, this->_resource.maxEnergy);
            lastRegenTime = now;
        }
    }

    void ConsumeEnergy(uint32 amount)
    {
        this->_resource.energy = (this->_resource.energy > amount) ? this->_resource.energy - amount : 0;
    }

    void GenerateComboPoints(uint32 amount)
    {
        this->_resource.comboPoints = std::min(this->_resource.comboPoints + amount, this->_resource.maxComboPoints);
    }

    float GetDistanceToTarget(::Unit* target) const
    {
        if (!target || !this->GetBot())
            return 1000.0f;
        return this->GetBot()->GetDistance(target);
    }

    void InitializeCooldowns()
    {
        this->RegisterCooldown(ADRENALINE_RUSH, 180000);       // 3 min CD
        this->RegisterCooldown(KILLING_SPREE, 120000);         // 2 min CD
        this->RegisterCooldown(BLADE_RUSH, 45000);             // 45 sec CD
        this->RegisterCooldown(VANISH, 120000);                // 2 min CD
        this->RegisterCooldown(CLOAK_OF_SHADOWS, 120000);      // 2 min CD
        this->RegisterCooldown(KICK, 15000);                   // 15 sec CD
        this->RegisterCooldown(BLIND, 120000);                 // 2 min CD
        this->RegisterCooldown(GOUGE, 15000);                  // 15 sec CD
        this->RegisterCooldown(MARKED_FOR_DEATH, 60000);       // 1 min CD
    }

private:
    RollTheBonesTracker _rollTheBonesTracker;
    bool _bladeFlurryActive;
    uint32 _bladeFlurryEndTime;
    bool _adrenalineRushActive;
    uint32 _adrenalineRushEndTime;
    bool _inStealth;
    uint32 _lastSinisterStrikeTime;
    uint32 _lastDispatchTime;
};

} // namespace Playerbot
