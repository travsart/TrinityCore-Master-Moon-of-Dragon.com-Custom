/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "DeathKnightAI.h"
#include "BloodSpecialization.h"
#include "FrostSpecialization.h"
#include "UnholySpecialization.h"
#include "Player.h"
#include "Group.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Item.h"
#include "Creature.h"
#include "ObjectAccessor.h"
#include "WorldSession.h"
#include "Map.h"
#include "../Combat/BotThreatManager.h"
#include "../Combat/TargetSelector.h"
#include "../Combat/PositionManager.h"
#include "../Combat/InterruptManager.h"
#include "../CooldownManager.h"
#include "Log.h"
#include <atomic>
#include <chrono>
#include <algorithm>
#include <unordered_map>

namespace Playerbot
{

// Death Knight rune types - using the ones from DeathKnightSpecialization.h
static constexpr uint8 RUNE_BLOOD = 0;
static constexpr uint8 RUNE_FROST = 1;
static constexpr uint8 RUNE_UNHOLY = 2;
static constexpr uint8 RUNE_DEATH = 3;

// Performance metrics structure
struct DeathKnightMetrics
{
    std::atomic<uint32> totalRunicPowerGenerated{0};
    std::atomic<uint32> totalRunicPowerSpent{0};
    std::atomic<uint32> totalRunesUsed{0};
    std::atomic<uint32> diseasesApplied{0};
    std::atomic<uint32> deathStrikesUsed{0};
    std::atomic<uint32> deathGripsUsed{0};
    std::atomic<uint32> interruptsExecuted{0};
    std::atomic<uint32> cooldownsUsed{0};
    std::atomic<float> averageReactionTime{0};
    std::atomic<float> runeEfficiency{0};
    std::atomic<float> diseaseUptime{0};
    std::chrono::steady_clock::time_point lastUpdate;

    void Reset()
    {
        totalRunicPowerGenerated = 0;
        totalRunicPowerSpent = 0;
        totalRunesUsed = 0;
        diseasesApplied = 0;
        deathStrikesUsed = 0;
        deathGripsUsed = 0;
        interruptsExecuted = 0;
        cooldownsUsed = 0;
        averageReactionTime = 0;
        runeEfficiency = 0;
        diseaseUptime = 0;
        lastUpdate = std::chrono::steady_clock::now();
    }

    void UpdateReactionTime(float deltaMs)
    {
        float current = averageReactionTime.load();
        averageReactionTime = (current * 0.9f) + (deltaMs * 0.1f);
    }

    void UpdateRuneEfficiency(uint32 runesUsed, uint32 powerGenerated)
    {
        if (runesUsed > 0)
        {
            float efficiency = static_cast<float>(powerGenerated) / runesUsed;
            float current = runeEfficiency.load();
            runeEfficiency = (current * 0.8f) + (efficiency * 0.2f);
        }
    }

    void UpdateDiseaseUptime(float currentUptime)
    {
        float current = diseaseUptime.load();
        diseaseUptime = (current * 0.95f) + (currentUptime * 0.05f);
    }
};

// Death Knight-specific spell IDs (3.3.5a)
enum DeathKnightSpells : uint32
{
    // Diseases
    FROST_FEVER             = 55095,
    BLOOD_PLAGUE            = 55078,

    // Blood Abilities
    BLOOD_STRIKE            = 49930,
    HEART_STRIKE            = 55050,
    BLOOD_BOIL              = 48721,
    RUNE_TAP                = 48982,
    VAMPIRIC_BLOOD          = 55233,
    DANCING_RUNE_WEAPON     = 49028,
    MARK_OF_BLOOD           = 49005,

    // Frost Abilities
    ICY_TOUCH               = 45477,
    OBLITERATE              = 49020,
    FROST_STRIKE            = 49143,
    HOWLING_BLAST           = 51411,
    CHAINS_OF_ICE           = 45524,
    UNBREAKABLE_ARMOR       = 51271,
    DEATHCHILL              = 49796,

    // Unholy Abilities
    PLAGUE_STRIKE           = 45462,
    SCOURGE_STRIKE          = 55090,
    DEATH_COIL              = 47541,
    DEATH_AND_DECAY         = 43265,
    CORPSE_EXPLOSION        = 51328,
    BONE_SHIELD             = 49222,
    SUMMON_GARGOYLE         = 49206,
    UNHOLY_FRENZY           = 49016,

    // Universal Abilities
    DEATH_STRIKE            = 49998,
    DEATH_GRIP              = 49576,
    ANTI_MAGIC_SHELL        = 48707,
    ANTI_MAGIC_ZONE         = 51052,
    ICEBOUND_FORTITUDE      = 48792,
    MIND_FREEZE             = 47528,
    STRANGULATE             = 47476,
    EMPOWER_RUNE_WEAPON     = 47568,
    ARMY_OF_THE_DEAD        = 42650,
    RAISE_DEAD              = 46584,
    HORN_OF_WINTER          = 57330,
    PATH_OF_FROST           = 3714,

