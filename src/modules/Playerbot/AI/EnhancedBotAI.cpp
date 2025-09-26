/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "EnhancedBotAI.h"
#include "Combat/CombatAIIntegrator.h"
#include "ClassAI/ClassAI.h"
#include "Player.h"
#include "Group.h"
#include "Log.h"
#include "SpellInfo.h"
#include "SpellAuras.h"
#include "MotionMaster.h"
#include "World.h"
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

namespace Playerbot
{

EnhancedBotAI::EnhancedBotAI(Player* bot) :
    BotAI(bot),
    _currentState(BotAIState::IDLE),
    _previousState(BotAIState::IDLE),
    _stateTransitionTime(0),
    _debugMode(false),
    _performanceMode(true),
    _maxUpdateRateHz(100), // 100 Hz max update rate
    _currentGroup(nullptr),
    _groupRole(GROUP_ROLE_NONE),
    _inCombat(false),
    _primaryTarget(nullptr),
    _combatStartTime(0),
    _currentManaPercent(100.0f),
    _currentHealthPercent(100.0f),
    _lastRestTime(0),
    _combatUpdateInterval(100),    // 100ms for combat
    _idleUpdateInterval(500),       // 500ms when idle
    _movementUpdateInterval(250),   // 250ms for movement
    _lastCombatUpdate(0),
    _lastIdleUpdate(0),
    _lastMovementUpdate(0),
    _lastGroupUpdate(0),
    _memoryBudgetBytes(10485760),   // 10MB budget
    _lastMemoryCheck(0),
    _memoryCheckInterval(5000),     // Check every 5 seconds
    _updateThrottleMs(0)
{
    InitializeCombatAI();
    InitializeClassAI();
    LoadConfiguration();

    _lastUpdateTime = std::chrono::high_resolution_clock::now();

    TC_LOG_DEBUG("bot.ai.enhanced", "EnhancedBotAI initialized for bot {}", bot->GetName());
}

EnhancedBotAI::~EnhancedBotAI() = default;

void EnhancedBotAI::UpdateAI(uint32 diff)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    // Performance throttling
    if (_updateThrottleMs > 0)
    {
        if (diff < _updateThrottleMs)
            return;
        _updateThrottleMs = 0;
    }

    StartPerformanceCapture();

    // Check performance budget
    if (!IsWithinPerformanceBudget())
    {
        ThrottleIfNeeded();
        return;
    }

    _stats.totalUpdates++;

