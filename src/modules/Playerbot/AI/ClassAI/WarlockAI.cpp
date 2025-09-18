/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "WarlockAI.h"
#include "ActionPriority.h"
#include "CooldownManager.h"
#include "ResourceManager.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Map.h"
#include "Pet.h"
#include "Log.h"

namespace Playerbot
{

WarlockAI::WarlockAI(Player* bot) : ClassAI(bot), _currentPet(WarlockPet::NONE),
    _petUnit(nullptr), _petBehavior(PetBehavior::DEFENSIVE), _manaSpent(0), _damageDealt(0),
    _dotDamage(0), _petDamage(0), _lastDoTCheck(0), _dotRefreshTimer(0), _maxDoTTargets(MAX_DOT_TARGETS),
    _lastPetCommand(0), _petHealthPercent(100), _lastPetHeal(0), _petNeedsHealing(false),
    _petIsActive(false), _lastSoulShardUse(0), _needsSoulShards(false), _corruptionTargets(0),
    _curseOfAgonyTargets(0), _unstableAfflictionStacks(0), _lastDrainLife(0),
    _demonicEmpowermentStacks(0), _lastMetamorphosis(0), _felguardCommands(0), _demonFormActive(false),
    _shadowBurnCharges(0), _backdraftStacks(0), _conflagrateCharges(0), _lastImmolate(0),
    _lastFear(0), _lastBanish(0), _lastShadowflame(0), _lastRainOfFire(0)
{
    _specialization = DetectSpecialization();
    _soulShards.count = 0;
    _soulShards.maxCount = 32;
    _soulShards.conserveMode = true;

    TC_LOG_DEBUG("playerbot.warlock", "WarlockAI initialized for {} with specialization {}",
                 GetBot()->GetName(), static_cast<uint32>(_specialization));
}

void WarlockAI::UpdateRotation(::Unit* target)
{
    if (!target)
        return;

    // Priority 1: Pet management
    UpdatePetManagement();

    // Priority 2: DoT management
    UpdateDoTManagement();

    // Priority 3: Specialization rotation
    switch (_specialization)
    {
        case WarlockSpec::AFFLICTION:
            UpdateAfflictionRotation(target);
            break;
        case WarlockSpec::DEMONOLOGY:
            UpdateDemonologyRotation(target);
            break;
        case WarlockSpec::DESTRUCTION:
            UpdateDestructionRotation(target);
            break;
    }

    // Priority 4: Curse management
    UpdateCurseManagement();

    // Priority 5: AoE opportunities
    std::vector<::Unit*> nearbyEnemies = GetNearbyEnemies(15.0f);
    if (nearbyEnemies.size() > 2)
    {
        UseAoEAbilities(nearbyEnemies);
    }
}

void WarlockAI::UpdateBuffs()
{
    UpdateWarlockBuffs();
}

void WarlockAI::UpdateCooldowns(uint32 diff)
{
    // Emergency responses
    if (IsInCriticalDanger())
    {
        HandleEmergencySituation();
    }
    else if (IsInDanger())
    {
        UseDefensiveAbilities();
    }

    // Soul shard management
    UpdateSoulShardManagement();

    // Pet health monitoring
    ManagePetHealth();

    // Specialization-specific cooldowns
    switch (_specialization)
    {
        case WarlockSpec::AFFLICTION:
            // Dark Ritual for mana if needed
            if (GetManaPercent() < 0.3f && CanUseAbility(DARK_RITUAL))
            {
                _actionQueue->AddAction(DARK_RITUAL, ActionPriority::SURVIVAL, 80.0f);
            }
            break;
        case WarlockSpec::DEMONOLOGY:
            ManageDemonForm();
            if (CanUseAbility(DEMONIC_EMPOWERMENT))
            {
                _actionQueue->AddAction(DEMONIC_EMPOWERMENT, ActionPriority::BURST, 75.0f);
            }
            break;
        case WarlockSpec::DESTRUCTION:
            // Use Shadow Burn charges efficiently
            if (_shadowBurnCharges > 0 && _currentTarget && _currentTarget->GetHealthPct() < 20.0f)
            {
                _actionQueue->AddAction(SHADOW_BURN, ActionPriority::BURST, 90.0f, _currentTarget);
            }
            break;
    }

    // Mana management through Life Tap
    if (GetManaPercent() < 0.4f && GetBot()->GetHealthPct() > 60.0f)
    {
        CastLifeTap();
    }
}

bool WarlockAI::CanUseAbility(uint32 spellId)
{
    if (!IsSpellReady(spellId) || !IsSpellUsable(spellId))
        return false;

    if (!HasEnoughResource(spellId))
        return false;

    // Check soul shard requirements
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (spellInfo && spellInfo->GetReagentCount() > 0)
    {
        // Check if spell requires soul shards
        for (uint32 i = 0; i < MAX_SPELL_REAGENTS; ++i)
        {
            if (spellInfo->Reagent[i] == 6265) // Soul Shard item ID
            {
                if (!HasSoulShardsAvailable(spellInfo->ReagentCount[i]))
                    return false;
            }
        }
    }

    // Check if we're channeling and spell would interrupt
    if (IsChanneling() && !IsSpellInstant(spellId))
        return false;

    return true;
}

void WarlockAI::OnCombatStart(::Unit* target)
{
    ClassAI::OnCombatStart(target);

    // Reset combat variables
    _manaSpent = 0;
    _damageDealt = 0;
    _dotDamage = 0;
    _petDamage = 0;
    _corruptionTargets = 0;
    _curseOfAgonyTargets = 0;

    // Summon pet if not present
    if (!IsPetAlive())
    {
        SummonOptimalPet();
    }

    // Set pet to aggressive for combat
    if (_petUnit)
    {
        PetAggressive();
        PetAttackTarget(target);
    }

    TC_LOG_DEBUG("playerbot.warlock", "Warlock {} entering combat - Spec: {}, Pet: {}, Soul Shards: {}",
                 GetBot()->GetName(), static_cast<uint32>(_specialization),
                 static_cast<uint32>(_currentPet), _soulShards.count);
}

void WarlockAI::OnCombatEnd()
{
    ClassAI::OnCombatEnd();

    // Analyze combat effectiveness
    AnalyzeDamageEffectiveness();

    // Reset pet to defensive
    if (_petUnit)
    {
        PetDefensive();
    }

    // Life Tap to restore mana if healthy
    if (GetManaPercent() < 0.7f && GetBot()->GetHealthPct() > 80.0f)
    {
        CastLifeTap();
    }

    // Clean up expired DoTs
    RemoveExpiredDoTs();
}

bool WarlockAI::HasEnoughResource(uint32 spellId)
{
    return _resourceManager->HasEnoughResource(spellId);
}

void WarlockAI::ConsumeResource(uint32 spellId)
{
    uint32 manaCost = 0;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (spellInfo && spellInfo->PowerType == POWER_MANA)
    {
        manaCost = spellInfo->ManaCost + spellInfo->ManaCostPercentage * GetMaxMana() / 100;
    }

    _resourceManager->ConsumeResource(spellId);
    _manaSpent += manaCost;

    // Consume soul shards if required
    if (spellInfo)
    {
        for (uint32 i = 0; i < MAX_SPELL_REAGENTS; ++i)
        {
            if (spellInfo->Reagent[i] == 6265) // Soul Shard
            {
                UseSoulShard(spellId);
                break;
            }
        }
    }
}

Position WarlockAI::GetOptimalPosition(::Unit* target)
{
    if (!target)
        return GetBot()->GetPosition();

    // Warlocks want to stay at optimal casting range
    float distance = GetOptimalRange(target);
    float angle = GetBot()->GetAngle(target);

    // Stay behind cover and away from melee
    Position pos;
    target->GetNearPosition(pos, distance, angle + M_PI);
    return pos;
}

float WarlockAI::GetOptimalRange(::Unit* target)
{
    return OPTIMAL_CASTING_RANGE;
}

void WarlockAI::UpdateAfflictionRotation(::Unit* target)
{
    if (!target)
        return;

    // Affliction priority: DoTs first, then fillers
    // 1. Unstable Affliction if available
    if (!target->HasAura(UNSTABLE_AFFLICTION) && CanUseAbility(UNSTABLE_AFFLICTION))
    {
        _actionQueue->AddAction(UNSTABLE_AFFLICTION, ActionPriority::ROTATION, 95.0f, target);
        return;
    }

    // 2. Corruption if not up
    if (!target->HasAura(CORRUPTION) && CanUseAbility(CORRUPTION))
    {
        _actionQueue->AddAction(CORRUPTION, ActionPriority::ROTATION, 90.0f, target);
        return;
    }

    // 3. Curse of Agony if not up
    if (!target->HasAura(CURSE_OF_AGONY) && CanUseAbility(CURSE_OF_AGONY))
    {
        _actionQueue->AddAction(CURSE_OF_AGONY, ActionPriority::ROTATION, 85.0f, target);
        return;
    }

    // 4. Drain Soul if target is low health
    if (target->GetHealthPct() < 25.0f && CanUseAbility(DRAIN_SOUL))
    {
        _actionQueue->AddAction(DRAIN_SOUL, ActionPriority::BURST, 80.0f, target);
        return;
    }

    // 5. Drain Life if we need health
    if (GetBot()->GetHealthPct() < 50.0f && CanUseAbility(DRAIN_LIFE))
    {
        _actionQueue->AddAction(DRAIN_LIFE, ActionPriority::SURVIVAL, 75.0f, target);
        return;
    }

    // 6. Shadow Bolt as filler
    if (CanUseAbility(SHADOW_BOLT))
    {
        _actionQueue->AddAction(SHADOW_BOLT, ActionPriority::ROTATION, 60.0f, target);
    }
}

void WarlockAI::UpdateDemonologyRotation(::Unit* target)
{
    if (!target)
        return;

    // Ensure proper pet is summoned
    if (_currentPet != WarlockPet::FELGUARD && CanUseAbility(SUMMON_FELGUARD))
    {
        _actionQueue->AddAction(SUMMON_FELGUARD, ActionPriority::BUFF, 100.0f);
        return;
    }

    // Metamorphosis if available
    if (!_demonFormActive && CanUseAbility(METAMORPHOSIS))
    {
        _actionQueue->AddAction(METAMORPHOSIS, ActionPriority::BURST, 95.0f);
        return;
    }

    // Corruption for Soul Burn synergy
    if (!target->HasAura(CORRUPTION) && CanUseAbility(CORRUPTION))
    {
        _actionQueue->AddAction(CORRUPTION, ActionPriority::ROTATION, 85.0f, target);
        return;
    }

    // Soul Burn if available
    if (CanUseAbility(SOUL_BURN))
    {
        _actionQueue->AddAction(SOUL_BURN, ActionPriority::ROTATION, 80.0f, target);
        return;
    }

    // Shadow Bolt as primary nuke
    if (CanUseAbility(SHADOW_BOLT))
    {
        _actionQueue->AddAction(SHADOW_BOLT, ActionPriority::ROTATION, 70.0f, target);
    }
}

void WarlockAI::UpdateDestructionRotation(::Unit* target)
{
    if (!target)
        return;

    // Destruction priority: Immolate -> direct damage
    // 1. Immolate if not up
    if (!target->HasAura(IMMOLATE) && CanUseAbility(IMMOLATE))
    {
        _actionQueue->AddAction(IMMOLATE, ActionPriority::ROTATION, 90.0f, target);
        return;
    }

    // 2. Conflagrate if Immolate is up
    if (target->HasAura(IMMOLATE) && CanUseAbility(CONFLAGRATE))
    {
        _actionQueue->AddAction(CONFLAGRATE, ActionPriority::BURST, 85.0f, target);
        return;
    }

    // 3. Shadow Burn in execute range
    if (target->GetHealthPct() < 20.0f && CanUseAbility(SHADOW_BURN))
    {
        _actionQueue->AddAction(SHADOW_BURN, ActionPriority::BURST, 95.0f, target);
        return;
    }

    // 4. Chaos Bolt if available
    if (CanUseAbility(CHAOS_BOLT))
    {
        _actionQueue->AddAction(CHAOS_BOLT, ActionPriority::BURST, 80.0f, target);
        return;
    }

    // 5. Incinerate as primary nuke
    if (CanUseAbility(INCINERATE))
    {
        _actionQueue->AddAction(INCINERATE, ActionPriority::ROTATION, 75.0f, target);
    }
    // 6. Shadow Bolt fallback
    else if (CanUseAbility(SHADOW_BOLT))
    {
        _actionQueue->AddAction(SHADOW_BOLT, ActionPriority::ROTATION, 65.0f, target);
    }
}

void WarlockAI::UpdatePetManagement()
{
    // Check if we need to summon a pet
    if (!IsPetAlive())
    {
        if (!_inCombat) // Don't summon during combat unless emergency
        {
            SummonOptimalPet();
        }
        return;
    }

    // Update pet behavior based on situation
    ManagePetBehavior();

    // Position pet optimally
    PositionPet();

    // Command pet abilities
    if (_currentTarget && _petUnit)
    {
        PetAttackTarget(_currentTarget);

        // Use pet-specific abilities
        switch (_currentPet)
        {
            case WarlockPet::VOIDWALKER:
                // Taunt if we're taking too much damage
                if (GetBot()->GetHealthPct() < 40.0f)
                {
                    PetUseAbility(17735); // Suffering (Taunt)
                }
                break;
            case WarlockPet::SUCCUBUS:
                // Seduce a secondary target if available
                if (GetEnemyCount() > 1)
                {
                    ::Unit* ccTarget = GetBestFearTarget();
                    if (ccTarget && ccTarget != _currentTarget)
                    {
                        PetUseAbility(6358, ccTarget); // Seduction
                    }
                }
                break;
            case WarlockPet::FELHUNTER:
                // Devour Magic on targets with buffs
                if (_currentTarget->GetTotalAuraList().size() > 0)
                {
                    PetUseAbility(19505, _currentTarget); // Devour Magic
                }
                break;
            case WarlockPet::FELGUARD:
                // Cleave if multiple enemies
                if (GetEnemyCount(8.0f) > 1)
                {
                    PetUseAbility(30213); // Legion Strike (Cleave)
                }
                break;
        }
    }
}

void WarlockAI::SummonOptimalPet()
{
    WarlockPet optimalPet = GetOptimalPetForSituation();
    uint32 summonSpell = 0;

    switch (optimalPet)
    {
        case WarlockPet::IMP:
            summonSpell = SUMMON_IMP;
            break;
        case WarlockPet::VOIDWALKER:
            summonSpell = SUMMON_VOIDWALKER;
            break;
        case WarlockPet::SUCCUBUS:
            summonSpell = SUMMON_SUCCUBUS;
            break;
        case WarlockPet::FELHUNTER:
            summonSpell = SUMMON_FELHUNTER;
            break;
        case WarlockPet::FELGUARD:
            summonSpell = SUMMON_FELGUARD;
            break;
    }

    if (summonSpell && CanUseAbility(summonSpell))
    {
        _actionQueue->AddAction(summonSpell, ActionPriority::BUFF, 100.0f);
        _currentPet = optimalPet;

        TC_LOG_DEBUG("playerbot.warlock", "Summoning pet: {}", static_cast<uint32>(optimalPet));
    }
}

WarlockPet WarlockAI::GetOptimalPetForSituation()
{
    // Choose pet based on specialization and situation
    switch (_specialization)
    {
        case WarlockSpec::AFFLICTION:
            // Felhunter for silence and dispel
            return WarlockPet::FELHUNTER;
        case WarlockSpec::DEMONOLOGY:
            // Felguard if available, otherwise Voidwalker
            return WarlockPet::FELGUARD;
        case WarlockSpec::DESTRUCTION:
            // Imp for DPS or Succubus for CC
            if (GetBot()->GetGroup())
                return WarlockPet::SUCCUBUS;
            else
                return WarlockPet::IMP;
    }

    // Default to Voidwalker for survivability
    return WarlockPet::VOIDWALKER;
}

void WarlockAI::CommandPet(uint32 action, ::Unit* target)
{
    if (!_petUnit)
        return;

    uint32 currentTime = getMSTime();
    if (currentTime - _lastPetCommand < 1000) // 1 second cooldown
        return;

    // This would send pet commands through TrinityCore's pet system
    _lastPetCommand = currentTime;
}

void WarlockAI::ManagePetHealth()
{
    if (!_petUnit)
        return;

    uint32 currentTime = getMSTime();
    if (currentTime - _lastPetHeal < PET_HEALTH_CHECK_INTERVAL)
        return;

    _petHealthPercent = _petUnit->GetHealthPct();

    // Heal pet if needed
    if (_petHealthPercent < 50.0f)
    {
        // Use Health Funnel if available
        if (CanUseAbility(755)) // Health Funnel
        {
            _actionQueue->AddAction(755, ActionPriority::SURVIVAL, 80.0f, _petUnit);
        }
        // Use healthstones
        else if (CanUseAbility(CREATE_HEALTHSTONE))
        {
            _actionQueue->AddAction(CREATE_HEALTHSTONE, ActionPriority::SURVIVAL, 70.0f);
        }
    }

    _lastPetHeal = currentTime;
}

void WarlockAI::ManagePetBehavior()
{
    if (!_petUnit)
        return;

    PetBehavior optimalBehavior = PetBehavior::DEFENSIVE;

    if (_inCombat)
    {
        if (GetBot()->GetHealthPct() < 30.0f)
            optimalBehavior = PetBehavior::AGGRESSIVE; // Pet tanks
        else
            optimalBehavior = PetBehavior::DEFENSIVE;
    }
    else
    {
        optimalBehavior = PetBehavior::PASSIVE;
    }

    if (_petBehavior != optimalBehavior)
    {
        switch (optimalBehavior)
        {
            case PetBehavior::PASSIVE:
                PetPassive();
                break;
            case PetBehavior::DEFENSIVE:
                PetDefensive();
                break;
            case PetBehavior::AGGRESSIVE:
                PetAggressive();
                break;
        }
        _petBehavior = optimalBehavior;
    }
}

void WarlockAI::PositionPet()
{
    if (!_petUnit || !_currentTarget)
        return;

    // Position pet based on type
    switch (_currentPet)
    {
        case WarlockPet::VOIDWALKER:
        case WarlockPet::FELGUARD:
            // Melee pets: position between warlock and target
            {
                Position petPos;
                _currentTarget->GetNearPosition(petPos, 3.0f, _currentTarget->GetAngle(GetBot()));
                PetMoveToPosition(petPos);
            }
            break;
        case WarlockPet::IMP:
        case WarlockPet::SUCCUBUS:
        case WarlockPet::FELHUNTER:
            // Ranged pets: keep at medium distance
            {
                Position petPos;
                _currentTarget->GetNearPosition(petPos, 15.0f, _currentTarget->GetAngle(GetBot()));
                PetMoveToPosition(petPos);
            }
            break;
    }
}

void WarlockAI::PetAttackTarget(::Unit* target)
{
    if (!_petUnit || !target)
        return;

    // Send pet attack command
    CommandPet(PET_ATTACK, target);
}

void WarlockAI::PetMoveToPosition(const Position& pos)
{
    if (!_petUnit)
        return;

    // Move pet to position
    _lastPetPosition = pos;
}

void WarlockAI::PetUseAbility(uint32 spellId, ::Unit* target)
{
    if (!_petUnit || !spellId)
        return;

    // Command pet to use ability
    CommandPet(spellId, target);
}

void WarlockAI::PetFollow()
{
    CommandPet(PET_FOLLOW);
}

void WarlockAI::PetStay()
{
    CommandPet(PET_STAY);
}

void WarlockAI::PetPassive()
{
    CommandPet(PET_PASSIVE);
}

void WarlockAI::PetDefensive()
{
    CommandPet(PET_DEFENSIVE);
}

void WarlockAI::PetAggressive()
{
    CommandPet(PET_AGGRESSIVE);
}

void WarlockAI::UpdateDoTManagement()
{
    uint32 currentTime = getMSTime();

    // Check DoTs periodically
    if (currentTime - _lastDoTCheck < DOT_CHECK_INTERVAL)
        return;

    // Refresh expiring DoTs
    RefreshExpiringDoTs();

    // Spread DoTs to new targets
    if (_corruptionTargets < _maxDoTTargets)
    {
        SpreadDoTs();
    }

    // Clean up expired DoT tracking
    RemoveExpiredDoTs();

    _lastDoTCheck = currentTime;
}

void WarlockAI::ApplyDoTsToTarget(::Unit* target)
{
    if (!target)
        return;

    // Apply based on specialization priority
    switch (_specialization)
    {
        case WarlockSpec::AFFLICTION:
            CastCorruption(target);
            CastUnstableAffliction(target);
            ApplyCurses(target);
            break;
        case WarlockSpec::DESTRUCTION:
            CastImmolate(target);
            break;
        case WarlockSpec::DEMONOLOGY:
            CastCorruption(target);
            break;
    }
}

void WarlockAI::RefreshExpiringDoTs()
{
    for (auto& targetDoTs : _activeDoTs)
    {
        ObjectGuid targetGuid = targetDoTs.first;
        ::Unit* target = ObjectAccessor::GetUnit(*GetBot(), targetGuid);

        if (!target || !target->IsAlive())
            continue;

        for (auto& dot : targetDoTs.second)
        {
            if (dot.remainingTime < 6000 && dot.remainingTime > 0) // 6 seconds left
            {
                if (ShouldApplyDoT(target, dot.spellId))
                {
                    _actionQueue->AddAction(dot.spellId, ActionPriority::ROTATION, 80.0f, target);
                }
            }
        }
    }
}

void WarlockAI::SpreadDoTs()
{
    std::vector<::Unit*> dotTargets = GetDoTTargets();

    for (::Unit* target : dotTargets)
    {
        if (_corruptionTargets >= _maxDoTTargets)
            break;

        if (!IsDoTActive(target, CORRUPTION))
        {
            CastCorruption(target);
            break; // One per update cycle
        }
    }
}

bool WarlockAI::ShouldApplyDoT(::Unit* target, uint32 spellId)
{
    if (!target)
        return false;

    // Check if target will live long enough
    if (target->GetHealthPct() < 25.0f && spellId != DRAIN_SOUL)
        return false;

    // Check if DoT is already active
    if (IsDoTActive(target, spellId))
        return false;

    return true;
}

bool WarlockAI::IsDoTActive(::Unit* target, uint32 spellId)
{
    return target ? target->HasAura(spellId) : false;
}

std::vector<::Unit*> WarlockAI::GetDoTTargets(uint32 maxTargets)
{
    std::vector<::Unit*> enemies = GetNearbyEnemies(40.0f);

    // Limit to maxTargets
    if (enemies.size() > maxTargets)
        enemies.resize(maxTargets);

    return enemies;
}

void WarlockAI::RemoveExpiredDoTs()
{
    auto it = _activeDoTs.begin();
    while (it != _activeDoTs.end())
    {
        ::Unit* target = ObjectAccessor::GetUnit(*GetBot(), it->first);
        if (!target || !target->IsAlive())
        {
            it = _activeDoTs.erase(it);
        }
        else
        {
            // Remove expired DoTs from vector
            auto& dots = it->second;
            dots.erase(std::remove_if(dots.begin(), dots.end(),
                [](const DoTInfo& dot) { return dot.remainingTime == 0; }), dots.end());

            if (dots.empty())
                it = _activeDoTs.erase(it);
            else
                ++it;
        }
    }
}

void WarlockAI::UpdateSoulShardManagement()
{
    // Count current soul shards
    _soulShards.count = GetBot()->GetItemCount(6265); // Soul Shard item ID

    // Determine if we should conserve
    _soulShards.conserveMode = _soulShards.count < SOUL_SHARD_CONSERVATION_THRESHOLD;

    // Create soul shards if we're low
    if (_soulShards.count < 5 && !_inCombat)
    {
        _needsSoulShards = true;
    }
}

void WarlockAI::UseSoulShard(uint32 spellId)
{
    if (_soulShards.count > 0)
    {
        _soulShards.count--;
        _lastSoulShardUse = getMSTime();

        TC_LOG_DEBUG("playerbot.warlock", "Used soul shard for spell {}, {} remaining", spellId, _soulShards.count);
    }
}

bool WarlockAI::HasSoulShardsAvailable(uint32 required)
{
    return _soulShards.count >= required;
}

void WarlockAI::UpdateCurseManagement()
{
    if (!_currentTarget)
        return;

    // Apply optimal curse to current target
    uint32 optimalCurse = GetOptimalCurseForTarget(_currentTarget);
    if (optimalCurse && !_currentTarget->HasAura(optimalCurse))
    {
        _actionQueue->AddAction(optimalCurse, ActionPriority::ROTATION, 70.0f, _currentTarget);
    }
}

void WarlockAI::ApplyCurses(::Unit* target)
{
    if (!target)
        return;

    uint32 curse = GetOptimalCurseForTarget(target);
    if (curse && !target->HasAura(curse))
    {
        _actionQueue->AddAction(curse, ActionPriority::ROTATION, 65.0f, target);
    }
}

uint32 WarlockAI::GetOptimalCurseForTarget(::Unit* target)
{
    if (!target)
        return 0;

    // Choose curse based on target type and situation
    if (target->GetPowerType() == POWER_MANA)
    {
        return CURSE_OF_TONGUES; // Slow casting
    }
    else if (_specialization == WarlockSpec::AFFLICTION)
    {
        return CURSE_OF_AGONY; // DoT damage
    }
    else
    {
        return CURSE_OF_ELEMENTS; // Increased spell damage
    }
}

void WarlockAI::UpdateWarlockBuffs()
{
    // Armor spells
    UpdateArmorSpells();

    // Soul Link if Demonology
    if (_specialization == WarlockSpec::DEMONOLOGY && !HasAura(SOUL_LINK))
    {
        if (CanUseAbility(SOUL_LINK))
        {
            CastSpell(SOUL_LINK);
        }
    }

    // Amplify Curse if available
    if (!HasAura(AMPLIFY_CURSE) && CanUseAbility(AMPLIFY_CURSE))
    {
        CastSpell(AMPLIFY_CURSE);
    }
}

void WarlockAI::UpdateArmorSpells()
{
    // Choose armor based on specialization
    switch (_specialization)
    {
        case WarlockSpec::AFFLICTION:
        case WarlockSpec::DEMONOLOGY:
            if (!HasAura(DEMON_ARMOR) && !HasAura(FEL_ARMOR))
            {
                if (CanUseAbility(DEMON_ARMOR))
                    CastSpell(DEMON_ARMOR);
                else if (CanUseAbility(DEMON_SKIN))
                    CastSpell(DEMON_SKIN);
            }
            break;
        case WarlockSpec::DESTRUCTION:
            if (!HasAura(FEL_ARMOR) && !HasAura(DEMON_ARMOR))
            {
                if (CanUseAbility(FEL_ARMOR))
                    CastSpell(FEL_ARMOR);
                else if (CanUseAbility(DEMON_ARMOR))
                    CastSpell(DEMON_ARMOR);
            }
            break;
    }
}

void WarlockAI::UseDefensiveAbilities()
{
    // Fear nearby enemies
    if (GetEnemyCount(8.0f) > 0)
    {
        ::Unit* fearTarget = GetBestFearTarget();
        if (fearTarget)
        {
            UseFear(fearTarget);
        }
    }

    // Drain Life if we need health
    if (GetBot()->GetHealthPct() < 40.0f && _currentTarget)
    {
        CastDrainLife(_currentTarget);
    }

    // Death Coil for emergency healing
    if (GetBot()->GetHealthPct() < 25.0f && CanUseAbility(DEATH_COIL))
    {
        _actionQueue->AddAction(DEATH_COIL, ActionPriority::EMERGENCY, 90.0f, _currentTarget);
    }

    // Howl of Terror for AoE fear
    if (GetEnemyCount(10.0f) > 1 && CanUseAbility(HOWL_OF_TERROR))
    {
        _actionQueue->AddAction(HOWL_OF_TERROR, ActionPriority::SURVIVAL, 80.0f);
    }
}

void WarlockAI::CastDrainLife(::Unit* target)
{
    if (!target || !CanUseAbility(DRAIN_LIFE))
        return;

    uint32 currentTime = getMSTime();
    if (currentTime - _lastDrainLife > 8000) // 8 second channel
    {
        _actionQueue->AddAction(DRAIN_LIFE, ActionPriority::SURVIVAL, 85.0f, target);
        _lastDrainLife = currentTime;
    }
}

void WarlockAI::UseFear(::Unit* target)
{
    if (!target || !CanUseAbility(FEAR))
        return;

    uint32 currentTime = getMSTime();
    if (currentTime - _lastFear > FEAR_COOLDOWN)
    {
        _actionQueue->AddAction(FEAR, ActionPriority::SURVIVAL, 80.0f, target);
        _lastFear = currentTime;
        _fearTargets[target->GetGUID()] = currentTime;
    }
}

void WarlockAI::UseBanish(::Unit* target)
{
    if (!target || !CanUseAbility(BANISH))
        return;

    uint32 currentTime = getMSTime();
    if (currentTime - _lastBanish > BANISH_COOLDOWN)
    {
        _actionQueue->AddAction(BANISH, ActionPriority::INTERRUPT, 75.0f, target);
        _lastBanish = currentTime;
        _banishTargets[target->GetGUID()] = currentTime;
    }
}

void WarlockAI::UseAoEAbilities(const std::vector<::Unit*>& enemies)
{
    uint32 enemyCount = static_cast<uint32>(enemies.size());

    if (enemyCount < 3)
        return;

    // Rain of Fire for stationary AoE
    if (CanUseAbility(RAIN_OF_FIRE))
    {
        uint32 currentTime = getMSTime();
        if (currentTime - _lastRainOfFire > 8000) // 8 second channel
        {
            float score = 60.0f + (enemyCount * 10.0f);
            _actionQueue->AddAction(RAIN_OF_FIRE, ActionPriority::ROTATION, score, enemies[0]);
            _lastRainOfFire = currentTime;
        }
    }

    // Seed of Corruption for spreading DoTs
    if (_specialization == WarlockSpec::AFFLICTION && CanUseAbility(SEED_OF_CORRUPTION))
    {
        ::Unit* centerTarget = enemies[0];
        _actionQueue->AddAction(SEED_OF_CORRUPTION, ActionPriority::ROTATION, 70.0f, centerTarget);
    }

    // Hellfire if we're tanky enough
    if (GetBot()->GetHealthPct() > 60.0f && CanUseAbility(HELLFIRE))
    {
        float score = 50.0f + (enemyCount * 8.0f);
        _actionQueue->AddAction(HELLFIRE, ActionPriority::ROTATION, score);
    }
}

void WarlockAI::CastCorruption(::Unit* target)
{
    if (!target || target->HasAura(CORRUPTION) || !CanUseAbility(CORRUPTION))
        return;

    if (ShouldApplyDoT(target, CORRUPTION))
    {
        _actionQueue->AddAction(CORRUPTION, ActionPriority::ROTATION, 85.0f, target);
        _corruptionTargets++;
    }
}

void WarlockAI::CastImmolate(::Unit* target)
{
    if (!target || target->HasAura(IMMOLATE) || !CanUseAbility(IMMOLATE))
        return;

    _actionQueue->AddAction(IMMOLATE, ActionPriority::ROTATION, 80.0f, target);
}

uint32 WarlockAI::GetMana()
{
    return _resourceManager->GetResource(ResourceType::MANA);
}

uint32 WarlockAI::GetMaxMana()
{
    return _resourceManager->GetMaxResource(ResourceType::MANA);
}

float WarlockAI::GetManaPercent()
{
    return _resourceManager->GetResourcePercent(ResourceType::MANA);
}

void WarlockAI::CastLifeTap()
{
    if (GetBot()->GetHealthPct() > 30.0f && CanUseAbility(LIFE_TAP))
    {
        _actionQueue->AddAction(LIFE_TAP, ActionPriority::SURVIVAL, 70.0f);
    }
}

bool WarlockAI::IsPetAlive()
{
    return _petUnit && _petUnit->IsAlive();
}

bool WarlockAI::IsInDanger()
{
    float healthPct = GetBot()->GetHealthPct();
    uint32 nearbyEnemies = GetEnemyCount(10.0f);

    return healthPct < 50.0f || nearbyEnemies > 2 || IsInMeleeRange(_currentTarget);
}

bool WarlockAI::IsInCriticalDanger()
{
    float healthPct = GetBot()->GetHealthPct();
    uint32 nearbyEnemies = GetEnemyCount(5.0f);

    return healthPct < 25.0f || nearbyEnemies > 3;
}

void WarlockAI::HandleEmergencySituation()
{
    // Death Coil for immediate health
    if (CanUseAbility(DEATH_COIL))
    {
        _actionQueue->AddAction(DEATH_COIL, ActionPriority::EMERGENCY, 100.0f, _currentTarget);
    }

    // Fear everything nearby
    if (CanUseAbility(HOWL_OF_TERROR))
    {
        _actionQueue->AddAction(HOWL_OF_TERROR, ActionPriority::EMERGENCY, 95.0f);
    }

    // Pet emergency taunt
    if (_petUnit && _currentPet == WarlockPet::VOIDWALKER)
    {
        PetUseAbility(17735); // Suffering
    }
}

::Unit* WarlockAI::GetBestFearTarget()
{
    std::vector<::Unit*> enemies = GetNearbyEnemies(30.0f);

    for (::Unit* enemy : enemies)
    {
        if (enemy == _currentTarget)
            continue;

        // Check if target can be feared
        if (enemy->GetCreatureType() == CREATURE_TYPE_HUMANOID ||
            enemy->GetCreatureType() == CREATURE_TYPE_BEAST)
        {
            auto it = _fearTargets.find(enemy->GetGUID());
            if (it == _fearTargets.end() || getMSTime() - it->second > 60000) // 1 minute DR
            {
                return enemy;
            }
        }
    }

    return nullptr;
}

WarlockSpec WarlockAI::DetectSpecialization()
{
    // This would detect the warlock's specialization based on talents
    // For now, return a default
    return WarlockSpec::AFFLICTION; // Default to Affliction for DoT focus
}

void WarlockAI::RecordDamageDealt(uint32 damage, ::Unit* target)
{
    _damageDealt += damage;
    RecordPerformanceMetric("damage_dealt", damage);
}

void WarlockAI::AnalyzeDamageEffectiveness()
{
    if (_manaSpent > 0)
    {
        float efficiency = static_cast<float>(_damageDealt) / _manaSpent;
        TC_LOG_DEBUG("playerbot.warlock", "Combat efficiency: {} damage per mana", efficiency);
    }

    TC_LOG_DEBUG("playerbot.warlock", "Combat summary - Total: {}, DoT: {}, Pet: {}",
                 _damageDealt, _dotDamage, _petDamage);
}

// Placeholder implementations for complex methods
void WarlockAI::OptimizeManaUsage() {}
void WarlockAI::UseManaRegeneration() { CastLifeTap(); }
void WarlockAI::UpdateWarlockPositioning() {}
bool WarlockAI::IsAtOptimalRange(::Unit* target) { return GetBot()->GetDistance(target) <= OPTIMAL_CASTING_RANGE; }
bool WarlockAI::NeedsToKite(::Unit* target) { return IsInMeleeRange(target); }
void WarlockAI::PerformKiting(::Unit* target) {}
void WarlockAI::MaintainPetPosition() { PositionPet(); }
void WarlockAI::ManageThreat() {}
bool WarlockAI::HasTooMuchThreat() { return false; }
void WarlockAI::ReduceThreat() {}
void WarlockAI::UseSoulshatter() {}
::Unit* WarlockAI::GetBestCurseTarget() { return _currentTarget; }
::Unit* WarlockAI::GetBestBanishTarget() { return nullptr; }
::Unit* WarlockAI::GetBestDrainTarget() { return _currentTarget; }
::Unit* WarlockAI::GetHighestPriorityTarget() { return _currentTarget; }
void WarlockAI::UseEmergencyAbilities() { HandleEmergencySituation(); }
void WarlockAI::PetEmergencyTaunt() {}
void WarlockAI::OptimizeRotationEfficiency() {}
bool WarlockAI::IsPetInRange() { return _petUnit ? GetBot()->GetDistance(_petUnit) < PET_COMMAND_RANGE : false; }
uint32 WarlockAI::GetPetSpellId(WarlockPet petType, uint32 abilityIndex) { return 0; }
bool WarlockAI::IsChanneling() { return GetBot()->HasChannelInterruptFlag(CHANNEL_INTERRUPT_FLAG_INTERRUPT); }
bool WarlockAI::CanCastWithoutInterruption() { return !IsChanneling() && !IsMoving(); }
bool WarlockAI::ShouldResummonPet() { return !IsPetAlive(); }
void WarlockAI::TrackDoTDamage(::Unit* target, uint32 spellId) {}
uint32 WarlockAI::GetDoTRemainingTime(::Unit* target, uint32 spellId) { return target ? target->GetAuraRemainingTime(spellId) : 0; }
void WarlockAI::CreateSoulShard(::Unit* killedTarget) {}
void WarlockAI::ConserveSoulShards() { _soulShards.conserveMode = true; }
void WarlockAI::OptimizeSoulShardUsage() {}
void WarlockAI::CastCurseOfElements(::Unit* target) {}
void WarlockAI::CastCurseOfShadow(::Unit* target) {}
void WarlockAI::CastCurseOfTongues(::Unit* target) {}
void WarlockAI::CastCurseOfWeakness(::Unit* target) {}
void WarlockAI::CastCurseOfAgony(::Unit* target) {}
void WarlockAI::CastDemonSkin() {}
void WarlockAI::CastDemonArmor() {}
void WarlockAI::CastFelArmor() {}
void WarlockAI::CastSoulLink() {}
void WarlockAI::CastAmplifyCurse() {}
void WarlockAI::CastSiphonSoul(::Unit* target) {}
void WarlockAI::CastDeathCoil(::Unit* target) {}
void WarlockAI::CastHowlOfTerror() {}
void WarlockAI::CastRainOfFire(const std::vector<::Unit*>& enemies) {}
void WarlockAI::CastSeedOfCorruption(::Unit* target) {}
void WarlockAI::CastHellfire() {}
void WarlockAI::UseCrowdControl(::Unit* target) {}
void WarlockAI::UseAfflictionAbilities(::Unit* target) {}
void WarlockAI::CastUnstableAffliction(::Unit* target) {}
void WarlockAI::CastDrainSoul(::Unit* target) {}
void WarlockAI::CastDarkRitual() {}
void WarlockAI::UseDemonologyAbilities(::Unit* target) {}
void WarlockAI::CastDemonicEmpowerment() {}
void WarlockAI::CastMetamorphosis() {}
void WarlockAI::CastSoulBurn(::Unit* target) {}
void WarlockAI::CastSummonFelguard() {}
void WarlockAI::ManageDemonForm() {}
void WarlockAI::UseDestructionAbilities(::Unit* target) {}
void WarlockAI::CastIncinerate(::Unit* target) {}
void WarlockAI::CastConflagrate(::Unit* target) {}
void WarlockAI::CastShadowBurn(::Unit* target) {}
void WarlockAI::CastChaosLight(::Unit* target) {}
void WarlockAI::CastDrainMana(::Unit* target) {}
::Unit* WarlockAI::GetBestDoTTarget() { return _currentTarget; }
void WarlockAI::ApplyDoTsToMultipleTargets() {}
bool WarlockAI::HasEnoughMana(uint32 amount) { return GetMana() >= amount; }
bool WarlockAI::IsSpellInstant(uint32 spellId) { return GetSpellCastTime(spellId) == 0; }
uint32 WarlockAI::GetSpellCastTime(uint32 spellId) { SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE); return spellInfo ? spellInfo->CastTime : 0; }

