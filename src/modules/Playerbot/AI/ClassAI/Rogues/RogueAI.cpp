/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "RogueAI.h"
#include "AssassinationSpecialization.h"
#include "CombatSpecialization.h"
#include "SubtletySpecialization.h"
#include "Player.h"
#include "Group.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Item.h"
#include "Creature.h"
#include "ObjectAccessor.h"
#include "Map.h"
#include "WorldSession.h"
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

// Performance metrics structure
struct RogueMetrics
{
    std::atomic<uint32> totalEnergySpent{0};
    std::atomic<uint32> totalComboPointsGenerated{0};
    std::atomic<uint32> totalFinishersExecuted{0};
    std::atomic<uint32> stealthOpeners{0};
    std::atomic<uint32> poisonApplications{0};
    std::atomic<uint32> interruptsExecuted{0};
    std::atomic<uint32> backstabsLanded{0};
    std::atomic<uint32> cooldownsUsed{0};
    std::atomic<float> averageReactionTime{0};
    std::atomic<float> energyEfficiency{0};
    std::chrono::steady_clock::time_point lastUpdate;

    void Reset()
    {
        totalEnergySpent = 0;
        totalComboPointsGenerated = 0;
        totalFinishersExecuted = 0;
        stealthOpeners = 0;
        poisonApplications = 0;
        interruptsExecuted = 0;
        backstabsLanded = 0;
        cooldownsUsed = 0;
        averageReactionTime = 0;
        energyEfficiency = 0;
        lastUpdate = std::chrono::steady_clock::now();
    }

    void UpdateReactionTime(float deltaMs)
    {
        float current = averageReactionTime.load();
        averageReactionTime = (current * 0.9f) + (deltaMs * 0.1f);
    }

    void UpdateEnergyEfficiency(uint32 energyUsed, uint32 comboGenerated)
    {
        if (energyUsed > 0)
        {
            float efficiency = static_cast<float>(comboGenerated) / energyUsed * 100.0f;
            float current = energyEfficiency.load();
            energyEfficiency = (current * 0.8f) + (efficiency * 0.2f);
        }
    }
};


// Combat positioning strategy
class RogueCombatPositioning
{
public:
    explicit RogueCombatPositioning(Player* bot) : _bot(bot) {}

    Position CalculateOptimalPosition(Unit* target, RogueSpec spec)
    {
        if (!target || !_bot)
            return _bot->GetPosition();

        Position optimalPos = _bot->GetPosition();
        float currentDistance = _bot->GetDistance(target);

        // Calculate angle to target's back
        float targetFacing = target->GetOrientation();
        float idealAngle = targetFacing + M_PI; // Behind target

        // Adjust for specialization preferences
        switch (spec)
        {
            case RogueSpec::ASSASSINATION:
                // Prefers staying behind for Mutilate
                if (currentDistance > 3.0f || !IsBehindTarget(target))
                {
                    optimalPos.m_positionX = target->GetPositionX() + cos(idealAngle) * 2.5f;
                    optimalPos.m_positionY = target->GetPositionY() + sin(idealAngle) * 2.5f;
                    optimalPos.m_positionZ = target->GetPositionZ();
                }
                break;

            case RogueSpec::COMBAT:
                // More flexible positioning, focuses on uptime
                if (currentDistance > 5.0f)
                {
                    float angle = target->GetRelativeAngle(_bot);
                    optimalPos.m_positionX = target->GetPositionX() + cos(angle) * 3.0f;
                    optimalPos.m_positionY = target->GetPositionY() + sin(angle) * 3.0f;
                    optimalPos.m_positionZ = target->GetPositionZ();
                }
                break;

            case RogueSpec::SUBTLETY:
                // Highly mobile, uses shadowstep
                if (currentDistance > 10.0f)
                {
                    // Position for shadowstep range
                    float angle = target->GetRelativeAngle(_bot);
                    optimalPos.m_positionX = target->GetPositionX() + cos(angle) * 8.0f;
                    optimalPos.m_positionY = target->GetPositionY() + sin(angle) * 8.0f;
                    optimalPos.m_positionZ = target->GetPositionZ();
                }
                else if (!IsBehindTarget(target))
                {
                    optimalPos.m_positionX = target->GetPositionX() + cos(idealAngle) * 2.0f;
                    optimalPos.m_positionY = target->GetPositionY() + sin(idealAngle) * 2.0f;
                    optimalPos.m_positionZ = target->GetPositionZ();
                }
                break;
        }

        return optimalPos;
    }