    try
    {
        // State-based update routing
        switch (_currentState)
        {
            case BotAIState::COMBAT:
                UpdateCombat(diff);
                _stats.combatUpdates++;
                break;

            case BotAIState::IDLE:
                UpdateIdle(diff);
                _stats.idleUpdates++;
                break;

            case BotAIState::TRAVELLING:
            case BotAIState::FOLLOWING:
                UpdateMovement(diff);
                break;

            case BotAIState::QUESTING:
                UpdateQuesting(diff);
                break;

            case BotAIState::TRADING:
            case BotAIState::GATHERING:
                UpdateSocial(diff);
                break;

            case BotAIState::DEAD:
                // Wait for resurrection
                break;

            case BotAIState::FLEEING:
                // Flee logic handled in combat update
                UpdateCombat(diff);
                break;

            case BotAIState::RESTING:
                // Rest logic
                if (GetBot()->GetHealthPct() >= 95.0f && GetBot()->GetPowerPct(POWER_MANA) >= 95.0f)
                {
                    TransitionToState(BotAIState::IDLE);
                }
                break;
        }

        // Always update group coordination if in a group
        if (_currentGroup)
        {
            UpdateGroupCoordination(diff);
        }

        // Process events
        ProcessCombatEvents(diff);
        ProcessMovementEvents(diff);
        ProcessGroupEvents(diff);

        // Memory management
        _lastMemoryCheck += diff;
        if (_lastMemoryCheck >= _memoryCheckInterval)
        {
            CleanupExpiredData();
            CompactMemory();
            _lastMemoryCheck = 0;
        }
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("bot.ai.enhanced", "Update exception for bot {}: {}",
            GetBot()->GetName(), e.what());
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    EndPerformanceCapture(elapsed);

    // Log performance if in debug mode
    if (_debugMode && _stats.totalUpdates % 100 == 0)
    {
        LogPerformanceReport();
    }
}

void EnhancedBotAI::Reset()
{
    BotAI::Reset();

    _currentState = BotAIState::IDLE;
    _previousState = BotAIState::IDLE;
    _inCombat = false;
    _primaryTarget = nullptr;
    _threatList.clear();

    if (_combatIntegrator)
        _combatIntegrator->Reset();

    if (_classAI)
        _classAI->Reset();

    _stats.Reset();

    TC_LOG_DEBUG("bot.ai.enhanced", "EnhancedBotAI reset for bot {}", GetBot()->GetName());
}

void EnhancedBotAI::OnDeath()
{
    BotAI::OnDeath();

    TransitionToState(BotAIState::DEAD);

    if (_combatIntegrator)
        _combatIntegrator->OnCombatEnd();

    _inCombat = false;
    _primaryTarget = nullptr;
}

void EnhancedBotAI::OnRespawn()
{
    BotAI::OnRespawn();

    TransitionToState(BotAIState::IDLE);

    // Reset health and mana tracking
    _currentHealthPercent = GetBot()->GetHealthPct();
    _currentManaPercent = GetBot()->GetPowerPct(POWER_MANA);
}

AIUpdateResult EnhancedBotAI::UpdateEnhanced(uint32 diff)
{
    AIUpdateResult result;

    // Use the standard update with enhanced tracking
    UpdateAI(diff);

    result.actionsExecuted = _stats.actionsExecuted;
    result.triggersChecked = _stats.decisionsM;
    result.updateTime = _stats.avgUpdateTime;

    return result;
}

// Combat event handlers
void EnhancedBotAI::OnCombatStart(Unit* target)
{
    if (!target)
        return;

    _inCombat = true;
    _primaryTarget = target;
    _combatStartTime = getMSTime();

    TransitionToState(BotAIState::COMBAT);

    // Initialize combat systems
    if (_combatIntegrator)
    {
        _combatIntegrator->OnCombatStart(target);
    }

    if (_classAI)
    {
        _classAI->OnCombatStart(target);
    }

    TC_LOG_DEBUG("bot.ai.enhanced", "Combat started for bot {} against {}",
        GetBot()->GetName(), target->GetName());
}

void EnhancedBotAI::OnCombatEnd()
{
    _inCombat = false;
    _primaryTarget = nullptr;
    _threatList.clear();

    if (_combatIntegrator)
    {
        _combatIntegrator->OnCombatEnd();
    }

    if (_classAI)
    {
        _classAI->OnCombatEnd();
    }

    // Check if we should rest
    if (ShouldRest())
    {
        TransitionToState(BotAIState::RESTING);
    }
    else
    {
        TransitionToState(BotAIState::IDLE);
    }

    TC_LOG_DEBUG("bot.ai.enhanced", "Combat ended for bot {}", GetBot()->GetName());
}

void EnhancedBotAI::OnTargetChanged(Unit* newTarget)
{
    Unit* oldTarget = _primaryTarget;
    _primaryTarget = newTarget;

    if (_combatIntegrator)
    {
        _combatIntegrator->OnTargetChanged(newTarget);
    }

    if (_classAI)
    {
        _classAI->OnTargetChanged(newTarget);
    }

    TC_LOG_DEBUG("bot.ai.enhanced", "Target changed for bot {} from {} to {}",
        GetBot()->GetName(),
        oldTarget ? oldTarget->GetName() : "none",
        newTarget ? newTarget->GetName() : "none");
}

// Group event handlers
void EnhancedBotAI::OnGroupJoined(Group* group)
{
    _currentGroup = group;

    if (_combatIntegrator)
    {
        _combatIntegrator->SetGroup(group);
    }

    TC_LOG_DEBUG("bot.ai.enhanced", "Bot {} joined group", GetBot()->GetName());
}

void EnhancedBotAI::OnGroupLeft()
{
    _currentGroup = nullptr;
    _groupRole = GROUP_ROLE_NONE;
    _followTarget.Clear();

    if (_combatIntegrator)
    {
        _combatIntegrator->SetGroup(nullptr);
    }

    TC_LOG_DEBUG("bot.ai.enhanced", "Bot {} left group", GetBot()->GetName());
}

void EnhancedBotAI::OnGroupRoleChanged(GroupRole newRole)
{
    _groupRole = newRole;

    // Update combat AI configuration based on role
    if (_combatIntegrator)
    {
        CombatAIConfig config = _combatIntegrator->GetConfig();

        switch (newRole)
        {
            case GROUP_ROLE_TANK:
                config.enableThreatManagement = true;
                config.threatUpdateThreshold = 5.0f;
                _combatIntegrator = CombatAIFactory::CreateTankCombatAI(GetBot());
                break;

            case GROUP_ROLE_HEALER:
                config.enableKiting = true;
                config.positionUpdateThreshold = 10.0f;
                _combatIntegrator = CombatAIFactory::CreateHealerCombatAI(GetBot());
                break;

            case GROUP_ROLE_DAMAGE:
                config.enableInterrupts = true;
                config.targetSwitchCooldownMs = 500;
                _combatIntegrator = CombatAIFactory::CreateDPSAI(GetBot());
                break;
        }
    }
}

// Configuration
void EnhancedBotAI::SetCombatConfig(CombatAIConfig const& config)
{
    if (_combatIntegrator)
    {
        _combatIntegrator->SetConfig(config);
    }
}

CombatAIConfig const& EnhancedBotAI::GetCombatConfig() const
{
    static CombatAIConfig defaultConfig;
    return _combatIntegrator ? _combatIntegrator->GetConfig() : defaultConfig;
}

// Internal update methods
void EnhancedBotAI::UpdateCombat(uint32 diff)
{
    _lastCombatUpdate += diff;
    if (_lastCombatUpdate < _combatUpdateInterval)
        return;

    if (!_combatIntegrator)
        return;

    // Update combat AI integrator
    IntegrationResult result = _combatIntegrator->Update(diff);

    if (!result.success)
    {
        TC_LOG_ERROR("bot.ai.enhanced", "Combat update failed for bot {}: {}",
            GetBot()->GetName(), result.errorMessage);
    }

    // Update class-specific combat rotation
    if (_classAI && _primaryTarget)
    {
        _classAI->UpdateRotation(_primaryTarget);
        _classAI->UpdateCooldowns(diff);
    }

    // Check for state transitions
    if (ShouldFlee())
    {
        TransitionToState(BotAIState::FLEEING);
    }
    else if (!_inCombat)
    {
        OnCombatEnd();
    }

    _stats.actionsExecuted += result.actionsExecuted;
    _lastCombatUpdate = 0;
}

void EnhancedBotAI::UpdateIdle(uint32 diff)
{
    _lastIdleUpdate += diff;
    if (_lastIdleUpdate < _idleUpdateInterval)
        return;

    // Check for combat
    if (ShouldEngageCombat())
    {
        // Find nearest enemy
        Unit* enemy = GetBot()->SelectNearbyTarget(nullptr, 40.0f);
        if (enemy)
        {
            OnCombatStart(enemy);
            return;
        }
    }

    // Update buffs
    if (_classAI)
    {
        _classAI->UpdateBuffs();
    }

    // Check if should follow group
    if (ShouldFollowGroup() && _followTarget)
    {
        if (Player* leader = ObjectAccessor::GetPlayer(*GetBot(), _followTarget))
        {
            if (GetBot()->GetExactDist2d(leader) > 10.0f)
            {
                GetBot()->GetMotionMaster()->MoveFollow(leader, 5.0f, M_PI / 2);
                TransitionToState(BotAIState::FOLLOWING);
            }
        }
    }

    // Check if should rest
    if (ShouldRest())
    {
        TransitionToState(BotAIState::RESTING);
    }

    _lastIdleUpdate = 0;
}

void EnhancedBotAI::UpdateMovement(uint32 diff)
{
    _lastMovementUpdate += diff;
    if (_lastMovementUpdate < _movementUpdateInterval)
        return;

    // Update pathfinding if combat integrator is available
    if (_combatIntegrator && _combatIntegrator->IsInCombat())
    {
        // Combat movement is handled by combat integrator
        return;
    }

    // Following movement
    if (_currentState == BotAIState::FOLLOWING && _followTarget)
    {
        if (Player* leader = ObjectAccessor::GetPlayer(*GetBot(), _followTarget))
        {
            float distance = GetBot()->GetExactDist2d(leader);

            if (distance < 5.0f)
            {
                GetBot()->GetMotionMaster()->Clear();
                TransitionToState(BotAIState::IDLE);
            }
            else if (distance > 50.0f)
            {
                // Teleport if too far
                GetBot()->NearTeleportTo(leader->GetPositionX(), leader->GetPositionY(),
                    leader->GetPositionZ(), leader->GetOrientation());
            }
        }
    }

    _lastMovementUpdate = 0;
}

void EnhancedBotAI::UpdateGroupCoordination(uint32 diff)
{
    _lastGroupUpdate += diff;
    if (_lastGroupUpdate < 1000) // Update every second
        return;

    if (!_currentGroup)
        return;

    // Update combat integrator group coordination
    if (_combatIntegrator && _inCombat)
    {
        _combatIntegrator->UpdateGroupCoordination();
    }

    // Check group leader for following
    if (!_inCombat && _currentState != BotAIState::FOLLOWING)
    {
        if (Player* leader = ObjectAccessor::GetPlayer(*GetBot(), _currentGroup->GetLeaderGUID()))
        {
            if (leader != GetBot())
            {
                _followTarget = leader->GetGUID();
            }
        }
    }

    _lastGroupUpdate = 0;
}

void EnhancedBotAI::UpdateQuesting(uint32 diff)
{
    // TODO: Implement questing logic
}

void EnhancedBotAI::UpdateSocial(uint32 diff)
{
    // TODO: Implement social interactions
}

// Decision making
bool EnhancedBotAI::ShouldEngageCombat()
{
    // Don't engage if dead or resting
    if (_currentState == BotAIState::DEAD || _currentState == BotAIState::RESTING)
        return false;

    // Don't engage if low health
    if (GetBot()->GetHealthPct() < 30.0f)
        return false;

    // Check for nearby enemies
    Unit* enemy = GetBot()->SelectNearbyTarget(nullptr, 40.0f);
    return enemy != nullptr;
}

bool EnhancedBotAI::ShouldFlee()
{
    // Flee if very low health
    if (GetBot()->GetHealthPct() < 15.0f)
        return true;

    // Flee if outnumbered
    if (_threatList.size() > 3)
        return true;

    return false;
}

bool EnhancedBotAI::ShouldRest()
{
    // Rest if low health or mana
    if (GetBot()->GetHealthPct() < 50.0f)
        return true;

    if (GetBot()->GetPowerPct(POWER_MANA) < 30.0f)
        return true;

    return false;
}

bool EnhancedBotAI::ShouldLoot()
{
    // TODO: Implement looting logic
    return false;
}

bool EnhancedBotAI::ShouldFollowGroup()
{
    return _currentGroup != nullptr && !_inCombat;
}

// Performance monitoring
void EnhancedBotAI::StartPerformanceCapture()
{
    // Capture start metrics
}

void EnhancedBotAI::EndPerformanceCapture(std::chrono::microseconds elapsed)
{
    _stats.totalUpdateTime += elapsed;
    _stats.avgUpdateTime = _stats.totalUpdateTime / std::max(1u, _stats.totalUpdates);

    if (elapsed > _stats.maxUpdateTime)
        _stats.maxUpdateTime = elapsed;

    // Calculate CPU usage
    float cpuPercent = (elapsed.count() / 1000.0f) / 10.0f; // Assuming 10ms budget
    _stats.cpuUsagePercent = cpuPercent;

#ifdef _WIN32
    // Get memory usage
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)))
    {
        _stats.memoryUsageBytes = pmc.PrivateUsage;
    }
