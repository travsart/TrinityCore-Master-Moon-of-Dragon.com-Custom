/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * Frost Death Knight Refactored - Template-Based Implementation
 *
 * This file provides a complete, template-based implementation of Frost Death Knight
 * using the MeleeDpsSpecialization with dual resource system (Runes + Runic Power).
 */

#pragma once

#include "../CombatSpecializationTemplates.h"
#include "../ResourceTypes.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellAuraEffects.h"
#include "Log.h"
#include "DeathKnightSpecialization.h"

namespace Playerbot
{

// ============================================================================
// FROST DEATH KNIGHT SPELL IDs (WoW 11.2 - The War Within)
// ============================================================================

enum FrostDeathKnightSpells
{
    // Rune Spenders
    OBLITERATE               = 49020,   // 2 Runes, main damage dealer
    HOWLING_BLAST            = 49184,   // 1 Rune, AoE + applies Frost Fever
    REMORSELESS_WINTER       = 196770,  // 1 Rune, 20 sec CD, AoE slow
    GLACIAL_ADVANCE          = 194913,  // 2 Runes, ranged AoE (talent)
    FROSTSCYTHE              = 207230,  // 2 Runes, AoE cleave (talent)

    // Runic Power Spenders
    FROST_STRIKE             = 49143,   // 25 RP, main RP spender
    HORN_OF_WINTER           = 57330,   // 2 Runes + 25 RP gen (talent)

    // Cooldowns
    PILLAR_OF_FROST          = 51271,   // 1 min CD, major damage buff
    EMPOWER_RUNE_WEAPON      = 47568,   // 2 min CD, rune refresh
    BREATH_OF_SINDRAGOSA     = 152279,  // 2 min CD, channel (talent)
    FROSTWYRMS_FURY          = 279302,  // 3 min CD, AoE burst (talent)

    // Utility
    DEATH_GRIP_FROST         = 49576,   // 25 sec CD, pull
    MIND_FREEZE_FROST        = 47528,   // Interrupt
    CHAINS_OF_ICE            = 45524,   // Root/slow
    DARK_COMMAND_FROST       = 56222,   // Taunt
    ANTI_MAGIC_SHELL_FROST   = 48707,   // 1 min CD, magic absorption
    ICEBOUND_FORTITUDE_FROST = 48792,   // 3 min CD, damage reduction
    DEATHS_ADVANCE_FROST     = 48265,   // 1.5 min CD, speed + mitigation

    // Procs and Buffs
    KILLING_MACHINE          = 51128,   // Proc: crit on Obliterate
    RIME                     = 59052,   // Proc: free Howling Blast
    RAZORICE                 = 50401,   // Debuff: stacking damage amp
    FROZEN_PULSE             = 194909,  // Passive AoE (talent)

    // Diseases
    FROST_FEVER_DK           = 55095,   // Disease from Howling Blast

    // Talents
    OBLITERATION             = 281238,  // Pillar of Frost extension
    BREATH_OF_SINDRAGOSA_TALENT = 152279, // Channel burst
    GATHERING_STORM          = 194912,  // Remorseless Winter buff
    ICECAP                   = 207126,  // Pillar of Frost CDR
    INEXORABLE_ASSAULT       = 253593,  // Cold Heart stacking buff
    COLD_HEART               = 281208   // Chains of Ice nuke (talent)
};

// Dual resource type for Frost Death Knight (simplified runes)
struct FrostRuneRunicPowerResource
{
    uint32 bloodRunes{0};
    uint32 frostRunes{0};
    uint32 unholyRunes{0};
    uint32 runicPower{0};
    uint32 maxRunicPower{100};
    bool available{true};

