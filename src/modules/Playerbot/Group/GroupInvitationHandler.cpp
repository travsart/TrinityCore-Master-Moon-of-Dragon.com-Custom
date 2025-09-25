/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "GroupInvitationHandler.h"
#include "Player.h"
#include "Group.h"
#include "GroupMgr.h"
#include "WorldSession.h"
#include "WorldPacket.h"
#include "ObjectAccessor.h"
#include "Log.h"
#include "World.h"
#include "SocialMgr.h"
#include "PartyPackets.h"
#include "Opcodes.h"
#include "Config/PlayerbotConfig.h"
#include <algorithm>

namespace Playerbot
{

GroupInvitationHandler::GroupInvitationHandler(Player* bot)
    : _bot(bot)
    , _autoAcceptEnabled(true)
    , _responseDelayMs(500)
    , _maxInvitationsPerMinute(10)
    , _maxAcceptRange(100.0f)
    , _updateTimer(0)
    , _cleanupTimer(0)
{
    if (!_bot)
    {
        TC_LOG_ERROR("playerbot", "GroupInvitationHandler: Attempted to create handler with null bot");
        return;
    }

    TC_LOG_DEBUG("playerbot", "GroupInvitationHandler: Initialized for bot {} ({})",
        _bot->GetName(), _bot->GetGUID().ToString());
}

GroupInvitationHandler::~GroupInvitationHandler()
{
    ClearPendingInvitations();
}

bool GroupInvitationHandler::HandleInvitation(WorldPackets::Party::PartyInvite const& packet)
{
    std::lock_guard<std::mutex> lock(_invitationMutex);

    // Extract invitation details
    ObjectGuid inviterGuid = packet.InviterGUID;
    std::string inviterName = packet.InviterName;
    uint32 proposedRoles = packet.ProposedRoles;

    TC_LOG_DEBUG("playerbot", "GroupInvitationHandler: Bot {} received invitation from {} ({})",
        _bot->GetName(), inviterName, inviterGuid.ToString());

    // Update statistics
    _stats.totalInvitations++;
    _stats.lastInvitation = std::chrono::steady_clock::now();

    // Validate inviter first
    Player* inviter = ObjectAccessor::FindPlayer(inviterGuid);
    if (!inviter)
    {
        TC_LOG_DEBUG("playerbot", "GroupInvitationHandler: Inviter {} not found, declining", inviterGuid.ToString());
        _stats.invalidInvitations++;
        return false;
    }

    // Check if invitation is valid
    if (!IsValidInviter(inviter))
    {
        TC_LOG_DEBUG("playerbot", "GroupInvitationHandler: Invalid inviter {}, declining", inviterName);
        DeclineInvitation(inviterGuid, "Invalid inviter");
        _stats.invalidInvitations++;
        return false;
    }

    // Check for invitation loops
    if (!ValidateNoInvitationLoop(inviterGuid))
    {
        TC_LOG_DEBUG("playerbot", "GroupInvitationHandler: Invitation loop detected from {}, declining", inviterName);
        DeclineInvitation(inviterGuid, "Loop prevention");
        _stats.declinedInvitations++;
        return false;
    }

    // Check if bot already has too many pending invitations (anti-spam)
    if (_pendingInvitations.size() > 5)
    {
        TC_LOG_DEBUG("playerbot", "GroupInvitationHandler: Too many pending invitations for bot {}", _bot->GetName());
        _stats.declinedInvitations++;
        return false;
    }

    // Create pending invitation
    PendingInvitation invitation;
    invitation.inviterGuid = inviterGuid;
    invitation.inviterName = inviterName;
    invitation.proposedRoles = proposedRoles;
    invitation.timestamp = std::chrono::steady_clock::now();
    invitation.isProcessing = false;

    // Add to queue
    _pendingInvitations.push(invitation);

    TC_LOG_DEBUG("playerbot", "GroupInvitationHandler: Queued invitation from {} for bot {}",
        inviterName, _bot->GetName());

    return true;
}

void GroupInvitationHandler::Update(uint32 diff)
{
    // Update timers
    _updateTimer += diff;
    _cleanupTimer += diff;

    // Cleanup expired invitations periodically
    if (_cleanupTimer >= CLEANUP_INTERVAL)
    {
        CleanupExpiredInvitations();
        _cleanupTimer = 0;
    }

    // Process invitations at regular intervals
    if (_updateTimer >= UPDATE_INTERVAL)
    {
        ProcessNextInvitation();
        _updateTimer = 0;
    }
}

bool GroupInvitationHandler::ShouldAcceptInvitation(ObjectGuid inviterGuid) const
{
    // Check if auto-accept is enabled
    if (!_autoAcceptEnabled)
        return false;

    // Get inviter
    Player* inviter = ObjectAccessor::FindPlayer(inviterGuid);
    if (!inviter)
        return false;

    // Validate inviter
    if (!IsValidInviter(inviter))
        return false;

    // Check if bot is already in a group
    if (_bot->GetGroup())
    {
        TC_LOG_DEBUG("playerbot", "GroupInvitationHandler: Bot {} already in group, declining", _bot->GetName());
        return false;
    }

    // Check if bot has pending group invite
    if (_bot->GetGroupInvite())
    {
        // If it's from the same inviter, accept it
        if (_bot->GetGroupInvite()->GetLeaderGUID() == inviterGuid)
            return true;

        TC_LOG_DEBUG("playerbot", "GroupInvitationHandler: Bot {} has different pending invite", _bot->GetName());
        return false;
    }

    // Check inviter's group if they have one
    Group* inviterGroup = inviter->GetGroup();
    if (inviterGroup)
    {
        if (!CanJoinGroup(inviterGroup))
            return false;
    }

    return true;
}

bool GroupInvitationHandler::IsValidInviter(Player* inviter) const
{
    if (!inviter)
        return false;

    // Don't accept invitations from other bots to prevent bot loops
    if (PlayerbotConfig* config = PlayerbotConfig::instance())
    {
        // Check if inviter is a bot (implementation depends on your bot detection method)
        // For now, we'll check if they have a bot session or are flagged as bots
        if (inviter->GetSession() && inviter->GetSession()->GetPlayer())
        {
            // You may need to add a method to detect if a player is a bot
            // For safety, we'll accept all players for now but log it
            TC_LOG_DEBUG("playerbot", "GroupInvitationHandler: Checking inviter {} validity", inviter->GetName());
        }
    }

    // Check if inviter is in range
    if (!IsInviterInRange(inviter))
    {
        TC_LOG_DEBUG("playerbot", "GroupInvitationHandler: Inviter {} out of range", inviter->GetName());
        return false;
    }

    // Check if inviter is on same team (unless cross-faction is allowed)
    if (!sWorld->getBoolConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_GROUP))
    {
        if (_bot->GetTeam() != inviter->GetTeam())
        {
            TC_LOG_DEBUG("playerbot", "GroupInvitationHandler: Cross-faction invitation from {}", inviter->GetName());
            return false;
        }
    }

