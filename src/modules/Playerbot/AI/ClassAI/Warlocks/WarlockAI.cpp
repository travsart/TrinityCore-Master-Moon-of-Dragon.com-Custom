/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "WarlockAI.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "SpellAuras.h"
#include "Pet.h"
#include "ObjectAccessor.h"
#include "Map.h"
#include "Group.h"
#include "Log.h"
#include "World.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Cell.h"
#include "CellImpl.h"
#include "SpellHistory.h"
#include "CreatureAI.h"
#include "MotionMaster.h"
#include "../../../AI/Combat/BotThreatManager.h"
#include "../../../AI/Combat/TargetSelector.h"
#include "../../../AI/Combat/PositionManager.h"
#include "../../../AI/Combat/InterruptManager.h"
#include "AfflictionSpecialization.h"
#include "DemonologySpecialization.h"
#include "DestructionSpecialization.h"
#include "../BaselineRotationManager.h"

namespace Playerbot
{

// Constructor with proper member initialization matching the header
WarlockAI::WarlockAI(Player* bot) :
    ClassAI(bot),
    _currentSpec(WarlockSpec::AFFLICTION),
    _specialization(nullptr),
    _warlockMetrics{},
    _threatManager(std::make_unique<BotThreatManager>(bot)),
    _targetSelector(std::make_unique<TargetSelector>(bot, _threatManager.get())),
    _positionManager(std::make_unique<PositionManager>(bot, _threatManager.get())),
    _interruptManager(std::make_unique<InterruptManager>(bot)),
    _currentSoulShards(0),
    _petActive(false),
    _petHealthPercent(0),
    _lastPetCheck(std::chrono::steady_clock::now()),
    _optimalManaThreshold(0.4f),
    _lowManaMode(false),
    _lastLifeTapTime(0)
{
    // Initialize specialization based on talent analysis
    InitializeSpecialization();

    // Reset metrics
    _warlockMetrics.Reset();

    // Log initialization
    TC_LOG_DEBUG("playerbot.warlock", "WarlockAI initialized for {} with specialization {}",
                 GetBot()->GetName(), static_cast<uint32>(_currentSpec));
}

void WarlockAI::InitializeSpecialization()
{
    // Detect current specialization from talents
    _currentSpec = DetectCurrentSpecialization();

    // Create appropriate specialization handler
    SwitchSpecialization(_currentSpec);
}

WarlockSpec WarlockAI::DetectCurrentSpecialization()
{
    Player* bot = GetBot();
    if (!bot)
        return WarlockSpec::AFFLICTION;

    // Count talent points in each tree
    uint32 afflictionPoints = 0;
    uint32 demonologyPoints = 0;
    uint32 destructionPoints = 0;

    // This would analyze talent points in each tree
    // For now, we'll use a simple check based on known key talents

    // Check for key Affliction talents (e.g., Unstable Affliction, Haunt)
    if (bot->HasSpell(30108) || bot->HasSpell(48181)) // Unstable Affliction or Haunt
    {
        afflictionPoints += 51;
    }

    // Check for key Demonology talents (e.g., Metamorphosis, Demonic Empowerment)
    if (bot->HasSpell(59672) || bot->HasSpell(47193)) // Metamorphosis or Demonic Empowerment
    {
        demonologyPoints += 51;
    }

    // Check for key Destruction talents (e.g., Chaos Bolt, Conflagrate)
    if (bot->HasSpell(50796) || bot->HasSpell(17962)) // Chaos Bolt or Conflagrate
    {
        destructionPoints += 51;
    }

    // Determine specialization based on highest point investment
    if (demonologyPoints > afflictionPoints && demonologyPoints > destructionPoints)
        return WarlockSpec::DEMONOLOGY;
    else if (destructionPoints > afflictionPoints && destructionPoints > demonologyPoints)
        return WarlockSpec::DESTRUCTION;
    else
        return WarlockSpec::AFFLICTION; // Default to Affliction
}

void WarlockAI::SwitchSpecialization(WarlockSpec newSpec)
{
    if (_currentSpec == newSpec && _specialization)
        return;

    _currentSpec = newSpec;

    // Create new specialization handler
    switch (_currentSpec)
    {
        case WarlockSpec::AFFLICTION:
            _specialization = std::make_unique<AfflictionSpecialization>(GetBot());
            TC_LOG_DEBUG("playerbot.warlock", "Switched to Affliction specialization");
            break;
        case WarlockSpec::DEMONOLOGY:
            _specialization = std::make_unique<DemonologySpecialization>(GetBot());
            TC_LOG_DEBUG("playerbot.warlock", "Switched to Demonology specialization");
            break;
        case WarlockSpec::DESTRUCTION:
            _specialization = std::make_unique<DestructionSpecialization>(GetBot());
            TC_LOG_DEBUG("playerbot.warlock", "Switched to Destruction specialization");
            break;
    }
}

void WarlockAI::UpdateRotation(::Unit* target)
{
    if (!target)
        return;

    // Check if bot should use baseline rotation (levels 1-9 or no spec)
    if (BaselineRotationManager::ShouldUseBaselineRotation(GetBot()))
    {
        static BaselineRotationManager baselineManager;
        baselineManager.HandleAutoSpecialization(GetBot());

        if (baselineManager.ExecuteBaselineRotation(GetBot(), target))
            return;

        // Fallback: basic ranged attack
        if (!GetBot()->IsNonMeleeSpellCast(false))
        {
            if (GetBot()->GetDistance(target) <= 35.0f)
            {
                GetBot()->AttackerStateUpdate(target);
            }
        }
        return;
    }

    // Specialized rotation for level 10+ with spec
    if (!_specialization)
        return;

    // Update performance metrics
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - _warlockMetrics.lastUpdate).count() > COMBAT_METRICS_UPDATE_INTERVAL)
    {
        _warlockMetrics.lastUpdate = now;

        // Calculate efficiency metrics
        if (_warlockMetrics.manaSpent > 0)
        {
            _warlockMetrics.manaEfficiency = static_cast<float>(_warlockMetrics.damageDealt) / _warlockMetrics.manaSpent;
        }

        // Update pet uptime
        if (_petActive)
        {
            auto combatDuration = std::chrono::duration_cast<std::chrono::seconds>(now - _warlockMetrics.combatStartTime).count();
            if (combatDuration > 0)
            {
                _warlockMetrics.petUptime = 100.0f; // Pet is currently active
            }
        }
    }

    // Delegate to specialization for rotation
    DelegateToSpecialization(target);
}

