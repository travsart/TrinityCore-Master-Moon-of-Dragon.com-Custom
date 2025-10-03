/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * Windwalker Monk Refactored - Template-Based Implementation
 *
 * This file provides a complete, template-based implementation of Windwalker Monk
 * using the MeleeDpsSpecialization with dual resource system (Energy + Chi).
 */

#pragma once

#include "../CombatSpecializationTemplates.h"
#include "../ResourceTypes.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellAuraEffects.h"
#include "Log.h"
#include "MonkSpecialization.h"

namespace Playerbot
{

// ============================================================================
// WINDWALKER MONK SPELL IDs (WoW 11.2 - The War Within)
// ============================================================================

enum WindwalkerMonkSpells
{
    // Chi Generators
    TIGER_PALM_WIND          = 100780,  // 25 Energy, generates 2 Chi
    EXPEL_HARM_WIND          = 322101,  // 15 Energy, generates 1 Chi
    CHI_WAVE_WIND            = 115098,  // 15 sec CD, generates 1 Chi (talent)
    CHI_BURST_WIND           = 123986,  // 30 sec CD, generates 1 Chi (talent)
    CRACKLING_JADE_LIGHTNING = 117952,  // Channel, generates Chi

    // Chi Spenders
    RISING_SUN_KICK          = 107428,  // 2 Chi, 10 sec CD, applies debuff
    BLACKOUT_KICK            = 100784,  // 1 Chi, combo builder
    FISTS_OF_FURY            = 113656,  // 3 Chi, 24 sec CD, channel burst
    SPINNING_CRANE_KICK      = 101546,  // 2 Chi, AoE
    WHIRLING_DRAGON_PUNCH    = 152175,  // 2 Chi, 24 sec CD, burst (talent)

    // Strike of the Windlord
    STRIKE_OF_THE_WINDLORD   = 392983,  // 2 Chi, 40 sec CD, burst damage

    // Major Cooldowns
    STORM_EARTH_AND_FIRE     = 137639,  // 2 min CD, clone ability
    INVOKE_XUEN              = 123904,  // 2 min CD, summon celestial
    SERENITY                 = 152173,  // 1.5 min CD, burst window (talent)
    WEAPONS_OF_ORDER_WIND    = 387184,  // 2 min CD, damage buff (talent)

    // Utility
    TOUCH_OF_DEATH           = 322109,  // 3 min CD, execute
    TOUCH_OF_KARMA           = 122470,  // 1.5 min CD, damage absorption + reflect
    FORTIFYING_BREW_WIND     = 243435,  // 6 min CD, damage reduction
    DIFFUSE_MAGIC_WIND       = 122783,  // 1.5 min CD, magic immunity
    PARALYSIS_WIND           = 115078,  // CC
    LEG_SWEEP                = 119381,  // 1 min CD, AoE stun
    RING_OF_PEACE            = 116844,  // 45 sec CD, AoE silence (talent)

    // Movement
    ROLL_WIND                = 109132,  // Mobility
    FLYING_SERPENT_KICK      = 101545,  // 25 sec CD, mobility + damage
    TIGER_LUST_WIND          = 116841,  // Sprint + snare removal

    // Procs and Buffs
    TEACHINGS_OF_THE_MONASTERY_WIND = 202090, // Buff from Blackout Kick
    DANCE_OF_CHI_JI          = 325202,  // Spinning Crane Kick damage buff
    COMBO_BREAKER            = 137384,  // Proc: free Blackout Kick
    BLACKOUT_COMBO           = 196736,  // Talent: empowers next ability

    // Talents
    FAELINE_STOMP            = 388193,  // 30 sec CD, ground effect
    BONEDUST_BREW_WIND       = 386276,  // 1 min CD, damage amp
    FALLEN_ORDER             = 326860,  // 3 min CD, summon clones
    JADE_IGNITION            = 392979   // Passive: damage amp
};

// Dual resource type for Windwalker Monk
struct EnergyChiResourceWindwalker
{
    uint32 energy{0};
    uint32 chi{0};
    uint32 maxEnergy{100};
    uint32 maxChi{6};

    bool available{true};
    bool Consume(uint32 energyCost) {
        if (energy >= energyCost) {
            energy -= energyCost;
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
            maxEnergy = bot->GetMaxPower(POWER_ENERGY);
            energy = bot->GetPower(POWER_ENERGY);
        }
        chi = 0;
    }
};

// ============================================================================
// WINDWALKER HIT COMBO TRACKER
// ============================================================================

class WindwalkerHitComboTracker
{
public:
    WindwalkerHitComboTracker()
        : _lastSpellCast(0)
        , _comboCount(0)
    {
    }