    // Presences
    BLOOD_PRESENCE          = 48266,
    FROST_PRESENCE          = 48263,
    UNHOLY_PRESENCE         = 48265,

    // Runic Power Abilities
    RUNE_STRIKE             = 56815,
    DEATH_PACT              = 48743
};

// Rune types defined above

// Rune management system
class RuneManager
{
public:
    explicit RuneManager(Player* bot) : _bot(bot)
    {
        ResetRunes();
    }

    void ResetRunes()
    {
        // Initialize with 2 of each rune type
        for (int i = 0; i < 6; ++i)
        {
            _runes[i].type = static_cast<RuneType>(i / 2);
            _runes[i].available = true;
            _runes[i].cooldownEnd = 0;
        }
    }

    bool HasRunes(uint8 blood, uint8 frost, uint8 unholy)
    {
        uint8 availableBlood = 0, availableFrost = 0, availableUnholy = 0, availableDeath = 0;

        for (const auto& rune : _runes)
        {
            if (!rune.available)
                continue;

            switch (static_cast<uint8>(rune.type))
            {
                case RUNE_BLOOD:
                    availableBlood++;
                    break;
                case RUNE_FROST:
                    availableFrost++;
                    break;
                case RUNE_UNHOLY:
                    availableUnholy++;
                    break;
                case RUNE_DEATH:
                    availableDeath++;
                    break;
            }
        }

        // Death runes can be used for any type
        uint8 bloodNeeded = (blood > availableBlood) ? blood - availableBlood : 0;
        uint8 frostNeeded = (frost > availableFrost) ? frost - availableFrost : 0;
        uint8 unholyNeeded = (unholy > availableUnholy) ? unholy - availableUnholy : 0;

        return (bloodNeeded + frostNeeded + unholyNeeded) <= availableDeath;
    }

    void ConsumeRunes(uint8 blood, uint8 frost, uint8 unholy)
    {
        // Consume specific runes first, then death runes
        for (auto& rune : _runes)
        {
            if (!rune.available)
                continue;

            if (blood > 0 && rune.type == RuneType::BLOOD)
            {
                rune.available = false;
                rune.cooldownEnd = getMSTime() + 10000; // 10 second cooldown
                blood--;
            }
            else if (frost > 0 && rune.type == RuneType::FROST)
            {
                rune.available = false;
                rune.cooldownEnd = getMSTime() + 10000;
                frost--;
            }
            else if (unholy > 0 && rune.type == RuneType::UNHOLY)
            {
                rune.available = false;
                rune.cooldownEnd = getMSTime() + 10000;
                unholy--;
            }
        }

        // Use death runes for remaining
        for (auto& rune : _runes)
        {
            if (!rune.available || rune.type != RuneType::DEATH)
                continue;

            if (blood > 0 || frost > 0 || unholy > 0)
            {
                rune.available = false;
                rune.cooldownEnd = getMSTime() + 10000;
                if (blood > 0) blood--;
                else if (frost > 0) frost--;
                else if (unholy > 0) unholy--;
            }
        }
    }

    void UpdateRunes(uint32 diff)
    {
        uint32 currentTime = getMSTime();
        for (auto& rune : _runes)
        {
            if (!rune.available && currentTime >= rune.cooldownEnd)
            {
                rune.available = true;
            }
        }
    }

    uint8 GetAvailableRunes(RuneType type) const
    {
        uint8 count = 0;
        for (const auto& rune : _runes)
        {
            if (rune.available && (rune.type == type || rune.type == RuneType::DEATH))
                count++;
        }
        return count;
    }

private:
    struct Rune
    {
        RuneType type;
        bool available;
        uint32 cooldownEnd;
    };

    Player* _bot;
    Rune _runes[6];
};

// Disease tracking system
class DiseaseManager
{
public:
    explicit DiseaseManager(Player* bot) : _bot(bot) {}

    void UpdateDiseases(Unit* target)
    {
        if (!target)
            return;

        uint32 currentTime = getMSTime();

        // Check Frost Fever
        if (HasDisease(target, FROST_FEVER))
        {
            _frostFeverTargets[target->GetGUID()] = currentTime + 15000; // 15 second duration
        }
        else
        {
            _frostFeverTargets.erase(target->GetGUID());
        }

        // Check Blood Plague
        if (HasDisease(target, BLOOD_PLAGUE))
        {
            _bloodPlagueTargets[target->GetGUID()] = currentTime + 15000;
        }
        else
        {
            _bloodPlagueTargets.erase(target->GetGUID());
        }

        CleanupExpiredDiseases(currentTime);
    }

