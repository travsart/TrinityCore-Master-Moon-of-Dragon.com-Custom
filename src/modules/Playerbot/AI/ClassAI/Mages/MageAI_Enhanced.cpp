/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "MageAI.h"
#include "ArcaneSpecialization_Enhanced.h"
#include "FireSpecialization_Enhanced.h"
#include "FrostSpecialization_Enhanced.h"
#include "Player.h"
#include "SpellMgr.h"
#include "Log.h"

namespace Playerbot
{

MageAI::MageAI(Player* bot) : ClassAI(bot), _currentSpec(MageSpec::ARCANE)
{
    InitializeSpecialization();

    // Initialize combat systems
    _threatManager = std::make_unique<ThreatManager>(bot);
    _targetSelector = std::make_unique<TargetSelector>(bot);
    _positionManager = std::make_unique<PositionManager>(bot);
    _interruptManager = std::make_unique<InterruptManager>(bot);

    // Reset performance tracking
    _combatMetrics.Reset();

    TC_LOG_DEBUG("playerbots", "MageAI initialized for player {} with specialization {}",
                 bot->GetName(), static_cast<uint32>(_currentSpec));
}

void MageAI::UpdateRotation(::Unit* target)
{
    if (!target || !_specialization)
        return;

    // Update advanced combat logic
    UpdateAdvancedCombatLogic(target);

    // Update specialization-specific rotation
    _specialization->UpdateRotation(target);

    // Handle crowd control situations
    UseCrowdControl(target);

    // Update positioning
    UpdateMagePositioning();

    // Handle emergency situations
    if (IsInCriticalDanger())
        HandleEmergencySituation();

    // Optimize casting sequence
    OptimizeCastingSequence(target);
}

void MageAI::UpdateBuffs()
{
    if (!_specialization)
        return;

    // Update specialization buffs
    _specialization->UpdateBuffs();

    // Update general mage buffs
    UpdateMageBuffs();

    // Handle armor spells
    UpdateArmorSpells();

    // Cast Arcane Intellect if needed
    CastArcaneIntellect();
}

void MageAI::UpdateCooldowns(uint32 diff)
{
    if (!_specialization)
        return;

    // Update specialization cooldowns
    _specialization->UpdateCooldowns(diff);

    // Update performance metrics
    UpdatePerformanceMetrics(diff);

    // Update combat systems
    if (_threatManager)
        _threatManager->Update(diff);
    if (_interruptManager)
        _interruptManager->Update(diff);
}

bool MageAI::CanUseAbility(uint32 spellId)
{
    if (!_specialization)
        return false;

    // Check specialization-specific restrictions
    if (!_specialization->CanUseAbility(spellId))
        return false;

    // Check if we have enough mana
    if (!HasEnoughResource(spellId))
        return false;

    // Check if we're not silenced or interrupted
    if (_bot->HasUnitState(UNIT_STATE_SILENCED))
        return false;

    return true;
}

void MageAI::OnCombatStart(::Unit* target)
{
    if (!_specialization || !target)
        return;

    TC_LOG_DEBUG("playerbots", "MageAI combat started for player {} against {}",
                 _bot->GetName(), target->GetName());

    // Reset combat metrics
    _combatMetrics.Reset();

    // Notify specialization
    _specialization->OnCombatStart(target);

    // Apply defensive buffs
    UseBarrierSpells();

    // Summon Water Elemental if Frost
    if (_currentSpec == MageSpec::FROST)
    {
        if (auto frostSpec = dynamic_cast<FrostSpecialization_Enhanced*>(_specialization.get()))
        {
            frostSpec->ExecuteSummonWaterElemental();
        }
    }

    // Initialize threat management
    if (_threatManager)
        _threatManager->OnCombatStart(target);
}

void MageAI::OnCombatEnd()
{
    if (!_specialization)
        return;

    TC_LOG_DEBUG("playerbots", "MageAI combat ended for player {}", _bot->GetName());

    // Notify specialization
    _specialization->OnCombatEnd();

    // Analyze combat effectiveness
    AnalyzeCastingEffectiveness();

    // Update spell priorities based on performance
    OptimizeSpellPriorities();

    // Reset positioning
    if (_positionManager)
        _positionManager->OnCombatEnd();
}

bool MageAI::HasEnoughResource(uint32 spellId)
{
    if (!_specialization)
        return false;

    return _specialization->HasEnoughResource(spellId);
}

void MageAI::ConsumeResource(uint32 spellId)
{
    if (!_specialization)
        return;

    _specialization->ConsumeResource(spellId);

    // Track mana spending
    uint32 manaCost = CalculateSpellManaCost(spellId, _bot);
    _manaSpent += manaCost;
    _combatMetrics.totalManaSpent += manaCost;
}

Position MageAI::GetOptimalPosition(::Unit* target)
{
    if (!_specialization || !target)
        return _bot->GetPosition();

    // Get specialization-specific optimal position
    Position specPosition = _specialization->GetOptimalPosition(target);

    // Adjust for threat and safety
    if (_positionManager)
    {
        return _positionManager->OptimizePosition(specPosition, target);
    }

    return specPosition;
}

float MageAI::GetOptimalRange(::Unit* target)
{
    if (!_specialization || !target)
        return OPTIMAL_CASTING_RANGE;

    float specRange = _specialization->GetOptimalRange(target);

    // Adjust for threat level
    if (HasTooMuchThreat())
        specRange = std::max(specRange, KITING_RANGE);

    return specRange;
}

void MageAI::InitializeSpecialization()
{
    if (!_bot)
        return;

    // Detect current specialization
    _currentSpec = DetectCurrentSpecialization();

    // Create appropriate specialization instance
    switch (_currentSpec)
    {
        case MageSpec::ARCANE:
            _specialization = std::make_unique<ArcaneSpecialization_Enhanced>(_bot);
            TC_LOG_DEBUG("playerbots", "Initialized Arcane specialization for {}", _bot->GetName());
            break;

        case MageSpec::FIRE:
            _specialization = std::make_unique<FireSpecialization_Enhanced>(_bot);
            TC_LOG_DEBUG("playerbots", "Initialized Fire specialization for {}", _bot->GetName());
            break;

        case MageSpec::FROST:
            _specialization = std::make_unique<FrostSpecialization_Enhanced>(_bot);
            TC_LOG_DEBUG("playerbots", "Initialized Frost specialization for {}", _bot->GetName());
            break;

        default:
            TC_LOG_ERROR("playerbots", "Unknown mage specialization for player {}: {}",
                         _bot->GetName(), static_cast<uint32>(_currentSpec));
            _specialization = std::make_unique<ArcaneSpecialization_Enhanced>(_bot);
            break;
    }

    if (!_specialization)
    {
        TC_LOG_ERROR("playerbots", "Failed to initialize mage specialization for player {}", _bot->GetName());
        return;
    }

    TC_LOG_INFO("playerbots", "Successfully initialized Mage AI for player {} with {} specialization",
                _bot->GetName(),
                _currentSpec == MageSpec::ARCANE ? "Arcane" :
                _currentSpec == MageSpec::FIRE ? "Fire" : "Frost");
}

MageSpec MageAI::DetectCurrentSpecialization()
{
    if (!_bot)
        return MageSpec::ARCANE;

    // Count talent points in each tree
    uint32 arcanePoints = 0;
    uint32 firePoints = 0;
    uint32 frostPoints = 0;

    // Iterate through talent trees
    for (uint32 i = 0; i < MAX_TALENT_TABS; ++i)
    {
        for (uint32 j = 0; j < MAX_TALENT_RANK; ++j)
        {
            if (PlayerTalentMap::iterator itr = _bot->GetTalentMap(PLAYER_TALENT_SPEC_ACTIVE)->find(i * MAX_TALENT_RANK + j);
                itr != _bot->GetTalentMap(PLAYER_TALENT_SPEC_ACTIVE)->end())
            {
                TalentEntry const* talentInfo = sTalentStore.LookupEntry(itr->second->talentId);
                if (!talentInfo)
                    continue;

                switch (talentInfo->TalentTab)
                {
                    case 0: arcanePoints += itr->second->currentRank; break;
                    case 1: firePoints += itr->second->currentRank; break;
                    case 2: frostPoints += itr->second->currentRank; break;
                }
            }
        }
    }

    // Determine specialization based on highest talent investment
    if (arcanePoints >= firePoints && arcanePoints >= frostPoints)
        return MageSpec::ARCANE;
    else if (firePoints >= frostPoints)
        return MageSpec::FIRE;
    else
        return MageSpec::FROST;
}

// Enhanced methods implementation for MageAI

void MageAI::UpdateAdvancedCombatLogic(::Unit* target)
{
    if (!target)
        return;

    // Threat analysis and management
    if (_threatManager)
    {
        ThreatManager::ThreatAnalysis threatAnalysis = _threatManager->AnalyzeThreatSituation();

        if (threatAnalysis.threatLevel > ThreatManager::ThreatLevel::MODERATE)
        {
            HandleHighThreatSituation();
        }
    }

    // Mana management and resource optimization
    ManageResourceEfficiency();

    // Combat phase transitions
    HandleCombatPhaseTransitions();

    // Predictive casting based on enemy behavior
    PredictEnemyMovement(target);

    // School mastery optimization
    UpdateSchoolMastery();
    AdaptToTargetResistances(target);
}

::Unit* MageAI::SelectOptimalTarget(const std::vector<::Unit*>& enemies)
{
    if (enemies.empty())
        return nullptr;

    if (!_targetSelector)
        return enemies.front();

    TargetSelector::SelectionContext context;
    context.currentTarget = GetBot()->GetTarget();
    context.maxRange = OPTIMAL_CASTING_RANGE;
    context.role = TargetSelector::SelectionRole::RANGED_DPS;
    context.prioritizeInterrupts = true;

    TargetSelector::SelectionResult result = _targetSelector->SelectBestTarget(context);

    if (result.success && result.target)
        return result.target;

    // Fallback to priority-based selection
    ::Unit* bestTarget = nullptr;
    float bestPriority = 0.0f;

    for (::Unit* enemy : enemies)
    {
        if (!enemy || !enemy->IsAlive())
            continue;

        float priority = CalculateTargetPriority(enemy);

        if (priority > bestPriority)
        {
            bestPriority = priority;
            bestTarget = enemy;
        }
    }

    return bestTarget;
}

float MageAI::CalculateTargetPriority(::Unit* target)
{
    if (!target)
        return 0.0f;

    float priority = 1.0f;

    // Higher priority for lower health enemies (easier kills)
    float healthPct = target->GetHealthPct();
    if (healthPct < 30.0f)
        priority += 2.0f;
    else if (healthPct < 60.0f)
        priority += 1.0f;

    // Higher priority for casting enemies (interrupt potential)
    if (target->HasUnitState(UNIT_STATE_CASTING))
        priority += 3.0f;

    // Higher priority for closer enemies
    float distance = GetBot()->GetDistance(target);
    if (distance < 15.0f)
        priority += 1.5f;
    else if (distance > 35.0f)
        priority -= 1.0f;

    // Lower priority for heavily armored targets
    if (target->GetArmor() > 5000)
        priority -= 0.5f;

    // Higher priority for targets with debuffs
    if (target->HasAura(LIVING_BOMB) || target->HasAura(POLYMORPH))
        priority += 1.0f;

    return priority;
}

void MageAI::HandleMultipleEnemies(const std::vector<::Unit*>& enemies)
{
    if (enemies.size() < 3)
        return;

    // Use AoE crowd control
    if (CanUseAbility(FROST_NOVA) && !IsOnCooldown(FROST_NOVA))
    {
        GetBot()->CastSpell(GetBot(), FROST_NOVA, false);
        RecordSpellCast(FROST_NOVA, nullptr);
        return;
    }

    // Use Blizzard for large groups
    if (enemies.size() >= 5 && CanUseAbility(BLIZZARD) && HasEnoughMana(600))
    {
        Position centerPos = CalculateAoECenter(enemies);
        if (centerPos.IsPositionValid())
        {
            GetBot()->CastSpell(centerPos.GetPositionX(), centerPos.GetPositionY(), centerPos.GetPositionZ(), BLIZZARD, false);
            RecordSpellCast(BLIZZARD, nullptr);
            return;
        }
    }

    // Use Flamestrike for medium groups
    if (enemies.size() >= 3 && CanUseAbility(FLAMESTRIKE) && HasEnoughMana(500))
    {
        Position centerPos = CalculateAoECenter(enemies);
        if (centerPos.IsPositionValid())
        {
            GetBot()->CastSpell(centerPos.GetPositionX(), centerPos.GetPositionY(), centerPos.GetPositionZ(), FLAMESTRIKE, false);
            RecordSpellCast(FLAMESTRIKE, nullptr);
            return;
        }
    }

    // Kite the group
    if (_positionManager)
    {
        PositionManager::MovementContext context;
        context.primaryTarget = enemies.front();
        context.actor = GetBot();
        context.role = PositionManager::MovementRole::RANGED_DPS;
        context.preferredRange = KITING_RANGE;
        context.avoidMelee = true;

        _positionManager->UpdatePosition(context);
    }
}

Position MageAI::CalculateAoECenter(const std::vector<::Unit*>& enemies)
{
    if (enemies.empty())
        return Position();

    float totalX = 0.0f, totalY = 0.0f, totalZ = 0.0f;
    uint32 validCount = 0;

    for (::Unit* enemy : enemies)
    {
        if (enemy && enemy->IsAlive())
        {
            totalX += enemy->GetPositionX();
            totalY += enemy->GetPositionY();
            totalZ += enemy->GetPositionZ();
            ++validCount;
        }
    }

    if (validCount == 0)
        return Position();

    return Position(totalX / validCount, totalY / validCount, totalZ / validCount);
}

void MageAI::HandleLowManaEmergency()
{
    float manaPct = GetManaPercent();

    if (manaPct < MANA_EMERGENCY_THRESHOLD)
    {
        // Use mana gems if available
        if (HasManaGem())
        {
            UseManaGem();
            return;
        }

        // Use mana shield to convert health to effective mana
        if (!GetBot()->HasAura(MANA_SHIELD) && CanUseAbility(MANA_SHIELD))
        {
            CastManaShield();
        }

        // Use Evocation if available (assuming it exists)
        uint32 evocationSpell = 12051; // Evocation spell ID
        if (CanUseAbility(evocationSpell) && !IsOnCooldown(evocationSpell))
        {
            GetBot()->CastSpell(GetBot(), evocationSpell, false);
            RecordSpellCast(evocationSpell, GetBot());
        }

        // Switch to mana-conservative rotation
        EnterConservePhase();
    }
}

void MageAI::HandleHighThreatSituation()
{
    // Use threat reduction abilities
    if (!GetBot()->HasAura(INVISIBILITY) && CanUseAbility(INVISIBILITY))
    {
        GetBot()->CastSpell(GetBot(), INVISIBILITY, false);
        RecordSpellCast(INVISIBILITY, GetBot());
        return;
    }

    // Use Blink to create distance
    if (CanUseAbility(BLINK) && !IsOnCooldown(BLINK))
    {
        UseBlink();
        return;
    }

    // Use Ice Block as last resort
    if (GetBot()->GetHealthPct() < 20.0f && CanUseAbility(ICE_BLOCK))
    {
        GetBot()->CastSpell(GetBot(), ICE_BLOCK, false);
        RecordSpellCast(ICE_BLOCK, GetBot());
        return;
    }

    // Use crowd control to manage threat
    std::vector<::Unit*> nearbyEnemies = GetNearbyEnemies(15.0f);
    for (::Unit* enemy : nearbyEnemies)
    {
        if (CanPolymorphSafely(enemy))
        {
            UsePolymorph(enemy);
            break;
        }
    }
}

void MageAI::ExecuteEmergencyTeleport()
{
    // Check if we have teleport spells available
    std::vector<uint32> teleportSpells = {
        TELEPORT_STORMWIND,
        TELEPORT_IRONFORGE,
        3563, // Teleport: Undercity
        3566, // Teleport: Thunder Bluff
        3567, // Teleport: Orgrimmar
        32271, // Teleport: Exodar
        32272  // Teleport: Silvermoon
    };

    for (uint32 spellId : teleportSpells)
    {
        if (CanUseAbility(spellId) && !IsOnCooldown(spellId))
        {
            GetBot()->CastSpell(GetBot(), spellId, false);
            RecordSpellCast(spellId, GetBot());
            TC_LOG_INFO("playerbot.mage", "Emergency teleport executed by {}", GetBot()->GetName());
            return;
        }
    }
}

void MageAI::OptimizeCastingSequence(::Unit* target)
{
    if (!target)
        return;

    // Analyze target's movement patterns
    bool isMovingTarget = target->HasUnitState(UNIT_STATE_MOVING);

    if (isMovingTarget)
    {
        // Prioritize instant casts for moving targets
        OptimizeInstantCasts();
    }
    else
    {
        // Use slower, more powerful spells for stationary targets
        if (_currentSpec == MageSpec::FIRE && CanUseAbility(PYROBLAST))
        {
            GetBot()->CastSpell(target, PYROBLAST, false);
            RecordSpellCast(PYROBLAST, target);
        }
        else if (_currentSpec == MageSpec::FROST && CanUseAbility(FROSTBOLT))
        {
            GetBot()->CastSpell(target, FROSTBOLT, false);
            RecordSpellCast(FROSTBOLT, target);
        }
    }
}

void MageAI::ManageResourceEfficiency()
{
    float manaPct = GetManaPercent();

    // Adjust rotation based on mana levels
    if (manaPct < MANA_CONSERVATION_THRESHOLD)
    {
        EnterConservePhase();
    }
    else if (manaPct > 0.8f)
    {
        EnterBurnPhase();
    }

    // Use mana-efficient spells when low
    if (manaPct < 0.4f)
    {
        // Prioritize low-cost, high-efficiency spells
        SetSpellPriority(FIRE_BLAST, 10);
        SetSpellPriority(ICE_LANCE, 9);
        SetSpellPriority(SCORCH, 8);
    }
}

void MageAI::HandleCombatPhaseTransitions()
{
    uint32 currentTime = getMSTime();
    auto combatDuration = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - _combatMetrics.combatStartTime).count();