void WarlockAI::DelegateToSpecialization(::Unit* target)
{
    if (!_specialization)
        return;

    // Let specialization handle the rotation
    _specialization->UpdateRotation(target);

    // Track ability usage
    _warlockMetrics.spellsCast++;
}

void WarlockAI::UpdateBuffs()
{
    // Use baseline buffs for low-level bots
    if (BaselineRotationManager::ShouldUseBaselineRotation(GetBot()))
    {
        static BaselineRotationManager baselineManager;
        baselineManager.ApplyBaselineBuffs(GetBot());
        return;
    }

    // Specialized buff management for level 10+ with spec
    // Update warlock-specific buffs
    UpdateWarlockBuffs();

    // Delegate to specialization for spec-specific buffs
    if (_specialization)
    {
        _specialization->UpdateBuffs();
    }
}

void WarlockAI::UpdateCooldowns(uint32 diff)
{
    // Manage warlock-specific cooldowns
    ManageWarlockCooldowns();

    // Delegate to specialization
    if (_specialization)
    {
        _specialization->UpdateCooldowns(diff);
    }
}

bool WarlockAI::CanUseAbility(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    // Check if spell is known and ready
    if (!bot->HasSpell(spellId))
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, bot->GetMap()->GetDifficultyID());
    if (!spellInfo)
        return false;

    // Check if we have enough resources
    if (!HasEnoughResource(spellId))
        return false;

    // Check cooldown
    if (bot->GetSpellHistory()->HasCooldown(spellId))
        return false;

    // Check if we're casting or channeling
    if (bot->IsNonMeleeSpellCast(false, true, true))
        return false;

    // Check soul shard requirements
    for (uint32 i = 0; i < MAX_SPELL_REAGENTS; ++i)
    {
        if (spellInfo->Reagent[i] == 6265) // Soul Shard item ID
        {
            if (bot->GetItemCount(6265) < spellInfo->ReagentCount[i])
                return false;
        }
    }

    // Delegate to specialization for additional checks
    if (_specialization)
    {
        return _specialization->CanUseAbility(spellId);
    }

    return true;
}

