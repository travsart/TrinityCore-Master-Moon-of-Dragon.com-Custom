/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * Protection Paladin Refactored - Template-Based Implementation
 *
 * This file provides a complete, template-based implementation of Protection Paladin
 * using the TankSpecialization with dual resource system (Mana + Holy Power).
 */

#pragma once

#include "../CombatSpecializationTemplates.h"
#include "../ResourceTypes.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellAuraEffects.h"
#include "Log.h"
#include "PaladinSpecialization.h"

namespace Playerbot
{

// ============================================================================
// PROTECTION PALADIN SPELL IDs (WoW 11.2 - The War Within)
// ============================================================================

enum ProtectionPaladinSpells
{
    // Holy Power Generators
    JUDGMENT_PROT            = 275779,  // 3% mana, 6 sec CD, 1 HP
    HAMMER_OF_WRATH_PROT     = 24275,   // 10% mana, 7.5 sec CD, 1 HP (execute)
    BLESSED_HAMMER           = 204019,  // 10% mana, 3 charges, 1 HP per charge (talent)
    AVENGERS_SHIELD          = 31935,   // 10% mana, 15 sec CD, ranged silence

    // Holy Power Spenders
    SHIELD_OF_THE_RIGHTEOUS  = 53600,   // 3 HP, physical damage reduction
    WORD_OF_GLORY_PROT       = 85673,   // 3 HP, self-heal
    LIGHT_OF_THE_PROTECTOR   = 184092,  // 3 HP, strong self-heal (talent)

    // Threat Generation
    CONSECRATION             = 26573,   // 18% mana, ground AoE
    HAMMER_OF_THE_RIGHTEOUS  = 53595,   // 9% mana, melee AoE

    // Active Mitigation
    GUARDIAN_OF_ANCIENT_KINGS = 86659,  // 5 min CD, 50% damage reduction
    ARDENT_DEFENDER          = 31850,   // 2 min CD, cheat death
    DIVINE_PROTECTION_PROT   = 498,     // 1 min CD, magic damage reduction
    BLESSING_OF_SPELLWARDING = 204018,  // Magic immunity (replaces Divine Protection)

    // Major Cooldowns
    AVENGING_WRATH_PROT      = 31884,   // 2 min CD, damage/healing buff
    SENTINEL                 = 389539,  // 5 min CD, massive armor (talent)
    FINAL_STAND              = 204077,  // Increases Ardent Defender effectiveness

    // Utility
    HAND_OF_RECKONING        = 62124,   // Taunt
    BLESSING_OF_FREEDOM_PROT = 1044,    // Remove movement impairment
    BLESSING_OF_PROTECTION_PROT = 1022, // Physical immunity
    LAY_ON_HANDS_PROT        = 633,     // 10 min CD, full heal
    DIVINE_SHIELD_PROT       = 642,     // 5 min CD, immunity
    CLEANSE_TOXINS           = 213644,  // Dispel poison/disease

    // Auras
    DEVOTION_AURA_PROT       = 465,     // Armor buff
    CONCENTRATION_AURA_PROT  = 317920,  // Interrupt resistance
    RETRIBUTION_AURA_PROT    = 183435,  // Damage reflect

    // Procs and Buffs
    GRAND_CRUSADER           = 85043,   // Proc: free Avenger's Shield
    SHINING_LIGHT            = 327510,  // Proc: free Word of Glory

    // Talents
    SERAPHIM                 = 152262,  // 3 HP, all stats buff
    BULWARK_OF_RIGHTEOUS_FURY = 386653, // Shield of the Righteous extended duration
    MOMENT_OF_GLORY          = 327193,  // Avenger's Shield CDR
    FIRST_AVENGER            = 203776   // Avenger's Shield extra charge
};

// Forward declaration - ManaHolyPowerResource should be defined in ResourceTypes.h
// If not, we define it here as a local type
#ifndef PLAYERBOT_RESOURCE_TYPES_MANA_HOLY_POWER
#define PLAYERBOT_RESOURCE_TYPES_MANA_HOLY_POWER
struct ManaHolyPowerResource
{
    uint32 mana{0};
    uint32 holyPower{0};
    uint32 maxMana{100000};
    uint32 maxHolyPower{5};

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
        return mana > 0 ? 100 : 0; // Simplified for ComplexResource concept
    }