    // Early combat (0-15 seconds): Aggressive opening
    if (combatDuration < 15)
    {
        UseOffensiveCooldowns();
    }
    // Mid combat (15-60 seconds): Sustained damage
    else if (combatDuration < 60)
    {
        ManageResourceEfficiency();
    }
    // Late combat (60+ seconds): Conservation mode
    else
    {
        EnterConservePhase();
        HandleLowManaEmergency();
    }
}

void MageAI::PredictEnemyMovement(::Unit* target)
{
    if (!target)
        return;

    // Store previous position for movement prediction
    static std::unordered_map<ObjectGuid, Position> previousPositions;
    static std::unordered_map<ObjectGuid, uint32> lastUpdateTimes;

    ObjectGuid targetGuid = target->GetGUID();
    Position currentPos = target->GetPosition();
    uint32 currentTime = getMSTime();

    auto prevPosIt = previousPositions.find(targetGuid);
    auto lastTimeIt = lastUpdateTimes.find(targetGuid);

    if (prevPosIt != previousPositions.end() && lastTimeIt != lastUpdateTimes.end())
    {
        Position prevPos = prevPosIt->second;
        uint32 timeDiff = currentTime - lastTimeIt->second;

        if (timeDiff > 0)
        {
            // Calculate movement vector
            float deltaX = currentPos.GetPositionX() - prevPos.GetPositionX();
            float deltaY = currentPos.GetPositionY() - prevPos.GetPositionY();
            float speed = sqrt(deltaX * deltaX + deltaY * deltaY) / (timeDiff / 1000.0f);

            // If target is moving fast, use instant casts
            if (speed > 5.0f)
            {
                OptimizeInstantCasts();
            }
        }
    }

    // Update tracking data
    previousPositions[targetGuid] = currentPos;
    lastUpdateTimes[targetGuid] = currentTime;
}

