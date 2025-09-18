/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef TRINITY_SURVIVALSPECIALIZATION_H
#define TRINITY_SURVIVALSPECIALIZATION_H

#include "HunterSpecialization.h"
#include <queue>

namespace Playerbot
{

// Survival specific spells
enum SurvivalSpells : uint32
{
    // Survival talents and abilities
    HUNTER_VS_WILD_SURVIVAL = 56339,
    SURVIVALIST = 19255,
    ENTRAPMENT = 19184,
    SAVAGE_STRIKES = 19159,
    DEFLECTION = 19295,
    IMPROVED_WING_CLIP = 19229,
    CLEVER_TRAPS = 19239,
    SURVIVALIST_TALENT = 19255,
    SUREFOOTED = 19290,
    TRAP_MASTERY = 19376,
    LIGHTNING_REFLEXES = 19168,
    RESOURCEFULNESS = 34491,
    EXPOSE_WEAKNESS = 34500,
    MASTER_TACTICIAN = 34506,
    COUNTERATTACK_TALENT = 19306,
    DETERRENCE_TALENT = 19263,
    ASPECT_MASTERY = 53265,
    HUNTING_PARTY = 53290,
    LOCK_AND_LOAD = 56342,
    EXPLOSIVE_SHOT_TALENT = 60053,
    T_N_T = 56333,
    BLACK_ARROW_TALENT = 3674,

    // Survival specific shots and abilities
    EXPLOSIVE_SHOT_SURVIVAL = 60053,
    BLACK_ARROW_SURVIVAL = 3674,
    WYVERN_STING_SURVIVAL = 19386,
    COUNTERATTACK_ABILITY = 19306,
    MONGOOSE_BITE_RANK_5 = 14271,
    RAPTOR_STRIKE_RANK_11 = 48996,
    WING_CLIP_RANK_3 = 14268,

    // Melee abilities
    RAPTOR_STRIKE_BASE = 2973,
    MONGOOSE_BITE_BASE = 1495,
    WING_CLIP_BASE = 2974,
    COUNTERATTACK_BASE = 19306,

    // Survival-focused utility
    DETERRENCE_ABILITY = 19263,
    ASPECT_OF_THE_MONKEY_IMPROVED = 13163,
    CAMOUFLAGE = 51753,
    MASTER_S_CALL_SURVIVAL = 53271
};

// Survival combat modes
enum class SurvivalMode : uint8
{
    RANGED_DOT = 0,     // Standard ranged with DoTs
    MELEE_HYBRID = 1,   // Close combat with melee abilities
    TRAP_CONTROL = 2,   // Heavy trap usage
    KITING = 3,         // Movement-based combat
    DEFENSIVE = 4,      // Survival focus
    BURST_DOT = 5,      // DoT burst phase
    EXECUTE = 6         // Low health finishing
};

// DoT management system
struct DotEffect
{
    uint32 spellId;
    ObjectGuid targetGuid;
    uint32 applicationTime;
    uint32 duration;
    uint32 tickInterval;
    uint32 damagePerTick;
    uint32 remainingTicks;
    bool isRefreshable;

    DotEffect(uint32 spell = 0, ObjectGuid target = ObjectGuid::Empty, uint32 applied = 0,
              uint32 dur = 0, uint32 interval = 3000, uint32 damage = 0, uint32 ticks = 0, bool refresh = true)
        : spellId(spell), targetGuid(target), applicationTime(applied), duration(dur),
          tickInterval(interval), damagePerTick(damage), remainingTicks(ticks), isRefreshable(refresh) {}

    bool IsActive() const { return (getMSTime() - applicationTime) < duration; }
    bool NeedsRefresh() const { return isRefreshable && GetRemainingTime() < (duration * 0.3f); }
    uint32 GetRemainingTime() const
    {
        uint32 elapsed = getMSTime() - applicationTime;
        return elapsed < duration ? (duration - elapsed) : 0;
    }
    float GetRemainingDamage() const { return remainingTicks * damagePerTick; }
};

// Trap strategy system
enum class TrapStrategy : uint8
{
    DEFENSIVE = 0,      // Protect hunter
    OFFENSIVE = 1,      // Damage dealing
    CONTROL = 2,        // Crowd control
    AREA_DENIAL = 3,    // Zone control
    COMBO = 4          // Multi-trap combinations
};

// Trap placement optimization
struct TrapPlacement
{
    Position position;
    uint32 trapSpell;
    TrapStrategy strategy;
    uint32 priority;
    uint32 placementTime;
    float effectiveRadius;

