/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "RoleAssignment.h"
#include "Player.h"
#include "Group.h"
#include "GroupMgr.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include <algorithm>

namespace Playerbot
{

RoleAssignment* RoleAssignment::instance()
{
    static RoleAssignment _instance;
    return &_instance;
}

RoleAssignment::RoleAssignment()
{
    InitializeClassRoleMappings();
    _globalStatistics.Reset();

    TC_LOG_INFO("playerbot", "RoleAssignment: Initialized role assignment system");
}

bool RoleAssignment::AssignRoles(Group* group, RoleAssignmentStrategy strategy)
{
    if (!group)
        return false;

    std::lock_guard<std::mutex> lock(_assignmentMutex);

    uint32 groupId = group->GetGUID().GetCounter();
    _groupStrategies[groupId] = strategy;

    TC_LOG_DEBUG("playerbot", "RoleAssignment: Assigning roles for group %u with strategy %u",
                 groupId, static_cast<uint8>(strategy));

    // Analyze all group members
    for (GroupReference const& itr : group->GetMembers())
    {
        if (Player* member = itr.GetSource())
        {
            PlayerRoleProfile profile = AnalyzePlayerCapabilities(member);
            _playerProfiles[member->GetGUID().GetCounter()] = profile;
        }
    }

    // Execute assignment strategy
    switch (strategy)
    {
        case RoleAssignmentStrategy::OPTIMAL:
            ExecuteOptimalStrategy(group);
            break;
        case RoleAssignmentStrategy::BALANCED:
            ExecuteBalancedStrategy(group);
            break;
        case RoleAssignmentStrategy::FLEXIBLE:
            ExecuteFlexibleStrategy(group);
            break;
        case RoleAssignmentStrategy::STRICT:
            ExecuteStrictStrategy(group);
            break;
        case RoleAssignmentStrategy::HYBRID_FRIENDLY:
            ExecuteHybridStrategy(group);
            break;
        case RoleAssignmentStrategy::DUNGEON_FOCUSED:
            ExecuteDungeonStrategy(group);
            break;
        case RoleAssignmentStrategy::RAID_FOCUSED:
            ExecuteRaidStrategy(group);
            break;
        case RoleAssignmentStrategy::PVP_FOCUSED:
            ExecutePvPStrategy(group);
            break;
        default:
            ExecuteOptimalStrategy(group);
            break;
    }

    // Validate final assignment
    bool isValid = ValidateRoleAssignment(group);

    _globalStatistics.totalAssignments.fetch_add(1);
    if (isValid)
        _globalStatistics.successfulAssignments.fetch_add(1);

    return isValid;
}

bool RoleAssignment::AssignRole(uint32 playerGuid, GroupRole role, Group* group)
{
    if (!group)
        return false;

    std::lock_guard<std::mutex> lock(_assignmentMutex);

    Player* player = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(playerGuid));
    if (!player)
        return false;

    // Update player profile
    auto it = _playerProfiles.find(playerGuid);
    if (it != _playerProfiles.end())
    {
        it->second.assignedRole = role;
        it->second.lastRoleUpdate = getMSTime();
    }
    else
    {
        PlayerRoleProfile profile = AnalyzePlayerCapabilities(player);
        profile.assignedRole = role;
        _playerProfiles[playerGuid] = profile;
    }

    NotifyRoleAssignment(player, role, group);

    TC_LOG_DEBUG("playerbot", "RoleAssignment: Assigned role %u to player %s in group %u",
                 static_cast<uint8>(role), player->GetName().c_str(), group->GetGUID().GetCounter());

    return true;
}

