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
#include <vector>
#include <thread>

namespace Playerbot
{
namespace Diagnostics
{

// Forward declarations
struct CallStackFrame;
struct ThreadState;
struct DeadlockReport;

/**
 * @brief Interface for deadlock detection and diagnostics
 *
 * Provides comprehensive deadlock detection with call stack capture,
 * thread state monitoring, mutex ownership tracking, and automatic
 * diagnostic dumps with Visual Studio integration.
 */
class TC_GAME_API IDeadlockDetector
{
public:
    virtual ~IDeadlockDetector() = default;

    // Initialization
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;

    // Thread registration
    virtual void RegisterThread(std::thread::id threadId, std::string const& name) = 0;
    virtual void UnregisterThread(std::thread::id threadId) = 0;

    // Deadlock detection
    virtual DeadlockReport DetectFutureDeadlock(
        ObjectGuid botGuid,
        uint32 futureIndex,
        uint32 totalFutures,
        uint32 waitTimeMs,
        std::thread::id waitingThreadId) = 0;

    // Call stack capture
    virtual std::vector<CallStackFrame> CaptureCallStack(uint32 skipFrames = 0, uint32 maxFrames = 64) = 0;
    virtual ThreadState CaptureThreadState(std::thread::id threadId) = 0;

    // Diagnostic output
    virtual void DumpDeadlockReport(DeadlockReport const& report, std::string const& outputFile) = 0;
    virtual void LogDeadlockReport(DeadlockReport const& report) = 0;

    // Visual Studio integration
    virtual void WriteVisualStudioBreakpointFile(DeadlockReport const& report) = 0;
    virtual void LaunchVisualStudioDebugger(DeadlockReport const& report) = 0;

    // Configuration
    virtual void SetCallStackCaptureEnabled(bool enabled) = 0;
    virtual void SetAutoLaunchDebugger(bool enabled) = 0;
    virtual void SetDumpDirectory(std::string const& dir) = 0;

    // Statistics
    virtual uint32 GetTotalDeadlocksDetected() const = 0;
    virtual std::vector<DeadlockReport> GetRecentDeadlocks(uint32 count = 10) const = 0;
};

} // namespace Diagnostics
} // namespace Playerbot
