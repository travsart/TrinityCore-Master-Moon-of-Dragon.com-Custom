/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

// Combat/ThreatManager.h removed - not used in this file
#include "InterruptCoordinator.h"
#include "Player.h"
#include "Unit.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Group.h"
#include "Log.h"
#include "Timer.h"
#include "ObjectAccessor.h"
#include "../BotAI.h"
#include <algorithm>
#include <random>

namespace Playerbot
{

// Spell priority comparator for TriggerResult priority queue
bool TriggerResultComparator::operator()(TriggerResult const& a, TriggerResult const& b) const
{
    // Higher priority values should be processed first
    return a.urgency < b.urgency;
}

InterruptCoordinator::InterruptCoordinator(Group* group)
    : _group(group)
    , _lastUpdate(std::chrono::steady_clock::now())
{
    LoadDefaultPriorities();
    TC_LOG_DEBUG("playerbot", "InterruptCoordinator: Initialized for group %s",
                 group ? std::to_string(group->GetGUID().GetCounter()).c_str() : "nullptr");
}

void InterruptCoordinator::Update(uint32 diff)
{
    if (!_active.load())
        return;

    auto startTime = std::chrono::steady_clock::now();

    // Update bot cooldowns and positions
    {
        std::shared_lock lock(_botMutex);
        for (auto& [botGuid, botInfo] : _botInfo)
        {
            UpdateBotCooldowns(botGuid);
        }
    }

    // Clean up expired casts and assignments
    CleanupExpiredCasts();
    CleanupExpiredAssignments();

    // Update assignment deadlines
    UpdateAssignmentDeadlines();

    // Performance optimization every 10 updates
    if (++_updateCount % 10 == 0)
    {
        OptimizeForPerformance();
    }

    // Update performance metrics
    auto endTime = std::chrono::steady_clock::now();
    auto updateTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    {
        std::lock_guard lock(_metricsMutex);
        if (updateTime > _metrics.maxAssignmentTime)
            _metrics.maxAssignmentTime = updateTime;

        // Rolling average calculation
        auto currentAvg = _metrics.averageAssignmentTime;
        _metrics.averageAssignmentTime = std::chrono::microseconds(
            (currentAvg.count() * 9 + updateTime.count()) / 10
        );
    }

    _lastUpdate = startTime;
}

void InterruptCoordinator::RegisterBot(Player* bot, BotAI* ai)
{
    if (!bot || !ai)
        return;

    ObjectGuid botGuid = bot->GetGUID();

    std::unique_lock lock(_botMutex);

    _botInfo[botGuid] = BotInterruptInfo(botGuid);
    _botAI[botGuid] = ai;

    InitializeBotCapabilities(botGuid, bot);

    TC_LOG_DEBUG("playerbot", "InterruptCoordinator: Registered bot %s with %u interrupt abilities",
                 bot->GetName().c_str(), static_cast<uint32>(_botInfo[botGuid].abilities.size()));
}

void InterruptCoordinator::UnregisterBot(ObjectGuid botGuid)
{
    std::unique_lock lock(_botMutex);

    _botInfo.erase(botGuid);
    _botAI.erase(botGuid);
    _assignedBots.erase(botGuid);

    // Remove any pending assignments for this bot
    std::unique_lock assignLock(_assignmentMutex);
    _pendingAssignments.erase(
        std::remove_if(_pendingAssignments.begin(), _pendingAssignments.end(),
                      [botGuid](const InterruptAssignment& assignment) {
                          return assignment.assignedBot == botGuid;
                      }),
        _pendingAssignments.end()
    );

    TC_LOG_DEBUG("playerbot", "InterruptCoordinator: Unregistered bot %s", botGuid.ToString().c_str());
}

void InterruptCoordinator::RefreshBotCapabilities(ObjectGuid botGuid)
{
    std::unique_lock lock(_botMutex);

    auto botIt = _botAI.find(botGuid);
    if (botIt != _botAI.end() && botIt->second)
    {
        if (Player* bot = botIt->second->GetBot())
        {
            InitializeBotCapabilities(botGuid, bot);
        }
    }
}

bool InterruptCoordinator::OnSpellCastStart(Unit* caster, Spell const* spell)
{
    if (!caster || !spell || !_active.load())
        return false;

    const SpellInfo* spellInfo = spell->GetSpellInfo();
    if (!spellInfo)
        return false;

    uint32 spellId = spellInfo->Id;
    ObjectGuid casterGuid = caster->GetGUID();

    // Check if we should interrupt this spell
    if (!ShouldInterrupt(spellId, caster))
        return false;

    // Create casting spell info
    CastingSpellInfo castInfo;
    castInfo.casterGuid = casterGuid;
    castInfo.spellId = spellId;
    castInfo.priority = GetSpellPriority(spellId);
    castInfo.castStart = std::chrono::steady_clock::now();
    castInfo.castTimeMs = spellInfo->CastTimeEntry ? spellInfo->CastTimeEntry->Base : 0;
    castInfo.isChanneled = spellInfo->IsChanneled();
    castInfo.canBeInterrupted = !spellInfo->HasAttribute(SPELL_ATTR7_NO_UI_NOT_INTERRUPTIBLE);
    castInfo.schoolMask = spellInfo->SchoolMask;
    castInfo.casterPosition = Position(caster->GetPositionX(), caster->GetPositionY(), caster->GetPositionZ());

    // Calculate cast end time
    uint32 totalCastTime = castInfo.isChanneled ? spellInfo->GetDuration() : castInfo.castTimeMs;
    castInfo.castEnd = castInfo.castStart + std::chrono::milliseconds(totalCastTime);

    // Store the active cast
    {
        std::unique_lock lock(_castMutex);
        _activeCasts[casterGuid] = castInfo;
    }

    // Update metrics
    _metrics.spellsDetected.fetch_add(1);

    // Assign interrupt
    bool assigned = AssignInterrupt(castInfo);
    if (assigned)
        _metrics.interruptsAssigned.fetch_add(1);
    else
        _metrics.assignmentFailures.fetch_add(1);

    TC_LOG_DEBUG("playerbot", "InterruptCoordinator: Detected spell cast %u from %s, priority %u, assigned: %s",
                 spellId, caster->GetName().c_str(), static_cast<uint32>(castInfo.priority), assigned ? "yes" : "no");

    return assigned;
}

void InterruptCoordinator::OnSpellCastFinish(Unit* caster, uint32 spellId, bool interrupted)
{
    if (!caster)
        return;

    ObjectGuid casterGuid = caster->GetGUID();

    // Remove from active casts
    {
        std::unique_lock lock(_castMutex);
        _activeCasts.erase(casterGuid);
    }

    // Update metrics if this was an interrupt success
    if (interrupted)
    {
        _metrics.interruptsSucceeded.fetch_add(1);
        TC_LOG_DEBUG("playerbot", "InterruptCoordinator: Spell %u from %s successfully interrupted",
                     spellId, caster->GetName().c_str());
    }

    // Remove any pending assignments for this cast
    std::unique_lock assignLock(_assignmentMutex);
    _pendingAssignments.erase(
        std::remove_if(_pendingAssignments.begin(), _pendingAssignments.end(),
                      [casterGuid, spellId](const InterruptAssignment& assignment) {
                          return assignment.targetCaster == casterGuid && assignment.targetSpell == spellId;
                      }),
        _pendingAssignments.end()
    );
}

void InterruptCoordinator::OnSpellCastCancel(Unit* caster, uint32 spellId)
{
    OnSpellCastFinish(caster, spellId, false);
}

void InterruptCoordinator::OnInterruptExecuted(ObjectGuid botGuid, ObjectGuid targetGuid, uint32 interruptSpell, bool succeeded)
{
    // Update bot cooldown
    {
        std::unique_lock lock(_botMutex);
        auto botIt = _botInfo.find(botGuid);
        if (botIt != _botInfo.end())
        {
            // Find the interrupt ability and set its cooldown
            for (const auto& ability : botIt->second.abilities)
            {
                if (ability.spellId == interruptSpell)
                {
                    auto cooldownEnd = std::chrono::steady_clock::now() + std::chrono::milliseconds(ability.cooldownMs);
                    botIt->second.cooldowns[interruptSpell] = cooldownEnd;
                    botIt->second.lastInterrupt = std::chrono::steady_clock::now();
                    botIt->second.interruptCount++;
                    break;
                }
            }
        }
    }

    // Update metrics
    _metrics.interruptsExecuted.fetch_add(1);
    if (succeeded)
        _metrics.interruptsSucceeded.fetch_add(1);
    else
        _metrics.interruptsFailed.fetch_add(1);

    // Mark assignment as executed
    {
        std::unique_lock lock(_assignmentMutex);
        for (auto& assignment : _pendingAssignments)
        {
            if (assignment.assignedBot == botGuid && assignment.targetCaster == targetGuid)
            {
                assignment.executed = true;
                assignment.succeeded = succeeded;
                break;
            }
        }
    }

    TC_LOG_DEBUG("playerbot", "InterruptCoordinator: Bot %s executed interrupt %u on target %s, success: %s",
                 botGuid.ToString().c_str(), interruptSpell, targetGuid.ToString().c_str(), succeeded ? "yes" : "no");
}

void InterruptCoordinator::OnInterruptFailed(ObjectGuid botGuid, ObjectGuid targetGuid, uint32 interruptSpell, std::string const& reason)
{
    OnInterruptExecuted(botGuid, targetGuid, interruptSpell, false);

    TC_LOG_DEBUG("playerbot", "InterruptCoordinator: Bot %s failed to interrupt %u on target %s: %s",
                 botGuid.ToString().c_str(), interruptSpell, targetGuid.ToString().c_str(), reason.c_str());

    // If this was a critical spell and backup assignment is enabled, try to assign backup
    if (_enableBackupAssignment)
    {
        std::shared_lock castLock(_castMutex);
        auto castIt = _activeCasts.find(targetGuid);
        if (castIt != _activeCasts.end() && castIt->second.priority <= InterruptPriority::HIGH)
        {
            // Remove the failed bot from consideration and try to reassign
            CastingSpellInfo castInfo = castIt->second;
            castLock.unlock();

            // Mark the failed bot as temporarily unavailable
            {
                std::unique_lock botLock(_botMutex);
                auto botIt = _botInfo.find(botGuid);
                if (botIt != _botInfo.end())
                {
                    // Set a short cooldown to prevent immediate reassignment
                    auto tempCooldown = std::chrono::steady_clock::now() + std::chrono::milliseconds(5000);
                    botIt->second.cooldowns[interruptSpell] = tempCooldown;
                }
            }

            AssignInterrupt(castInfo);
        }
    }
}

void InterruptCoordinator::SetSpellPriority(uint32 spellId, InterruptPriority priority)
{
    std::unique_lock lock(_castMutex);
    _spellPriorities[spellId] = priority;
}

InterruptPriority InterruptCoordinator::GetSpellPriority(uint32 spellId) const
{
    std::shared_lock lock(_castMutex);
    auto it = _spellPriorities.find(spellId);
    if (it != _spellPriorities.end())
        return it->second;

    // Analyze spell if not in our priority map
    return AnalyzeSpellPriority(spellId, nullptr);
}

void InterruptCoordinator::LoadDefaultPriorities()
{
    std::unique_lock lock(_castMutex);

    // Critical interrupts (Must interrupt)
    _spellPriorities[118] = InterruptPriority::CRITICAL;    // Polymorph
    _spellPriorities[5782] = InterruptPriority::CRITICAL;   // Fear
    _spellPriorities[8122] = InterruptPriority::CRITICAL;   // Psychic Scream
    _spellPriorities[20066] = InterruptPriority::CRITICAL;  // Repentance
    _spellPriorities[51514] = InterruptPriority::CRITICAL;  // Hex
    _spellPriorities[2637] = InterruptPriority::CRITICAL;   // Hibernate

    // High priority interrupts (Major heals, buffs)
    _spellPriorities[2060] = InterruptPriority::HIGH;       // Greater Heal
    _spellPriorities[2061] = InterruptPriority::HIGH;       // Flash Heal
    _spellPriorities[25314] = InterruptPriority::HIGH;      // Greater Heal Rank 7
    _spellPriorities[10917] = InterruptPriority::HIGH;      // Flash Heal Rank 9
    _spellPriorities[1064] = InterruptPriority::HIGH;       // Chain Heal
    _spellPriorities[8004] = InterruptPriority::HIGH;       // Healing Stream Totem

    // Medium priority (Minor heals, damage buffs)
    _spellPriorities[2050] = InterruptPriority::MEDIUM;     // Lesser Heal
    _spellPriorities[6064] = InterruptPriority::MEDIUM;     // Heal
    _spellPriorities[2054] = InterruptPriority::MEDIUM;     // Heal Rank 2
    _spellPriorities[6078] = InterruptPriority::MEDIUM;     // Renew

    // Low priority (Minor effects)
    _spellPriorities[1243] = InterruptPriority::LOW;        // Power Word: Fortitude
    _spellPriorities[8091] = InterruptPriority::LOW;        // Lightning Bolt
    _spellPriorities[133] = InterruptPriority::LOW;         // Fireball

    TC_LOG_DEBUG("playerbot", "InterruptCoordinator: Loaded %u default spell priorities",
                 static_cast<uint32>(_spellPriorities.size()));
}

size_t InterruptCoordinator::GetRegisteredBotCount() const
{
    std::shared_lock lock(_botMutex);
    return _botInfo.size();
}

size_t InterruptCoordinator::GetActiveCastCount() const
{
    std::shared_lock lock(_castMutex);
    return _activeCasts.size();
}

size_t InterruptCoordinator::GetPendingAssignmentCount() const
{
    std::shared_lock lock(_assignmentMutex);
    return _pendingAssignments.size();
}

void InterruptCoordinator::ResetMetrics()
{
    std::lock_guard lock(_metricsMutex);
    _metrics.Reset();
}

std::vector<CastingSpellInfo> InterruptCoordinator::GetActiveCasts() const
{
    std::shared_lock lock(_castMutex);
    std::vector<CastingSpellInfo> casts;
    casts.reserve(_activeCasts.size());

    for (const auto& [guid, castInfo] : _activeCasts)
    {
        casts.push_back(castInfo);
    }

    return casts;
}

std::vector<InterruptAssignment> InterruptCoordinator::GetPendingAssignments() const
{
    std::shared_lock lock(_assignmentMutex);
    return _pendingAssignments;
}

bool InterruptCoordinator::AssignInterrupt(CastingSpellInfo const& castInfo)
{
    auto assignmentStart = std::chrono::steady_clock::now();

    // Quick check if any bots are available
    std::vector<ObjectGuid> candidates;
    {
        std::shared_lock lock(_botMutex);
        for (const auto& [botGuid, botInfo] : _botInfo)
        {
            if (CanBotInterrupt(botGuid, castInfo))
            {
                candidates.push_back(botGuid);
            }
        }
    }

    if (candidates.empty())
    {
        TC_LOG_DEBUG("playerbot", "InterruptCoordinator: No available bots for interrupt assignment");
        return false;
    }

    // Select best interrupter
    ObjectGuid selectedBot = SelectBestInterrupter(castInfo, candidates);
    if (selectedBot.IsEmpty())
        return false;

    // Get the interrupt ability for this bot
    const InterruptAbility* ability = nullptr;
    {
        std::shared_lock lock(_botMutex);
        auto botIt = _botInfo.find(selectedBot);
        if (botIt != _botInfo.end())
        {
            // Calculate distance to target
            float distance = 0.0f;
            if (Player* bot = ObjectAccessor::FindPlayer(selectedBot))
            {
                distance = bot->GetDistance(castInfo.casterPosition.GetPositionX(),
                                           castInfo.casterPosition.GetPositionY(),
                                           castInfo.casterPosition.GetPositionZ());
            }

            ability = botIt->second.GetBestAvailableInterrupt(distance);
        }
    }

    if (!ability)
        return false;

    // Calculate timing
    uint32 interruptTiming = CalculateInterruptTiming(castInfo, *ability);

    // Create assignment
    InterruptAssignment assignment;
    assignment.assignedBot = selectedBot;
    assignment.targetCaster = castInfo.casterGuid;
    assignment.targetSpell = castInfo.spellId;
    assignment.interruptSpell = ability->spellId;
    assignment.assignmentTime = std::chrono::steady_clock::now();
    assignment.executionDeadline = castInfo.castStart + std::chrono::milliseconds(interruptTiming);

    // Store assignment
    {
        std::unique_lock lock(_assignmentMutex);
        _pendingAssignments.push_back(assignment);
        _assignedBots.insert(selectedBot);
    }

    // Notify the bot AI about the assignment
    {
        std::shared_lock lock(_botMutex);
        auto botIt = _botAI.find(selectedBot);
        if (botIt != _botAI.end() && botIt->second)
        {
            // The bot AI will pick up this assignment through its update cycle
            // We could add a direct notification method here if needed
        }
    }

    // Update assignment time metrics
    auto assignmentTime = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - assignmentStart
    );