void MageAI::PrecastSpells(::Unit* target)
{
    if (!target)
        return;

    // Precast high-damage spells when target is predictable
    if (!target->HasUnitState(UNIT_STATE_MOVING))
    {
        if (_currentSpec == MageSpec::FIRE)
        {
            if (CanUseAbility(PYROBLAST) && !GetBot()->HasUnitState(UNIT_STATE_CASTING))
            {
                GetBot()->CastSpell(target, PYROBLAST, false);
                RecordSpellCast(PYROBLAST, target);
            }
        }
        else if (_currentSpec == MageSpec::ARCANE)
        {
            if (GetArcaneCharges() < MAX_ARCANE_CHARGES && CanUseAbility(ARCANE_BLAST))
            {
                GetBot()->CastSpell(target, ARCANE_BLAST, false);
                RecordSpellCast(ARCANE_BLAST, target);
            }
        }
    }
}

void MageAI::HandleMovingTargets(::Unit* target)
{
    if (!target || !target->HasUnitState(UNIT_STATE_MOVING))
        return;

    // Use instant or fast-cast spells
    OptimizeInstantCasts();

    // Lead the target for projectile spells
    Position targetPos = target->GetPosition();
    Position predictedPos = PredictTargetPosition(target, 1.5f); // 1.5 second prediction

    if (predictedPos.IsPositionValid())
    {
        // Cast AoE spells at predicted position
        if (CanUseAbility(FLAMESTRIKE))
        {
            GetBot()->CastSpell(predictedPos.GetPositionX(), predictedPos.GetPositionY(),
                              predictedPos.GetPositionZ(), FLAMESTRIKE, false);
            RecordSpellCast(FLAMESTRIKE, target);
        }
    }
}