    bool Consume(uint32 runesCost) {
        uint32 totalRunes = bloodRunes + frostRunes + unholyRunes;
        if (totalRunes >= runesCost) {
            uint32 remaining = runesCost;
            if (bloodRunes > 0) {
                uint32 toConsume = std::min(bloodRunes, remaining);
                bloodRunes -= toConsume;
                remaining -= toConsume;
            }
            if (remaining > 0 && frostRunes > 0) {
                uint32 toConsume = std::min(frostRunes, remaining);
                frostRunes -= toConsume;
                remaining -= toConsume;
            }
            if (remaining > 0 && unholyRunes > 0) {
                uint32 toConsume = std::min(unholyRunes, remaining);
                unholyRunes -= toConsume;
                remaining -= toConsume;
            }
            available = (bloodRunes + frostRunes + unholyRunes) > 0;
            return true;
        }
        return false;
    }

    void Regenerate(uint32 diff) {
        // Runes regenerate over time
        static uint32 regenTimer = 0;
        regenTimer += diff;
        if (regenTimer >= 10000) { // 10 seconds per rune
            uint32 totalRunes = bloodRunes + frostRunes + unholyRunes;
            if (totalRunes < 6) {
                if (bloodRunes < 2) bloodRunes++;
                else if (frostRunes < 2) frostRunes++;
                else if (unholyRunes < 2) unholyRunes++;
            }
            regenTimer = 0;
        }
        available = (bloodRunes + frostRunes + unholyRunes) > 0;
    }

    [[nodiscard]] uint32 GetAvailable() const {
        return bloodRunes + frostRunes + unholyRunes;
    }

    [[nodiscard]] uint32 GetMax() const {
        return 6; // Max 6 runes
    }

    void Initialize(Player* bot) {
        bloodRunes = 2;
        frostRunes = 2;
        unholyRunes = 2;
        runicPower = 0;
        available = true;
    }
};

// ============================================================================
// FROST KILLING MACHINE TRACKER
// ============================================================================

class FrostKillingMachineTracker
{
public:
    FrostKillingMachineTracker()
        : _kmActive(false)
        , _kmStacks(0)
    {
    }

    void ActivateProc(uint32 stacks = 1)
    {
        _kmActive = true;
        _kmStacks = stacks;
    }

    void ConsumeProc()
    {
        if (_kmStacks > 0)
            _kmStacks--;

        if (_kmStacks == 0)
            _kmActive = false;
    }

    bool IsActive() const { return _kmActive; }
    uint32 GetStacks() const { return _kmStacks; }

    void Update(Player* bot)
    {
        if (!bot)
            return;

        if (Aura* aura = bot->GetAura(KILLING_MACHINE))
        {
            _kmActive = true;
            _kmStacks = aura->GetStackAmount();
        }
        else
        {
            _kmActive = false;
            _kmStacks = 0;
        }
    }

private:
    bool _kmActive;
    uint32 _kmStacks;
};

// ============================================================================
// FROST RIME TRACKER
// ============================================================================

class FrostRimeTracker
{
public:
    FrostRimeTracker()
        : _rimeActive(false)
    {
    }

    void ActivateProc()
    {
        _rimeActive = true;
    }

    void ConsumeProc()
    {
        _rimeActive = false;
    }

    bool IsActive() const { return _rimeActive; }

    void Update(Player* bot)
    {
        if (!bot)
            return;

        _rimeActive = bot->HasAura(RIME);
    }

private:
    bool _rimeActive;
};

// ============================================================================
// FROST DEATH KNIGHT REFACTORED
// ============================================================================

class FrostDeathKnightRefactored : public MeleeDpsSpecialization<FrostRuneRunicPowerResource>, public DeathKnightSpecialization
{
public:
    using Base = MeleeDpsSpecialization<FrostRuneRunicPowerResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::_resource;
    explicit FrostDeathKnightRefactored(Player* bot)
        : MeleeDpsSpecialization<FrostRuneRunicPowerResource>(bot)
        , DeathKnightSpecialization(bot)
        , _kmTracker()
        , _rimeTracker()
        , _pillarOfFrostActive(false)
        , _pillarEndTime(0)
        , _breathOfSindragosaActive(false)
        , _lastRemorselessWinterTime(0)
    {
        // Initialize runes/runic power resources
        this->_resource.Initialize(bot);

        InitializeCooldowns();

        TC_LOG_DEBUG("playerbot", "FrostDeathKnightRefactored initialized for {}", bot->GetName());
    }