    bool HasBothDiseases(Unit* target) const
    {
        if (!target)
            return false;

        ObjectGuid guid = target->GetGUID();
        return _frostFeverTargets.find(guid) != _frostFeverTargets.end() &&
               _bloodPlagueTargets.find(guid) != _bloodPlagueTargets.end();
    }

    bool NeedsFrostFever(Unit* target) const
    {
        if (!target)
            return false;

        auto it = _frostFeverTargets.find(target->GetGUID());
        if (it == _frostFeverTargets.end())
            return true;

        // Refresh if less than 5 seconds remaining
        return (it->second - getMSTime()) < 5000;
    }

    bool NeedsBloodPlague(Unit* target) const
    {
        if (!target)
            return false;

        auto it = _bloodPlagueTargets.find(target->GetGUID());
        if (it == _bloodPlagueTargets.end())
            return true;

        // Refresh if less than 5 seconds remaining
        return (it->second - getMSTime()) < 5000;
    }

    float GetDiseaseUptime() const
    {
        if (_totalTargetTime == 0)
            return 0.0f;

        return static_cast<float>(_diseasedTime) / _totalTargetTime * 100.0f;
    }

private:
    bool HasDisease(Unit* target, uint32 spellId) const
    {
        // Check if target has the aura
        return target->HasAura(spellId);
    }

    void CleanupExpiredDiseases(uint32 currentTime)
    {
        // Remove expired entries
        for (auto it = _frostFeverTargets.begin(); it != _frostFeverTargets.end();)
        {
            if (it->second < currentTime)
                it = _frostFeverTargets.erase(it);
            else
                ++it;
        }

        for (auto it = _bloodPlagueTargets.begin(); it != _bloodPlagueTargets.end();)
        {
            if (it->second < currentTime)
                it = _bloodPlagueTargets.erase(it);
            else
                ++it;
        }
    }

    Player* _bot;
    std::unordered_map<ObjectGuid, uint32> _frostFeverTargets;
    std::unordered_map<ObjectGuid, uint32> _bloodPlagueTargets;
    uint32 _totalTargetTime = 0;
    uint32 _diseasedTime = 0;
};

// Combat metrics tracking
class DeathKnightCombatMetrics
{
public:
    void RecordAbilityUsage(uint32 spellId, bool success, uint32 runesUsed = 0, uint32 powerCost = 0)
    {
        auto now = std::chrono::steady_clock::now();
        _abilityTimings[spellId] = now;

        if (success)
        {
            _successfulCasts[spellId]++;
            _totalRunesUsed += runesUsed;
            _totalRunicPowerUsed += powerCost;
        }
        else
            _failedCasts[spellId]++;

        _lastGCD = now;
    }

    void RecordRunicPowerGeneration(uint32 power)
    {
        _totalRunicPowerGenerated += power;
    }

    float GetAbilitySuccessRate(uint32 spellId) const
    {
        auto successful = _successfulCasts.find(spellId);
        auto failed = _failedCasts.find(spellId);

        uint32 total = 0;
        uint32 success = 0;

        if (successful != _successfulCasts.end())
        {
            success = successful->second;
            total += success;
        }

        if (failed != _failedCasts.end())
            total += failed->second;

        return total > 0 ? (float)success / total : 0.0f;
    }

    bool IsOnGlobalCooldown() const
    {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastGCD);
        return elapsed.count() < 1500; // 1.5 second GCD
    }

private:
    std::unordered_map<uint32, std::chrono::steady_clock::time_point> _abilityTimings;
    std::unordered_map<uint32, uint32> _successfulCasts;
    std::unordered_map<uint32, uint32> _failedCasts;
    std::chrono::steady_clock::time_point _lastGCD;
    uint32 _totalRunesUsed = 0;
    uint32 _totalRunicPowerUsed = 0;
    uint32 _totalRunicPowerGenerated = 0;
};

// Combat positioning strategy
class DeathKnightCombatPositioning
{
public:
    explicit DeathKnightCombatPositioning(Player* bot) : _bot(bot) {}

