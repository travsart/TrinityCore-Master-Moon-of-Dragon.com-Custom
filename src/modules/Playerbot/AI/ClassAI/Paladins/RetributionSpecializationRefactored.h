/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * Retribution Paladin Specialization - REFACTORED
 *
 * This demonstrates how to migrate existing specializations to the new
 * template-based architecture, eliminating code duplication.
 *
 * BEFORE: 433 lines of code with duplicate UpdateCooldowns, CanUseAbility, etc.
 * AFTER: ~150 lines focusing ONLY on Retribution-specific logic
 */

#pragma once

#include "../CombatSpecializationTemplates.h"
#include "../ResourceTypes.h"
#include "../CombatSpecializationTemplates.h"
#include "PaladinSpecialization.h"

namespace Playerbot
{

/**
 * Refactored Retribution Paladin using template architecture
 *
 * Key improvements:
 * - Inherits from MeleeDpsSpecialization<ManaResource> for role defaults
 * - Automatically gets UpdateCooldowns, CanUseAbility, OnCombatStart/End
 * - Uses HolyPowerSystem as secondary resource
 * - Eliminates ~280 lines of duplicate code
 */
class RetributionPaladinRefactored : public MeleeDpsSpecialization<ManaResource>, public PaladinSpecialization
{
public:
    using Base = MeleeDpsSpecialization<ManaResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::CanUseAbility;
    using Base::IsInMeleeRange;
    using Base::_resource;
    explicit RetributionPaladinRefactored(Player* bot)
        : MeleeDpsSpecialization<ManaResource>(bot)
        , PaladinSpecialization(bot)
        , _holyPower()
        , _hasArtOfWar(false)
        , _hasDivinePurpose(false)
        , _sealTwistWindow(0)
    {
        // Initialize Holy Power system
        _holyPower.Initialize(bot);
    }

    // ========================================================================
    // CORE ROTATION - Only Retribution-specific logic
    // ========================================================================

    void UpdateRotation(::Unit* target) override
    {
        if (!target || !target->IsHostileTo(this->GetBot()))
            return;

        // Update procs and buffs
        CheckForProcs();

        // Execute priority rotation
        ExecutePriorityRotation(target);
    }

    void UpdateBuffs() override
    {
        Player* bot = this->GetBot();

        // Maintain Retribution Aura
        if (!bot->HasAura(SPELL_RETRIBUTION_AURA))
        {
            this->CastSpell(bot, SPELL_RETRIBUTION_AURA);
        }

        // Maintain Seal of Truth for single target
        if (!bot->HasAura(SPELL_SEAL_OF_TRUTH))
        {
            this->CastSpell(bot, SPELL_SEAL_OF_TRUTH);
        }

        // Blessings handled by group coordination
    }

protected:
    // ========================================================================
    // RETRIBUTION-SPECIFIC MECHANICS
    // ========================================================================

    /**
     * Execute abilities based on priority system
     */
    void ExecutePriorityRotation(::Unit* target)
    {
        // Hammer of Wrath (Execute phase)
        if (target->GetHealthPct() < 20.0f && this->CanUseAbility(SPELL_HAMMER_OF_WRATH))
        {
            this->CastSpell(target, SPELL_HAMMER_OF_WRATH);
            return;
        }

        // Templar's Verdict at 3+ Holy Power
        if (_holyPower.GetAvailable() >= 3 && this->CanUseAbility(SPELL_TEMPLARS_VERDICT))
        {
            this->CastSpell(target, SPELL_TEMPLARS_VERDICT);
            _holyPower.Consume(3);
            return;
        }

        // Divine Storm for AoE (3+ enemies)
        if (_holyPower.GetAvailable() >= 3 && this->GetEnemiesInRange(8.0f) >= 3)
        {
            if (this->CanUseAbility(SPELL_DIVINE_STORM))
            {
                this->CastSpell(this->GetBot(), SPELL_DIVINE_STORM);
                _holyPower.Consume(3);
                return;
            }
        }

        // Crusader Strike - Primary Holy Power generator
        if (this->CanUseAbility(SPELL_CRUSADER_STRIKE))
        {
            this->CastSpell(target, SPELL_CRUSADER_STRIKE);
            _holyPower.Generate(1);
            return;
        }

        // Exorcism with Art of War proc
        if (_hasArtOfWar && this->CanUseAbility(SPELL_EXORCISM))
        {
            this->CastSpell(target, SPELL_EXORCISM);
            _hasArtOfWar = false;
            return;
        }

        // Judgment
        if (this->CanUseAbility(SPELL_JUDGMENT))
        {
            this->CastSpell(target, SPELL_JUDGMENT);
            return;
        }

        // Consecration if in melee range
        if (this->IsInMeleeRange(target) && this->CanUseAbility(SPELL_CONSECRATION))
        {
            this->CastSpell(this->GetBot(), SPELL_CONSECRATION);
            return;
        }

        // Holy Wrath for burst
        if (ShouldUseCooldowns(target) && this->CanUseAbility(SPELL_HOLY_WRATH))
        {
            this->CastSpell(target, SPELL_HOLY_WRATH);
            return;
        }
    }