    bool IsBehindTarget(Unit* target) const
    {
        if (!target || !_bot)
            return false;

        float targetFacing = target->GetOrientation();
        float angleToMe = target->GetAbsoluteAngle(_bot);
        float diff = std::abs(targetFacing - angleToMe);

        if (diff > M_PI)
            diff = 2 * M_PI - diff;

        return diff < (M_PI / 3); // Within 60 degrees behind
    }

    float GetOptimalRange(RogueSpec spec) const
    {
        switch (spec)
        {
            case RogueSpec::ASSASSINATION:
                return 2.5f; // Close for mutilate
            case RogueSpec::COMBAT:
                return 5.0f; // Standard melee
            case RogueSpec::SUBTLETY:
                return 3.0f; // Flexible
            default:
                return 5.0f;
        }
    }

private:
    Player* _bot;
};

// Energy management system
class EnergyManager
{
public:
    explicit EnergyManager(Player* bot) : _bot(bot), _lastTickTime(getMSTime()) {}

    bool ShouldPoolEnergy(uint32 targetEnergy = 60)
    {
        if (!_bot)
            return false;

        uint32 currentEnergy = _bot->GetPower(POWER_ENERGY);
        return currentEnergy < targetEnergy;
    }

    float CalculateEnergyRegenRate()
    {
        // Base regen is 10 energy per second
        float baseRegen = 10.0f;

        // Check for Combat Potency talent (Combat spec)
        if (_bot->HasSpell(35551))
            baseRegen *= 1.2f;

        // Check for Vigor talent
        if (_bot->HasSpell(14983))
            baseRegen *= 1.1f;

        return baseRegen;
    }

    uint32 GetTimeToEnergy(uint32 targetEnergy)
    {
        uint32 currentEnergy = _bot->GetPower(POWER_ENERGY);
        if (currentEnergy >= targetEnergy)
            return 0;

        float regenRate = CalculateEnergyRegenRate();
        uint32 energyNeeded = targetEnergy - currentEnergy;

        return static_cast<uint32>((energyNeeded / regenRate) * 1000);
    }

    void UpdateEnergyTracking()
    {
        uint32 currentTime = getMSTime();
        uint32 deltaTime = currentTime - _lastTickTime;

        if (deltaTime >= 100) // Update every 100ms
        {
            uint32 currentEnergy = _bot->GetPower(POWER_ENERGY);
            _energyHistory.push_back({currentTime, currentEnergy});

            // Keep only last 10 seconds of history
            while (!_energyHistory.empty() &&
                   currentTime - _energyHistory.front().timestamp > 10000)
            {
                _energyHistory.erase(_energyHistory.begin());
            }

            _lastTickTime = currentTime;
        }
    }

private:
    Player* _bot;
    uint32 _lastTickTime;
    struct EnergySnapshot
    {
        uint32 timestamp;
        uint32 energy;
    };
    std::vector<EnergySnapshot> _energyHistory;
};

// Combat metrics tracking
class RogueCombatMetrics
{
public:
    void RecordAbilityUsage(uint32 spellId, bool success, uint32 energyCost = 0)
    {
        auto now = std::chrono::steady_clock::now();
        _abilityTimings[spellId] = now;

        if (success)
        {
            _successfulCasts[spellId]++;
            _totalEnergyUsed += energyCost;

            // Track finisher usage
            if (IsFinisher(spellId))
                _finisherCount++;
        }
        else
            _failedCasts[spellId]++;

        _lastGCD = now;
    }

    void RecordComboPointGeneration(uint32 points)
    {
        _totalComboPoints += points;
        _comboPointHistory.push_back({std::chrono::steady_clock::now(), points});
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
        return elapsed.count() < 1000; // 1 second GCD for rogues
    }

private:
    bool IsFinisher(uint32 spellId) const
    {
        return spellId == SLICE_AND_DICE || spellId == RUPTURE ||
               spellId == EVISCERATE || spellId == KIDNEY_SHOT ||
               spellId == EXPOSE_ARMOR || spellId == ENVENOM;
    }