    void UpdateRotation(::Unit* target) override
    {
        if (!target || !target->IsAlive() || !target->IsHostileTo(this->GetBot()))
            return;

        // Update Frost state
        UpdateFrostState();

        // Use major cooldowns
        HandleCooldowns();

        // Determine if AoE or single target
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
        if (!bot)
            return;

        // Defensive cooldowns
        HandleDefensiveCooldowns();
    }


protected:
    void ExecuteSingleTargetRotation(::Unit* target)
    {
        uint32 rp = this->_resource.runicPower;
        uint32 totalRunes = this->_resource.bloodRunes + this->_resource.frostRunes + this->_resource.unholyRunes;

        // Priority 1: Breath of Sindragosa (if talented and high RP)
        if (_breathOfSindragosaActive)
        {
            // During Breath, spam Obliterate and Frost Strike to maintain
            if (rp < 20 && totalRunes >= 2 && this->CanCastSpell(OBLITERATE, target))
            {
                this->CastSpell(target, OBLITERATE);
                ConsumeRunes(RuneType::FROST, 2);
                GenerateRunicPower(15);
                return;
            }

            if (rp >= 25 && this->CanCastSpell(FROST_STRIKE, target))
            {
                this->CastSpell(target, FROST_STRIKE);
                ConsumeRunicPower(25);
                return;
            }
        }

        // Priority 2: Use Rime proc (free Howling Blast)
        if (_rimeTracker.IsActive() && this->CanCastSpell(HOWLING_BLAST, target))
        {
            this->CastSpell(target, HOWLING_BLAST);
            _rimeTracker.ConsumeProc();
            return;
        }

        // Priority 3: Obliterate with Killing Machine proc
        if (_kmTracker.IsActive() && totalRunes >= 2 && this->CanCastSpell(OBLITERATE, target))
        {
            this->CastSpell(target, OBLITERATE);
            _kmTracker.ConsumeProc();
            ConsumeRunes(RuneType::FROST, 2);
            GenerateRunicPower(15);
            return;
        }

        // Priority 4: Remorseless Winter (AoE slow)
        if (totalRunes >= 1 && this->CanCastSpell(REMORSELESS_WINTER, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), REMORSELESS_WINTER);
            _lastRemorselessWinterTime = getMSTime();
            ConsumeRunes(RuneType::FROST, 1);
            return;
        }

        // Priority 5: Frost Strike (dump RP)
        if (rp >= 50 && this->CanCastSpell(FROST_STRIKE, target))
        {
            this->CastSpell(target, FROST_STRIKE);
            ConsumeRunicPower(25);
            return;
        }

        // Priority 6: Obliterate (main rune spender)
        if (totalRunes >= 2 && this->CanCastSpell(OBLITERATE, target))
        {
            this->CastSpell(target, OBLITERATE);
            ConsumeRunes(RuneType::FROST, 2);
            GenerateRunicPower(15);
            return;
        }

        // Priority 7: Frost Strike (prevent RP capping)
        if (rp >= 25 && this->CanCastSpell(FROST_STRIKE, target))
        {
            this->CastSpell(target, FROST_STRIKE);
            ConsumeRunicPower(25);
            return;
        }