Position MageAI::PredictTargetPosition(::Unit* target, float seconds)
{
    if (!target)
        return Position();

    // Simple linear prediction based on current movement
    Position currentPos = target->GetPosition();

    if (!target->HasUnitState(UNIT_STATE_MOVING))
        return currentPos;

    // Get movement direction (simplified)
    float orientation = target->GetOrientation();
    float speed = target->GetSpeed(MOVE_RUN);

    float deltaX = cos(orientation) * speed * seconds;
    float deltaY = sin(orientation) * speed * seconds;

    return Position(currentPos.GetPositionX() + deltaX,
                   currentPos.GetPositionY() + deltaY,
                   currentPos.GetPositionZ());
}

void MageAI::OptimizeInstantCasts()
{
    // Prioritize instant-cast spells
    if (CanUseAbility(FIRE_BLAST))
    {
        SetSpellPriority(FIRE_BLAST, 15);
    }

    if (CanUseAbility(ICE_LANCE))
    {
        SetSpellPriority(ICE_LANCE, 14);
    }

    if (HasClearcastingProc() && CanUseAbility(ARCANE_MISSILES))
    {
        SetSpellPriority(ARCANE_MISSILES, 16);
    }

    // Use Presence of Mind to make next spell instant
    if (CanUseAbility(PRESENCE_OF_MIND) && !IsOnCooldown(PRESENCE_OF_MIND))
    {
        CastPresenceOfMind();
    }
}