    {
        std::lock_guard lock(_metricsMutex);
        if (assignmentTime > _metrics.maxAssignmentTime)
            _metrics.maxAssignmentTime = assignmentTime;
    }

    TC_LOG_DEBUG("playerbot", "InterruptCoordinator: Assigned interrupt %u to bot %s for spell %u (timing: %ums)",
                 ability->spellId, selectedBot.ToString().c_str(), castInfo.spellId, interruptTiming);

    return true;
}

ObjectGuid InterruptCoordinator::SelectBestInterrupter(CastingSpellInfo const& castInfo, std::vector<ObjectGuid>& candidates)
{
    if (candidates.empty())
        return ObjectGuid::Empty;

    // Scoring algorithm for bot selection
    struct BotScore
    {
        ObjectGuid botGuid;
        float score = 0.0f;

        BotScore(ObjectGuid guid) : botGuid(guid) {}
    };

    std::vector<BotScore> scores;
    scores.reserve(candidates.size());

    std::shared_lock lock(_botMutex);

    for (ObjectGuid botGuid : candidates)
    {
        auto botIt = _botInfo.find(botGuid);
        if (botIt == _botInfo.end())
            continue;

        const BotInterruptInfo& botInfo = botIt->second;
        BotScore score(botGuid);

        // Base score
        score.score = 100.0f;

        // Distance factor (closer is better)
        if (botInfo.distanceToTarget > 0.0f)
        {
            score.score -= std::min(50.0f, botInfo.distanceToTarget * 2.0f);
        }

        // Cooldown factor (shorter cooldowns are better)
        const InterruptAbility* ability = botInfo.GetBestAvailableInterrupt(botInfo.distanceToTarget);
        if (ability)
        {
            score.score -= ability->cooldownMs / 1000.0f; // Prefer shorter cooldowns
        }

        // Recent interrupt factor (distribute interrupts)
        if (_useRotation)
        {
            auto timeSinceLastInterrupt = std::chrono::steady_clock::now() - botInfo.lastInterrupt;
            auto secondsSince = std::chrono::duration_cast<std::chrono::seconds>(timeSinceLastInterrupt).count();
            score.score += std::min(20.0f, static_cast<float>(secondsSince)); // Bonus for not interrupting recently
        }

        // Assignment load factor (prefer less busy bots)
        if (_assignedBots.count(botGuid) > 0)
        {
            score.score -= 30.0f; // Penalty for already having an assignment
        }

        scores.push_back(score);
    }

    // Sort by score (highest first)
    std::sort(scores.begin(), scores.end(),
              [](const BotScore& a, const BotScore& b) { return a.score > b.score; });

    return !scores.empty() ? scores[0].botGuid : ObjectGuid::Empty;
}