void WarlockAI::OnCombatStart(::Unit* target)
{
    ClassAI::OnCombatStart(target);

    // Reset combat metrics
    _warlockMetrics.Reset();

    // Update pet status
    UpdatePetCheck();

    // Initialize threat management
    if (_threatManager)
    {
        // BotThreatManager handles combat start automatically
    }

    // Delegate to specialization
    if (_specialization)
    {
        _specialization->OnCombatStart(target);
    }

    TC_LOG_DEBUG("playerbot.warlock", "Warlock {} entering combat - Spec: {}, Soul Shards: {}",
                 GetBot()->GetName(), static_cast<uint32>(_currentSpec), _currentSoulShards.load());
}

void WarlockAI::OnCombatEnd()
{
    ClassAI::OnCombatEnd();

    // Log combat metrics
    auto combatDuration = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - _warlockMetrics.combatStartTime).count();

    if (combatDuration > 0)
    {
        TC_LOG_DEBUG("playerbot.warlock", "Combat summary for {}: Duration: {}s, Damage: {}, DoT: {}, Pet: {}, Efficiency: {:.2f}",
                     GetBot()->GetName(), combatDuration,
                     _warlockMetrics.damageDealt.load(),
                     _warlockMetrics.dotDamage.load(),
                     _warlockMetrics.petDamage.load(),
                     _warlockMetrics.manaEfficiency.load());
    }

    // Life Tap if needed
    ManageLifeTapTiming();

    // Delegate to specialization
    if (_specialization)
    {
        _specialization->OnCombatEnd();
    }
}

bool WarlockAI::HasEnoughResource(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, bot->GetMap()->GetDifficultyID());
    if (!spellInfo)
        return false;

    // Check mana
    auto powerCosts = spellInfo->CalcPowerCost(bot, spellInfo->GetSchoolMask());
    uint32 manaCost = 0;
    for (auto const& cost : powerCosts)
        if (cost.Power == POWER_MANA)
            manaCost = cost.Amount;
    if (bot->GetPower(POWER_MANA) < int32(manaCost))
    {
        _lowManaMode = true;
        return false;
    }

    // Check if we're in low mana mode
    if (bot->GetPowerPct(POWER_MANA) < LOW_MANA_THRESHOLD * 100)
    {
        _lowManaMode = true;
    }
    else if (bot->GetPowerPct(POWER_MANA) > 50)
    {
        _lowManaMode = false;
    }

    return true;
}

void WarlockAI::ConsumeResource(uint32 spellId)
{
    Player* bot = GetBot();
    if (!bot)
        return;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, bot->GetMap()->GetDifficultyID());
    if (!spellInfo)
        return;

    // Track mana spent
    auto powerCosts = spellInfo->CalcPowerCost(bot, spellInfo->GetSchoolMask());
    uint32 manaCost = 0;
    for (auto const& cost : powerCosts)
        if (cost.Power == POWER_MANA)
            manaCost = cost.Amount;
    _warlockMetrics.manaSpent += manaCost;

    // Track soul shard usage
    for (uint32 i = 0; i < MAX_SPELL_REAGENTS; ++i)
    {
        if (spellInfo->Reagent[i] == 6265) // Soul Shard
        {
            _warlockMetrics.soulShardsUsed++;
            _currentSoulShards = bot->GetItemCount(6265);
            break;
        }
    }
}

