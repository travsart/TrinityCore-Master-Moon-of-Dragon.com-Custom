/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * Brewmaster Monk Refactored - Template-Based Implementation
 *
 * This file provides a complete, template-based implementation of Brewmaster Monk
 * using the TankSpecialization with dual resource system (Energy + Chi).
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
// BREWMASTER MONK SPELL IDs (WoW 11.2 - The War Within)
// ============================================================================

enum BrewmasterMonkSpells
{
    // Chi Generators
    KEG_SMASH                = 121253,  // 40 Energy, 8 sec CD, generates 2 Chi
    TIGER_PALM_BREW          = 100780,  // 25 Energy, generates 2 Chi
    EXPEL_HARM_BREW          = 322101,  // 15 Energy, generates 1 Chi, self-heal
    CHI_WAVE                 = 115098,  // 15 sec CD, generates 1 Chi (talent)
    CHI_BURST                = 123986,  // 30 sec CD, generates 1 Chi (talent)

    // Chi Spenders
    BLACKOUT_KICK_BREW       = 205523,  // 1-3 Chi, reduces brew cooldown
    BREATH_OF_FIRE           = 115181,  // 2 Chi, cone AoE + DoT
    SPINNING_CRANE_KICK_BREW = 322729,  // 2 Chi, AoE channel

    // Active Mitigation (Brews)
    PURIFYING_BREW           = 119582,  // Removes Stagger damage
    CELESTIAL_BREW           = 322507,  // 1 min CD, absorb shield
    FORTIFYING_BREW_BREW     = 115203,  // 6 min CD, damage reduction + max HP

    // Stagger Management
    IRONSKIN_BREW            = 115308,  // Increases Stagger effectiveness
    SHUFFLE                  = 215479,  // Buff from Blackout Kick

    // Threat Generation
    PROVOKE                  = 115546,  // Taunt
    RISING_SUN_KICK_BREW     = 107428,  // 2 Chi, threat modifier

    // Major Cooldowns
    INVOKE_NIUZAO            = 132578,  // 3 min CD, summon statue (talent)
    WEAPONS_OF_ORDER         = 387184,  // 2 min CD, damage/defense buff (talent)
    BONEDUST_BREW            = 386276,  // 1 min CD, damage amp (talent)

    // Utility
    TRANSCENDENCE            = 101643,  // Teleport anchor
    TRANSCENDENCE_TRANSFER   = 119996,  // Teleport to anchor
    ROLL                     = 109132,  // Mobility
    TIGER_LUST               = 116841,  // Sprint + snare removal
    DETOX                    = 218164,  // Dispel poison/disease

    // Defensive Cooldowns
    DAMPEN_HARM              = 122278,  // 2 min CD, damage reduction
    ZEN_MEDITATION           = 115176,  // 5 min CD, channel massive DR
    DIFFUSE_MAGIC            = 122783,  // 1.5 min CD, magic immunity (talent)

    // Procs and Buffs
    ELUSIVE_BRAWLER          = 195630,  // Passive dodge stacks
    GIFT_OF_THE_OX           = 124502,  // Healing orbs
    COUNTERSTRIKE            = 383800,  // Parry proc

    // Talents
    BLACK_OX_BREW            = 115399,  // Resets brew cooldowns
    CHARRED_PASSIONS         = 386965,  // Breath of Fire enhancement
    EXPLODING_KEG            = 325153   // Keg Smash knockdown
};

// Dual resource type for Monk
struct EnergyChiResource
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
// BREWMASTER STAGGER TRACKER
// ============================================================================

class BrewmasterStaggerTracker
{
public:
    BrewmasterStaggerTracker()
        : _staggerAmount(0)
        , _staggerPercent(0.0f)
        , _lastStaggerCheck(0)
    {
    }

    void UpdateStagger(Player* bot)
    {
        if (!bot)
            return;

        uint32 now = getMSTime();
        if (now - _lastStaggerCheck < 100) // Throttle checks
            return;

        _lastStaggerCheck = now;

        // Check for Stagger aura
        if (Aura* stagger = bot->GetAura(124255)) // Light Stagger
        {
            _staggerAmount = stagger->GetEffect(0)->GetAmount();
            _staggerPercent = (_staggerAmount * 100.0f) / bot->GetMaxHealth();
        }
        else if (Aura* stagger = bot->GetAura(124274)) // Moderate Stagger
        {
            _staggerAmount = stagger->GetEffect(0)->GetAmount();
            _staggerPercent = (_staggerAmount * 100.0f) / bot->GetMaxHealth();
        }
        else if (Aura* stagger = bot->GetAura(124273)) // Heavy Stagger
        {
            _staggerAmount = stagger->GetEffect(0)->GetAmount();
            _staggerPercent = (_staggerAmount * 100.0f) / bot->GetMaxHealth();
        }
        else
        {
            _staggerAmount = 0;
            _staggerPercent = 0.0f;
        }
    }