    [[nodiscard]] uint32 GetMax() const {
        return 100; // Simplified for ComplexResource concept
    }

    void Initialize(Player* bot) {
        if (bot) {
            maxMana = bot->GetMaxPower(POWER_MANA);
            mana = bot->GetPower(POWER_MANA);
            maxHolyPower = bot->GetMaxPower(POWER_HOLY_POWER);
            holyPower = bot->GetPower(POWER_HOLY_POWER);
        }
    }
};
#endif // PLAYERBOT_RESOURCE_TYPES_MANA_HOLY_POWER

// ============================================================================
// PROTECTION PALADIN SHIELD TRACKER
// ============================================================================

class ProtectionShieldTracker
{
public:
    ProtectionShieldTracker()
        : _shieldActive(false)
        , _shieldEndTime(0)
        , _shieldStacks(0)
    {
    }

    void ApplyShield()
    {
        _shieldActive = true;
        _shieldEndTime = getMSTime() + 4500; // 4.5 sec duration
        _shieldStacks = 1;
    }

    bool IsActive() const { return _shieldActive; }

    uint32 GetTimeRemaining() const
    {
        if (!_shieldActive)
            return 0;

        uint32 now = getMSTime();
        return _shieldEndTime > now ? _shieldEndTime - now : 0;
    }

    bool NeedsRefresh() const
    {
        // Refresh if less than 1.5 seconds remaining
        return !_shieldActive || GetTimeRemaining() < 1500;
    }

    void Update()
    {
        if (_shieldActive && getMSTime() >= _shieldEndTime)
        {
            _shieldActive = false;
            _shieldEndTime = 0;
            _shieldStacks = 0;
        }
    }

private:
    bool _shieldActive;
    uint32 _shieldEndTime;
    uint32 _shieldStacks;
};

// ============================================================================
// PROTECTION PALADIN REFACTORED
// ============================================================================

class ProtectionPaladinRefactored : public TankSpecialization<ManaHolyPowerResource>, public PaladinSpecialization
{
public:
    using Base = TankSpecialization<ManaHolyPowerResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::_resource;
    explicit ProtectionPaladinRefactored(Player* bot)
        : TankSpecialization<ManaHolyPowerResource>(bot)
        , PaladinSpecialization(bot)
        , _shieldTracker()
        , _consecrationActive(false)
        , _consecrationEndTime(0)
        , _grandCrusaderProc(false)
        , _lastJudgmentTime(0)
        , _lastAvengersShieldTime(0)
    {
        // Initialize mana/holy power resources
        this->_resource.Initialize(bot);

        InitializeCooldowns();

        TC_LOG_DEBUG("playerbot", "ProtectionPaladinRefactored initialized for {}", bot->GetName());
    }

    void UpdateRotation(::Unit* target) override
    {
        if (!target || !target->IsAlive() || !target->IsHostileTo(this->GetBot()))
            return;

        // Update Protection state
        UpdateProtectionState();

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

        // Maintain Devotion Aura
        if (!bot->HasAura(DEVOTION_AURA_PROT) && this->CanCastSpell(DEVOTION_AURA_PROT, bot))
        {
            this->CastSpell(bot, DEVOTION_AURA_PROT);
        }

        // Emergency defensives
        HandleEmergencyDefensives();
    }

    // OnTauntRequired - calls base class virtual taunt method
    void TauntTarget(::Unit* target) override
    {
        if (this->CanCastSpell(HAND_OF_RECKONING, target))
        {
            this->CastSpell(target, HAND_OF_RECKONING);
            TC_LOG_DEBUG("playerbot", "Protection: Taunt cast on {}", target->GetName());
        }
    }