    std::unordered_map<uint32, std::chrono::steady_clock::time_point> _abilityTimings;
    std::unordered_map<uint32, uint32> _successfulCasts;
    std::unordered_map<uint32, uint32> _failedCasts;
    std::chrono::steady_clock::time_point _lastGCD;
    uint32 _totalEnergyUsed = 0;
    uint32 _totalComboPoints = 0;
    uint32 _finisherCount = 0;

    struct ComboPointEvent
    {
        std::chrono::steady_clock::time_point time;
        uint32 points;
    };
    std::vector<ComboPointEvent> _comboPointHistory;
};

RogueAI::RogueAI(Player* bot) :
    ClassAI(bot),
    _detectedSpec(RogueSpec::ASSASSINATION),
    _specialization(nullptr),
    _energySpent(0),
    _comboPointsGenerated(0),
    _finishersExecuted(0),
    _lastPoison(0),
    _lastStealth(0)
{
    // Initialize combat systems
    InitializeCombatSystems();

    // Detect and initialize specialization
    DetectSpecialization();
    InitializeSpecialization();

    // Initialize performance tracking
    _metrics = std::make_unique<RogueMetrics>();
    _combatMetrics = std::make_unique<RogueCombatMetrics>();
    _energyManager = std::make_unique<EnergyManager>(bot);
    _positioning = std::make_unique<RogueCombatPositioning>(bot);

    TC_LOG_DEBUG("playerbot", "RogueAI initialized for {} with specialization {}",
                 bot->GetName(), static_cast<uint32>(_detectedSpec));
}

void RogueAI::InitializeCombatSystems()
{
    // Initialize advanced combat system components
    _threatManager = std::make_unique<BotThreatManager>(GetBot());
    _targetSelector = std::make_unique<TargetSelector>(GetBot(), _threatManager.get());
    _positionManager = std::make_unique<PositionManager>(GetBot(), _threatManager.get());
    _interruptManager = std::make_unique<InterruptManager>(GetBot());
    _cooldownManager = std::make_unique<CooldownManager>();

    TC_LOG_DEBUG("playerbot", "RogueAI combat systems initialized for {}", GetBot()->GetName());
}

void RogueAI::DetectSpecialization()
{
    if (!GetBot())
    {
        _detectedSpec = RogueSpec::ASSASSINATION;
        return;
    }

    // Advanced specialization detection based on talents and abilities
    uint32 assassinationPoints = 0;
    uint32 combatPoints = 0;
    uint32 subtletyPoints = 0;

    // Check key Assassination abilities/talents
    if (GetBot()->HasSpell(MUTILATE))
        assassinationPoints += 10;
    if (GetBot()->HasSpell(ENVENOM))
        assassinationPoints += 8;
    if (GetBot()->HasSpell(COLD_BLOOD))
        assassinationPoints += 6;
    if (GetBot()->HasSpell(VENDETTA))
        assassinationPoints += 10;
    if (GetBot()->HasSpell(1329)) // Mutilate (lower rank)
        assassinationPoints += 5;

    // Check key Combat abilities/talents
    if (GetBot()->HasSpell(BLADE_FLURRY))
        combatPoints += 10;
    if (GetBot()->HasSpell(ADRENALINE_RUSH))
        combatPoints += 8;
    if (GetBot()->HasSpell(KILLING_SPREE))
        combatPoints += 10;
    if (GetBot()->HasSpell(13877)) // Blade Flurry (base)
        combatPoints += 5;

    // Check key Subtlety abilities/talents
    if (GetBot()->HasSpell(HEMORRHAGE))
        subtletyPoints += 8;
    if (GetBot()->HasSpell(SHADOWSTEP))
        subtletyPoints += 10;
    if (GetBot()->HasSpell(SHADOW_DANCE))
        subtletyPoints += 10;
    if (GetBot()->HasSpell(14185)) // Preparation
        subtletyPoints += 6;

    // Determine specialization based on points
    if (assassinationPoints >= combatPoints && assassinationPoints >= subtletyPoints)
        _detectedSpec = RogueSpec::ASSASSINATION;
    else if (combatPoints > assassinationPoints && combatPoints >= subtletyPoints)
        _detectedSpec = RogueSpec::COMBAT;
    else
        _detectedSpec = RogueSpec::SUBTLETY;

    TC_LOG_DEBUG("playerbot", "RogueAI detected specialization: {} (A:{}, C:{}, S:{})",
                 static_cast<uint32>(_detectedSpec), assassinationPoints,
                 combatPoints, subtletyPoints);
}