    uint32 GetStaggerAmount() const { return _staggerAmount; }
    float GetStaggerPercent() const { return _staggerPercent; }

    bool IsHeavyStagger() const { return _staggerPercent > 6.0f; }
    bool IsModerateStagger() const { return _staggerPercent > 3.0f; }
    bool ShouldPurify() const { return _staggerPercent > 4.0f; } // Purify at 4%+ of max HP

private:
    uint32 _staggerAmount;
    float _staggerPercent;
    uint32 _lastStaggerCheck;
};

// ============================================================================
// BREWMASTER SHUFFLE TRACKER
// ============================================================================

class BrewmasterShuffleTracker
{
public:
    BrewmasterShuffleTracker()
        : _shuffleActive(false)
        , _shuffleEndTime(0)
    {
    }

    void ApplyShuffle()
    {
        _shuffleActive = true;
        _shuffleEndTime = getMSTime() + 5000; // 5 sec base duration
    }

    void ExtendShuffle(uint32 durationMs)
    {
        if (_shuffleActive)
            _shuffleEndTime += durationMs;
        else
        {
            _shuffleActive = true;
            _shuffleEndTime = getMSTime() + durationMs;
        }
    }

    bool IsActive() const { return _shuffleActive; }

    uint32 GetTimeRemaining() const
    {
        if (!_shuffleActive)
            return 0;

        uint32 now = getMSTime();
        return _shuffleEndTime > now ? _shuffleEndTime - now : 0;
    }

    bool NeedsRefresh() const
    {
        // Refresh if less than 2 seconds remaining
        return !_shuffleActive || GetTimeRemaining() < 2000;
    }

    void Update()
    {
        if (_shuffleActive && getMSTime() >= _shuffleEndTime)
        {
            _shuffleActive = false;
            _shuffleEndTime = 0;
        }
    }

private:
    bool _shuffleActive;
    uint32 _shuffleEndTime;
};

// ============================================================================
// BREWMASTER MONK REFACTORED
// ============================================================================

class BrewmasterMonkRefactored : public TankSpecialization<EnergyChiResource>, public MonkSpecialization
{
public:
    using Base = TankSpecialization<EnergyChiResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::_resource;
    explicit BrewmasterMonkRefactored(Player* bot)
        : TankSpecialization<EnergyChiResource>(bot)
        , MonkSpecialization(bot)
        , _staggerTracker()
        , _shuffleTracker()
        , _ironskinBrewActive(false)
        , _ironskinEndTime(0)
        , _lastKegSmashTime(0)
    {
        // Initialize energy/chi resources
        this->_resource.Initialize(bot);

        InitializeCooldowns();

        TC_LOG_DEBUG("playerbot", "BrewmasterMonkRefactored initialized for {}", bot->GetName());
    }

    void UpdateRotation(::Unit* target) override
    {
        if (!target || !target->IsAlive() || !target->IsHostileTo(this->GetBot()))
            return;

        // Update Brewmaster state
        UpdateBrewmasterState();

        // Handle active mitigation first
        HandleActiveMitigation();

        // Determine if AoE or single target
        uint32 enemyCount = this->GetEnemiesInRange(8.0f);
        if (enemyCount >= 3)
        {
            ExecuteAoEThreatRotation(target, enemyCount);
        }
        else
        {
            ExecuteSingleTargetThreatRotation(target);
        }
    }

    void UpdateBuffs() override
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        // Emergency defensives
        HandleEmergencyDefensives();
    }

    void OnTauntRequired(::Unit* target) override
    {
        if (this->CanCastSpell(PROVOKE, target))
        {
            this->CastSpell(target, PROVOKE);
            TC_LOG_DEBUG("playerbot", "Brewmaster: Taunt cast on {}", target->GetName());
        }
    }

    float GetOptimalRange(::Unit* target) override
    {
        return 5.0f; // Melee range for tanking
    }

