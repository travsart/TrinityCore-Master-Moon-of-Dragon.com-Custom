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
#include <cstdint>

class WorldSession;

namespace Playerbot
{

class BotSession;

/**
 * @class BotPacketSimulator
 * @brief Simulates client→server packets that bots don't naturally send
 *
 * PROBLEM: Real players send acknowledgement packets during login/world entry
 * that drive critical server-side state transitions (time sync, object visibility,
 * movement initialization). Bots don't have network clients to send these packets,
 * causing various issues that previously required manual workarounds.
 *
 * SOLUTION: Forge these critical packets by calling the appropriate TrinityCore
 * packet handlers directly, making bot login indistinguishable from real players.
 *
 * Critical Packets Simulated:
 * 1. CMSG_QUEUED_MESSAGES_END - Resume communication after async DB load
 *    - Triggers time synchronization
 *    - Required for login flow to proceed
 *    - Handler: WorldSession::HandleQueuedMessagesEnd()
 *
 * 2. CMSG_MOVE_INIT_ACTIVE_MOVER_COMPLETE - Acknowledge active mover initialization
 *    - Sets PLAYER_LOCAL_FLAG_OVERRIDE_TRANSPORT_SERVER_TIME automatically
 *    - Enables object visibility (fixes CanNeverSee() check)
 *    - Updates player visibility
 *    - Handler: WorldSession::HandleMoveInitActiveMoverComplete()
 *
 * 3. CMSG_TIME_SYNC_RESPONSE - Periodic time synchronization
 *    - Maintains clock delta calculations
 *    - Prevents movement prediction drift
 *    - Handler: WorldSession::HandleTimeSyncResponse()
 *
 * BENEFITS:
 * - Eliminates manual flag workarounds in bot login code
 * - Matches real player login flow exactly
 * - Automatic phase assignment via TrinityCore APIs
 * - Better long-term stability and maintainability
 */
class TC_GAME_API BotPacketSimulator
{
public:
    explicit BotPacketSimulator(BotSession* session);
    ~BotPacketSimulator() = default;

    BotPacketSimulator(BotPacketSimulator const&) = delete;
    BotPacketSimulator& operator=(BotPacketSimulator const&) = delete;

    /**
     * Simulate CMSG_QUEUED_MESSAGES_END packet
     * Called after SendInitialPacketsBeforeAddToMap() to resume communication
     * Triggers time synchronization and allows login to proceed
     */
    void SimulateQueuedMessagesEnd();

    /**
     * Simulate CMSG_MOVE_INIT_ACTIVE_MOVER_COMPLETE packet
     * Called after SendInitialPacketsAfterAddToMap() to complete mover initialization
     * Automatically sets PLAYER_LOCAL_FLAG_OVERRIDE_TRANSPORT_SERVER_TIME
     * Updates player visibility for object detection
     */
    void SimulateMoveInitActiveMoverComplete();

    /**
     * Simulate CMSG_TIME_SYNC_RESPONSE packet
     * Called periodically to maintain time synchronization
     * Prevents movement prediction drift over time
     */
    void SimulateTimeSyncResponse(uint32 counter);

    /**
     * Enable periodic time sync responses (every 10 seconds)
     * Called after bot successfully enters world
     */
    void EnablePeriodicTimeSync();

    /**
     * Disable periodic time sync (called during logout/cleanup)
     */
    void DisablePeriodicTimeSync();

    /**
     * Update method called by BotSession::Update()
     * Handles periodic time sync if enabled
     */
    void Update(uint32 diff);

private:
    BotSession* _session;                           ///< Owning bot session
    uint32 _simulatedClientTime = 0;                ///< Fake client timestamp (incremented each sync)
    bool _periodicTimeSyncEnabled = false;          ///< Whether periodic sync is active
    uint32 _timeSyncCounter = 0;                    ///< Accumulated time for periodic sync
    static constexpr uint32 TIME_SYNC_INTERVAL = 10000; ///< 10 seconds between syncs
};

} // namespace Playerbot
