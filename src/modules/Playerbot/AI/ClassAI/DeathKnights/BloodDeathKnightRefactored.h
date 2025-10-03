/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * Blood Death Knight Refactored - Template-Based Implementation
 *
 * This file provides a complete, template-based implementation of Blood Death Knight
 * using the TankSpecialization with dual resource system (Runes + Runic Power).
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
// BLOOD DEATH KNIGHT SPELL IDs (WoW 11.2 - The War Within)
// ============================================================================

enum BloodDeathKnightSpells
{
    // Rune Spenders
    HEART_STRIKE             = 206930,  // 1 Rune, main threat generator
    BLOOD_BOIL               = 50842,   // 2 Runes, AoE threat
    DEATHS_CARESS            = 195292,  // 1 Rune, ranged pull
    MARROWREND               = 195182,  // 2 Runes, generates Bone Shield stacks
    CONSUMPTION              = 274156,  // 3-5 Runes, AoE leech (talent)

    // Runic Power Spenders
    DEATH_STRIKE             = 49998,   // 35-45 RP, self-heal + shield
    DEATHS_AND_DECAY_BLOOD   = 43265,   // 30 RP, ground AoE
    BONESTORM                = 194844,  // 10 RP per sec, AoE channel (talent)

    // Active Mitigation
    VAMPIRIC_BLOOD           = 55233,   // 1.5 min CD, massive self-heal + max HP
    DANCING_RUNE_WEAPON      = 49028,   // 2 min CD, threat + parry
    ICEBOUND_FORTITUDE       = 48792,   // 3 min CD, damage reduction
    ANTI_MAGIC_SHELL         = 48707,   // 1 min CD, magic absorption
    RUNE_TAP                 = 194679,  // 25 sec CD, damage reduction (talent)
    VAMPIRIC_STRIKE          = 433895,  // Empowered Death Strike (talent)

    // Threat Generation
    DARK_COMMAND             = 56222,   // Taunt
    BLOOD_PLAGUE             = 55078,   // Disease DoT
    FROST_FEVER              = 55095,   // Disease DoT (from Icy Touch)

    // Major Cooldowns
    RAISE_DEAD_BLOOD         = 46585,   // Permanent pet
    ARMY_OF_THE_DEAD         = 42650,   // 8 min CD, summon ghouls
    GOREFIENDS_GRASP         = 108199,  // 2 min CD, AoE grip (talent)
    BLOODDRINKER             = 206931,  // 30 sec CD, channel heal (talent)
    TOMBSTONE                = 219809,  // 1 min CD, consume Bone Shield for shield (talent)

    // Utility
    DEATH_GRIP               = 49576,   // 25 sec CD, pull
    DEATHS_ADVANCE           = 48265,   // 1.5 min CD, speed + damage reduction
    MIND_FREEZE              = 47528,   // Interrupt
    ASPHYXIATE               = 221562,  // 45 sec CD, stun
    CONTROL_UNDEAD           = 111673,  // Mind control undead
    RAISE_ALLY               = 61999,   // Battle res

    // Procs and Buffs
    BONE_SHIELD              = 195181,  // Passive: stacks from Marrowrend
    CRIMSON_SCOURGE          = 81136,   // Proc: free Blood Boil
    HEMOSTASIS               = 273947,  // Buff: increased Blood Boil damage (talent)
    OSSUARY                  = 219786,  // Passive: reduces Death Strike cost (talent)

    // Talents
    BLOOD_TAP                = 221699,  // Rune regen talent
    RAPID_DECOMPOSITION      = 194662,  // Disease tick speed
    HEARTBREAKER             = 221536,  // Heart Strike generates RP
    FOUL_BULWARK             = 206974,  // Armor from Bone Shield
    RELISH_IN_BLOOD          = 317610   // Extra Bone Shield stacks
};

// Dual resource type for Blood Death Knight (Runes + Runic Power)
struct RuneRunicPowerResource
{
    uint32 bloodRunes{0};
    uint32 frostRunes{0};
    uint32 unholyRunes{0};
    uint32 runicPower{0};
    uint32 maxRunicPower{125};
    bool available{true};