#endif
}

bool EnhancedBotAI::IsWithinPerformanceBudget() const
{
    // Check CPU budget (0.1% = 100 microseconds per 100ms)
    if (_stats.avgUpdateTime.count() > 100)
        return false;

    // Check memory budget
    if (_stats.memoryUsageBytes > _memoryBudgetBytes)
        return false;

    return true;
}

void EnhancedBotAI::ThrottleIfNeeded()
{
    // If over budget, increase update interval
    if (_stats.avgUpdateTime.count() > 100)
    {
        _updateThrottleMs = 50; // Skip next 50ms
    }
}

void EnhancedBotAI::LogPerformanceReport()
{
    TC_LOG_INFO("bot.ai.enhanced.performance",
        "Bot {} Performance Report:\n"
        "  Total Updates: {}\n"
        "  Combat Updates: {}\n"
        "  Avg Update Time: {} us\n"
        "  Max Update Time: {} us\n"
        "  CPU Usage: {:.3f}%\n"
        "  Memory Usage: {:.2f} MB",
        GetBot()->GetName(),
        _stats.totalUpdates,
        _stats.combatUpdates,
        _stats.avgUpdateTime.count(),
        _stats.maxUpdateTime.count(),
        _stats.cpuUsagePercent,
        _stats.memoryUsageBytes / 1048576.0f);

    if (_combatIntegrator)
    {
        CombatMetrics const& metrics = _combatIntegrator->GetMetrics();
        TC_LOG_INFO("bot.ai.enhanced.performance",
            "  Combat Metrics:\n"
            "    Interrupts: {}/{}\n"
            "    Position Changes: {}\n"
            "    Threat Adjustments: {}",
            metrics.interruptsSuccessful.load(),
            metrics.interruptsAttempted.load(),
            metrics.positionChanges.load(),
            metrics.threatAdjustments.load());
    }
}

