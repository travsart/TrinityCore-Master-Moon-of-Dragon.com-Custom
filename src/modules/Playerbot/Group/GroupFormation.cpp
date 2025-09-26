/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "GroupFormation.h"
#include "Player.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include <algorithm>
#include <cmath>

namespace Playerbot
{

// Static template initialization
std::unordered_map<FormationType, FormationTemplate> GroupFormation::_formationTemplates;

GroupFormation::GroupFormation(uint32 groupId, FormationType type)
    : _groupId(groupId)
    , _formationType(type)
    , _behavior(FormationBehavior::FLEXIBLE)
    , _leaderGuid(0)
    , _formationDirection(0.0f)
    , _formationSpacing(DEFAULT_FORMATION_SPACING)
    , _formationRadius(0.0f)
    , _lastDirection(0.0f)
    , _lastUpdateTime(getMSTime())
{
    // Initialize formation templates if not done
    if (_formationTemplates.empty())
        InitializeFormationTemplates();

    _formationCenter = Position();
    _lastCenterPosition = Position();
    _metrics.Reset();

    TC_LOG_DEBUG("playerbot", "GroupFormation: Created formation for group %u with type %u",
                 groupId, static_cast<uint8>(type));
}

void GroupFormation::SetFormationType(FormationType type)
{
    std::lock_guard<std::mutex> lock(_formationMutex);

    if (_formationType == type)
        return;

    _formationType = type;
    RecalculateFormationPositions();

    TC_LOG_DEBUG("playerbot", "GroupFormation: Formation type changed to %u for group %u",
                 static_cast<uint8>(type), _groupId);
}

void GroupFormation::SetFormationBehavior(FormationBehavior behavior)
{
    std::lock_guard<std::mutex> lock(_formationMutex);
    _behavior = behavior;

    // Adjust spacing based on behavior
    switch (behavior)
    {
        case FormationBehavior::RIGID:
            _formationSpacing = DEFAULT_FORMATION_SPACING * 0.8f;
            break;
        case FormationBehavior::FLEXIBLE:
            _formationSpacing = DEFAULT_FORMATION_SPACING;
            break;
        case FormationBehavior::COMBAT_READY:
            _formationSpacing = DEFAULT_FORMATION_SPACING * 1.2f;
            break;
        case FormationBehavior::TRAVEL_MODE:
            _formationSpacing = DEFAULT_FORMATION_SPACING * 0.6f;
            break;
        case FormationBehavior::STEALTH_MODE:
            _formationSpacing = DEFAULT_FORMATION_SPACING * 0.5f;
            break;
        case FormationBehavior::DEFENSIVE_MODE:
            _formationSpacing = DEFAULT_FORMATION_SPACING * 1.5f;
            break;
    }

    RecalculateFormationPositions();
}

void GroupFormation::SetCustomFormation(const std::vector<Position>& positions)
{
    std::lock_guard<std::mutex> lock(_formationMutex);

    if (positions.size() != _members.size())
    {
        TC_LOG_WARN("playerbot", "GroupFormation: Custom formation position count (%u) doesn't match member count (%u)",
                    static_cast<uint32>(positions.size()), static_cast<uint32>(_members.size()));
        return;
    }

    for (size_t i = 0; i < std::min(positions.size(), _members.size()); ++i)
    {
        _members[i].assignedPosition = positions[i];
    }

    _formationType = FormationType::CUSTOM_FORMATION;
}

void GroupFormation::AddMember(uint32 memberGuid, const Position& preferredPosition)
{
    std::lock_guard<std::mutex> lock(_formationMutex);

    // Check if member already exists
    for (const auto& member : _members)
    {
        if (member.memberGuid == memberGuid)
            return;
    }

    Position assignedPos = preferredPosition;
    if (assignedPos.GetExactDist(0, 0, 0) < 0.1f) // No preferred position
    {
        // Calculate default position based on formation type
        assignedPos = CalculateMemberPosition(FormationMember(memberGuid, Position()));
    }

    _members.emplace_back(memberGuid, assignedPos);

    // Set first member as leader if no leader assigned
    if (_leaderGuid == 0)
        _leaderGuid = memberGuid;

    RecalculateFormationPositions();

    TC_LOG_DEBUG("playerbot", "GroupFormation: Added member %u to formation, total members: %u",
                 memberGuid, static_cast<uint32>(_members.size()));
}

void GroupFormation::RemoveMember(uint32 memberGuid)
{
    std::lock_guard<std::mutex> lock(_formationMutex);

    _members.erase(
        std::remove_if(_members.begin(), _members.end(),
                      [memberGuid](const FormationMember& member) {
                          return member.memberGuid == memberGuid;
                      }),
        _members.end()
    );

    // Reassign leader if removed
    if (_leaderGuid == memberGuid && !_members.empty())
        _leaderGuid = _members[0].memberGuid;

    RecalculateFormationPositions();

    TC_LOG_DEBUG("playerbot", "GroupFormation: Removed member %u from formation, remaining members: %u",
                 memberGuid, static_cast<uint32>(_members.size()));
}

void GroupFormation::UpdateFormation(const Position& centerPosition, float direction)
{
    std::lock_guard<std::mutex> lock(_formationMutex);

    bool positionChanged = _formationCenter.GetExactDist(centerPosition) > FORMATION_UPDATE_THRESHOLD;
    bool directionChanged = std::abs(_formationDirection - direction) > 0.1f;

    if (positionChanged || directionChanged)
    {
        _lastCenterPosition = _formationCenter;
        _formationCenter = centerPosition;
        _lastDirection = _formationDirection;
        _formationDirection = direction;

        UpdateMemberPositions();
        _metrics.positionAdjustments.fetch_add(1);
    }

    _lastUpdateTime = getMSTime();
}

Position GroupFormation::GetAssignedPosition(uint32 memberGuid) const
{
    std::lock_guard<std::mutex> lock(_formationMutex);

    for (const auto& member : _members)
    {
        if (member.memberGuid == memberGuid)
            return member.assignedPosition;
    }

    return Position();
}

Position GroupFormation::GetFormationCenter() const
{
    std::lock_guard<std::mutex> lock(_formationMutex);
    return _formationCenter;
}

float GroupFormation::GetFormationRadius() const
{
    std::lock_guard<std::mutex> lock(_formationMutex);
    return _formationRadius;
}

bool GroupFormation::IsInFormation(uint32 memberGuid, float tolerance) const
{
    std::lock_guard<std::mutex> lock(_formationMutex);

    for (const auto& member : _members)
    {
        if (member.memberGuid == memberGuid)
        {
            if (Player* player = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(member.memberGuid)))
            {
                float distance = member.assignedPosition.GetExactDist(player->GetPosition());
                return distance <= tolerance;
            }
        }
    }