    Position CalculateOptimalPosition(Unit* target, DeathKnightSpec spec)
    {
        if (!target || !_bot)
            return _bot->GetPosition();

        Position optimalPos = _bot->GetPosition();
        float currentDistance = _bot->GetDistance(target);

        switch (spec)
        {
            case DeathKnightSpec::BLOOD:
                // Tank positioning - in front of target
                if (currentDistance > 5.0f)
                {
                    float angle = target->GetOrientation();
                    optimalPos.m_positionX = target->GetPositionX() + cos(angle) * 3.0f;
                    optimalPos.m_positionY = target->GetPositionY() + sin(angle) * 3.0f;
                    optimalPos.m_positionZ = target->GetPositionZ();
                }
                break;

            case DeathKnightSpec::FROST:
                // Melee DPS positioning - behind or to the side
                if (currentDistance > 5.0f)
                {
                    float angle = target->GetOrientation() + M_PI; // Behind target
                    optimalPos.m_positionX = target->GetPositionX() + cos(angle) * 3.0f;
                    optimalPos.m_positionY = target->GetPositionY() + sin(angle) * 3.0f;
                    optimalPos.m_positionZ = target->GetPositionZ();
                }
                break;

            case DeathKnightSpec::UNHOLY:
                // Flexible positioning with pet management
                if (currentDistance > 5.0f && currentDistance < 30.0f)
                {
                    // Stay at mid-range for Death Coil
                    float angle = target->GetRelativeAngle(_bot);
                    optimalPos.m_positionX = target->GetPositionX() + cos(angle) * 10.0f;
                    optimalPos.m_positionY = target->GetPositionY() + sin(angle) * 10.0f;
                    optimalPos.m_positionZ = target->GetPositionZ();
                }
                else if (currentDistance > 30.0f)
                {
                    // Use Death Grip range
                    float angle = target->GetRelativeAngle(_bot);
                    optimalPos.m_positionX = target->GetPositionX() + cos(angle) * 25.0f;
                    optimalPos.m_positionY = target->GetPositionY() + sin(angle) * 25.0f;
                    optimalPos.m_positionZ = target->GetPositionZ();
                }
                break;
        }

        return optimalPos;
    }

    float GetOptimalRange(DeathKnightSpec spec) const
    {
        switch (spec)
        {
            case DeathKnightSpec::BLOOD:
                return 5.0f; // Tank in melee
            case DeathKnightSpec::FROST:
                return 5.0f; // Melee DPS
            case DeathKnightSpec::UNHOLY:
                return 10.0f; // Flexible range
            default:
                return 5.0f;
        }
    }

private:
    Player* _bot;
};

DeathKnightAI::DeathKnightAI(Player* bot) :
    ClassAI(bot),
    _detectedSpec(DeathKnightSpec::BLOOD),
    _specialization(nullptr),
    _runicPowerSpent(0),
    _runesUsed(0),
    _diseasesApplied(0),
    _lastPresence(0),
    _lastHorn(0)
{
    // Initialize combat systems
    InitializeCombatSystems();

    // Detect and initialize specialization
    DetectSpecialization();
    InitializeSpecialization();

    // Initialize performance tracking
    _metrics = std::make_unique<DeathKnightMetrics>();
    _combatMetrics = std::make_unique<DeathKnightCombatMetrics>();
    _runeManager = std::make_unique<RuneManager>(bot);
    _diseaseManager = std::make_unique<DiseaseManager>(bot);
    _positioning = std::make_unique<DeathKnightCombatPositioning>(bot);

    TC_LOG_DEBUG("playerbot", "DeathKnightAI initialized for {} with specialization {}",
                 bot->GetName(), static_cast<uint32>(_detectedSpec));
}

void DeathKnightAI::InitializeCombatSystems()
{
    // Initialize advanced combat system components
    _threatManager = std::make_unique<BotThreatManager>(GetBot());
    _targetSelector = std::make_unique<TargetSelector>(GetBot(), _threatManager.get());
    _positionManager = std::make_unique<PositionManager>(GetBot(), _threatManager.get());
    _interruptManager = std::make_unique<InterruptManager>(GetBot());
    _cooldownManager = std::make_unique<CooldownManager>();

    TC_LOG_DEBUG("playerbot", "DeathKnightAI combat systems initialized for {}", GetBot()->GetName());
}