    // Check if inviter is ignored
    if (_bot->GetSocial()->HasIgnore(inviter->GetGUID(), inviter->GetSession()->GetAccountGUID()))
    {
        TC_LOG_DEBUG("playerbot", "GroupInvitationHandler: Inviter {} is ignored", inviter->GetName());
        return false;
    }

    return true;
}

bool GroupInvitationHandler::CanJoinGroup(Group* group) const
{
    if (!group)
        return true; // No group yet, can join

    // Check if group is full
    if (group->IsFull())
    {
        TC_LOG_DEBUG("playerbot", "GroupInvitationHandler: Group is full");
        return false;
    }

    // Check if bot meets level requirements
    Player* leader = ObjectAccessor::FindPlayer(group->GetLeaderGUID());
    if (leader && !leader->GetSocial()->HasFriend(_bot->GetGUID()) &&
        leader->GetLevel() < sWorld->getIntConfig(CONFIG_PARTY_LEVEL_REQ))
    {
        TC_LOG_DEBUG("playerbot", "GroupInvitationHandler: Bot doesn't meet level requirements");
        return false;
    }

    // Check instance compatibility
    if (group->IsCreated())
    {
        // Check if bot can enter the same instances as the group
        uint32 groupInstanceId = 0;
        for (Group::MemberSlot const& member : group->GetMemberSlots())
        {
            if (Player* groupMember = ObjectAccessor::FindPlayer(member.guid))
            {
                if (groupMember->GetInstanceId() != 0)
                {
                    groupInstanceId = groupMember->GetInstanceId();
                    break;
                }
            }
        }

        if (groupInstanceId != 0 && _bot->GetInstanceId() != 0 &&
            _bot->GetInstanceId() != groupInstanceId)
        {
            TC_LOG_DEBUG("playerbot", "GroupInvitationHandler: Instance mismatch");
            return false;
        }
    }

    return true;
}