protected:
    void ExecuteSingleTargetThreatRotation(::Unit* target)
    {
        uint32 energy = this->_resource.energy;
        uint32 chi = this->_resource.chi;

        // Priority 1: Maintain Shuffle
        if (_shuffleTracker.NeedsRefresh() && chi >= 1)
        {
            if (this->CanCastSpell(BLACKOUT_KICK_BREW, target))
            {
                this->CastSpell(target, BLACKOUT_KICK_BREW);
                _shuffleTracker.ApplyShuffle();
                ConsumeChi(1);
                return;
            }
        }

        // Priority 2: Keg Smash on cooldown (best Chi generator + threat)
        if (energy >= 40 && chi < 5 && this->CanCastSpell(KEG_SMASH, target))
        {
            this->CastSpell(target, KEG_SMASH);
            _lastKegSmashTime = getMSTime();
            GenerateChi(2);
            return;
        }

        // Priority 3: Breath of Fire (after Keg Smash for ignite)
        if (chi >= 2 && getMSTime() - _lastKegSmashTime < 2000)
        {
            if (this->CanCastSpell(BREATH_OF_FIRE, target))
            {
                this->CastSpell(target, BREATH_OF_FIRE);
                ConsumeChi(2);
                return;
            }
        }

        // Priority 4: Rising Sun Kick for threat
        if (chi >= 2 && this->CanCastSpell(RISING_SUN_KICK_BREW, target))
        {
            this->CastSpell(target, RISING_SUN_KICK_BREW);
            ConsumeChi(2);
            return;
        }

        // Priority 5: Blackout Kick to spend Chi
        if (chi >= 3 && this->CanCastSpell(BLACKOUT_KICK_BREW, target))
        {
            this->CastSpell(target, BLACKOUT_KICK_BREW);
            _shuffleTracker.ExtendShuffle(5000);
            ConsumeChi(1);
            return;
        }

        // Priority 6: Tiger Palm for Chi generation
        if (energy >= 25 && chi < 5 && this->CanCastSpell(TIGER_PALM_BREW, target))
        {
            this->CastSpell(target, TIGER_PALM_BREW);
            GenerateChi(2);
            return;
        }

        // Priority 7: Expel Harm (heal + Chi)
        if (energy >= 15 && chi < 5 && this->GetBot()->GetHealthPct() < 90.0f)
        {
            if (this->CanCastSpell(EXPEL_HARM_BREW, this->GetBot()))
            {
                this->CastSpell(EXPEL_HARM_BREW, this->GetBot());
                GenerateChi(1);
                return;
            }
        }
    }

    void ExecuteAoEThreatRotation(::Unit* target, uint32 enemyCount)
    {
        uint32 energy = this->_resource.energy;
        uint32 chi = this->_resource.chi;

        // Priority 1: Maintain Shuffle
        if (_shuffleTracker.NeedsRefresh() && chi >= 1)
        {
            if (this->CanCastSpell(BLACKOUT_KICK_BREW, target))
            {
                this->CastSpell(target, BLACKOUT_KICK_BREW);
                _shuffleTracker.ApplyShuffle();
                ConsumeChi(1);
                return;
            }
        }

        // Priority 2: Keg Smash (AoE Chi generator)
        if (energy >= 40 && chi < 5 && this->CanCastSpell(KEG_SMASH, target))
        {
            this->CastSpell(target, KEG_SMASH);
            _lastKegSmashTime = getMSTime();
            GenerateChi(2);
            return;
        }

        // Priority 3: Breath of Fire (AoE + DoT)
        if (chi >= 2 && this->CanCastSpell(BREATH_OF_FIRE, target))
        {
            this->CastSpell(target, BREATH_OF_FIRE);
            ConsumeChi(2);
            return;
        }

        // Priority 4: Spinning Crane Kick (AoE Chi spender)
        if (chi >= 2 && enemyCount >= 4 && this->CanCastSpell(SPINNING_CRANE_KICK_BREW, this->GetBot()))
        {
            this->CastSpell(SPINNING_CRANE_KICK_BREW, this->GetBot());
            ConsumeChi(2);
            return;
        }

        // Priority 5: Tiger Palm for Chi generation
        if (energy >= 25 && chi < 5 && this->CanCastSpell(TIGER_PALM_BREW, target))
        {
            this->CastSpell(target, TIGER_PALM_BREW);
            GenerateChi(2);
            return;
        }
    }

    void HandleActiveMitigation()
    {
        Player* bot = this->GetBot();
        if (!bot)
            return;

        // Update Ironskin Brew status
        UpdateIronskinBrew();

        // Priority 1: Purify heavy Stagger
        if (_staggerTracker.ShouldPurify() && this->CanCastSpell(PURIFYING_BREW, bot))
        {
            this->CastSpell(bot, PURIFYING_BREW);
            TC_LOG_DEBUG("playerbot", "Brewmaster: Purifying Brew used - Stagger at {:.1f}%", _staggerTracker.GetStaggerPercent());
            return;
        }

        // Priority 2: Maintain Ironskin Brew
        if (!_ironskinBrewActive || GetIronskinTimeRemaining() < 3000)
        {
            if (this->CanCastSpell(IRONSKIN_BREW, bot))
            {
                this->CastSpell(bot, IRONSKIN_BREW);
                _ironskinBrewActive = true;
                _ironskinEndTime = getMSTime() + 7000; // 7 sec duration
                TC_LOG_DEBUG("playerbot", "Brewmaster: Ironskin Brew applied");
                return;
            }
        }
    }

    void HandleEmergencyDefensives()
    {
        Player* bot = this->GetBot();
        float healthPct = bot->GetHealthPct();

        // Critical: Fortifying Brew
        if (healthPct < 25.0f && this->CanCastSpell(FORTIFYING_BREW_BREW, bot))
        {
            this->CastSpell(bot, FORTIFYING_BREW_BREW);
            TC_LOG_DEBUG("playerbot", "Brewmaster: Fortifying Brew emergency");
            return;
        }

        // Very low: Celestial Brew (absorb shield)
        if (healthPct < 40.0f && this->CanCastSpell(CELESTIAL_BREW, bot))
        {
            this->CastSpell(bot, CELESTIAL_BREW);
            TC_LOG_DEBUG("playerbot", "Brewmaster: Celestial Brew shield");
            return;
        }

        // Low: Zen Meditation (channeled DR)
        if (healthPct < 30.0f && this->CanCastSpell(ZEN_MEDITATION, bot))
        {
            this->CastSpell(bot, ZEN_MEDITATION);
            TC_LOG_DEBUG("playerbot", "Brewmaster: Zen Meditation");
            return;
        }

        // Moderate: Dampen Harm
        if (healthPct < 50.0f && this->CanCastSpell(DAMPEN_HARM, bot))
        {
            this->CastSpell(bot, DAMPEN_HARM);
            TC_LOG_DEBUG("playerbot", "Brewmaster: Dampen Harm");
            return;
        }
    }

