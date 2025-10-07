/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef TRINITY_HUNTERPLAYERAI_H
#define TRINITY_HUNTERPLAYERAI_H

#include "../ClassAI.h"
#include "HunterSpecialization.h"
#include <memory>
#include <atomic>
#include <unordered_set>

class Player;
class Pet;

namespace Playerbot
{

// Forward declarations
class CombatBehaviorIntegration;
class BeastMasterySpecialization;
class MarksmanshipSpecialization;
class SurvivalSpecialization;

enum class HunterSpec : uint8
{
    BEAST_MASTERY = 1,
    MARKSMANSHIP = 2,
    SURVIVAL = 3
};

// Hunter spell constants (shared across specs)
enum HunterCombatSpells : uint32
{
    // Core Hunter abilities
    HUNTER_S_MARK         = 1130,
    ARCANE_SHOT           = 3044,
    SERPENT_STING         = 1978,
    STEADY_SHOT           = 56641,
    KILL_SHOT             = 53351,
    HUNTER_DISENGAGE      = 781,
    HUNTER_DETERRENCE     = 19263,
    FEIGN_DEATH           = 5384,

    // Interrupts
    COUNTER_SHOT          = 147362,
    SILENCING_SHOT        = 34490,

    // Crowd Control
    FREEZING_TRAP         = 1499,
    ICE_TRAP              = 13809,
    SNAKE_TRAP            = 34600,
    SCATTER_SHOT          = 19503,
    CONCUSSIVE_SHOT       = 5116,
    WING_CLIP             = 2974,

    // AoE abilities
    MULTI_SHOT            = 2643,
    VOLLEY                = 1510,
    EXPLOSIVE_SHOT        = 60053,
    BARRAGE               = 120360,

    // Cooldowns
    BESTIAL_WRATH         = 19574,
    RAPID_FIRE            = 3045,
    TRUESHOT              = 288613,
    ASPECT_OF_THE_WILD    = 193530,

    // Aspects
    ASPECT_OF_THE_HAWK    = 13165,
    ASPECT_OF_THE_MONKEY  = 13163,
    ASPECT_OF_THE_CHEETAH = 5118,
    ASPECT_OF_THE_PACK    = 13159,
    ASPECT_OF_THE_VIPER   = 34074,
    ASPECT_OF_THE_DRAGONHAWK = 61846,
    ASPECT_OF_THE_TURTLE  = 186265,

    // Pet management
    CALL_PET              = 883,
    DISMISS_PET           = 2641,
    MEND_PET              = 136,
    REVIVE_PET            = 982,
    PET_ATTACK            = 52398,
    PET_FOLLOW            = 52399,
    PET_STAY              = 52400,
    KILL_COMMAND          = 34026,
    MASTER_S_CALL         = 53271,

    // Defensive
    EXHILARATION          = 109304,
    ASPECT_OF_THE_TURTLE  = 186265,
    TURTLE                = 186265 // Alias
};

// Combat metrics structure for performance tracking
struct HunterCombatMetrics
{
    std::atomic<uint32> shotsLanded{0};
    std::atomic<uint32> shotsMissed{0};
    std::atomic<uint32> criticalStrikes{0};
    std::atomic<uint32> interrupts{0};
    std::atomic<uint32> trapsTriggered{0};
    std::atomic<uint32> petCommands{0};
    std::atomic<uint32> focusSpent{0};
    std::atomic<uint32> damageDealt{0};
    std::atomic<float> timeAtRange{0.0f};
    std::atomic<float> timeInDeadZone{0.0f};

    void Reset()
    {
        shotsLanded = 0;
        shotsMissed = 0;
        criticalStrikes = 0;
        interrupts = 0;
        trapsTriggered = 0;
        petCommands = 0;
        focusSpent = 0;
        damageDealt = 0;
        timeAtRange = 0.0f;
        timeInDeadZone = 0.0f;
    }
};

class HunterAI : public ClassAI
{
public:
    explicit HunterAI(Player* bot);
    ~HunterAI() override = default;

    // Core AI interface
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

    // Hunter-specific methods
    HunterSpec GetCurrentSpecialization() const;
    Pet* GetPet() const;
    bool HasActivePet() const;

    // Combat behavior integration
    CombatBehaviorIntegration* GetCombatBehaviors() const { return _combatBehaviors.get(); }

private:
    // Initialization methods
    void InitializeCombatSystems();
    void DetectSpecialization();
    void InitializeSpecialization();
    void SwitchSpecialization(HunterSpec newSpec);
    void DelegateToSpecialization(::Unit* target);

