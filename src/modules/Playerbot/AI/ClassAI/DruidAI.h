/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "ClassAI.h"
#include "Position.h"
#include <unordered_map>
#include <queue>
#include <vector>

namespace Playerbot
{

// Druid specializations
enum class DruidSpec : uint8
{
    BALANCE = 0,
    FERAL = 1,
    GUARDIAN = 2,
    RESTORATION = 3
};

// Druid forms
enum class DruidForm : uint8
{
    HUMANOID = 0,
    BEAR = 1,
    CAT = 2,
    AQUATIC = 3,
    TRAVEL = 4,
    MOONKIN = 5,
    TREE_OF_LIFE = 6,
    FLIGHT = 7
};

// Eclipse states for Balance
enum class EclipseState : uint8
{
    NONE = 0,
    SOLAR = 1,
    LUNAR = 2
};

// Combo point tracking for Feral
struct ComboPointInfo
{
    uint32 current;
    uint32 maximum;
    uint32 lastGenerated;
    ::Unit* target;

    ComboPointInfo() : current(0), maximum(5), lastGenerated(0), target(nullptr) {}

    bool HasComboPoints(uint32 required = 1) const { return current >= required; }
    void AddComboPoint() { current = std::min(current + 1, maximum); }
    void SpendComboPoints() { current = 0; }
    void SetTarget(::Unit* t) { target = t; }
};

// HoT tracking for Restoration
struct HealOverTimeInfo
{
    uint32 spellId;
    ::Unit* target;
    uint32 remainingTime;
    uint32 ticksRemaining;
    uint32 healPerTick;
    uint32 lastTick;

    HealOverTimeInfo() : spellId(0), target(nullptr), remainingTime(0),
                         ticksRemaining(0), healPerTick(0), lastTick(0) {}

    HealOverTimeInfo(uint32 spell, ::Unit* tgt, uint32 duration, uint32 healing)
        : spellId(spell), target(tgt), remainingTime(duration),
          ticksRemaining(duration / 3000), healPerTick(healing), lastTick(getMSTime()) {}
};

// Form transition tracking
struct FormTransition
{
    DruidForm fromForm;
    DruidForm toForm;
    uint32 lastTransition;
    uint32 cooldown;
    bool inProgress;

    FormTransition() : fromForm(DruidForm::HUMANOID), toForm(DruidForm::HUMANOID),
                       lastTransition(0), cooldown(1500), inProgress(false) {}
};

// Druid AI implementation with full form management and shapeshifting
class TC_GAME_API DruidAI : public ClassAI
{
public:
    explicit DruidAI(Player* bot);
    ~DruidAI() = default;

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
    // Druid-specific data
    DruidSpec _specialization;
    DruidForm _currentForm;
    DruidForm _previousForm;
    uint32 _damageDealt;
    uint32 _healingDone;
    uint32 _formShifts;
    uint32 _manaSpent;

    // Form management system
    FormTransition _formTransition;
    std::unordered_map<DruidForm, uint32> _formCooldowns;
    uint32 _lastFormShift;
    uint32 _formShiftGCD;
    bool _needsFormShift;
    DruidForm _optimalForm;

    // Balance Eclipse system
    EclipseState _eclipseState;
    uint32 _solarEnergy;
    uint32 _lunarEnergy;
    uint32 _lastEclipseShift;
    uint32 _starfireCount;
    uint32 _wrathCount;
    bool _eclipseActive;

    // Feral combo point system
    ComboPointInfo _comboPoints;
    uint32 _energy;
    uint32 _maxEnergy;
    uint32 _lastEnergyRegen;
    uint32 _tigersFuryReady;
    uint32 _savageRoarRemaining;
    uint32 _ripRemaining;

    // Guardian rage system
    uint32 _rage;
    uint32 _maxRage;
    uint32 _lastRageDecay;
    uint32 _thrashStacks;
    uint32 _lacerateStacks;
    uint32 _survivalInstinctsReady;
    uint32 _frenziedRegenerationReady;

    // Restoration healing system
    std::unordered_map<ObjectGuid, std::vector<HealOverTimeInfo>> _activeHoTs;
    std::priority_queue<::Unit*> _healingPriorities;
    uint32 _lastGroupScan;
    uint32 _treeOfLifeRemaining;
    uint32 _tranquilityReady;
    bool _inTreeForm;

    // DoT and HoT tracking
    std::unordered_map<ObjectGuid, uint32> _moonfireTimers;
    std::unordered_map<ObjectGuid, uint32> _sunfireTimers;
    std::unordered_map<ObjectGuid, uint32> _rejuvenationTimers;
    std::unordered_map<ObjectGuid, uint32> _lifebloomTimers;
    std::unordered_map<ObjectGuid, uint32> _regrowthTimers;