void RogueAI::InitializeSpecialization()
{
    // Create specialization instance based on detected spec
    switch (_detectedSpec)
    {
        case RogueSpec::ASSASSINATION:
            _specialization = std::make_unique<AssassinationSpecialization>(GetBot());
            TC_LOG_DEBUG("playerbot", "RogueAI: Initialized Assassination specialization");
            break;

        case RogueSpec::COMBAT:
            _specialization = std::make_unique<CombatSpecialization>(GetBot());
            TC_LOG_DEBUG("playerbot", "RogueAI: Initialized Combat specialization");
            break;

        case RogueSpec::SUBTLETY:
            _specialization = std::make_unique<SubtletySpecialization>(GetBot());
            TC_LOG_DEBUG("playerbot", "RogueAI: Initialized Subtlety specialization");
            break;
    }

    // Specialization is initialized in its constructor
}

void RogueAI::UpdateRotation(Unit* target)
{
    if (!target || !GetBot())
        return;

    auto startTime = std::chrono::steady_clock::now();

    // Update energy tracking
    _energyManager->UpdateEnergyTracking();

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
}

void RogueAI::ExecuteFallbackRotation(Unit* target)
{
    if (!target || !GetBot())
        return;

    float distance = GetBot()->GetDistance(target);
    uint32 energy = GetBot()->GetPower(POWER_ENERGY);
    uint8 comboPoints = GetBot()->GetPower(POWER_COMBO_POINTS);

    // Stealth management
    if (!GetBot()->IsInCombat() && !HasAura(STEALTH) && distance > 10.0f)
    {
        if (CanUseAbility(STEALTH))
        {
            CastSpell(STEALTH);
            _metrics->stealthOpeners++;
            return;
        }
    }

    // Stealth opener
    if (HasAura(STEALTH) && distance <= 5.0f)
    {
        if (ExecuteStealthOpener(target))
            return;
    }

    // Normal combat rotation
    if (distance <= 5.0f)
    {
        // Maintain Slice and Dice
        if (comboPoints >= 2 && !HasAura(SLICE_AND_DICE))
        {
            if (CanUseAbility(SLICE_AND_DICE))
            {
                CastSpell(target, SLICE_AND_DICE);
                _metrics->totalFinishersExecuted++;
                return;
            }
        }

        // Use finishers at 5 combo points
        if (comboPoints >= 5)
        {
            if (ExecuteFinisher(target))
                return;
        }

        // Build combo points
        if (energy >= 40)
        {
            if (BuildComboPoints(target))
                return;
        }
    }

    // Handle interrupts
    if (target->HasUnitState(UNIT_STATE_CASTING))
    {
        if (_interruptManager->IsSpellInterruptWorthy(target->GetCurrentSpell(CURRENT_GENERIC_SPELL) ? target->GetCurrentSpell(CURRENT_GENERIC_SPELL)->GetSpellInfo()->Id : 0, target))
        {
            if (CanUseAbility(KICK))
            {
                CastSpell(target, KICK);
                _metrics->interruptsExecuted++;
                return;
            }
        }
    }
}

bool RogueAI::ExecuteStealthOpener(Unit* target)
{
    if (!target || !HasAura(STEALTH))
        return false;

    // Priority: Cheap Shot > Ambush > Garrote
    if (CanUseAbility(CHEAP_SHOT))
    {
        CastSpell(target, CHEAP_SHOT);
        _combatMetrics->RecordAbilityUsage(CHEAP_SHOT, true, 40);
        return true;
    }

    if (_positioning->IsBehindTarget(target) && CanUseAbility(AMBUSH))
    {
        CastSpell(target, AMBUSH);
        _combatMetrics->RecordAbilityUsage(AMBUSH, true, 60);
        return true;
    }

    if (CanUseAbility(GARROTE))
    {
        CastSpell(target, GARROTE);
        _combatMetrics->RecordAbilityUsage(GARROTE, true, 50);
        return true;
    }

    return false;
}