bool InterruptCoordinator::CanBotInterrupt(ObjectGuid botGuid, CastingSpellInfo const& castInfo) const
{
    auto botIt = _botInfo.find(botGuid);
    if (botIt == _botInfo.end())
        return false;

    const BotInterruptInfo& botInfo = botIt->second;

    // Check if bot has available interrupt abilities
    if (!botInfo.HasAvailableInterrupt(botInfo.distanceToTarget))
        return false;

    // Check if spell can be interrupted
    if (!castInfo.canBeInterrupted)
        return false;

    // Check timing - make sure there's enough time to interrupt
    uint32 remainingTime = castInfo.GetRemainingCastTime();
    if (remainingTime < _minInterruptDelay + 200) // 200ms safety margin
        return false;

    // Check if bot is already assigned to another interrupt
    if (_assignedBots.count(botGuid) > 0)
        return false;

    return true;
}

void InterruptCoordinator::InitializeBotCapabilities(ObjectGuid botGuid, Player* bot)
{
    if (!bot)
        return;

    auto& botInfo = _botInfo[botGuid];
    botInfo.abilities = GetClassInterrupts(static_cast<Classes>(bot->GetClass()));

    TC_LOG_DEBUG("playerbot", "InterruptCoordinator: Initialized %u interrupt abilities for %s",
                 static_cast<uint32>(botInfo.abilities.size()), bot->GetName().c_str());
}