void DeathKnightAI::DetectSpecialization()
{
    if (!GetBot())
    {
        _detectedSpec = DeathKnightSpec::BLOOD;
        return;
    }

    // Advanced specialization detection based on talents and abilities
    uint32 bloodPoints = 0;
    uint32 frostPoints = 0;
    uint32 unholyPoints = 0;

    // Check key Blood abilities/talents
    if (GetBot()->HasSpell(HEART_STRIKE))
        bloodPoints += 10;
    if (GetBot()->HasSpell(VAMPIRIC_BLOOD))
        bloodPoints += 8;
    if (GetBot()->HasSpell(DANCING_RUNE_WEAPON))
        bloodPoints += 10;
    if (GetBot()->HasSpell(RUNE_TAP))
        bloodPoints += 6;

    // Check key Frost abilities/talents
    if (GetBot()->HasSpell(FROST_STRIKE))
        frostPoints += 10;
    if (GetBot()->HasSpell(HOWLING_BLAST))
        frostPoints += 8;
    if (GetBot()->HasSpell(UNBREAKABLE_ARMOR))
        frostPoints += 10;
    if (GetBot()->HasSpell(DEATHCHILL))
        frostPoints += 6;

    // Check key Unholy abilities/talents
    if (GetBot()->HasSpell(SCOURGE_STRIKE))
        unholyPoints += 10;
    if (GetBot()->HasSpell(SUMMON_GARGOYLE))
        unholyPoints += 10;
    if (GetBot()->HasSpell(BONE_SHIELD))
        unholyPoints += 8;
    if (GetBot()->HasSpell(UNHOLY_FRENZY))
        unholyPoints += 6;

    // Determine specialization based on points
    if (bloodPoints >= frostPoints && bloodPoints >= unholyPoints)
        _detectedSpec = DeathKnightSpec::BLOOD;
    else if (frostPoints > bloodPoints && frostPoints >= unholyPoints)
        _detectedSpec = DeathKnightSpec::FROST;
    else
        _detectedSpec = DeathKnightSpec::UNHOLY;

    TC_LOG_DEBUG("playerbot", "DeathKnightAI detected specialization: {} (B:{}, F:{}, U:{})",
                 static_cast<uint32>(_detectedSpec), bloodPoints, frostPoints, unholyPoints);
}

void DeathKnightAI::InitializeSpecialization()
{
    // Create specialization instance based on detected spec
    switch (_detectedSpec)
    {
        case DeathKnightSpec::BLOOD:
            _specialization = std::make_unique<BloodSpecialization>(GetBot());
            TC_LOG_DEBUG("playerbot", "DeathKnightAI: Initialized Blood specialization");
            break;

        case DeathKnightSpec::FROST:
            _specialization = std::make_unique<FrostSpecialization>(GetBot());
            TC_LOG_DEBUG("playerbot", "DeathKnightAI: Initialized Frost specialization");
            break;

        case DeathKnightSpec::UNHOLY:
            _specialization = std::make_unique<UnholySpecialization>(GetBot());
            TC_LOG_DEBUG("playerbot", "DeathKnightAI: Initialized Unholy specialization");
            break;
    }

    // Specialization is initialized in its constructor
}

void DeathKnightAI::UpdateRotation(Unit* target)
{
    if (!target || !GetBot())
        return;

    auto startTime = std::chrono::steady_clock::now();

    // Update disease tracking
    _diseaseManager->UpdateDiseases(target);

    // Check if we're on global cooldown
    if (_combatMetrics->IsOnGlobalCooldown())
        return;

    // Delegate to specialization if available
    if (_specialization)
    {
        _specialization->UpdateRotation(target);
    }
    else
    {
        ExecuteFallbackRotation(target);
    }

    // Update performance metrics
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    _metrics->UpdateReactionTime(duration.count() / 1000.0f);
    _metrics->UpdateDiseaseUptime(_diseaseManager->GetDiseaseUptime());
}