Position WarlockAI::GetOptimalPosition(::Unit* target)
{
    if (!target || !_positionManager)
        return GetBot()->GetPosition();

    // Use position manager for optimal positioning
    return _positionManager->FindRangedPosition(target, GetOptimalRange(target));
}

float WarlockAI::GetOptimalRange(::Unit* target)
{
    if (!target)
        return 30.0f;

    // Warlocks prefer to stay at max casting range
    return 35.0f; // Slightly less than max to account for movement
}

// Warlock-specific utility implementations
void WarlockAI::UpdateWarlockBuffs()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    // Demon Armor/Fel Armor
    if (!bot->HasAura(47889) && !bot->HasAura(47893)) // Demon Armor / Fel Armor
    {
        if (bot->HasSpell(47893)) // Fel Armor (higher rank)
            bot->CastSpell(bot, 47893, false);
        else if (bot->HasSpell(47889)) // Demon Armor
            bot->CastSpell(bot, 47889, false);
    }

    // Soul Link for Demonology
    if (_currentSpec == WarlockSpec::DEMONOLOGY)
    {
        if (bot->HasSpell(19028) && !bot->HasAura(19028) && _petActive)
        {
            bot->CastSpell(bot, 19028, false);
        }
    }
}

void WarlockAI::UpdatePetCheck()
{
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastPetCheck).count() < PET_CHECK_INTERVAL)
        return;

    _lastPetCheck = now;

    Player* bot = GetBot();
    if (!bot)
        return;

    Pet* pet = bot->GetPet();
    _petActive = (pet && pet->IsAlive());

    if (_petActive && pet)
    {
        _petHealthPercent = static_cast<uint32>(pet->GetHealthPct());

        // Check if pet needs healing
        if (_petHealthPercent < 50)
        {
            // Use Health Funnel if available
            if (bot->HasSpell(755) && !bot->GetSpellHistory()->HasCooldown(755))
            {
                bot->CastSpell(pet, 755, false);
                TC_LOG_DEBUG("playerbot.warlock", "Healing pet with Health Funnel");
            }
        }
    }
    else
    {
        _petHealthPercent = 0;

        // Summon pet if we don't have one and not in combat
        if (!bot->IsInCombat())
        {
            uint32 summonSpell = 0;

            // Choose pet based on spec
            switch (_currentSpec)
            {
                case WarlockSpec::AFFLICTION:
                    summonSpell = 691; // Summon Felhunter
                    break;
                case WarlockSpec::DEMONOLOGY:
                    summonSpell = 30146; // Summon Felguard
                    if (!bot->HasSpell(summonSpell))
                        summonSpell = 697; // Summon Voidwalker
                    break;
                case WarlockSpec::DESTRUCTION:
                    summonSpell = 688; // Summon Imp
                    break;
            }

            if (summonSpell && bot->HasSpell(summonSpell) && !bot->GetSpellHistory()->HasCooldown(summonSpell))
            {
                bot->CastSpell(bot, summonSpell, false);
                TC_LOG_DEBUG("playerbot.warlock", "Summoning pet with spell {}", summonSpell);
            }
        }
    }
}

void WarlockAI::UpdateSoulShardCheck()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    _currentSoulShards = bot->GetItemCount(6265);

    // Track soul shard history for optimization
    _soulShardHistory.push(_currentSoulShards);
    if (_soulShardHistory.size() > 10)
        _soulShardHistory.pop();
}