    return false;
}

std::vector<uint32> GroupFormation::GetMembersOutOfPosition(float tolerance) const
{
    std::lock_guard<std::mutex> lock(_formationMutex);
    std::vector<uint32> outOfPosition;

    for (const auto& member : _members)
    {
        if (Player* player = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(member.memberGuid)))
        {
            float distance = member.assignedPosition.GetExactDist(player->GetPosition());
            if (distance > tolerance)
                outOfPosition.push_back(member.memberGuid);
        }
    }

    return outOfPosition;
}

void GroupFormation::Update(uint32 diff)
{
    if (!_isActive.load())
        return;

    uint32 currentTime = getMSTime();

    // Update formation metrics
    if (currentTime - _lastUpdateTime >= FORMATION_SMOOTHING_INTERVAL)
    {
        UpdateMetrics();
        PerformFormationSmoothing();
        _lastUpdateTime = currentTime;
    }

    // Handle collision resolution
    HandleCollisionResolution();

    // Apply flexibility adjustments
    ApplyFlexibilityAdjustments();
}

void GroupFormation::UpdateMetrics()
{
    std::lock_guard<std::mutex> lock(_formationMutex);

    if (_members.empty())
        return;

    float totalDeviation = 0.0f;
    uint32 validMembers = 0;

    for (const auto& member : _members)
    {
        if (Player* player = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(member.memberGuid)))
        {
            float deviation = member.assignedPosition.GetExactDist(player->GetPosition());
            totalDeviation += deviation;
            validMembers++;
        }
    }

    if (validMembers > 0)
    {
        float avgDeviation = totalDeviation / validMembers;
        _metrics.averageDeviation.store(avgDeviation);

        // Calculate formation stability (inverse of deviation)
        float stability = std::max(0.0f, 1.0f - (avgDeviation / MAX_FORMATION_DEVIATION));
        _metrics.formationStability.store(stability);
    }

    _metrics.lastUpdate = std::chrono::steady_clock::now();
}

bool GroupFormation::IsFormationValid() const
{
    std::lock_guard<std::mutex> lock(_formationMutex);

    if (_members.empty())
        return false;

    // Check if formation integrity is maintained
    float coherence = CalculateFormationCoherence();
    return coherence >= MIN_COORDINATION_EFFICIENCY;
}

float GroupFormation::CalculateFormationCoherence() const
{
    if (_members.empty())
        return 0.0f;

    uint32 membersInPosition = 0;

    for (const auto& member : _members)
    {
        if (IsInFormation(member.memberGuid, FORMATION_TOLERANCE))
            membersInPosition++;
    }

    return static_cast<float>(membersInPosition) / _members.size();
}