// Private methods
void EnhancedBotAI::InitializeCombatAI()
{
    _combatIntegrator = CombatAIFactory::CreateCombatAI(GetBot());

    if (_combatIntegrator)
    {
        // Register this AI with the combat integrator
        _combatIntegrator->RegisterClassAI(_classAI.get());
    }
}

void EnhancedBotAI::InitializeClassAI()
{
    _classAI = ClassAIFactory::CreateClassAI(GetBot());

    if (_classAI && _combatIntegrator)
    {
        _combatIntegrator->RegisterClassAI(_classAI.get());
    }
}

void EnhancedBotAI::LoadConfiguration()
{
    // Load from config or use defaults
    _debugMode = sWorld->getBoolConfig(CONFIG_DEBUG_BATTLEGROUND); // Example config
    _performanceMode = true;
    _maxUpdateRateHz = 100;
    _memoryBudgetBytes = 10485760; // 10MB
}

void EnhancedBotAI::TransitionToState(BotAIState newState)
{
    if (_currentState == newState)
        return;

    BotAIState oldState = _currentState;
    _previousState = _currentState;
    _currentState = newState;
    _stateTransitionTime = getMSTime();

    HandleStateTransition(oldState, newState);

    TC_LOG_DEBUG("bot.ai.enhanced", "Bot {} transitioned from {} to {}",
        GetBot()->GetName(),
        static_cast<int>(oldState),
        static_cast<int>(newState));
}