private:
    void UpdateBrewmasterState()
    {
        // Update Stagger tracker
        _staggerTracker.UpdateStagger(this->GetBot());

        // Update Shuffle tracker
        _shuffleTracker.Update();

        // Update Ironskin Brew
        if (_ironskinBrewActive && getMSTime() >= _ironskinEndTime)
        {
            _ironskinBrewActive = false;
            _ironskinEndTime = 0;
        }

        // Update Chi from bot
        if (this->GetBot())
        {
            this->_resource.chi = this->GetBot()->GetPower(POWER_CHI);
            this->_resource.energy = this->GetBot()->GetPower(POWER_ENERGY);
        }
    }

    void UpdateIronskinBrew()
    {
        if (this->GetBot() && this->GetBot()->HasAura(IRONSKIN_BREW))
        {
            _ironskinBrewActive = true;
        }
        else
        {
            _ironskinBrewActive = false;
            _ironskinEndTime = 0;
        }
    }

    uint32 GetIronskinTimeRemaining() const
    {
        if (!_ironskinBrewActive)
            return 0;

        uint32 now = getMSTime();
        return _ironskinEndTime > now ? _ironskinEndTime - now : 0;
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
        RegisterCooldown(KEG_SMASH, 8000);              // 8 sec CD
        RegisterCooldown(PROVOKE, 8000);                // 8 sec CD (taunt)
        RegisterCooldown(PURIFYING_BREW, 20000);        // 20 sec CD (2 charges)
        RegisterCooldown(CELESTIAL_BREW, 60000);        // 1 min CD
        RegisterCooldown(FORTIFYING_BREW_BREW, 360000); // 6 min CD
        RegisterCooldown(DAMPEN_HARM, 120000);          // 2 min CD
        RegisterCooldown(ZEN_MEDITATION, 300000);       // 5 min CD
        RegisterCooldown(INVOKE_NIUZAO, 180000);        // 3 min CD
        RegisterCooldown(WEAPONS_OF_ORDER, 120000);     // 2 min CD
        RegisterCooldown(BONEDUST_BREW, 60000);         // 1 min CD
    }

private:
    BrewmasterStaggerTracker _staggerTracker;
    BrewmasterShuffleTracker _shuffleTracker;
    bool _ironskinBrewActive;
    uint32 _ironskinEndTime;
    uint32 _lastKegSmashTime;
};

} // namespace Playerbot
