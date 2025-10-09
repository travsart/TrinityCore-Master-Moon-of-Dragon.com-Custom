/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * Assassination Rogue Refactored - Template-Based Implementation
 *
 * This file provides a complete, template-based implementation of Assassination Rogue
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

// NOTE: Spell IDs are defined in RogueSpecialization.h (included above)
// NOTE: ComboPointsAssassination is defined in RogueResourceTypes.h (spec-specific resource type)

// ============================================================================
// ASSASSINATION DOT TRACKER
// ============================================================================

class AssassinationDotTracker
{
public:
    struct DotInfo
    {
        uint32 spellId;
        uint32 endTime;
        uint32 duration;
        bool active;

        DotInfo() : spellId(0), endTime(0), duration(0), active(false) {}
        DotInfo(uint32 id, uint32 dur) : spellId(id), endTime(0), duration(dur), active(false) {}

        uint32 GetTimeRemaining() const {
            if (!active) return 0;
            uint32 now = getMSTime();
            return endTime > now ? endTime - now : 0;
        }

        bool NeedsRefresh() const {
            return !active || GetTimeRemaining() < 5400; // Pandemic window (30% of duration)
        }
    };

    AssassinationDotTracker()
    {
        _dots[GARROTE] = DotInfo(GARROTE, 18000);
        _dots[RUPTURE] = DotInfo(RUPTURE, 24000); // 4s per CP
        _dots[CRIMSON_TEMPEST] = DotInfo(CRIMSON_TEMPEST, 14000);
    }

    void ApplyDot(uint32 spellId, uint32 comboPoints = 0)
    {
        if (_dots.find(spellId) != _dots.end())
        {
            uint32 duration = _dots[spellId].duration;
            if (spellId == RUPTURE)
                duration = 4000 * std::max(1u, comboPoints); // 4s per CP

            _dots[spellId].active = true;
            _dots[spellId].endTime = getMSTime() + duration;
        }
    }

    bool IsActive(uint32 spellId) const
    {
        auto it = _dots.find(spellId);
        if (it != _dots.end())
            return it->second.active && it->second.GetTimeRemaining() > 0;
        return false;
    }

    bool NeedsRefresh(uint32 spellId) const
    {
        auto it = _dots.find(spellId);
        return it != _dots.end() && it->second.NeedsRefresh();
    }

    uint32 GetTimeRemaining(uint32 spellId) const
    {
        auto it = _dots.find(spellId);
        return it != _dots.end() ? it->second.GetTimeRemaining() : 0;
    }

    void Update()
    {
        uint32 now = getMSTime();
        for (auto& [spellId, dot] : _dots)
        {
            if (dot.active && now >= dot.endTime)
            {
                dot.active = false;
                dot.endTime = 0;
            }
        }
    }

private:
    std::unordered_map<uint32, DotInfo> _dots;
};

// ============================================================================
// ASSASSINATION ROGUE REFACTORED
// ============================================================================

class AssassinationRogueRefactored : public MeleeDpsSpecialization<ComboPointsAssassination>
{
public:
    // Use base class members with type alias for cleaner syntax
    using Base = MeleeDpsSpecialization<ComboPointsAssassination>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::GetEnemiesInRange;
    using Base::_resource;

    explicit AssassinationRogueRefactored(Player* bot)
        : MeleeDpsSpecialization<ComboPointsAssassination>(bot)
        , _dotTracker()
        , _inStealth(false)
        , _lastMutilateTime(0)
        , _lastEnvenomTime(0)
        , _vendettaActive(false)
        , _vendettaEndTime(0)
    {
        // Initialize energy/combo resources
        this->_resource.maxEnergy = bot->HasSpell(VIGOR) ? 120 : 100;
        this->_resource.maxComboPoints = bot->HasSpell(DEEPER_STRATAGEM) ? 6 : 5;
        this->_resource.energy = this->_resource.maxEnergy;
        this->_resource.comboPoints = 0;

        InitializeCooldowns();

        TC_LOG_DEBUG("playerbot", "AssassinationRogueRefactored initialized for {}", bot->GetName());
    }

    void UpdateRotation(::Unit* target) override
    {
        if (!target || !target->IsAlive() || !target->IsHostileTo(this->GetBot()))
            return;

        // Update tracking systems
        UpdateAssassinationState();

        // Check stealth status
        _inStealth = this->GetBot()->HasAuraType(SPELL_AURA_MOD_STEALTH);

        // Stealth opener
        if (_inStealth)
        {
            ExecuteStealthOpener(target);
            return;
        }

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

        // Maintain poisons
        if (!bot->HasAura(DEADLY_POISON) && this->CanCastSpell(DEADLY_POISON, bot))
        {
            this->CastSpell(bot, DEADLY_POISON);
        }

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
    }