bool RoleAssignment::SwapRoles(uint32 player1Guid, uint32 player2Guid, Group* group)
{
    if (!group)
        return false;

    std::lock_guard<std::mutex> lock(_assignmentMutex);

    auto it1 = _playerProfiles.find(player1Guid);
    auto it2 = _playerProfiles.find(player2Guid);

    if (it1 == _playerProfiles.end() || it2 == _playerProfiles.end())
        return false;

    GroupRole role1 = it1->second.assignedRole;
    GroupRole role2 = it2->second.assignedRole;

    // Check if players can perform swapped roles
    if (!CanPlayerSwitchRole(ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(player1Guid)), role2, group) ||
        !CanPlayerSwitchRole(ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(player2Guid)), role1, group))
    {
        return false;
    }

    // Perform the swap
    it1->second.assignedRole = role2;
    it2->second.assignedRole = role1;

    TC_LOG_DEBUG("playerbot", "RoleAssignment: Swapped roles between players %u and %u in group %u",
                 player1Guid, player2Guid, group->GetGUID().GetCounter());

    return true;
}

PlayerRoleProfile RoleAssignment::AnalyzePlayerCapabilities(Player* player)
{
    if (!player)
        return PlayerRoleProfile(0, 0, 0, 0);

    PlayerRoleProfile profile(player->GetGUID().GetCounter(), player->getClass(), 0, player->GetLevel());

    BuildPlayerProfile(profile, player);
    CalculateRoleCapabilities(profile, player);
    AnalyzePlayerGear(profile, player);
    UpdateRoleExperience(profile, player);

    return profile;
}

std::vector<RoleScore> RoleAssignment::CalculateRoleScores(Player* player, Group* group)
{
    std::vector<RoleScore> scores;

    if (!player)
        return scores;

    // Calculate scores for all possible roles
    for (int roleInt = 0; roleInt <= static_cast<int>(GroupRole::UTILITY); ++roleInt)
    {
        GroupRole role = static_cast<GroupRole>(roleInt);
        RoleScore score(role);

        score.effectiveness = CalculateClassRoleEffectiveness(player->getClass(), 0, role);
        score.gearScore = CalculateGearScore(player, role);
        score.experienceScore = CalculateExperienceScore(player->GetGUID().GetCounter(), role);
        score.synergy = CalculateSynergyScore(player, role, group);
        score.availabilityScore = 1.0f; // Default availability

        score.CalculateTotalScore();
        scores.push_back(score);
    }

    // Sort by total score (highest first)
    std::sort(scores.begin(), scores.end(),
              [](const RoleScore& a, const RoleScore& b) {
                  return a.totalScore > b.totalScore;
              });

    return scores;
}

GroupRole RoleAssignment::RecommendRole(Player* player, Group* group)
{
    if (!player)
        return GroupRole::NONE;

    std::vector<RoleScore> scores = CalculateRoleScores(player, group);

    if (!scores.empty())
        return scores[0].role;

    return GroupRole::NONE;
}

GroupComposition RoleAssignment::AnalyzeGroupComposition(Group* group)
{
    GroupComposition composition;

    if (!group)
        return composition;

    std::lock_guard<std::mutex> lock(_assignmentMutex);

    // Count assigned roles
    for (GroupReference const& itr : group->GetMembers())
    {
        if (Player* member = itr.GetSource())
        {
            uint32 memberGuid = member->GetGUID().GetCounter();
            auto profileIt = _playerProfiles.find(memberGuid);

            if (profileIt != _playerProfiles.end())
            {
                GroupRole assignedRole = profileIt->second.assignedRole;
                composition.roleAssignments[assignedRole].push_back(memberGuid);
                composition.roleFulfillment[assignedRole]++;
            }

            composition.totalMembers++;
        }
    }

    // Check for main tank and healer
    composition.hasMainTank = composition.roleFulfillment[GroupRole::TANK] > 0;
    composition.hasMainHealer = composition.roleFulfillment[GroupRole::HEALER] > 0;
    composition.dpsCount = composition.roleFulfillment[GroupRole::MELEE_DPS] +
                          composition.roleFulfillment[GroupRole::RANGED_DPS];

    // Calculate composition score
    composition.compositionScore = CalculateCompositionScore(composition);
    composition.isValid = IsCompositionViable(composition);

    return composition;
}

bool RoleAssignment::IsCompositionViable(const GroupComposition& composition)
{
    // Basic viability checks
    if (!composition.hasMainTank || !composition.hasMainHealer)
        return false;

    if (composition.dpsCount == 0)
        return false;

    if (composition.totalMembers < 3 || composition.totalMembers > 25)
        return false;

    return composition.compositionScore >= COMPOSITION_SCORE_THRESHOLD;
}