void MageAI::UpdateSchoolMastery()
{
    // Analyze recent spell effectiveness
    AnalyzeCastingEffectiveness();

    // Adapt spell priorities based on effectiveness
    float arcaneEfficiency = CalculateSpellEfficiency(ARCANE_MISSILES);
    float fireEfficiency = CalculateSpellEfficiency(FIREBALL);
    float frostEfficiency = CalculateSpellEfficiency(FROSTBOLT);

    // Adjust rotation based on most effective school
    if (fireEfficiency > arcaneEfficiency && fireEfficiency > frostEfficiency)
    {
        SetSpellPriority(FIREBALL, 12);
        SetSpellPriority(FIRE_BLAST, 11);
    }
    else if (frostEfficiency > arcaneEfficiency)
    {
        SetSpellPriority(FROSTBOLT, 12);
        SetSpellPriority(ICE_LANCE, 11);
    }
    else
    {
        SetSpellPriority(ARCANE_MISSILES, 12);
        SetSpellPriority(ARCANE_BLAST, 11);
    }
}

float MageAI::GetSchoolMasteryBonus(MageSchool school)
{
    // Calculate mastery bonus based on specialization and talents
    float bonus = 1.0f;

    if (_currentSpec == MageSpec::ARCANE && school == MageSchool::ARCANE)
        bonus += 0.15f;
    else if (_currentSpec == MageSpec::FIRE && school == MageSchool::FIRE)
        bonus += 0.15f;
    else if (_currentSpec == MageSpec::FROST && school == MageSchool::FROST)
        bonus += 0.15f;

    // Add talent bonuses (simplified)
    if (HasTalent(11210)) // Improved Fireball
        bonus += 0.05f;
    if (HasTalent(11222)) // Improved Frostbolt
        bonus += 0.05f;

    return bonus;
}