void InterruptCoordinator::UpdateBotCooldowns(ObjectGuid botGuid)
{
    auto botIt = _botInfo.find(botGuid);
    if (botIt == _botInfo.end())
        return;

    auto& botInfo = botIt->second;
    auto now = std::chrono::steady_clock::now();

    // Remove expired cooldowns
    for (auto it = botInfo.cooldowns.begin(); it != botInfo.cooldowns.end();)
    {
        if (it->second <= now)
            it = botInfo.cooldowns.erase(it);
        else
            ++it;
    }

    // Update position information if position manager is available
    // Position manager integration placeholder - can be implemented later
}

std::vector<InterruptAbility> InterruptCoordinator::GetClassInterrupts(Classes playerClass) const
{
    std::vector<InterruptAbility> abilities;

    switch (playerClass)
    {
        case CLASS_WARRIOR:
            abilities.emplace_back(6552, 10000, 5.0f, 6000, CLASS_WARRIOR, 6);  // Pummel
            abilities.emplace_back(72, 6000, 8.0f, 6000, CLASS_WARRIOR, 6);     // Shield Bash
            break;

        case CLASS_PALADIN:
            abilities.emplace_back(96231, 15000, 10.0f, 4000, CLASS_PALADIN, 14); // Rebuke
            break;

        case CLASS_HUNTER:
            abilities.emplace_back(147362, 30000, 40.0f, 5000, CLASS_HUNTER, 6);  // Counter Shot
            abilities.emplace_back(19801, 24000, 5.0f, 5000, CLASS_HUNTER, 8);    // Tranquilizing Shot
            break;

        case CLASS_ROGUE:
            abilities.emplace_back(1766, 15000, 5.0f, 5000, CLASS_ROGUE, 6);      // Kick
            abilities.emplace_back(408, 20000, 10.0f, 8000, CLASS_ROGUE, 14);     // Kidney Shot
            break;

        case CLASS_PRIEST:
            abilities.emplace_back(15487, 45000, 30.0f, 8000, CLASS_PRIEST, 26);  // Silence
            abilities.emplace_back(8122, 30000, 8.0f, 4000, CLASS_PRIEST, 14);    // Psychic Scream
            break;

        case CLASS_DEATH_KNIGHT:
            abilities.emplace_back(47528, 15000, 5.0f, 5000, CLASS_DEATH_KNIGHT, 55); // Mind Freeze
            abilities.emplace_back(49576, 35000, 30.0f, 4000, CLASS_DEATH_KNIGHT, 56); // Death Grip
            break;

        case CLASS_SHAMAN:
            abilities.emplace_back(57994, 12000, 5.0f, 5000, CLASS_SHAMAN, 12);   // Wind Shear
            break;

        case CLASS_MAGE:
            abilities.emplace_back(2139, 24000, 40.0f, 6000, CLASS_MAGE, 6);      // Counterspell
            break;

        case CLASS_WARLOCK:
            abilities.emplace_back(19647, 24000, 30.0f, 6000, CLASS_WARLOCK, 8);  // Spell Lock (Felhunter)
            abilities.emplace_back(6789, 45000, 20.0f, 3000, CLASS_WARLOCK, 10);  // Death Coil
            break;

        case CLASS_MONK:
            abilities.emplace_back(116705, 15000, 5.0f, 4000, CLASS_MONK, 6);     // Spear Hand Strike
            break;

        case CLASS_DRUID:
            abilities.emplace_back(78675, 60000, 8.0f, 4000, CLASS_DRUID, 22);    // Solar Beam
            abilities.emplace_back(80964, 60000, 30.0f, 4000, CLASS_DRUID, 58);   // Skull Bash
            break;

        case CLASS_DEMON_HUNTER:
            abilities.emplace_back(183752, 15000, 20.0f, 3000, CLASS_DEMON_HUNTER, 99); // Disrupt
            abilities.emplace_back(179057, 60000, 8.0f, 4000, CLASS_DEMON_HUNTER, 104);  // Chaos Nova
            break;

        case CLASS_EVOKER:
            abilities.emplace_back(351338, 40000, 25.0f, 4000, CLASS_EVOKER, 58);  // Quell
            break;
    }

    return abilities;
}

