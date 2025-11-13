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
#include <vector>

class Player;

namespace Playerbot
{

// Forward declarations
enum class BotPriority : uint8;

/**
 * @brief Interface for bot priority management system
 *
 * Manages bot update priorities, scheduling, and performance monitoring
 * to optimize server resource utilization across thousands of concurrent bots.
 */
class TC_GAME_API IBotPriorityManager
{
public:
    virtual ~IBotPriorityManager() = default;

    // Lifecycle
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;

    // Priority management
    virtual void SetPriority(ObjectGuid botGuid, BotPriority priority) = 0;
    virtual void UpdatePriorityForBot(Player* bot, uint32 currentTime) = 0;
    virtual void AutoAdjustPriority(Player* bot, uint32 currentTime) = 0;

    // Update scheduling
    virtual bool ShouldUpdateThisTick(ObjectGuid botGuid, uint32 currentTick) const = 0;
    virtual uint32 GetUpdateInterval(BotPriority priority) const = 0;

    // Performance tracking
    virtual void RecordUpdateStart(ObjectGuid botGuid, uint32 currentTime) = 0;
    virtual void RecordUpdateEnd(ObjectGuid botGuid, uint32 durationMicros) = 0;
    virtual void RecordUpdateSkipped(ObjectGuid botGuid) = 0;
    virtual void RecordUpdateError(ObjectGuid botGuid, uint32 currentTime) = 0;

    // Statistics and monitoring
    virtual uint32 GetBotCountByPriority(BotPriority priority) const = 0;
    virtual uint32 GetEstimatedBotsThisTick(uint32 currentTick) const = 0;
    virtual void GetPriorityDistribution(uint32& emergency, uint32& high, uint32& medium, uint32& low, uint32& suspended) const = 0;

    // Load management
    virtual void SuspendLowPriorityBots(uint32 targetCount) = 0;
    virtual void ResumeSuspendedBots(uint32 targetCount) = 0;

    // Error detection
    virtual void DetectStalledBots(uint32 currentTime, uint32 stallThresholdMs) = 0;
    virtual std::vector<ObjectGuid> GetStalledBots() const = 0;

    // Bot management
    virtual void RemoveBot(ObjectGuid botGuid) = 0;
    virtual void Clear() = 0;

    // Configuration
    virtual void SetMaxBotsPerPriority(BotPriority priority, uint32 maxBots) = 0;
    virtual void SetUpdateInterval(BotPriority priority, uint32 intervalTicks) = 0;
    virtual uint32 GetMaxBotsPerPriority(BotPriority priority) const = 0;

    // Logging
    virtual void LogPriorityDistribution() const = 0;
    virtual void LogPerformanceStatistics() const = 0;
};

} // namespace Playerbot
