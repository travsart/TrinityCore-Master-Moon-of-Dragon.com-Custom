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
#include "../../Combat/CombatBehaviorIntegration.h"
#include "Position.h"
#include "ObjectGuid.h"
#include "PetDefines.h"
#include <memory>
#include <atomic>
#include <unordered_set>
#include <vector>
#include "GameTime.h"

class Player;
class Pet;

namespace Playerbot
{

enum class HunterSpec : uint8
{
    BEAST_MASTERY = 1,
    MARKSMANSHIP = 2,
    SURVIVAL = 3
};

// Trap information tracking
struct TrapInfo
{
    uint32 spellId;
    uint32 lastUsed;
    Position position;
    uint32 duration;

    TrapInfo(uint32 spell = 0, uint32 last = 0, Position pos = Position(), uint32 dur = 30000)
        : spellId(spell), lastUsed(last), position(pos), duration(dur) {}

    bool IsReady() const { return (GameTime::GetGameTimeMS() - lastUsed) >= 30000; } // 30sec trap cooldown
    bool IsActive() const { return (GameTime::GetGameTimeMS() - lastUsed) < duration; }
};

// Pet information tracking
struct PetInfo
{
    ObjectGuid guid;
    uint32 health;
    uint32 maxHealth;
    uint32 happiness;
    PetType type;
    uint32 lastCommand;
    uint32 lastFeed;
    bool isDead;

    PetInfo() : guid(ObjectGuid::Empty), health(0), maxHealth(0), happiness(0),
                type(PetType::MAX_PET_TYPE), lastCommand(0), lastFeed(0), isDead(true) {}

    float GetHealthPct() const
    {
        return maxHealth > 0 ? (float(health) / maxHealth * 100.0f) : 0.0f;
    }
};

// Hunter spell constants defined in HunterSpecialization.h (HunterSpells enum)

// Combat metrics structure for performance tracking
struct HunterCombatMetrics
{
    ::std::atomic<uint32> shotsLanded{0};
    ::std::atomic<uint32> shotsMissed{0};
    ::std::atomic<uint32> criticalStrikes{0};
    ::std::atomic<uint32> interrupts{0};
    ::std::atomic<uint32> trapsTriggered{0};
    ::std::atomic<uint32> petCommands{0};
    ::std::atomic<uint32> focusSpent{0};
    ::std::atomic<uint32> damageDealt{0};
    float timeAtRange{0.0f};      // Not atomic - += not supported for atomic<float>
    float timeInDeadZone{0.0f};   // Not atomic - += not supported for atomic<float>

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
    Pet* GetPet() const;
    bool HasActivePet() const;
    HunterSpec GetCurrentSpecialization() const;

    // Combat behavior integration
    CombatBehaviorIntegration* GetCombatBehaviors() const { return _combatBehaviors.get(); }

private:
    // Initialization methods
    void InitializeCombatSystems();

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
    ::std::unique_ptr<CombatBehaviorIntegration> _combatBehaviors;

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
    ::std::unordered_set<ObjectGuid> _frozenTargets;
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

public:
    // Hunter Spell IDs
    enum HunterSpells
    {
        // Shots and Attacks
        STEADY_SHOT = 56641,
        ARCANE_SHOT = 3044,
        MULTI_SHOT = 2643,
        AIMED_SHOT = 19434,
        KILL_SHOT = 53351,
        EXPLOSIVE_SHOT = 53301,
        SERPENT_STING = 1978,
        CONCUSSIVE_SHOT = 5116,

        // Pet Abilities
        KILL_COMMAND = 34026,
        MEND_PET = 136,
        REVIVE_PET = 982,
        CALL_PET = 883,
        MASTER_S_CALL = 53271,

        // Traps
        FREEZING_TRAP = 187650,
        EXPLOSIVE_TRAP = 191433,
        SNAKE_TRAP = 34600,

        // Defensive/Utility
        HUNTER_DISENGAGE = 781,
        FEIGN_DEATH = 5384,
        DETERRENCE = 19263,
        EXHILARATION = 109304,
        WING_CLIP = 2974,
        SCATTER_SHOT = 19503,
        COUNTER_SHOT = 147362,
        SILENCING_SHOT = 34490,

        // Aspects
        ASPECT_OF_THE_HAWK = 13165,
        ASPECT_OF_THE_WILD = 20043,
        ASPECT_OF_THE_CHEETAH = 5118,
        ASPECT_OF_THE_TURTLE = 186265,
        ASPECT_OF_THE_DRAGONHAWK = 61846,
        ASPECT_OF_THE_PACK = 13159,
        ASPECT_OF_THE_VIPER = 34074,

        // Marks/Debuffs
        HUNTER_S_MARK = 1130,

        // Cooldowns
        RAPID_FIRE = 3045,
        BESTIAL_WRATH = 19574,
        TRUESHOT = 288613,
        BARRAGE = 120360,
        VOLLEY = 260243,

        // Tracking Abilities (WoW 11.2)
        TRACK_BEASTS = 1494,
        TRACK_DEMONS = 19878,
        TRACK_DRAGONKIN = 19879,
        TRACK_ELEMENTALS = 19880,
        TRACK_GIANTS = 19882,
        TRACK_HUMANOIDS = 19883,
        TRACK_UNDEAD = 19884,
        TRACK_HIDDEN = 19885
    };
};

} // namespace Playerbot

#endif
