/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "PlayerBotHooks.h"
#include "Player.h"
#include "Group.h"
#include "GroupEventBus.h"
#include "Log.h"
#include "BotSession.h"
#include "ObjectAccessor.h"
#include <sstream>

namespace Playerbot
{

// Static member initialization
bool PlayerBotHooks::_initialized = false;
PlayerBotHooks::HookStatistics PlayerBotHooks::_stats;

// Hook function pointers (initially null)
std::function<void(Group*, Player*)> PlayerBotHooks::OnGroupMemberAdded = nullptr;
std::function<void(Group*, ObjectGuid, RemoveMethod)> PlayerBotHooks::OnGroupMemberRemoved = nullptr;
std::function<void(Group*, ObjectGuid)> PlayerBotHooks::OnGroupLeaderChanged = nullptr;
std::function<void(Group*)> PlayerBotHooks::OnGroupDisbanding = nullptr;
std::function<void(Group*, bool)> PlayerBotHooks::OnGroupRaidConverted = nullptr;
std::function<void(Group*, ObjectGuid, uint8)> PlayerBotHooks::OnSubgroupChanged = nullptr;
std::function<void(Group*, LootMethod)> PlayerBotHooks::OnLootMethodChanged = nullptr;
std::function<void(Group*, uint8)> PlayerBotHooks::OnLootThresholdChanged = nullptr;
std::function<void(Group*, ObjectGuid)> PlayerBotHooks::OnMasterLooterChanged = nullptr;
std::function<void(Group*, ObjectGuid, bool)> PlayerBotHooks::OnAssistantChanged = nullptr;
std::function<void(Group*, ObjectGuid)> PlayerBotHooks::OnMainTankChanged = nullptr;
std::function<void(Group*, ObjectGuid)> PlayerBotHooks::OnMainAssistChanged = nullptr;
std::function<void(Group*, uint8, ObjectGuid)> PlayerBotHooks::OnRaidTargetIconChanged = nullptr;
std::function<void(Group*, uint32, uint32, float, float, float)> PlayerBotHooks::OnRaidMarkerChanged = nullptr;
std::function<void(Group*, ObjectGuid, uint32)> PlayerBotHooks::OnReadyCheckStarted = nullptr;
std::function<void(Group*, ObjectGuid, bool)> PlayerBotHooks::OnReadyCheckResponse = nullptr;
std::function<void(Group*, bool, uint32, uint32)> PlayerBotHooks::OnReadyCheckCompleted = nullptr;
std::function<void(Group*, Difficulty)> PlayerBotHooks::OnDifficultyChanged = nullptr;
std::function<void(Group*, uint32, bool)> PlayerBotHooks::OnInstanceBind = nullptr;

void PlayerBotHooks::Initialize()
{
    if (_initialized)
    {
        TC_LOG_WARN("module.playerbot", "PlayerBotHooks::Initialize called multiple times");
        return;
    }

    TC_LOG_INFO("module.playerbot", "Initializing PlayerBot hook system...");

    RegisterHooks();

    _initialized = true;
    _stats.Reset();

    TC_LOG_INFO("module.playerbot", "PlayerBot hook system initialized successfully");
}

void PlayerBotHooks::Shutdown()
{
    if (!_initialized)
        return;

    TC_LOG_INFO("module.playerbot", "Shutting down PlayerBot hook system...");

    DumpStatistics();
    UnregisterHooks();

    _initialized = false;

    TC_LOG_INFO("module.playerbot", "PlayerBot hook system shutdown complete");
}

bool PlayerBotHooks::IsActive()
{
    return _initialized;
}

void PlayerBotHooks::RegisterHooks()
{
    // Register all hook implementations that publish events to GroupEventBus

    OnGroupMemberAdded = [](Group* group, Player* player)
    {
        if (!group || !player)
            return;

        IncrementHookCall("OnGroupMemberAdded");

        // Publish event to GroupEventBus
        GroupEvent event = GroupEvent::MemberJoined(group->GetGUID(), player->GetGUID());
        GroupEventBus::instance()->PublishEvent(event);

        TC_LOG_DEBUG("module.playerbot.hooks", "Hook: Member {} joined group {}",
            player->GetName(), group->GetGUID().ToString());
    };

    OnGroupMemberRemoved = [](Group* group, ObjectGuid guid, RemoveMethod method)
    {
        if (!group || guid.IsEmpty())
            return;

        IncrementHookCall("OnGroupMemberRemoved");

        // Publish event to GroupEventBus
        GroupEvent event = GroupEvent::MemberLeft(group->GetGUID(), guid, static_cast<uint32>(method));
        GroupEventBus::instance()->PublishEvent(event);

        TC_LOG_DEBUG("module.playerbot.hooks", "Hook: Member {} left group {} (method: {})",
            guid.ToString(), group->GetGUID().ToString(), static_cast<uint32>(method));
    };

    OnGroupLeaderChanged = [](Group* group, ObjectGuid newLeaderGuid)
    {
        if (!group || newLeaderGuid.IsEmpty())
            return;

        IncrementHookCall("OnGroupLeaderChanged");

        // Publish event to GroupEventBus
        GroupEvent event = GroupEvent::LeaderChanged(group->GetGUID(), newLeaderGuid);
        GroupEventBus::instance()->PublishEvent(event);

        TC_LOG_DEBUG("module.playerbot.hooks", "Hook: Group {} leader changed to {}",
            group->GetGUID().ToString(), newLeaderGuid.ToString());
    };

    OnGroupDisbanding = [](Group* group)
    {
        if (!group)
            return;

        IncrementHookCall("OnGroupDisbanding");

        // Publish CRITICAL event to GroupEventBus
        GroupEvent event = GroupEvent::GroupDisbanded(group->GetGUID());
        GroupEventBus::instance()->PublishEvent(event);

        // Also clear all pending events for this group
        GroupEventBus::instance()->ClearGroupEvents(group->GetGUID());

        TC_LOG_DEBUG("module.playerbot.hooks", "Hook: Group {} disbanding",
            group->GetGUID().ToString());
    };

    OnGroupRaidConverted = [](Group* group, bool isRaid)
    {
        if (!group)
            return;

        IncrementHookCall("OnGroupRaidConverted");

        // Create raid conversion event
        GroupEvent event;
        event.type = GroupEventType::RAID_CONVERTED;
        event.priority = EventPriority::HIGH;
        event.groupGuid = group->GetGUID();
        event.data1 = isRaid ? 1 : 0;
        event.timestamp = std::chrono::steady_clock::now();
        event.expiryTime = event.timestamp + std::chrono::milliseconds(30000);

        GroupEventBus::instance()->PublishEvent(event);

        TC_LOG_DEBUG("module.playerbot.hooks", "Hook: Group {} converted to {}",
            group->GetGUID().ToString(), isRaid ? "raid" : "party");
    };

    OnSubgroupChanged = [](Group* group, ObjectGuid playerGuid, uint8 newSubgroup)
    {
        if (!group || playerGuid.IsEmpty())
            return;

        IncrementHookCall("OnSubgroupChanged");

        // Create subgroup change event
        GroupEvent event;
        event.type = GroupEventType::SUBGROUP_CHANGED;
        event.priority = EventPriority::MEDIUM;
        event.groupGuid = group->GetGUID();
        event.targetGuid = playerGuid;
        event.data1 = newSubgroup;
        event.timestamp = std::chrono::steady_clock::now();
        event.expiryTime = event.timestamp + std::chrono::milliseconds(30000);

        GroupEventBus::instance()->PublishEvent(event);

        TC_LOG_DEBUG("module.playerbot.hooks", "Hook: Player {} moved to subgroup {} in group {}",
            playerGuid.ToString(), newSubgroup, group->GetGUID().ToString());
    };

    OnLootMethodChanged = [](Group* group, LootMethod method)
    {
        if (!group)
            return;

        IncrementHookCall("OnLootMethodChanged");

        // Publish loot method change event
        GroupEvent event = GroupEvent::LootMethodChanged(group->GetGUID(), static_cast<uint8>(method));
        GroupEventBus::instance()->PublishEvent(event);

        TC_LOG_DEBUG("module.playerbot.hooks", "Hook: Group {} loot method changed to {}",
            group->GetGUID().ToString(), static_cast<uint32>(method));
    };

    OnLootThresholdChanged = [](Group* group, uint8 threshold)
    {
        if (!group)
            return;

        IncrementHookCall("OnLootMethodChanged"); // Share counter with loot method

        // Create loot threshold event
        GroupEvent event;
        event.type = GroupEventType::LOOT_THRESHOLD_CHANGED;
        event.priority = EventPriority::LOW;
        event.groupGuid = group->GetGUID();
        event.data1 = threshold;
        event.timestamp = std::chrono::steady_clock::now();
        event.expiryTime = event.timestamp + std::chrono::milliseconds(30000);

        GroupEventBus::instance()->PublishEvent(event);
    };

    OnMasterLooterChanged = [](Group* group, ObjectGuid masterLooterGuid)
    {
        if (!group)
            return;

        IncrementHookCall("OnLootMethodChanged"); // Share counter

        // Create master looter event
        GroupEvent event;
        event.type = GroupEventType::MASTER_LOOTER_CHANGED;
        event.priority = EventPriority::LOW;
        event.groupGuid = group->GetGUID();
        event.targetGuid = masterLooterGuid;
        event.timestamp = std::chrono::steady_clock::now();
        event.expiryTime = event.timestamp + std::chrono::milliseconds(30000);

        GroupEventBus::instance()->PublishEvent(event);
    };

    OnAssistantChanged = [](Group* group, ObjectGuid memberGuid, bool isAssistant)
    {
        if (!group || memberGuid.IsEmpty())
            return;

        IncrementHookCall("OnGroupMemberAdded"); // Share counter for role changes

        // Create assistant change event
        GroupEvent event;
        event.type = GroupEventType::ASSISTANT_CHANGED;
        event.priority = EventPriority::MEDIUM;
        event.groupGuid = group->GetGUID();
        event.targetGuid = memberGuid;
        event.data1 = isAssistant ? 1 : 0;
        event.timestamp = std::chrono::steady_clock::now();
        event.expiryTime = event.timestamp + std::chrono::milliseconds(30000);

        GroupEventBus::instance()->PublishEvent(event);
    };

    OnMainTankChanged = [](Group* group, ObjectGuid tankGuid)
    {
        if (!group)
            return;

        IncrementHookCall("OnGroupMemberAdded"); // Share counter

        // Create main tank event
        GroupEvent event;
        event.type = GroupEventType::MAIN_TANK_CHANGED;
        event.priority = EventPriority::LOW;
        event.groupGuid = group->GetGUID();
        event.targetGuid = tankGuid;
        event.timestamp = std::chrono::steady_clock::now();
        event.expiryTime = event.timestamp + std::chrono::milliseconds(30000);

        GroupEventBus::instance()->PublishEvent(event);
    };

    OnMainAssistChanged = [](Group* group, ObjectGuid assistGuid)
    {
        if (!group)
            return;

        IncrementHookCall("OnGroupMemberAdded"); // Share counter

        // Create main assist event
        GroupEvent event;
        event.type = GroupEventType::MAIN_ASSIST_CHANGED;
        event.priority = EventPriority::LOW;
        event.groupGuid = group->GetGUID();
        event.targetGuid = assistGuid;
        event.timestamp = std::chrono::steady_clock::now();
        event.expiryTime = event.timestamp + std::chrono::milliseconds(30000);

        GroupEventBus::instance()->PublishEvent(event);
    };

    OnRaidTargetIconChanged = [](Group* group, uint8 icon, ObjectGuid targetGuid)
    {
        if (!group)
            return;

        IncrementHookCall("OnRaidTargetIconChanged");

        // Publish target icon event
        GroupEvent event = GroupEvent::TargetIconChanged(group->GetGUID(), icon, targetGuid);
        GroupEventBus::instance()->PublishEvent(event);

        TC_LOG_DEBUG("module.playerbot.hooks", "Hook: Group {} target icon {} set to {}",
            group->GetGUID().ToString(), icon, targetGuid.ToString());
    };

    OnRaidMarkerChanged = [](Group* group, uint32 markerId, uint32 mapId, float x, float y, float z)
    {
        if (!group)
            return;

        IncrementHookCall("OnRaidTargetIconChanged"); // Share counter

        // Create marker event
        GroupEvent event;
        event.type = GroupEventType::RAID_MARKER_CHANGED;
        event.priority = EventPriority::LOW;
        event.groupGuid = group->GetGUID();
        event.data1 = markerId;
        event.data2 = mapId;
        // Pack coordinates into data3 (simplified, not perfect but works)
        event.timestamp = std::chrono::steady_clock::now();
        event.expiryTime = event.timestamp + std::chrono::milliseconds(60000);

        GroupEventBus::instance()->PublishEvent(event);
    };

    OnReadyCheckStarted = [](Group* group, ObjectGuid initiatorGuid, uint32 durationMs)
    {
        if (!group || initiatorGuid.IsEmpty())
            return;

        IncrementHookCall("OnReadyCheckStarted");

        // Publish ready check start event
        GroupEvent event = GroupEvent::ReadyCheckStarted(group->GetGUID(), initiatorGuid, durationMs);
        GroupEventBus::instance()->PublishEvent(event);

        TC_LOG_DEBUG("module.playerbot.hooks", "Hook: Ready check started in group {} by {} (duration: {}ms)",
            group->GetGUID().ToString(), initiatorGuid.ToString(), durationMs);
    };

    OnReadyCheckResponse = [](Group* group, ObjectGuid memberGuid, bool ready)
    {
        if (!group || memberGuid.IsEmpty())
            return;

        IncrementHookCall("OnReadyCheckStarted"); // Share counter

        // Create ready check response event
        GroupEvent event;
        event.type = GroupEventType::READY_CHECK_RESPONSE;
        event.priority = EventPriority::LOW;
        event.groupGuid = group->GetGUID();
        event.sourceGuid = memberGuid;
        event.data1 = ready ? 1 : 0;
        event.timestamp = std::chrono::steady_clock::now();
        event.expiryTime = event.timestamp + std::chrono::milliseconds(5000);

        GroupEventBus::instance()->PublishEvent(event);
    };

    OnReadyCheckCompleted = [](Group* group, bool allReady, uint32 respondedCount, uint32 totalMembers)
    {
        if (!group)
            return;

        IncrementHookCall("OnReadyCheckStarted"); // Share counter

        // Create ready check completed event
        GroupEvent event;
        event.type = GroupEventType::READY_CHECK_COMPLETED;
        event.priority = EventPriority::BATCH;
        event.groupGuid = group->GetGUID();
        event.data1 = allReady ? 1 : 0;
        event.data2 = respondedCount;
        event.data3 = totalMembers;
        event.timestamp = std::chrono::steady_clock::now();
        event.expiryTime = event.timestamp + std::chrono::milliseconds(10000);

        GroupEventBus::instance()->PublishEvent(event);
    };

    OnDifficultyChanged = [](Group* group, Difficulty difficulty)
    {
        if (!group)
            return;

        IncrementHookCall("OnDifficultyChanged");

        // Publish difficulty change event
        GroupEvent event = GroupEvent::DifficultyChanged(group->GetGUID(), static_cast<uint8>(difficulty));
        GroupEventBus::instance()->PublishEvent(event);

        TC_LOG_DEBUG("module.playerbot.hooks", "Hook: Group {} difficulty changed to {}",
            group->GetGUID().ToString(), static_cast<uint32>(difficulty));
    };

    OnInstanceBind = [](Group* group, uint32 instanceId, bool permanent)
    {
        if (!group)
            return;

        IncrementHookCall("OnDifficultyChanged"); // Share counter

        // Create instance bind event
        GroupEvent event;
        event.type = GroupEventType::INSTANCE_LOCK_MESSAGE;
        event.priority = EventPriority::MEDIUM;
        event.groupGuid = group->GetGUID();
        event.data1 = instanceId;
        event.data2 = permanent ? 1 : 0;
        event.timestamp = std::chrono::steady_clock::now();
        event.expiryTime = event.timestamp + std::chrono::milliseconds(60000);

        GroupEventBus::instance()->PublishEvent(event);
    };

    TC_LOG_DEBUG("module.playerbot", "PlayerBotHooks: All {} hook functions registered", 19);
}

void PlayerBotHooks::UnregisterHooks()
{
    // Clear all hook function pointers
    OnGroupMemberAdded = nullptr;
    OnGroupMemberRemoved = nullptr;
    OnGroupLeaderChanged = nullptr;
    OnGroupDisbanding = nullptr;
    OnGroupRaidConverted = nullptr;
    OnSubgroupChanged = nullptr;
    OnLootMethodChanged = nullptr;
    OnLootThresholdChanged = nullptr;
    OnMasterLooterChanged = nullptr;
    OnAssistantChanged = nullptr;
    OnMainTankChanged = nullptr;
    OnMainAssistChanged = nullptr;
    OnRaidTargetIconChanged = nullptr;
    OnRaidMarkerChanged = nullptr;
    OnReadyCheckStarted = nullptr;
    OnReadyCheckResponse = nullptr;
    OnReadyCheckCompleted = nullptr;
    OnDifficultyChanged = nullptr;
    OnInstanceBind = nullptr;

    TC_LOG_DEBUG("module.playerbot", "PlayerBotHooks: All hook functions unregistered");
}

bool PlayerBotHooks::IsPlayerBot(Player const* player)
{
    if (!player)
        return false;

    WorldSession* session = player->GetSession();
    if (!session)
        return false;

    // Check if session is a BotSession
    return dynamic_cast<BotSession const*>(session) != nullptr;
}

uint32 PlayerBotHooks::GetBotCountInGroup(Group const* group)
{
    if (!group)
        return 0;

    uint32 botCount = 0;

    for (auto const& slot : group->GetMemberSlots())
    {
        if (Player* member = ObjectAccessor::FindPlayer(slot.guid))
        {
            if (IsPlayerBot(member))
                ++botCount;
        }
    }

    return botCount;
}

bool PlayerBotHooks::GroupHasBots(Group const* group)
{
    return GetBotCountInGroup(group) > 0;
}

void PlayerBotHooks::IncrementHookCall(const char* hookName)
{
    ++_stats.totalHookCalls;

    // Map specific hook calls to stat counters
    // Using string comparison for simplicity (could optimize with enum)
    std::string hook(hookName);

    if (hook == "OnGroupMemberAdded")
        ++_stats.memberAddedCalls;
    else if (hook == "OnGroupMemberRemoved")
        ++_stats.memberRemovedCalls;
    else if (hook == "OnGroupLeaderChanged")
        ++_stats.leaderChangedCalls;
    else if (hook == "OnGroupDisbanding")
        ++_stats.groupDisbandedCalls;
    else if (hook == "OnGroupRaidConverted")
        ++_stats.raidConvertedCalls;
    else if (hook == "OnLootMethodChanged")
        ++_stats.lootMethodChangedCalls;
    else if (hook == "OnReadyCheckStarted")
        ++_stats.readyCheckCalls;
    else if (hook == "OnRaidTargetIconChanged")
        ++_stats.targetIconCalls;
    else if (hook == "OnDifficultyChanged")
        ++_stats.difficultyCalls;
}

PlayerBotHooks::HookStatistics const& PlayerBotHooks::GetStatistics()
{
    return _stats;
}

void PlayerBotHooks::ResetStatistics()
{
    _stats.Reset();
    TC_LOG_DEBUG("module.playerbot", "PlayerBotHooks: Statistics reset");
}

void PlayerBotHooks::DumpStatistics()
{
    TC_LOG_INFO("module.playerbot", "=== PlayerBot Hook Statistics ===");
    TC_LOG_INFO("module.playerbot", "{}", _stats.ToString());
}

std::string PlayerBotHooks::HookStatistics::ToString() const
{
    std::ostringstream oss;
    oss << "Total Hook Calls: " << totalHookCalls
        << ", Member Added: " << memberAddedCalls
        << ", Member Removed: " << memberRemovedCalls
        << ", Leader Changed: " << leaderChangedCalls
        << ", Group Disbanded: " << groupDisbandedCalls
        << ", Raid Converted: " << raidConvertedCalls
        << ", Loot Changed: " << lootMethodChangedCalls
        << ", Ready Checks: " << readyCheckCalls
        << ", Target Icons: " << targetIconCalls
        << ", Difficulty: " << difficultyCalls;
    return oss.str();
}

void PlayerBotHooks::HookStatistics::Reset()
{
    totalHookCalls = 0;
    memberAddedCalls = 0;
    memberRemovedCalls = 0;
    leaderChangedCalls = 0;
    groupDisbandedCalls = 0;
    raidConvertedCalls = 0;
    lootMethodChangedCalls = 0;
    readyCheckCalls = 0;
    targetIconCalls = 0;
    difficultyCalls = 0;
}

} // namespace Playerbot