// WarlockSpellCalculator implementation
std::unordered_map<uint32, uint32> WarlockSpellCalculator::_dotDamageCache;
std::unordered_map<WarlockPet, uint32> WarlockSpellCalculator::_petDamageCache;
std::mutex WarlockSpellCalculator::_cacheMutex;

uint32 WarlockSpellCalculator::CalculateShadowBoltDamage(Player* caster, ::Unit* target)
{
    return 1000; // Placeholder
}

uint32 WarlockSpellCalculator::CalculateCorruptionDamage(Player* caster, ::Unit* target)
{
    return 500; // Placeholder
}

uint32 WarlockSpellCalculator::CalculateImmolateDoTDamage(Player* caster, ::Unit* target)
{
    return 600; // Placeholder
}

float WarlockSpellCalculator::CalculateDoTEfficiency(uint32 spellId, Player* caster, ::Unit* target)
{
    return 2.0f; // Placeholder
}

bool WarlockSpellCalculator::IsDoTWorthCasting(uint32 spellId, Player* caster, ::Unit* target)
{
    return target ? target->GetHealthPct() > 30.0f : false;
}

uint32 WarlockSpellCalculator::GetOptimalDoTDuration(uint32 spellId, ::Unit* target)
{
    return 18000; // 18 seconds placeholder
}