    TrapPlacement(Position pos = Position(), uint32 spell = 0, TrapStrategy strat = TrapStrategy::DEFENSIVE,
                  uint32 prio = 0, uint32 time = 0, float radius = 8.0f)
        : position(pos), trapSpell(spell), strategy(strat), priority(prio),
          placementTime(time), effectiveRadius(radius) {}

    bool IsOptimalForTarget(::Unit* target) const;
    float CalculateEffectiveness(::Unit* target) const;
};

// Melee combat tracking
struct MeleeSequence
{
    std::queue<uint32> abilityQueue;
    uint32 lastMeleeTime;
    uint32 comboPoints;
    bool inMeleeRange;
    float meleeEfficiency;

    MeleeSequence() : lastMeleeTime(0), comboPoints(0), inMeleeRange(false), meleeEfficiency(0.0f) {}

    void AddAbility(uint32 spellId) { abilityQueue.push(spellId); }
    uint32 GetNextAbility() { return abilityQueue.empty() ? 0 : abilityQueue.front(); }
    void ConsumeAbility() { if (!abilityQueue.empty()) abilityQueue.pop(); }
    bool HasAbilities() const { return !abilityQueue.empty(); }
};

class SurvivalSpecialization : public HunterSpecialization
{
public:
    explicit SurvivalSpecialization(Player* bot);
    ~SurvivalSpecialization() override = default;

    // Core rotation interface
    void UpdateRotation(::Unit* target) override;
    void UpdateBuffs() override;
    void UpdateCooldowns(uint32 diff) override;
    bool CanUseAbility(uint32 spellId) override;
    void OnCombatStart(::Unit* target) override;
    void OnCombatEnd() override;
    bool HasEnoughResource(uint32 spellId) override;
    void ConsumeResource(uint32 spellId) override;
    Position GetOptimalPosition(::Unit* target) override;
    float GetOptimalRange(::Unit* target) override;

    // Pet management interface (basic for Survival)
    void UpdatePetManagement() override;
    void SummonPet() override;
    void CommandPetAttack(::Unit* target) override;
    void CommandPetFollow() override;
    void CommandPetStay() override;
    void MendPetIfNeeded() override;
    void FeedPetIfNeeded() override;
    bool HasActivePet() const override;
    PetInfo GetPetInfo() const override;

    // Trap management interface (advanced for Survival)
    void UpdateTrapManagement() override;
    void PlaceTrap(uint32 trapSpell, Position position) override;
    bool ShouldPlaceTrap() const override;
    uint32 GetOptimalTrapSpell() const override;
    std::vector<TrapInfo> GetActiveTraps() const override;

    // Aspect management interface
    void UpdateAspectManagement() override;
    void SwitchToOptimalAspect() override;
    uint32 GetOptimalAspect() const override;
    bool HasCorrectAspect() const override;

    // Range and positioning
    void UpdateRangeManagement() override;
    bool IsInDeadZone(::Unit* target) const override;
    bool ShouldKite(::Unit* target) const override;
    Position GetKitePosition(::Unit* target) const override;
    void HandleDeadZone(::Unit* target) override;

    // Tracking management
    void UpdateTracking() override;
    uint32 GetOptimalTracking() const override;
    void ApplyTracking(uint32 trackingSpell) override;

private:
    // Survival specific rotation methods
    bool ExecuteRangedDotRotation(::Unit* target);
    bool ExecuteMeleeHybridRotation(::Unit* target);
    bool ExecuteTrapControlRotation(::Unit* target);
    bool ExecuteKitingRotation(::Unit* target);
    bool ExecuteDefensiveRotation(::Unit* target);
    bool ExecuteBurstDotRotation(::Unit* target);
    bool ExecuteExecuteRotation(::Unit* target);

    // DoT management system
    void UpdateDotManagement();
    void ApplyDot(::Unit* target, uint32 spellId);
    void RefreshExpiredDots();
    bool ShouldApplyDot(::Unit* target, uint32 spellId) const;
    bool ShouldRefreshDot(::Unit* target, uint32 spellId) const;
    std::vector<DotEffect> GetActiveDots() const;
    DotEffect* GetDotOnTarget(::Unit* target, uint32 spellId);
    float CalculateDotDps() const;
    void OptimizeDotRotation(::Unit* target);

    // Advanced trap system
    void UpdateAdvancedTrapManagement();
    void PlanTrapSequence(::Unit* target);
    void ExecuteTrapCombo(::Unit* target);
    TrapPlacement CalculateOptimalTrapPosition(::Unit* target, uint32 trapSpell) const;
    bool ShouldUseTrapCombo() const;
    void HandleTrapTriggers();
    void UpdateTrapCooldowns();