    /**
     * Check for Retribution-specific procs
     */
    void CheckForProcs()
    {
        Player* bot = this->GetBot();

        // Art of War proc (instant Exorcism)
        _hasArtOfWar = bot->HasAura(SPELL_ART_OF_WAR_PROC);

        // Divine Purpose proc (free 3 Holy Power ability)
        _hasDivinePurpose = bot->HasAura(SPELL_DIVINE_PURPOSE_PROC);
    }

    /**
     * Advanced seal twisting for extra DPS
     */
    void UpdateSealTwisting()
    {
        uint32 currentTime = getMSTime();

        // Twist between Seal of Truth and Seal of Righteousness
        if (currentTime > _sealTwistWindow)
        {
            Player* bot = this->GetBot();

            if (bot->HasAura(SPELL_SEAL_OF_TRUTH))
            {
                // Quick swap to Righteousness for instant damage
                this->CastSpell(bot, SPELL_SEAL_OF_RIGHTEOUSNESS);
                _sealTwistWindow = currentTime + 100; // Very brief window
            }
            else
            {
                // Back to Truth for DoT
                this->CastSpell(bot, SPELL_SEAL_OF_TRUTH);
                _sealTwistWindow = currentTime + 10000; // 10 seconds
            }
        }
    }

    /**
     * Determine if we should use offensive cooldowns
     */
    bool ShouldUseCooldowns(::Unit* target) const
    {
        // Use on bosses or when multiple enemies
        return (target->GetMaxHealth() > this->GetBot()->GetMaxHealth() * 10) ||
               this->GetEnemiesInRange(10.0f) >= 3;
    }

    // ========================================================================
    // COMBAT LIFECYCLE HOOKS
    // ========================================================================

    void OnCombatStartSpecific(::Unit* target) override
    {
        // Pop offensive cooldowns at start for burst
        if (ShouldUseCooldowns(target))
        {
            Player* bot = this->GetBot();
            if (this->CanUseAbility(SPELL_AVENGING_WRATH))
            {
                this->CastSpell(bot, SPELL_AVENGING_WRATH);
            }

            if (this->CanUseAbility(SPELL_GUARDIAN_OF_ANCIENT_KINGS))
            {
                this->CastSpell(bot, SPELL_GUARDIAN_OF_ANCIENT_KINGS);
            }
        }

        // Reset Holy Power tracking
        _holyPower.Initialize(this->GetBot());
    }

    void OnCombatEndSpecific() override
    {
        // Reset proc tracking
        _hasArtOfWar = false;
        _hasDivinePurpose = false;
    }

private:
    // ========================================================================
    // SPELL IDS
    // ========================================================================

    enum RetributionSpells
    {
        // Seals
        SPELL_SEAL_OF_TRUTH         = 31801,
        SPELL_SEAL_OF_RIGHTEOUSNESS = 21084,

        // Auras
        SPELL_RETRIBUTION_AURA      = 7294,

        // Core Abilities
        SPELL_CRUSADER_STRIKE       = 35395,
        SPELL_TEMPLARS_VERDICT      = 85256,
        SPELL_DIVINE_STORM          = 53385,
        SPELL_HAMMER_OF_WRATH       = 24275,
        SPELL_EXORCISM              = 879,
        SPELL_JUDGMENT              = 20271,
        SPELL_CONSECRATION          = 26573,
        SPELL_HOLY_WRATH            = 2812,

        // Cooldowns
        SPELL_AVENGING_WRATH        = 31884,
        SPELL_GUARDIAN_OF_ANCIENT_KINGS = 86150,

        // Procs
        SPELL_ART_OF_WAR_PROC       = 59578,
        SPELL_DIVINE_PURPOSE_PROC   = 90174,
    };

    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================

    // Secondary resource system
    HolyPowerSystem _holyPower;

    // Proc tracking
    bool _hasArtOfWar;
    bool _hasDivinePurpose;

    // Seal twisting
    uint32 _sealTwistWindow;
};

} // namespace Playerbot