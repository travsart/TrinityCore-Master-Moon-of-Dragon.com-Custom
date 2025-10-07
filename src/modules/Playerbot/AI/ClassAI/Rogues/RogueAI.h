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
#include "RogueSpecialization.h"
#include "EnergyManager.h"
#include "Position.h"
#include "Unit.h"
#include "../Combat/BotThreatManager.h"
#include "../Combat/TargetSelector.h"
#include "../Combat/PositionManager.h"
#include "../Combat/InterruptManager.h"
#include "../CooldownManager.h"
#include <memory>
#include <atomic>
#include <chrono>

namespace Playerbot
{

// Forward declarations
struct RogueMetrics;
class RogueCombatMetrics;
class RogueCombatPositioning;
class AssassinationRogueRefactored;
class OutlawRogueRefactored;
class SubtletyRogueRefactored;

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
        float diff = std::abs(targetFacing - angleToMe);

        if (diff > M_PI)
            diff = 2 * M_PI - diff;

        return diff < (M_PI / 3); // Within 60 degrees behind
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
    RogueSpec GetCurrentSpecialization() const;

private:
    // Initialization
    void InitializeCombatSystems();
    void DetectSpecialization();
    void InitializeSpecialization();

    // Combat execution
    void ExecuteFallbackRotation(Unit* target);
    bool ExecuteStealthOpener(Unit* target);
    bool ExecuteFinisher(Unit* target);
    bool BuildComboPoints(Unit* target);
    void ActivateBurstCooldowns(Unit* target);

    // Utility
    void ApplyPoisons();
    bool IsFinisher(uint32 spellId) const;

    // Specialization system
    std::unique_ptr<RogueSpecialization> _specialization;
    RogueSpec _detectedSpec;
    void SwitchSpecialization(RogueSpec newSpec);
    void DelegateToSpecialization(::Unit* target);

    // Combat systems
    std::unique_ptr<BotThreatManager> _threatManager;
    std::unique_ptr<TargetSelector> _targetSelector;
    std::unique_ptr<PositionManager> _positionManager;
    std::unique_ptr<InterruptManager> _interruptManager;
    std::unique_ptr<CooldownManager> _cooldownManager;

    // Rogue-specific systems
    RogueMetrics* _metrics;
    RogueCombatMetrics* _combatMetrics;
    RogueCombatPositioning* _positioning;
    std::unique_ptr<EnergyManager> _energyManager;

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

    // Member variable for positioning
    std::unique_ptr<RogueCombatPositioning> _combatPositioning;
};

} // namespace Playerbot

#endif // ROGUE_AI_H