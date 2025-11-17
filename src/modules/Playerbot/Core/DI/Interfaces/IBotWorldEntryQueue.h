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
#include <memory>

namespace Playerbot
{

// Forward declarations
class BotWorldEntry;

/**
 * @brief Interface for bot world entry queue management
 *
 * Manages concurrent bot world entries to prevent server overload
 * with queuing and statistics tracking.
 */
class TC_GAME_API IBotWorldEntryQueue
{
public:
    struct QueueStats
    {
        uint32 queuedEntries;
        uint32 activeEntries;
        uint32 completedEntries;
        uint32 failedEntries;
        float averageEntryTime; // in seconds
    };

    virtual ~IBotWorldEntryQueue() = default;

    // Queue management
    virtual uint32 QueueEntry(::std::shared_ptr<BotWorldEntry> entry) = 0;
    virtual void ProcessQueue(uint32 maxConcurrent = 10) = 0;

    // Statistics
    virtual QueueStats GetStats() const = 0;

    // Emergency operations
    virtual void ClearQueue() = 0;
};

} // namespace Playerbot
