/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "Define.h"
#include "ObjectGuid.h"
#include <string>

class Player;
class Unit;
class Group;

namespace Playerbot
{

class BotAI;

// Forward declarations
enum class InterruptPriority : uint8;
enum class FallbackMethod : uint8;
struct UnifiedInterruptPlan;

/**
 * @brief Interface for unified interrupt coordination system
 *
 * Provides comprehensive interrupt management across multiple bots with
 * priority-based assignment, fallback mechanisms, and group coordination.
 */
class TC_GAME_API IUnifiedInterruptSystem
{
public:
    virtual ~IUnifiedInterruptSystem() = default;

    // System management
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual void Update(Player* bot, uint32 diff) = 0;

    // Bot registration
    virtual void RegisterBot(Player* bot, BotAI* ai) = 0;
    virtual void UnregisterBot(ObjectGuid botGuid) = 0;
    virtual void UpdateBotCapabilities(Player* bot) = 0;

    // Cast detection and tracking
    virtual void OnEnemyCastStart(Unit* caster, uint32 spellId, uint32 castTime) = 0;
    virtual void OnEnemyCastInterrupted(ObjectGuid casterGuid, uint32 spellId) = 0;
    virtual void OnEnemyCastComplete(ObjectGuid casterGuid, uint32 spellId) = 0;

    // Spell database access
    virtual InterruptPriority GetSpellPriority(uint32 spellId, uint8 mythicLevel = 0) = 0;
    virtual bool ShouldAlwaysInterrupt(uint32 spellId) = 0;

    // Interrupt execution
    virtual bool ExecuteInterruptPlan(Player* bot, UnifiedInterruptPlan const& plan) = 0;

    // Group coordination
    virtual void CoordinateGroupInterrupts(Group* group) = 0;
    virtual bool ShouldBotInterrupt(ObjectGuid botGuid, ObjectGuid& targetGuid, uint32& spellId) = 0;

    // Interrupt tracking
    virtual void OnInterruptExecuted(ObjectGuid botGuid, bool success) = 0;
    virtual void MarkInterruptUsed(ObjectGuid botGuid, uint32 spellId) = 0;

    // Fallback mechanisms
    virtual bool HandleFailedInterrupt(Player* bot, Unit* target, uint32 failedSpellId) = 0;
    virtual bool ExecuteFallback(Player* bot, Unit* target, FallbackMethod method) = 0;
    virtual bool RequestInterruptPositioning(Player* bot, Unit* target) = 0;

    // Statistics and reporting
    virtual void ResetStatistics() = 0;
    virtual ::std::string GetStatusString() const = 0;
};

} // namespace Playerbot
