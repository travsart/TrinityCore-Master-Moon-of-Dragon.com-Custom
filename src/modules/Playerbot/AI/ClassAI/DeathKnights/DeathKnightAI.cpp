/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "RuneManager.h"
#include "DeathKnightAI.h"
#include "BloodSpecialization.h"
#include "FrostSpecialization.h"
#include "UnholySpecialization.h"
#include "../../Combat/CombatBehaviorIntegration.h"
#include "Player.h"
#include "Group.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Item.h"
#include "Creature.h"
#include "ObjectAccessor.h"
#include "WorldSession.h"
#include "Map.h"
#include "SpellAuras.h"
#include "CellImpl.h"
#include "GridNotifiersImpl.h"
#include "../Combat/BotThreatManager.h"
#include "../Combat/TargetSelector.h"
#include "../Combat/PositionManager.h"
#include "../Combat/InterruptManager.h"
#include "../BaselineRotationManager.h"
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
    EPIDEMIC                = 207317, // AoE disease spread

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
    PILLAR_OF_FROST         = 51271, // Major frost DPS cooldown

    // Unholy Abilities
    PLAGUE_STRIKE           = 45462,
    SCOURGE_STRIKE          = 55090,
    DEATH_COIL              = 47541,
    DEATH_AND_DECAY         = 43265,
    CORPSE_EXPLOSION        = 51328,
    BONE_SHIELD             = 195181, // Updated for WoW 11.2
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
    DARK_COMMAND            = 56222, // Taunt

    // Presences
    BLOOD_PRESENCE          = 48266,
    FROST_PRESENCE          = 48263,
    UNHOLY_PRESENCE         = 48265,

    // Runic Power Abilities
    RUNE_STRIKE             = 56815,
    DEATH_PACT              = 48743
};

// Death Knight constants
constexpr float OPTIMAL_MELEE_RANGE = 5.0f;
constexpr float DEATH_GRIP_MIN_RANGE = 10.0f;
constexpr float DEATH_GRIP_MAX_RANGE = 30.0f;
constexpr float DEFENSIVE_COOLDOWN_THRESHOLD = 40.0f;
constexpr float HEALTH_EMERGENCY_THRESHOLD = 20.0f;
constexpr uint32 RUNIC_POWER_DUMP_THRESHOLD = 80;
constexpr uint32 PRESENCE_CHECK_INTERVAL = 5000;
constexpr uint32 HORN_CHECK_INTERVAL = 30000;
constexpr uint32 DEATH_GRIP_COOLDOWN = 25000;
constexpr uint32 DARK_COMMAND_COOLDOWN = 8000;

// Rune types defined above

