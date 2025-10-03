/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "../ClassAI.h"
#include "PriestSpecialization.h"
#include "Position.h"
#include <unordered_map>
#include <queue>
#include <vector>

namespace Playerbot
{

// Priest AI implementation with full healing and shadow capabilities
class TC_GAME_API PriestAI : public ClassAI
{
public:
    explicit PriestAI(Player* bot);
    ~PriestAI();

    // ClassAI interface implementation
    void UpdateRotation(::Unit* target) override;
    void UpdateBuffs() override;
    void UpdateCooldowns(uint32 diff) override;
    bool CanUseAbility(uint32 spellId) override;

    // Combat state callbacks
    void OnCombatStart(::Unit* target) override;
    void OnCombatEnd() override;

protected:
    // Resource management
    bool HasEnoughResource(uint32 spellId) override;
    void ConsumeResource(uint32 spellId) override;

    // Positioning
    Position GetOptimalPosition(::Unit* target) override;
    float GetOptimalRange(::Unit* target) override;

private:
    // Specialization system
    PriestSpec _currentSpec;
    std::unique_ptr<PriestSpecialization> _specialization;

    // Performance tracking
    uint32 _manaSpent;
    uint32 _healingDone;
    uint32 _damageDealt;
    uint32 _playersHealed;
    uint32 _damagePrevented;

    // Shared utility tracking
    uint32 _lastDispel;
    uint32 _lastFearWard;
    uint32 _lastPsychicScream;
    uint32 _lastInnerFire;
    std::unordered_map<ObjectGuid, uint32> _mindControlTargets;

    // Specialization management
    void InitializeSpecialization();
    void UpdateSpecialization();
    PriestSpec DetectCurrentSpecialization();
    void SwitchSpecialization(PriestSpec newSpec);

    // Delegation to specialization
    void DelegateToSpecialization(::Unit* target);

    // Shared priest utilities
    void UpdatePriestBuffs();
    void CastInnerFire();
    void UpdateFortitudeBuffs();
    void CastPowerWordFortitude();

    // Mana management
    bool HasEnoughMana(uint32 amount);
    uint32 GetMana();
    uint32 GetMaxMana();
    float GetManaPercent();
    void OptimizeManaUsage();
    bool ShouldConserveMana();
    void UseManaRegeneration();
    void CastHymnOfHope();

    // Defensive abilities
    void UseDefensiveAbilities();
    void CastPsychicScream();
    void CastFade();
    void CastDispelMagic();
    void CastFearWard();
    void UseShadowProtection();

    // Crowd control
    void UseCrowdControl(::Unit* target);
    void CastMindControl(::Unit* target);
    void CastShackleUndead(::Unit* target);
    void CastSilence(::Unit* target);

    // Positioning and movement
    void UpdatePriestPositioning();
    bool IsAtOptimalHealingRange(::Unit* target);
    void MaintainHealingPosition();
    bool IsInDanger();
    void FindSafePosition();

    // Target selection
    ::Unit* GetBestHealTarget();
    ::Unit* GetBestDispelTarget();
    ::Unit* GetBestMindControlTarget();
    ::Unit* GetHighestPriorityPatient();

    // Utility and support
    void ProvideUtilitySupport();
    void UpdateDispelling();
    void CheckForDebuffs();
    void AssistGroupMembers();

    // Role adaptation
    void AdaptToGroupRole();
    void SwitchToHealingRole();
    void SwitchToDamageRole();
    void DetermineOptimalRole();

    // Specialization-specific mechanics
    void ManageHolyPower();
    void UpdateCircleOfHealing();
    void ManageSerendipity();

    void ManageDisciplineMechanics();
    void UpdateBorrowedTime();
    void ManageGrace();

    void ManageShadowMechanics();
    void UpdateShadowWeaving();
    void ManageVampiricEmbrace();

    // Threat and aggro management
    void ManageThreat();
    bool HasTooMuchThreat();
    void ReduceThreat();
    void UseFade();

    // Performance optimization
    void RecordHealingDone(uint32 amount, ::Unit* target);
    void RecordDamageDone(uint32 amount, ::Unit* target);
    void AnalyzeHealingEfficiency();
    void OptimizeHealingRotation();

    // Helper methods
    bool IsHealingSpell(uint32 spellId);
    bool IsDamageSpell(uint32 spellId);
    uint32 GetSpellHealAmount(uint32 spellId);
    uint32 GetHealOverTimeRemaining(::Unit* target, uint32 spellId);
    bool TargetHasHoT(::Unit* target, uint32 spellId);
    bool HasDispellableDebuff(::Unit* unit);
    bool IsTank(::Unit* unit);
    bool IsHealer(::Unit* unit);

    // Performance calculation methods
    uint32 CalculateDamageDealt(::Unit* target) const;
    uint32 CalculateHealingDone() const;
    uint32 CalculateManaUsage() const;