    // Note: GetOptimalRange is declared final in base template (TankSpecialization)
    // It returns 5.0f for all tanks, so no override needed

protected:
    void ExecuteSingleTargetThreatRotation(::Unit* target)
    {
        uint32 hp = this->_resource.holyPower;

        // Priority 1: Maintain Shield of the Righteous
        if (_shieldTracker.NeedsRefresh() && hp >= 3)
        {
            if (this->CanCastSpell(SHIELD_OF_THE_RIGHTEOUS, this->GetBot()))
            {
                this->CastSpell(this->GetBot(), SHIELD_OF_THE_RIGHTEOUS);
                _shieldTracker.ApplyShield();
                ConsumeHolyPower(3);
                return;
            }
        }

        // Priority 2: Use Grand Crusader proc
        if (_grandCrusaderProc)
        {
            if (this->CanCastSpell(AVENGERS_SHIELD, target))
            {
                this->CastSpell(target, AVENGERS_SHIELD);
                _lastAvengersShieldTime = getMSTime();
                _grandCrusaderProc = false;
                return;
            }
        }

        // Priority 3: Judgment for Holy Power
        if (hp < 5 && this->CanCastSpell(JUDGMENT_PROT, target))
        {
            this->CastSpell(target, JUDGMENT_PROT);
            _lastJudgmentTime = getMSTime();
            GenerateHolyPower(1);
            return;
        }

        // Priority 4: Hammer of Wrath (execute range)
        if (target->GetHealthPct() < 20.0f && hp < 5)
        {
            if (this->CanCastSpell(HAMMER_OF_WRATH_PROT, target))
            {
                this->CastSpell(target, HAMMER_OF_WRATH_PROT);
                GenerateHolyPower(1);
                return;
            }
        }

        // Priority 5: Avenger's Shield on cooldown
        if (this->CanCastSpell(AVENGERS_SHIELD, target))
        {
            this->CastSpell(target, AVENGERS_SHIELD);
            _lastAvengersShieldTime = getMSTime();
            return;
        }

        // Priority 6: Maintain Consecration
        if (!_consecrationActive && this->CanCastSpell(CONSECRATION, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), CONSECRATION);
            _consecrationActive = true;
            _consecrationEndTime = getMSTime() + 12000;
            return;
        }

        // Priority 7: Blessed Hammer (talent)
        if (hp < 5 && this->CanCastSpell(BLESSED_HAMMER, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), BLESSED_HAMMER);
            GenerateHolyPower(1);
            return;
        }

        // Priority 8: Hammer of the Righteous
        if (this->CanCastSpell(HAMMER_OF_THE_RIGHTEOUS, target))
        {
            this->CastSpell(target, HAMMER_OF_THE_RIGHTEOUS);
            return;
        }
    }

    void ExecuteAoEThreatRotation(::Unit* target, uint32 enemyCount)
    {
        uint32 hp = this->_resource.holyPower;

        // Priority 1: Shield of the Righteous
        if (_shieldTracker.NeedsRefresh() && hp >= 3)
        {
            if (this->CanCastSpell(SHIELD_OF_THE_RIGHTEOUS, this->GetBot()))
            {
                this->CastSpell(this->GetBot(), SHIELD_OF_THE_RIGHTEOUS);
                _shieldTracker.ApplyShield();
                ConsumeHolyPower(3);
                return;
            }
        }

        // Priority 2: Avenger's Shield (cleaves)
        if (this->CanCastSpell(AVENGERS_SHIELD, target))
        {
            this->CastSpell(target, AVENGERS_SHIELD);
            return;
        }

        // Priority 3: Consecration for AoE threat
        if (!_consecrationActive && this->CanCastSpell(CONSECRATION, this->GetBot()))
        {
            this->CastSpell(this->GetBot(), CONSECRATION);
            _consecrationActive = true;
            _consecrationEndTime = getMSTime() + 12000;
            return;
        }

        // Priority 4: Hammer of the Righteous AoE
        if (this->CanCastSpell(HAMMER_OF_THE_RIGHTEOUS, target))
        {
            this->CastSpell(target, HAMMER_OF_THE_RIGHTEOUS);
            return;
        }

        // Priority 5: Judgment
        if (hp < 5 && this->CanCastSpell(JUDGMENT_PROT, target))
        {
            this->CastSpell(target, JUDGMENT_PROT);
            GenerateHolyPower(1);
            return;
        }
    }

    void HandleEmergencyDefensives()
    {
        Player* bot = this->GetBot();
        float healthPct = bot->GetHealthPct();

        // Critical: Divine Shield
        if (healthPct < 15.0f && this->CanCastSpell(DIVINE_SHIELD_PROT, bot))
        {
            this->CastSpell(bot, DIVINE_SHIELD_PROT);
            TC_LOG_DEBUG("playerbot", "Protection: Divine Shield emergency");
            return;
        }

        // Very low: Lay on Hands
        if (healthPct < 20.0f && this->CanCastSpell(LAY_ON_HANDS_PROT, bot))
        {
            this->CastSpell(bot, LAY_ON_HANDS_PROT);
            TC_LOG_DEBUG("playerbot", "Protection: Lay on Hands emergency");
            return;
        }

        // Low: Guardian of Ancient Kings
        if (healthPct < 35.0f && this->CanCastSpell(GUARDIAN_OF_ANCIENT_KINGS, bot))
        {
            this->CastSpell(bot, GUARDIAN_OF_ANCIENT_KINGS);
            TC_LOG_DEBUG("playerbot", "Protection: Guardian of Ancient Kings");
            return;
        }

        // Moderate: Ardent Defender
        if (healthPct < 50.0f && this->CanCastSpell(ARDENT_DEFENDER, bot))
        {
            this->CastSpell(bot, ARDENT_DEFENDER);
            TC_LOG_DEBUG("playerbot", "Protection: Ardent Defender");
            return;
        }

        // Heal with Word of Glory
        if (healthPct < 60.0f && this->_resource.holyPower >= 3)
        {
            if (this->CanCastSpell(WORD_OF_GLORY_PROT, bot))
            {
                this->CastSpell(bot, WORD_OF_GLORY_PROT);
                ConsumeHolyPower(3);
                return;
            }
        }
    }