InterruptPriority InterruptCoordinator::AnalyzeSpellPriority(uint32 spellId, Unit* caster) const
{
    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return InterruptPriority::IGNORE;

    // Check spell effects to determine priority
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        switch (spellInfo->GetEffect(SpellEffIndex(i)).Effect)
        {
            case SPELL_EFFECT_HEAL:
            case SPELL_EFFECT_HEAL_MAX_HEALTH:
                // Healing spells are high priority
                if (spellInfo->GetEffect(SpellEffIndex(i)).BasePoints > 2000) // Large heals
                    return InterruptPriority::CRITICAL;
                else
                    return InterruptPriority::HIGH;

            case SPELL_EFFECT_APPLY_AURA:
                switch (spellInfo->GetEffect(SpellEffIndex(i)).ApplyAuraName)
                {
                    case SPELL_AURA_TRANSFORM:
                    case SPELL_AURA_MOD_FEAR:
                    case SPELL_AURA_MOD_STUN:
                    case SPELL_AURA_MOD_CHARM:
                        return InterruptPriority::CRITICAL;

                    case SPELL_AURA_MOD_DAMAGE_PERCENT_DONE:
                    case SPELL_AURA_MOD_HEALING_DONE:
                        return InterruptPriority::HIGH;

                    default:
                        break;
                }
                break;

            case SPELL_EFFECT_SCHOOL_DAMAGE:
                // High damage spells are medium priority
                if (spellInfo->GetEffect(SpellEffIndex(i)).BasePoints > 3000)
                    return InterruptPriority::MEDIUM;
                break;

            default:
                break;
        }
    }

    // Default to low priority for unknown spells
    return InterruptPriority::LOW;
}