        // Priority 8: Horn of Winter (talent, generate resources)
        if (totalRunes < 3 && rp < 70 && this->CanCastSpell(HORN_OF_WINTER, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), HORN_OF_WINTER);
            GenerateRunicPower(25);
            return;
        }
    }

    void ExecuteAoERotation(::Unit* target, uint32 enemyCount)
    {
        uint32 rp = this->_resource.runicPower;
        uint32 totalRunes = this->_resource.bloodRunes + this->_resource.frostRunes + this->_resource.unholyRunes;

        // Priority 1: Remorseless Winter
        if (totalRunes >= 1 && this->CanCastSpell(REMORSELESS_WINTER, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), REMORSELESS_WINTER);
            _lastRemorselessWinterTime = getMSTime();
            ConsumeRunes(RuneType::FROST, 1);
            return;
        }

        // Priority 2: Howling Blast (AoE)
        if (totalRunes >= 1 && this->CanCastSpell(HOWLING_BLAST, target))
        {
            this->CastSpell(target, HOWLING_BLAST);
            ConsumeRunes(RuneType::FROST, 1);
            GenerateRunicPower(10);
            return;
        }

        // Priority 3: Frostscythe (talent, AoE cleave)
        if (totalRunes >= 2 && this->CanCastSpell(FROSTSCYTHE, target))
        {
            this->CastSpell(target, FROSTSCYTHE);
            ConsumeRunes(RuneType::FROST, 2);
            GenerateRunicPower(15);
            return;
        }

        // Priority 4: Glacial Advance (talent, ranged AoE)
        if (totalRunes >= 2 && this->CanCastSpell(GLACIAL_ADVANCE, target))
        {
            this->CastSpell(target, GLACIAL_ADVANCE);
            ConsumeRunes(RuneType::FROST, 2);
            return;
        }

        // Priority 5: Frost Strike (dump RP)
        if (rp >= 25 && this->CanCastSpell(FROST_STRIKE, target))
        {
            this->CastSpell(target, FROST_STRIKE);
            ConsumeRunicPower(25);
            return;
        }

        // Priority 6: Obliterate (if no AoE available)
        if (totalRunes >= 2 && this->CanCastSpell(OBLITERATE, target))
        {
            this->CastSpell(target, OBLITERATE);
            ConsumeRunes(RuneType::FROST, 2);
            GenerateRunicPower(15);
            return;
        }
    }

    void HandleCooldowns()
    {
        Player* bot = this->GetBot();
        uint32 rp = this->_resource.runicPower;
        uint32 totalRunes = this->_resource.bloodRunes + this->_resource.frostRunes + this->_resource.unholyRunes;

        // Pillar of Frost (major damage CD)
        if (totalRunes >= 3 && this->CanCastSpell(PILLAR_OF_FROST, bot))
        {
            this->CastSpell(bot, PILLAR_OF_FROST);
            _pillarOfFrostActive = true;
            _pillarEndTime = getMSTime() + 12000; // 12 sec duration
            TC_LOG_DEBUG("playerbot", "Frost: Pillar of Frost activated");
        }

        // Empower Rune Weapon (rune refresh)
        if (totalRunes == 0 && this->CanCastSpell(EMPOWER_RUNE_WEAPON, bot))
        {
            this->CastSpell(bot, EMPOWER_RUNE_WEAPON);
            this->_resource.bloodRunes = 2;
            this->_resource.frostRunes = 2;
            this->_resource.unholyRunes = 2;
            GenerateRunicPower(25);
            TC_LOG_DEBUG("playerbot", "Frost: Empower Rune Weapon");
        }

        // Breath of Sindragosa (talent, channel burst)
        if (rp >= 60 && this->CanCastSpell(BREATH_OF_SINDRAGOSA, bot))
        {
            this->CastSpell(bot, BREATH_OF_SINDRAGOSA);
            _breathOfSindragosaActive = true;
            TC_LOG_DEBUG("playerbot", "Frost: Breath of Sindragosa");
        }

        // Frostwyrm's Fury (AoE burst)
        if (this->CanCastSpell(FROSTWYRMS_FURY, bot))
        {
            this->CastSpell(bot, FROSTWYRMS_FURY);
            TC_LOG_DEBUG("playerbot", "Frost: Frostwyrm's Fury");
        }
    }

    void HandleDefensiveCooldowns()
    {
        Player* bot = this->GetBot();
        float healthPct = bot->GetHealthPct();

        // Icebound Fortitude
        if (healthPct < 40.0f && this->CanCastSpell(ICEBOUND_FORTITUDE_FROST, bot))
        {
            this->CastSpell(bot, ICEBOUND_FORTITUDE_FROST);
            TC_LOG_DEBUG("playerbot", "Frost: Icebound Fortitude");
            return;
        }

        // Anti-Magic Shell
        if (healthPct < 60.0f && this->CanCastSpell(ANTI_MAGIC_SHELL_FROST, bot))
        {
            this->CastSpell(bot, ANTI_MAGIC_SHELL_FROST);
            TC_LOG_DEBUG("playerbot", "Frost: Anti-Magic Shell");
            return;
        }

        // Death's Advance
        if (healthPct < 70.0f && this->CanCastSpell(DEATHS_ADVANCE_FROST, bot))
        {
            this->CastSpell(bot, DEATHS_ADVANCE_FROST);
            TC_LOG_DEBUG("playerbot", "Frost: Death's Advance");
            return;
        }
    }