void WarlockAI::OptimizeManaManagement()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    float manaPct = bot->GetPowerPct(POWER_MANA);

    // Adjust mana threshold based on combat situation
    if (bot->IsInCombat())
    {
        _optimalManaThreshold = 0.3f; // Lower threshold in combat
    }
    else
    {
        _optimalManaThreshold = 0.5f; // Higher threshold out of combat
    }

    // Update low mana mode
    _lowManaMode = (manaPct < _optimalManaThreshold * 100);
}

void WarlockAI::ManageLifeTapTiming()
{
    Player* bot = GetBot();
    if (!bot)
        return;

    uint32 now = getMSTime();

    // Don't Life Tap too frequently
    if (now - _lastLifeTapTime < 3000)
        return;

    float healthPct = bot->GetHealthPct();
    float manaPct = bot->GetPowerPct(POWER_MANA);

    // Only Life Tap if health is good and mana is low
    if (healthPct > LIFE_TAP_THRESHOLD * 100 && manaPct < _optimalManaThreshold * 100)
    {
        if (bot->HasSpell(57946) && !bot->GetSpellHistory()->HasCooldown(57946)) // Life Tap (max rank)
        {
            bot->CastSpell(bot, 57946, false);
            _lastLifeTapTime = now;
            _warlockMetrics.lifeTapsCast++;
            TC_LOG_DEBUG("playerbot.warlock", "Life Tap cast - Health: {:.1f}%, Mana: {:.1f}%", healthPct, manaPct);
        }
    }
}

void WarlockAI::OptimizePetPositioning()
{
    if (!_petActive || !_positionManager)
        return;

    Player* bot = GetBot();
    Pet* pet = bot ? bot->GetPet() : nullptr;
    if (!pet)
        return;

    Unit* target = bot->GetVictim();
    if (!target)
        return;

    // Position pet based on its type and combat role
    Position optimalPos;
    float distance = 0.0f;

    // Determine optimal position based on pet type
    if (_currentSpec == WarlockSpec::DEMONOLOGY && bot->HasSpell(30146)) // Felguard
    {
        // Melee pet - position in front of target
        distance = 3.0f;
        optimalPos = target->GetNearPosition(distance, 0);
    }
    else if (_currentSpec == WarlockSpec::AFFLICTION) // Felhunter
    {
        // Anti-caster pet - position near casters
        distance = 5.0f;
        optimalPos = target->GetNearPosition(distance, M_PI / 4);
    }
    else // Imp or Voidwalker
    {
        // Ranged or tank pet - position at medium range
        distance = 15.0f;
        optimalPos = target->GetNearPosition(distance, M_PI / 2);
    }

    // Command pet to move if needed
    if (pet->GetDistance(optimalPos) > 5.0f)
    {
        pet->GetMotionMaster()->MovePoint(0, optimalPos);
    }
}

void WarlockAI::HandlePetSpecialAbilities()
{
    if (!_petActive)
        return;

    Player* bot = GetBot();
    Pet* pet = bot ? bot->GetPet() : nullptr;
    if (!pet)
        return;

    Unit* target = bot->GetVictim();
    if (!target)
        return;

    // Use pet abilities based on type and situation
    // This would be expanded with actual pet ability IDs and usage logic
}

void WarlockAI::ManageWarlockCooldowns()
{
    // Manage major warlock cooldowns
    Player* bot = GetBot();
    if (!bot || !bot->IsInCombat())
        return;

    // Demonic Empowerment for Demonology
    if (_currentSpec == WarlockSpec::DEMONOLOGY && _petActive)
    {
        if (bot->HasSpell(47193) && !bot->GetSpellHistory()->HasCooldown(47193))
        {
            bot->CastSpell(bot, 47193, false);
        }
    }

    // Metamorphosis for Demonology
    if (_currentSpec == WarlockSpec::DEMONOLOGY)
    {
        if (bot->HasSpell(59672) && !bot->GetSpellHistory()->HasCooldown(59672))
        {
            // Use in high-pressure situations
            if (bot->GetVictim() && bot->GetVictim()->GetHealthPct() > 50)
            {
                bot->CastSpell(bot, 59672, false);
            }
        }
    }
}