bool InterruptCoordinator::ShouldInterrupt(uint32 spellId, Unit* caster) const
{
    InterruptPriority priority = GetSpellPriority(spellId);

    // Always interrupt critical and high priority spells
    if (priority <= InterruptPriority::HIGH)
        return true;

    // For medium and low priority, consider group capacity
    size_t botCount = GetRegisteredBotCount();
    size_t activeCasts = GetActiveCastCount();

    // If we have plenty of interrupt capacity, interrupt medium priority spells
    if (priority == InterruptPriority::MEDIUM && activeCasts < botCount / 2)
        return true;

    // Low priority spells only if we have excess capacity
    if (priority == InterruptPriority::LOW && activeCasts < botCount / 4)
        return true;

    return false;
}

uint32 InterruptCoordinator::CalculateInterruptTiming(CastingSpellInfo const& castInfo, InterruptAbility const& ability) const
{
    // Base timing - interrupt late enough to avoid early resistance but early enough to succeed
    uint32 castTimeMs = castInfo.castTimeMs;
    uint32 minTiming = _minInterruptDelay;
    uint32 maxTiming = castTimeMs > 500 ? castTimeMs - 200 : castTimeMs - 50; // Safety margin

    // For channeled spells, interrupt quickly
    if (castInfo.isChanneled)
    {
        minTiming = std::min(minTiming, 500u);
        maxTiming = std::min(maxTiming, 1000u);
    }

    // Adjust for spell priority - interrupt critical spells faster
    switch (castInfo.priority)
    {
        case InterruptPriority::CRITICAL:
            return minTiming;

        case InterruptPriority::HIGH:
            return minTiming + (maxTiming - minTiming) / 4;

        case InterruptPriority::MEDIUM:
            return minTiming + (maxTiming - minTiming) / 2;

        case InterruptPriority::LOW:
        default:
            return minTiming + (maxTiming - minTiming) * 3 / 4;
    }
}