void MageAI::AdaptToTargetResistances(::Unit* target)
{
    if (!target)
        return;

    // Check target's resistances
    uint32 fireResist = target->GetResistance(SPELL_SCHOOL_FIRE);
    uint32 frostResist = target->GetResistance(SPELL_SCHOOL_FROST);
    uint32 arcaneResist = target->GetResistance(SPELL_SCHOOL_ARCANE);

    // Switch to most effective school
    MageSchool optimalSchool = GetMostEffectiveSchool(target);

    switch (optimalSchool)
    {
        case MageSchool::FIRE:
            SetSpellPriority(FIREBALL, 15);
            SetSpellPriority(FIRE_BLAST, 14);
            break;
        case MageSchool::FROST:
            SetSpellPriority(FROSTBOLT, 15);
            SetSpellPriority(ICE_LANCE, 14);
            break;
        case MageSchool::ARCANE:
            SetSpellPriority(ARCANE_MISSILES, 15);
            SetSpellPriority(ARCANE_BLAST, 14);
            break;
        default:
            break;
    }
}

MageSchool MageAI::GetMostEffectiveSchool(::Unit* target)
{
    if (!target)
        return MageSchool::ARCANE;

    uint32 fireResist = target->GetResistance(SPELL_SCHOOL_FIRE);
    uint32 frostResist = target->GetResistance(SPELL_SCHOOL_FROST);
    uint32 arcaneResist = target->GetResistance(SPELL_SCHOOL_ARCANE);

    // Return school with lowest resistance
    if (fireResist <= frostResist && fireResist <= arcaneResist)
        return MageSchool::FIRE;
    else if (frostResist <= arcaneResist)
        return MageSchool::FROST;
    else
        return MageSchool::ARCANE;
}