void WarlockAI::OptimizeSoulShardUsage()
{
    // Optimize soul shard usage based on availability and need
    std::lock_guard<std::mutex> lock(_soulShardMutex);

    // Determine conservation mode based on shard count
    bool shouldConserve = (_currentSoulShards < 5);

    if (shouldConserve)
    {
        TC_LOG_DEBUG("playerbot.warlock", "Soul shard conservation mode active - {} shards remaining",
                     _currentSoulShards.load());
    }
}

void WarlockAI::HandleAoESituations()
{
    Player* bot = GetBot();
    if (!bot || !bot->IsInCombat())
        return;

    // Count nearby enemies using Trinity searcher
    std::list<Unit*> enemies;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(bot, bot, 30.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, enemies, check);
    Cell::VisitAllObjects(bot, searcher, 30.0f);

    if (enemies.size() >= 3)
    {
        // Seed of Corruption for Affliction
        if (_currentSpec == WarlockSpec::AFFLICTION && bot->HasSpell(27243))
        {
            Unit* target = bot->GetVictim();
            if (target && !target->HasAura(27243) && !bot->GetSpellHistory()->HasCooldown(27243))
            {
                bot->CastSpell(target, 27243, false);
            }
        }

        // Rain of Fire for all specs
        if (bot->HasSpell(47820) && !bot->GetSpellHistory()->HasCooldown(47820))
        {
            Unit* target = bot->GetVictim();
            if (target)
            {
                bot->CastSpell(target, 47820, false);
            }
        }
    }
}

void WarlockAI::ManageCurseApplication()
{
    Player* bot = GetBot();
    if (!bot || !bot->IsInCombat())
        return;

    Unit* target = bot->GetVictim();
    if (!target)
        return;

    // Choose appropriate curse based on target and situation
    uint32 curseSpell = 0;

    // Check if target is a caster
    if (target->GetPowerType() == POWER_MANA)
    {
        // Curse of Tongues for casters
        curseSpell = 11719;
    }
    else if (_currentSpec == WarlockSpec::AFFLICTION)
    {
        // Curse of Agony for Affliction
        curseSpell = 47864;
    }
    else
    {
        // Curse of the Elements for damage increase
        curseSpell = 47865;
    }

    // Apply curse if not present
    if (curseSpell && bot->HasSpell(curseSpell) && !target->HasAura(curseSpell) && !bot->GetSpellHistory()->HasCooldown(curseSpell))
    {
        bot->CastSpell(target, curseSpell, false);
    }
}

void WarlockAI::OptimizeDoTRotation()
{
    if (_currentSpec != WarlockSpec::AFFLICTION)
        return;

    Player* bot = GetBot();
    if (!bot || !bot->IsInCombat())
        return;

    Unit* target = bot->GetVictim();
    if (!target)
        return;

    // Apply DoTs in priority order
    // Corruption
    if (!target->HasAura(47813) && bot->HasSpell(47813) && !bot->GetSpellHistory()->HasCooldown(47813))
    {
        bot->CastSpell(target, 47813, false);
        _warlockMetrics.dotUptime = 100.0f;
        return;
    }

    // Unstable Affliction
    if (!target->HasAura(47843) && bot->HasSpell(47843) && !bot->GetSpellHistory()->HasCooldown(47843))
    {
        bot->CastSpell(target, 47843, false);
        return;
    }

    // Curse of Agony
    if (!target->HasAura(47864) && bot->HasSpell(47864) && !bot->GetSpellHistory()->HasCooldown(47864))
    {
        bot->CastSpell(target, 47864, false);
        return;
    }

    // Haunt (if available)
    if (bot->HasSpell(59164) && !bot->GetSpellHistory()->HasCooldown(59164))
    {
        bot->CastSpell(target, 59164, false);
        return;
    }
}

} // namespace Playerbot