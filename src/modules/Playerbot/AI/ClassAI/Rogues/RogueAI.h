/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef ROGUE_AI_H
#define ROGUE_AI_H

#include "../ClassAI.h"
#include "../SpellValidation_WoW112_Part2.h"
#include "Position.h"
#include "Unit.h"
#include "../Combat/BotThreatManager.h"
#include "../Combat/TargetSelector.h"
#include "../Combat/PositionManager.h"
#include "../Combat/InterruptManager.h"
#include "../Common/CooldownManager.h"
#include <memory>
#include <atomic>
#include <chrono>

namespace Playerbot
{

// Forward declarations for specialization classes (QW-4 FIX)
class AssassinationRogueRefactored;
class OutlawRogueRefactored;
class SubtletyRogueRefactored;

// Type aliases for consistency with base naming
using AssassinationRogue = AssassinationRogueRefactored;
using OutlawRogue = OutlawRogueRefactored;
using SubtletyRogue = SubtletyRogueRefactored;

// Forward declarations
struct RogueMetrics;
class RogueCombatMetrics;
class RogueCombatPositioning;

// Template-based specialization interface - no longer needed for forward declaration
// Each spec now uses its own distinct resource type (ComboPointsAssassination, etc.)

// Rogue specializations
enum class RogueSpec : uint8
{
    ASSASSINATION = 0,
    COMBAT        = 1,
    SUBTLETY      = 2
};

// Simple positioning helper class
class TC_GAME_API RogueCombatPositioning
{
public:
    explicit RogueCombatPositioning(Player* bot) : _bot(bot) {}

    Position CalculateOptimalPosition(Unit* target, RogueSpec spec);

    bool IsBehindTarget(Unit* target) const
    {
        if (!target || !_bot)
            return false;

        float targetFacing = target->GetOrientation();
        float angleToMe = target->GetAbsoluteAngle(_bot);
        float diff = ::std::abs(targetFacing - angleToMe);

        if (diff > static_cast<float>(M_PI))
            diff = 2.0f * static_cast<float>(M_PI) - diff;

        return diff < (static_cast<float>(M_PI) / 3.0f); // Within 60 degrees behind
    }

    float GetOptimalRange(RogueSpec spec) const;

private:
    Player* _bot;
};

class RogueAI : public ClassAI
{
public:
    explicit RogueAI(Player* bot);
    ~RogueAI() override;

    // Core AI Interface
    void UpdateRotation(Unit* target) override;
    void UpdateBuffs() override;
    void UpdateCooldowns(uint32 diff) override;
    bool CanUseAbility(uint32 spellId) override;
    void OnCombatStart(Unit* target) override;
    void OnCombatEnd() override;

    // Resource Management
    bool HasEnoughResource(uint32 spellId) override;
    void ConsumeResource(uint32 spellId) override;

    // Positioning
    Position GetOptimalPosition(Unit* target) override;
    float GetOptimalRange(Unit* target) override;

    // Specialization Management
private:
    // ========================================================================
    // QW-4 FIX: Per-instance specialization objects
    // Each bot has its own specialization object initialized with correct bot pointer
    // ========================================================================

    ::std::unique_ptr<AssassinationRogue> _assassinationSpec;
    ::std::unique_ptr<OutlawRogue> _outlawSpec;
    ::std::unique_ptr<SubtletyRogue> _subtletySpec;

    // Delegation to specialization
    void DelegateToSpecialization(::Unit* target);

    // Initialization
    void InitializeCombatSystems();
    // Combat execution
    void ExecuteFallbackRotation(Unit* target);
    bool ExecuteStealthOpener(Unit* target);
    bool ExecuteFinisher(Unit* target);
    bool BuildComboPoints(Unit* target);
    void ActivateBurstCooldowns(Unit* target);

    // Utility
    void ApplyPoisons();
    bool IsFinisher(uint32 spellId) const;

    // Combat systems
    ::std::unique_ptr<BotThreatManager> _threatManager;
    ::std::unique_ptr<TargetSelector> _targetSelector;
    ::std::unique_ptr<PositionManager> _positionManager;
    ::std::unique_ptr<InterruptManager> _interruptManager;
    ::std::unique_ptr<CooldownManager> _cooldownManager;

    // Rogue-specific systems
    RogueMetrics* _metrics;
    RogueCombatMetrics* _combatMetrics;
    RogueCombatPositioning* _positioning;
    // std::unique_ptr<EnergyManager> _energyManager;  // REMOVED - Legacy system

    // Combat tracking
    uint32 _energySpent;
    uint32 _comboPointsUsed;
    uint32 _stealthsUsed;
    uint32 _lastStealth;
    uint32 _lastVanish;

    // Helper methods for CombatBehaviorIntegration
    void ExecuteRogueBasicRotation(Unit* target);
    void RecordInterruptAttempt(Unit* target, uint32 spellId, bool success);
    void UseDefensiveCooldowns();
    uint32 GetNearbyEnemyCount(float range) const;
    void RecordAbilityUsage(uint32 spellId);
    void OnTargetChanged(Unit* newTarget);

    // Missing method declarations from implementation
    void ConsiderStealth();
    // ActivateBurstCooldowns already declared above
    bool HasEnoughEnergy(uint32 amount);
    uint32 GetEnergy();
    uint32 GetComboPoints();