std::vector<GroupRole> RoleAssignment::GetMissingRoles(Group* group)
{
    std::vector<GroupRole> missingRoles;

    GroupComposition composition = AnalyzeGroupComposition(group);

    for (const auto& [role, required] : composition.roleRequirements)
    {
        uint32 fulfilled = composition.roleFulfillment[role];
        if (fulfilled < required)
        {
            for (uint32 i = fulfilled; i < required; ++i)
            {
                missingRoles.push_back(role);
            }
        }
    }

    return missingRoles;
}

bool RoleAssignment::CanPlayerSwitchRole(Player* player, GroupRole newRole, Group* group)
{
    if (!player)
        return false;

    uint32 playerGuid = player->GetGUID().GetCounter();
    auto profileIt = _playerProfiles.find(playerGuid);

    if (profileIt == _playerProfiles.end())
        return false;

    // Check role capability
    auto capabilityIt = profileIt->second.roleCapabilities.find(newRole);
    if (capabilityIt == profileIt->second.roleCapabilities.end())
        return false;

    RoleCapability capability = capabilityIt->second;
    if (capability == RoleCapability::INCAPABLE)
        return false;

    // Check if role switch is on cooldown
    uint32 lastUpdate = profileIt->second.lastRoleUpdate;
    if (getMSTime() - lastUpdate < ROLE_SWITCH_COOLDOWN)
        return false;

    return true;
}

void RoleAssignment::Update(uint32 diff)
{
    static uint32 lastUpdate = 0;

    if (getMSTime() - lastUpdate < PROFILE_UPDATE_INTERVAL)
        return;

    RefreshPlayerProfiles();
    CleanupInactiveProfiles();
    UpdateRoleStatistics();

    lastUpdate = getMSTime();
}

