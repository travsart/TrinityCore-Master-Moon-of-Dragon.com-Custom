/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "BotPacketSimulator.h"
#include "BotSession.h"
#include "WorldSession.h"
#include "Player.h"
#include "Log.h"
#include "Timer.h"
#include "WorldPacket.h"
#include "Opcodes.h"

// TrinityCore packet structures
#include "AuthenticationPackets.h"
#include "MovementPackets.h"
#include "MiscPackets.h"

namespace Playerbot
{

BotPacketSimulator::BotPacketSimulator(BotSession* session)
    : _session(session)
{
    // Initialize simulated client time with current server time
    _simulatedClientTime = GameTime::GetGameTimeMS();
}

void BotPacketSimulator::SimulateQueuedMessagesEnd()
{
    if (!_session)
    {
        TC_LOG_ERROR("module.playerbot.packet", "BotPacketSimulator: null session in SimulateQueuedMessagesEnd");
        return;
    }

    Player* bot = _session->GetPlayer();
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return nullptr;
    }
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return;
    }
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return;
        }
    if (!bot)
    {
        TC_LOG_ERROR("module.playerbot.packet", "BotPacketSimulator: null player in SimulateQueuedMessagesEnd");
        return;
    }

    // CRITICAL: This packet is sent by real clients after receiving SMSG_RESUME_COMMS
    // It triggers time synchronization via HandleTimeSync with SPECIAL_RESUME_COMMS_TIME_SYNC_COUNTER
    // See: MovementHandler.cpp:838-841
    TC_LOG_DEBUG("module.playerbot.packet",
        "Bot {} simulating CMSG_QUEUED_MESSAGES_END (timestamp: {})",
        bot->GetName(), _simulatedClientTime);

    // Create WorldPacket with CMSG_QUEUED_MESSAGES_END opcode
    WorldPacket packet(CMSG_QUEUED_MESSAGES_END);
    packet << _simulatedClientTime;  // Write timestamp

if (!bot)

{

    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");

    return nullptr;

}

    // Construct packet structure from WorldPacket
    WorldPackets::Auth::QueuedMessagesEnd queuedMessagesEnd(std::move(packet));
    queuedMessagesEnd.Read();  // Extract data from WorldPacket

    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return nullptr;
    }
    // Call the handler directly (packet forging)
    _session->HandleQueuedMessagesEnd(queuedMessagesEnd);

    TC_LOG_INFO("module.playerbot.packet",
        "✅ Bot {} successfully simulated CMSG_QUEUED_MESSAGES_END - time sync initialized",
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return;
        }
        bot->GetName());
}

void BotPacketSimulator::SimulateMoveInitActiveMoverComplete()
{
    if (!_session)
    {
        TC_LOG_ERROR("module.playerbot.packet", "BotPacketSimulator: null session in SimulateMoveInitActiveMoverComplete");
        return;
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return;
    }
    }

    Player* bot = _session->GetPlayer();
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return;
        }
    if (!bot)
    {
        TC_LOG_ERROR("module.playerbot.packet", "BotPacketSimulator: null player in SimulateMoveInitActiveMoverComplete");
        return;
    }

    // CRITICAL: This packet is sent by real clients after receiving SMSG_MOVE_INIT_ACTIVE_MOVER
    // It triggers:
    // 1. Time synchronization via HandleTimeSync with SPECIAL_INIT_ACTIVE_MOVER_TIME_SYNC_COUNTER
    // 2. Sets PLAYER_LOCAL_FLAG_OVERRIDE_TRANSPORT_SERVER_TIME automatically (MovementHandler.cpp:886)
    // 3. Updates player visibility via UpdateObjectVisibility()
    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsInWorld");
        return nullptr;
    }
    //
    // Without this packet, Player::CanNeverSee() returns TRUE because the flag is missing,
    // making bots unable to see any objects in the world.
    // See: MovementHandler.cpp:843-848
    TC_LOG_DEBUG("module.playerbot.packet",
        "Bot {} simulating CMSG_MOVE_INIT_ACTIVE_MOVER_COMPLETE (ticks: {})",
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return;
        }
        bot->GetName(), _simulatedClientTime);

    // Create WorldPacket with CMSG_MOVE_INIT_ACTIVE_MOVER_COMPLETE opcode
    WorldPacket packet(CMSG_MOVE_INIT_ACTIVE_MOVER_COMPLETE);
    packet << _simulatedClientTime;  // Write ticks

    if (!bot)
    {
        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
        return nullptr;
    }
    // Construct packet structure from WorldPacket
    WorldPackets::Movement::MoveInitActiveMoverComplete moveInitComplete(std::move(packet));
    moveInitComplete.Read();  // Extract data from WorldPacket