    void RecordSpell(uint32 spellId)
    {
        if (_lastSpellCast == spellId)
        {
            // Broke combo by repeating same spell
            _comboCount = 1;
        }
        else
        {
            // Extended combo
            _comboCount++;
        }

        _lastSpellCast = spellId;
    }

    void Reset()
    {
        _lastSpellCast = 0;
        _comboCount = 0;
    }

    uint32 GetComboCount() const { return _comboCount; }
    float GetDamageMultiplier() const
    {
        // Hit Combo: 1% damage per stack (max 10%)
        return 1.0f + (std::min(_comboCount, 10u) * 0.01f);
    }

private:
    uint32 _lastSpellCast;
    uint32 _comboCount;
};

// ============================================================================
// WINDWALKER STORM EARTH AND FIRE TRACKER
// ============================================================================

class WindwalkerSEFTracker
{
public:
    WindwalkerSEFTracker()
        : _sefActive(false)
        , _sefEndTime(0)
    {
    }

    void Activate()
    {
        _sefActive = true;
        _sefEndTime = getMSTime() + 15000; // 15 sec duration
    }

    bool IsActive() const { return _sefActive; }

    uint32 GetTimeRemaining() const
    {
        if (!_sefActive)
            return 0;

        uint32 now = getMSTime();
        return _sefEndTime > now ? _sefEndTime - now : 0;
    }

    void Update()
    {
        if (_sefActive && getMSTime() >= _sefEndTime)
        {
            _sefActive = false;
            _sefEndTime = 0;
        }
    }

private:
    bool _sefActive;
    uint32 _sefEndTime;
};

// ============================================================================
// WINDWALKER MONK REFACTORED
// ============================================================================

class WindwalkerMonkRefactored : public MeleeDpsSpecialization<EnergyChiResourceWindwalker>, public MonkSpecialization
{
public:
    using Base = MeleeDpsSpecialization<EnergyChiResourceWindwalker>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::_resource;
    explicit WindwalkerMonkRefactored(Player* bot)
        : MeleeDpsSpecialization<EnergyChiResourceWindwalker>(bot)
        , MonkSpecialization(bot)
        , _hitComboTracker()
        , _sefTracker()
        , _lastRisingSunKickTime(0)
        , _comboBreaker(false)
    {
        // Initialize energy/chi resources
        this->_resource.Initialize(bot);

        InitializeCooldowns();

        TC_LOG_DEBUG("playerbot", "WindwalkerMonkRefactored initialized for {}", bot->GetName());
    }