void RoleAssignment::InitializeClassRoleMappings()
{
    // Death Knight
    _classSpecRoles[CLASS_DEATH_KNIGHT][0] = { // Blood
        {GroupRole::TANK, RoleCapability::PRIMARY},
        {GroupRole::MELEE_DPS, RoleCapability::SECONDARY}
    };
    _classSpecRoles[CLASS_DEATH_KNIGHT][1] = { // Frost
        {GroupRole::MELEE_DPS, RoleCapability::PRIMARY},
        {GroupRole::TANK, RoleCapability::EMERGENCY}
    };
    _classSpecRoles[CLASS_DEATH_KNIGHT][2] = { // Unholy
        {GroupRole::MELEE_DPS, RoleCapability::PRIMARY}
    };

    // Warrior
    _classSpecRoles[CLASS_WARRIOR][0] = { // Arms
        {GroupRole::MELEE_DPS, RoleCapability::PRIMARY},
        {GroupRole::TANK, RoleCapability::EMERGENCY}
    };
    _classSpecRoles[CLASS_WARRIOR][1] = { // Fury
        {GroupRole::MELEE_DPS, RoleCapability::PRIMARY}
    };
    _classSpecRoles[CLASS_WARRIOR][2] = { // Protection
        {GroupRole::TANK, RoleCapability::PRIMARY},
        {GroupRole::MELEE_DPS, RoleCapability::SECONDARY}
    };

    // Paladin
    _classSpecRoles[CLASS_PALADIN][0] = { // Holy
        {GroupRole::HEALER, RoleCapability::PRIMARY},
        {GroupRole::SUPPORT, RoleCapability::SECONDARY}
    };
    _classSpecRoles[CLASS_PALADIN][1] = { // Protection
        {GroupRole::TANK, RoleCapability::PRIMARY},
        {GroupRole::HEALER, RoleCapability::EMERGENCY}
    };
    _classSpecRoles[CLASS_PALADIN][2] = { // Retribution
        {GroupRole::MELEE_DPS, RoleCapability::PRIMARY},
        {GroupRole::SUPPORT, RoleCapability::SECONDARY}
    };

    // Hunter
    _classSpecRoles[CLASS_HUNTER][0] = { // Beast Mastery
        {GroupRole::RANGED_DPS, RoleCapability::PRIMARY},
        {GroupRole::UTILITY, RoleCapability::SECONDARY}
    };
    _classSpecRoles[CLASS_HUNTER][1] = { // Marksmanship
        {GroupRole::RANGED_DPS, RoleCapability::PRIMARY}
    };
    _classSpecRoles[CLASS_HUNTER][2] = { // Survival
        {GroupRole::MELEE_DPS, RoleCapability::PRIMARY},
        {GroupRole::UTILITY, RoleCapability::SECONDARY}
    };

    // Rogue
    _classSpecRoles[CLASS_ROGUE][0] = { // Assassination
        {GroupRole::MELEE_DPS, RoleCapability::PRIMARY},
        {GroupRole::UTILITY, RoleCapability::SECONDARY}
    };
    _classSpecRoles[CLASS_ROGUE][1] = { // Combat
        {GroupRole::MELEE_DPS, RoleCapability::PRIMARY}
    };
    _classSpecRoles[CLASS_ROGUE][2] = { // Subtlety
        {GroupRole::MELEE_DPS, RoleCapability::PRIMARY},
        {GroupRole::UTILITY, RoleCapability::SECONDARY}
    };

    // Priest
    _classSpecRoles[CLASS_PRIEST][0] = { // Discipline
        {GroupRole::HEALER, RoleCapability::PRIMARY},
        {GroupRole::SUPPORT, RoleCapability::SECONDARY}
    };
    _classSpecRoles[CLASS_PRIEST][1] = { // Holy
        {GroupRole::HEALER, RoleCapability::PRIMARY}
    };
    _classSpecRoles[CLASS_PRIEST][2] = { // Shadow
        {GroupRole::RANGED_DPS, RoleCapability::PRIMARY},
        {GroupRole::UTILITY, RoleCapability::SECONDARY}
    };

    // Shaman
    _classSpecRoles[CLASS_SHAMAN][0] = { // Elemental
        {GroupRole::RANGED_DPS, RoleCapability::PRIMARY},
        {GroupRole::UTILITY, RoleCapability::SECONDARY}
    };
    _classSpecRoles[CLASS_SHAMAN][1] = { // Enhancement
        {GroupRole::MELEE_DPS, RoleCapability::PRIMARY},
        {GroupRole::UTILITY, RoleCapability::SECONDARY}
    };
    _classSpecRoles[CLASS_SHAMAN][2] = { // Restoration
        {GroupRole::HEALER, RoleCapability::PRIMARY},
        {GroupRole::SUPPORT, RoleCapability::SECONDARY}
    };

    // Mage
    _classSpecRoles[CLASS_MAGE][0] = { // Arcane
        {GroupRole::RANGED_DPS, RoleCapability::PRIMARY},
        {GroupRole::UTILITY, RoleCapability::SECONDARY}
    };
    _classSpecRoles[CLASS_MAGE][1] = { // Fire
        {GroupRole::RANGED_DPS, RoleCapability::PRIMARY}
    };
    _classSpecRoles[CLASS_MAGE][2] = { // Frost
        {GroupRole::RANGED_DPS, RoleCapability::PRIMARY},
        {GroupRole::UTILITY, RoleCapability::SECONDARY}
    };

    // Warlock
    _classSpecRoles[CLASS_WARLOCK][0] = { // Affliction
        {GroupRole::RANGED_DPS, RoleCapability::PRIMARY},
        {GroupRole::UTILITY, RoleCapability::SECONDARY}
    };
    _classSpecRoles[CLASS_WARLOCK][1] = { // Demonology
        {GroupRole::RANGED_DPS, RoleCapability::PRIMARY},
        {GroupRole::SUPPORT, RoleCapability::SECONDARY}
    };
    _classSpecRoles[CLASS_WARLOCK][2] = { // Destruction
        {GroupRole::RANGED_DPS, RoleCapability::PRIMARY}
    };

    // Monk
    _classSpecRoles[CLASS_MONK][0] = { // Brewmaster
        {GroupRole::TANK, RoleCapability::PRIMARY},
        {GroupRole::MELEE_DPS, RoleCapability::SECONDARY}
    };
    _classSpecRoles[CLASS_MONK][1] = { // Mistweaver
        {GroupRole::HEALER, RoleCapability::PRIMARY},
        {GroupRole::SUPPORT, RoleCapability::SECONDARY}
    };
    _classSpecRoles[CLASS_MONK][2] = { // Windwalker
        {GroupRole::MELEE_DPS, RoleCapability::PRIMARY},
        {GroupRole::UTILITY, RoleCapability::SECONDARY}
    };

    // Druid
    _classSpecRoles[CLASS_DRUID][0] = { // Balance
        {GroupRole::RANGED_DPS, RoleCapability::PRIMARY},
        {GroupRole::UTILITY, RoleCapability::SECONDARY}
    };
    _classSpecRoles[CLASS_DRUID][1] = { // Feral
        {GroupRole::MELEE_DPS, RoleCapability::PRIMARY},
        {GroupRole::TANK, RoleCapability::HYBRID}
    };
    _classSpecRoles[CLASS_DRUID][2] = { // Guardian
        {GroupRole::TANK, RoleCapability::PRIMARY},
        {GroupRole::MELEE_DPS, RoleCapability::SECONDARY}
    };
    _classSpecRoles[CLASS_DRUID][3] = { // Restoration
        {GroupRole::HEALER, RoleCapability::PRIMARY},
        {GroupRole::SUPPORT, RoleCapability::SECONDARY}
    };

    // Demon Hunter
    _classSpecRoles[CLASS_DEMON_HUNTER][0] = { // Havoc
        {GroupRole::MELEE_DPS, RoleCapability::PRIMARY}
    };
    _classSpecRoles[CLASS_DEMON_HUNTER][1] = { // Vengeance
        {GroupRole::TANK, RoleCapability::PRIMARY},
        {GroupRole::MELEE_DPS, RoleCapability::SECONDARY}
    };

    // Evoker
    _classSpecRoles[CLASS_EVOKER][0] = { // Devastation
        {GroupRole::RANGED_DPS, RoleCapability::PRIMARY},
        {GroupRole::UTILITY, RoleCapability::SECONDARY}
    };
    _classSpecRoles[CLASS_EVOKER][1] = { // Preservation
        {GroupRole::HEALER, RoleCapability::PRIMARY},
        {GroupRole::SUPPORT, RoleCapability::SECONDARY}
    };
    _classSpecRoles[CLASS_EVOKER][2] = { // Augmentation
        {GroupRole::SUPPORT, RoleCapability::PRIMARY},
        {GroupRole::RANGED_DPS, RoleCapability::SECONDARY}
    };

    TC_LOG_INFO("playerbot", "RoleAssignment: Initialized class role mappings for all 13 classes");
}