    // Additional variables for integration tracking
    uint32 _comboPointsGenerated;
    uint32 _finishersExecuted;
    uint32 _lastPoison;

public:
    // Rogue Spell IDs - Using central registry (WoW 11.2)
    // See WoW112Spells::Rogue namespace in SpellValidation_WoW112_Part2.h
    enum RogueSpells
    {
        // ====================================================================
        // Combo Point Builders - All specs use central registry
        // ====================================================================
        SINISTER_STRIKE = WoW112Spells::Rogue::Common::SINISTER_STRIKE,
        BACKSTAB = WoW112Spells::Rogue::Common::BACKSTAB,
        MUTILATE = WoW112Spells::Rogue::Common::MUTILATE,
        SHIV = WoW112Spells::Rogue::Common::SHIV,
        AMBUSH = WoW112Spells::Rogue::Common::AMBUSH,
        GARROTE = WoW112Spells::Rogue::Common::GARROTE,
        CHEAP_SHOT = WoW112Spells::Rogue::Common::CHEAP_SHOT,
        SHADOWSTRIKE = WoW112Spells::Rogue::Common::SHADOWSTRIKE,

        // ====================================================================
        // Combo Point Finishers - Using central registry
        // ====================================================================
        SLICE_AND_DICE = WoW112Spells::Rogue::Common::SLICE_AND_DICE,
        RUPTURE = WoW112Spells::Rogue::Common::RUPTURE,
        EVISCERATE = WoW112Spells::Rogue::Common::EVISCERATE,
        KIDNEY_SHOT = WoW112Spells::Rogue::Common::KIDNEY_SHOT,
        ENVENOM = WoW112Spells::Rogue::Common::ENVENOM,
        CRIMSON_TEMPEST = WoW112Spells::Rogue::Common::CRIMSON_TEMPEST,
        BETWEEN_THE_EYES = WoW112Spells::Rogue::Common::BETWEEN_THE_EYES,

        // ====================================================================
        // Cooldowns - Using central registry
        // ====================================================================
        BLADE_FLURRY = WoW112Spells::Rogue::Common::BLADE_FLURRY,
        ADRENALINE_RUSH = WoW112Spells::Rogue::Common::ADRENALINE_RUSH,
        KILLING_SPREE = WoW112Spells::Rogue::Common::KILLING_SPREE,
        VENDETTA = WoW112Spells::Rogue::Common::VENDETTA,
        SHADOW_BLADES = WoW112Spells::Rogue::Common::SHADOW_BLADES,
        COLD_BLOOD = WoW112Spells::Rogue::Common::COLD_BLOOD,
        SHADOW_DANCE = WoW112Spells::Rogue::Common::SHADOW_DANCE,
        SHADOWSTEP = WoW112Spells::Rogue::Common::SHADOWSTEP,
        SYMBOLS_OF_DEATH = WoW112Spells::Rogue::Common::SYMBOLS_OF_DEATH,
        MARKED_FOR_DEATH = WoW112Spells::Rogue::Common::MARKED_FOR_DEATH,

        // ====================================================================
        // Utility - Using central registry
        // ====================================================================
        KICK = WoW112Spells::Rogue::Common::KICK,
        BLIND = WoW112Spells::Rogue::Common::BLIND,
        SAP = WoW112Spells::Rogue::Common::SAP,
        VANISH = WoW112Spells::Rogue::Common::VANISH,
        STEALTH = WoW112Spells::Rogue::Common::STEALTH,
        SPRINT = WoW112Spells::Rogue::Common::SPRINT,
        CLOAK_OF_SHADOWS = WoW112Spells::Rogue::Common::CLOAK_OF_SHADOWS,
        EVASION = WoW112Spells::Rogue::Common::EVASION,
        FEINT = WoW112Spells::Rogue::Common::FEINT,
        TRICKS_OF_THE_TRADE = WoW112Spells::Rogue::Common::TRICKS_OF_THE_TRADE,

        // ====================================================================
        // Poisons - Using central registry
        // ====================================================================
        DEADLY_POISON = WoW112Spells::Rogue::Common::DEADLY_POISON,
        INSTANT_POISON = WoW112Spells::Rogue::Common::INSTANT_POISON,
        WOUND_POISON = WoW112Spells::Rogue::Common::WOUND_POISON,
        NUMBING_POISON = WoW112Spells::Rogue::NUMBING_POISON,
        CRIPPLING_POISON = WoW112Spells::Rogue::Common::CRIPPLING_POISON,

        // ====================================================================
        // AoE - Using central registry
        // ====================================================================
        FAN_OF_KNIVES = WoW112Spells::Rogue::Common::FAN_OF_KNIVES,

        // ====================================================================
        // Assassination Spec - Using central registry
        // ====================================================================
        DEATHMARK = WoW112Spells::Rogue::Assassination::DEATHMARK,
        POISONED_KNIFE = WoW112Spells::Rogue::Assassination::POISONED_KNIFE,

        // ====================================================================
        // Talents - Using central registry where available
        // ====================================================================
        VIGOR = 14983, // Passive talent for energy pool (still used in 11.2)
        DEEPER_STRATAGEM = WoW112Spells::Rogue::Subtlety::DEEPER_STRATAGEM

        // ====================================================================
        // REMOVED SPELLS (Not in WoW 11.2):
        // - HEMORRHAGE (16511) - Removed from game
        // - EXPOSE_ARMOR (8647) - Removed from game
        // - DEADLY_THROW (26679) - Removed from game
        // - GOUGE (1776) - Removed from game
        // - MIND_NUMBING_POISON - Renamed to NUMBING_POISON
        // - POISON_BOMB (255544) - Internal proc, not used directly
        // ====================================================================
    };
};

} // namespace Playerbot

#endif // ROGUE_AI_H