    void UpdateRotation(::Unit* target) override
    {
        if (!target || !target->IsAlive() || !target->IsHostileTo(this->GetBot()))
            return;

        // Update Windwalker state
        UpdateWindwalkerState();

        // Use major cooldowns
        HandleCooldowns(target);

        // Determine if AoE or single target
        uint32 enemyCount = this->GetEnemiesInRange(8.0f);
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

    float GetOptimalRange(::Unit* target) override
    {
        return 5.0f; // Melee range
    }

protected:
    void ExecuteSingleTargetRotation(::Unit* target)
    {
        uint32 energy = this->_resource.energy;
        uint32 chi = this->_resource.chi;

        // Priority 1: Touch of Death (execute)
        if (target->GetHealthPct() < 15.0f && this->CanCastSpell(TOUCH_OF_DEATH, target))
        {
            this->CastSpell(target, TOUCH_OF_DEATH);
            _hitComboTracker.RecordSpell(TOUCH_OF_DEATH);
            return;
        }

        // Priority 2: Serenity burst window
        if (chi >= 4 && this->CanCastSpell(SERENITY, this->GetBot()))
        {
            this->CastSpell(SERENITY, this->GetBot());
            // During Serenity, spam Chi spenders
            return;
        }

        // Priority 3: Strike of the Windlord
        if (chi >= 2 && this->CanCastSpell(STRIKE_OF_THE_WINDLORD, target))
        {
            this->CastSpell(target, STRIKE_OF_THE_WINDLORD);
            _hitComboTracker.RecordSpell(STRIKE_OF_THE_WINDLORD);
            ConsumeChi(2);
            return;
        }

        // Priority 4: Rising Sun Kick (maintains debuff)
        if (chi >= 2 && this->CanCastSpell(RISING_SUN_KICK, target))
        {
            this->CastSpell(target, RISING_SUN_KICK);
            _lastRisingSunKickTime = getMSTime();
            _hitComboTracker.RecordSpell(RISING_SUN_KICK);
            ConsumeChi(2);
            return;
        }

        // Priority 5: Fists of Fury (channel burst)
        if (chi >= 3 && this->CanCastSpell(FISTS_OF_FURY, target))
        {
            this->CastSpell(target, FISTS_OF_FURY);
            _hitComboTracker.RecordSpell(FISTS_OF_FURY);
            ConsumeChi(3);
            return;
        }

        // Priority 6: Whirling Dragon Punch (talent)
        if (chi >= 2 && this->CanCastSpell(WHIRLING_DRAGON_PUNCH, target))
        {
            this->CastSpell(target, WHIRLING_DRAGON_PUNCH);
            _hitComboTracker.RecordSpell(WHIRLING_DRAGON_PUNCH);
            ConsumeChi(2);
            return;
        }

        // Priority 7: Blackout Kick (combo builder)
        if (chi >= 1)
        {
            // Use Combo Breaker proc if available
            if (_comboBreaker || this->CanCastSpell(BLACKOUT_KICK, target))
            {
                this->CastSpell(target, BLACKOUT_KICK);
                _hitComboTracker.RecordSpell(BLACKOUT_KICK);
                ConsumeChi(1);
                _comboBreaker = false;
                return;
            }
        }

        // Priority 8: Tiger Palm (Chi generator)
        if (energy >= 25 && chi < 5 && this->CanCastSpell(TIGER_PALM_WIND, target))
        {
            this->CastSpell(target, TIGER_PALM_WIND);
            _hitComboTracker.RecordSpell(TIGER_PALM_WIND);
            GenerateChi(2);
            return;
        }

        // Priority 9: Expel Harm (Chi generator + heal)
        if (energy >= 15 && chi < 5 && this->CanCastSpell(EXPEL_HARM_WIND, this->GetBot()))
        {
            this->CastSpell(EXPEL_HARM_WIND, this->GetBot());
            _hitComboTracker.RecordSpell(EXPEL_HARM_WIND);
            GenerateChi(1);
            return;
        }

        // Priority 10: Chi Wave (talent)
        if (chi < 5 && this->CanCastSpell(CHI_WAVE_WIND, target))
        {
            this->CastSpell(target, CHI_WAVE_WIND);
            _hitComboTracker.RecordSpell(CHI_WAVE_WIND);
            GenerateChi(1);
            return;
        }
    }

    void ExecuteAoERotation(::Unit* target, uint32 enemyCount)
    {
        uint32 energy = this->_resource.energy;
        uint32 chi = this->_resource.chi;

        // Priority 1: Fists of Fury (best AoE Chi spender)
        if (chi >= 3 && this->CanCastSpell(FISTS_OF_FURY, target))
        {
            this->CastSpell(target, FISTS_OF_FURY);
            _hitComboTracker.RecordSpell(FISTS_OF_FURY);
            ConsumeChi(3);
            return;
        }

        // Priority 2: Whirling Dragon Punch
        if (chi >= 2 && this->CanCastSpell(WHIRLING_DRAGON_PUNCH, target))
        {
            this->CastSpell(target, WHIRLING_DRAGON_PUNCH);
            _hitComboTracker.RecordSpell(WHIRLING_DRAGON_PUNCH);
            ConsumeChi(2);
            return;
        }

        // Priority 3: Spinning Crane Kick (AoE Chi spender)
        if (chi >= 2 && this->CanCastSpell(SPINNING_CRANE_KICK, this->GetBot()))
        {
            this->CastSpell(SPINNING_CRANE_KICK, this->GetBot());
            _hitComboTracker.RecordSpell(SPINNING_CRANE_KICK);
            ConsumeChi(2);
            return;
        }

        // Priority 4: Rising Sun Kick
        if (chi >= 2 && this->CanCastSpell(RISING_SUN_KICK, target))
        {
            this->CastSpell(target, RISING_SUN_KICK);
            _hitComboTracker.RecordSpell(RISING_SUN_KICK);
            ConsumeChi(2);
            return;
        }

        // Priority 5: Tiger Palm (Chi generator)
        if (energy >= 25 && chi < 5 && this->CanCastSpell(TIGER_PALM_WIND, target))
        {
            this->CastSpell(target, TIGER_PALM_WIND);
            _hitComboTracker.RecordSpell(TIGER_PALM_WIND);
            GenerateChi(2);
            return;
        }

        // Priority 6: Chi Wave
        if (chi < 5 && this->CanCastSpell(CHI_WAVE_WIND, target))
        {
            this->CastSpell(target, CHI_WAVE_WIND);
            _hitComboTracker.RecordSpell(CHI_WAVE_WIND);
            GenerateChi(1);
            return;
        }
    }

    void HandleCooldowns(::Unit* target)
    {
        uint32 chi = this->_resource.chi;

        // Use Storm, Earth, and Fire for burst
        if (chi >= 3 && !_sefTracker.IsActive() && this->CanCastSpell(STORM_EARTH_AND_FIRE, this->GetBot()))
        {
            this->CastSpell(STORM_EARTH_AND_FIRE, this->GetBot());
            _sefTracker.Activate();
            TC_LOG_DEBUG("playerbot", "Windwalker: Storm, Earth, and Fire activated");
        }

        // Invoke Xuen the White Tiger
        if (chi >= 3 && this->CanCastSpell(INVOKE_XUEN, this->GetBot()))
        {
            this->CastSpell(INVOKE_XUEN, this->GetBot());
            TC_LOG_DEBUG("playerbot", "Windwalker: Invoke Xuen");
        }

        // Touch of Karma (damage absorption + reflect)
        if (this->GetBot()->GetHealthPct() < 70.0f && this->CanCastSpell(TOUCH_OF_KARMA, target))
        {
            this->CastSpell(target, TOUCH_OF_KARMA);
            TC_LOG_DEBUG("playerbot", "Windwalker: Touch of Karma");
        }
    }

    void HandleDefensiveCooldowns()
    {
        Player* bot = this->GetBot();
        float healthPct = bot->GetHealthPct();

        // Fortifying Brew
        if (healthPct < 40.0f && this->CanCastSpell(FORTIFYING_BREW_WIND, bot))
        {
            this->CastSpell(bot, FORTIFYING_BREW_WIND);
            TC_LOG_DEBUG("playerbot", "Windwalker: Fortifying Brew");
            return;
        }

        // Diffuse Magic
        if (healthPct < 50.0f && this->CanCastSpell(DIFFUSE_MAGIC_WIND, bot))
        {
            this->CastSpell(bot, DIFFUSE_MAGIC_WIND);
            TC_LOG_DEBUG("playerbot", "Windwalker: Diffuse Magic");
            return;
        }
    }

private:
    void UpdateWindwalkerState()
    {
        // Update SEF tracker
        _sefTracker.Update();

        // Update Combo Breaker proc
        if (this->GetBot() && this->GetBot()->HasAura(COMBO_BREAKER))
            _comboBreaker = true;
        else
            _comboBreaker = false;

        // Update Chi and Energy from bot
        if (this->GetBot())
        {
            this->_resource.chi = this->GetBot()->GetPower(POWER_CHI);
            this->_resource.energy = this->GetBot()->GetPower(POWER_ENERGY);
        }
    }

    void GenerateChi(uint32 amount)
    {
        this->_resource.chi = std::min(this->_resource.chi + amount, this->_resource.maxChi);
    }

    void ConsumeChi(uint32 amount)
    {
        this->_resource.chi = (this->_resource.chi > amount) ? this->_resource.chi - amount : 0;
    }

    void InitializeCooldowns()
    {
        RegisterCooldown(RISING_SUN_KICK, 10000);       // 10 sec CD
        RegisterCooldown(FISTS_OF_FURY, 24000);         // 24 sec CD
        RegisterCooldown(WHIRLING_DRAGON_PUNCH, 24000); // 24 sec CD
        RegisterCooldown(STRIKE_OF_THE_WINDLORD, 40000);// 40 sec CD
        RegisterCooldown(STORM_EARTH_AND_FIRE, 120000); // 2 min CD
        RegisterCooldown(INVOKE_XUEN, 120000);          // 2 min CD
        RegisterCooldown(SERENITY, 90000);              // 1.5 min CD
        RegisterCooldown(TOUCH_OF_DEATH, 180000);       // 3 min CD
        RegisterCooldown(TOUCH_OF_KARMA, 90000);        // 1.5 min CD
        RegisterCooldown(FORTIFYING_BREW_WIND, 360000); // 6 min CD
        RegisterCooldown(DIFFUSE_MAGIC_WIND, 90000);    // 1.5 min CD
        RegisterCooldown(LEG_SWEEP, 60000);             // 1 min CD
        RegisterCooldown(RING_OF_PEACE, 45000);         // 45 sec CD
        RegisterCooldown(CHI_WAVE_WIND, 15000);         // 15 sec CD
        RegisterCooldown(CHI_BURST_WIND, 30000);        // 30 sec CD
    }

private:
    WindwalkerHitComboTracker _hitComboTracker;
    WindwalkerSEFTracker _sefTracker;
    uint32 _lastRisingSunKickTime;
    bool _comboBreaker;
};

} // namespace Playerbot