if (!bot)

{

    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsInWorld");

    return;

}

    // Call the handler directly (packet forging)
    _session->HandleMoveInitActiveMoverComplete(moveInitComplete);

    TC_LOG_INFO("module.playerbot.packet",
        "✅ Bot {} successfully simulated CMSG_MOVE_INIT_ACTIVE_MOVER_COMPLETE - visibility enabled, flag set automatically",
        if (!bot)
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return nullptr;
        }
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return;
        }
        bot->GetName());
}

void BotPacketSimulator::SimulateTimeSyncResponse(uint32 counter)
{
    if (!_session)
        if (!bot)
        {
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return;
            }
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return;
        }
        return;

    Player* bot = _session->GetPlayer();

if (!bot)

{

    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");

    return;

}
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method IsInWorld");
            return;
        }
        if (!bot)
        {
            TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
            return;
        }
    if (!bot || !bot->IsInWorld())
        return;

    // Create WorldPacket with CMSG_TIME_SYNC_RESPONSE opcode
    WorldPacket packet(CMSG_TIME_SYNC_RESPONSE);
    packet << counter;               // Write SequenceIndex (first)
    packet << _simulatedClientTime;  // Write ClientTime (second)

    // Construct packet structure from WorldPacket
    WorldPackets::Misc::TimeSyncResponse timeSyncResponse(std::move(packet));
    timeSyncResponse.Read();  // Extract data from WorldPacket

    // Call the handler directly
    _session->HandleTimeSyncResponse(timeSyncResponse);

if (!bot)

{

    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");

    return;

}

    TC_LOG_TRACE("module.playerbot.packet",
        "Bot {} simulated CMSG_TIME_SYNC_RESPONSE (counter: {}, clientTime: {})",
        bot->GetName(), counter, _simulatedClientTime);

    // Increment client time for next sync (simulate passage of time)
    _simulatedClientTime += TIME_SYNC_INTERVAL;
}

void BotPacketSimulator::EnablePeriodicTimeSync()
{
    _periodicTimeSyncEnabled = true;
    _timeSyncCounter = 0;

    if (Player* bot = _session ? _session->GetPlayer() : nullptr)
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return;
            }
    {
        TC_LOG_DEBUG("module.playerbot.packet",
            "Bot {} periodic time sync enabled (interval: {}ms)",
            bot->GetName(), TIME_SYNC_INTERVAL);
    }
}

void BotPacketSimulator::DisablePeriodicTimeSync()
{
    _periodicTimeSyncEnabled = false;

    if (Player* bot = _session ? _session->GetPlayer() : nullptr)
            if (!bot)
            {
                TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: bot in method GetName");
                return;
            }
    {
        TC_LOG_DEBUG("module.playerbot.packet",
            "Bot {} periodic time sync disabled",
            bot->GetName());
    }
}

void BotPacketSimulator::Update(uint32 diff)
{
    if (!_periodicTimeSyncEnabled)
        return;

    if (!_session || !_session->GetPlayer() || !_session->GetPlayer()->IsInWorld())
        return;

    _timeSyncCounter += diff;

    if (_timeSyncCounter >= TIME_SYNC_INTERVAL)
    {
        // Simulate time sync response with incremented counter
        static uint32 globalTimeSyncCounter = 1000; // Start at 1000 to avoid special counters
        SimulateTimeSyncResponse(globalTimeSyncCounter++);

        _timeSyncCounter = 0;
    }
}

} // namespace Playerbot