    bool Consume(uint32 runesCost) {
        uint32 totalRunes = bloodRunes + frostRunes + unholyRunes;
        if (totalRunes >= runesCost) {
            // Consume runes in order: Blood -> Frost -> Unholy
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
        // Runes regenerate over time (10 seconds per rune in WoW)
        // This is a simplified implementation
        static uint32 regenTimer = 0;
        regenTimer += diff;
        if (regenTimer >= 10000) { // 10 seconds
            uint32 totalRunes = bloodRunes + frostRunes + unholyRunes;
            if (totalRunes < 6) {
                // Regenerate one rune
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
        return 6; // Max 6 runes (2 blood + 2 frost + 2 unholy)
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
// BLOOD BONE SHIELD TRACKER
// ============================================================================

class BloodBoneShieldTracker
{
public:
    BloodBoneShieldTracker()
        : _boneShieldStacks(0)
        , _lastMarrowrendTime(0)
    {
    }

    void ApplyMarrowrend(uint32 stacks)
    {
        _boneShieldStacks = std::min(_boneShieldStacks + stacks, 10u);
        _lastMarrowrendTime = getMSTime();
    }

    void ConsumeStack()
    {
        if (_boneShieldStacks > 0)
            _boneShieldStacks--;
    }

    uint32 GetStacks() const { return _boneShieldStacks; }

    bool NeedsRefresh() const
    {
        // Refresh if below 5 stacks or expiring soon
        return _boneShieldStacks < 5;
    }

    void Update(Player* bot)
    {
        if (!bot)
            return;

        // Sync with actual aura
        if (Aura* aura = bot->GetAura(BONE_SHIELD))
            _boneShieldStacks = aura->GetStackAmount();
        else
            _boneShieldStacks = 0;
    }

private:
    uint32 _boneShieldStacks;
    uint32 _lastMarrowrendTime;
};

// ============================================================================
// BLOOD DEATH KNIGHT REFACTORED
// ============================================================================

class BloodDeathKnightRefactored : public TankSpecialization<RuneRunicPowerResource>, public DeathKnightSpecialization
{
public:
    using Base = TankSpecialization<RuneRunicPowerResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::_resource;
    explicit BloodDeathKnightRefactored(Player* bot)
        : TankSpecialization<RuneRunicPowerResource>(bot)
        , DeathKnightSpecialization(bot)
        , _boneShieldTracker()
        , _deathsAndDecayActive(false)
        , _deathsAndDecayEndTime(0)
        , _crimsonScourgeProc(false)
        , _lastDeathStrikeTime(0)
    {
        // Initialize runes/runic power resources
        this->_resource.Initialize(bot);

        InitializeCooldowns();

        TC_LOG_DEBUG("playerbot", "BloodDeathKnightRefactored initialized for {}", bot->GetName());
    }

    void UpdateRotation(::Unit* target) override
    {
        if (!target || !target->IsAlive() || !target->IsHostileTo(this->GetBot()))
            return;

        // Update Blood state
        UpdateBloodState();

        // Handle active mitigation
        HandleActiveMitigation();

        // Determine if AoE or single target
        uint32 enemyCount = this->GetEnemiesInRange(10.0f);
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


protected:
    void ExecuteSingleTargetThreatRotation(::Unit* target)
    {
        uint32 rp = this->_resource.runicPower;
        uint32 totalRunes = this->_resource.bloodRunes + this->_resource.frostRunes + this->_resource.unholyRunes;

        // Priority 1: Maintain Bone Shield (Marrowrend)
        if (_boneShieldTracker.NeedsRefresh() && totalRunes >= 2)
        {
            if (this->CanCastSpell(MARROWREND, target))
            {
                this->CastSpell(target, MARROWREND);
                _boneShieldTracker.ApplyMarrowrend(3);
                ConsumeRunes(RuneType::BLOOD, 2);
                GenerateRunicPower(15);
                return;
            }
        }

        // Priority 2: Death Strike for self-healing
        if (this->GetBot()->GetHealthPct() < 70.0f && rp >= 35)
        {
            if (this->CanCastSpell(DEATH_STRIKE, target))
            {
                this->CastSpell(target, DEATH_STRIKE);
                _lastDeathStrikeTime = getMSTime();
                ConsumeRunicPower(35);
                return;
            }
        }

        // Priority 3: Maintain Death's and Decay
        if (!_deathsAndDecayActive && rp >= 30)
        {
            if (this->CanCastSpell(DEATHS_AND_DECAY_BLOOD, this->GetBot()))
            {
                this->CastSpell(this->GetBot(), DEATHS_AND_DECAY_BLOOD);
                _deathsAndDecayActive = true;
                _deathsAndDecayEndTime = getMSTime() + 10000;
                ConsumeRunicPower(30);
                return;
            }
        }

        // Priority 4: Blood Boil (Crimson Scourge proc or normal)
        if (_crimsonScourgeProc || (totalRunes >= 2 && this->CanCastSpell(BLOOD_BOIL, this->GetBot())))
        {
            this->CastSpell(this->GetBot(), BLOOD_BOIL);
            if (_crimsonScourgeProc)
                _crimsonScourgeProc = false;
            else
            {
                ConsumeRunes(RuneType::BLOOD, 2);
                GenerateRunicPower(10);
            }
            return;
        }

        // Priority 5: Heart Strike (main threat generator)
        if (totalRunes >= 1 && this->CanCastSpell(HEART_STRIKE, target))
        {
            this->CastSpell(target, HEART_STRIKE);
            ConsumeRunes(RuneType::BLOOD, 1);
            GenerateRunicPower(10);
            return;
        }

        // Priority 6: Death Strike (dump RP before capping)
        if (rp >= 80 && this->CanCastSpell(DEATH_STRIKE, target))
        {
            this->CastSpell(target, DEATH_STRIKE);
            ConsumeRunicPower(35);
            return;
        }
    }

    void ExecuteAoEThreatRotation(::Unit* target, uint32 enemyCount)
    {
        uint32 rp = this->_resource.runicPower;
        uint32 totalRunes = this->_resource.bloodRunes + this->_resource.frostRunes + this->_resource.unholyRunes;

        // Priority 1: Maintain Bone Shield
        if (_boneShieldTracker.NeedsRefresh() && totalRunes >= 2)
        {
            if (this->CanCastSpell(MARROWREND, target))
            {
                this->CastSpell(target, MARROWREND);
                _boneShieldTracker.ApplyMarrowrend(3);
                ConsumeRunes(RuneType::BLOOD, 2);
                GenerateRunicPower(15);
                return;
            }
        }

        // Priority 2: Death's and Decay (AoE)
        if (!_deathsAndDecayActive && rp >= 30)
        {
            if (this->CanCastSpell(DEATHS_AND_DECAY_BLOOD, this->GetBot()))
            {
                this->CastSpell(this->GetBot(), DEATHS_AND_DECAY_BLOOD);
                _deathsAndDecayActive = true;
                _deathsAndDecayEndTime = getMSTime() + 10000;
                ConsumeRunicPower(30);
                return;
            }
        }

        // Priority 3: Blood Boil (AoE threat)
        if (totalRunes >= 2 && this->CanCastSpell(BLOOD_BOIL, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), BLOOD_BOIL);
            ConsumeRunes(RuneType::BLOOD, 2);
            GenerateRunicPower(10);
            return;
        }

        // Priority 4: Heart Strike
        if (totalRunes >= 1 && this->CanCastSpell(HEART_STRIKE, target))
        {
            this->CastSpell(target, HEART_STRIKE);
            ConsumeRunes(RuneType::BLOOD, 1);
            GenerateRunicPower(10);
            return;
        }

        // Priority 5: Death Strike (heal)
        if (this->GetBot()->GetHealthPct() < 80.0f && rp >= 35)
        {
            if (this->CanCastSpell(DEATH_STRIKE, target))
            {
                this->CastSpell(target, DEATH_STRIKE);
                ConsumeRunicPower(35);
                return;
            }
        }
    }

    void HandleActiveMitigation()
    {
        Player* bot = this->GetBot();
        float healthPct = bot->GetHealthPct();

        // Anti-Magic Shell (magic damage)
        if (healthPct < 80.0f && this->CanCastSpell(ANTI_MAGIC_SHELL, bot))
        {
            this->CastSpell(bot, ANTI_MAGIC_SHELL);
            TC_LOG_DEBUG("playerbot", "Blood: Anti-Magic Shell");
            return;
        }

        // Rune Tap (talent, quick mitigation)
        if (healthPct < 70.0f && this->CanCastSpell(RUNE_TAP, bot))
        {
            this->CastSpell(bot, RUNE_TAP);
            TC_LOG_DEBUG("playerbot", "Blood: Rune Tap");
            return;
        }
    }

    void HandleEmergencyDefensives()
    {
        Player* bot = this->GetBot();
        float healthPct = bot->GetHealthPct();

        // Critical: Icebound Fortitude
        if (healthPct < 30.0f && this->CanCastSpell(ICEBOUND_FORTITUDE, bot))
        {
            this->CastSpell(bot, ICEBOUND_FORTITUDE);
            TC_LOG_DEBUG("playerbot", "Blood: Icebound Fortitude emergency");
            return;
        }

        // Very low: Vampiric Blood
        if (healthPct < 40.0f && this->CanCastSpell(VAMPIRIC_BLOOD, bot))
        {
            this->CastSpell(bot, VAMPIRIC_BLOOD);
            TC_LOG_DEBUG("playerbot", "Blood: Vampiric Blood");
            return;
        }

        // Low: Dancing Rune Weapon
        if (healthPct < 50.0f && this->CanCastSpell(DANCING_RUNE_WEAPON, bot))
        {
            this->CastSpell(bot, DANCING_RUNE_WEAPON);
            TC_LOG_DEBUG("playerbot", "Blood: Dancing Rune Weapon");
            return;
        }

        // Moderate: Death Strike spam
        if (healthPct < 60.0f && this->_resource.runicPower >= 35)
        {
            if (this->CanCastSpell(DEATH_STRIKE, bot))
            {
                this->CastSpell(bot, DEATH_STRIKE);
                ConsumeRunicPower(35);
                return;
            }
        }
    }

private:
    void UpdateBloodState()
    {
        // Update Bone Shield tracker
        _boneShieldTracker.Update(this->GetBot());

        // Update Death's and Decay
        if (_deathsAndDecayActive && getMSTime() >= _deathsAndDecayEndTime)
        {
            _deathsAndDecayActive = false;
            _deathsAndDecayEndTime = 0;
        }

        // Update Crimson Scourge proc
        if (this->GetBot() && this->GetBot()->HasAura(CRIMSON_SCOURGE))
            _crimsonScourgeProc = true;
        else
            _crimsonScourgeProc = false;

        // Update Runic Power from bot
        if (this->GetBot())
            this->_resource.runicPower = this->GetBot()->GetPower(POWER_RUNIC_POWER);

        // Update runes (simplified - in real implementation, track individual runes)
        // For now, assume runes regenerate over time
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
        RegisterCooldown(MARROWREND, 0);                // No CD, rune-gated
        RegisterCooldown(HEART_STRIKE, 0);              // No CD, rune-gated
        RegisterCooldown(BLOOD_BOIL, 0);                // No CD, rune-gated
        RegisterCooldown(DEATH_STRIKE, 0);              // No CD, RP-gated
        RegisterCooldown(DARK_COMMAND, 8000);           // 8 sec CD (taunt)
        RegisterCooldown(VAMPIRIC_BLOOD, 90000);        // 1.5 min CD
        RegisterCooldown(DANCING_RUNE_WEAPON, 120000);  // 2 min CD
        RegisterCooldown(ICEBOUND_FORTITUDE, 180000);   // 3 min CD
        RegisterCooldown(ANTI_MAGIC_SHELL, 60000);      // 1 min CD
        RegisterCooldown(RUNE_TAP, 25000);              // 25 sec CD
        RegisterCooldown(DEATH_GRIP, 25000);            // 25 sec CD
        RegisterCooldown(DEATHS_ADVANCE, 90000);        // 1.5 min CD
        RegisterCooldown(GOREFIENDS_GRASP, 120000);     // 2 min CD
        RegisterCooldown(ARMY_OF_THE_DEAD, 480000);     // 8 min CD
    }

private:
    BloodBoneShieldTracker _boneShieldTracker;
    bool _deathsAndDecayActive;
    uint32 _deathsAndDecayEndTime;
    bool _crimsonScourgeProc;
    uint32 _lastDeathStrikeTime;
};

} // namespace Playerbot
