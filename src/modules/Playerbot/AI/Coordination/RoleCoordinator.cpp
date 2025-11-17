/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "RoleCoordinator.h"
#include "GroupCoordinator.h"
#include "Group.h"
#include "Player.h"
#include "Unit.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "GameTime.h"

namespace Playerbot
{
namespace Coordination
{

// ============================================================================
// TankCoordinator
// ============================================================================

void TankCoordinator::Update(GroupCoordinator* group, uint32 diff)
{
    if (!group)
        return;

    _lastUpdateTime += diff;
    if (_lastUpdateTime < _updateInterval)
        return;

    _lastUpdateTime = 0;

    UpdateMainTank(group);
    UpdateTankAssignments(group);
    UpdateTauntRotation(group);
}

ObjectGuid TankCoordinator::GetTankForTarget(ObjectGuid targetGuid) const
{
    auto it = _tankAssignments.find(targetGuid);
    if (it != _tankAssignments.end())
        return it->second;

    return ObjectGuid::Empty;
}

void TankCoordinator::RequestTankSwap()
{
    uint32 now = GameTime::GetGameTimeMS();

    // Check cooldown
    if (now < _lastTankSwapTime + _tankSwapCooldown)
    {
        TC_LOG_DEBUG("playerbot.coordination", "Tank swap on cooldown ({}ms remaining)",
            (_lastTankSwapTime + _tankSwapCooldown) - now);
        return;
    }

    // Swap main and off-tank
    ObjectGuid temp = _mainTank;
    _mainTank = _offTank;
    _offTank = temp;

    _lastTankSwapTime = now;

    TC_LOG_DEBUG("playerbot.coordination", "Tank swap executed: Main={}, Off={}",
        _mainTank.ToString(), _offTank.ToString());
}

bool TankCoordinator::NeedsTankSwap(ObjectGuid mainTankGuid) const
{
    Player* mainTank = ObjectAccessor::FindPlayer(mainTankGuid);
    if (!mainTank)
        return false;

    // Swap if main tank health critical (<20%)
    if (mainTank->GetHealthPct() < 20.0f)
        return true;

    // Swap if main tank has high stacks of tank debuff
    // TODO: Check for specific debuff stacks (requires boss mechanics knowledge)

    // Swap if main tank is out of defensive cooldowns
    // TODO: Check defensive cooldown availability

    return false;
}

void TankCoordinator::UpdateMainTank(GroupCoordinator* group)
{
    ::std::vector<ObjectGuid> tanks = group->GetBotsByRole(GroupRole::TANK);

    if (tanks.empty())
    {
        _mainTank = ObjectGuid::Empty;
        _offTank = ObjectGuid::Empty;
        return;
    }

    // If we have a main tank and it's still alive, keep it
    if (!_mainTank.IsEmpty())
    {
        Player* mainTank = ObjectAccessor::FindPlayer(_mainTank);
        if (mainTank && mainTank->IsAlive())
        {
            // Update off-tank
    for (ObjectGuid tankGuid : tanks)
            {
                if (tankGuid != _mainTank)
                {
                    _offTank = tankGuid;
                    break;
                }
            }
            return;
        }
    }

    // Select new main tank (highest max health)
    float highestHealth = 0.0f;
    ObjectGuid bestTank;

    for (ObjectGuid tankGuid : tanks)
    {
        Player* tank = ObjectAccessor::FindPlayer(tankGuid);
        if (!tank || !tank->IsAlive())
            continue;

        float maxHealth = static_cast<float>(tank->GetMaxHealth());
        if (maxHealth > highestHealth)
        {
            highestHealth = maxHealth;
            bestTank = tankGuid;
        }
    }

    if (!bestTank.IsEmpty())
    {
        _mainTank = bestTank;

        // Select off-tank (second highest health)
        float secondHighestHealth = 0.0f;
        ObjectGuid secondBestTank;

        for (ObjectGuid tankGuid : tanks)
        {
            if (tankGuid == _mainTank)
                continue;

            Player* tank = ObjectAccessor::FindPlayer(tankGuid);
            if (!tank || !tank->IsAlive())
                continue;

            float maxHealth = static_cast<float>(tank->GetMaxHealth());
            if (maxHealth > secondHighestHealth)
            {
                secondHighestHealth = maxHealth;
                secondBestTank = tankGuid;
            }
        }

        _offTank = secondBestTank;

        TC_LOG_DEBUG("playerbot.coordination", "Tank roles assigned: Main={}, Off={}",
            _mainTank.ToString(), _offTank.ToString());
    }
}

void TankCoordinator::UpdateTankAssignments(GroupCoordinator* group)
{
    // Clear old assignments
    _tankAssignments.clear();

    if (_mainTank.IsEmpty())
        return;

    Player* mainTank = ObjectAccessor::FindPlayer(_mainTank);
    if (!mainTank || !mainTank->IsInCombat())
        return;

    // Assign main tank to current target
    Unit* mainTarget = mainTank->GetSelectedUnit();
    if (mainTarget && mainTarget->IsAlive())
    {
        _tankAssignments[mainTarget->GetGUID()] = _mainTank;
    }

    // Assign off-tank to adds
    if (!_offTank.IsEmpty())
    {
        Player* offTank = ObjectAccessor::FindPlayer(_offTank);
        if (offTank && offTank->IsInCombat())
        {
            Unit* offTarget = offTank->GetSelectedUnit();
            if (offTarget && offTarget->IsAlive() && offTarget->GetGUID() != mainTarget->GetGUID())
            {
                _tankAssignments[offTarget->GetGUID()] = _offTank;
            }
        }
    }
}

void TankCoordinator::UpdateTauntRotation(GroupCoordinator* group)
{
    // Check if tank swap is needed
    if (!_mainTank.IsEmpty() && NeedsTankSwap(_mainTank))
    {
        RequestTankSwap();
    }
}

// ============================================================================
// HealerCoordinator
// ============================================================================

void HealerCoordinator::Update(GroupCoordinator* group, uint32 diff)
{
    if (!group)
        return;

    _lastUpdateTime += diff;
    if (_lastUpdateTime < _updateInterval)
        return;

    _lastUpdateTime = 0;

    UpdateHealingAssignments(group);
    UpdateDispelCoordination(group);
    UpdateCooldownRotation(group);
    UpdateManaManagement(group);
}

ObjectGuid HealerCoordinator::GetHealerForTank(ObjectGuid tankGuid) const
{
    for (auto const& assignment : _healingAssignments)
    {
        if (assignment.targetGuid == tankGuid && assignment.assignmentType == "tank")
            return assignment.healerGuid;
    }

    return ObjectGuid::Empty;
}

void HealerCoordinator::AssignHealerToTank(ObjectGuid healerGuid, ObjectGuid tankGuid)
{
    // Remove existing assignments for this healer
    _healingAssignments.erase(
        ::std::remove_if(_healingAssignments.begin(), _healingAssignments.end(),
            [healerGuid](HealingAssignment const& assignment) {
                return assignment.healerGuid == healerGuid;
            }),
        _healingAssignments.end()
    );

    // Create new assignment
    HealingAssignment assignment;
    assignment.healerGuid = healerGuid;
    assignment.targetGuid = tankGuid;
    assignment.assignmentType = "tank";
    assignment.priority = 100; // Highest priority

    _healingAssignments.push_back(assignment);

    TC_LOG_DEBUG("playerbot.coordination", "Assigned healer {} to tank {}",
        healerGuid.ToString(), tankGuid.ToString());
}

ObjectGuid HealerCoordinator::GetNextCooldownHealer(::std::string const& cooldownType) const
{
    uint32 now = GameTime::GetGameTimeMS();

    for (auto const& pair : _healerCooldowns)
    {
        ObjectGuid healerGuid = pair.first;
        auto const& cooldowns = pair.second;

        auto it = cooldowns.find(cooldownType);
        if (it == cooldowns.end() || now > it->second)
        {
            // Cooldown available
            return healerGuid;
        }
    }

    return ObjectGuid::Empty;
}

void HealerCoordinator::UseHealingCooldown(ObjectGuid healerGuid, ::std::string const& cooldownType, uint32 durationMs)
{
    uint32 expireTime = GameTime::GetGameTimeMS() + durationMs;
    _healerCooldowns[healerGuid][cooldownType] = expireTime;

    TC_LOG_DEBUG("playerbot.coordination", "Healer {} used cooldown: {} (expires in {}ms)",
        healerGuid.ToString(), cooldownType, durationMs);
}

::std::vector<ObjectGuid> HealerCoordinator::GetResurrectionPriority() const
{
    ::std::vector<ObjectGuid> priorities;

    // Priority order:
    // 1. Healers
    // 2. Tanks
    // 3. Ranged DPS
    // 4. Melee DPS
    for (auto const& assignment : _healingAssignments)
    {
        Player* target = ObjectAccessor::FindPlayer(assignment.targetGuid);
        if (target && !target->IsAlive())
        {
            priorities.push_back(assignment.targetGuid);
        }
    }

    return priorities;
}

void HealerCoordinator::UpdateHealingAssignments(GroupCoordinator* group)
{
    ::std::vector<ObjectGuid> healers = group->GetBotsByRole(GroupRole::HEALER);
    ::std::vector<ObjectGuid> tanks = group->GetBotsByRole(GroupRole::TANK);

    if (healers.empty())
        return;

    // Assign healers to tanks (1:1 ratio if possible)
    size_t healerIndex = 0;
    for (ObjectGuid tankGuid : tanks)
    {
        if (healerIndex >= healers.size())
            break;

        ObjectGuid healerGuid = healers[healerIndex];

        // Check if this tank already has a healer assigned
        bool alreadyAssigned = false;
        for (auto const& assignment : _healingAssignments)
        {
            if (assignment.targetGuid == tankGuid && assignment.assignmentType == "tank")
            {
                alreadyAssigned = true;
                break;
            }
        }

        if (!alreadyAssigned)
        {
            AssignHealerToTank(healerGuid, tankGuid);
            healerIndex++;
        }
    }

    // Remaining healers assigned to raid/group healing
    for (; healerIndex < healers.size(); ++healerIndex)
    {
        ObjectGuid healerGuid = healers[healerIndex];

        // Remove old assignments
        _healingAssignments.erase(
            ::std::remove_if(_healingAssignments.begin(), _healingAssignments.end(),
                [healerGuid](HealingAssignment const& assignment) {
                    return assignment.healerGuid == healerGuid;
                }),
            _healingAssignments.end()
        );

        // Create raid healing assignment
        HealingAssignment assignment;
        assignment.healerGuid = healerGuid;
        assignment.targetGuid = ObjectGuid::Empty; // No specific target
        assignment.assignmentType = "raid";
        assignment.priority = 50;

        _healingAssignments.push_back(assignment);
    }
}

void HealerCoordinator::UpdateDispelCoordination(GroupCoordinator* group)
{
    // Dispel coordination is handled by GroupCoordinator::AssignDispel()
    // This method can be used for advanced dispel logic (e.g., prioritizing certain debuff types)

    // Clean up expired cooldowns
    uint32 now = GameTime::GetGameTimeMS();

    for (auto it = _healerCooldowns.begin(); it != _healerCooldowns.end(); ++it)
    {
        auto& cooldowns = it->second;
        for (auto cdIt = cooldowns.begin(); cdIt != cooldowns.end();)
        {
            if (now > cdIt->second)
                cdIt = cooldowns.erase(cdIt);
            else
                ++cdIt;
        }
    }
}

void HealerCoordinator::UpdateCooldownRotation(GroupCoordinator* group)
{
    // Rotate major healing cooldowns among healers
    // Examples: Tranquility, Aura Mastery, Divine Hymn, Revival
    if (!group->IsInCombat())
        return;

    uint32 combatDuration = group->GetCombatDuration();

    // Use cooldowns at specific combat milestones
    if (combatDuration > 30000 && combatDuration < 35000) // 30-35s into combat
    {
        ObjectGuid healer = GetNextCooldownHealer("major_cd");
        if (!healer.IsEmpty())
        {
            UseHealingCooldown(healer, "major_cd", 120000); // 2min CD
            TC_LOG_DEBUG("playerbot.coordination", "Rotating major healing cooldown at 30s mark");
        }
    }

    if (combatDuration > 60000 && combatDuration < 65000) // 60-65s into combat
    {
        ObjectGuid healer = GetNextCooldownHealer("major_cd");
        if (!healer.IsEmpty())
        {
            UseHealingCooldown(healer, "major_cd", 120000); // 2min CD
            TC_LOG_DEBUG("playerbot.coordination", "Rotating major healing cooldown at 60s mark");
        }
    }
}

void HealerCoordinator::UpdateManaManagement(GroupCoordinator* group)
{
    ::std::vector<ObjectGuid> healers = group->GetBotsByRole(GroupRole::HEALER);

    float totalMana = 0.0f;
    float currentMana = 0.0f;
    uint32 healerCount = 0;

    for (ObjectGuid healerGuid : healers)
    {
        Player* healer = ObjectAccessor::FindPlayer(healerGuid);
        if (!healer)
            continue;

        totalMana += static_cast<float>(healer->GetMaxPower(POWER_MANA));
        currentMana += static_cast<float>(healer->GetPower(POWER_MANA));
        healerCount++;
    }

    if (healerCount == 0)
        return;

    float avgManaPct = (currentMana / totalMana) * 100.0f;

    // Coordinate mana conservation if group mana is low
    if (avgManaPct < 30.0f)
    {
        TC_LOG_DEBUG("playerbot.coordination", "Group healer mana low ({:.1f}%), coordinating conservation", avgManaPct);
        // TODO: Signal healers to use mana-efficient spells
    }
}

// ============================================================================
// DPSCoordinator
// ============================================================================

void DPSCoordinator::Update(GroupCoordinator* group, uint32 diff)
{
    if (!group)
        return;

    _lastUpdateTime += diff;
    if (_lastUpdateTime < _updateInterval)
        return;

    _lastUpdateTime = 0;

    UpdateFocusTarget(group);
    UpdateInterruptRotation(group);
    UpdateCCAssignments(group);
    UpdateBurstWindows(group);
}

void DPSCoordinator::SetFocusTarget(ObjectGuid targetGuid)
{
    if (_focusTarget != targetGuid)
    {
        _focusTarget = targetGuid;
        TC_LOG_DEBUG("playerbot.coordination", "DPS focus target changed to {}", targetGuid.ToString());
    }
}

ObjectGuid DPSCoordinator::GetNextInterrupter() const
{
    uint32 now = GameTime::GetGameTimeMS();

    for (auto const& interrupt : _interruptRotation)
    {
        if (now > interrupt.cooldownExpire)
        {
            return interrupt.dpsGuid;
        }
    }

    return ObjectGuid::Empty;
}

void DPSCoordinator::AssignInterrupt(ObjectGuid dpsGuid, ObjectGuid targetGuid)
{
    uint32 now = GameTime::GetGameTimeMS();

    InterruptAssignment assignment;
    assignment.dpsGuid = dpsGuid;
    assignment.targetGuid = targetGuid;
    assignment.assignedTime = now;
    assignment.cooldownExpire = now + 24000; // ~24s CD (most interrupts are 24s)

    // Remove old assignment for this DPS
    _interruptRotation.erase(
        ::std::remove_if(_interruptRotation.begin(), _interruptRotation.end(),
            [dpsGuid](InterruptAssignment const& assign) {
                return assign.dpsGuid == dpsGuid;
            }),
        _interruptRotation.end()
    );

    _interruptRotation.push_back(assignment);

    TC_LOG_DEBUG("playerbot.coordination", "Assigned interrupt to {} for target {}",
        dpsGuid.ToString(), targetGuid.ToString());
}

ObjectGuid DPSCoordinator::GetCCAssignment(ObjectGuid dpsGuid) const
{
    for (auto const& ccAssignment : _ccAssignments)
    {
        if (ccAssignment.dpsGuid == dpsGuid)
            return ccAssignment.targetGuid;
    }

    return ObjectGuid::Empty;
}

void DPSCoordinator::AssignCC(ObjectGuid dpsGuid, ObjectGuid targetGuid, ::std::string const& ccType)
{
    uint32 now = GameTime::GetGameTimeMS();

    // Remove old assignment
    _ccAssignments.erase(
        ::std::remove_if(_ccAssignments.begin(), _ccAssignments.end(),
            [dpsGuid](CCAssignment const& assign) {
                return assign.dpsGuid == dpsGuid;
            }),
        _ccAssignments.end()
    );

    // Create new assignment
    CCAssignment assignment;
    assignment.dpsGuid = dpsGuid;
    assignment.targetGuid = targetGuid;
    assignment.ccType = ccType;
    assignment.assignedTime = now;

    _ccAssignments.push_back(assignment);

    TC_LOG_DEBUG("playerbot.coordination", "Assigned CC ({}) to {} for target {}",
        ccType, dpsGuid.ToString(), targetGuid.ToString());
}

void DPSCoordinator::RequestBurstWindow(uint32 durationMs)
{
    if (_inBurstWindow)
    {
        TC_LOG_DEBUG("playerbot.coordination", "Burst window already active");
        return;
    }

    _inBurstWindow = true;
    _burstWindowStart = GameTime::GetGameTimeMS();
    _burstWindowDuration = durationMs;

    TC_LOG_DEBUG("playerbot.coordination", "Burst window activated for {}ms", durationMs);
}

bool DPSCoordinator::InBurstWindow() const
{
    if (!_inBurstWindow)
        return false;

    uint32 now = GameTime::GetGameTimeMS();
    return now < _burstWindowStart + _burstWindowDuration;
}

void DPSCoordinator::UpdateFocusTarget(GroupCoordinator* group)
{
    // Use group's focus target
    ObjectGuid groupFocus = group->GetFocusTarget();

    if (groupFocus != _focusTarget)
    {
        SetFocusTarget(groupFocus);
    }
}

void DPSCoordinator::UpdateInterruptRotation(GroupCoordinator* group)
{
    uint32 now = GameTime::GetGameTimeMS();

    // Clean up expired interrupt cooldowns
    _interruptRotation.erase(
        ::std::remove_if(_interruptRotation.begin(), _interruptRotation.end(),
            [now](InterruptAssignment const& assign) {
                return now > assign.cooldownExpire + 30000; // Remove after 30s past expiration
            }),
        _interruptRotation.end()
    );

    // Rebuild rotation from current DPS
    ::std::vector<ObjectGuid> meleeDPS = group->GetBotsByRole(GroupRole::MELEE_DPS);
    ::std::vector<ObjectGuid> rangedDPS = group->GetBotsByRole(GroupRole::RANGED_DPS);

    ::std::vector<ObjectGuid> allDPS;
    allDPS.insert(allDPS.end(), meleeDPS.begin(), meleeDPS.end());
    allDPS.insert(allDPS.end(), rangedDPS.begin(), rangedDPS.end());

    // Add new DPS to rotation if not already present
    for (ObjectGuid dpsGuid : allDPS)
    {
        bool found = false;
        for (auto const& interrupt : _interruptRotation)
        {
            if (interrupt.dpsGuid == dpsGuid)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            InterruptAssignment assignment;
            assignment.dpsGuid = dpsGuid;
            assignment.targetGuid = ObjectGuid::Empty;
            assignment.assignedTime = 0;
            assignment.cooldownExpire = 0; // Ready immediately

            _interruptRotation.push_back(assignment);
        }
    }
}

void DPSCoordinator::UpdateCCAssignments(GroupCoordinator* group)
{
    uint32 now = GameTime::GetGameTimeMS();

    // Clean up old CC assignments (>60s old)
    _ccAssignments.erase(
        ::std::remove_if(_ccAssignments.begin(), _ccAssignments.end(),
            [now](CCAssignment const& assign) {
                return now > assign.assignedTime + 60000;
            }),
        _ccAssignments.end()
    );
}

void DPSCoordinator::UpdateBurstWindows(GroupCoordinator* group)
{
    // Check if burst window expired
    if (_inBurstWindow && !InBurstWindow())
    {
        _inBurstWindow = false;
        TC_LOG_DEBUG("playerbot.coordination", "Burst window ended");
    }

    // Automatic burst windows at specific combat timings
    if (!group->IsInCombat())
        return;

    uint32 combatDuration = group->GetCombatDuration();

    // Initial burst (0-10s)
    if (combatDuration > 0 && combatDuration < 2000 && !_inBurstWindow)
    {
        RequestBurstWindow(10000); // 10s burst
    }

    // Coordinated burst at 2 minutes (execute phase simulation)
    if (combatDuration > 120000 && combatDuration < 122000 && !_inBurstWindow)
    {
        RequestBurstWindow(20000); // 20s burst
    }
}

// ============================================================================
// RoleCoordinatorManager
// ============================================================================

RoleCoordinatorManager::RoleCoordinatorManager()
{
    _tankCoordinator = ::std::make_unique<TankCoordinator>();
    _healerCoordinator = ::std::make_unique<HealerCoordinator>();
    _dpsCoordinator = ::std::make_unique<DPSCoordinator>();
}

void RoleCoordinatorManager::Update(GroupCoordinator* group, uint32 diff)
{
    if (!group)
        return;

    _tankCoordinator->Update(group, diff);
    _healerCoordinator->Update(group, diff);
    _dpsCoordinator->Update(group, diff);
}

} // namespace Coordination
} // namespace Playerbot