    // Utility tracking
    uint32 _lastInnervate;
    uint32 _lastBarkskin;
    uint32 _lastNaturesSwiftness;
    uint32 _lastEntanglingRoots;
    uint32 _lastCyclone;
    uint32 _lastHibernate;

    // Rotation methods by specialization
    void UpdateBalanceRotation(::Unit* target);
    void UpdateFeralRotation(::Unit* target);
    void UpdateGuardianRotation(::Unit* target);
    void UpdateRestorationRotation(::Unit* target);

    // Form management system
    void UpdateFormManagement();
    void ShiftToOptimalForm();
    void ShiftToForm(DruidForm form);
    bool CanShiftToForm(DruidForm form);
    DruidForm GetOptimalFormForSituation();
    DruidForm GetOptimalFormForSpecialization();
    bool IsFormShiftNeeded();
    void HandleFormTransition();

    // Form-specific checks
    bool IsInCombatForm();
    bool IsInHealingForm();
    bool IsInTravelForm();
    bool CanCastInCurrentForm(uint32 spellId);
    std::vector<uint32> GetAvailableSpellsInForm(DruidForm form);

    // Eclipse management (Balance)
    void UpdateEclipseSystem();
    void AdvanceEclipse();
    void CastEclipseSpell(::Unit* target);
    bool ShouldCastStarfire();
    bool ShouldCastWrath();
    void ManageEclipseState();

    // Combo point management (Feral)
    void UpdateComboPointSystem();
    void GenerateComboPoint(::Unit* target);
    void SpendComboPoints(::Unit* target);
    bool ShouldSpendComboPoints();
    void OptimizeComboPointUsage();

    // Energy management (Feral)
    void UpdateEnergyManagement();
    bool HasEnoughEnergy(uint32 required);
    void SpendEnergy(uint32 amount);
    uint32 GetEnergy();
    float GetEnergyPercent();

    // Rage management (Guardian)
    void UpdateRageManagement();
    void GenerateRage(uint32 amount);
    void SpendRage(uint32 amount);
    bool HasEnoughRage(uint32 required);
    uint32 GetRage();
    float GetRagePercent();

    // HoT management (Restoration)
    void UpdateHealOverTimeManagement();
    void ApplyHealingOverTime(::Unit* target, uint32 spellId);
    void RefreshExpiringHoTs();
    bool ShouldApplyHoT(::Unit* target, uint32 spellId);
    uint32 GetHoTRemainingTime(::Unit* target, uint32 spellId);
    void RemoveExpiredHoTs();

    // DoT management (Balance/Feral)
    void UpdateDamageOverTimeManagement();
    void ApplyDoT(::Unit* target, uint32 spellId);
    void RefreshExpiringDoTs();
    bool ShouldApplyDoT(::Unit* target, uint32 spellId);
    uint32 GetDoTRemainingTime(::Unit* target, uint32 spellId);

    // Balance abilities
    void UseBalanceAbilities(::Unit* target);
    void CastStarfire(::Unit* target);
    void CastWrath(::Unit* target);
    void CastMoonfire(::Unit* target);
    void CastSunfire(::Unit* target);
    void CastStarsurge(::Unit* target);
    void CastForceOfNature();
    void EnterMoonkinForm();

    // Feral abilities
    void UseFeralAbilities(::Unit* target);
    void CastShred(::Unit* target);
    void CastMangle(::Unit* target);
    void CastRake(::Unit* target);
    void CastRip(::Unit* target);
    void CastFerociosBite(::Unit* target);
    void CastSavageRoar();
    void CastTigersFury();
    void EnterCatForm();

    // Guardian abilities
    void UseGuardianAbilities(::Unit* target);
    void CastMaul(::Unit* target);
    void CastThrash();
    void CastSwipe();
    void CastLacerate(::Unit* target);
    void CastFrenziedRegeneration();
    void CastSurvivalInstincts();
    void EnterBearForm();

    // Restoration abilities
    void UseRestorationAbilities();
    void CastHealingTouch(::Unit* target);
    void CastRegrowth(::Unit* target);
    void CastRejuvenation(::Unit* target);
    void CastLifebloom(::Unit* target);
    void CastSwiftmend(::Unit* target);
    void CastTranquility();
    void CastInnervate(::Unit* target);
    void EnterTreeOfLifeForm();