void RoleAssignment::BuildPlayerProfile(PlayerRoleProfile& profile, Player* player)
{
    if (!player)
        return;

    profile.playerClass = player->getClass();
    profile.playerLevel = player->GetLevel();
    profile.lastRoleUpdate = getMSTime();

    // Set default preferences based on class
    profile.preferredRole = DetermineOptimalRole(player, nullptr, RoleAssignmentStrategy::OPTIMAL);
    profile.isFlexible = true;
    profile.overallRating = 5.0f;

    // Initialize alternative roles
    std::vector<RoleScore> scores = CalculateRoleScores(player, nullptr);
    for (size_t i = 1; i < std::min(scores.size(), size_t(3)); ++i)
    {
        if (scores[i].totalScore >= MIN_ROLE_EFFECTIVENESS)
            profile.alternativeRoles.push_back(scores[i].role);
    }
}

void RoleAssignment::CalculateRoleCapabilities(PlayerRoleProfile& profile, Player* player)
{
    if (!player)
        return;

    uint8 playerClass = player->getClass();
    uint8 playerSpec = 0; // TODO: Get actual spec when available

    auto classIt = _classSpecRoles.find(playerClass);
    if (classIt != _classSpecRoles.end())
    {
        auto specIt = classIt->second.find(playerSpec);
        if (specIt != classIt->second.end())
        {
            for (const auto& [role, capability] : specIt->second)
            {
                profile.roleCapabilities[role] = capability;
            }
        }
    }

    // Initialize all roles to incapable by default
    for (int roleInt = 0; roleInt <= static_cast<int>(GroupRole::UTILITY); ++roleInt)
    {
        GroupRole role = static_cast<GroupRole>(roleInt);
        if (profile.roleCapabilities.find(role) == profile.roleCapabilities.end())
        {
            profile.roleCapabilities[role] = RoleCapability::INCAPABLE;
        }
    }
}