uint32 WarlockSpellCalculator::CalculatePetDamage(WarlockPet petType, Player* caster)
{
    return 300; // Placeholder
}

WarlockPet WarlockSpellCalculator::GetOptimalPetForSpecialization(WarlockSpec spec)
{
    switch (spec)
    {
        case WarlockSpec::AFFLICTION: return WarlockPet::FELHUNTER;
        case WarlockSpec::DEMONOLOGY: return WarlockPet::FELGUARD;
        case WarlockSpec::DESTRUCTION: return WarlockPet::IMP;
        default: return WarlockPet::VOIDWALKER;
    }
}

uint32 WarlockSpellCalculator::GetPetSummonCost(WarlockPet petType)
{
    return 500; // Base mana cost placeholder
}

bool WarlockSpellCalculator::ShouldUseSoulShardForSpell(uint32 spellId, Player* caster)
{
    return true; // Placeholder
}

uint32 WarlockSpellCalculator::GetSoulShardPriority(uint32 spellId)
{
    return 50; // Placeholder priority
}

uint32 WarlockSpellCalculator::GetOptimalCurseForTarget(::Unit* target, Player* caster)
{
    return WarlockAI::CURSE_OF_ELEMENTS; // Placeholder
}

float WarlockSpellCalculator::CalculateCurseEffectiveness(uint32 curseId, ::Unit* target)
{
    return 1.5f; // Placeholder
}