void EnhancedBotAI::HandleStateTransition(BotAIState oldState, BotAIState newState)
{
    // Handle state-specific transitions
    switch (newState)
    {
        case BotAIState::COMBAT:
            _combatUpdateInterval = 100; // Faster updates in combat
            break;

        case BotAIState::IDLE:
            _idleUpdateInterval = 500; // Slower updates when idle
            break;

        case BotAIState::RESTING:
            GetBot()->SetStandState(UNIT_STAND_STATE_SIT);
            break;

        default:
            break;
    }

    // Handle leaving old state
    switch (oldState)
    {
        case BotAIState::RESTING:
            GetBot()->SetStandState(UNIT_STAND_STATE_STAND);
            break;

        default:
            break;
    }
}

void EnhancedBotAI::ProcessCombatEvents(uint32 diff)
{
    // Process combat-related events
    if (_inCombat && !GetBot()->IsInCombat())
    {
        OnCombatEnd();
    }
    else if (!_inCombat && GetBot()->IsInCombat())
    {
        Unit* target = GetBot()->GetVictim();
        if (target)
        {
            OnCombatStart(target);
        }
    }
}

void EnhancedBotAI::ProcessMovementEvents(uint32 diff)
{
    // Process movement-related events
}

void EnhancedBotAI::ProcessGroupEvents(uint32 diff)
{
    // Process group-related events
}

void EnhancedBotAI::CleanupExpiredData()
{
    // Clean up threat list
    _threatList.erase(
        std::remove_if(_threatList.begin(), _threatList.end(),
            [](Unit* unit) { return !unit || !unit->IsAlive(); }),
        _threatList.end()
    );
}

void EnhancedBotAI::CompactMemory()
{
    // Compact memory in combat integrator
    if (_combatIntegrator)
    {
        _combatIntegrator->CompactMemory();
    }

    // Shrink vectors
    _threatList.shrink_to_fit();
}

