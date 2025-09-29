/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef DEATHKNIGHT_AI_H
#define DEATHKNIGHT_AI_H

#include "../ClassAI.h"
#include "DeathKnightSpecialization.h"
#include <memory>
#include <atomic>
#include <chrono>

namespace Playerbot
{

// Forward declarations
struct DeathKnightMetrics;
class DeathKnightCombatMetrics;
class DeathKnightCombatPositioning;
class RuneManager;
class DiseaseManager;
class BotThreatManager;
class TargetSelector;
class PositionManager;
class InterruptManager;
class CooldownManager;


class DeathKnightAI : public ClassAI
{
public:
    explicit DeathKnightAI(Player* bot);
    ~DeathKnightAI() override;

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
    DeathKnightSpec GetCurrentSpecialization() const;

private:
    // Initialization
    void InitializeCombatSystems();
    void DetectSpecialization();
    void InitializeSpecialization();

    // Combat execution
    void ExecuteFallbackRotation(Unit* target);
    void ActivateBurstCooldowns(Unit* target);

    // Specialization system
    std::unique_ptr<DeathKnightSpecialization> _specialization;
    DeathKnightSpec _detectedSpec;

    // Combat systems
    std::unique_ptr<BotThreatManager> _threatManager;
    std::unique_ptr<TargetSelector> _targetSelector;
    std::unique_ptr<PositionManager> _positionManager;
    std::unique_ptr<InterruptManager> _interruptManager;
    std::unique_ptr<CooldownManager> _cooldownManager;

    // Death Knight-specific systems
    std::unique_ptr<DeathKnightMetrics> _metrics;
    std::unique_ptr<DeathKnightCombatMetrics> _combatMetrics;
    std::unique_ptr<DeathKnightCombatPositioning> _positioning;
    std::unique_ptr<RuneManager> _runeManager;
    std::unique_ptr<DiseaseManager> _diseaseManager;

    // Combat tracking
    uint32 _runicPowerSpent;
    uint32 _runesUsed;
    uint32 _diseasesApplied;
    uint32 _lastPresence;
    uint32 _lastHorn;
};

} // namespace Playerbot

#endif // DEATHKNIGHT_AI_H