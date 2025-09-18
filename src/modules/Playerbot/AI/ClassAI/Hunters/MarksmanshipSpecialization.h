/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef TRINITY_MARKSMANSHIPSPECIALIZATION_H
#define TRINITY_MARKSMANSHIPSPECIALIZATION_H

#include "HunterSpecialization.h"
#include <deque>

namespace Playerbot
{

// Marksmanship specific spells
enum MarksmanshipSpells : uint32
{
    // Marksmanship talents and abilities
    IMPROVED_TRACKING = 19347,
    LETHAL_SHOTS = 19238,
    CAREFUL_AIM = 34482,
    IMPROVED_HUNTER_S_MARK = 19421,
    MORTAL_SHOTS = 19485,
    RANGED_WEAPON_SPECIALIZATION = 19507,
    TRUESHOT = 19506,
    BARRAGE = 35100,
    MASTER_MARKSMAN = 34485,
    PIERCING_SHOTS = 53234,
    MARKED_FOR_DEATH = 53241,
    IMPROVED_STEADY_SHOT = 53221,
    HUNTER_VS_WILD = 56339,
    WILD_QUIVER = 53215,

    // Marksmanship specific shots
    AIMED_SHOT_RANK_1 = 19434,
    AIMED_SHOT_RANK_9 = 49050,
    EXPLOSIVE_SHOT_RANK_1 = 60053,
    EXPLOSIVE_SHOT_RANK_4 = 60052,
    SILENCING_SHOT_RANK_1 = 34490,
    PIERCING_SHOTS_EFFECT = 63468,
    WILD_QUIVER_EFFECT = 53220,
    IMPROVED_STEADY_SHOT_EFFECT = 53224
};

// Shot priority system for optimal DPS
enum class ShotType : uint8
{
    KILL_SHOT = 0,      // Highest priority - execute
    AIMED_SHOT = 1,     // High damage, cast time
    CHIMERA_SHOT = 2,   // Instant high damage
    ARCANE_SHOT = 3,    // Instant moderate damage
    STEADY_SHOT = 4,    // Filler, mana efficient
    MULTI_SHOT = 5,     // AoE
    AUTO_SHOT = 6       // Automatic
};

// Marksmanship rotation phases
enum class MMRotationPhase : uint8
{
    OPENING = 0,         // Initial shot sequence
    AIMED_SHOT_CYCLE = 1, // Aimed Shot focused rotation
    STEADY_SHOT_SPAM = 2, // Mana efficient DPS
    EXECUTE_PHASE = 3,    // Kill Shot spam
    AOE_PHASE = 4,        // Multi-Shot focused
    BURST_PHASE = 5,      // Cooldown usage
    MANA_CONSERVATION = 6 // Low mana management
};

// Shot weaving system for optimal DPS
struct ShotWeave
{
    ShotType primaryShot;
    ShotType fillerShot;
    uint32 weaveCount;
    uint32 totalDuration;

    ShotWeave(ShotType primary = ShotType::STEADY_SHOT, ShotType filler = ShotType::AUTO_SHOT,
              uint32 count = 1, uint32 duration = 3000)
        : primaryShot(primary), fillerShot(filler), weaveCount(count), totalDuration(duration) {}
};

// Shot tracking for rotation optimization
struct ShotInfo
{
    uint32 spellId;
    uint32 lastCast;
    uint32 castTime;
    uint32 cooldown;
    uint32 manaCost;
    uint32 damage;
    bool isInstant;

    ShotInfo(uint32 spell = 0, uint32 last = 0, uint32 cast = 0, uint32 cd = 0,
             uint32 mana = 0, uint32 dmg = 0, bool instant = false)
        : spellId(spell), lastCast(last), castTime(cast), cooldown(cd),
          manaCost(mana), damage(dmg), isInstant(instant) {}

    bool IsReady() const { return (getMSTime() - lastCast) >= cooldown; }
    bool IsChanneling() const { return !isInstant && (getMSTime() - lastCast) < castTime; }
};

// Marksmanship combat stance
enum class CombatStance : uint8
{
    AGGRESSIVE = 0,      // Maximum DPS
    CONSERVATIVE = 1,    // Mana management
    BURST = 2,          // Cooldown usage
    DEFENSIVE = 3,      // Survival focused
    AOE = 4,            // Multi-target
    EXECUTE = 5         // Low health targets
};

class MarksmanshipSpecialization : public HunterSpecialization
{
public:
    explicit MarksmanshipSpecialization(Player* bot);
    ~MarksmanshipSpecialization() override = default;

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

    // Pet management interface (minimal for Marksmanship)
    void UpdatePetManagement() override;
    void SummonPet() override;
    void CommandPetAttack(::Unit* target) override;
    void CommandPetFollow() override;
    void CommandPetStay() override;
    void MendPetIfNeeded() override;
    void FeedPetIfNeeded() override;
    bool HasActivePet() const override;
    PetInfo GetPetInfo() const override;