    // Melee combat system
    void UpdateMeleeManagement();
    void PlanMeleeSequence(::Unit* target);
    void ExecuteMeleeCombo(::Unit* target);
    bool ShouldEngageMelee(::Unit* target) const;
    bool ShouldExitMelee(::Unit* target) const;
    void OptimizeMeleeDps(::Unit* target);

    // Survival specific abilities
    bool ShouldUseExplosiveShot(::Unit* target) const;
    bool ShouldUseBlackArrow(::Unit* target) const;
    bool ShouldUseWyvernSting(::Unit* target) const;
    bool ShouldUseCounterattack(::Unit* target) const;
    bool ShouldUseMongooseBite(::Unit* target) const;
    bool ShouldUseRaptorStrike(::Unit* target) const;
    bool ShouldUseDeterrence() const;

    void CastExplosiveShot(::Unit* target);
    void CastBlackArrow(::Unit* target);
    void CastWyvernSting(::Unit* target);
    void CastCounterattack(::Unit* target);
    void CastMongooseBite(::Unit* target);
    void CastRaptorStrike(::Unit* target);
    void CastDeterrence();

    // Survival mode management
    void UpdateSurvivalMode();
    void AdaptToThreatLevel();
    SurvivalMode DetermineBestMode(::Unit* target) const;
    void TransitionToMode(SurvivalMode newMode);

    // Kiting and positioning
    void UpdateKitingStrategy();
    void ExecuteKitingPattern(::Unit* target);
    Position CalculateNextKitePosition(::Unit* target) const;
    bool ShouldUseSlowingEffects(::Unit* target) const;
    void ApplySlowingEffects(::Unit* target);

    // Resource efficiency
    void UpdateResourceEfficiency();
    void OptimizeManaForDots();
    bool ShouldConserveManaForTraps() const;
    void PrioritizeResourceUsage();

    // Threat and aggro management
    void UpdateThreatManagement();
    void HandleHighThreat();
    void ReduceThreatGeneration();
    bool ShouldUseFeignDeath() const;

    // Survival specific state
    SurvivalMode _survivalMode;
    TrapStrategy _currentTrapStrategy;
    MeleeSequence _meleeSequence;
    std::vector<DotEffect> _activeDots;
    std::vector<TrapPlacement> _plannedTraps;

    // Timing and management
    uint32 _lastDotCheck;
    uint32 _lastTrapCheck;
    uint32 _lastMeleeCheck;
    uint32 _lastModeUpdate;
    uint32 _lastKiteUpdate;
    uint32 _lastThreatCheck;

    // Cooldown tracking
    uint32 _explosiveShotReady;
    uint32 _blackArrowReady;
    uint32 _wyvernStingReady;
    uint32 _deterrenceReady;
    uint32 _feignDeathReady;
    uint32 _lastExplosiveShot;
    uint32 _lastBlackArrow;
    uint32 _lastWyvernSting;
    uint32 _lastDeterrence;
    uint32 _lastFeignDeath;

    // Combat metrics
    uint32 _totalDotDamage;
    uint32 _totalTrapDamage;
    uint32 _totalMeleeDamage;
    uint32 _totalRangedDamage;
    uint32 _dotsApplied;
    uint32 _trapsTriggered;
    uint32 _meleeHits;
    uint32 _kitingTime;
    float _dotUptime;
    float _trapEfficiency;
    float _meleeEfficiency;

    // Multi-target DoT tracking
    std::unordered_map<ObjectGuid, std::vector<DotEffect>> _targetDots;
    uint32 _dotTargetCount;
    uint32 _maxDotTargets;

    // Defensive state tracking
    uint32 _lastDamageTaken;
    uint32 _consecutiveHits;
    float _currentThreatLevel;
    bool _inEmergencyMode;
    bool _kitingActive;
    bool _inMeleeMode;
    bool _deterrenceActive;

    // Advanced positioning
    std::queue<Position> _kitingPath;
    Position _safePosition;
    Position _trapPosition;
    float _optimalKiteDistance;
    uint32 _lastPositionChange;

    // Trap state
    uint32 _activeTrapCount;
    uint32 _trapCooldownRemaining;
    uint32 _lastTrapPlacement;
    bool _trapComboReady;
    TrapStrategy _nextTrapStrategy;
};

} // namespace Playerbot

#endif