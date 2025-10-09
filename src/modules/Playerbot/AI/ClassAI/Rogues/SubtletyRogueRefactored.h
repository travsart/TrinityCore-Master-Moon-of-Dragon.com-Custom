/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * Subtlety Rogue Refactored - Template-Based Implementation
 *
 * This file provides a complete, template-based implementation of Subtlety Rogue
 * using the MeleeDpsSpecialization with dual resource system (Energy + Combo Points).
 */

#pragma once

#include "../CombatSpecializationTemplates.h"
#include "../ResourceTypes.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellAuraEffects.h"
#include "Log.h"
#include "RogueSpecialization.h"
#include "RogueResourceTypes.h"

namespace Playerbot
{

// NOTE: Common Rogue spell IDs are defined in RogueSpecialization.h
// NOTE: EnergyComboResource is defined in RogueResourceTypes.h
// NOTE: Shared spells (SHADOW_DANCE, SYMBOLS_OF_DEATH, EVASION, SAP, etc.) are in RogueSpecialization.h
// Only Subtlety-unique spell IDs defined below to avoid duplicate definition errors

// ============================================================================
// SUBTLETY ROGUE SPELL IDs (WoW 11.2 - The War Within)
// ============================================================================

enum SubtletySpells
{
    // NOTE: Shared spells (BACKSTAB, RUPTURE, STEALTH, VANISH, KICK, etc.) are in RogueSpecialization.h
    // Only Subtlety-UNIQUE spells defined here to avoid duplicate definitions

    // Combo Point Builders (Unique to Subtlety)
    SHADOWSTRIKE_SUB         = 185438,  // From stealth/Shadow Dance, 2 CP (unique ID)
    SHURIKEN_STORM           = 197835,  // 35 Energy, AoE, 1 CP per target

    // Combo Point Spenders (Unique to Subtlety)
    EVISCERATE_SUB           = 196819,  // Finisher, high damage (unique Subtlety version)
    BLACK_POWDER             = 319175,  // AoE finisher
    SECRET_TECHNIQUE         = 280719,  // Finisher, teleport attacks (talent)

    // Major Cooldowns
    SHADOW_BLADES            = 121471,  // 3 min CD, all attacks generate CP (talent)
    SHURIKEN_TORNADO         = 277925,  // 1 min CD, sustained AoE (talent)

    // Shadow Techniques
    SHADOW_TECHNIQUES_PROC   = 196911,  // Passive extra CP generation

    // Finisher Buffs
    SLICE_AND_DICE_SUB       = 315496,  // Attack speed buff (unique ID)

    // Procs and Buffs
    DANSE_MACABRE            = 393969,  // Buff from spending CP
    DEEPER_DAGGERS           = 383405,  // Eviscerate increases next Eviscerate

    // Talents
    DARK_SHADOW              = 245687,  // Shadow Dance CDR
    DEEPER_STRATAGEM_SUB     = 193531,  // 6 max combo points
    MARKED_FOR_DEATH_SUB     = 137619,  // Instant 5 CP
    SHURIKEN_TORNADO_TALENT  = 277925   // AoE sustained damage
};

// ============================================================================
// SHADOW DANCE TRACKER
// ============================================================================

class ShadowDanceTracker
{
public:
    ShadowDanceTracker()
        : _charges(3)
        , _maxCharges(3)
        , _active(false)
        , _endTime(0)
        , _lastUseTime(0)
        , _lastRechargeTime(0)
        , _chargeCooldown(60000) // 60 sec per charge
        , _duration(8000)        // 8 sec duration
    {
    }

    void Update()
    {
        uint32 now = getMSTime();

        // Check if Shadow Dance expired
        if (_active && now >= _endTime)
        {
            _active = false;
            _endTime = 0;
        }

        // Recharge charges
        if (_charges < _maxCharges)
        {
            uint32 timeSinceRecharge = now - _lastRechargeTime;
            if (timeSinceRecharge >= _chargeCooldown)
            {
                _charges++;
                _lastRechargeTime = now;
            }
        }
    }