void InterruptCoordinator::CleanupExpiredCasts()
{
    std::unique_lock lock(_castMutex);

    for (auto it = _activeCasts.begin(); it != _activeCasts.end();)
    {
        if (it->second.IsExpired())
        {
            TC_LOG_DEBUG("playerbot", "InterruptCoordinator: Removing expired cast %u from %s",
                         it->second.spellId, it->first.ToString().c_str());
            it = _activeCasts.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void InterruptCoordinator::CleanupExpiredAssignments()
{
    std::unique_lock lock(_assignmentMutex);

    for (auto it = _pendingAssignments.begin(); it != _pendingAssignments.end();)
    {
        if (it->IsExpired() || it->executed)
        {
            _assignedBots.erase(it->assignedBot);
            it = _pendingAssignments.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void InterruptCoordinator::UpdateAssignmentDeadlines()
{
    std::shared_lock lock(_assignmentMutex);

    for (const auto& assignment : _pendingAssignments)
    {
        if (!assignment.executed && assignment.GetTimeUntilDeadline() < 100)
        {
            // Assignment is about to expire - could trigger emergency reassignment
            TC_LOG_DEBUG("playerbot", "InterruptCoordinator: Assignment for bot %s is about to expire",
                         assignment.assignedBot.ToString().c_str());
        }
    }
}

void InterruptCoordinator::OptimizeForPerformance()
{
    // Clean up old metrics data, optimize data structures, etc.
    // This is called periodically to maintain performance

    // Clean up old cooldown data
    std::unique_lock botLock(_botMutex);
    for (auto& [botGuid, botInfo] : _botInfo)
    {
        UpdateBotCooldowns(botGuid);
    }
    botLock.unlock();

    // Clean up old assignments
    CleanupExpiredAssignments();

    // Clean up old casts
    CleanupExpiredCasts();

    TC_LOG_DEBUG("playerbot", "InterruptCoordinator: Performance optimization complete - %u bots, %u active casts, %u pending assignments",
                 static_cast<uint32>(GetRegisteredBotCount()),
                 static_cast<uint32>(GetActiveCastCount()),
                 static_cast<uint32>(GetPendingAssignmentCount()));
}

void InterruptCoordinator::AddEncounterPattern(uint32 npcId, std::vector<uint32> const& spellSequence, std::vector<uint32> const& timings)
{
    if (spellSequence.size() != timings.size())
    {
        TC_LOG_ERROR("playerbot", "InterruptCoordinator: Encounter pattern size mismatch for NPC %u", npcId);
        return;
    }

    EncounterPattern pattern;
    pattern.npcId = npcId;
    pattern.spellSequence = spellSequence;
    pattern.timings = timings;

    _encounterPatterns[npcId] = pattern;

    TC_LOG_DEBUG("playerbot", "InterruptCoordinator: Added encounter pattern for NPC %u with %u spells",
                 npcId, static_cast<uint32>(spellSequence.size()));
}

void InterruptCoordinator::ClearEncounterPatterns()
{
    _encounterPatterns.clear();
}

void InterruptCoordinator::AdjustPrioritiesForGroup()
{
    if (!_group)
        return;

    // Analyze group composition and adjust spell priorities accordingly
    uint32 healerCount = 0;
    uint32 tankCount = 0;
    uint32 dpsCount = 0;

    // Count roles in group based on class and basic role detection
    for (auto const& slot : _group->GetMemberSlots())
    {
        if (Player* player = ObjectAccessor::FindPlayer(slot.guid))
        {
            Classes playerClass = static_cast<Classes>(player->GetClass());

            // Basic role detection based on class
            switch (playerClass)
            {
                case CLASS_PRIEST:
                case CLASS_SHAMAN:
                case CLASS_DRUID:
                    // These classes can heal, assume healing role for priority adjustment
                    healerCount++;
                    break;

                case CLASS_WARRIOR:
                case CLASS_PALADIN:
                case CLASS_DEATH_KNIGHT:
                case CLASS_MONK:
                case CLASS_DEMON_HUNTER:
                    // These classes can tank, assume tank role for priority adjustment
                    tankCount++;
                    break;

                default:
                    // All other classes considered DPS
                    dpsCount++;
                    break;
            }
        }
    }

    std::unique_lock lock(_castMutex);

    // Adjust priorities based on group composition
    if (healerCount == 0)
    {
        // No healers - prioritize interrupting enemy heals more
        for (auto& [spellId, priority] : _spellPriorities)
        {
            const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
            if (spellInfo && spellInfo->HasEffect(SPELL_EFFECT_HEAL))
            {
                if (priority > InterruptPriority::CRITICAL)
                    priority = static_cast<InterruptPriority>(static_cast<uint8>(priority) - 1);
            }
        }
    }

    if (tankCount == 0)
    {
        // No tanks - prioritize interrupting damage and fear effects
        for (auto& [spellId, priority] : _spellPriorities)
        {
            const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
            if (spellInfo && (spellInfo->HasEffect(SPELL_EFFECT_SCHOOL_DAMAGE) ||
                            spellInfo->HasAura(SPELL_AURA_MOD_FEAR)))
            {
                if (priority > InterruptPriority::CRITICAL)
                    priority = static_cast<InterruptPriority>(static_cast<uint8>(priority) - 1);
            }
        }
    }

    TC_LOG_DEBUG("playerbot", "InterruptCoordinator: Adjusted priorities for group composition - %u healers, %u tanks, %u dps",
                 healerCount, tankCount, dpsCount);
}

} // namespace Playerbot