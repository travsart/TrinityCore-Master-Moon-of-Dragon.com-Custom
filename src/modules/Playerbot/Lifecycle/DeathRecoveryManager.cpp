/*
 * Copyright (C) 2025 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "DeathRecoveryManager.h"
#include "Player.h"
#include "Creature.h"
#include "Corpse.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Map.h"
#include "WorldSession.h"
#include "MotionMaster.h"
#include "Log.h"
#include "Config.h"
#include "Config/PlayerbotConfig.h"
#include "AI/BotAI.h"
#include "Interaction/Core/InteractionManager.h"
#include "GridNotifiers.h"
#include "CellImpl.h"
#include <sstream>

namespace Playerbot
{

// ============================================================================
// DeathRecoveryConfig Implementation
// ============================================================================

DeathRecoveryConfig DeathRecoveryConfig::LoadFromConfig()
{
    DeathRecoveryConfig config;

    config.autoReleaseDelayMs = sConfigMgr->GetIntDefault("Playerbot.AutoReleaseDelay", 5) * 1000;
    config.preferCorpseRun = sConfigMgr->GetBoolDefault("Playerbot.PreferCorpseRun", true);
    config.maxCorpseRunDistance = sConfigMgr->GetFloatDefault("Playerbot.MaxCorpseRunDistance", 200.0f);
    config.autoSpiritHealer = sConfigMgr->GetBoolDefault("Playerbot.AutoSpiritHealer", true);
    config.allowBattleResurrection = sConfigMgr->GetBoolDefault("Playerbot.AllowBattleResurrection", true);
    config.navigationUpdateInterval = sConfigMgr->GetIntDefault("Playerbot.DeathRecovery.NavigationInterval", 500);
    config.corpseDistanceCheckInterval = sConfigMgr->GetIntDefault("Playerbot.DeathRecovery.DistanceCheckInterval", 1000);
    config.skipResurrectionSickness = sConfigMgr->GetBoolDefault("Playerbot.SkipResurrectionSickness", false);
    config.spiritHealerSearchRadius = sConfigMgr->GetFloatDefault("Playerbot.SpiritHealerSearchRadius", 150.0f);
    config.resurrectionTimeout = sConfigMgr->GetIntDefault("Playerbot.ResurrectionTimeout", 300) * 1000;
    config.logDebugInfo = sConfigMgr->GetBoolDefault("Playerbot.LogDeathRecovery", true);

    return config;
}

// ============================================================================
// DeathRecoveryStatistics Implementation
// ============================================================================

void DeathRecoveryStatistics::RecordDeath()
{
    ++totalDeaths;
}

void DeathRecoveryStatistics::RecordResurrection(ResurrectionMethod method, uint64 recoveryTimeMs, bool hadSickness)
{
    switch (method)
    {
        case ResurrectionMethod::CORPSE_RUN:
            ++corpseResurrections;
            break;
        case ResurrectionMethod::SPIRIT_HEALER:
            ++spiritHealerResurrections;
            if (hadSickness)
                ++resurrectionsWithSickness;
            break;
        case ResurrectionMethod::BATTLE_RESURRECTION:
            ++battleResurrections;
            break;
        default:
            break;
    }

    totalRecoveryTimeMs += recoveryTimeMs;
    uint32 totalResurrections = corpseResurrections + spiritHealerResurrections + battleResurrections;
    if (totalResurrections > 0)
        averageRecoveryTimeMs = totalRecoveryTimeMs / totalResurrections;
}

void DeathRecoveryStatistics::RecordFailure()
{
    ++failedResurrections;
}

std::string DeathRecoveryStatistics::ToString() const
{
    std::ostringstream oss;
    oss << "Death Recovery Statistics:\n"
        << "  Total Deaths: " << totalDeaths << "\n"
        << "  Corpse Resurrections: " << corpseResurrections << "\n"
        << "  Spirit Healer Resurrections: " << spiritHealerResurrections << "\n"
        << "  Battle Resurrections: " << battleResurrections << "\n"
        << "  Failed Resurrections: " << failedResurrections << "\n"
        << "  Average Recovery Time: " << (averageRecoveryTimeMs / 1000.0f) << "s\n"
        << "  Resurrections with Sickness: " << resurrectionsWithSickness;
    return oss.str();
}

// ============================================================================
// DeathRecoveryManager Implementation
// ============================================================================

DeathRecoveryManager::DeathRecoveryManager(Player* bot, BotAI* ai)
    : m_bot(bot)
    , m_ai(ai)
    , m_state(DeathRecoveryState::NOT_DEAD)
    , m_method(ResurrectionMethod::UNDECIDED)
    , m_corpseDistance(-1.0f)
    , m_spiritHealerGuid(ObjectGuid::Empty)
    , m_navigationActive(false)
    , m_releaseTimer(0)
    , m_stateTimer(0)
    , m_retryTimer(0)
    , m_retryCount(0)
{
    m_config = DeathRecoveryConfig::LoadFromConfig();

    if (m_config.logDebugInfo)
    {
        TC_LOG_DEBUG("playerbot.death", "DeathRecoveryManager created for bot {}",
            m_bot ? m_bot->GetName() : "nullptr");
    }
}

DeathRecoveryManager::~DeathRecoveryManager()
{
    if (m_config.logDebugInfo && m_bot)
    {
        TC_LOG_DEBUG("playerbot.death", "DeathRecoveryManager destroyed for bot {}",
            m_bot->GetName());
    }
}

// ========================================================================
// LIFECYCLE EVENTS
// ========================================================================

void DeathRecoveryManager::OnDeath()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!ValidateBotState())
    {
        TC_LOG_ERROR("playerbot.death", "💀 OnDeath: ValidateBotState FAILED!");
        return;
    }

    m_stats.RecordDeath();
    m_deathTime = std::chrono::steady_clock::now();
    m_method = ResurrectionMethod::UNDECIDED;
    m_spiritHealerGuid = ObjectGuid::Empty;
    m_navigationActive = false;
    m_releaseTimer = m_config.autoReleaseDelayMs;
    m_retryCount = 0;

    // Check if bot is already a ghost (TrinityCore auto-releases spirit on death)
    if (IsGhost())
    {
        TC_LOG_ERROR("playerbot.death", "💀 Bot {} DIED! Already a ghost, skipping spirit release. IsAlive={}, IsGhost={}",
            m_bot->GetName(), m_bot->IsAlive(), IsGhost());
        TransitionToState(DeathRecoveryState::GHOST_DECIDING, "Bot died as ghost");
    }
    else
    {
        TC_LOG_ERROR("playerbot.death", "💀 Bot {} DIED! Initiating death recovery. Auto-release in {}s. IsAlive={}, IsGhost={}",
            m_bot->GetName(), m_config.autoReleaseDelayMs / 1000.0f, m_bot->IsAlive(), IsGhost());
        TransitionToState(DeathRecoveryState::JUST_DIED, "Bot died");
    }
}

void DeathRecoveryManager::OnResurrection()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_state == DeathRecoveryState::NOT_DEAD)
        return;

    uint64 recoveryTime = GetTimeSinceDeath();
    bool hadSickness = m_method == ResurrectionMethod::SPIRIT_HEALER && WillReceiveResurrectionSickness();

    m_stats.RecordResurrection(m_method, recoveryTime, hadSickness);

    TC_LOG_INFO("playerbot.death", "Bot {} resurrected via {} in {:.2f}s",
        m_bot ? m_bot->GetName() : "nullptr",
        m_method == ResurrectionMethod::CORPSE_RUN ? "corpse" :
        m_method == ResurrectionMethod::SPIRIT_HEALER ? "spirit healer" :
        m_method == ResurrectionMethod::BATTLE_RESURRECTION ? "battle rez" : "unknown",
        recoveryTime / 1000.0f);

    Reset();
}

void DeathRecoveryManager::Reset()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_state = DeathRecoveryState::NOT_DEAD;
    m_method = ResurrectionMethod::UNDECIDED;
    m_corpseDistance = -1.0f;
    m_spiritHealerGuid = ObjectGuid::Empty;
    m_navigationActive = false;
    m_releaseTimer = 0;
    m_stateTimer = 0;
    m_retryTimer = 0;
    m_retryCount = 0;

    LogDebug("Death recovery state reset");
}

// ========================================================================
// MAIN UPDATE LOOP
// ========================================================================

void DeathRecoveryManager::Update(uint32 diff)
{
    // Only update if in death recovery
    if (m_state == DeathRecoveryState::NOT_DEAD)
        return;

    std::lock_guard<std::mutex> lock(m_mutex);

    if (!ValidateBotState())
    {
        TC_LOG_ERROR("playerbot.death", "❌ Bot {} Update: ValidateBotState FAILED!", m_bot ? m_bot->GetName() : "nullptr");
        HandleResurrectionFailure("Bot state validation failed");
        return;
    }

    // Check for timeout
    if (IsResurrectionTimedOut())
    {
        TC_LOG_ERROR("playerbot.death", "⏰ Bot {} Update: Resurrection TIMED OUT!", m_bot->GetName());
        HandleResurrectionFailure("Resurrection timed out");
        return;
    }

    // CLEANUP FIX: Detect bots stuck in old failed state before ghost flag fix
    // Old bug signature: State=RESURRECTION_FAILED (10) with IsGhost=false
    // These bots need to be force-resurrected immediately to unstick them
    // Note: Don't check IsAlive() here because ForceResurrection will reset state to NOT_DEAD,
    // and ResurrectPlayer() doesn't set IsAlive instantly, which would cause repeated triggering
    if (m_state.load() == DeathRecoveryState::RESURRECTION_FAILED && !IsGhost())
    {
        TC_LOG_ERROR("playerbot.death", "🚨 Bot {} STUCK in old failed state (State=10, IsGhost=false) - FORCE RESURRECTING!",
            m_bot->GetName());
        ForceResurrection(ResurrectionMethod::SPIRIT_HEALER);
        return;
    }

    // Log state every 5 seconds
    static uint32 logTimer = 0;
    logTimer += diff;
    if (logTimer >= 5000)
    {
        logTimer = 0;
        TC_LOG_ERROR("playerbot.death", "🔄 Bot {} Update: State={}, IsAlive={}, IsGhost={}",
            m_bot->GetName(), static_cast<int>(m_state.load()), m_bot->IsAlive(), IsGhost());
    }

    // Handle current state
    switch (m_state.load())
    {
        case DeathRecoveryState::JUST_DIED:
            HandleJustDied(diff);
            break;
        case DeathRecoveryState::RELEASING_SPIRIT:
            HandleReleasingSpirit(diff);
            break;
        case DeathRecoveryState::GHOST_DECIDING:
            HandleGhostDeciding(diff);
            break;
        case DeathRecoveryState::RUNNING_TO_CORPSE:
            HandleRunningToCorpse(diff);
            break;
        case DeathRecoveryState::AT_CORPSE:
            HandleAtCorpse(diff);
            break;
        case DeathRecoveryState::FINDING_SPIRIT_HEALER:
            HandleFindingSpiritHealer(diff);
            break;
        case DeathRecoveryState::MOVING_TO_SPIRIT_HEALER:
            HandleMovingToSpiritHealer(diff);
            break;
        case DeathRecoveryState::AT_SPIRIT_HEALER:
            HandleAtSpiritHealer(diff);
            break;
        case DeathRecoveryState::RESURRECTING:
            HandleResurrecting(diff);
            break;
        case DeathRecoveryState::RESURRECTION_FAILED:
            HandleResurrectionFailed(diff);
            break;
        default:
            break;
    }
}

// ========================================================================
// STATE MACHINE HANDLERS
// ========================================================================

void DeathRecoveryManager::HandleJustDied(uint32 diff)
{
    // Wait for auto-release timer
    if (m_releaseTimer > diff)
    {
        m_releaseTimer -= diff;
        if (m_releaseTimer % 1000 < diff) // Log every second
        {
            TC_LOG_ERROR("playerbot.death", "⏳ Bot {} waiting to release spirit... {:.1f}s remaining",
                m_bot->GetName(), m_releaseTimer / 1000.0f);
        }
        return;
    }

    // Check if bot already released (manually or by another system)
    if (IsGhost())
    {
        TC_LOG_ERROR("playerbot.death", "👻 Bot {} already a ghost, proceeding to decision phase", m_bot->GetName());
        TransitionToState(DeathRecoveryState::GHOST_DECIDING, "Already ghost");
        return;
    }

    // Release spirit
    TC_LOG_ERROR("playerbot.death", "🚀 Bot {} auto-release timer expired, releasing spirit...", m_bot->GetName());
    TransitionToState(DeathRecoveryState::RELEASING_SPIRIT, "Auto-release timer expired");
}

void DeathRecoveryManager::HandleReleasingSpirit(uint32 diff)
{
    TC_LOG_ERROR("playerbot.death", "🌟 Bot {} attempting to release spirit... IsGhost={}", m_bot->GetName(), IsGhost());

    if (ExecuteReleaseSpirit())
    {
        TC_LOG_ERROR("playerbot.death", "✅ Bot {} spirit released successfully! IsGhost={}", m_bot->GetName(), IsGhost());
        TransitionToState(DeathRecoveryState::GHOST_DECIDING, "Spirit released successfully");
    }
    else
    {
        // Retry after delay
        m_stateTimer += diff;
        if (m_stateTimer > 2000) // Retry every 2 seconds
        {
            m_stateTimer = 0;
            TC_LOG_ERROR("playerbot.death", "🔄 Bot {} retrying spirit release (IsGhost={})", m_bot->GetName(), IsGhost());
        }
    }
}

void DeathRecoveryManager::HandleGhostDeciding(uint32 diff)
{
    TC_LOG_ERROR("playerbot.death", "🤔 Bot {} deciding resurrection method...", m_bot->GetName());

    // Check for special cases first (battlegrounds, arenas, etc)
    if (CheckSpecialResurrectionCases())
    {
        TC_LOG_ERROR("playerbot.death", "🎮 Bot {} in special zone, using special resurrection", m_bot->GetName());
        return;
    }

    // Make resurrection decision
    DecideResurrectionMethod();

    if (m_method == ResurrectionMethod::CORPSE_RUN)
    {
        TC_LOG_ERROR("playerbot.death", "🏃 Bot {} chose CORPSE RUN (distance: {:.1f}y)", m_bot->GetName(), GetCorpseDistance());
        TransitionToState(DeathRecoveryState::RUNNING_TO_CORPSE, "Chose corpse run");
    }
    else if (m_method == ResurrectionMethod::SPIRIT_HEALER)
    {
        TC_LOG_ERROR("playerbot.death", "👼 Bot {} chose SPIRIT HEALER", m_bot->GetName());
        TransitionToState(DeathRecoveryState::FINDING_SPIRIT_HEALER, "Chose spirit healer");
    }
    else
    {
        TC_LOG_ERROR("playerbot.death", "❌ Bot {} FAILED to decide resurrection method!", m_bot->GetName());
        HandleResurrectionFailure("Failed to decide resurrection method");
    }
}

void DeathRecoveryManager::HandleRunningToCorpse(uint32 diff)
{
    // Update corpse distance periodically
    m_stateTimer += diff;
    if (m_stateTimer >= m_config.corpseDistanceCheckInterval)
    {
        m_stateTimer = 0;
        UpdateCorpseDistance();

        if (m_corpseDistance < 0.0f)
        {
            TC_LOG_ERROR("playerbot.death", "🔴 Bot {} CRITICAL: Lost corpse location during corpse run!", m_bot->GetName());
            HandleResurrectionFailure("Lost corpse location");
            return;
        }

        TC_LOG_INFO("playerbot.death", "📏 Bot {} distance to corpse: {:.1f} yards (resurrection range: {})",
            m_bot->GetName(), m_corpseDistance, CORPSE_RESURRECTION_RANGE);
    }

    // Check if in range
    if (IsInCorpseRange())
    {
        TC_LOG_INFO("playerbot.death", "✅ Bot {} reached corpse! Distance: {:.1f} yards",
            m_bot->GetName(), m_corpseDistance);
        TransitionToState(DeathRecoveryState::AT_CORPSE, "Reached corpse");
        return;
    }

    // Update navigation periodically
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastNav = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_lastNavigationUpdate).count();

    if (timeSinceLastNav >= m_config.navigationUpdateInterval)
    {
        TC_LOG_DEBUG("playerbot.death", "🗺️  Bot {} updating navigation to corpse (distance: {:.1f}y)",
            m_bot->GetName(), m_corpseDistance);

        if (NavigateToCorpse())
        {
            m_lastNavigationUpdate = now;
        }
        else
        {
            TC_LOG_ERROR("playerbot.death", "🔴 Bot {} CRITICAL: Failed to navigate to corpse!",
                m_bot->GetName());
            HandleResurrectionFailure("Failed to navigate to corpse");
        }
    }
}

void DeathRecoveryManager::HandleAtCorpse(uint32 diff)
{
    if (InteractWithCorpse())
    {
        TransitionToState(DeathRecoveryState::RESURRECTING, "Interacting with corpse");
    }
    else
    {
        // If interaction fails, might have moved out of range
        UpdateCorpseDistance();
        if (!IsInCorpseRange())
        {
            TransitionToState(DeathRecoveryState::RUNNING_TO_CORPSE, "Moved out of corpse range");
        }
        else
        {
            m_stateTimer += diff;
            if (m_stateTimer > 5000) // Retry after 5 seconds
            {
                m_stateTimer = 0;
                LogDebug("Retrying corpse interaction");
            }
        }
    }
}

void DeathRecoveryManager::HandleFindingSpiritHealer(uint32 diff)
{
    Creature* spiritHealer = FindNearestSpiritHealer();

    if (spiritHealer)
    {
        m_spiritHealerGuid = spiritHealer->GetGUID();
        m_spiritHealerLocation = WorldLocation(spiritHealer->GetMapId(),
            spiritHealer->GetPositionX(),
            spiritHealer->GetPositionY(),
            spiritHealer->GetPositionZ(),
            spiritHealer->GetOrientation());

        TransitionToState(DeathRecoveryState::MOVING_TO_SPIRIT_HEALER, "Found spirit healer");
    }
    else
    {
        m_stateTimer += diff;
        if (m_stateTimer > 10000) // Retry every 10 seconds
        {
            m_stateTimer = 0;
            LogDebug("No spirit healer found, retrying search");

            // After multiple failures, try corpse run instead
            if (++m_retryCount >= 3)
            {
                TC_LOG_WARN("playerbot.death", "Bot {} cannot find spirit healer, switching to corpse run",
                    m_bot->GetName());
                m_method = ResurrectionMethod::CORPSE_RUN;
                TransitionToState(DeathRecoveryState::RUNNING_TO_CORPSE, "Fallback to corpse run");
            }
        }
    }
}

void DeathRecoveryManager::HandleMovingToSpiritHealer(uint32 diff)
{
    Creature* spiritHealer = ObjectAccessor::GetCreature(*m_bot, m_spiritHealerGuid);

    if (!spiritHealer || !spiritHealer->IsAlive())
    {
        m_spiritHealerGuid = ObjectGuid::Empty;
        TransitionToState(DeathRecoveryState::FINDING_SPIRIT_HEALER, "Spirit healer invalid");
        return;
    }

    float distance = m_bot->GetDistance(spiritHealer);

    if (distance <= SPIRIT_HEALER_INTERACTION_RANGE)
    {
        TransitionToState(DeathRecoveryState::AT_SPIRIT_HEALER, "Reached spirit healer");
        return;
    }

    // Update navigation
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastNav = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_lastNavigationUpdate).count();

    if (timeSinceLastNav >= m_config.navigationUpdateInterval)
    {
        if (NavigateToSpiritHealer())
        {
            m_lastNavigationUpdate = now;
            LogDebug("Navigating to spirit healer, distance: " + std::to_string(distance));
        }
        else
        {
            HandleResurrectionFailure("Failed to navigate to spirit healer");
        }
    }
}

void DeathRecoveryManager::HandleAtSpiritHealer(uint32 diff)
{
    if (InteractWithSpiritHealer())
    {
        TransitionToState(DeathRecoveryState::RESURRECTING, "Interacting with spirit healer");
    }
    else
    {
        m_stateTimer += diff;
        if (m_stateTimer > 5000) // Retry after 5 seconds
        {
            m_stateTimer = 0;
            LogDebug("Retrying spirit healer interaction");
        }
    }
}

void DeathRecoveryManager::HandleResurrecting(uint32 diff)
{
    // Check if bot is now alive
    if (m_bot->IsAlive())
    {
        TC_LOG_INFO("playerbot.death", "🎉 Bot {} IS ALIVE! Calling OnResurrection()...",
            m_bot->GetName());
        OnResurrection();
        return;
    }

    // Wait for resurrection to complete (max 30 seconds)
    m_stateTimer += diff;

    // Log waiting status every 5 seconds
    if (m_stateTimer % 5000 < diff)
    {
        TC_LOG_WARN("playerbot.death", "⏳ Bot {} waiting for resurrection... ({:.1f}s elapsed, IsAlive={})",
            m_bot->GetName(), m_stateTimer / 1000.0f, m_bot->IsAlive());
    }

    if (m_stateTimer > 30000)
    {
        TC_LOG_ERROR("playerbot.death", "🔴 Bot {} CRITICAL: Resurrection did not complete after 30 seconds! (IsAlive={})",
            m_bot->GetName(), m_bot->IsAlive());
        HandleResurrectionFailure("Resurrection did not complete");
    }
}

void DeathRecoveryManager::HandleResurrectionFailed(uint32 diff)
{
    m_retryTimer += diff;

    if (m_retryTimer >= RETRY_DELAY_MS)
    {
        m_retryTimer = 0;

        if (++m_retryCount >= MAX_RETRY_ATTEMPTS)
        {
            TC_LOG_ERROR("playerbot.death", "Bot {} exhausted all resurrection attempts",
                m_bot->GetName());
            m_stats.RecordFailure();

            // Force resurrection as last resort
            ForceResurrection(ResurrectionMethod::SPIRIT_HEALER);
        }
        else
        {
            TC_LOG_WARN("playerbot.death", "Bot {} retrying resurrection (attempt {}/{})",
                m_bot->GetName(), m_retryCount, MAX_RETRY_ATTEMPTS);

            // Reset to decision phase
            TransitionToState(DeathRecoveryState::GHOST_DECIDING, "Retry resurrection");
        }
    }
}

// ========================================================================
// DECISION LOGIC
// ========================================================================

void DeathRecoveryManager::DecideResurrectionMethod()
{
    // Check configuration preference
    if (ShouldDoCorpseRun())
    {
        m_method = ResurrectionMethod::CORPSE_RUN;
        TC_LOG_DEBUG("playerbot.death", "Bot {} chose corpse run (distance: {:.1f}y)",
            m_bot->GetName(), GetCorpseDistance());
    }
    else if (ShouldUseSpiritHealer())
    {
        m_method = ResurrectionMethod::SPIRIT_HEALER;
        TC_LOG_DEBUG("playerbot.death", "Bot {} chose spirit healer",
            m_bot->GetName());
    }
    else
    {
        // Default to corpse run
        m_method = ResurrectionMethod::CORPSE_RUN;
        TC_LOG_WARN("playerbot.death", "Bot {} defaulting to corpse run",
            m_bot->GetName());
    }
}

bool DeathRecoveryManager::ShouldDoCorpseRun() const
{
    if (!m_config.preferCorpseRun)
        return false;

    UpdateCorpseDistance();

    if (m_corpseDistance < 0.0f)
        return false; // No corpse

    if (m_corpseDistance > m_config.maxCorpseRunDistance)
        return false; // Too far

    return true;
}

bool DeathRecoveryManager::ShouldUseSpiritHealer() const
{
    if (!m_config.autoSpiritHealer)
        return false;

    // Use spirit healer if corpse is too far or unreachable
    UpdateCorpseDistance();

    if (m_corpseDistance < 0.0f)
        return true; // No corpse

    if (m_corpseDistance > m_config.maxCorpseRunDistance)
        return true; // Too far

    return false;
}

bool DeathRecoveryManager::CheckSpecialResurrectionCases()
{
    if (!m_bot)
        return false;

    // Battlegrounds/arenas have special resurrection mechanics
    if (m_bot->InBattleground())
    {
        // Battlegrounds typically auto-resurrect
        TC_LOG_DEBUG("playerbot.death", "Bot {} in battleground, using default BG resurrection",
            m_bot->GetName());
        m_method = ResurrectionMethod::AUTO_RESURRECT;
        TransitionToState(DeathRecoveryState::RESURRECTING, "Battleground auto-resurrection");
        return true;
    }

    if (m_bot->InArena())
    {
        // Arenas don't allow resurrection during match
        TC_LOG_DEBUG("playerbot.death", "Bot {} in arena, waiting for match end",
            m_bot->GetName());
        return true; // Stay in current state
    }

    return false;
}

// ========================================================================
// RESURRECTION EXECUTION
// ========================================================================

bool DeathRecoveryManager::ExecuteReleaseSpirit()
{
    if (!m_bot)
        return false;

    // Check if already a ghost
    if (IsGhost())
        return true;

    // CRITICAL FIX: Must call BuildPlayerRepop() first to:
    // 1. Create corpse
    // 2. Apply ghost aura (spell 8326)
    // 3. Set ghost state properly
    m_bot->BuildPlayerRepop();

    // CRITICAL FIX #2: Explicitly set PLAYER_FLAGS_GHOST for bots
    // The ghost aura (spell 8326) should set this flag, but it doesn't work reliably for bots
    // Bot Player objects need explicit flag setting instead of relying on aura effects
    m_bot->SetPlayerFlag(PLAYER_FLAGS_GHOST);

    // Then teleport to graveyard
    m_bot->RepopAtGraveyard();

    TC_LOG_ERROR("playerbot.death", "✅ Bot {} released spirit (corpse created, ghost flag SET, teleported to graveyard). IsGhost={}",
        m_bot->GetName(), IsGhost());
    return true;
}

bool DeathRecoveryManager::NavigateToCorpse()
{
    if (!m_bot)
        return false;

    WorldLocation corpseLocation = GetCorpseLocation();
    if (corpseLocation.GetMapId() == MAPID_INVALID)
        return false;

    // Use bot movement system
    m_bot->GetMotionMaster()->MovePoint(0,
        corpseLocation.GetPositionX(),
        corpseLocation.GetPositionY(),
        corpseLocation.GetPositionZ());

    m_navigationActive = true;
    return true;
}

bool DeathRecoveryManager::InteractWithCorpse()
{
    if (!m_bot)
    {
        TC_LOG_ERROR("playerbot.death", "🔴 InteractWithCorpse: Bot is nullptr!");
        return false;
    }

    if (!IsInCorpseRange())
    {
        TC_LOG_ERROR("playerbot.death", "🔴 Bot {} InteractWithCorpse FAILED: Not in corpse range! Distance: {:.1f} yards (need <= {})",
            m_bot->GetName(), m_corpseDistance, CORPSE_RESURRECTION_RANGE);
        return false;
    }

    TC_LOG_INFO("playerbot.death", "⚰️  Bot {} calling ResurrectPlayer() at corpse (distance: {:.1f}y)...",
        m_bot->GetName(), m_corpseDistance);

    // TrinityCore API: Resurrect at corpse
    m_bot->ResurrectPlayer(0.5f, false); // 50% health/mana, no sickness
    m_bot->SpawnCorpseBones();

    bool isAlive = m_bot->IsAlive();
    TC_LOG_INFO("playerbot.death", "✅ Bot {} ResurrectPlayer() called! IsAlive() = {}",
        m_bot->GetName(), isAlive ? "TRUE" : "FALSE");

    return true;
}

bool DeathRecoveryManager::NavigateToSpiritHealer()
{
    if (!m_bot)
        return false;

    Creature* spiritHealer = ObjectAccessor::GetCreature(*m_bot, m_spiritHealerGuid);
    if (!spiritHealer)
        return false;

    m_bot->GetMotionMaster()->MovePoint(0,
        spiritHealer->GetPositionX(),
        spiritHealer->GetPositionY(),
        spiritHealer->GetPositionZ());

    m_navigationActive = true;
    return true;
}

bool DeathRecoveryManager::InteractWithSpiritHealer()
{
    if (!m_bot || !CanInteractWithSpiritHealer())
        return false;

    Creature* spiritHealer = ObjectAccessor::GetCreature(*m_bot, m_spiritHealerGuid);
    if (!spiritHealer)
        return false;

    // Use interaction manager if available
    if (m_ai)
    {
        // This will trigger gossip and eventually SPIRIT_HEALER_CONFIRM event
        // which will call ExecuteGraveyardResurrection()
        TC_LOG_DEBUG("playerbot.death", "Bot {} initiating spirit healer gossip",
            m_bot->GetName());
    }

    // Fallback: direct resurrection
    return ExecuteGraveyardResurrection();
}

bool DeathRecoveryManager::ExecuteGraveyardResurrection()
{
    if (!m_bot)
        return false;

    // TrinityCore API: Graveyard resurrection
    m_bot->ResurrectPlayer(0.5f, true); // 50% health/mana, with resurrection sickness

    // Apply resurrection sickness if appropriate
    if (WillReceiveResurrectionSickness() && !m_config.skipResurrectionSickness)
    {
        // Sickness is automatically applied by ResurrectPlayer with applySickness=true
        TC_LOG_DEBUG("playerbot.death", "Bot {} received resurrection sickness",
            m_bot->GetName());
    }

    TC_LOG_INFO("playerbot.death", "Bot {} resurrected at graveyard", m_bot->GetName());
    return true;
}

// ========================================================================
// STATE QUERIES
// ========================================================================

uint64 DeathRecoveryManager::GetTimeSinceDeath() const
{
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - m_deathTime).count();
}

bool DeathRecoveryManager::IsGhost() const
{
    return m_bot && m_bot->HasPlayerFlag(PLAYER_FLAGS_GHOST);
}

WorldLocation DeathRecoveryManager::GetCorpseLocation() const
{
    if (!m_bot)
        return WorldLocation();

    Corpse* corpse = m_bot->GetCorpse();
    if (!corpse)
        return WorldLocation();

    return WorldLocation(corpse->GetMapId(),
        corpse->GetPositionX(),
        corpse->GetPositionY(),
        corpse->GetPositionZ(),
        corpse->GetOrientation());
}

float DeathRecoveryManager::GetCorpseDistance() const
{
    return m_corpseDistance;
}

bool DeathRecoveryManager::IsInCorpseRange() const
{
    return m_corpseDistance >= 0.0f && m_corpseDistance <= CORPSE_RESURRECTION_RANGE;
}

Creature* DeathRecoveryManager::FindNearestSpiritHealer() const
{
    if (!m_bot)
        return nullptr;

    std::list<Creature*> spiritHealers;
    Trinity::AllCreaturesOfEntryInRange checker(m_bot, 0, m_config.spiritHealerSearchRadius);
    Trinity::CreatureListSearcher<Trinity::AllCreaturesOfEntryInRange> searcher(m_bot, spiritHealers, checker);
    Cell::VisitGridObjects(m_bot, searcher, m_config.spiritHealerSearchRadius);

    Creature* nearest = nullptr;
    float nearestDist = m_config.spiritHealerSearchRadius;

    for (Creature* creature : spiritHealers)
    {
        if (!creature || !creature->IsAlive())
            continue;

        uint64 npcFlags = creature->GetNpcFlags();
        if (!(npcFlags & UNIT_NPC_FLAG_SPIRIT_HEALER) &&
            !(npcFlags & UNIT_NPC_FLAG_AREA_SPIRIT_HEALER))
            continue;

        float dist = m_bot->GetDistance(creature);
        if (dist < nearestDist)
        {
            nearest = creature;
            nearestDist = dist;
        }
    }

    return nearest;
}

bool DeathRecoveryManager::CanInteractWithSpiritHealer() const
{
    if (!m_bot || m_spiritHealerGuid.IsEmpty())
        return false;

    Creature* spiritHealer = ObjectAccessor::GetCreature(*m_bot, m_spiritHealerGuid);
    if (!spiritHealer || !spiritHealer->IsAlive())
        return false;

    float distance = m_bot->GetDistance(spiritHealer);
    return distance <= SPIRIT_HEALER_INTERACTION_RANGE;
}

// ========================================================================
// RESURRECTION CONTROL
// ========================================================================

bool DeathRecoveryManager::TriggerCorpseResurrection()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_state == DeathRecoveryState::NOT_DEAD)
        return false;

    m_method = ResurrectionMethod::CORPSE_RUN;
    TransitionToState(DeathRecoveryState::RUNNING_TO_CORPSE, "Manual corpse resurrection");
    return true;
}

bool DeathRecoveryManager::TriggerSpiritHealerResurrection()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_state == DeathRecoveryState::NOT_DEAD)
        return false;

    m_method = ResurrectionMethod::SPIRIT_HEALER;
    TransitionToState(DeathRecoveryState::FINDING_SPIRIT_HEALER, "Manual spirit healer resurrection");
    return true;
}

bool DeathRecoveryManager::AcceptBattleResurrection(ObjectGuid casterGuid, uint32 spellId)
{
    if (!m_config.allowBattleResurrection)
        return false;

    if (m_state == DeathRecoveryState::NOT_DEAD)
        return false;

    m_method = ResurrectionMethod::BATTLE_RESURRECTION;
    TransitionToState(DeathRecoveryState::RESURRECTING, "Accepting battle resurrection");

    TC_LOG_INFO("playerbot.death", "Bot {} accepting battle resurrection from {}",
        m_bot->GetName(), casterGuid.ToString());

    return true;
}

bool DeathRecoveryManager::ForceResurrection(ResurrectionMethod method)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_bot)
        return false;

    TC_LOG_WARN("playerbot.death", "Bot {} force resurrection via {}",
        m_bot->GetName(),
        method == ResurrectionMethod::CORPSE_RUN ? "corpse" : "spirit healer");

    if (method == ResurrectionMethod::CORPSE_RUN)
    {
        m_bot->ResurrectPlayer(1.0f, false); // Full health, no sickness
    }
    else
    {
        m_bot->ResurrectPlayer(1.0f, true); // Full health, with sickness
    }

    OnResurrection();
    return true;
}

// ========================================================================
// CONFIGURATION
// ========================================================================

void DeathRecoveryManager::SetConfig(DeathRecoveryConfig const& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

void DeathRecoveryManager::ReloadConfig()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = DeathRecoveryConfig::LoadFromConfig();
    TC_LOG_INFO("playerbot.death", "Death recovery configuration reloaded");
}

// ========================================================================
// STATISTICS
// ========================================================================

void DeathRecoveryManager::ResetStatistics()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats = DeathRecoveryStatistics();
}

void DeathRecoveryManager::LogStatistics() const
{
    TC_LOG_INFO("playerbot.death", "Bot {} death recovery statistics:\n{}",
        m_bot ? m_bot->GetName() : "nullptr",
        m_stats.ToString());
}

// ========================================================================
// VALIDATION & ERROR HANDLING
// ========================================================================

bool DeathRecoveryManager::ValidateBotState() const
{
    if (!m_bot)
    {
        TC_LOG_ERROR("playerbot.death", "DeathRecoveryManager: Bot is nullptr");
        return false;
    }

    if (!m_bot->IsInWorld())
    {
        TC_LOG_ERROR("playerbot.death", "Bot {} is not in world", m_bot->GetName());
        return false;
    }

    return true;
}

bool DeathRecoveryManager::IsResurrectionTimedOut() const
{
    return GetTimeSinceDeath() > m_config.resurrectionTimeout;
}

void DeathRecoveryManager::HandleResurrectionFailure(std::string const& reason)
{
    TC_LOG_ERROR("playerbot.death", "Bot {} resurrection failed: {}",
        m_bot ? m_bot->GetName() : "nullptr", reason);

    TransitionToState(DeathRecoveryState::RESURRECTION_FAILED, reason);
}

void DeathRecoveryManager::TransitionToState(DeathRecoveryState newState, std::string const& reason)
{
    DeathRecoveryState oldState = m_state.load();
    m_state = newState;
    m_lastStateTransition = std::chrono::steady_clock::now();
    m_stateTimer = 0;

    if (m_config.logDebugInfo)
    {
        TC_LOG_DEBUG("playerbot.death", "Bot {} death recovery: {} -> {} ({})",
            m_bot ? m_bot->GetName() : "nullptr",
            static_cast<int>(oldState),
            static_cast<int>(newState),
            reason);
    }
}

// ========================================================================
// HELPER METHODS
// ========================================================================

void DeathRecoveryManager::UpdateCorpseDistance() const
{
    WorldLocation corpseLocation = GetCorpseLocation();

    if (corpseLocation.GetMapId() == MAPID_INVALID || !m_bot)
    {
        m_corpseDistance = -1.0f;
        return;
    }

    if (corpseLocation.GetMapId() != m_bot->GetMapId())
    {
        m_corpseDistance = -1.0f; // Different map
        return;
    }

    m_corpseDistance = m_bot->GetExactDist(
        corpseLocation.GetPositionX(),
        corpseLocation.GetPositionY(),
        corpseLocation.GetPositionZ());

    m_lastCorpseDistanceCheck = std::chrono::steady_clock::now();
}

bool DeathRecoveryManager::IsInSpecialZone() const
{
    return m_bot && (m_bot->InBattleground() || m_bot->InArena() || (m_bot->GetMap() && m_bot->GetMap()->IsDungeon()));
}

bool DeathRecoveryManager::WillReceiveResurrectionSickness() const
{
    if (!m_bot)
        return false;

    // Resurrection sickness only applies to players level 11+
    return m_bot->GetLevel() > 10 && m_method == ResurrectionMethod::SPIRIT_HEALER;
}

void DeathRecoveryManager::LogDebug(std::string const& message) const
{
    if (m_config.logDebugInfo)
    {
        TC_LOG_DEBUG("playerbot.death", "Bot {}: {}",
            m_bot ? m_bot->GetName() : "nullptr", message);
    }
}

} // namespace Playerbot
