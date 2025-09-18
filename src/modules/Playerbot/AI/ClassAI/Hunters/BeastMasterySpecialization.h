/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef TRINITY_BEASTMASTERYSPECIALIZATION_H
#define TRINITY_BEASTMASTERYSPECIALIZATION_H

#include "HunterSpecialization.h"
#include <unordered_set>

namespace Playerbot
{

// Beast Mastery specific spells
enum BeastMasterySpells : uint32
{
    // Beast Mastery talents
    SPIRIT_BOND = 19578,
    BESTIAL_DISCIPLINE = 19590,
    UNLEASHED_FURY = 19616,
    FEROCIOUS_INSPIRATION = 34455,
    CATLIKE_REFLEXES = 34462,
    SERPENTS_SWIFTNESS = 34466,
    LONGEVITY = 53262,
    COBRA_STRIKES = 53257,
    KINDRED_SPIRITS = 56314,
    BEAST_MASTERY = 53270,

    // Beast Mastery abilities
    TRUESHOT_AURA = 19506,
    SILENCING_SHOT = 34490,
    MASTER_S_CALL = 53271,

    // Pet specific
    BESTIAL_WRATH_PET = 19574,
    ENRAGE_PET = 19574,
    DASH_PET = 23099,
    DIVE_PET = 23145,
    FURIOUS_HOWL = 24604,
    CALL_OF_THE_WILD = 53434
};

// Beast Mastery rotation phases
enum class BMRotationPhase : uint8
{
    OPENING = 0,        // Pet summoning and setup
    BURST_WITH_PET = 1, // Bestial Wrath burst phase
    STEADY_DPS = 2,     // Standard rotation
    PET_FOCUSED = 3,    // Pet is primary DPS
    RANGED_SUPPORT = 4, // Supporting pet from range
    EMERGENCY = 5       // Emergency situations
};

// Pet commands and strategies
enum class PetStrategy : uint8
{
    AGGRESSIVE = 0,     // Full DPS mode
    DEFENSIVE = 1,      // Protect hunter
    PASSIVE = 2,        // No action
    ASSIST = 3,         // Attack hunter's target
    TANK = 4           // Pet tanking mode
};

class BeastMasterySpecialization : public HunterSpecialization
{
public:
    explicit BeastMasterySpecialization(Player* bot);
    ~BeastMasterySpecialization() override = default;

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

    // Pet management interface
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
    // Beast Mastery specific rotation methods
    bool ExecuteOpeningRotation(::Unit* target);
    bool ExecuteBurstRotation(::Unit* target);
    bool ExecuteSteadyDpsRotation(::Unit* target);
    bool ExecutePetFocusedRotation(::Unit* target);
    bool ExecuteRangedSupportRotation(::Unit* target);
    bool ExecuteEmergencyRotation(::Unit* target);

    // Pet management methods
    void UpdateAdvancedPetManagement();
    void OptimizePetBehavior(::Unit* target);
    void HandlePetBurstPhase();
    void HandlePetEmergency();
    void CommandPetSpecialAbilities(::Unit* target);
    void ManagePetHappiness();
    void ManagePetHealth();
    void HandlePetRevive();
    bool ShouldUseBestialWrath() const;
    bool ShouldUseIntimidation(::Unit* target) const;
    bool ShouldUseCallOfTheWild() const;

    // Shot rotation methods
    bool ShouldUseSteadyShot(::Unit* target) const;
    bool ShouldUseArcaneShot(::Unit* target) const;
    bool ShouldUseMultiShot(::Unit* target) const;
    bool ShouldUseKillShot(::Unit* target) const;
    bool ShouldUseConcussiveShot(::Unit* target) const;
    void CastSteadyShot(::Unit* target);
    void CastArcaneShot(::Unit* target);
    void CastMultiShot(::Unit* target);
    void CastKillShot(::Unit* target);
    void CastConcussiveShot(::Unit* target);

    // Beast Mastery specific abilities
    void CastBestialWrath();
    void CastIntimidation(::Unit* target);
    void CastCallOfTheWild();
    void CastSilencingShot(::Unit* target);
    void CastMastersCall();

    // Utility methods
    void HandleTargetSwitching();
    void UpdateBurstPhase();
    void UpdateCombatPhase();
    bool ShouldFocusOnPetDPS() const;
    bool ShouldUseCooldowns() const;
    void HandleMultipleTargets();
    float CalculateOptimalPetPosition(::Unit* target);

    // Beast Mastery specific state
    BMRotationPhase _bmRotationPhase;
    PetStrategy _currentPetStrategy;
    uint32 _bestialWrathReady;
    uint32 _intimidationReady;
    uint32 _callOfTheWildReady;
    uint32 _silencingShotReady;
    uint32 _mastersCallReady;
    uint32 _lastBestialWrath;
    uint32 _lastIntimidation;
    uint32 _lastCallOfTheWild;
    uint32 _lastSilencingShot;
    uint32 _lastMastersCall;
    uint32 _lastPetFeed;
    uint32 _lastPetHappinessCheck;
    uint32 _lastPetCommand;
    uint32 _lastBurstCheck;
    uint32 _petReviveAttempts;
    bool _petInBurstMode;
    bool _emergencyModeActive;
    uint32 _multiTargetCount;
    std::unordered_set<ObjectGuid> _multiTargets;
    uint32 _totalPetDamage;
    uint32 _totalHunterDamage;
    float _petDpsRatio;
    uint32 _steadyShotCount;
    uint32 _arcaneShotCount;
    uint32 _killShotCount;
    Position _lastKnownPetPosition;
    uint32 _petPositionUpdateTime;
};

} // namespace Playerbot

#endif