float RoleAssignment::CalculateClassRoleEffectiveness(uint8 playerClass, uint8 playerSpec, GroupRole role)
{
    auto classIt = _classSpecRoles.find(playerClass);
    if (classIt == _classSpecRoles.end())
        return 0.0f;

    auto specIt = classIt->second.find(playerSpec);
    if (specIt == classIt->second.end())
        return 0.0f;

    for (const auto& [classRole, capability] : specIt->second)
    {
        if (classRole == role)
        {
            switch (capability)
            {
                case RoleCapability::PRIMARY:
                    return 1.0f;
                case RoleCapability::SECONDARY:
                    return 0.8f;
                case RoleCapability::HYBRID:
                    return 0.9f;
                case RoleCapability::EMERGENCY:
                    return 0.5f;
                default:
                    return 0.0f;
            }
        }
    }

    return 0.0f;
}

float RoleAssignment::CalculateGearScore(Player* player, GroupRole role)
{
    // TODO: Implement gear scoring based on role appropriateness
    // This would analyze equipped items and their stats
    return 0.7f; // Placeholder
}

float RoleAssignment::CalculateExperienceScore(uint32 playerGuid, GroupRole role)
{
    std::lock_guard<std::mutex> lock(_performanceMutex);

    auto playerIt = _rolePerformance.find(playerGuid);
    if (playerIt == _rolePerformance.end())
        return 0.5f; // Default experience

    auto roleIt = playerIt->second.find(role);
    if (roleIt == playerIt->second.end())
        return 0.5f; // No experience in this role

    return roleIt->second.averageEffectiveness.load();
}

float RoleAssignment::CalculateSynergyScore(Player* player, GroupRole role, Group* group)
{
    // TODO: Implement synergy calculation based on group composition
    // This would consider class interactions, buff/debuff synergies, etc.
    return 0.6f; // Placeholder
}

GroupRole RoleAssignment::DetermineOptimalRole(Player* player, Group* group, RoleAssignmentStrategy strategy)
{
    if (!player)
        return GroupRole::NONE;

    std::vector<RoleScore> scores = CalculateRoleScores(player, group);

    if (scores.empty())
        return GroupRole::NONE;

    // Apply strategy-specific filtering
    switch (strategy)
    {
        case RoleAssignmentStrategy::STRICT:
            // Only consider primary roles
            for (const auto& score : scores)
            {
                uint32 playerGuid = player->GetGUID().GetCounter();
                auto profileIt = _playerProfiles.find(playerGuid);
                if (profileIt != _playerProfiles.end())
                {
                    auto capIt = profileIt->second.roleCapabilities.find(score.role);
                    if (capIt != profileIt->second.roleCapabilities.end() &&
                        capIt->second == RoleCapability::PRIMARY)
                    {
                        return score.role;
                    }
                }
            }
            break;

        default:
            // Return highest scoring role
            return scores[0].role;
    }

    return GroupRole::NONE;
}

float RoleAssignment::CalculateCompositionScore(const GroupComposition& composition)
{
    float score = 5.0f; // Base score

    // Tank requirements
    if (composition.hasMainTank)
        score += 2.0f;
    else
        score -= 3.0f;

    // Healer requirements
    if (composition.hasMainHealer)
        score += 2.0f;
    else
        score -= 3.0f;

    // DPS balance
    if (composition.dpsCount >= 2)
        score += 1.0f;
    else if (composition.dpsCount == 1)
        score += 0.5f;
    else
        score -= 2.0f;

    // Group size
    if (composition.totalMembers >= 3 && composition.totalMembers <= 5)
        score += 1.0f;
    else if (composition.totalMembers > 5)
        score -= 0.5f;

    return std::max(0.0f, std::min(10.0f, score));
}