void WarlockSpellCalculator::CacheWarlockData()
{
    // Placeholder implementation
}

// WarlockPetController implementation
WarlockPetController::WarlockPetController(WarlockAI* owner, ::Unit* pet) : _owner(owner), _pet(pet),
    _behavior(PetBehavior::DEFENSIVE), _currentTarget(nullptr), _lastCommand(0), _lastAbilityUse(0)
{
}

void WarlockPetController::Update(uint32 diff)
{
    if (!_pet || !_pet->IsAlive())
        return;

    UpdatePetCombat();
    UpdatePetMovement();
    UpdatePetAbilities();
}

void WarlockPetController::SetBehavior(PetBehavior behavior)
{
    _behavior = behavior;
}

void WarlockPetController::AttackTarget(::Unit* target)
{
    _currentTarget = target;
}

bool WarlockPetController::IsAlive() const
{
    return _pet && _pet->IsAlive();
}

bool WarlockPetController::IsInCombat() const
{
    return _pet && _pet->IsInCombat();
}

uint32 WarlockPetController::GetHealthPercent() const
{
    return _pet ? _pet->GetHealthPct() : 0;
}

Position WarlockPetController::GetPosition() const
{
    return _pet ? _pet->GetPosition() : Position();
}

// Placeholder implementations
void WarlockPetController::MoveToPosition(const Position& pos) {}
void WarlockPetController::UseAbility(uint32 spellId, ::Unit* target) {}
void WarlockPetController::ImpFirebolt(::Unit* target) {}
void WarlockPetController::VoidwalkerTorment() {}
void WarlockPetController::SuccubusSeduction(::Unit* target) {}
void WarlockPetController::FelhunterDevourMagic(::Unit* target) {}
void WarlockPetController::FelguardCleave() {}
void WarlockPetController::UpdatePetCombat() {}
void WarlockPetController::UpdatePetMovement() {}
void WarlockPetController::UpdatePetAbilities() {}
bool WarlockPetController::ShouldUsePetAbility(uint32 spellId) { return true; }

} // namespace Playerbot