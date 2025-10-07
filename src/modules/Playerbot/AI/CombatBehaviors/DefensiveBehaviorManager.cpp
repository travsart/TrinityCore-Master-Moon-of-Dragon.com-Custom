/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "DefensiveBehaviorManager.h"
#include "AI/BotAI.h"
#include "Player.h"
#include "Unit.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "Group.h"
#include "ObjectAccessor.h"
#include "Log.h"
#include "Timer.h"
#include "SpellHistory.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include <algorithm>
#include <execution>

namespace Playerbot
{

namespace
{
    // Role Detection Helpers
    enum BotRole : uint8 {
        BOT_ROLE_TANK = 0,
        BOT_ROLE_HEALER = 1,
        BOT_ROLE_DPS = 2
    };

    BotRole GetPlayerRole(Player const* player) {
        if (!player) return BOT_ROLE_DPS;
        Classes cls = static_cast<Classes>(player->GetClass());
        uint8 spec = 0; // Simplified for now - spec detection would need talent system integration
        switch (cls) {
            case CLASS_WARRIOR: return (spec == 2) ? BOT_ROLE_TANK : BOT_ROLE_DPS;
            case CLASS_PALADIN:
                if (spec == 1) return BOT_ROLE_HEALER;
                if (spec == 2) return BOT_ROLE_TANK;
                return BOT_ROLE_DPS;
            case CLASS_DEATH_KNIGHT: return (spec == 0) ? BOT_ROLE_TANK : BOT_ROLE_DPS;
            case CLASS_MONK:
                if (spec == 0) return BOT_ROLE_TANK;
                if (spec == 1) return BOT_ROLE_HEALER;
                return BOT_ROLE_DPS;
            case CLASS_DRUID:
                if (spec == 2) return BOT_ROLE_TANK;
                if (spec == 3) return BOT_ROLE_HEALER;
                return BOT_ROLE_DPS;
            case CLASS_DEMON_HUNTER: return (spec == 1) ? BOT_ROLE_TANK : BOT_ROLE_DPS;
            case CLASS_PRIEST: return (spec == 2) ? BOT_ROLE_DPS : BOT_ROLE_HEALER;
            case CLASS_SHAMAN: return (spec == 2) ? BOT_ROLE_HEALER : BOT_ROLE_DPS;
            default: return BOT_ROLE_DPS;
        }
    }
    bool IsTank(Player const* p) { return GetPlayerRole(p) == BOT_ROLE_TANK; }
    bool IsHealer(Player const* p) { return GetPlayerRole(p) == BOT_ROLE_HEALER; }

    // Spell IDs for defensive cooldowns by class
    enum DefensiveSpells : uint32
    {
        // Warrior
        SHIELD_WALL = 871,
        LAST_STAND = 12975,
        SHIELD_BLOCK = 2565,
        ENRAGED_REGENERATION = 55694,
        SPELL_REFLECTION = 23920,
        BERSERKER_RAGE = 18499,
        DEFENSIVE_STANCE = 71,

        // Paladin
        DIVINE_SHIELD = 642,
        DIVINE_PROTECTION = 498,
        LAY_ON_HANDS = 48788,
        HAND_OF_PROTECTION = 10278,
        HAND_OF_SACRIFICE = 6940,
        ARDENT_DEFENDER = 31850,
        SACRED_SHIELD = 53601,

        // Hunter
        DETERRENCE = 19263,
        FEIGN_DEATH = 5384,
        DISENGAGE = 781,
        ASPECT_OF_THE_MONKEY = 13163,
        MASTERS_CALL = 53271,

        // Rogue
        EVASION = 5277,
        CLOAK_OF_SHADOWS = 31224,
        VANISH = 26889,
        SPRINT = 11305,
        COMBAT_READINESS = 74001,
        PREPARATION = 14185,

        // Priest
        PAIN_SUPPRESSION = 33206,
        GUARDIAN_SPIRIT = 47788,
        POWER_WORD_SHIELD = 48066,
        DESPERATE_PRAYER = 48173,
        DISPERSION = 47585,
        FADE = 586,
        PSYCHIC_SCREAM = 10890,

        // Death Knight
        ICEBOUND_FORTITUDE = 48792,
        ANTI_MAGIC_SHELL = 48707,
        VAMPIRIC_BLOOD = 55233,
        BONE_SHIELD = 49222,
        UNBREAKABLE_ARMOR = 51271,
        LICHBORNE = 49039,
        RUNE_TAP = 48982,

        // Shaman
        SHAMANISTIC_RAGE = 30823,
        ASTRAL_SHIFT = 51490,
        EARTH_ELEMENTAL_TOTEM = 2062,
        NATURE_SWIFTNESS = 16188,
        GROUNDING_TOTEM = 8177,

        // Mage
        ICE_BLOCK = 45438,
        ICE_BARRIER = 43039,
        MANA_SHIELD = 43020,
        BLINK = 1953,
        INVISIBILITY = 66,
        MIRROR_IMAGE = 55342,
        FROST_NOVA = 42917,

        // Warlock
        SHADOW_WARD = 47891,
        DEMONIC_CIRCLE_TELEPORT = 48020,
        DARK_PACT = 59092,
        HOWL_OF_TERROR = 17928,
        DEATH_COIL = 47860,
        SOULSHATTER = 29858,