void MageAI::RecordSpellResist(uint32 spellId, ::Unit* target)
{
    // Track resistance for spell effectiveness analysis
    static std::unordered_map<uint32, uint32> resistCounts;
    static std::unordered_map<uint32, uint32> totalCasts;

    resistCounts[spellId]++;
    totalCasts[spellId]++;

    TC_LOG_DEBUG("playerbot.mage", "Spell {} resisted by {} (Total resists: {}/{})",
                 spellId, target ? target->GetName() : "Unknown",
                 resistCounts[spellId], totalCasts[spellId]);
}

void MageAI::RecordInterruptAttempt(uint32 spellId, ::Unit* target, bool success)
{
    if (success)
        _successfulCounterspells++;

    // Update interrupt success rate
    float currentRate = _combatMetrics.interruptSuccessRate.load();
    float newRate = success ? (currentRate + 1.0f) / 2.0f : currentRate * 0.9f;
    _combatMetrics.interruptSuccessRate.store(newRate);

    TC_LOG_DEBUG("playerbot.mage", "Interrupt attempt on spell {} by {}: {} (Success rate: {:.1f}%)",
                 spellId, target ? target->GetName() : "Unknown",
                 success ? "SUCCESS" : "FAILED", newRate * 100.0f);
}

float MageAI::CalculateSpellEfficiency(uint32 spellId)
{
    // Calculate spell efficiency based on damage per mana
    static std::unordered_map<uint32, float> damagePerMana;

    // Get spell info
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return 0.0f;

    auto powerCosts = spellInfo->CalcPowerCost(GetBot(), spellInfo->GetSchoolMask()); uint32 manaCost = 0; for (auto const& cost : powerCosts) { if (cost.Power == POWER_MANA) { manaCost = cost.Amount; break; } }
    if (manaCost == 0)
        return 100.0f; // Free spells are infinitely efficient

    // Estimate average damage (simplified)
    float avgDamage = EstimateSpellDamage(spellId);

    float efficiency = avgDamage / manaCost;
    damagePerMana[spellId] = efficiency;

    return efficiency;
}