float GroupFormation::CalculateFormationEfficiency() const
{
    float coherence = CalculateFormationCoherence();
    float stability = _metrics.formationStability.load();
    float movementEff = _metrics.movementEfficiency.load();

    return (coherence + stability + movementEff) / 3.0f;
}

void GroupFormation::InitializeFormationTemplates()
{
    // Line Formation
    FormationTemplate lineTemplate(FormationType::LINE_FORMATION, "Line Formation", 3.0f);
    lineTemplate.description = "Members arranged in a single line";
    lineTemplate.relativePositions = {
        Position(0, 0, 0),     // Leader
        Position(-3, 0, 0),    // Left
        Position(3, 0, 0),     // Right
        Position(-6, 0, 0),    // Far left
        Position(6, 0, 0)      // Far right
    };
    _formationTemplates[FormationType::LINE_FORMATION] = lineTemplate;

    // Wedge Formation
    FormationTemplate wedgeTemplate(FormationType::WEDGE_FORMATION, "Wedge Formation", 4.0f);
    wedgeTemplate.description = "V-shaped formation for advancing";
    wedgeTemplate.relativePositions = {
        Position(0, 0, 0),     // Leader at point
        Position(-2, -3, 0),   // Left wing
        Position(2, -3, 0),    // Right wing
        Position(-4, -6, 0),   // Left rear
        Position(4, -6, 0)     // Right rear
    };
    _formationTemplates[FormationType::WEDGE_FORMATION] = wedgeTemplate;

    // Circle Formation
    FormationTemplate circleTemplate(FormationType::CIRCLE_FORMATION, "Circle Formation", 5.0f);
    circleTemplate.description = "Defensive circle formation";
    circleTemplate.supportsDynamicSize = true;
    _formationTemplates[FormationType::CIRCLE_FORMATION] = circleTemplate;

    // Diamond Formation
    FormationTemplate diamondTemplate(FormationType::DIAMOND_FORMATION, "Diamond Formation", 4.0f);
    diamondTemplate.description = "Diamond shaped formation";
    diamondTemplate.relativePositions = {
        Position(0, 4, 0),     // Front
        Position(-3, 0, 0),    // Left
        Position(3, 0, 0),     // Right
        Position(0, -4, 0),    // Rear
        Position(0, 0, 0)      // Center
    };
    _formationTemplates[FormationType::DIAMOND_FORMATION] = diamondTemplate;

    // Add other formation templates...

    TC_LOG_INFO("playerbot", "GroupFormation: Initialized %u formation templates",
                static_cast<uint32>(_formationTemplates.size()));
}

FormationTemplate GroupFormation::GetFormationTemplate(FormationType type)
{
    if (_formationTemplates.empty())
        InitializeFormationTemplates();

    auto it = _formationTemplates.find(type);
    if (it != _formationTemplates.end())
        return it->second;

    // Return loose formation as default
    return _formationTemplates[FormationType::LOOSE_FORMATION];
}

void GroupFormation::RecalculateFormationPositions()
{
    if (_members.empty())
        return;

    std::vector<Position> positions;
    uint32 memberCount = static_cast<uint32>(_members.size());

    switch (_formationType)
    {
        case FormationType::LINE_FORMATION:
            positions = GenerateLineFormation(memberCount, _formationSpacing);
            break;
        case FormationType::WEDGE_FORMATION:
            positions = GenerateWedgeFormation(memberCount, _formationSpacing);
            break;
        case FormationType::CIRCLE_FORMATION:
            positions = GenerateCircleFormation(memberCount, _formationSpacing);
            break;
        case FormationType::DIAMOND_FORMATION:
            positions = GenerateDiamondFormation(memberCount, _formationSpacing);
            break;
        case FormationType::DEFENSIVE_SQUARE:
            positions = GenerateDefensiveSquare(memberCount, _formationSpacing);
            break;
        case FormationType::ARROW_FORMATION:
            positions = GenerateArrowFormation(memberCount, _formationSpacing);
            break;
        case FormationType::LOOSE_FORMATION:
        default:
            positions = GenerateLooseFormation(memberCount, _formationSpacing);
            break;
    }

    // Assign positions to members
    for (size_t i = 0; i < std::min(positions.size(), _members.size()); ++i)
    {
        _members[i].assignedPosition = positions[i];
    }

    // Calculate formation radius
    _formationRadius = 0.0f;
    for (const auto& pos : positions)
    {
        float distance = pos.GetExactDist(0, 0, 0);
        _formationRadius = std::max(_formationRadius, distance);
    }
}