void DeathKnightAI::ExecuteFallbackRotation(Unit* target)
{
    if (!target || !GetBot())
        return;

    float distance = GetBot()->GetDistance(target);
    uint32 runicPower = GetBot()->GetPower(POWER_RUNIC_POWER);

    // Apply diseases first
    if (_diseaseManager->NeedsFrostFever(target))
    {
        if (CanUseAbility(ICY_TOUCH) && _runeManager->HasRunes(0, 1, 0))
        {
            CastSpell(target, ICY_TOUCH);
            _runeManager->ConsumeRunes(0, 1, 0);
            _metrics->diseasesApplied++;
            _diseasesApplied++;
            return;
        }
    }

    if (_diseaseManager->NeedsBloodPlague(target))
    {
        if (CanUseAbility(PLAGUE_STRIKE) && _runeManager->HasRunes(0, 0, 1))
        {
            CastSpell(target, PLAGUE_STRIKE);
            _runeManager->ConsumeRunes(0, 0, 1);
            _metrics->diseasesApplied++;
            _diseasesApplied++;
            return;
        }
    }

    // Use Death Strike for healing if needed
    if (GetBot()->GetHealthPct() < 70.0f)
    {
        if (CanUseAbility(DEATH_STRIKE) && _runeManager->HasRunes(0, 1, 1))
        {
            CastSpell(target, DEATH_STRIKE);
            _runeManager->ConsumeRunes(0, 1, 1);
            _metrics->deathStrikesUsed++;
            return;
        }
    }

    // Main rotation based on range
    if (distance <= 5.0f)
    {
        // Melee range abilities
        if (_diseaseManager->HasBothDiseases(target))
        {
            // Spread diseases with Blood Boil for AoE
            if (CanUseAbility(BLOOD_BOIL) && _runeManager->HasRunes(1, 0, 0))
            {
                CastSpell(BLOOD_BOIL);
                _runeManager->ConsumeRunes(1, 0, 0);
                return;
            }

            // Use spec-specific strike
            switch (_detectedSpec)
            {
                case DeathKnightSpec::BLOOD:
                    if (CanUseAbility(HEART_STRIKE) && _runeManager->HasRunes(1, 0, 0))
                    {
                        CastSpell(target, HEART_STRIKE);
                        _runeManager->ConsumeRunes(1, 0, 0);
                        _combatMetrics->RecordRunicPowerGeneration(15);
                        return;
                    }
                    break;

                case DeathKnightSpec::FROST:
                    if (CanUseAbility(OBLITERATE) && _runeManager->HasRunes(0, 1, 1))
                    {
                        CastSpell(target, OBLITERATE);
                        _runeManager->ConsumeRunes(0, 1, 1);
                        _combatMetrics->RecordRunicPowerGeneration(25);
                        return;
                    }
                    break;

                case DeathKnightSpec::UNHOLY:
                    if (CanUseAbility(SCOURGE_STRIKE) && _runeManager->HasRunes(0, 0, 1))
                    {
                        CastSpell(target, SCOURGE_STRIKE);
                        _runeManager->ConsumeRunes(0, 0, 1);
                        _combatMetrics->RecordRunicPowerGeneration(15);
                        return;
                    }
                    break;
            }
        }

        // Use Blood Strike as fallback
        if (CanUseAbility(BLOOD_STRIKE) && _runeManager->HasRunes(1, 0, 0))
        {
            CastSpell(target, BLOOD_STRIKE);
            _runeManager->ConsumeRunes(1, 0, 0);
            return;
        }
    }
    else if (distance > 5.0f && distance <= 30.0f)
    {
        // Death Grip to pull target
        if (distance > 10.0f && CanUseAbility(DEATH_GRIP))
        {
            CastSpell(target, DEATH_GRIP);
            _metrics->deathGripsUsed++;
            return;
        }

        // Death Coil for ranged damage
        if (runicPower >= 40 && CanUseAbility(DEATH_COIL))
        {
            CastSpell(target, DEATH_COIL);
            _metrics->totalRunicPowerSpent += 40;
            _runicPowerSpent += 40;
            return;
        }
    }

    // Runic Power dump abilities
    if (runicPower >= 60)
    {
        switch (_detectedSpec)
        {
            case DeathKnightSpec::FROST:
                if (CanUseAbility(FROST_STRIKE))
                {
                    CastSpell(target, FROST_STRIKE);
                    _metrics->totalRunicPowerSpent += 40;
                    _runicPowerSpent += 40;
                    return;
                }
                break;

            case DeathKnightSpec::BLOOD:
                if (CanUseAbility(RUNE_STRIKE))
                {
                    CastSpell(target, RUNE_STRIKE);
                    _metrics->totalRunicPowerSpent += 20;
                    _runicPowerSpent += 20;
                    return;
                }
                break;

            case DeathKnightSpec::UNHOLY:
                if (CanUseAbility(DEATH_COIL))
                {
                    CastSpell(target, DEATH_COIL);
                    _metrics->totalRunicPowerSpent += 40;
                    _runicPowerSpent += 40;
                    return;
                }
                break;
        }
    }

    // Handle interrupts
    if (target->HasUnitState(UNIT_STATE_CASTING))
    {
        if (_interruptManager->IsSpellInterruptWorthy(target->GetCurrentSpell(CURRENT_GENERIC_SPELL) ? target->GetCurrentSpell(CURRENT_GENERIC_SPELL)->GetSpellInfo()->Id : 0, target))
        {
            if (distance <= 5.0f && CanUseAbility(MIND_FREEZE))
            {
                CastSpell(target, MIND_FREEZE);
                _metrics->interruptsExecuted++;
                return;
            }
            else if (distance <= 30.0f && CanUseAbility(STRANGULATE))
            {
                CastSpell(target, STRANGULATE);
                _metrics->interruptsExecuted++;
                return;
            }
        }
    }
}

void DeathKnightAI::UpdateBuffs()
{
    if (!GetBot())
        return;

    uint32 currentTime = getMSTime();

    // Maintain presence
    if (currentTime - _lastPresence > 5000) // Check every 5 seconds
    {
        uint32 presenceSpell = 0;
        switch (_detectedSpec)
        {
            case DeathKnightSpec::BLOOD:
                presenceSpell = BLOOD_PRESENCE;
                break;
            case DeathKnightSpec::FROST:
                presenceSpell = FROST_PRESENCE;
                break;
            case DeathKnightSpec::UNHOLY:
                presenceSpell = UNHOLY_PRESENCE;
                break;
        }

        if (presenceSpell && !HasAura(presenceSpell) && CanUseAbility(presenceSpell))
        {
            CastSpell(presenceSpell);
            _lastPresence = currentTime;
        }
    }

    // Maintain Horn of Winter
    if (currentTime - _lastHorn > 30000 && !HasAura(HORN_OF_WINTER))
    {
        if (CanUseAbility(HORN_OF_WINTER))
        {
            CastSpell(HORN_OF_WINTER);
            _lastHorn = currentTime;
        }
    }

    // Maintain Bone Shield for Blood/Unholy
    if ((_detectedSpec == DeathKnightSpec::BLOOD || _detectedSpec == DeathKnightSpec::UNHOLY) &&
        !HasAura(BONE_SHIELD) && CanUseAbility(BONE_SHIELD))
    {
        CastSpell(BONE_SHIELD);
    }

    // Delegate to specialization for spec-specific buffs
    if (_specialization)
    {
        _specialization->UpdateBuffs();
    }
}