float MageAI::EstimateSpellDamage(uint32 spellId)
{
    // Simplified damage estimation
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return 0.0f;

    // Get base damage from spell
    float baseDamage = spellInfo->GetEffect(EFFECT_0)->CalcValue(GetBot());

    // Apply spell power and other modifiers
    float spellPower = GetBot()->GetUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + SPELL_SCHOOL_FIRE);
    float modifier = 1.0f + (spellPower / 1000.0f); // Simplified calculation

    return baseDamage * modifier;
}

void MageAI::OptimizeSpellPriorities()
{
    // Optimize spell priorities based on current situation
    std::vector<::Unit*> nearbyEnemies = GetNearbyEnemies(30.0f);

    if (nearbyEnemies.size() > 3)
    {
        // AoE situation - prioritize AoE spells
        SetSpellPriority(BLIZZARD, 20);
        SetSpellPriority(FLAMESTRIKE, 19);
        SetSpellPriority(ARCANE_EXPLOSION, 18);
    }
    else if (nearbyEnemies.size() == 1)
    {
        // Single target - prioritize single target spells
        SetSpellPriority(FIREBALL, 20);
        SetSpellPriority(FROSTBOLT, 19);
        SetSpellPriority(ARCANE_MISSILES, 18);
    }

    // Adjust for mana levels
    float manaPct = GetManaPercent();
    if (manaPct < 0.3f)
    {
        // Low mana - prioritize efficient spells
        SetSpellPriority(FIRE_BLAST, 15);
        SetSpellPriority(ICE_LANCE, 14);
        SetSpellPriority(SCORCH, 13);
    }
}

// Helper method implementations

bool MageAI::HasManaGem()
{
    // Check if player has mana gems in inventory
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* item = GetBot()->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (item && IsManaGem(item->GetEntry()))
            return true;
    }
    return false;
}

bool MageAI::IsManaGem(uint32 itemId)
{
    // List of mana gem item IDs
    std::vector<uint32> manaGems = {
        5514, // Mana Agate
        5513, // Mana Jade
        8007, // Mana Citrine
        8008, // Mana Ruby
        22044, // Mana Emerald
        33312  // Mana Sapphire
    };

    return std::find(manaGems.begin(), manaGems.end(), itemId) != manaGems.end();
}

void MageAI::UseManaGem()
{
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* item = GetBot()->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (item && IsManaGem(item->GetEntry()))
        {
            GetBot()->UseItem(INVENTORY_SLOT_BAG_0, i, true);
            TC_LOG_DEBUG("playerbot.mage", "Used mana gem: {}", item->GetEntry());
            break;
        }
    }
}

bool MageAI::HasClearcastingProc()
{
    return GetBot()->HasAura(CLEARCASTING);
}

void MageAI::EnterBurnPhase()
{
    _inBurnPhase = true;
    _inConservePhase = false;
    _burnPhaseStartTime = getMSTime();

    TC_LOG_DEBUG("playerbot.mage", "{} entering burn phase", GetBot()->GetName());
}

void MageAI::EnterConservePhase()
{
    _inBurnPhase = false;
    _inConservePhase = true;
    _conservePhaseStartTime = getMSTime();

    TC_LOG_DEBUG("playerbot.mage", "{} entering conserve phase", GetBot()->GetName());
}

uint32 MageAI::GetArcaneCharges()
{
    Aura* aura = GetBot()->GetAura(ARCANE_CHARGES);
    return aura ? aura->GetStackAmount() : 0;
}

void MageAI::SetSpellPriority(uint32 spellId, uint32 priority)
{
    // Implementation would depend on the action priority system
    // This is a placeholder for the priority setting mechanism
    TC_LOG_DEBUG("playerbot.mage", "Set spell {} priority to {}", spellId, priority);
}

bool MageAI::IsOnCooldown(uint32 spellId)
{
    return GetBot()->HasSpellCooldown(spellId);
}

// Constants for enhanced functionality
static constexpr uint32 CLEARCASTING = 12536;
static constexpr uint32 ARCANE_CHARGES = 36032;
static constexpr float BURN_PHASE_MANA_THRESHOLD = 0.8f;
static constexpr float CONSERVE_PHASE_MANA_THRESHOLD = 0.4f;

} // namespace Playerbot