    // GetOptimalRange is final in MeleeDpsSpecialization, cannot override
    // Returns 5.0f melee range by default

protected:
    void ExecuteSingleTargetRotation(::Unit* target)
    {
        uint32 energy = this->_resource.energy;
        uint32 cp = this->_resource.comboPoints;
        uint32 maxCp = this->_resource.maxComboPoints;

        // Priority 1: Vendetta on cooldown
        if (this->CanCastSpell(VENDETTA, target))
        {
            this->CastSpell(target, VENDETTA);
            _vendettaActive = true;
            _vendettaEndTime = getMSTime() + 20000;
            return;
        }

        // Priority 2: Deathmark on cooldown
        if (this->CanCastSpell(DEATHMARK, target))
        {
            this->CastSpell(target, DEATHMARK);
            return;
        }

        // Priority 3: Refresh Garrote
        if (_dotTracker.NeedsRefresh(GARROTE) && energy >= 45)
        {
            if (this->CanCastSpell(GARROTE, target))
            {
                this->CastSpell(target, GARROTE);
                _dotTracker.ApplyDot(GARROTE);
                ConsumeEnergy(45);
                return;
            }
        }

        // Priority 4: Finishers at 4-5+ CP
        if (cp >= (maxCp - 1))
        {
            // Refresh Rupture if needed
            if (_dotTracker.NeedsRefresh(RUPTURE) && energy >= 25)
            {
                if (this->CanCastSpell(RUPTURE, target))
                {
                    this->CastSpell(target, RUPTURE);
                    _dotTracker.ApplyDot(RUPTURE, cp);
                    ConsumeEnergy(25);
                    this->_resource.comboPoints = 0;
                    return;
                }
            }

            // Envenom for damage
            if (energy >= 35 && this->CanCastSpell(ENVENOM, target))
            {
                this->CastSpell(target, ENVENOM);
                _lastEnvenomTime = getMSTime();
                ConsumeEnergy(35);
                this->_resource.comboPoints = 0;
                return;
            }
        }

        // Priority 5: Kingsbane (talent)
        if (energy >= 35 && this->CanCastSpell(KINGSBANE, target))
        {
            this->CastSpell(target, KINGSBANE);
            ConsumeEnergy(35);
            return;
        }

        // Priority 6: Mutilate for combo points
        if (energy >= 50 && cp < maxCp)
        {
            if (this->CanCastSpell(MUTILATE, target))
            {
                this->CastSpell(target, MUTILATE);
                _lastMutilateTime = getMSTime();
                ConsumeEnergy(50);
                GenerateComboPoints(2);
                return;
            }
        }

        // Priority 7: Poisoned Knife if can't melee
        if (GetDistanceToTarget(target) > 10.0f && energy >= 40)
        {
            if (this->CanCastSpell(POISONED_KNIFE, target))
            {
                this->CastSpell(target, POISONED_KNIFE);
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

        // Priority 1: Crimson Tempest finisher
        if (cp >= 4 && energy >= 35)
        {
            if (this->GetBot()->HasSpell(CRIMSON_TEMPEST) && this->CanCastSpell(CRIMSON_TEMPEST, this->GetBot()))
            {
                this->CastSpell(this->GetBot(), CRIMSON_TEMPEST);
                _dotTracker.ApplyDot(CRIMSON_TEMPEST);
                ConsumeEnergy(35);
                this->_resource.comboPoints = 0;
                return;
            }
        }

        // Priority 2: Fan of Knives for AoE combo building
        if (energy >= 35 && cp < maxCp)
        {
            if (this->CanCastSpell(FAN_OF_KNIVES, this->GetBot()))
            {
                this->CastSpell(this->GetBot(), FAN_OF_KNIVES);
                ConsumeEnergy(35);
                GenerateComboPoints(std::min(enemyCount, 5u)); // 1 CP per target hit
                return;
            }
        }

        // Fallback to single target
        ExecuteSingleTargetRotation(target);
    }

    void ExecuteStealthOpener(::Unit* target)
    {
        // Priority 1: Garrote from stealth (silence)
        if (this->CanCastSpell(GARROTE, target))
        {
            this->CastSpell(target, GARROTE);
            _dotTracker.ApplyDot(GARROTE);
            _inStealth = false;
            return;
        }

        // Priority 2: Cheap Shot for stun
        if (this->CanCastSpell(CHEAP_SHOT, target))
        {
            this->CastSpell(target, CHEAP_SHOT);
            GenerateComboPoints(2);
            _inStealth = false;
            return;
        }

        // Priority 3: Ambush for damage
        if (this->CanCastSpell(AMBUSH, target))
        {
            this->CastSpell(target, AMBUSH);
            GenerateComboPoints(2);
            _inStealth = false;
            return;
        }
    }

private:
    void UpdateAssassinationState()
    {
        // Update DoT tracker
        _dotTracker.Update();

        // Check Vendetta expiry
        if (_vendettaActive && getMSTime() >= _vendettaEndTime)
        {
            _vendettaActive = false;
            _vendettaEndTime = 0;
        }

        // Regenerate energy (10 per second)
        uint32 now = getMSTime();
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

    float GetDistanceToTarget(::Unit* target) const
    {
        if (!target || !this->GetBot())
            return 1000.0f;
        return this->GetBot()->GetDistance(target);
    }

    void InitializeCooldowns()
    {
        this->RegisterCooldown(VENDETTA, 120000);         // 2 min CD
        this->RegisterCooldown(DEATHMARK, 120000);        // 2 min CD
        this->RegisterCooldown(KINGSBANE, 60000);         // 1 min CD
        this->RegisterCooldown(EXSANGUINATE, 45000);      // 45 sec CD
        this->RegisterCooldown(VANISH, 120000);           // 2 min CD
        this->RegisterCooldown(CLOAK_OF_SHADOWS, 120000); // 2 min CD
        this->RegisterCooldown(KICK, 15000);              // 15 sec CD
        this->RegisterCooldown(BLIND, 120000);            // 2 min CD
    }

private:
    AssassinationDotTracker _dotTracker;
    bool _inStealth;
    uint32 _lastMutilateTime;
    uint32 _lastEnvenomTime;
    bool _vendettaActive;
    uint32 _vendettaEndTime;
};

} // namespace Playerbot
