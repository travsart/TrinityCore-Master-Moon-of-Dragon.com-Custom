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
#include <memory>
#include <atomic>
#include <chrono>

namespace Playerbot
{

// Forward declarations
struct RogueMetrics;
class RogueCombatMetrics;
class RogueCombatPositioning;
class EnergyManager;
class BotThreatManager;
class TargetSelector;
class PositionManager;
class InterruptManager;
class CooldownManager;

enum class RogueSpec : uint8
{
    ASSASSINATION = 0,
    COMBAT        = 1,
    SUBTLETY      = 2
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

    // Combat systems
    std::unique_ptr<BotThreatManager> _threatManager;
    std::unique_ptr<TargetSelector> _targetSelector;
    std::unique_ptr<PositionManager> _positionManager;
    std::unique_ptr<InterruptManager> _interruptManager;
    std::unique_ptr<CooldownManager> _cooldownManager;

    // Rogue-specific systems
    std::unique_ptr<RogueMetrics> _metrics;
    std::unique_ptr<RogueCombatMetrics> _combatMetrics;
    std::unique_ptr<RogueCombatPositioning> _positioning;
    std::unique_ptr<EnergyManager> _energyManager;

    // Combat tracking
    uint32 _energySpent;
    uint32 _comboPointsGenerated;
    uint32 _finishersExecuted;
    uint32 _lastPoison;
    uint32 _lastStealth;
};

} // namespace Playerbot

#endif // ROGUE_AI_H