void DeathKnightAI::UpdateCooldowns(uint32 diff)
{
    if (!GetBot())
        return;

    // Update rune manager
    _runeManager->UpdateRunes(diff);

    // Update cooldown manager
    if (_cooldownManager)
        _cooldownManager->Update(diff);

    // Delegate to specialization
    if (_specialization)
    {
        _specialization->UpdateCooldowns(diff);
    }
}

bool DeathKnightAI::CanUseAbility(uint32 spellId)
{
    if (!GetBot())
        return false;

    // Check basic requirements
    if (!GetBot()->HasSpell(spellId))
        return false;

    if (!IsSpellReady(spellId))
        return false;

    if (!HasEnoughResource(spellId))
        return false;

    // Additional checks via specialization
    if (_specialization)
    {
        return _specialization->CanUseAbility(spellId);
    }

    return true;
}

bool DeathKnightAI::HasEnoughResource(uint32 spellId)
{
    if (!GetBot())
        return false;

    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId, GetBot()->GetMap()->GetDifficultyID());
    if (!spellInfo)
        return false;

    // Check runic power cost
    auto powerCosts = spellInfo->CalcPowerCost(GetBot(), spellInfo->GetSchoolMask());
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_RUNIC_POWER)
        {
            uint32 currentPower = GetBot()->GetPower(POWER_RUNIC_POWER);
            if (currentPower < cost.Amount)
                return false;
        }
    }

    // Check rune requirements
    switch (spellId)
    {
        case ICY_TOUCH:
            return _runeManager->HasRunes(0, 1, 0);
        case PLAGUE_STRIKE:
            return _runeManager->HasRunes(0, 0, 1);
        case BLOOD_STRIKE:
        case HEART_STRIKE:
        case BLOOD_BOIL:
            return _runeManager->HasRunes(1, 0, 0);
        case DEATH_STRIKE:
        case OBLITERATE:
            return _runeManager->HasRunes(0, 1, 1);
        case SCOURGE_STRIKE:
            return _runeManager->HasRunes(0, 0, 1);
        default:
            break;
    }

    // Additional checks via specialization
    if (_specialization)
    {
        return _specialization->HasEnoughResource(spellId);
    }

    return true;
}

void DeathKnightAI::ConsumeResource(uint32 spellId)
{
    if (!GetBot())
        return;

    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId, GetBot()->GetMap()->GetDifficultyID());
    if (!spellInfo)
        return;

    // Track runic power consumption
    auto powerCosts = spellInfo->CalcPowerCost(GetBot(), spellInfo->GetSchoolMask());
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_RUNIC_POWER)
        {
            _metrics->totalRunicPowerSpent += cost.Amount;
            _runicPowerSpent += cost.Amount;
        }
    }

    // Track rune usage
    uint8 runesUsed = 0;
    switch (spellId)
    {
        case ICY_TOUCH:
        case PLAGUE_STRIKE:
        case BLOOD_STRIKE:
        case HEART_STRIKE:
        case BLOOD_BOIL:
        case SCOURGE_STRIKE:
            runesUsed = 1;
            break;
        case DEATH_STRIKE:
        case OBLITERATE:
            runesUsed = 2;
            break;
        default:
            break;
    }

    if (runesUsed > 0)
    {
        _metrics->totalRunesUsed += runesUsed;
        _runesUsed += runesUsed;
    }

    // Delegate to specialization
    if (_specialization)
    {
        _specialization->ConsumeResource(spellId);
    }
}