// Factory implementations
std::unique_ptr<EnhancedBotAI> EnhancedBotAIFactory::CreateEnhancedAI(Player* bot)
{
    return std::make_unique<EnhancedBotAI>(bot);
}

std::unique_ptr<EnhancedBotAI> EnhancedBotAIFactory::CreateTankAI(Player* bot)
{
    auto ai = std::make_unique<EnhancedBotAI>(bot);

    CombatAIConfig config;
    config.enableThreatManagement = true;
    config.threatUpdateThreshold = 5.0f;
    config.positionUpdateThreshold = 3.0f;

    ai->SetCombatConfig(config);
    return ai;
}

std::unique_ptr<EnhancedBotAI> EnhancedBotAIFactory::CreateHealerAI(Player* bot)
{
    auto ai = std::make_unique<EnhancedBotAI>(bot);

    CombatAIConfig config;
    config.enableKiting = true;
    config.positionUpdateThreshold = 10.0f;
    config.interruptReactionTimeMs = 150;

    ai->SetCombatConfig(config);
    return ai;
}

std::unique_ptr<EnhancedBotAI> EnhancedBotAIFactory::CreateDPSAI(Player* bot)
{
    auto ai = std::make_unique<EnhancedBotAI>(bot);

    CombatAIConfig config;
    config.enableInterrupts = true;
    config.targetSwitchCooldownMs = 500;

    ai->SetCombatConfig(config);
    return ai;
}

// Class-specific factory methods
std::unique_ptr<EnhancedBotAI> EnhancedBotAIFactory::CreateWarriorAI(Player* bot)
{
    auto ai = std::make_unique<EnhancedBotAI>(bot);

    // Warriors are typically tanks or melee DPS
    CombatAIConfig config;
    config.enableThreatManagement = true;
    config.enablePositioning = true;
    config.positionUpdateThreshold = 5.0f;

    ai->SetCombatConfig(config);
    return ai;
}

std::unique_ptr<EnhancedBotAI> EnhancedBotAIFactory::CreatePriestAI(Player* bot)
{
    auto ai = std::make_unique<EnhancedBotAI>(bot);

    // Priests are typically healers
    CombatAIConfig config;
    config.enableKiting = true;
    config.enableInterrupts = true;
    config.positionUpdateThreshold = 15.0f;

    ai->SetCombatConfig(config);
    return ai;
}

// Implement remaining class-specific factory methods...
std::unique_ptr<EnhancedBotAI> EnhancedBotAIFactory::CreatePaladinAI(Player* bot)
{
    return CreateEnhancedAI(bot);
}

std::unique_ptr<EnhancedBotAI> EnhancedBotAIFactory::CreateHunterAI(Player* bot)
{
    return CreateEnhancedAI(bot);
}

std::unique_ptr<EnhancedBotAI> EnhancedBotAIFactory::CreateRogueAI(Player* bot)
{
    return CreateEnhancedAI(bot);
}

std::unique_ptr<EnhancedBotAI> EnhancedBotAIFactory::CreateDeathKnightAI(Player* bot)
{
    return CreateEnhancedAI(bot);
}

std::unique_ptr<EnhancedBotAI> EnhancedBotAIFactory::CreateShamanAI(Player* bot)
{
    return CreateEnhancedAI(bot);
}

std::unique_ptr<EnhancedBotAI> EnhancedBotAIFactory::CreateMageAI(Player* bot)
{
    return CreateEnhancedAI(bot);
}

std::unique_ptr<EnhancedBotAI> EnhancedBotAIFactory::CreateWarlockAI(Player* bot)
{
    return CreateEnhancedAI(bot);
}

std::unique_ptr<EnhancedBotAI> EnhancedBotAIFactory::CreateMonkAI(Player* bot)
{
    return CreateEnhancedAI(bot);
}

std::unique_ptr<EnhancedBotAI> EnhancedBotAIFactory::CreateDruidAI(Player* bot)
{
    return CreateEnhancedAI(bot);
}

std::unique_ptr<EnhancedBotAI> EnhancedBotAIFactory::CreateDemonHunterAI(Player* bot)
{
    return CreateEnhancedAI(bot);
}

std::unique_ptr<EnhancedBotAI> EnhancedBotAIFactory::CreateEvokerAI(Player* bot)
{
    return CreateEnhancedAI(bot);
}

} // namespace Playerbot