    // Utility abilities
    void UseUtilityAbilities();
    void CastBarkskin();
    void CastNaturesSwiftness();
    void CastEntanglingRoots(::Unit* target);
    void CastCyclone(::Unit* target);
    void CastHibernate(::Unit* target);
    void CastRemoveCurse(::Unit* target);

    // Travel and mobility
    void UseTravelAbilities();
    void EnterTravelForm();
    void EnterAquaticForm();
    void EnterFlightForm();
    void CastDash();
    void ManageTravelForms();

    // Mana management
    bool HasEnoughMana(uint32 amount);
    uint32 GetMana();
    uint32 GetMaxMana();
    float GetManaPercent();
    void OptimizeManaUsage();
    void ConserveMana();

    // Positioning and movement
    void UpdateDruidPositioning();
    bool IsAtOptimalRange(::Unit* target);
    void MaintainOptimalRange(::Unit* target);
    bool NeedsRepositioning();

    // Target selection
    ::Unit* GetBestHealTarget();
    ::Unit* GetBestDoTTarget();
    ::Unit* GetBestCrowdControlTarget();
    ::Unit* GetHighestThreatTarget();
    std::vector<::Unit*> GetAoETargets();

    // Threat and aggro management
    void ManageThreat();
    bool HasTooMuchThreat();
    void BuildThreat(::Unit* target);
    void ReduceThreat();

    // Emergency responses
    void HandleEmergencySituation();
    bool IsInCriticalDanger();
    void UseEmergencyAbilities();
    void UseOhShitButtons();

    // Performance optimization
    void RecordDamageDealt(uint32 damage, ::Unit* target);
    void RecordHealingDone(uint32 amount, ::Unit* target);
    void AnalyzeFormEffectiveness();
    void OptimizeFormUsage();

    // Helper methods
    bool IsChanneling();
    bool IsShapeshifted();
    DruidSpec DetectSpecialization();
    bool HasTalent(uint32 talentId);
    uint32 GetShapeshiftSpellId(DruidForm form);
    bool RequiresHumanoidForm(uint32 spellId);

    // Constants
    static constexpr float MELEE_RANGE = 5.0f;
    static constexpr float OPTIMAL_CASTING_RANGE = 30.0f;
    static constexpr float OPTIMAL_HEALING_RANGE = 40.0f;
    static constexpr uint32 FORM_SHIFT_GCD = 1500; // 1.5 seconds
    static constexpr uint32 ECLIPSE_ENERGY_MAX = 100;
    static constexpr uint32 COMBO_POINTS_MAX = 5;
    static constexpr uint32 ENERGY_MAX = 100;
    static constexpr uint32 RAGE_MAX = 100;
    static constexpr uint32 ENERGY_REGEN_RATE = 10; // per second
    static constexpr uint32 RAGE_DECAY_RATE = 2; // per second
    static constexpr float MANA_CONSERVATION_THRESHOLD = 0.3f; // 30%
    static constexpr uint32 HOT_REFRESH_THRESHOLD = 6000; // 6 seconds before expiry
    static constexpr uint32 DOT_REFRESH_THRESHOLD = 6000; // 6 seconds before expiry

    // Spell IDs (version-specific)
    enum DruidSpells
    {
        // Shapeshift forms
        BEAR_FORM = 5487,
        CAT_FORM = 768,
        AQUATIC_FORM = 1066,
        TRAVEL_FORM = 783,
        MOONKIN_FORM = 24858,
        TREE_OF_LIFE = 33891,
        FLIGHT_FORM = 33943,

        // Balance spells
        STARFIRE = 2912,
        WRATH = 5176,
        MOONFIRE = 8921,
        SUNFIRE = 93402,
        STARSURGE = 78674,
        FORCE_OF_NATURE = 33831,
        ECLIPSE_SOLAR = 48517,
        ECLIPSE_LUNAR = 48518,

        // Feral spells
        SHRED = 5221,
        MANGLE_CAT = 33876,
        RAKE = 1822,
        RIP = 1079,
        FEROCIOUS_BITE = 22568,
        SAVAGE_ROAR = 52610,
        TIGERS_FURY = 5217,
        DASH = 1850,

        // Guardian spells
        MAUL = 6807,
        MANGLE_BEAR = 33878,
        THRASH = 77758,
        SWIPE = 779,
        LACERATE = 33745,
        FRENZIED_REGENERATION = 22842,
        SURVIVAL_INSTINCTS = 61336,

        // Restoration spells
        HEALING_TOUCH = 5185,
        REGROWTH = 8936,
        REJUVENATION = 774,
        LIFEBLOOM = 33763,
        SWIFTMEND = 18562,
        TRANQUILITY = 740,
        INNERVATE = 29166,
        NATURES_SWIFTNESS = 17116,