        // Druid
        BARKSKIN = 22812,
        SURVIVAL_INSTINCTS = 61336,
        FRENZIED_REGENERATION = 22842,
        NATURE_GRASP = 53312,
        DASH = 33357,
        TRANQUILITY = 48447,

        // Consumables
        HEALTH_POTION = 54736,  // Runic Healing Potion
        HEALTHSTONE = 47875,     // Demonic Healthstone
        HEAVY_FROSTWEAVE_BANDAGE = 45545
    };

    // Helper function to check if damage type is magical
    bool IsMagicalDamage(uint32 schoolMask)
    {
        return (schoolMask & ~SPELL_SCHOOL_MASK_NORMAL) != 0;
    }

    // Helper function to calculate linear prediction
    float LinearPredict(float currentValue, float rateOfChange, float timeAhead)
    {
        return std::max(0.0f, currentValue + (rateOfChange * timeAhead));
    }
}

// Constructor
DefensiveBehaviorManager::DefensiveBehaviorManager(BotAI* ai)
    : _ai(ai)
    , _bot(ai ? ai->GetBot() : nullptr)
    , _cachedPriority(DefensivePriority::PRIORITY_PREEMPTIVE)
    , _priorityCacheTime(0)
    , _damageHistoryIndex(0)
    , _sortedDefensivesTime(0)
{
    // Initialize damage history circular buffer
    _damageHistory.resize(DAMAGE_HISTORY_SIZE);
    for (auto& entry : _damageHistory)
    {
        entry.damage = 0;
        entry.timestamp = 0;
        entry.isMagical = false;
    }

    // Initialize role-specific thresholds
    if (_bot)
    {
        BotRole role = GetPlayerRole(_bot);
        _thresholds = GetRoleThresholds(static_cast<uint8>(role));

        // Initialize class-specific defensive cooldowns
        InitializeClassDefensives();
    }

    // Initialize performance metrics
    _metrics = PerformanceMetrics{};
}

// Destructor
DefensiveBehaviorManager::~DefensiveBehaviorManager() = default;

// Main update method
void DefensiveBehaviorManager::Update(uint32 diff)
{
    if (!_bot || !_bot->IsAlive())
        return;

    auto startTime = std::chrono::steady_clock::now();

    // Update defensive state (throttled for performance)
    uint32 currentTime = getMSTime();
    if (currentTime - _currentState.lastUpdateTime >= STATE_UPDATE_INTERVAL)
    {
        UpdateState();
        _currentState.lastUpdateTime = currentTime;
    }

    // Update external defensive requests
    CoordinateExternalDefensives();

    // Clean up old external requests (older than 5 seconds)
    _externalRequests.erase(
        std::remove_if(_externalRequests.begin(), _externalRequests.end(),
            [currentTime](const ExternalDefensiveRequest& req) {
                return (currentTime - req.requestTime) > 5000;
            }),
        _externalRequests.end()
    );

    // Clean up old provided defensives tracking
    for (auto it = _providedDefensives.begin(); it != _providedDefensives.end(); )
    {
        if (currentTime - it->second > 10000) // 10 second cooldown on helping same target
            it = _providedDefensives.erase(it);
        else
            ++it;
    }

    // Track performance metrics
    UpdateMetrics(startTime);
}

// Update current defensive state
void DefensiveBehaviorManager::UpdateState()
{
    if (!_bot)
        return;

    // Update health status
    _currentState.healthPercent = _bot->GetHealthPct();
    _currentState.incomingDPS = GetIncomingDPS();
    _currentState.predictedHealth = PredictHealth(2.0f);

    // Count debuffs
    _currentState.debuffCount = 0;
    _currentState.hasMajorDebuff = false;

    Unit::AuraApplicationMap const& appliedAuras = _bot->GetAppliedAuras();
    for (auto const& [spellId, auraApp] : appliedAuras)
    {
        if (!auraApp || !auraApp->GetBase())
            continue;

        Aura* aura = auraApp->GetBase();
        if (!aura->GetSpellInfo()->IsPositive())
        {
            _currentState.debuffCount++;

            // Check for major debuffs (stuns, fears, etc.)
            if (aura->HasEffectType(SPELL_AURA_MOD_STUN) ||
                aura->HasEffectType(SPELL_AURA_MOD_FEAR) ||
                aura->HasEffectType(SPELL_AURA_MOD_CONFUSE) ||
                aura->HasEffectType(SPELL_AURA_MOD_CHARM) ||
                aura->HasEffectType(SPELL_AURA_MOD_PACIFY) ||
                aura->HasEffectType(SPELL_AURA_MOD_SILENCE))
            {
                _currentState.hasMajorDebuff = true;
            }
        }
    }

    // Count nearby enemies
    _currentState.nearbyEnemies = CountNearbyEnemies(10.0f);

    // Check group status
    _currentState.tankDead = false;
    _currentState.healerOOM = false;

    if (Group* group = _bot->GetGroup())
    {
        for (GroupReference& ref : group->GetMembers())
        {
            if (Player* member = ref.GetSource())
            {
                if (member == _bot)
                    continue;

                // Check tank status
                if (GetPlayerRole(member) == BOT_ROLE_TANK && !member->IsAlive())
                    _currentState.tankDead = true;

                // Check healer mana
                if (GetPlayerRole(member) == BOT_ROLE_HEALER && member->IsAlive())
                {
                    if (member->GetPowerPct(POWER_MANA) < 20.0f)
                        _currentState.healerOOM = true;
                }
            }
        }
    }
}

// Check if bot needs defensive
bool DefensiveBehaviorManager::NeedsDefensive() const
{
    if (!_bot || !_bot->IsAlive() || !_bot->IsInCombat())
        return false;

    // Quick health check
    if (_currentState.healthPercent > _thresholds.preemptiveHP &&
        _currentState.incomingDPS < _thresholds.incomingDPSThreshold)
        return false;

    // Critical health - always need defensive
    if (_currentState.healthPercent <= _thresholds.criticalHP)
        return true;

    // Major debuff - consider defensive
    if (_currentState.hasMajorDebuff && _currentState.healthPercent <= _thresholds.majorCooldownHP)
        return true;

    // High incoming damage
    float maxHPPerSec = _bot->GetMaxHealth() * _thresholds.incomingDPSThreshold;
    if (_currentState.incomingDPS > maxHPPerSec)
        return true;

    // Predicted death within 2 seconds
    if (_currentState.predictedHealth <= _thresholds.criticalHP)
        return true;

    // Tank dead and taking damage
    if (_currentState.tankDead && _currentState.healthPercent <= _thresholds.minorCooldownHP)
        return true;

    // Healer OOM and low health
    if (_currentState.healerOOM && _currentState.healthPercent <= _thresholds.majorCooldownHP)
        return true;

    // Multiple enemies and moderate damage
    if (_currentState.nearbyEnemies >= 3 && _currentState.healthPercent <= _thresholds.minorCooldownHP)
        return true;

    return false;
}

// Select best defensive cooldown
uint32 DefensiveBehaviorManager::SelectDefensive() const
{
    if (!_bot)
        return 0;

    DefensivePriority priority = GetCurrentPriority();
    return SelectBestDefensive(priority);
}

// Get current defensive priority
DefensiveBehaviorManager::DefensivePriority DefensiveBehaviorManager::GetCurrentPriority() const
{
    uint32 currentTime = getMSTime();

    // Use cached priority if still valid
    if (currentTime - _priorityCacheTime < PRIORITY_CACHE_DURATION)
        return _cachedPriority;

    _cachedPriority = EvaluatePriority();
    _priorityCacheTime = currentTime;
    return _cachedPriority;
}

// Evaluate current priority level
DefensiveBehaviorManager::DefensivePriority DefensiveBehaviorManager::EvaluatePriority() const
{
    if (_currentState.healthPercent <= _thresholds.criticalHP)
        return DefensivePriority::PRIORITY_CRITICAL;

    if (_currentState.healthPercent <= _thresholds.majorCooldownHP)
        return DefensivePriority::PRIORITY_MAJOR;

    if (_currentState.healthPercent <= _thresholds.minorCooldownHP)
        return DefensivePriority::PRIORITY_MODERATE;

    if (_currentState.healthPercent <= _thresholds.preemptiveHP)
        return DefensivePriority::PRIORITY_MINOR;

    return DefensivePriority::PRIORITY_PREEMPTIVE;
}

// Register damage taken
void DefensiveBehaviorManager::RegisterDamage(uint32 damage, uint32 timestamp)
{
    if (timestamp == 0)
        timestamp = getMSTime();

    // Store in circular buffer
    DamageEntry& entry = _damageHistory[_damageHistoryIndex];
    entry.damage = damage;
    entry.timestamp = timestamp;
    entry.isMagical = false; // Would need spell school info for accuracy

    _damageHistoryIndex = (_damageHistoryIndex + 1) % DAMAGE_HISTORY_SIZE;
}

// Prepare for incoming spike damage
void DefensiveBehaviorManager::PrepareForIncoming(uint32 spellId)
{
    if (!_bot || !spellId)
        return;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return;

    // Check if this is a major damage spell
    bool isMajorThreat = false;
    for (auto const& effect : spellInfo->GetEffects())
    {
        if (effect.IsEffect(SPELL_EFFECT_SCHOOL_DAMAGE) ||
            effect.IsEffect(SPELL_EFFECT_WEAPON_DAMAGE) ||
            effect.IsEffect(SPELL_EFFECT_HEALTH_LEECH))
        {
            // Estimate potential damage (simplified)
            uint32 estimatedDamage = effect.CalcValue() * 2; // Rough estimate
            if (estimatedDamage > _bot->GetMaxHealth() * 0.3f)
                isMajorThreat = true;
        }
    }

    // Pre-emptively use defensive if major threat detected
    if (isMajorThreat && _currentState.healthPercent < _thresholds.minorCooldownHP)
    {
        uint32 defensive = SelectDefensive();
        if (defensive && !_bot->GetSpellHistory()->HasCooldown(defensive))
        {
            _bot->CastSpell(_bot, defensive, false);
            MarkDefensiveUsed(defensive);
        }
    }
}

// Calculate incoming DPS
float DefensiveBehaviorManager::GetIncomingDPS() const
{
    uint32 currentTime = getMSTime();
    uint32 totalDamage = 0;
    uint32 oldestTime = currentTime;

    // Sum damage over last 3 seconds
    for (const auto& entry : _damageHistory)
    {
        if (entry.timestamp == 0)
            continue;

        uint32 age = currentTime - entry.timestamp;
        if (age <= 3000) // Last 3 seconds
        {
            totalDamage += entry.damage;
            if (entry.timestamp < oldestTime)
                oldestTime = entry.timestamp;
        }
    }

    // Calculate DPS
    float timeSpan = (currentTime - oldestTime) / 1000.0f;
    if (timeSpan < 0.1f)
        timeSpan = 0.1f;

    return totalDamage / timeSpan;
}

// Predict future health
float DefensiveBehaviorManager::PredictHealth(float secondsAhead) const
{
    if (!_bot)
        return 0.0f;

    float currentHP = _bot->GetHealth();
    float dps = GetIncomingDPS();
    float predictedHP = LinearPredict(currentHP, -dps, secondsAhead);

    return (predictedHP / _bot->GetMaxHealth()) * 100.0f;
}

// Register a defensive cooldown
void DefensiveBehaviorManager::RegisterDefensiveCooldown(const DefensiveCooldown& cooldown)
{
    _defensiveCooldowns[cooldown.spellId] = cooldown;
    _sortedDefensivesTime = 0; // Invalidate cache
}

// Check if defensive is available
bool DefensiveBehaviorManager::IsDefensiveAvailable(uint32 spellId) const
{
    if (!_bot || !spellId)
        return false;

    // Check if we have the spell
    if (!_bot->HasSpell(spellId))
        return false;

    // Check cooldown
    if (_bot->GetSpellHistory()->HasCooldown(spellId))
        return false;

    // Check if defensive exists in our registry
    auto it = _defensiveCooldowns.find(spellId);
    if (it == _defensiveCooldowns.end())
        return false;

    // Check health range requirements
    const DefensiveCooldown& cooldown = it->second;
    if (_currentState.healthPercent < cooldown.maxHealthPercent ||
        _currentState.healthPercent > cooldown.minHealthPercent)
        return false;

    return MeetsRequirements(cooldown);
}

// Mark defensive as used
void DefensiveBehaviorManager::MarkDefensiveUsed(uint32 spellId)
{
    auto it = _defensiveCooldowns.find(spellId);
    if (it != _defensiveCooldowns.end())
    {
        it->second.lastUsedTime = getMSTime();
        it->second.usageCount++;
        _metrics.defensivesUsed++;
    }
}

// Request external defensive from group
void DefensiveBehaviorManager::RequestExternalDefensive(ObjectGuid target, DefensivePriority priority)
{
    // Check if we already have a request for this target
    for (auto& req : _externalRequests)
    {
        if (req.targetGuid == target && !req.fulfilled)
        {
            // Update priority if higher
            if (priority > req.priority)
                req.priority = priority;
            return;
        }
    }

    // Add new request
    ExternalDefensiveRequest request;
    request.targetGuid = target;
    request.priority = priority;
    request.requestTime = getMSTime();
    request.fulfilled = false;

    if (Unit* targetUnit = ObjectAccessor::GetUnit(*_bot, target))
    {
        request.healthPercent = targetUnit->GetHealthPct();
    }

    _externalRequests.push_back(request);
}

// Get target for external defensive
ObjectGuid DefensiveBehaviorManager::GetExternalDefensiveTarget() const
{
    if (!_bot || _externalRequests.empty())
        return ObjectGuid::Empty;

    // Find highest priority unfulfilled request
    const ExternalDefensiveRequest* bestRequest = nullptr;
    for (const auto& req : _externalRequests)
    {
        if (req.fulfilled)
            continue;

        // Check if we've already helped this target recently
        auto it = _providedDefensives.find(req.targetGuid);
        if (it != _providedDefensives.end())
        {
            if (getMSTime() - it->second < 10000) // 10 second cooldown
                continue;
        }

        if (!bestRequest || req.priority > bestRequest->priority)
            bestRequest = &req;
    }

    return bestRequest ? bestRequest->targetGuid : ObjectGuid::Empty;
}

// Coordinate external defensives across group
void DefensiveBehaviorManager::CoordinateExternalDefensives()
{
    if (!_bot || !_bot->GetGroup())
        return;

    // Check our own health first
    if (_currentState.healthPercent < _thresholds.majorCooldownHP)
    {
        RequestExternalDefensive(_bot->GetGUID(), GetCurrentPriority());
    }

    // Check if we can provide external defensive
    ObjectGuid targetGuid = GetExternalDefensiveTarget();
    if (!targetGuid.IsEmpty())
    {
        Unit* target = ObjectAccessor::GetUnit(*_bot, targetGuid);
        if (target && target->IsAlive())
        {
            // Try to provide external defensive based on class
            bool provided = false;
            switch (_bot->GetClass())
            {
                case CLASS_PALADIN:
                    if (!_bot->GetSpellHistory()->HasCooldown(HAND_OF_PROTECTION))
                    {
                        _bot->CastSpell(target, HAND_OF_PROTECTION, false);
                        provided = true;
                    }
                    else if (!_bot->GetSpellHistory()->HasCooldown(HAND_OF_SACRIFICE))
                    {
                        _bot->CastSpell(target, HAND_OF_SACRIFICE, false);
                        provided = true;
                    }
                    break;

                case CLASS_PRIEST:
                    if (!_bot->GetSpellHistory()->HasCooldown(PAIN_SUPPRESSION))
                    {
                        _bot->CastSpell(target, PAIN_SUPPRESSION, false);
                        provided = true;
                    }
                    else if (!_bot->GetSpellHistory()->HasCooldown(GUARDIAN_SPIRIT))
                    {
                        _bot->CastSpell(target, GUARDIAN_SPIRIT, false);
                        provided = true;
                    }
                    break;
            }

            if (provided)
            {
                _providedDefensives[targetGuid] = getMSTime();
                _metrics.externalDefensivesProvided++;

                // Mark request as fulfilled
                for (auto& req : _externalRequests)
                {
                    if (req.targetGuid == targetGuid)
                    {
                        req.fulfilled = true;
                        break;
                    }
                }
            }
        }
    }
}

// Get role-specific thresholds (static)
DefensiveBehaviorManager::RoleThresholds DefensiveBehaviorManager::GetRoleThresholds(uint8 role)
{
    RoleThresholds thresholds;

    switch (role)
    {
        case BOT_ROLE_TANK:
            thresholds.criticalHP = 0.15f;
            thresholds.majorCooldownHP = 0.35f;
            thresholds.minorCooldownHP = 0.55f;
            thresholds.preemptiveHP = 0.75f;
            thresholds.incomingDPSThreshold = 0.40f;
            thresholds.fleeEnemyCount = 8;
            break;

        case BOT_ROLE_HEALER:
            thresholds.criticalHP = 0.25f;
            thresholds.majorCooldownHP = 0.45f;
            thresholds.minorCooldownHP = 0.65f;
            thresholds.preemptiveHP = 0.85f;
            thresholds.incomingDPSThreshold = 0.25f;
            thresholds.fleeEnemyCount = 3;
            break;

        case BOT_ROLE_DPS:
        default:
            thresholds.criticalHP = 0.20f;
            thresholds.majorCooldownHP = 0.40f;
            thresholds.minorCooldownHP = 0.60f;
            thresholds.preemptiveHP = 0.80f;
            thresholds.incomingDPSThreshold = 0.30f;
            thresholds.fleeEnemyCount = 5;
            break;
    }

    return thresholds;
}

// Check if should use health potion
bool DefensiveBehaviorManager::ShouldUseHealthPotion() const
{
    if (!_bot || !_bot->IsInCombat())
        return false;

    // Check health threshold
    if (_currentState.healthPercent > 40.0f)
        return false;

    // Check if potion is on cooldown (2 minute shared cooldown)
    if (_bot->GetSpellHistory()->HasCooldown(HEALTH_POTION))
        return false;

    // Check if we have potions in inventory
    // Simplified check - would need inventory scanning in real implementation
    return true;
}

// Check if should use healthstone
bool DefensiveBehaviorManager::ShouldUseHealthstone() const
{
    if (!_bot || !_bot->IsInCombat())
        return false;

    // Check health threshold
    if (_currentState.healthPercent > 35.0f)
        return false;

    // Check if healthstone is on cooldown
    if (_bot->GetSpellHistory()->HasCooldown(HEALTHSTONE))
        return false;

    // Check if we have healthstone in inventory
    // Simplified check - would need inventory scanning in real implementation
    return true;
}

// Check if should use bandage
bool DefensiveBehaviorManager::ShouldUseBandage() const
{
    if (!_bot || _bot->IsInCombat())
        return false;

    // Check health threshold
    if (_currentState.healthPercent > 60.0f)
        return false;

    // Check if we have First Aid skill
    // Simplified check - would need skill checking in real implementation
    return true;
}

// Get class-specific defensives (static)
std::vector<DefensiveBehaviorManager::DefensiveCooldown> DefensiveBehaviorManager::GetClassDefensives(uint8 classId)
{
    std::vector<DefensiveCooldown> defensives;

    switch (classId)
    {
        case CLASS_WARRIOR:
        {
            defensives.push_back({SHIELD_WALL, DefensiveSpellTier::TIER_MAJOR_REDUCTION, 0.0f, 50.0f, 300000, 12000, true});
            defensives.push_back({LAST_STAND, DefensiveSpellTier::TIER_REGENERATION, 0.0f, 40.0f, 180000, 20000, true});
            defensives.push_back({ENRAGED_REGENERATION, DefensiveSpellTier::TIER_REGENERATION, 0.0f, 60.0f, 180000, 10000, true});
            defensives.push_back({SHIELD_BLOCK, DefensiveSpellTier::TIER_AVOIDANCE, 0.0f, 70.0f, 60000, 10000, false, false, false, true});
            defensives.push_back({SPELL_REFLECTION, DefensiveSpellTier::TIER_AVOIDANCE, 0.0f, 100.0f, 10000, 5000, false, false, false, false, true});
            defensives.push_back({BERSERKER_RAGE, DefensiveSpellTier::TIER_AVOIDANCE, 0.0f, 100.0f, 30000, 10000, false});
            break;
        }

        case CLASS_PALADIN:
        {
            defensives.push_back({DIVINE_SHIELD, DefensiveSpellTier::TIER_IMMUNITY, 0.0f, 20.0f, 300000, 12000, true});
            defensives.push_back({DIVINE_PROTECTION, DefensiveSpellTier::TIER_MAJOR_REDUCTION, 0.0f, 50.0f, 60000, 12000, true});
            defensives.push_back({LAY_ON_HANDS, DefensiveSpellTier::TIER_REGENERATION, 0.0f, 15.0f, 1200000, 0, true});
            defensives.push_back({ARDENT_DEFENDER, DefensiveSpellTier::TIER_MODERATE_REDUCTION, 0.0f, 40.0f, 120000, 10000, true});
            defensives.push_back({SACRED_SHIELD, DefensiveSpellTier::TIER_MODERATE_REDUCTION, 0.0f, 80.0f, 0, 60000, true});
            break;
        }

        case CLASS_HUNTER:
        {
            defensives.push_back({DETERRENCE, DefensiveSpellTier::TIER_IMMUNITY, 0.0f, 30.0f, 90000, 5000, true});
            defensives.push_back({FEIGN_DEATH, DefensiveSpellTier::TIER_AVOIDANCE, 0.0f, 50.0f, 30000, 0, true, true});
            defensives.push_back({DISENGAGE, DefensiveSpellTier::TIER_AVOIDANCE, 0.0f, 100.0f, 16000, 0, true});
            defensives.push_back({ASPECT_OF_THE_MONKEY, DefensiveSpellTier::TIER_AVOIDANCE, 0.0f, 100.0f, 0, 0, true});
            defensives.push_back({MASTERS_CALL, DefensiveSpellTier::TIER_AVOIDANCE, 0.0f, 100.0f, 60000, 4000, true});
            break;
        }

        case CLASS_ROGUE:
        {
            defensives.push_back({EVASION, DefensiveSpellTier::TIER_AVOIDANCE, 0.0f, 40.0f, 180000, 15000, true, false, false, true});
            defensives.push_back({CLOAK_OF_SHADOWS, DefensiveSpellTier::TIER_MAJOR_REDUCTION, 0.0f, 50.0f, 90000, 5000, true, false, false, false, true});
            defensives.push_back({VANISH, DefensiveSpellTier::TIER_IMMUNITY, 0.0f, 30.0f, 180000, 3000, true, true});
            defensives.push_back({SPRINT, DefensiveSpellTier::TIER_AVOIDANCE, 0.0f, 100.0f, 180000, 15000, true});
            defensives.push_back({COMBAT_READINESS, DefensiveSpellTier::TIER_MODERATE_REDUCTION, 0.0f, 60.0f, 120000, 20000, true});
            break;
        }

        case CLASS_PRIEST:
        {
            defensives.push_back({POWER_WORD_SHIELD, DefensiveSpellTier::TIER_MODERATE_REDUCTION, 0.0f, 90.0f, 4000, 15000, true});
            defensives.push_back({DESPERATE_PRAYER, DefensiveSpellTier::TIER_REGENERATION, 0.0f, 50.0f, 120000, 0, true});
            defensives.push_back({DISPERSION, DefensiveSpellTier::TIER_MAJOR_REDUCTION, 0.0f, 30.0f, 120000, 6000, true});
            defensives.push_back({FADE, DefensiveSpellTier::TIER_AVOIDANCE, 0.0f, 100.0f, 24000, 10000, true});
            defensives.push_back({PSYCHIC_SCREAM, DefensiveSpellTier::TIER_AVOIDANCE, 0.0f, 50.0f, 30000, 0, true, false, false, false, false, true, 3});
            break;
        }

        case CLASS_DEATH_KNIGHT:
        {
            defensives.push_back({ICEBOUND_FORTITUDE, DefensiveSpellTier::TIER_MAJOR_REDUCTION, 0.0f, 40.0f, 120000, 12000, true});
            defensives.push_back({ANTI_MAGIC_SHELL, DefensiveSpellTier::TIER_MAJOR_REDUCTION, 0.0f, 60.0f, 45000, 5000, true, false, false, false, true});
            defensives.push_back({VAMPIRIC_BLOOD, DefensiveSpellTier::TIER_REGENERATION, 0.0f, 50.0f, 60000, 10000, true});
            defensives.push_back({BONE_SHIELD, DefensiveSpellTier::TIER_MODERATE_REDUCTION, 0.0f, 80.0f, 60000, 300000, true});
            defensives.push_back({UNBREAKABLE_ARMOR, DefensiveSpellTier::TIER_MODERATE_REDUCTION, 0.0f, 60.0f, 60000, 20000, true});
            defensives.push_back({RUNE_TAP, DefensiveSpellTier::TIER_REGENERATION, 0.0f, 60.0f, 30000, 0, true});
            break;
        }

        case CLASS_SHAMAN:
        {
            defensives.push_back({SHAMANISTIC_RAGE, DefensiveSpellTier::TIER_MODERATE_REDUCTION, 0.0f, 50.0f, 60000, 15000, true});
            defensives.push_back({ASTRAL_SHIFT, DefensiveSpellTier::TIER_MODERATE_REDUCTION, 0.0f, 40.0f, 120000, 6000, true});
            defensives.push_back({EARTH_ELEMENTAL_TOTEM, DefensiveSpellTier::TIER_AVOIDANCE, 0.0f, 30.0f, 600000, 120000, true});
            defensives.push_back({GROUNDING_TOTEM, DefensiveSpellTier::TIER_AVOIDANCE, 0.0f, 100.0f, 15000, 45000, true, false, false, false, true});
            break;
        }

        case CLASS_MAGE:
        {
            defensives.push_back({ICE_BLOCK, DefensiveSpellTier::TIER_IMMUNITY, 0.0f, 25.0f, 300000, 10000, true});
            defensives.push_back({ICE_BARRIER, DefensiveSpellTier::TIER_MODERATE_REDUCTION, 0.0f, 70.0f, 30000, 60000, true});
            defensives.push_back({MANA_SHIELD, DefensiveSpellTier::TIER_MODERATE_REDUCTION, 0.0f, 60.0f, 0, 60000, true});
            defensives.push_back({BLINK, DefensiveSpellTier::TIER_AVOIDANCE, 0.0f, 100.0f, 15000, 0, true});
            defensives.push_back({INVISIBILITY, DefensiveSpellTier::TIER_AVOIDANCE, 0.0f, 50.0f, 180000, 20000, true, true});
            defensives.push_back({MIRROR_IMAGE, DefensiveSpellTier::TIER_AVOIDANCE, 0.0f, 40.0f, 180000, 30000, true});
            break;
        }

        case CLASS_WARLOCK:
        {
            defensives.push_back({SHADOW_WARD, DefensiveSpellTier::TIER_MODERATE_REDUCTION, 0.0f, 60.0f, 30000, 30000, true, false, false, false, true});
            defensives.push_back({DEMONIC_CIRCLE_TELEPORT, DefensiveSpellTier::TIER_AVOIDANCE, 0.0f, 100.0f, 30000, 0, true});
            defensives.push_back({HOWL_OF_TERROR, DefensiveSpellTier::TIER_AVOIDANCE, 0.0f, 40.0f, 40000, 0, true, false, false, false, false, true, 3});
            defensives.push_back({DEATH_COIL, DefensiveSpellTier::TIER_REGENERATION, 0.0f, 50.0f, 120000, 0, true});
            defensives.push_back({SOULSHATTER, DefensiveSpellTier::TIER_AVOIDANCE, 0.0f, 50.0f, 180000, 0, true});
            break;
        }

        case CLASS_DRUID:
        {
            defensives.push_back({BARKSKIN, DefensiveSpellTier::TIER_MODERATE_REDUCTION, 0.0f, 60.0f, 60000, 12000, false});
            defensives.push_back({SURVIVAL_INSTINCTS, DefensiveSpellTier::TIER_MAJOR_REDUCTION, 0.0f, 30.0f, 180000, 20000, true});
            defensives.push_back({FRENZIED_REGENERATION, DefensiveSpellTier::TIER_REGENERATION, 0.0f, 50.0f, 180000, 10000, true});
            defensives.push_back({NATURE_GRASP, DefensiveSpellTier::TIER_AVOIDANCE, 0.0f, 70.0f, 60000, 45000, true, false, false, true});
            defensives.push_back({DASH, DefensiveSpellTier::TIER_AVOIDANCE, 0.0f, 100.0f, 180000, 15000, true});
            break;
        }
    }

    return defensives;
}

// Initialize class-specific defensives
void DefensiveBehaviorManager::InitializeClassDefensives()
{
    if (!_bot)
        return;

    std::vector<DefensiveCooldown> classDefensives = GetClassDefensives(_bot->GetClass());
    for (const auto& defensive : classDefensives)
    {
        // Only register if bot has the spell
        if (_bot->HasSpell(defensive.spellId))
        {
            RegisterDefensiveCooldown(defensive);
        }
    }
}

// Reset all state
void DefensiveBehaviorManager::Reset()
{
    _currentState = DefensiveState{};
    _cachedPriority = DefensivePriority::PRIORITY_PREEMPTIVE;
    _priorityCacheTime = 0;
    _sortedDefensivesTime = 0;

    // Clear damage history
    for (auto& entry : _damageHistory)
    {
        entry.damage = 0;
        entry.timestamp = 0;
        entry.isMagical = false;
    }
    _damageHistoryIndex = 0;

    // Clear requests
    _externalRequests.clear();
    _providedDefensives.clear();

    // Reset usage tracking but keep defensive definitions
    for (auto& [spellId, cooldown] : _defensiveCooldowns)
    {
        cooldown.lastUsedTime = 0;
        cooldown.usageCount = 0;
    }
}

// Select best defensive based on priority
uint32 DefensiveBehaviorManager::SelectBestDefensive(DefensivePriority priority) const
{
    if (!_bot)
        return 0;

    uint32 currentTime = getMSTime();

    // Update sorted defensives cache if needed
    if (currentTime - _sortedDefensivesTime >= SORTED_DEFENSIVES_CACHE_DURATION)
    {
        _sortedDefensives.clear();
        for (const auto& [spellId, cooldown] : _defensiveCooldowns)
        {
            if (IsDefensiveAvailable(spellId))
                _sortedDefensives.push_back(spellId);
        }

        // Sort by tier and score
        std::sort(_sortedDefensives.begin(), _sortedDefensives.end(),
            [this, priority](uint32 a, uint32 b) {
                const DefensiveCooldown& cdA = _defensiveCooldowns.at(a);
                const DefensiveCooldown& cdB = _defensiveCooldowns.at(b);

                // Higher tier is better
                if (cdA.tier != cdB.tier)
                    return cdA.tier > cdB.tier;

                // Higher score is better
                return CalculateDefensiveScore(cdA, priority) > CalculateDefensiveScore(cdB, priority);
            });

        _sortedDefensivesTime = currentTime;
    }

    // Return best available defensive
    for (uint32 spellId : _sortedDefensives)
    {
        if (IsDefensiveAvailable(spellId))
            return spellId;
    }

    return 0;
}

// Check if defensive meets requirements
bool DefensiveBehaviorManager::MeetsRequirements(const DefensiveCooldown& cooldown) const
{
    // Check melee requirement
    if (cooldown.requiresMelee && !IsDamageMostlyPhysical())
        return false;

    // Check magic requirement
    if (cooldown.requiresMagic && !IsDamageMostlyMagical())
        return false;

    // Check multiple enemies requirement
    if (cooldown.requiresMultipleEnemies && _currentState.nearbyEnemies < 2)
        return false;

    // Check minimum enemy count
    if (cooldown.minEnemyCount > 0 && _currentState.nearbyEnemies < cooldown.minEnemyCount)
        return false;

    // Check if it would break on damage
    if (cooldown.breakOnDamage && _currentState.incomingDPS > 0)
        return false;

    return true;
}

// Calculate defensive score
float DefensiveBehaviorManager::CalculateDefensiveScore(const DefensiveCooldown& cooldown,
                                                         DefensivePriority priority) const
{
    float score = 100.0f;

    // Tier weight (higher tier = better)
    score += static_cast<float>(cooldown.tier) * 20.0f;

    // Priority matching (use stronger defensives for higher priority)
    float priorityMatch = std::abs(static_cast<float>(cooldown.tier) - static_cast<float>(priority));
    score -= priorityMatch * 10.0f;

    // Duration bonus (longer = better)
    score += (cooldown.durationMs / 1000.0f) * 2.0f;

    // Cooldown penalty (longer cooldown = worse)
    score -= (cooldown.cooldownMs / 10000.0f) * 5.0f;

    // GCD penalty (no GCD = better for emergencies)
    if (!cooldown.requiresGCD)
        score += 15.0f;

    // Recent usage penalty
    uint32 timeSinceUse = getMSTime() - cooldown.lastUsedTime;
    if (timeSinceUse < 30000) // Used in last 30 seconds
        score -= 20.0f;

    // Health range bonus (if we're in optimal range)
    float healthMidpoint = (cooldown.minHealthPercent + cooldown.maxHealthPercent) / 2.0f;
    float healthDistance = std::abs(_currentState.healthPercent - healthMidpoint);
    score -= healthDistance * 0.5f;

    return score;
}

// Count nearby enemies
uint32 DefensiveBehaviorManager::CountNearbyEnemies(float range) const
{
    if (!_bot)
        return 0;

    std::list<Unit*> enemies;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(_bot, _bot, range);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(_bot, enemies, u_check);
    Cell::VisitAllObjects(_bot, searcher, range);
    return enemies.size();
}

// Check if damage is mostly magical
bool DefensiveBehaviorManager::IsDamageMostlyMagical() const
{
    uint32 magicalDamage = 0;
    uint32 physicalDamage = 0;
    uint32 currentTime = getMSTime();

    for (const auto& entry : _damageHistory)
    {
        if (entry.timestamp == 0)
            continue;

        uint32 age = currentTime - entry.timestamp;
        if (age <= 3000) // Last 3 seconds
        {
            if (entry.isMagical)
                magicalDamage += entry.damage;
            else
                physicalDamage += entry.damage;
        }
    }

    return magicalDamage > physicalDamage;
}

// Check if damage is mostly physical
bool DefensiveBehaviorManager::IsDamageMostlyPhysical() const
{
    return !IsDamageMostlyMagical();
}

// Update performance metrics
void DefensiveBehaviorManager::UpdateMetrics(std::chrono::steady_clock::time_point startTime)
{
    auto endTime = std::chrono::steady_clock::now();
    auto updateTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    _metrics.updatesPerformed++;

    // Update average (simple moving average)
    if (_metrics.averageUpdateTime.count() == 0)
        _metrics.averageUpdateTime = updateTime;
    else
        _metrics.averageUpdateTime = (_metrics.averageUpdateTime * 9 + updateTime) / 10;

    // Update max
    if (updateTime > _metrics.maxUpdateTime)
        _metrics.maxUpdateTime = updateTime;
}

} // namespace Playerbot