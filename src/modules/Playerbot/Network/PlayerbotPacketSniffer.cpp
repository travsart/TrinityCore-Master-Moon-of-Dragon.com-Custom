/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "PlayerbotPacketSniffer.h"
#include "WorldSession.h"
#include "WorldPacket.h"
#include "Player.h"
#include "PlayerBotHooks.h"
#include "Log.h"
#include <sstream>
#include <iomanip>

namespace Playerbot
{

// Static member initialization
std::unordered_map<OpcodeServer, PacketCategory> PlayerbotPacketSniffer::_packetCategoryMap;
bool PlayerbotPacketSniffer::_initialized = false;
std::atomic<uint64_t> PlayerbotPacketSniffer::_totalPackets{0};
std::array<std::atomic<uint64_t>, static_cast<uint8>(PacketCategory::MAX_CATEGORY)> PlayerbotPacketSniffer::_categoryPackets{};
std::atomic<uint64_t> PlayerbotPacketSniffer::_totalProcessTimeUs{0};
std::atomic<uint64_t> PlayerbotPacketSniffer::_peakProcessTimeUs{0};
std::chrono::steady_clock::time_point PlayerbotPacketSniffer::_startTime;

// Typed packet handler registry (WoW 11.2)
std::unordered_map<std::type_index, PlayerbotPacketSniffer::TypedPacketHandler> PlayerbotPacketSniffer::_typedPacketHandlers;

void PlayerbotPacketSniffer::Initialize()
{
    if (_initialized)
        return;

    TC_LOG_INFO("playerbot", "PlayerbotPacketSniffer: Initializing packet interception system...");

    // Initialize opcode mapping
    InitializeOpcodeMapping();

    // Register typed packet handlers (WoW 11.2)
    RegisterGroupPacketHandlers();
    RegisterCombatPacketHandlers();
    RegisterCooldownPacketHandlers();
    RegisterAuraPacketHandlers();
    RegisterLootPacketHandlers();
    RegisterQuestPacketHandlers();
    RegisterResourcePacketHandlers();
    RegisterSocialPacketHandlers();
    RegisterAuctionPacketHandlers();
    RegisterNPCPacketHandlers();
    RegisterInstancePacketHandlers();

    // Initialize statistics
    _totalPackets.store(0);
    for (auto& counter : _categoryPackets)
        counter.store(0);
    _totalProcessTimeUs.store(0);
    _peakProcessTimeUs.store(0);
    _startTime = std::chrono::steady_clock::now();

    _initialized = true;

    TC_LOG_INFO("playerbot", "PlayerbotPacketSniffer: Initialized with {} opcode mappings and {} typed handlers",
        _packetCategoryMap.size(), _typedPacketHandlers.size());
}

void PlayerbotPacketSniffer::Shutdown()
{
    if (!_initialized)
        return;

    TC_LOG_INFO("playerbot", "PlayerbotPacketSniffer: Shutting down...");

    DumpStatistics();

    _packetCategoryMap.clear();
    _initialized = false;

    TC_LOG_INFO("playerbot", "PlayerbotPacketSniffer: Shutdown complete");
}

void PlayerbotPacketSniffer::OnPacketSend(WorldSession* session, WorldPacket const& packet)
{
    if (!_initialized || !session)
        return;

    // Performance: Early exit if not a bot
    Player* player = session->GetPlayer();
    if (!player || !PlayerBotHooks::IsPlayerBot(player))
        return;

    auto startTime = std::chrono::high_resolution_clock::now();

    // Categorize and route packet
    PacketCategory category = CategorizePacket(static_cast<OpcodeServer>(packet.GetOpcode()));
    if (category != PacketCategory::UNKNOWN)
    {
        RouteToCategory(category, session, packet);

        // Update statistics
        _categoryPackets[static_cast<uint8>(category)].fetch_add(1, std::memory_order_relaxed);
    }

    _totalPackets.fetch_add(1, std::memory_order_relaxed);

    // Track processing time
    auto endTime = std::chrono::high_resolution_clock::now();
    uint64_t processingTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

    _totalProcessTimeUs.fetch_add(processingTimeUs, std::memory_order_relaxed);

    // Update peak time (lock-free CAS loop)
    uint64_t currentPeak = _peakProcessTimeUs.load(std::memory_order_relaxed);
    while (processingTimeUs > currentPeak &&
           !_peakProcessTimeUs.compare_exchange_weak(currentPeak, processingTimeUs, std::memory_order_relaxed));
}

void PlayerbotPacketSniffer::RouteToCategory(PacketCategory category, WorldSession* session, WorldPacket const& packet)
{
    switch (category)
    {
        case PacketCategory::GROUP:
            ParseGroupPacket(session, packet);
            break;
        case PacketCategory::COMBAT:
            ParseCombatPacket(session, packet);
            break;
        case PacketCategory::COOLDOWN:
            ParseCooldownPacket(session, packet);
            break;
        case PacketCategory::LOOT:
            ParseLootPacket(session, packet);
            break;
        case PacketCategory::QUEST:
            ParseQuestPacket(session, packet);
            break;
        case PacketCategory::AURA:
            ParseAuraPacket(session, packet);
            break;
        case PacketCategory::RESOURCE:
            ParseResourcePacket(session, packet);
            break;
        case PacketCategory::SOCIAL:
            ParseSocialPacket(session, packet);
            break;
        case PacketCategory::AUCTION:
            ParseAuctionPacket(session, packet);
            break;
        case PacketCategory::NPC:
            ParseNPCPacket(session, packet);
            break;
        case PacketCategory::INSTANCE:
            ParseInstancePacket(session, packet);
            break;
        default:
            break;
    }
}

PacketCategory PlayerbotPacketSniffer::CategorizePacket(OpcodeServer opcode)
{
    auto it = _packetCategoryMap.find(opcode);
    if (it != _packetCategoryMap.end())
        return it->second;

    return PacketCategory::UNKNOWN;
}

void PlayerbotPacketSniffer::InitializeOpcodeMapping()
{
    // DEPRECATED: Opcode-based packet routing is no longer used in WoW 11.2
    // All packet handling now occurs via typed packet handlers registered in
    // Register*PacketHandlers() functions (see Parse*Packet_Typed.cpp files)
    //
    // This function is kept for backward compatibility but does nothing.
    // The CategorizePacket() function will always return PacketCategory::UNKNOWN
    // and typed packets are intercepted before reaching OnPacketSend().

    TC_LOG_DEBUG("playerbot", "PlayerbotPacketSniffer: Opcode mapping skipped (using typed packet handlers)");
}

// Category-specific parsers (deprecated - all handling now via typed handlers)
void PlayerbotPacketSniffer::ParseGroupPacket(WorldSession* session, WorldPacket const& packet)
{
    // Deprecated: All group packet handling now in ParseGroupPacket_Typed.cpp via typed handlers
}

void PlayerbotPacketSniffer::ParseCombatPacket(WorldSession* session, WorldPacket const& packet)
{
    // Deprecated: All combat packet handling now in ParseCombatPacket_Typed.cpp via typed handlers
}

void PlayerbotPacketSniffer::ParseCooldownPacket(WorldSession* session, WorldPacket const& packet)
{
    // Deprecated: All cooldown packet handling now in ParseCooldownPacket_Typed.cpp via typed handlers
}

void PlayerbotPacketSniffer::ParseLootPacket(WorldSession* session, WorldPacket const& packet)
{
    // Deprecated: All loot packet handling now in ParseLootPacket_Typed.cpp via typed handlers
}

void PlayerbotPacketSniffer::ParseQuestPacket(WorldSession* session, WorldPacket const& packet)
{
    // Deprecated: All quest packet handling now in ParseQuestPacket_Typed.cpp via typed handlers
}

void PlayerbotPacketSniffer::ParseAuraPacket(WorldSession* session, WorldPacket const& packet)
{
    // Deprecated: All aura packet handling now in ParseAuraPacket_Typed.cpp via typed handlers
}

void PlayerbotPacketSniffer::ParseResourcePacket(WorldSession* session, WorldPacket const& packet)
{
    // Not yet implemented
}

void PlayerbotPacketSniffer::ParseSocialPacket(WorldSession* session, WorldPacket const& packet)
{
    // Not yet implemented
}

void PlayerbotPacketSniffer::ParseAuctionPacket(WorldSession* session, WorldPacket const& packet)
{
    // Not yet implemented
}

void PlayerbotPacketSniffer::ParseNPCPacket(WorldSession* session, WorldPacket const& packet)
{
    // Not yet implemented
}

void PlayerbotPacketSniffer::ParseInstancePacket(WorldSession* session, WorldPacket const& packet)
{
    // Not yet implemented
}

// Statistics implementation
std::string PlayerbotPacketSniffer::Statistics::ToString() const
{
    std::ostringstream ss;

    ss << "\n=== PlayerbotPacketSniffer Statistics ===\n";
    ss << "Total Packets Processed: " << totalPacketsProcessed << "\n";
    ss << "Average Processing Time: " << avgProcessTimeUs << " μs\n";
    ss << "Peak Processing Time: " << peakProcessTimeUs << " μs\n";

    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - startTime).count();
    ss << "Uptime: " << uptime << " seconds\n";