private:
    void UpdateProtectionState()
    {
        uint32 now = getMSTime();

        // Update Shield of the Righteous
        _shieldTracker.Update();

        // Update Consecration
        if (_consecrationActive && now >= _consecrationEndTime)
        {
            _consecrationActive = false;
            _consecrationEndTime = 0;
        }

        // Update Grand Crusader proc
        if (this->GetBot() && this->GetBot()->HasAura(GRAND_CRUSADER))
            _grandCrusaderProc = true;
        else
            _grandCrusaderProc = false;

        // Update Holy Power from bot
        if (this->GetBot())
            this->_resource.holyPower = this->GetBot()->GetPower(POWER_HOLY_POWER);
    }

    void GenerateHolyPower(uint32 amount)
    {
        this->_resource.holyPower = std::min(this->_resource.holyPower + amount, this->_resource.maxHolyPower);
    }

    void ConsumeHolyPower(uint32 amount)
    {
        this->_resource.holyPower = (this->_resource.holyPower > amount) ? this->_resource.holyPower - amount : 0;
    }

    void InitializeCooldowns()
    {
        RegisterCooldown(JUDGMENT_PROT, 6000);              // 6 sec CD
        RegisterCooldown(HAMMER_OF_WRATH_PROT, 7500);       // 7.5 sec CD
        RegisterCooldown(AVENGERS_SHIELD, 15000);           // 15 sec CD
        RegisterCooldown(GUARDIAN_OF_ANCIENT_KINGS, 300000); // 5 min CD
        RegisterCooldown(ARDENT_DEFENDER, 120000);          // 2 min CD
        RegisterCooldown(DIVINE_PROTECTION_PROT, 60000);    // 1 min CD
        RegisterCooldown(AVENGING_WRATH_PROT, 120000);      // 2 min CD
        RegisterCooldown(LAY_ON_HANDS_PROT, 600000);        // 10 min CD
        RegisterCooldown(DIVINE_SHIELD_PROT, 300000);       // 5 min CD
        RegisterCooldown(HAND_OF_RECKONING, 8000);          // 8 sec CD (taunt)
        RegisterCooldown(SERAPHIM, 45000);                  // 45 sec CD
    }

private:
    ProtectionShieldTracker _shieldTracker;
    bool _consecrationActive;
    uint32 _consecrationEndTime;
    bool _grandCrusaderProc;
    uint32 _lastJudgmentTime;
    uint32 _lastAvengersShieldTime;
};

} // namespace Playerbot