private:
    void UpdateFrostState()
    {
        // Update Killing Machine tracker
        _kmTracker.Update(this->GetBot());

        // Update Rime tracker
        _rimeTracker.Update(this->GetBot());

        // Update Pillar of Frost
        if (_pillarOfFrostActive && getMSTime() >= _pillarEndTime)
        {
            _pillarOfFrostActive = false;
            _pillarEndTime = 0;
        }

        // Update Breath of Sindragosa
        if (_breathOfSindragosaActive)
        {
            if (!this->GetBot() || !this->GetBot()->HasAura(BREATH_OF_SINDRAGOSA))
                _breathOfSindragosaActive = false;
        }

        // Update Runic Power from bot
        if (this->GetBot())
            this->_resource.runicPower = this->GetBot()->GetPower(POWER_RUNIC_POWER);

        // Update runes (simplified)
        uint32 now = getMSTime();
        static uint32 lastRuneUpdate = 0;
        if (now - lastRuneUpdate > 10000) // Every 10 seconds
        {
            this->_resource.bloodRunes = 2;
            this->_resource.frostRunes = 2;
            this->_resource.unholyRunes = 2;
            lastRuneUpdate = now;
        }
    }

    void GenerateRunicPower(uint32 amount)
    {
        this->_resource.runicPower = std::min(this->_resource.runicPower + amount, this->_resource.maxRunicPower);
    }

    void ConsumeRunicPower(uint32 amount)
    {
        this->_resource.runicPower = (this->_resource.runicPower > amount) ? this->_resource.runicPower - amount : 0;
    }

    void ConsumeRunes(RuneType type, uint32 count = 1) override
    {
        this->_resource.Consume(count);
    }

    void InitializeCooldowns()
    {
        RegisterCooldown(OBLITERATE, 0);                // No CD, rune-gated
        RegisterCooldown(FROST_STRIKE, 0);              // No CD, RP-gated
        RegisterCooldown(HOWLING_BLAST, 0);             // No CD, rune-gated
        RegisterCooldown(REMORSELESS_WINTER, 20000);    // 20 sec CD
        RegisterCooldown(PILLAR_OF_FROST, 60000);       // 1 min CD
        RegisterCooldown(EMPOWER_RUNE_WEAPON, 120000);  // 2 min CD
        RegisterCooldown(BREATH_OF_SINDRAGOSA, 120000); // 2 min CD
        RegisterCooldown(FROSTWYRMS_FURY, 180000);      // 3 min CD
        RegisterCooldown(DEATH_GRIP_FROST, 25000);      // 25 sec CD
        RegisterCooldown(ANTI_MAGIC_SHELL_FROST, 60000);// 1 min CD
        RegisterCooldown(ICEBOUND_FORTITUDE_FROST, 180000); // 3 min CD
        RegisterCooldown(DEATHS_ADVANCE_FROST, 90000);  // 1.5 min CD
    }

private:
    FrostKillingMachineTracker _kmTracker;
    FrostRimeTracker _rimeTracker;
    bool _pillarOfFrostActive;
    uint32 _pillarEndTime;
    bool _breathOfSindragosaActive;
    uint32 _lastRemorselessWinterTime;
};

} // namespace Playerbot