    // Trap management interface
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
    // Marksmanship specific rotation methods
    bool ExecuteOpeningRotation(::Unit* target);
    bool ExecuteAimedShotCycle(::Unit* target);
    bool ExecuteSteadyShotSpam(::Unit* target);
    bool ExecuteExecutePhase(::Unit* target);
    bool ExecuteAoePhase(::Unit* target);
    bool ExecuteBurstPhase(::Unit* target);
    bool ExecuteManaConservation(::Unit* target);

    // Shot weaving and optimization
    void UpdateShotWeaving();
    void OptimizeShotRotation(::Unit* target);
    ShotWeave CalculateOptimalWeave(::Unit* target);
    bool ShouldInterruptWeave() const;
    void ExecuteShotWeave(::Unit* target, const ShotWeave& weave);

    // Shot priority and decision making
    ShotType GetHighestPriorityShot(::Unit* target) const;
    bool ShouldUseAimedShot(::Unit* target) const;
    bool ShouldUseSteadyShot(::Unit* target) const;
    bool ShouldUseChimeraShot(::Unit* target) const;
    bool ShouldUseArcaneShot(::Unit* target) const;
    bool ShouldUseMultiShot(::Unit* target) const;
    bool ShouldUseKillShot(::Unit* target) const;
    bool ShouldUseSilencingShot(::Unit* target) const;

    // Shot casting methods
    void CastAimedShot(::Unit* target);
    void CastSteadyShot(::Unit* target);
    void CastChimeraShot(::Unit* target);
    void CastArcaneShot(::Unit* target);
    void CastMultiShot(::Unit* target);
    void CastKillShot(::Unit* target);
    void CastSilencingShot(::Unit* target);

    // Cooldown and buff management
    void UpdateCooldownUsage();
    bool ShouldUseRapidFire() const;
    bool ShouldUseReadiness() const;
    void CastRapidFire();
    void CastReadiness();
    void UpdateTrueshotAura();

    // Mana and resource management
    void UpdateManaManagement();
    void OptimizeManaUsage();
    bool ShouldEnterManaConservationMode() const;
    bool ShouldExitManaConservationMode() const;
    bool ShouldUseAspectOfTheViper() const;
    void HandleLowMana();

    // Combat stance and adaptation
    void UpdateCombatStance();
    void AdaptToSituation(::Unit* target);
    CombatStance DetermineBestStance(::Unit* target) const;
    void TransitionToStance(CombatStance newStance);

    // Auto-shot management
    void UpdateAutoShotTiming();
    bool ShouldClipAutoShot() const;
    uint32 GetAutoShotRemainingTime() const;
    void OptimizeAutoShotWeaving(::Unit* target);

    // Multi-target and cleave optimization
    void UpdateMultiTargetAnalysis();
    bool ShouldFocusOnAoe() const;
    void HandleMultiTargetPrioritization();
    ::Unit* SelectOptimalTarget() const;

    // Utility and positioning
    void HandleInterrupts(::Unit* target);
    void HandleCrowdControl();
    void UpdatePositionalAdvantage();
    bool ShouldMaintainMaxRange() const;

    // Marksmanship specific state
    MMRotationPhase _mmRotationPhase;
    CombatStance _currentStance;
    ShotWeave _currentWeave;
    std::deque<ShotInfo> _shotHistory;

    // Timing and optimization
    uint32 _lastAutoShotStart;
    uint32 _autoShotDuration;
    uint32 _nextAutoShotTime;
    uint32 _lastShotWeaveUpdate;
    uint32 _lastStanceUpdate;
    uint32 _lastManaCheck;
    uint32 _lastCooldownCheck;
    uint32 _lastTargetAnalysis;

    // Cooldown tracking
    uint32 _rapidFireReady;
    uint32 _readinessReady;
    uint32 _silencingShotReady;
    uint32 _lastRapidFire;
    uint32 _lastReadiness;
    uint32 _lastSilencingShot;

    // Combat metrics and optimization
    uint32 _totalShotsFired;
    uint32 _aimedShotsHit;
    uint32 _steadyShotsHit;
    uint32 _chimeraShots;
    uint32 _killShots;
    uint32 _multiShotsUsed;
    uint32 _autoShotsHit;
    float _averageShotDamage;
    float _dpsLastInterval;
    uint32 _manaEfficiency;

    // Multi-target tracking
    uint32 _nearbyTargetCount;
    uint32 _priorityTargetCount;
    std::vector<ObjectGuid> _multiTargetList;
    ObjectGuid _primaryTarget;
    ObjectGuid _secondaryTarget;

    // State flags
    bool _inBurstMode;
    bool _conservingMana;
    bool _castingAimedShot;
    bool _rapidFireActive;
    bool _trueshotAuraActive;
    bool _improvedSteadyShotActive;
    bool _piercingShotsActive;
    bool _markedForDeathActive;

    // Shot timing optimization
    uint32 _globalCooldownEnd;
    uint32 _aimedShotCastStart;
    uint32 _aimedShotCastEnd;
    uint32 _steadyShotCastTime;
    uint32 _optimalShotInterval;

    // Advanced positioning
    Position _lastOptimalPosition;
    uint32 _lastPositionUpdate;
    bool _needsRepositioning;
    float _currentRange;
    float _targetRange;
};

} // namespace Playerbot

#endif