// Rune management system removed - using external RuneManager class


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
    _lastHorn(0),
    _lastDeathGrip(0),
    _lastDarkCommand(0),
    _successfulInterrupts(0)
{
    // Initialize combat systems
    InitializeCombatSystems();

    // Detect and initialize specialization
    DetectSpecialization();
    InitializeSpecialization();

    // Initialize performance tracking
    _metrics = new DeathKnightMetrics();
    _combatMetrics = new DeathKnightCombatMetrics();
    _runeManager = new RuneManager(bot);
    _diseaseManager = std::make_unique<DiseaseManager>(bot);
    _positioning = new DeathKnightCombatPositioning(bot);

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

    // Check if bot should use baseline rotation (levels 1-9 or no spec)
    if (BaselineRotationManager::ShouldUseBaselineRotation(GetBot()))
    {
        static BaselineRotationManager baselineManager;
        baselineManager.HandleAutoSpecialization(GetBot());

        if (baselineManager.ExecuteBaselineRotation(GetBot(), target))
            return;

        // Fallback: Use Death Grip if available for ranged pull
        if (GetBot()->GetDistance(target) > OPTIMAL_MELEE_RANGE && ShouldUseDeathGrip(target))
        {
            if (CanUseAbility(DEATH_GRIP))
            {
                CastSpell(target, DEATH_GRIP);
                return;
            }
        }
        return;
    }

    // ========================================================================
    // COMBAT BEHAVIOR INTEGRATION - Priority-based decision making
    // ========================================================================
    auto* behaviors = GetCombatBehaviors();

    // Priority 1: Handle interrupts (Mind Freeze/Strangulate)
    if (behaviors && behaviors->ShouldInterrupt(target))
    {
        if (HandleInterrupts(target))
            return;
    }

    // Priority 2: Handle defensives (Icebound Fortitude, Anti-Magic Shell, Vampiric Blood)
    if (behaviors && behaviors->NeedsDefensive())
    {
        if (HandleDefensives())
            return;
    }

    // Priority 3: Check for target switching
    if (behaviors && behaviors->ShouldSwitchTarget())
    {
        if (HandleTargetSwitching(target))
            return;
    }

    // Priority 4: AoE vs Single-Target decision
    if (behaviors && behaviors->ShouldAOE())
    {
        if (HandleAoERotation(target))
            return;
    }

    // Priority 5: Use major cooldowns at optimal time
    if (behaviors && behaviors->ShouldUseCooldowns())
    {
        if (HandleOffensiveCooldowns(target))
            return;
    }

    // Priority 6: Rune and Runic Power Management
    if (HandleRuneAndPowerManagement(target))
        return;

    // Priority 7: Execute normal rotation through specialization or fallback
    ExecuteSpecializationRotation(target);

    // Update performance metrics
    auto startTime = std::chrono::steady_clock::now();
    _diseaseManager->UpdateDiseases(target);
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
        if (CanUseAbility(ICY_TOUCH) && _runeManager->HasRunes(0u, 1u, 0u))
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
        if (CanUseAbility(PLAGUE_STRIKE) && _runeManager->HasRunes(0u, 0u, 1u))
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
        if (CanUseAbility(DEATH_STRIKE) && _runeManager->HasRunes(0u, 1u, 1u))
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
            if (CanUseAbility(BLOOD_BOIL) && _runeManager->HasRunes(1u, 0u, 0u))
            {
                CastSpell(BLOOD_BOIL);
                _runeManager->ConsumeRunes(1, 0, 0);
                return;
            }

            // Use spec-specific strike
            switch (_detectedSpec)
            {
                case DeathKnightSpec::BLOOD:
                    if (CanUseAbility(HEART_STRIKE) && _runeManager->HasRunes(1u, 0u, 0u))
                    {
                        CastSpell(target, HEART_STRIKE);
                        _runeManager->ConsumeRunes(1, 0, 0);
                        _combatMetrics->RecordRunicPowerGeneration(15);
                        return;
                    }
                    break;

                case DeathKnightSpec::FROST:
                    if (CanUseAbility(OBLITERATE) && _runeManager->HasRunes(0u, 1u, 1u))
                    {
                        CastSpell(target, OBLITERATE);
                        _runeManager->ConsumeRunes(0, 1, 1);
                        _combatMetrics->RecordRunicPowerGeneration(25);
                        return;
                    }
                    break;

                case DeathKnightSpec::UNHOLY:
                    if (CanUseAbility(SCOURGE_STRIKE) && _runeManager->HasRunes(0u, 0u, 1u))
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
        if (CanUseAbility(BLOOD_STRIKE) && _runeManager->HasRunes(1u, 0u, 0u))
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
            return _runeManager->HasRunes(0u, 1u, 0u);
        case PLAGUE_STRIKE:
            return _runeManager->HasRunes(0u, 0u, 1u);
        case BLOOD_STRIKE:
        case HEART_STRIKE:
        case BLOOD_BOIL:
            return _runeManager->HasRunes(1u, 0u, 0u);
        case DEATH_STRIKE:
        case OBLITERATE:
            return _runeManager->HasRunes(0u, 1u, 1u);
        case SCOURGE_STRIKE:
            return _runeManager->HasRunes(0u, 0u, 1u);
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

// ========================================================================
// CombatBehaviorIntegration Helper Methods Implementation
// ========================================================================

bool DeathKnightAI::HandleInterrupts(Unit* target)
{
    auto* behaviors = GetCombatBehaviors();
    if (!behaviors)
        return false;

    Unit* interruptTarget = behaviors->GetInterruptTarget();
    if (!interruptTarget)
        interruptTarget = target;

    if (!interruptTarget || !interruptTarget->HasUnitState(UNIT_STATE_CASTING))
        return false;

    float distance = GetBot()->GetDistance(interruptTarget);

    // Priority: Mind Freeze for melee range, Strangulate for ranged
    if (distance <= OPTIMAL_MELEE_RANGE && CanUseAbility(MIND_FREEZE))
    {
        if (CastSpell(interruptTarget, MIND_FREEZE))
        {
            RecordInterruptAttempt(interruptTarget, MIND_FREEZE, true);
            TC_LOG_DEBUG("module.playerbot.ai", "Death Knight {} interrupted {} with Mind Freeze",
                         GetBot()->GetName(), interruptTarget->GetName());
            return true;
        }
    }
    else if (distance <= DEATH_GRIP_MAX_RANGE && CanUseAbility(STRANGULATE))
    {
        if (CastSpell(interruptTarget, STRANGULATE))
        {
            RecordInterruptAttempt(interruptTarget, STRANGULATE, true);
            TC_LOG_DEBUG("module.playerbot.ai", "Death Knight {} interrupted {} with Strangulate",
                         GetBot()->GetName(), interruptTarget->GetName());
            return true;
        }
    }

    return false;
}

bool DeathKnightAI::HandleDefensives()
{
    if (!GetBot())
        return false;

    float healthPct = GetBot()->GetHealthPct();
    bool actionTaken = false;

    // Icebound Fortitude at critical health
    if (healthPct < HEALTH_EMERGENCY_THRESHOLD && CanUseAbility(ICEBOUND_FORTITUDE))
    {
        if (CastSpell(ICEBOUND_FORTITUDE))
        {
            RecordAbilityUsage(ICEBOUND_FORTITUDE);
            TC_LOG_DEBUG("module.playerbot.ai", "Death Knight {} activated Icebound Fortitude",
                         GetBot()->GetName());
            actionTaken = true;
        }
    }

    // Anti-Magic Shell against casters
    Unit* currentTarget = GetBot()->GetSelectedUnit();
    if (currentTarget && currentTarget->HasUnitState(UNIT_STATE_CASTING))
    {
        if (CanUseAbility(ANTI_MAGIC_SHELL))
        {
            if (CastSpell(ANTI_MAGIC_SHELL))
            {
                RecordAbilityUsage(ANTI_MAGIC_SHELL);
                TC_LOG_DEBUG("module.playerbot.ai", "Death Knight {} activated Anti-Magic Shell",
                             GetBot()->GetName());
                actionTaken = true;
            }
        }
    }

    // Spec-specific defensives
    switch (_detectedSpec)
    {
        case DeathKnightSpec::BLOOD:
            // Vampiric Blood for blood DKs
            if (healthPct < DEFENSIVE_COOLDOWN_THRESHOLD && CanUseAbility(VAMPIRIC_BLOOD))
            {
                if (CastSpell(VAMPIRIC_BLOOD))
                {
                    RecordAbilityUsage(VAMPIRIC_BLOOD);
                    TC_LOG_DEBUG("module.playerbot.ai", "Blood Death Knight {} activated Vampiric Blood",
                                 GetBot()->GetName());
                    actionTaken = true;
                }
            }
            // Rune Tap for quick heal
            if (healthPct < 60.0f && CanUseAbility(RUNE_TAP))
            {
                if (CastSpell(RUNE_TAP))
                {
                    RecordAbilityUsage(RUNE_TAP);
                    actionTaken = true;
                }
            }
            break;

        case DeathKnightSpec::FROST:
            // Unbreakable Armor for frost DKs
            if (healthPct < 50.0f && CanUseAbility(UNBREAKABLE_ARMOR))
            {
                if (CastSpell(UNBREAKABLE_ARMOR))
                {
                    RecordAbilityUsage(UNBREAKABLE_ARMOR);
                    TC_LOG_DEBUG("module.playerbot.ai", "Frost Death Knight {} activated Unbreakable Armor",
                                 GetBot()->GetName());
                    actionTaken = true;
                }
            }
            break;

        case DeathKnightSpec::UNHOLY:
            // Bone Shield maintenance
            if (!HasAura(BONE_SHIELD) && CanUseAbility(BONE_SHIELD))
            {
                if (CastSpell(BONE_SHIELD))
                {
                    RecordAbilityUsage(BONE_SHIELD);
                    actionTaken = true;
                }
            }
            break;
    }

    // Death Strike for self-healing when low
    if (healthPct < 70.0f && _runeManager->HasRunes(0u, 1u, 1u) && CanUseAbility(DEATH_STRIKE))
    {
        Unit* target = GetBot()->GetSelectedUnit();
        if (target && IsInMeleeRange(target))
        {
            if (CastSpell(target, DEATH_STRIKE))
            {
                _runeManager->ConsumeRunes(0, 1, 1);
                RecordAbilityUsage(DEATH_STRIKE);
                _metrics->deathStrikesUsed++;
                actionTaken = true;
            }
        }
    }

    return actionTaken;
}

bool DeathKnightAI::HandleTargetSwitching(Unit*& target)
{
    auto* behaviors = GetCombatBehaviors();
    if (!behaviors)
        return false;

    Unit* priorityTarget = behaviors->GetPriorityTarget();
    if (!priorityTarget || priorityTarget == target)
        return false;

    OnTargetChanged(priorityTarget);
    target = priorityTarget;

    TC_LOG_DEBUG("module.playerbot.ai", "Death Knight {} switching target to {}",
                 GetBot()->GetName(), priorityTarget->GetName());

    // Use Death Grip on new target if needed
    float distance = GetBot()->GetDistance(priorityTarget);
    if (distance > DEATH_GRIP_MIN_RANGE && distance <= DEATH_GRIP_MAX_RANGE)
    {
        if (ShouldUseDeathGrip(priorityTarget) && CanUseAbility(DEATH_GRIP))
        {
            if (CastSpell(priorityTarget, DEATH_GRIP))
            {
                _lastDeathGrip = getMSTime();
                RecordAbilityUsage(DEATH_GRIP);
                _metrics->deathGripsUsed++;
                return true;
            }
        }
    }

    // Use Dark Command (taunt) if we're a tank
    if (_detectedSpec == DeathKnightSpec::BLOOD && ShouldUseDarkCommand(priorityTarget))
    {
        if (CanUseAbility(DARK_COMMAND))
        {
            if (CastSpell(priorityTarget, DARK_COMMAND))
            {
                _lastDarkCommand = getMSTime();
                RecordAbilityUsage(DARK_COMMAND);
                return true;
            }
        }
    }

    return false;
}

bool DeathKnightAI::HandleAoERotation(Unit* target)
{
    if (!target || !GetBot())
        return false;

    uint32 enemyCount = GetNearbyEnemyCount(10.0f);
    if (enemyCount < 3)
        return false;

    // Death and Decay for AoE damage
    if (CanUseAbility(DEATH_AND_DECAY) && _runeManager->HasRunes(1u, 1u, 1u))
    {
        if (CastSpell(DEATH_AND_DECAY))
        {
            _runeManager->ConsumeRunes(1, 1, 1);
            RecordAbilityUsage(DEATH_AND_DECAY);
            TC_LOG_DEBUG("module.playerbot.ai", "Death Knight {} using Death and Decay for AoE",
                         GetBot()->GetName());
            return true;
        }
    }

    // Blood Boil to spread diseases
    if (_diseaseManager->HasBothDiseases(target) && CanUseAbility(BLOOD_BOIL) && _runeManager->HasRunes(1u, 0u, 0u))
    {
        if (CastSpell(BLOOD_BOIL))
        {
            _runeManager->ConsumeRunes(1, 0, 0);
            RecordAbilityUsage(BLOOD_BOIL);
            TC_LOG_DEBUG("module.playerbot.ai", "Death Knight {} using Blood Boil to spread diseases",
                         GetBot()->GetName());
            return true;
        }
    }

    // Epidemic for Unholy (if available)
    if (_detectedSpec == DeathKnightSpec::UNHOLY && CanUseAbility(EPIDEMIC))
    {
        if (CastSpell(EPIDEMIC))
        {
            RecordAbilityUsage(EPIDEMIC);
            TC_LOG_DEBUG("module.playerbot.ai", "Unholy Death Knight {} using Epidemic for AoE",
                         GetBot()->GetName());
            return true;
        }
    }

    // Howling Blast for Frost
    if (_detectedSpec == DeathKnightSpec::FROST && CanUseAbility(HOWLING_BLAST) && _runeManager->HasRunes(0u, 1u, 0u))
    {
        if (CastSpell(target, HOWLING_BLAST))
        {
            _runeManager->ConsumeRunes(0, 1, 0);
            RecordAbilityUsage(HOWLING_BLAST);
            TC_LOG_DEBUG("module.playerbot.ai", "Frost Death Knight {} using Howling Blast for AoE",
                         GetBot()->GetName());
            return true;
        }
    }

    return false;
}

bool DeathKnightAI::HandleOffensiveCooldowns(Unit* target)
{
    if (!target || !GetBot())
        return false;

    bool actionTaken = false;

    switch (_detectedSpec)
    {
        case DeathKnightSpec::BLOOD:
            // Dancing Rune Weapon for Blood
            if (CanUseAbility(DANCING_RUNE_WEAPON))
            {
                if (CastSpell(DANCING_RUNE_WEAPON))
                {
                    RecordAbilityUsage(DANCING_RUNE_WEAPON);
                    _metrics->cooldownsUsed++;
                    TC_LOG_DEBUG("module.playerbot.ai", "Blood Death Knight {} activated Dancing Rune Weapon",
                                 GetBot()->GetName());
                    actionTaken = true;
                }
            }
            break;

        case DeathKnightSpec::FROST:
            // Pillar of Frost for massive damage boost
            if (CanUseAbility(PILLAR_OF_FROST))
            {
                if (CastSpell(PILLAR_OF_FROST))
                {
                    RecordAbilityUsage(PILLAR_OF_FROST);
                    _metrics->cooldownsUsed++;
                    TC_LOG_DEBUG("module.playerbot.ai", "Frost Death Knight {} activated Pillar of Frost",
                                 GetBot()->GetName());
                    actionTaken = true;
                }
            }
            // Empower Rune Weapon for rune regeneration
            if (CanUseAbility(EMPOWER_RUNE_WEAPON))
            {
                if (CastSpell(EMPOWER_RUNE_WEAPON))
                {
                    RecordAbilityUsage(EMPOWER_RUNE_WEAPON);
                    _metrics->cooldownsUsed++;
                    actionTaken = true;
                }
            }
            break;

        case DeathKnightSpec::UNHOLY:
            // Summon Gargoyle for burst damage
            if (CanUseAbility(SUMMON_GARGOYLE))
            {
                if (CastSpell(SUMMON_GARGOYLE))
                {
                    RecordAbilityUsage(SUMMON_GARGOYLE);
                    _metrics->cooldownsUsed++;
                    TC_LOG_DEBUG("module.playerbot.ai", "Unholy Death Knight {} summoned Gargoyle",
                                 GetBot()->GetName());
                    actionTaken = true;
                }
            }
            // Unholy Frenzy for attack speed
            if (CanUseAbility(UNHOLY_FRENZY))
            {
                if (CastSpell(target, UNHOLY_FRENZY))
                {
                    RecordAbilityUsage(UNHOLY_FRENZY);
                    _metrics->cooldownsUsed++;
                    actionTaken = true;
                }
            }
            break;
    }

    // Army of the Dead for major fights (all specs)
    if (target->GetTypeId() == TYPEID_UNIT && target->ToCreature()->isWorldBoss())
    {
        if (CanUseAbility(ARMY_OF_THE_DEAD))
        {
            if (CastSpell(ARMY_OF_THE_DEAD))
            {
                RecordAbilityUsage(ARMY_OF_THE_DEAD);
                _metrics->cooldownsUsed++;
                TC_LOG_DEBUG("module.playerbot.ai", "Death Knight {} summoned Army of the Dead",
                             GetBot()->GetName());
                actionTaken = true;
            }
        }
    }

    return actionTaken;
}

bool DeathKnightAI::HandleRuneAndPowerManagement(Unit* target)
{
    if (!target || !GetBot())
        return false;

    uint32 runicPower = GetBot()->GetPower(POWER_RUNIC_POWER);

    // Runic Power dump when capped
    if (runicPower >= RUNIC_POWER_DUMP_THRESHOLD)
    {
        switch (_detectedSpec)
        {
            case DeathKnightSpec::FROST:
                if (CanUseAbility(FROST_STRIKE))
                {
                    if (CastSpell(target, FROST_STRIKE))
                    {
                        _metrics->totalRunicPowerSpent += 40;
                        _runicPowerSpent += 40;
                        RecordAbilityUsage(FROST_STRIKE);
                        return true;
                    }
                }
                break;

            case DeathKnightSpec::BLOOD:
                if (CanUseAbility(RUNE_STRIKE))
                {
                    if (CastSpell(target, RUNE_STRIKE))
                    {
                        _metrics->totalRunicPowerSpent += 20;
                        _runicPowerSpent += 20;
                        RecordAbilityUsage(RUNE_STRIKE);
                        return true;
                    }
                }
                break;

            case DeathKnightSpec::UNHOLY:
                if (CanUseAbility(DEATH_COIL))
                {
                    if (CastSpell(target, DEATH_COIL))
                    {
                        _metrics->totalRunicPowerSpent += 40;
                        _runicPowerSpent += 40;
                        RecordAbilityUsage(DEATH_COIL);
                        return true;
                    }
                }
                break;
        }
    }

    // Empower Rune Weapon when out of runes
    if (!_runeManager->HasAnyRunes() && CanUseAbility(EMPOWER_RUNE_WEAPON))
    {
        if (CastSpell(EMPOWER_RUNE_WEAPON))
        {
            RecordAbilityUsage(EMPOWER_RUNE_WEAPON);
            TC_LOG_DEBUG("module.playerbot.ai", "Death Knight {} used Empower Rune Weapon for rune regeneration",
                         GetBot()->GetName());
            return true;
        }
    }

    return false;
}

void DeathKnightAI::ExecuteSpecializationRotation(Unit* target)
{
    if (!target || !GetBot())
        return;

    // Update presence if needed
    UpdatePresenceIfNeeded();

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
}

void DeathKnightAI::UpdatePresenceIfNeeded()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastPresence < PRESENCE_CHECK_INTERVAL)
        return;

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

void DeathKnightAI::UseDefensiveCooldowns()
{
    // Legacy method - now handled by HandleDefensives()
    HandleDefensives();
}

void DeathKnightAI::UseAntiMagicDefenses(Unit* target)
{
    if (!target || !GetBot())
        return;

    // Anti-Magic Shell for personal protection
    if (CanUseAbility(ANTI_MAGIC_SHELL))
    {
        CastSpell(ANTI_MAGIC_SHELL);
        RecordAbilityUsage(ANTI_MAGIC_SHELL);
    }

    // Anti-Magic Zone for group protection
    if (GetBot()->GetGroup() && CanUseAbility(ANTI_MAGIC_ZONE))
    {
        CastSpell(ANTI_MAGIC_ZONE);
        RecordAbilityUsage(ANTI_MAGIC_ZONE);
    }
}

bool DeathKnightAI::ShouldUseDeathGrip(Unit* target) const
{
    if (!target || !GetBot())
        return false;

    uint32 currentTime = getMSTime();
    if (currentTime - _lastDeathGrip < DEATH_GRIP_COOLDOWN)
        return false;

    float distance = GetBot()->GetDistance(target);
    if (distance < DEATH_GRIP_MIN_RANGE || distance > DEATH_GRIP_MAX_RANGE)
        return false;

    // Check if target is a caster that should be pulled
    if (target->GetTypeId() == TYPEID_UNIT)
    {
        Creature* creature = target->ToCreature();
        if (creature && creature->GetCreatureTemplate()->unit_class == UNIT_CLASS_MAGE)
            return true;
    }

    // Pull ranged attackers
    if (distance > 15.0f)
        return true;

    return false;
}

bool DeathKnightAI::ShouldUseDarkCommand(Unit* target) const
{
    if (!target || !GetBot())
        return false;

    // Only for tank spec
    if (_detectedSpec != DeathKnightSpec::BLOOD)
        return false;

    uint32 currentTime = getMSTime();
    if (currentTime - _lastDarkCommand < DARK_COMMAND_COOLDOWN)
        return false;

    // Check if we need to taunt
    Unit* currentVictim = target->GetVictim();
    if (!currentVictim || currentVictim == GetBot())
        return false;

    // Taunt if attacking a healer or squishy
    if (currentVictim->GetTypeId() == TYPEID_PLAYER)
    {
        Player* player = currentVictim->ToPlayer();
        if (player && (player->GetClass() == CLASS_PRIEST || player->GetClass() == CLASS_MAGE))
            return true;
    }

    return false;
}

uint32 DeathKnightAI::GetNearbyEnemyCount(float range) const
{
    if (!GetBot())
        return 0;

    uint32 count = 0;
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck u_check(GetBot(), GetBot(), range);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(GetBot(), targets, u_check);
    Cell::VisitAllObjects(GetBot(), searcher, range);

    for (auto& unit : targets)
    {
        if (GetBot()->IsValidAttackTarget(unit))
            count++;
    }

    return count;
}

bool DeathKnightAI::IsInMeleeRange(Unit* target) const
{
    if (!target || !GetBot())
        return false;

    return GetBot()->GetDistance(target) <= OPTIMAL_MELEE_RANGE;
}

bool DeathKnightAI::HasRunesForSpell(uint32 spellId) const
{
    switch (spellId)
    {
        case ICY_TOUCH:
            return _runeManager->HasRunes(0u, 1u, 0u);
        case PLAGUE_STRIKE:
            return _runeManager->HasRunes(0u, 0u, 1u);
        case BLOOD_STRIKE:
        case HEART_STRIKE:
        case BLOOD_BOIL:
            return _runeManager->HasRunes(1u, 0u, 0u);
        case DEATH_STRIKE:
        case OBLITERATE:
            return _runeManager->HasRunes(0u, 1u, 1u);
        case SCOURGE_STRIKE:
            return _runeManager->HasRunes(0u, 0u, 1u);
        case DEATH_AND_DECAY:
            return _runeManager->HasRunes(1u, 1u, 1u);
        default:
            return true;
    }
}

uint32 DeathKnightAI::GetRunicPowerCost(uint32 spellId) const
{
    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId, GetBot()->GetMap()->GetDifficultyID());
    if (!spellInfo)
        return 0;

    auto powerCosts = spellInfo->CalcPowerCost(GetBot(), spellInfo->GetSchoolMask());
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_RUNIC_POWER)
            return cost.Amount;
    }

    return 0;
}

void DeathKnightAI::RecordInterruptAttempt(Unit* target, uint32 spellId, bool success)
{
    if (success)
    {
        _successfulInterrupts++;
        _metrics->interruptsExecuted++;
        TC_LOG_DEBUG("module.playerbot.ai", "Death Knight {} successfully interrupted with spell {}",
                     GetBot()->GetName(), spellId);
    }
}

void DeathKnightAI::RecordAbilityUsage(uint32 spellId)
{
    _abilityUsage[spellId]++;
    _combatMetrics->RecordAbilityUsage(spellId, true);
}

void DeathKnightAI::OnTargetChanged(Unit* newTarget)
{
    if (!newTarget)
        return;

    TC_LOG_DEBUG("module.playerbot.ai", "Death Knight {} changed target to {}",
                 GetBot()->GetName(), newTarget->GetName());

    // Reset disease tracking for new target
    _diseaseManager->UpdateDiseases(newTarget);
}

DeathKnightAI::~DeathKnightAI()
{
    // Cleanup is handled by smart pointers for unique_ptr members
    delete _runeManager;
    delete _combatMetrics;
    delete _positioning;
    delete _metrics;
}

} // namespace Playerbot