    ss << "\nPackets per Category:\n";
    const char* categoryNames[] = {
        "GROUP", "COMBAT", "COOLDOWN", "LOOT", "QUEST", "AURA",
        "RESOURCE", "SOCIAL", "AUCTION", "NPC", "INSTANCE", "UNKNOWN"
    };

    for (uint8 i = 0; i < static_cast<uint8>(PacketCategory::MAX_CATEGORY); ++i)
    {
        if (packetsPerCategory[i] > 0)
        {
            ss << "  " << std::setw(12) << std::left << categoryNames[i]
               << ": " << std::setw(10) << std::right << packetsPerCategory[i];

            if (totalPacketsProcessed > 0)
            {
                float percentage = (float)packetsPerCategory[i] / totalPacketsProcessed * 100.0f;
                ss << " (" << std::fixed << std::setprecision(2) << percentage << "%)";
            }
            ss << "\n";
        }
    }

    return ss.str();
}

void PlayerbotPacketSniffer::Statistics::Reset()
{
    totalPacketsProcessed = 0;
    packetsPerCategory.fill(0);
    avgProcessTimeUs = 0;
    peakProcessTimeUs = 0;
    startTime = std::chrono::steady_clock::now();
}

PlayerbotPacketSniffer::Statistics PlayerbotPacketSniffer::GetStatistics()
{
    Statistics stats;
    stats.totalPacketsProcessed = _totalPackets.load(std::memory_order_relaxed);

    for (uint8 i = 0; i < static_cast<uint8>(PacketCategory::MAX_CATEGORY); ++i)
        stats.packetsPerCategory[i] = _categoryPackets[i].load(std::memory_order_relaxed);

    uint64_t totalTime = _totalProcessTimeUs.load(std::memory_order_relaxed);
    stats.avgProcessTimeUs = stats.totalPacketsProcessed > 0 ?
        totalTime / stats.totalPacketsProcessed : 0;

    stats.peakProcessTimeUs = _peakProcessTimeUs.load(std::memory_order_relaxed);
    stats.startTime = _startTime;

    return stats;
}

void PlayerbotPacketSniffer::DumpStatistics()
{
    Statistics stats = GetStatistics();
    TC_LOG_INFO("playerbot", "{}", stats.ToString());
}

} // namespace Playerbot