    bool CanUse() const
    {
        return _charges > 0 && !_active;
    }

    bool IsActive() const { return _active; }
    uint32 GetCharges() const { return _charges; }

    uint32 GetTimeRemaining() const
    {
        if (!_active)
            return 0;

        uint32 now = getMSTime();
        return _endTime > now ? _endTime - now : 0;
    }

    void Use()
    {
        if (_charges > 0)
        {
            _charges--;
            _lastUseTime = getMSTime();
            _active = true;
            _endTime = _lastUseTime + _duration;

            if (_charges == (_maxCharges - 1))
                _lastRechargeTime = _lastUseTime;
        }
    }

    bool ShouldUse(uint32 comboPoints) const
    {
        // Use Shadow Dance when:
        // 1. Have charges available
        // 2. Low combo points to build quickly
        // 3. Not already active

        if (!CanUse())
            return false;

        // Use to build combo points quickly
        if (comboPoints < 3)
            return true;

        // Use all charges eventually
        if (_charges == _maxCharges)
            return true;

        return false;
    }

private:
    uint32 _charges;
    const uint32 _maxCharges;
    bool _active;
    uint32 _endTime;
    uint32 _lastUseTime;
    uint32 _lastRechargeTime;
    const uint32 _chargeCooldown;
    const uint32 _duration;
};

// ============================================================================
// SUBTLETY ROGUE REFACTORED
// ============================================================================

class SubtletyRogueRefactored : public MeleeDpsSpecialization<EnergyComboResource>, public RogueSpecialization
{
public:
    // Use base class members with type alias for cleaner syntax
    using Base = MeleeDpsSpecialization<EnergyComboResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::_resource;
    explicit SubtletyRogueRefactored(Player* bot)
        : MeleeDpsSpecialization<EnergyComboResource>(bot)
        , RogueSpecialization(bot)
        , _shadowDanceTracker()
        , _inStealth(false)
        , _symbolsOfDeathActive(false)
        , _symbolsOfDeathEndTime(0)
        , _shadowBladesActive(false)
        , _shadowBladesEndTime(0)
        , _lastBackstabTime(0)
        , _lastShadowstrikeTime(0)
        , _lastEviscerateTime(0)
    {
        // Initialize energy/combo resources
        this->_resource.maxEnergy = 100;
        this->_resource.maxComboPoints = bot->HasSpell(DEEPER_STRATAGEM_SUB) ? 6 : 5;
        this->_resource.energy = this->_resource.maxEnergy;
        this->_resource.comboPoints = 0;

        InitializeCooldowns();

        TC_LOG_DEBUG("playerbot", "SubtletyRogueRefactored initialized for {}", bot->GetName());
    }

    void UpdateRotation(::Unit* target) override
    {
        if (!target || !target->IsAlive() || !target->IsHostileTo(this->GetBot()))
            return;

        // Update tracking systems
        UpdateSubtletyState();

        // Check stealth status (stealth or Shadow Dance)
        _inStealth = this->GetBot()->HasAuraType(SPELL_AURA_MOD_STEALTH) || _shadowDanceTracker.IsActive();

        // Main rotation
        uint32 enemyCount = this->GetEnemiesInRange(10.0f);
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

        if (bot->GetHealthPct() < 50.0f && this->CanCastSpell(EVASION, bot))
        {
            this->CastSpell(bot, EVASION);
        }
    }

    // NOTE: GetOptimalRange is final in base class, cannot override
    // Melee range (5.0f) is already handled by MeleeDpsSpecialization

protected:
    void ExecuteSingleTargetRotation(::Unit* target)
    {
        uint32 energy = this->_resource.energy;
        uint32 cp = this->_resource.comboPoints;
        uint32 maxCp = this->_resource.maxComboPoints;

        // Priority 1: Symbols of Death on cooldown
        if (this->CanCastSpell(SYMBOLS_OF_DEATH, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), SYMBOLS_OF_DEATH);
            _symbolsOfDeathActive = true;
            _symbolsOfDeathEndTime = getMSTime() + 10000;
            return;
        }