Position GroupFormation::CalculateMemberPosition(const FormationMember& member) const
{
    // Transform relative position to world position
    Position relativePos = member.assignedPosition;

    // Apply formation rotation
    float cos_rot = std::cos(_formationDirection);
    float sin_rot = std::sin(_formationDirection);

    float rotatedX = relativePos.GetPositionX() * cos_rot - relativePos.GetPositionY() * sin_rot;
    float rotatedY = relativePos.GetPositionX() * sin_rot + relativePos.GetPositionY() * cos_rot;

    return Position(_formationCenter.GetPositionX() + rotatedX,
                   _formationCenter.GetPositionY() + rotatedY,
                   _formationCenter.GetPositionZ() + relativePos.GetPositionZ());
}

void GroupFormation::UpdateMemberPositions()
{
    for (auto& member : _members)
    {
        member.assignedPosition = CalculateMemberPosition(member);
        member.lastPositionUpdate = getMSTime();
    }
}

std::vector<Position> GroupFormation::GenerateLineFormation(uint32 memberCount, float spacing) const
{
    std::vector<Position> positions;

    if (memberCount == 0)
        return positions;

    // Leader at center
    positions.emplace_back(0, 0, 0);

    // Arrange others in line
    for (uint32 i = 1; i < memberCount; ++i)
    {
        float offset = ((i + 1) / 2) * spacing;
        if (i % 2 == 0) // Even indices go left
            offset = -offset;

        positions.emplace_back(offset, 0, 0);
    }

    return positions;
}

std::vector<Position> GroupFormation::GenerateCircleFormation(uint32 memberCount, float spacing) const
{
    std::vector<Position> positions;

    if (memberCount == 0)
        return positions;

    if (memberCount == 1)
    {
        positions.emplace_back(0, 0, 0);
        return positions;
    }

    float radius = spacing * memberCount / (2.0f * M_PI);
    radius = std::max(radius, 3.0f); // Minimum radius

    for (uint32 i = 0; i < memberCount; ++i)
    {
        float angle = (2.0f * M_PI * i) / memberCount;
        float x = radius * std::cos(angle);
        float y = radius * std::sin(angle);
        positions.emplace_back(x, y, 0);
    }

    return positions;
}

std::vector<Position> GroupFormation::GenerateLooseFormation(uint32 memberCount, float spacing) const
{
    std::vector<Position> positions;

    if (memberCount == 0)
        return positions;

    // Leader at center
    positions.emplace_back(0, 0, 0);

    // Arrange others in a loose pattern
    for (uint32 i = 1; i < memberCount; ++i)
    {
        // Spiral pattern for loose formation
        float angle = i * 2.4f; // Golden angle approximation
        float radius = std::sqrt(static_cast<float>(i)) * spacing * 0.8f;

        float x = radius * std::cos(angle);
        float y = radius * std::sin(angle);
        positions.emplace_back(x, y, 0);
    }

    return positions;
}

// Placeholder implementations for other formation generators
std::vector<Position> GroupFormation::GenerateWedgeFormation(uint32 memberCount, float spacing) const
{
    // TODO: Implement wedge formation algorithm
    return GenerateLooseFormation(memberCount, spacing);
}

std::vector<Position> GroupFormation::GenerateDiamondFormation(uint32 memberCount, float spacing) const
{
    // TODO: Implement diamond formation algorithm
    return GenerateLooseFormation(memberCount, spacing);
}

std::vector<Position> GroupFormation::GenerateDefensiveSquare(uint32 memberCount, float spacing) const
{
    // TODO: Implement defensive square algorithm
    return GenerateLooseFormation(memberCount, spacing);
}

std::vector<Position> GroupFormation::GenerateArrowFormation(uint32 memberCount, float spacing) const
{
    // TODO: Implement arrow formation algorithm
    return GenerateLooseFormation(memberCount, spacing);
}

void GroupFormation::PerformFormationSmoothing()
{
    // Implement formation smoothing logic
    std::lock_guard<std::mutex> lock(_formationMutex);

    // Adjust positions for smoother transitions
    for (auto& member : _members)
    {
        if (member.isFlexible)
        {
            // Allow small adjustments for flexibility
            member.maxDeviationDistance = _formationSpacing * 0.3f;
        }
    }
}

void GroupFormation::HandleCollisionResolution()
{
    // TODO: Implement collision resolution logic
    // This would handle cases where formation positions overlap with terrain or other objects
}

void GroupFormation::ApplyFlexibilityAdjustments()
{
    // TODO: Implement flexibility adjustments
    // This would allow flexible members to adjust their positions based on current conditions
}

} // namespace Playerbot