    // Combat priority handlers (9 priorities from integration)
    bool HandleInterrupts(::Unit* target);
    bool HandleDefensives(::Unit* target);
    bool HandlePositioning(::Unit* target);
    bool HandlePetManagement(::Unit* target);
    bool HandleTargetSwitching(::Unit* target);
    bool HandleCrowdControl(::Unit* target);
    bool HandleAoEDecisions(::Unit* target);
    bool HandleOffensiveCooldowns(::Unit* target);
    void ExecuteNormalRotation(::Unit* target);

    // Pet management system
    void UpdatePetStatus();
    bool NeedsPetRevive() const;
    bool NeedsPetHeal() const;
    bool ShouldDismissPet() const;
    void CommandPetAttack(::Unit* target);
    void CommandPetFollow();
    void CommandPetStay();
    bool IsPetInCombat() const;
    float GetPetHealthPercent() const;
    void HealPet();
    void RevivePet();
    void CallPet();

    // Trap management
    bool CanPlaceTrap() const;
    bool ShouldPlaceFreezingTrap(::Unit* target) const;
    bool ShouldPlaceExplosiveTrap() const;
    bool ShouldPlaceSnakeTrap() const;
    void PlaceTrap(uint32 trapSpell, const Position& pos);
    uint32 GetBestTrapForSituation() const;

    // Range management
    bool IsInOptimalRange(::Unit* target) const;
    bool IsInDeadZone(::Unit* target) const;
    bool NeedsToKite(::Unit* target) const;
    void MaintainRange(::Unit* target);
    float GetDistanceToTarget(::Unit* target) const;

    // Hunter-specific mechanics
    void ManageAspects();
    void UpdateTracking();
    bool HasAnyAspect(); // Not const - calls non-const HasAura()
    uint32 GetCurrentAspect(); // Not const - calls non-const HasAura()
    void SwitchToCombatAspect();
    void SwitchToMovementAspect();
    bool ValidateAspectForAbility(uint32 spellId) const;
    uint32 GetOptimalAspect() const;

    // Utility methods
    bool ShouldFeignDeath() const;
    bool CanInterruptTarget(::Unit* target) const;
    ::Unit* GetBestCrowdControlTarget() const;
    uint32 GetNearbyEnemyCount(float range) const;
    bool HasFocus(uint32 amount) const;
    uint32 GetFocus() const;
    uint32 GetMaxFocus() const;
    float GetFocusPercent() const;
    void LogCombatMetrics();

    // Helper methods
    Player* GetMainTank(); // Not const - calls GetClass() on group members
    bool IsTargetDangerous(::Unit* target) const;
    bool ShouldSaveDefensives() const;
    void RecordShotResult(bool hit, bool crit);
    void RecordTrapPlacement(uint32 trapSpell);

    // Member variables
    HunterSpec _detectedSpec;
    std::unique_ptr<HunterSpecialization> _specialization;
    std::unique_ptr<CombatBehaviorIntegration> _combatBehaviors;

    // Combat state tracking
    HunterCombatMetrics _combatMetrics;
    uint32 _lastCounterShot;
    uint32 _lastFeignDeath;
    uint32 _lastDeterrence;
    uint32 _lastDisengage;
    uint32 _lastTrapPlacement;
    uint32 _lastPetCommand;
    uint32 _lastAspectSwitch;
    uint32 _lastPetRevive;
    uint32 _lastPetHeal;

    // Pet state
    ObjectGuid _petGuid;
    bool _petNeedsHeal;
    bool _petIsAggressive;
    uint32 _petTargetSwitch;

    // Trap management
    std::unordered_set<ObjectGuid> _frozenTargets;
    Position _lastTrapPosition;
    uint32 _activeTrapType;

    // Performance tracking
    uint32 _updateCounter;
    uint32 _totalUpdateTime;
    uint32 _peakUpdateTime;

    // Constants
    static constexpr float OPTIMAL_RANGE_MIN = 8.0f;   // Outside dead zone
    static constexpr float OPTIMAL_RANGE_MAX = 35.0f;  // Max effective range
    static constexpr float OPTIMAL_RANGE_PREFERRED = 25.0f; // Preferred combat range
    static constexpr float DEAD_ZONE_MIN = 0.0f;       // Melee range
    static constexpr float DEAD_ZONE_MAX = 8.0f;       // Can't shoot in this range
    static constexpr float KITING_RANGE = 30.0f;       // Range when kiting
    static constexpr float TRAP_PLACEMENT_RANGE = 30.0f;
    static constexpr uint32 PET_HEAL_THRESHOLD = 50;   // Heal pet below 50% HP
    static constexpr uint32 DEFENSIVE_HEALTH_THRESHOLD = 30; // Use defensives below 30% HP
    static constexpr uint32 FEIGN_DEATH_THRESHOLD = 20; // Feign death below 20% HP
};

} // namespace Playerbot

#endif