bool GroupInvitationHandler::AcceptInvitation(ObjectGuid inviterGuid)
{
    std::lock_guard<std::mutex> lock(_invitationMutex);

    TC_LOG_DEBUG("playerbot", "GroupInvitationHandler: Bot {} accepting invitation from {}",
        _bot->GetName(), inviterGuid.ToString());

    // Record accept time
    auto acceptTime = std::chrono::steady_clock::now();

    // Send accept packet
    if (!SendAcceptPacket())
    {
        TC_LOG_ERROR("playerbot", "GroupInvitationHandler: Failed to send accept packet for bot {}", _bot->GetName());
        return false;
    }

    // Update statistics
    _stats.acceptedInvitations++;
    if (_stats.lastInvitation != std::chrono::steady_clock::time_point{})
    {
        auto responseTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            acceptTime - _stats.lastInvitation);
        UpdateStatistics(true, responseTime);
    }

    // Remember this inviter to prevent loops
    _recentInviters.insert(inviterGuid);
    _lastAcceptTime = acceptTime;

    // Clear current inviter
    _currentInviter = ObjectGuid::Empty;

    LogInvitationEvent("ACCEPTED", inviterGuid);

    return true;
}

void GroupInvitationHandler::DeclineInvitation(ObjectGuid inviterGuid, std::string const& reason)
{
    std::lock_guard<std::mutex> lock(_invitationMutex);

    TC_LOG_DEBUG("playerbot", "GroupInvitationHandler: Bot {} declining invitation from {} ({})",
        _bot->GetName(), inviterGuid.ToString(), reason);

    // Send decline packet
    SendDeclinePacket(reason);

    // Update statistics
    _stats.declinedInvitations++;

    // Clear current inviter
    if (_currentInviter == inviterGuid)
        _currentInviter = ObjectGuid::Empty;

    LogInvitationEvent("DECLINED", inviterGuid, reason);
}

bool GroupInvitationHandler::HasPendingInvitation() const
{
    std::lock_guard<std::mutex> lock(_invitationMutex);
    return !_pendingInvitations.empty() || !_currentInviter.IsEmpty();
}

ObjectGuid GroupInvitationHandler::GetPendingInviter() const
{
    std::lock_guard<std::mutex> lock(_invitationMutex);

    if (!_currentInviter.IsEmpty())
        return _currentInviter;

    if (!_pendingInvitations.empty())
        return _pendingInvitations.front().inviterGuid;

    return ObjectGuid::Empty;
}

void GroupInvitationHandler::ClearPendingInvitations()
{
    std::lock_guard<std::mutex> lock(_invitationMutex);

    while (!_pendingInvitations.empty())
        _pendingInvitations.pop();

    _currentInviter = ObjectGuid::Empty;
    _recentInviters.clear();
}