        // Priority 2: Shadow Blades on cooldown (talent)
        if (this->CanCastSpell(SHADOW_BLADES, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), SHADOW_BLADES);
            _shadowBladesActive = true;
            _shadowBladesEndTime = getMSTime() + 20000;
            return;
        }

        // Priority 3: Shadow Dance to build combo points
        if (_shadowDanceTracker.ShouldUse(cp))
        {
            if (this->CanCastSpell(SHADOW_DANCE, this->GetBot()))
            {
                this->CastSpell(this->GetBot(), SHADOW_DANCE);
                _shadowDanceTracker.Use();
                _inStealth = true; // Enables stealth abilities
                return;
            }
        }

        // Priority 4: Shadowstrike from stealth/Shadow Dance
        if (_inStealth && energy >= 40)
        {
            if (this->CanCastSpell(SHADOWSTRIKE_SUB, target))
            {
                this->CastSpell(target, SHADOWSTRIKE_SUB);
                _lastShadowstrikeTime = getMSTime();
                ConsumeEnergy(40);
                GenerateComboPoints(2);
                // Shadow Blades makes all attacks give CP
                if (_shadowBladesActive)
                    GenerateComboPoints(1);
                return;
            }
        }

        // Priority 5: Secret Technique finisher at 5-6 CP (talent)
        if (cp >= maxCp && energy >= 30)
        {
            if (this->GetBot()->HasSpell(SECRET_TECHNIQUE) && this->CanCastSpell(SECRET_TECHNIQUE, target))
            {
                this->CastSpell(target, SECRET_TECHNIQUE);
                ConsumeEnergy(30);
                this->_resource.comboPoints = 0;
                return;
            }
        }

        // Priority 6: Eviscerate finisher at 5-6 CP
        if (cp >= (maxCp - 1) && energy >= 35)
        {
            if (this->CanCastSpell(EVISCERATE_SUB, target))
            {
                this->CastSpell(target, EVISCERATE_SUB);
                _lastEviscerateTime = getMSTime();
                ConsumeEnergy(35);
                this->_resource.comboPoints = 0;
                return;
            }
        }

        // Priority 7: Rupture if not active
        if (!HasRupture(target) && cp >= 4 && energy >= 25)
        {
            if (this->CanCastSpell(RUPTURE, target))
            {
                this->CastSpell(target, RUPTURE);
                ConsumeEnergy(25);
                this->_resource.comboPoints = 0;
                return;
            }
        }

        // Priority 8: Backstab for combo points (from behind)
        if (energy >= 35 && cp < maxCp)
        {
            if (this->IsBehindTarget(target) && this->CanCastSpell(BACKSTAB, target))
            {
                this->CastSpell(target, BACKSTAB);
                _lastBackstabTime = getMSTime();
                ConsumeEnergy(35);
                GenerateComboPoints(1);
                if (_shadowBladesActive)
                    GenerateComboPoints(1);
                return;
            }
        }