    // Missing utility methods
    void HealGroupMembers();
    void HealTarget(::Unit* target);
    bool IsEmergencyHeal(::Unit* target);
    bool IsEmergencyHeal(uint32 spellId);
    bool ShouldPrioritizeHealing(::Unit* target);
    bool ShouldUseShadowProtection();
    uint32 CountUnbuffedGroupMembers(uint32 spellId);
    void CheckMajorCooldowns();
    ::Unit* FindGroupTank();
    ::Unit* GetLowestHealthAlly(float maxRange);

    // Specialization detection
    PriestSpec DetectSpecialization();
    void OptimizeForSpecialization();
    bool HasTalent(uint32 talentId);

    // Constants
    static constexpr float OPTIMAL_HEALING_RANGE = 40.0f;
    static constexpr float SAFE_HEALING_RANGE = 30.0f;
    static constexpr uint32 HEAL_SCAN_INTERVAL = 1000; // 1 second
    static constexpr uint32 TRIAGE_INTERVAL = 500; // 0.5 seconds
    static constexpr uint32 DISPEL_COOLDOWN = 8000; // 8 seconds
    static constexpr uint32 FEAR_WARD_DURATION = 180000; // 3 minutes
    static constexpr float MANA_CONSERVATION_THRESHOLD = 0.3f; // 30%
    static constexpr float EMERGENCY_HEALTH_THRESHOLD = 0.25f; // 25%
    static constexpr float CRITICAL_HEALTH_THRESHOLD = 0.5f; // 50%

    // Spell IDs (version-specific)
    enum PriestSpells
    {
        // Healing spells
        HEAL = 2054,
        GREATER_HEAL = 2060,
        FLASH_HEAL = 2061,
        RENEW = 139,
        PRAYER_OF_HEALING = 596,
        CIRCLE_OF_HEALING = 34861,
        BINDING_HEAL = 32546,
        GUARDIAN_SPIRIT = 47788,

        // Discipline spells
        POWER_WORD_SHIELD = 17,
        PENANCE = 47540,
        PAIN_SUPPRESSION = 33206,
        POWER_INFUSION = 10060,
        INNER_FOCUS = 14751,

        // Shadow spells
        SHADOW_WORD_PAIN = 589,
        MIND_BLAST = 8092,
        MIND_FLAY = 15407,
        VAMPIRIC_TOUCH = 34914,
        DEVOURING_PLAGUE = 2944,
        SHADOW_WORD_DEATH = 32379,
        SHADOWFORM = 15473,
        VAMPIRIC_EMBRACE = 15286,

        // Utility spells
        DISPEL_MAGIC = 527,
        CURE_DISEASE = 528,
        ABOLISH_DISEASE = 552,
        PSYCHIC_SCREAM = 8122,
        MIND_CONTROL = 605,
        SHACKLE_UNDEAD = 9484,
        FADE = 586,
        FEAR_WARD = 6346,

        // Buffs
        POWER_WORD_FORTITUDE = 21562, // Updated for WoW 11.2
        PRAYER_OF_FORTITUDE = 21562,
        DIVINE_SPIRIT = 14752,
        PRAYER_OF_SPIRIT = 27681,
        INNER_FIRE = 588,
        SHADOW_PROTECTION = 976,
        PRAYER_OF_SHADOW_PROTECTION = 27683,

        // Cooldowns
        HYMN_OF_HOPE = 64901,
        DIVINE_HYMN = 64843,
        RESURRECTION = 2006,
        LEVITATE = 1706
    };
};

// Utility class for priest healing calculations
class TC_GAME_API PriestHealCalculator
{
public:
    // Healing calculations
    static uint32 CalculateHealAmount(uint32 spellId, Player* caster, ::Unit* target);
    static uint32 CalculateHealOverTime(uint32 spellId, Player* caster);
    static float CalculateHealEfficiency(uint32 spellId, Player* caster);

    // Mana efficiency
    static float CalculateHealPerMana(uint32 spellId, Player* caster);
    static uint32 GetOptimalHealForSituation(Player* caster, ::Unit* target, uint32 missingHealth);

    // Spell selection
    static bool ShouldUseDirectHeal(Player* caster, ::Unit* target);
    static bool ShouldUseHealOverTime(Player* caster, ::Unit* target);
    static bool ShouldUseGroupHeal(Player* caster, const std::vector<::Unit*>& targets);

    // Threat calculations
    static float CalculateHealThreat(uint32 healAmount, Player* caster);
    static bool WillOverheal(uint32 spellId, Player* caster, ::Unit* target);

private:
    // Cache for healing calculations
    static std::unordered_map<uint32, uint32> _baseHealCache;
    static std::unordered_map<uint32, float> _efficiencyCache;
    static std::mutex _cacheMutex;

    static void CacheHealData(uint32 spellId);
};

} // namespace Playerbot