void GroupInvitationHandler::SetResponseDelay(uint32 delayMs)
{
    _responseDelayMs = std::min(std::max(delayMs, MIN_RESPONSE_DELAY), MAX_RESPONSE_DELAY);
    TC_LOG_DEBUG("playerbot", "GroupInvitationHandler: Response delay set to {}ms for bot {}",
        _responseDelayMs, _bot->GetName());
}

WorldSession* GroupInvitationHandler::GetSession() const
{
    return _bot ? _bot->GetSession() : nullptr;
}

bool GroupInvitationHandler::SendAcceptPacket()
{
    WorldSession* session = GetSession();
    if (!session)
        return false;

    // Check if bot has a pending group invite from TrinityCore
    Group* inviteGroup = _bot->GetGroupInvite();
    if (!inviteGroup)
    {
        TC_LOG_DEBUG("playerbot", "GroupInvitationHandler: No pending group invite for bot {}", _bot->GetName());
        return false;
    }

    // Create a dummy packet for the constructor
    WorldPacket dummyPacket(CMSG_PARTY_INVITE_RESPONSE, 1);

    // Create and configure response
    WorldPackets::Party::PartyInviteResponse response(std::move(dummyPacket));
    response.Accept = true;
    // PartyIndex is optional, leave it unset for default behavior

    // Send the packet through the session
    session->HandlePartyInviteResponseOpcode(response);

    TC_LOG_DEBUG("playerbot", "GroupInvitationHandler: Sent accept packet for bot {}", _bot->GetName());
    return true;
}

void GroupInvitationHandler::SendDeclinePacket(std::string const& reason)
{
    WorldSession* session = GetSession();
    if (!session)
        return;

    // Create a dummy packet for the constructor
    WorldPacket dummyPacket(CMSG_PARTY_INVITE_RESPONSE, 1);

    // Create and configure response
    WorldPackets::Party::PartyInviteResponse response(std::move(dummyPacket));
    response.Accept = false;

    // Send the packet through the session
    session->HandlePartyInviteResponseOpcode(response);

    TC_LOG_DEBUG("playerbot", "GroupInvitationHandler: Sent decline packet for bot {} ({})",
        _bot->GetName(), reason);
}

bool GroupInvitationHandler::ValidateNoInvitationLoop(ObjectGuid inviterGuid) const
{
    // Check if we recently accepted an invitation from this inviter
    if (_recentInviters.find(inviterGuid) != _recentInviters.end())
    {
        // Check if enough time has passed
        auto timeSinceLastAccept = std::chrono::steady_clock::now() - _lastAcceptTime;
        if (timeSinceLastAccept < std::chrono::milliseconds(RECENT_INVITER_MEMORY))
        {
            TC_LOG_DEBUG("playerbot", "GroupInvitationHandler: Recent invitation from same inviter detected");
            return false;
        }
    }

    // Check if inviter is a bot that we invited (would create a loop)
    Player* inviter = ObjectAccessor::FindPlayer(inviterGuid);
    if (inviter && inviter->GetGroup())
    {
        // If we're the leader of a group and the inviter is in our group, this would be a loop
        if (_bot->GetGroup() && _bot->GetGroup()->IsLeader(_bot->GetGUID()))
        {
            if (inviter->GetGroup() == _bot->GetGroup())
            {
                TC_LOG_DEBUG("playerbot", "GroupInvitationHandler: Invitation loop detected - inviter in our group");
                return false;
            }
        }
    }

    return true;
}

bool GroupInvitationHandler::IsInviterInRange(Player* inviter) const
{
    if (!inviter || !_bot)
        return false;

    // Check if on same map
    if (_bot->GetMapId() != inviter->GetMapId())
        return false;

    // Check distance
    float distance = _bot->GetDistance(inviter);
    if (distance > _maxAcceptRange)
    {
        TC_LOG_DEBUG("playerbot", "GroupInvitationHandler: Inviter {} is too far ({}y > {}y)",
            inviter->GetName(), distance, _maxAcceptRange);
        return false;
    }

    return true;
}