        // Priority 9: Shadowstrike if can't get behind (less efficient)
        if (energy >= 40 && cp < maxCp)
        {
            if (this->CanCastSpell(SHADOWSTRIKE_SUB, target))
            {
                this->CastSpell(target, SHADOWSTRIKE_SUB);
                ConsumeEnergy(40);
                GenerateComboPoints(2);
                return;
            }
        }
    }

    void ExecuteAoERotation(::Unit* target, uint32 enemyCount)
    {
        uint32 energy = this->_resource.energy;
        uint32 cp = this->_resource.comboPoints;
        uint32 maxCp = this->_resource.maxComboPoints;

        // Priority 1: Shuriken Tornado (talent, sustained AoE)
        if (this->CanCastSpell(SHURIKEN_TORNADO_TALENT, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), SHURIKEN_TORNADO_TALENT);
            return;
        }

        // Priority 2: Symbols of Death
        if (this->CanCastSpell(SYMBOLS_OF_DEATH, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), SYMBOLS_OF_DEATH);
            _symbolsOfDeathActive = true;
            _symbolsOfDeathEndTime = getMSTime() + 10000;
            return;
        }

        // Priority 3: Shadow Dance
        if (_shadowDanceTracker.CanUse())
        {
            if (this->CanCastSpell(SHADOW_DANCE, this->GetBot()))
            {
                this->CastSpell(this->GetBot(), SHADOW_DANCE);
                _shadowDanceTracker.Use();
                _inStealth = true;
                return;
            }
        }

        // Priority 4: Black Powder finisher at 5+ CP
        if (cp >= 5 && energy >= 35)
        {
            if (this->CanCastSpell(BLACK_POWDER, this->GetBot()))
            {
                this->CastSpell(this->GetBot(), BLACK_POWDER);
                ConsumeEnergy(35);
                this->_resource.comboPoints = 0;
                return;
            }
        }

        // Priority 5: Shuriken Storm for AoE combo building
        if (energy >= 35 && cp < maxCp)
        {
            if (this->CanCastSpell(SHURIKEN_STORM, this->GetBot()))
            {
                this->CastSpell(this->GetBot(), SHURIKEN_STORM);
                ConsumeEnergy(35);
                GenerateComboPoints(std::min(enemyCount, 5u));
                return;
            }
        }

        // Fallback to single target if AoE abilities on CD
        ExecuteSingleTargetRotation(target);
    }

private:
    void UpdateSubtletyState()
    {
        uint32 now = getMSTime();

        // Update Shadow Dance tracker
        _shadowDanceTracker.Update();

        // Check Symbols of Death expiry
        if (_symbolsOfDeathActive && now >= _symbolsOfDeathEndTime)
        {
            _symbolsOfDeathActive = false;
            _symbolsOfDeathEndTime = 0;
        }

        // Check Shadow Blades expiry
        if (_shadowBladesActive && now >= _shadowBladesEndTime)
        {
            _shadowBladesActive = false;
            _shadowBladesEndTime = 0;
        }

        // Regenerate energy (10 per second)
        static uint32 lastRegenTime = now;
        uint32 timeDiff = now - lastRegenTime;

        if (timeDiff >= 100) // Every 100ms
        {
            uint32 energyRegen = (timeDiff / 100);
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

    bool IsBehindTarget(::Unit* target) const
    {
        if (!target || !this->GetBot())
            return false;

        // Check if bot is behind target (simplified)
        return GetBot()->isInBack(target);
    }

    bool HasRupture(::Unit* target) const
    {
        // Simplified - check if target has Rupture aura
        return target && target->HasAura(RUPTURE, this->GetBot()->GetGUID());
    }

    void InitializeCooldowns()
    {
        RegisterCooldown(SHADOW_DANCE, 60000);           // 60 sec per charge
        RegisterCooldown(SYMBOLS_OF_DEATH, 30000);       // 30 sec CD
        RegisterCooldown(SHADOW_BLADES, 180000);         // 3 min CD
        RegisterCooldown(SHURIKEN_TORNADO_TALENT, 60000); // 1 min CD
        RegisterCooldown(VANISH, 120000);            // 2 min CD
        RegisterCooldown(CLOAK_OF_SHADOWS, 120000);  // 2 min CD
        RegisterCooldown(EVASION, 120000);               // 2 min CD
        RegisterCooldown(KICK, 15000);               // 15 sec CD
        RegisterCooldown(BLIND, 120000);             // 2 min CD
        RegisterCooldown(MARKED_FOR_DEATH_SUB, 60000);   // 1 min CD
    }

private:
    ShadowDanceTracker _shadowDanceTracker;
    bool _inStealth;
    bool _symbolsOfDeathActive;
    uint32 _symbolsOfDeathEndTime;
    bool _shadowBladesActive;
    uint32 _shadowBladesEndTime;
    uint32 _lastBackstabTime;
    uint32 _lastShadowstrikeTime;
    uint32 _lastEviscerateTime;
};

} // namespace Playerbot