        // Utility spells
        BARKSKIN = 22812,
        ENTANGLING_ROOTS = 339,
        CYCLONE = 33786,
        HIBERNATE = 2637,
        REMOVE_CURSE = 2782,
        ABOLISH_POISON = 2893,

        // Buffs
        MARK_OF_THE_WILD = 1126,
        THORNS = 467,
        OMEN_OF_CLARITY = 16864
    };
};

// Utility class for druid calculations
class TC_GAME_API DruidCalculator
{
public:
    // Damage calculations
    static uint32 CalculateStarfireDamage(Player* caster, ::Unit* target);
    static uint32 CalculateWrathDamage(Player* caster, ::Unit* target);
    static uint32 CalculateShredDamage(Player* caster, ::Unit* target);

    // Healing calculations
    static uint32 CalculateHealingTouchAmount(Player* caster, ::Unit* target);
    static uint32 CalculateRegrowthAmount(Player* caster, ::Unit* target);
    static uint32 CalculateRejuvenationTick(Player* caster, ::Unit* target);

    // Eclipse calculations
    static EclipseState CalculateNextEclipseState(uint32 solarEnergy, uint32 lunarEnergy);
    static uint32 CalculateEclipseDamageBonus(EclipseState state, uint32 spellId);

    // Form optimization
    static DruidForm GetOptimalFormForSituation(DruidSpec spec, bool inCombat, bool needsHealing);
    static bool ShouldShiftToForm(DruidForm current, DruidForm desired, Player* caster);
    static uint32 CalculateFormShiftCost(DruidForm fromForm, DruidForm toForm);

    // HoT/DoT efficiency
    static float CalculateHoTEfficiency(uint32 spellId, Player* caster, ::Unit* target);
    static float CalculateDoTEfficiency(uint32 spellId, Player* caster, ::Unit* target);
    static bool ShouldRefreshHoT(uint32 spellId, ::Unit* target, uint32 remainingTime);

private:
    // Cache for druid calculations
    static std::unordered_map<uint32, uint32> _damageCache;
    static std::unordered_map<uint32, uint32> _healingCache;
    static std::unordered_map<DruidForm, uint32> _formEfficiencyCache;
    static std::mutex _cacheMutex;

    static void CacheDruidData();
};

// Form manager for intelligent form switching
class TC_GAME_API DruidFormManager
{
public:
    DruidFormManager(DruidAI* owner);
    ~DruidFormManager() = default;

    // Form management
    void Update(uint32 diff);
    void RequestFormShift(DruidForm targetForm);
    bool CanShiftToForm(DruidForm form) const;
    void ForceFormShift(DruidForm form);

    // Form state
    DruidForm GetCurrentForm() const;
    DruidForm GetPreviousForm() const;
    bool IsShifting() const;
    uint32 GetFormShiftCooldown() const;

    // Form optimization
    DruidForm GetOptimalForm() const;
    void OptimizeFormForSituation();
    void AdaptToSituation(bool inCombat, bool needsHealing, bool needsTravel);

private:
    DruidAI* _owner;
    DruidForm _currentForm;
    DruidForm _previousForm;
    DruidForm _requestedForm;
    bool _isShifting;
    uint32 _lastShift;
    uint32 _shiftCooldown;

    // Form analysis
    void AnalyzeFormNeeds();
    bool ShouldShiftForCombat() const;
    bool ShouldShiftForHealing() const;
    bool ShouldShiftForTravel() const;
    DruidForm GetCombatForm() const;
    DruidForm GetHealingForm() const;
    DruidForm GetTravelForm() const;
};

// Eclipse controller for Balance druids
class TC_GAME_API EclipseController
{
public:
    EclipseController(DruidAI* owner);
    ~EclipseController() = default;

    // Eclipse management
    void Update(uint32 diff);
    void CastEclipseSpell(::Unit* target);
    EclipseState GetCurrentState() const;
    uint32 GetSolarEnergy() const;
    uint32 GetLunarEnergy() const;

    // Eclipse optimization
    bool ShouldCastStarfire() const;
    bool ShouldCastWrath() const;
    void OptimizeEclipseRotation(::Unit* target);

private:
    DruidAI* _owner;
    EclipseState _currentState;
    uint32 _solarEnergy;
    uint32 _lunarEnergy;
    uint32 _lastEclipseUpdate;

    void UpdateEclipseEnergy();
    void AdvanceToNextEclipse();
    uint32 CalculateSpellEclipseValue(uint32 spellId);
};

} // namespace Playerbot