void GroupInvitationHandler::LogInvitationEvent(std::string const& action, ObjectGuid inviterGuid, std::string const& reason) const
{
    Player* inviter = ObjectAccessor::FindPlayer(inviterGuid);
    std::string inviterName = inviter ? inviter->GetName() : inviterGuid.ToString();

    if (reason.empty())
    {
        TC_LOG_INFO("playerbot.group", "Bot {} {} invitation from {}",
            _bot->GetName(), action, inviterName);
    }
    else
    {
        TC_LOG_INFO("playerbot.group", "Bot {} {} invitation from {} ({})",
            _bot->GetName(), action, inviterName, reason);
    }
}

void GroupInvitationHandler::UpdateStatistics(bool accepted, std::chrono::milliseconds responseTime)
{
    // Update average response time with moving average
    if (_stats.acceptedInvitations + _stats.declinedInvitations > 0)
    {
        uint32 totalResponses = _stats.acceptedInvitations + _stats.declinedInvitations;
        _stats.averageResponseTime = std::chrono::milliseconds(
            (_stats.averageResponseTime.count() * (totalResponses - 1) + responseTime.count()) / totalResponses
        );
    }
    else
    {
        _stats.averageResponseTime = responseTime;
    }
}

bool GroupInvitationHandler::ProcessNextInvitation()
{
    std::lock_guard<std::mutex> lock(_invitationMutex);

    // Check if we're already processing an invitation
    if (!_currentInviter.IsEmpty())
    {
        // Check if enough time has passed for response delay
        auto now = std::chrono::steady_clock::now();
        auto timeSinceInvite = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - _stats.lastInvitation);

        if (timeSinceInvite.count() >= _responseDelayMs)
        {
            // Time to accept the invitation
            if (ShouldAcceptInvitation(_currentInviter))
            {
                AcceptInvitation(_currentInviter);
            }
            else
            {
                DeclineInvitation(_currentInviter, "Validation failed");
            }
            _currentInviter = ObjectGuid::Empty;
        }
        return true;
    }

    // Process next invitation from queue
    if (_pendingInvitations.empty())
        return false;

    PendingInvitation& invitation = _pendingInvitations.front();

    // Check if invitation has expired
    auto now = std::chrono::steady_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - invitation.timestamp);

    if (age.count() > INVITATION_TIMEOUT)
    {
        TC_LOG_DEBUG("playerbot", "GroupInvitationHandler: Invitation from {} expired",
            invitation.inviterName);
        _pendingInvitations.pop();
        _stats.invalidInvitations++;
        return ProcessNextInvitation(); // Try next one
    }

    // Start processing this invitation
    _currentInviter = invitation.inviterGuid;
    _pendingInvitations.pop();

    TC_LOG_DEBUG("playerbot", "GroupInvitationHandler: Processing invitation from {} for bot {}",
        invitation.inviterName, _bot->GetName());

    return true;
}

void GroupInvitationHandler::CleanupExpiredInvitations()
{
    std::lock_guard<std::mutex> lock(_invitationMutex);

    // Clean up expired invitations from queue
    std::queue<PendingInvitation> validInvitations;
    auto now = std::chrono::steady_clock::now();

    while (!_pendingInvitations.empty())
    {
        PendingInvitation& invitation = _pendingInvitations.front();
        auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - invitation.timestamp);

        if (age.count() <= INVITATION_TIMEOUT)
        {
            validInvitations.push(invitation);
        }
        else
        {
            TC_LOG_DEBUG("playerbot", "GroupInvitationHandler: Cleaned up expired invitation from {}",
                invitation.inviterName);
            _stats.invalidInvitations++;
        }

        _pendingInvitations.pop();
    }

    _pendingInvitations = validInvitations;

    // Clean up old inviters from recent list
    auto cutoffTime = now - std::chrono::milliseconds(RECENT_INVITER_MEMORY);
    if (_lastAcceptTime < cutoffTime)
    {
        _recentInviters.clear();
    }
}

} // namespace Playerbot