bool RogueAI::ExecuteFinisher(Unit* target)
{
    if (!target)
        return false;

    uint8 comboPoints = GetBot()->GetPower(POWER_COMBO_POINTS);
    if (comboPoints < 1)
        return false;

    // Maintain Slice and Dice
    if (GetAuraRemainingTime(SLICE_AND_DICE) < 5000 && comboPoints >= 2)
    {
        if (CanUseAbility(SLICE_AND_DICE))
        {
            CastSpell(target, SLICE_AND_DICE);
            _combatMetrics->RecordAbilityUsage(SLICE_AND_DICE, true, 25);
            _metrics->totalFinishersExecuted++;
            return true;
        }
    }

    // Apply/Refresh Rupture for longer fights
    if (target->GetHealthPct() > 35.0f && !HasAura(RUPTURE, target))
    {
        if (CanUseAbility(RUPTURE))
        {
            CastSpell(target, RUPTURE);
            _combatMetrics->RecordAbilityUsage(RUPTURE, true, 25);
            _metrics->totalFinishersExecuted++;
            return true;
        }
    }

    // Kidney Shot for control
    if (target->GetTypeId() == TYPEID_PLAYER && CanUseAbility(KIDNEY_SHOT))
    {
        CastSpell(target, KIDNEY_SHOT);
        _combatMetrics->RecordAbilityUsage(KIDNEY_SHOT, true, 25);
        _metrics->totalFinishersExecuted++;
        return true;
    }

    // Eviscerate for burst damage
    if (CanUseAbility(EVISCERATE))
    {
        CastSpell(target, EVISCERATE);
        _combatMetrics->RecordAbilityUsage(EVISCERATE, true, 35);
        _metrics->totalFinishersExecuted++;
        return true;
    }

    return false;
}

bool RogueAI::BuildComboPoints(Unit* target)
{
    if (!target)
        return false;

    bool behindTarget = _positioning->IsBehindTarget(target);

    // Backstab if behind
    if (behindTarget && CanUseAbility(BACKSTAB))
    {
        CastSpell(target, BACKSTAB);
        _combatMetrics->RecordAbilityUsage(BACKSTAB, true, 60);
        _combatMetrics->RecordComboPointGeneration(1);
        _metrics->backstabsLanded++;
        return true;
    }

    // Spec-specific builders
    switch (_detectedSpec)
    {
        case RogueSpec::ASSASSINATION:
            if (CanUseAbility(MUTILATE))
            {
                CastSpell(target, MUTILATE);
                _combatMetrics->RecordAbilityUsage(MUTILATE, true, 60);
                _combatMetrics->RecordComboPointGeneration(2);
                return true;
            }
            break;

        case RogueSpec::SUBTLETY:
            if (CanUseAbility(HEMORRHAGE))
            {
                CastSpell(target, HEMORRHAGE);
                _combatMetrics->RecordAbilityUsage(HEMORRHAGE, true, 35);
                _combatMetrics->RecordComboPointGeneration(1);
                return true;
            }
            break;

        case RogueSpec::COMBAT:
            // Combat prefers Sinister Strike
            break;
    }

    // Default to Sinister Strike
    if (CanUseAbility(SINISTER_STRIKE))
    {
        CastSpell(target, SINISTER_STRIKE);
        _combatMetrics->RecordAbilityUsage(SINISTER_STRIKE, true, 45);
        _combatMetrics->RecordComboPointGeneration(1);
        return true;
    }

    return false;
}

void RogueAI::UpdateBuffs()
{
    if (!GetBot())
        return;

    uint32 currentTime = getMSTime();

    // Apply poisons (check every 30 seconds)
    if (currentTime - _lastPoison > 30000)
    {
        ApplyPoisons();
        _lastPoison = currentTime;
    }

    // Maintain stealth out of combat
    if (!GetBot()->IsInCombat() && currentTime - _lastStealth > 10000)
    {
        if (!HasAura(STEALTH) && CanUseAbility(STEALTH))
        {
            CastSpell(STEALTH);
            _lastStealth = currentTime;
        }
    }

    // Delegate to specialization for spec-specific buffs
    if (_specialization)
    {
        _specialization->UpdateBuffs();
    }
}