void RoleAssignment::ExecuteOptimalStrategy(Group* group)
{
    if (!group)
        return;

    // Get all members and their optimal roles
    std::vector<std::pair<Player*, GroupRole>> assignments;

    for (GroupReference const& itr : group->GetMembers())
    {
        if (Player* member = itr.GetSource())
        {
            GroupRole optimalRole = DetermineOptimalRole(member, group, RoleAssignmentStrategy::OPTIMAL);
            assignments.emplace_back(member, optimalRole);
        }
    }

    // Sort by role priority (tank > healer > dps)
    std::sort(assignments.begin(), assignments.end(),
              [](const auto& a, const auto& b) {
                  return static_cast<int>(a.second) < static_cast<int>(b.second);
              });

    // Assign roles
    for (const auto& [player, role] : assignments)
    {
        AssignRole(player->GetGUID().GetCounter(), role, group);
    }
}

// Placeholder implementations for other strategies
void RoleAssignment::ExecuteBalancedStrategy(Group* group) { ExecuteOptimalStrategy(group); }
void RoleAssignment::ExecuteFlexibleStrategy(Group* group) { ExecuteOptimalStrategy(group); }
void RoleAssignment::ExecuteStrictStrategy(Group* group) { ExecuteOptimalStrategy(group); }
void RoleAssignment::ExecuteHybridStrategy(Group* group) { ExecuteOptimalStrategy(group); }
void RoleAssignment::ExecuteDungeonStrategy(Group* group) { ExecuteOptimalStrategy(group); }
void RoleAssignment::ExecuteRaidStrategy(Group* group) { ExecuteOptimalStrategy(group); }
void RoleAssignment::ExecutePvPStrategy(Group* group) { ExecuteOptimalStrategy(group); }

void RoleAssignment::AnalyzePlayerGear(PlayerRoleProfile& profile, Player* player)
{
    // TODO: Implement gear analysis for role scoring
}

void RoleAssignment::UpdateRoleExperience(PlayerRoleProfile& profile, Player* player)
{
    // TODO: Update experience scores based on performance history
}

bool RoleAssignment::ValidateRoleAssignment(Group* group)
{
    GroupComposition composition = AnalyzeGroupComposition(group);
    return IsCompositionViable(composition);
}

void RoleAssignment::NotifyRoleAssignment(Player* player, GroupRole role, Group* group)
{
    if (!player)
        return;

    const char* roleNames[] = {
        "Tank", "Healer", "Melee DPS", "Ranged DPS", "Support", "Utility", "None"
    };

    TC_LOG_DEBUG("playerbot", "RoleAssignment: Player %s assigned role %s",
                 player->GetName().c_str(), roleNames[static_cast<int>(role)]);
}

void RoleAssignment::RefreshPlayerProfiles()
{
    // Update profiles for active players
    for (auto& [playerGuid, profile] : _playerProfiles)
    {
        if (Player* player = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(playerGuid)))
        {
            BuildPlayerProfile(profile, player);
        }
    }
}

void RoleAssignment::CleanupInactiveProfiles()
{
    uint32 currentTime = getMSTime();
    const uint32 CLEANUP_THRESHOLD = 24 * 60 * 60 * 1000; // 24 hours

    auto it = _playerProfiles.begin();
    while (it != _playerProfiles.end())
    {
        if (currentTime - it->second.lastRoleUpdate > CLEANUP_THRESHOLD)
        {
            // Check if player is still online
            if (!ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(it->first)))
            {
                it = _playerProfiles.erase(it);
                continue;
            }
        }
        ++it;
    }
}

void RoleAssignment::UpdateRoleStatistics()
{
    _globalStatistics.lastStatsUpdate = std::chrono::steady_clock::now();
}

RoleAssignment::RoleStatistics RoleAssignment::GetGlobalRoleStatistics()
{
    return _globalStatistics;
}

} // namespace Playerbot