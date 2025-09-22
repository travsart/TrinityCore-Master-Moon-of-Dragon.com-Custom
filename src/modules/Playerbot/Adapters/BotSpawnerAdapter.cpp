/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "BotSpawnerAdapter.h"
#include "Lifecycle/BotSpawnOrchestrator.h"
#include "Lifecycle/BotSpawner.h"
#include "Lifecycle/BotCharacterSelector.h"
#include "Lifecycle/BotPerformanceMonitor.h"
#include "Lifecycle/BotResourcePool.h"
#include "Lifecycle/BotPopulationManager.h"
#include "Session/BotSessionFactory.h"
#include "Config/PlayerbotConfig.h"
#include "Logging/Log.h"
#include <chrono>

namespace Playerbot
{

// =====================================================
// BotSpawnerAdapter Implementation
// =====================================================

BotSpawnerAdapter::BotSpawnerAdapter()
{
    TC_LOG_DEBUG("module.playerbot.adapter",
        "BotSpawnerAdapter: Creating adapter for orchestrator-based spawning");
}

BotSpawnerAdapter::~BotSpawnerAdapter()
{
    Shutdown();
}

bool BotSpawnerAdapter::Initialize()
{
    TC_LOG_INFO("module.playerbot.adapter",
        "BotSpawnerAdapter: Initializing orchestrator adapter");

    if (!InitializeOrchestrator())
    {
        TC_LOG_ERROR("module.playerbot.adapter",
            "BotSpawnerAdapter: Failed to initialize orchestrator");
        return false;
    }

    ConfigureOrchestrator();
    _enabled = sPlayerbotConfig->GetBool("Playerbot.Enable", false);

    TC_LOG_INFO("module.playerbot.adapter",
        "BotSpawnerAdapter: Successfully initialized (enabled: {})", _enabled);
    return true;
}

void BotSpawnerAdapter::Shutdown()
{
    TC_LOG_INFO("module.playerbot.adapter",
        "BotSpawnerAdapter: Shutting down adapter and orchestrator");

    if (_orchestrator)
    {
        _orchestrator->Shutdown();
        _orchestrator.reset();
    }
}

void BotSpawnerAdapter::Update(uint32 diff)
{
    if (!_enabled || !_orchestrator)
        return;

    auto start = std::chrono::high_resolution_clock::now();
    _orchestrator->Update(diff);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    RecordApiCall(duration);
}

bool BotSpawnerAdapter::SpawnBot(SpawnRequest const& request)
{
    if (!_enabled || !_orchestrator)
        return false;

    auto start = std::chrono::high_resolution_clock::now();
    ++_stats.callsToSpawnBot;

    bool result = _orchestrator->SpawnBot(request);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    RecordApiCall(duration);

    return result;
}

uint32 BotSpawnerAdapter::SpawnBots(std::vector<SpawnRequest> const& requests)
{
    if (!_enabled || !_orchestrator)
        return 0;

    auto start = std::chrono::high_resolution_clock::now();
    ++_stats.callsToSpawnBots;

    uint32 result = _orchestrator->SpawnBots(requests);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    RecordApiCall(duration);

    return result;
}

void BotSpawnerAdapter::SpawnToPopulationTarget()
{
    if (!_enabled || !_orchestrator)
        return;

    _orchestrator->SpawnToPopulationTarget();
}

void BotSpawnerAdapter::UpdatePopulationTargets()
{
    if (!_enabled || !_orchestrator)
        return;

    _orchestrator->UpdatePopulationTargets();
}

bool BotSpawnerAdapter::DespawnBot(ObjectGuid guid, std::string const& reason)
{
    if (!_enabled || !_orchestrator)
        return false;

    ++_stats.callsToDespawnBot;
    return _orchestrator->DespawnBot(guid, reason);
}

void BotSpawnerAdapter::DespawnBot(ObjectGuid guid, bool forced)
{
    if (!_enabled || !_orchestrator)
        return;

    ++_stats.callsToDespawnBot;
    _orchestrator->DespawnBot(guid, forced);
}

uint32 BotSpawnerAdapter::GetActiveBotCount() const
{
    if (!_enabled || !_orchestrator)
        return 0;

    ++_stats.queryCalls;
    return _orchestrator->GetActiveBotCount();
}

uint32 BotSpawnerAdapter::GetActiveBotCount(uint32 zoneId) const
{
    if (!_enabled || !_orchestrator)
        return 0;

    ++_stats.queryCalls;
    return _orchestrator->GetActiveBotCount(zoneId);
}

bool BotSpawnerAdapter::CanSpawnMore() const
{
    if (!_enabled || !_orchestrator)
        return false;

    ++_stats.queryCalls;
    return _orchestrator->CanSpawnMore();
}

bool BotSpawnerAdapter::CanSpawnInZone(uint32 zoneId) const
{
    if (!_enabled || !_orchestrator)
        return false;

    ++_stats.queryCalls;
    return _orchestrator->CanSpawnInZone(zoneId);
}

void BotSpawnerAdapter::SetMaxBots(uint32 maxBots)
{
    _maxBots = maxBots;
    if (_orchestrator)
        _orchestrator->SetMaxBots(maxBots);
}

void BotSpawnerAdapter::SetBotToPlayerRatio(float ratio)
{
    _botToPlayerRatio = ratio;
    if (_orchestrator)
        _orchestrator->SetBotToPlayerRatio(ratio);
}

bool BotSpawnerAdapter::IsEnabled() const
{
    return _enabled;
}

void BotSpawnerAdapter::SetEnabled(bool enabled)
{
    _enabled = enabled;
    TC_LOG_INFO("module.playerbot.adapter",
        "BotSpawnerAdapter: Adapter enabled state changed to: {}", enabled);
}

void BotSpawnerAdapter::ResetAdapterStats()
{
    _stats = AdapterStats{};
    TC_LOG_DEBUG("module.playerbot.adapter",
        "BotSpawnerAdapter: Performance statistics reset");
}

bool BotSpawnerAdapter::InitializeOrchestrator()
{
    try
    {
        _orchestrator = std::make_unique<BotSpawnOrchestrator>();
        return _orchestrator->Initialize();
    }
    catch (std::exception const& ex)
    {
        TC_LOG_ERROR("module.playerbot.adapter",
            "BotSpawnerAdapter: Exception during orchestrator initialization: {}", ex.what());
        return false;
    }
}

void BotSpawnerAdapter::ConfigureOrchestrator()
{
    if (!_orchestrator)
        return;

    _orchestrator->SetMaxBots(_maxBots);
    _orchestrator->SetBotToPlayerRatio(_botToPlayerRatio);

    TC_LOG_DEBUG("module.playerbot.adapter",
        "BotSpawnerAdapter: Orchestrator configured with maxBots={}, ratio={}",
        _maxBots, _botToPlayerRatio);
}

void BotSpawnerAdapter::RecordApiCall(uint64 durationMicroseconds)
{
    // Simple moving average for performance tracking
    if (_stats.avgCallDurationUs == 0)
        _stats.avgCallDurationUs = durationMicroseconds;
    else
        _stats.avgCallDurationUs = (_stats.avgCallDurationUs + durationMicroseconds) / 2;
}

// =====================================================
// LegacyBotSpawnerAdapter Implementation
// =====================================================

LegacyBotSpawnerAdapter::LegacyBotSpawnerAdapter()
{
    TC_LOG_DEBUG("module.playerbot.adapter",
        "LegacyBotSpawnerAdapter: Creating adapter for legacy spawning");
}

LegacyBotSpawnerAdapter::~LegacyBotSpawnerAdapter()
{
    Shutdown();
}

bool LegacyBotSpawnerAdapter::Initialize()
{
    TC_LOG_INFO("module.playerbot.adapter",
        "LegacyBotSpawnerAdapter: Initializing legacy adapter");

    try
    {
        _legacySpawner = std::make_unique<BotSpawner>();
        return _legacySpawner->Initialize();
    }
    catch (std::exception const& ex)
    {
        TC_LOG_ERROR("module.playerbot.adapter",
            "LegacyBotSpawnerAdapter: Exception during initialization: {}", ex.what());
        return false;
    }
}

void LegacyBotSpawnerAdapter::Shutdown()
{
    if (_legacySpawner)
    {
        _legacySpawner->Shutdown();
        _legacySpawner.reset();
    }
}

void LegacyBotSpawnerAdapter::Update(uint32 diff)
{
    if (_legacySpawner)
        _legacySpawner->Update(diff);
}

bool LegacyBotSpawnerAdapter::SpawnBot(SpawnRequest const& request)
{
    if (!_legacySpawner)
        return false;

    return _legacySpawner->SpawnBot(request);
}

uint32 LegacyBotSpawnerAdapter::SpawnBots(std::vector<SpawnRequest> const& requests)
{
    if (!_legacySpawner)
        return 0;

    return _legacySpawner->SpawnBots(requests);
}

void LegacyBotSpawnerAdapter::SpawnToPopulationTarget()
{
    if (_legacySpawner)
        _legacySpawner->SpawnToPopulationTarget();
}

void LegacyBotSpawnerAdapter::UpdatePopulationTargets()
{
    if (_legacySpawner)
        _legacySpawner->UpdatePopulationTargets();
}

bool LegacyBotSpawnerAdapter::DespawnBot(ObjectGuid guid, std::string const& reason)
{
    if (!_legacySpawner)
        return false;

    return _legacySpawner->DespawnBot(guid, reason);
}

void LegacyBotSpawnerAdapter::DespawnBot(ObjectGuid guid, bool forced)
{
    if (_legacySpawner)
        _legacySpawner->DespawnBot(guid, forced);
}

uint32 LegacyBotSpawnerAdapter::GetActiveBotCount() const
{
    return _legacySpawner ? _legacySpawner->GetActiveBotCount() : 0;
}

uint32 LegacyBotSpawnerAdapter::GetActiveBotCount(uint32 zoneId) const
{
    return _legacySpawner ? _legacySpawner->GetActiveBotCount(zoneId) : 0;
}

bool LegacyBotSpawnerAdapter::CanSpawnMore() const
{
    return _legacySpawner ? _legacySpawner->CanSpawnMore() : false;
}

bool LegacyBotSpawnerAdapter::CanSpawnInZone(uint32 zoneId) const
{
    return _legacySpawner ? _legacySpawner->CanSpawnInZone(zoneId) : false;
}

void LegacyBotSpawnerAdapter::SetMaxBots(uint32 maxBots)
{
    if (_legacySpawner)
        _legacySpawner->SetMaxBots(maxBots);
}

void LegacyBotSpawnerAdapter::SetBotToPlayerRatio(float ratio)
{
    if (_legacySpawner)
        _legacySpawner->SetBotToPlayerRatio(ratio);
}

bool LegacyBotSpawnerAdapter::IsEnabled() const
{
    return _legacySpawner ? _legacySpawner->IsEnabled() : false;
}

void LegacyBotSpawnerAdapter::SetEnabled(bool enabled)
{
    if (_legacySpawner)
        _legacySpawner->SetEnabled(enabled);
}

// =====================================================
// BotSpawnerFactory Implementation
// =====================================================

std::unique_ptr<IBotSpawner> BotSpawnerFactory::CreateSpawner(SpawnerType type)
{
    if (type == SpawnerType::AUTO)
        type = DetectBestSpawnerType();

    TC_LOG_INFO("module.playerbot.factory",
        "BotSpawnerFactory: Creating spawner of type: {}", GetSpawnerTypeName(type));

    switch (type)
    {
        case SpawnerType::ORCHESTRATED:
            return std::make_unique<BotSpawnerAdapter>();

        case SpawnerType::LEGACY:
            return std::make_unique<LegacyBotSpawnerAdapter>();

        default:
            TC_LOG_ERROR("module.playerbot.factory",
                "BotSpawnerFactory: Unknown spawner type, falling back to legacy");
            return std::make_unique<LegacyBotSpawnerAdapter>();
    }
}

BotSpawnerFactory::SpawnerType BotSpawnerFactory::DetectBestSpawnerType()
{
    // Check if orchestrator is available and should be used
    if (IsOrchestratorAvailable() && !ShouldUseLegacySpawner())
    {
        TC_LOG_DEBUG("module.playerbot.factory",
            "BotSpawnerFactory: Auto-detected ORCHESTRATED spawner");
        return SpawnerType::ORCHESTRATED;
    }

    TC_LOG_DEBUG("module.playerbot.factory",
        "BotSpawnerFactory: Auto-detected LEGACY spawner");
    return SpawnerType::LEGACY;
}

std::string BotSpawnerFactory::GetSpawnerTypeName(SpawnerType type)
{
    switch (type)
    {
        case SpawnerType::ORCHESTRATED: return "ORCHESTRATED";
        case SpawnerType::LEGACY: return "LEGACY";
        case SpawnerType::AUTO: return "AUTO";
        default: return "UNKNOWN";
    }
}

bool BotSpawnerFactory::IsOrchestratorAvailable()
{
    // Check if all required components for orchestrator are available
    return sPlayerbotConfig->GetBool("Playerbot.UseOrchestrator", true);
}

bool BotSpawnerFactory::ShouldUseLegacySpawner()
{
    // Check configuration flag to force legacy mode
    return sPlayerbotConfig->GetBool("Playerbot.ForceLegacyMode", false);
}

} // namespace Playerbot