void RogueAI::ApplyPoisons()
{
    Item* mainHand = GetBot()->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    Item* offHand = GetBot()->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);

    // Main hand poison based on spec
    if (mainHand && !mainHand->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT))
    {
        uint32 poisonSpell = 0;
        switch (_detectedSpec)
        {
            case RogueSpec::ASSASSINATION:
                poisonSpell = DEADLY_POISON;
                break;
            case RogueSpec::COMBAT:
                poisonSpell = INSTANT_POISON;
                break;
            case RogueSpec::SUBTLETY:
                poisonSpell = WOUND_POISON;
                break;
        }

        if (poisonSpell && CanUseAbility(poisonSpell))
        {
            CastSpell(poisonSpell);
            _metrics->poisonApplications++;
            TC_LOG_DEBUG("playerbot", "RogueAI: Applied main hand poison");
        }
    }

    // Off hand poison
    if (offHand && offHand->GetTemplate()->GetClass() == ITEM_CLASS_WEAPON &&
        !offHand->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT))
    {
        uint32 poisonSpell = 0;
        switch (_detectedSpec)
        {
            case RogueSpec::ASSASSINATION:
                poisonSpell = INSTANT_POISON;
                break;
            case RogueSpec::COMBAT:
                poisonSpell = CRIPPLING_POISON;
                break;
            case RogueSpec::SUBTLETY:
                poisonSpell = MIND_NUMBING_POISON;
                break;
        }

        if (poisonSpell && CanUseAbility(poisonSpell))
        {
            CastSpell(poisonSpell);
            _metrics->poisonApplications++;
            TC_LOG_DEBUG("playerbot", "RogueAI: Applied off hand poison");
        }
    }
}

void RogueAI::UpdateCooldowns(uint32 diff)
{
    if (!GetBot())
        return;

    // Update cooldown manager
    if (_cooldownManager)
        _cooldownManager->Update(diff);

    // Delegate to specialization
    if (_specialization)
    {
        _specialization->UpdateCooldowns(diff);
    }

    // Update energy manager
    _energyManager->UpdateEnergyTracking();
}

bool RogueAI::CanUseAbility(uint32 spellId)
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

bool RogueAI::HasEnoughResource(uint32 spellId)
{
    if (!GetBot())
        return false;

    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId, GetBot()->GetMap()->GetDifficultyID());
    if (!spellInfo)
        return false;

    // Check energy cost
    auto powerCosts = spellInfo->CalcPowerCost(GetBot(), spellInfo->GetSchoolMask());
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_ENERGY)
        {
            uint32 currentEnergy = GetBot()->GetPower(POWER_ENERGY);
            if (currentEnergy < cost.Amount)
                return false;
        }
    }

    // Check combo point requirements for finishers
    if (IsFinisher(spellId))
    {
        uint8 comboPoints = GetBot()->GetPower(POWER_COMBO_POINTS);
        if (comboPoints < 1)
            return false;
    }

    // Additional checks via specialization
    if (_specialization)
    {
        return _specialization->HasEnoughResource(spellId);
    }

    return true;
}

bool RogueAI::IsFinisher(uint32 spellId) const
{
    return spellId == SLICE_AND_DICE || spellId == RUPTURE ||
           spellId == EVISCERATE || spellId == KIDNEY_SHOT ||
           spellId == EXPOSE_ARMOR || spellId == ENVENOM ||
           spellId == DEADLY_THROW;
}

void RogueAI::ConsumeResource(uint32 spellId)
{
    if (!GetBot())
        return;

    const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId, GetBot()->GetMap()->GetDifficultyID());
    if (!spellInfo)
        return;

    // Track energy consumption
    auto powerCosts = spellInfo->CalcPowerCost(GetBot(), spellInfo->GetSchoolMask());
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_ENERGY)
        {
            _metrics->totalEnergySpent += cost.Amount;
            _energySpent += cost.Amount;
        }
    }

    // Track combo point consumption for finishers
    if (IsFinisher(spellId))
    {
        _metrics->totalFinishersExecuted++;
        _finishersExecuted++;
    }

    // Track combo point generation
    if (!IsFinisher(spellId))
    {
        _metrics->totalComboPointsGenerated++;
        _comboPointsGenerated++;
    }

    // Delegate to specialization
    if (_specialization)
    {
        _specialization->ConsumeResource(spellId);
    }
}