void DeathKnightAI::OnCombatStart(Unit* target)
{
    if (!target || !GetBot())
        return;

    ClassAI::OnCombatStart(target);

    // Reset combat metrics
    _runicPowerSpent = 0;
    _runesUsed = 0;
    _diseasesApplied = 0;

    // Apply diseases immediately
    if (CanUseAbility(ICY_TOUCH))
    {
        CastSpell(target, ICY_TOUCH);
        _diseaseManager->UpdateDiseases(target);
    }

    // Use offensive cooldowns for boss fights
    if (target->GetTypeId() == TYPEID_UNIT && target->ToCreature()->isWorldBoss())
    {
        ActivateBurstCooldowns(target);

        // Army of the Dead for major encounters
        if (CanUseAbility(ARMY_OF_THE_DEAD))
        {
            CastSpell(ARMY_OF_THE_DEAD);
            TC_LOG_DEBUG("playerbot", "DeathKnightAI: Summoned Army of the Dead for boss");
        }
    }

    // Use defensive cooldowns if taking magic damage
    if (target->GetTypeId() == TYPEID_UNIT)
    {
        Creature* creature = target->ToCreature();
        if (creature && creature->GetCreatureTemplate()->unit_class == UNIT_CLASS_MAGE)
        {
            if (CanUseAbility(ANTI_MAGIC_SHELL))
            {
                CastSpell(ANTI_MAGIC_SHELL);
                TC_LOG_DEBUG("playerbot", "DeathKnightAI: Activated Anti-Magic Shell");
            }
        }
    }

    // Delegate to specialization
    if (_specialization)
    {
        _specialization->OnCombatStart(target);
    }

    TC_LOG_DEBUG("playerbot", "DeathKnightAI: Combat started against {} with spec {}",
                 target->GetName(), static_cast<uint32>(_detectedSpec));
}

void DeathKnightAI::OnCombatEnd()
{
    ClassAI::OnCombatEnd();

    // Calculate combat metrics
    if (_runesUsed > 0)
    {
        _metrics->UpdateRuneEfficiency(_runesUsed, _metrics->totalRunicPowerGenerated);
    }

    // Reset rune system
    _runeManager->ResetRunes();

    // Delegate to specialization
    if (_specialization)
    {
        _specialization->OnCombatEnd();
    }

    TC_LOG_DEBUG("playerbot", "DeathKnightAI: Combat ended. RP spent: {}, Runes used: {}, Diseases: {}",
                 _runicPowerSpent, _runesUsed, _diseasesApplied);
}

void DeathKnightAI::ActivateBurstCooldowns(Unit* target)
{
    if (!target)
        return;

    switch (_detectedSpec)
    {
        case DeathKnightSpec::BLOOD:
            if (CanUseAbility(VAMPIRIC_BLOOD))
            {
                CastSpell(VAMPIRIC_BLOOD);
                _metrics->cooldownsUsed++;
            }
            if (CanUseAbility(DANCING_RUNE_WEAPON))
            {
                CastSpell(DANCING_RUNE_WEAPON);
                _metrics->cooldownsUsed++;
            }
            break;

        case DeathKnightSpec::FROST:
            if (CanUseAbility(UNBREAKABLE_ARMOR))
            {
                CastSpell(UNBREAKABLE_ARMOR);
                _metrics->cooldownsUsed++;
            }
            if (CanUseAbility(DEATHCHILL))
            {
                CastSpell(DEATHCHILL);
                _metrics->cooldownsUsed++;
            }
            if (CanUseAbility(EMPOWER_RUNE_WEAPON))
            {
                CastSpell(EMPOWER_RUNE_WEAPON);
                _metrics->cooldownsUsed++;
            }
            break;

        case DeathKnightSpec::UNHOLY:
            if (CanUseAbility(SUMMON_GARGOYLE))
            {
                CastSpell(SUMMON_GARGOYLE);
                _metrics->cooldownsUsed++;
            }
            if (CanUseAbility(UNHOLY_FRENZY))
            {
                CastSpell(target, UNHOLY_FRENZY);
                _metrics->cooldownsUsed++;
            }
            break;
    }
}

Position DeathKnightAI::GetOptimalPosition(Unit* target)
{
    if (!target || !GetBot())
        return GetBot()->GetPosition();

    // Use positioning manager for advanced calculations
    if (_positioning)
    {
        return _positioning->CalculateOptimalPosition(target, _detectedSpec);
    }

    // Fallback to specialization
    if (_specialization)
    {
        return _specialization->GetOptimalPosition(target);
    }

    return ClassAI::GetOptimalPosition(target);
}

float DeathKnightAI::GetOptimalRange(Unit* target)
{
    if (!target)
        return 5.0f;

    // Use positioning manager
    if (_positioning)
    {
        return _positioning->GetOptimalRange(_detectedSpec);
    }

    // Delegate to specialization
    if (_specialization)
    {
        return _specialization->GetOptimalRange(target);
    }

    return 5.0f; // Default melee range
}

DeathKnightSpec DeathKnightAI::GetCurrentSpecialization() const
{
    return _detectedSpec;
}

DeathKnightAI::~DeathKnightAI()
{
    // Cleanup is handled by smart pointers
}

} // namespace Playerbot