void RogueAI::OnCombatStart(Unit* target)
{
    if (!target || !GetBot())
        return;

    ClassAI::OnCombatStart(target);

    // Reset combat metrics
    _energySpent = 0;
    _comboPointsGenerated = 0;
    _finishersExecuted = 0;

    // Open from stealth if possible
    if (HasAura(STEALTH))
    {
        ExecuteStealthOpener(target);
    }

    // Use offensive cooldowns for boss fights
    if (target->GetTypeId() == TYPEID_UNIT && target->ToCreature()->isWorldBoss())
    {
        ActivateBurstCooldowns(target);
    }

    // Use defensive cooldowns if low health
    if (GetBot()->GetHealthPct() < 50.0f)
    {
        if (CanUseAbility(EVASION))
        {
            CastSpell(EVASION);
            TC_LOG_DEBUG("playerbot", "RogueAI: Activated Evasion (defensive)");
        }
    }

    // Delegate to specialization
    if (_specialization)
    {
        _specialization->OnCombatStart(target);
    }

    TC_LOG_DEBUG("playerbot", "RogueAI: Combat started against {} with spec {}",
                 target->GetName(), static_cast<uint32>(_detectedSpec));
}

void RogueAI::OnCombatEnd()
{
    ClassAI::OnCombatEnd();

    // Calculate combat metrics
    if (_energySpent > 0)
    {
        float efficiency = static_cast<float>(_comboPointsGenerated) / _energySpent;
        _metrics->UpdateEnergyEfficiency(_energySpent, _comboPointsGenerated);
    }

    // Re-stealth after combat
    if (!HasAura(STEALTH) && CanUseAbility(STEALTH))
    {
        CastSpell(STEALTH);
    }

    // Delegate to specialization
    if (_specialization)
    {
        _specialization->OnCombatEnd();
    }

    TC_LOG_DEBUG("playerbot", "RogueAI: Combat ended. Energy spent: {}, CP generated: {}, Finishers: {}",
                 _energySpent, _comboPointsGenerated, _finishersExecuted);
}

void RogueAI::ActivateBurstCooldowns(Unit* target)
{
    if (!target)
        return;

    switch (_detectedSpec)
    {
        case RogueSpec::ASSASSINATION:
            if (CanUseAbility(COLD_BLOOD))
            {
                CastSpell(COLD_BLOOD);
                _metrics->cooldownsUsed++;
            }
            if (CanUseAbility(VENDETTA))
            {
                CastSpell(target, VENDETTA);
                _metrics->cooldownsUsed++;
            }
            break;

        case RogueSpec::COMBAT:
            if (CanUseAbility(BLADE_FLURRY))
            {
                CastSpell(BLADE_FLURRY);
                _metrics->cooldownsUsed++;
            }
            if (CanUseAbility(ADRENALINE_RUSH))
            {
                CastSpell(ADRENALINE_RUSH);
                _metrics->cooldownsUsed++;
            }
            if (CanUseAbility(KILLING_SPREE))
            {
                CastSpell(target, KILLING_SPREE);
                _metrics->cooldownsUsed++;
            }
            break;

        case RogueSpec::SUBTLETY:
            if (CanUseAbility(SHADOW_DANCE))
            {
                CastSpell(SHADOW_DANCE);
                _metrics->cooldownsUsed++;
            }
            if (CanUseAbility(SHADOWSTEP))
            {
                CastSpell(target, SHADOWSTEP);
                _metrics->cooldownsUsed++;
            }
            break;
    }
}

Position RogueAI::GetOptimalPosition(Unit* target)
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

float RogueAI::GetOptimalRange(Unit* target)
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

RogueSpec RogueAI::GetCurrentSpecialization() const
{
    return _detectedSpec;
}

RogueAI::~RogueAI()
{
    // Cleanup is handled by smart pointers
}

} // namespace Playerbot