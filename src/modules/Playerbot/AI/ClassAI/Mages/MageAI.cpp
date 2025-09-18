/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "MageAI.h"
#include "ActionPriority.h"
#include "CooldownManager.h"
#include "ResourceManager.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Map.h"
#include "Log.h"

namespace Playerbot
{

// Initialize spell school mappings
const std::unordered_map<uint32, MageSchool> MageAI::_spellSchools = {
    // Arcane spells
    {ARCANE_MISSILES, MageSchool::ARCANE},
    {ARCANE_BLAST, MageSchool::ARCANE},
    {ARCANE_BARRAGE, MageSchool::ARCANE},
    {ARCANE_ORB, MageSchool::ARCANE},
    {ARCANE_EXPLOSION, MageSchool::ARCANE},

    // Fire spells
    {FIREBALL, MageSchool::FIRE},
    {FIRE_BLAST, MageSchool::FIRE},
    {PYROBLAST, MageSchool::FIRE},
    {FLAMESTRIKE, MageSchool::FIRE},
    {SCORCH, MageSchool::FIRE},
    {LIVING_BOMB, MageSchool::FIRE},
    {DRAGON_BREATH, MageSchool::FIRE},

    // Frost spells
    {FROSTBOLT, MageSchool::FROST},
    {ICE_LANCE, MageSchool::FROST},
    {FROZEN_ORB, MageSchool::FROST},
    {BLIZZARD, MageSchool::FROST},
    {CONE_OF_COLD, MageSchool::FROST},
    {FROST_NOVA, MageSchool::FROST}
};

MageAI::MageAI(Player* bot) : ClassAI(bot), _manaSpent(0), _damageDealt(0),
    _spellsCast(0), _interruptedCasts(0), _lastPolymorph(0), _lastCounterspell(0),
    _lastBlink(0), _arcaneCharges(0), _arcaneOrbCharges(0), _arcaneBlastStacks(0),
    _lastArcanePower(0), _combustionStacks(0), _pyroblastProcs(0), _hotStreakAvailable(false),
    _lastCombustion(0), _frostboltCounter(0), _icicleStacks(0), _frozenOrbCharges(0),
    _lastIcyVeins(0), _wintersChill(false), _lastManaShield(0), _lastIceBarrier(0)
{
    _specialization = DetectSpecialization();
    TC_LOG_DEBUG("playerbot.mage", "MageAI initialized for {} with specialization {}",
                 GetBot()->GetName(), static_cast<uint32>(_specialization));
}

void MageAI::UpdateRotation(::Unit* target)
{
    if (!target)
        return;

    // Update positioning first
    UpdateMagePositioning();

    // Check if we can cast (not moving, not silenced, etc.)
    if (!CanCastSpell())
        return;

    // Use crowd control if needed
    UseCrowdControl(target);

    // Execute rotation based on specialization
    switch (_specialization)
    {
        case MageSpec::ARCANE:
            UpdateArcaneRotation(target);
            break;
        case MageSpec::FIRE:
            UpdateFireRotation(target);
            break;
        case MageSpec::FROST:
            UpdateFrostRotation(target);
            break;
    }

    // Check for AoE opportunities
    std::vector<::Unit*> nearbyEnemies = GetNearbyEnemies(10.0f);
    if (nearbyEnemies.size() > 1)
    {
        UseAoEAbilities(nearbyEnemies);
    }
}

void MageAI::UpdateBuffs()
{
    UpdateMageBuffs();
}

void MageAI::UpdateCooldowns(uint32 diff)
{
    // Use defensive abilities if in danger
    if (IsInCriticalDanger())
    {
        HandleEmergencySituation();
    }
    else if (IsInDanger())
    {
        UseDefensiveAbilities();
    }

    // Use offensive cooldowns in good situations
    if (_inCombat && _currentTarget && GetBot()->GetHealthPct() > 50.0f)
    {
        UseOffensiveCooldowns();
    }

    // Manage mana efficiency
    OptimizeManaUsage();

    // Update specialization-specific mechanics
    switch (_specialization)
    {
        case MageSpec::ARCANE:
            ManageArcaneCharges();
            UpdateArcaneOrb();
            break;
        case MageSpec::FIRE:
            ManageCombustion();
            UpdateHotStreak();
            break;
        case MageSpec::FROST:
            ManageFrostbolt();
            UpdateIcicles();
            break;
    }

    // Update performance metrics
    UpdatePerformanceMetrics(diff);
}

bool MageAI::CanUseAbility(uint32 spellId)
{
    if (!IsSpellReady(spellId) || !IsSpellUsable(spellId))
        return false;

    if (!HasEnoughResource(spellId))
        return false;

    // Check if we're currently casting
    if (IsCasting() || IsChanneling())
    {
        // Only allow instant spells while casting
        if (!IsSpellInstant(spellId))
            return false;
    }

    // Check range to target for targeted spells
    if (_currentTarget)
    {
        if (!IsInRange(_currentTarget, spellId))
            return false;

        if (!HasLineOfSight(_currentTarget))
            return false;
    }

    // School-specific checks
    MageSchool school = GetSpellSchool(spellId);

    // Check for spell school lockouts (from counterspell, etc.)
    // This would integrate with TrinityCore's spell school lockout system

    return true;
}

void MageAI::OnCombatStart(::Unit* target)
{
    ClassAI::OnCombatStart(target);

    // Reset combat variables
    _manaSpent = 0;
    _damageDealt = 0;
    _spellsCast = 0;
    _interruptedCasts = 0;

    // Specialization-specific combat preparation
    switch (_specialization)
    {
        case MageSpec::ARCANE:
            _arcaneCharges = 0;
            _arcaneBlastStacks = 0;
            break;
        case MageSpec::FIRE:
            _combustionStacks = 0;
            _hotStreakAvailable = false;
            break;
        case MageSpec::FROST:
            _frostboltCounter = 0;
            _wintersChill = false;
            break;
    }

    TC_LOG_DEBUG("playerbot.mage", "Mage {} entering combat - Spec: {}, Mana: {}%",
                 GetBot()->GetName(), static_cast<uint32>(_specialization), GetManaPercent() * 100);
}

void MageAI::OnCombatEnd()
{
    ClassAI::OnCombatEnd();

    // Analyze combat effectiveness
    AnalyzeCastingEffectiveness();

    // Use mana regeneration abilities if needed
    if (GetManaPercent() < 0.5f)
    {
        UseManaRegeneration();
    }
}

bool MageAI::HasEnoughResource(uint32 spellId)
{
    return _resourceManager->HasEnoughResource(spellId);
}

void MageAI::ConsumeResource(uint32 spellId)
{
    uint32 manaCost = 0;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (spellInfo && spellInfo->PowerType == POWER_MANA)
    {
        manaCost = spellInfo->ManaCost + spellInfo->ManaCostPercentage * GetMaxMana() / 100;
    }

    _resourceManager->ConsumeResource(spellId);
    _manaSpent += manaCost;

    TC_LOG_DEBUG("playerbot.mage", "Consumed {} mana for spell {}", manaCost, spellId);
}

Position MageAI::GetOptimalPosition(::Unit* target)
{
    if (!target)
        return GetBot()->GetPosition();

    // Mages want to stay at optimal casting range
    float distance = GetOptimalRange(target);
    float angle = GetBot()->GetAngle(target);

    // Stay behind cover if possible, and away from melee range
    Position pos;
    target->GetNearPosition(pos, distance, angle + M_PI); // Opposite side from target
    return pos;
}

float MageAI::GetOptimalRange(::Unit* target)
{
    // Mages are ranged casters
    return OPTIMAL_CASTING_RANGE;
}

void MageAI::UpdateArcaneRotation(::Unit* target)
{
    if (!target || !IsAtOptimalRange(target))
        return;

    // Arcane priority rotation
    // 1. Arcane Orb if available and low mana
    if (GetManaPercent() < 0.4f && _arcaneOrbCharges > 0 && CanUseAbility(ARCANE_ORB))
    {
        _actionQueue->AddAction(ARCANE_ORB, ActionPriority::ROTATION, 90.0f, target);
        return;
    }

    // 2. Arcane Barrage if at max charges or low mana
    if ((_arcaneCharges >= MAX_ARCANE_CHARGES || GetManaPercent() < 0.3f) &&
        CanUseAbility(ARCANE_BARRAGE))
    {
        float score = 85.0f + (_arcaneCharges * 5.0f);
        _actionQueue->AddAction(ARCANE_BARRAGE, ActionPriority::ROTATION, score, target);
        return;
    }

    // 3. Arcane Blast to build charges
    if (CanUseAbility(ARCANE_BLAST))
    {
        float score = 80.0f - (_arcaneCharges * 10.0f); // Lower priority as charges increase
        if (GetManaPercent() > 0.5f)
            score += 10.0f;

        _actionQueue->AddAction(ARCANE_BLAST, ActionPriority::ROTATION, score, target);
        return;
    }

    // 4. Arcane Missiles if everything else is on cooldown
    if (CanUseAbility(ARCANE_MISSILES))
    {
        _actionQueue->AddAction(ARCANE_MISSILES, ActionPriority::ROTATION, 60.0f, target);
    }
}

void MageAI::UpdateFireRotation(::Unit* target)
{
    if (!target || !IsAtOptimalRange(target))
        return;

    // Fire priority rotation
    // 1. Hot Streak Pyroblast (instant cast proc)
    if (_hotStreakAvailable && CanUseAbility(PYROBLAST))
    {
        _actionQueue->AddAction(PYROBLAST, ActionPriority::BURST, 100.0f, target);
        _hotStreakAvailable = false;
        return;
    }

    // 2. Fire Blast for instant damage and crit chance
    if (CanUseAbility(FIRE_BLAST))
    {
        _actionQueue->AddAction(FIRE_BLAST, ActionPriority::ROTATION, 90.0f, target);
        return;
    }

    // 3. Scorch if target is low health (execute range)
    if (target->GetHealthPct() < 25.0f && CanUseAbility(SCORCH))
    {
        _actionQueue->AddAction(SCORCH, ActionPriority::BURST, 85.0f, target);
        return;
    }

    // 4. Living Bomb if not up
    if (!target->HasAura(LIVING_BOMB) && CanUseAbility(LIVING_BOMB))
    {
        _actionQueue->AddAction(LIVING_BOMB, ActionPriority::ROTATION, 80.0f, target);
        return;
    }

    // 5. Fireball as main nuke
    if (CanUseAbility(FIREBALL))
    {
        float score = 75.0f;
        if (GetManaPercent() > 0.6f)
            score += 10.0f;

        _actionQueue->AddAction(FIREBALL, ActionPriority::ROTATION, score, target);
    }
}

void MageAI::UpdateFrostRotation(::Unit* target)
{
    if (!target || !IsAtOptimalRange(target))
        return;

    // Frost priority rotation
    // 1. Ice Lance if target has Winter's Chill or is frozen
    if ((_wintersChill || target->HasAuraType(SPELL_AURA_MOD_STUN)) &&
        CanUseAbility(ICE_LANCE))
    {
        _actionQueue->AddAction(ICE_LANCE, ActionPriority::BURST, 95.0f, target);
        return;
    }

    // 2. Frozen Orb if available and multiple targets or cooldown available
    if (_frozenOrbCharges > 0 && CanUseAbility(FROZEN_ORB))
    {
        uint32 enemyCount = GetEnemyCount(15.0f);
        float score = 85.0f + (enemyCount * 5.0f);
        _actionQueue->AddAction(FROZEN_ORB, ActionPriority::ROTATION, score, target);
        return;
    }

    // 3. Frostbolt as main nuke (builds Winter's Chill)
    if (CanUseAbility(FROSTBOLT))
    {
        float score = 80.0f;
        if (!_wintersChill)
            score += 10.0f; // Higher priority to build Winter's Chill

        _actionQueue->AddAction(FROSTBOLT, ActionPriority::ROTATION, score, target);
    }
}

bool MageAI::HasEnoughMana(uint32 amount)
{
    return GetMana() >= amount;
}

uint32 MageAI::GetMana()
{
    return _resourceManager->GetResource(ResourceType::MANA);
}

uint32 MageAI::GetMaxMana()
{
    return _resourceManager->GetMaxResource(ResourceType::MANA);
}

float MageAI::GetManaPercent()
{
    return _resourceManager->GetResourcePercent(ResourceType::MANA);
}

void MageAI::OptimizeManaUsage()
{
    float manaPercent = GetManaPercent();

    // Emergency mana conservation
    if (manaPercent < MANA_EMERGENCY_THRESHOLD)
    {
        // Stop casting expensive spells
        // Use mana gems or other mana restoration
        if (CanUseAbility(CONJURE_MANA_GEM))
        {
            _actionQueue->AddAction(CONJURE_MANA_GEM, ActionPriority::EMERGENCY, 100.0f);
        }
    }
    // Conservative mana usage
    else if (manaPercent < MANA_CONSERVATION_THRESHOLD)
    {
        // Prefer more mana-efficient spells
        // Avoid expensive AoE abilities
    }
}

bool MageAI::ShouldConserveMana()
{
    return GetManaPercent() < MANA_CONSERVATION_THRESHOLD;
}

void MageAI::UseManaRegeneration()
{
    // Use mana gems if available
    if (CanUseAbility(CONJURE_MANA_GEM))
    {
        CastSpell(CONJURE_MANA_GEM);
    }

    // Evocation if available (channeled mana regen)
    // This would be a high-level spell for mana regeneration
}

void MageAI::UpdateMageBuffs()
{
    // Maintain armor spell
    UpdateArmorSpells();

    // Cast Arcane Intellect if not up
    CastArcaneIntellect();

    // Maintain defensive buffs based on situation
    if (IsInDanger())
    {
        CastManaShield();
        CastIceBarrier();
    }
}

void MageAI::CastMageArmor()
{
    if (!HasAura(MAGE_ARMOR) && CanUseAbility(MAGE_ARMOR))
    {
        CastSpell(MAGE_ARMOR);
    }
}

void MageAI::CastManaShield()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastManaShield > 30000 && CanUseAbility(MANA_SHIELD))
    {
        if (CastSpell(MANA_SHIELD))
        {
            _lastManaShield = currentTime;
        }
    }
}

void MageAI::CastIceBarrier()
{
    uint32 currentTime = getMSTime();
    if (_specialization == MageSpec::FROST &&
        currentTime - _lastIceBarrier > 30000 && CanUseAbility(ICE_BARRIER))
    {
        if (CastSpell(ICE_BARRIER))
        {
            _lastIceBarrier = currentTime;
        }
    }
}

void MageAI::CastArcaneIntellect()
{
    if (!HasAura(ARCANE_INTELLECT) && CanUseAbility(ARCANE_INTELLECT))
    {
        CastSpell(ARCANE_INTELLECT);
    }
}

void MageAI::UpdateArmorSpells()
{
    // Choose armor based on specialization and situation
    switch (_specialization)
    {
        case MageSpec::ARCANE:
        case MageSpec::FIRE:
            if (!HasAura(MAGE_ARMOR) && !HasAura(MOLTEN_ARMOR))
            {
                if (CanUseAbility(MOLTEN_ARMOR))
                    CastSpell(MOLTEN_ARMOR);
                else if (CanUseAbility(MAGE_ARMOR))
                    CastSpell(MAGE_ARMOR);
            }
            break;
        case MageSpec::FROST:
            if (!HasAura(FROST_ARMOR) && !HasAura(MAGE_ARMOR))
            {
                if (CanUseAbility(FROST_ARMOR))
                    CastSpell(FROST_ARMOR);
                else if (CanUseAbility(MAGE_ARMOR))
                    CastSpell(MAGE_ARMOR);
            }
            break;
    }
}

void MageAI::UseDefensiveAbilities()
{
    // Blink away from danger
    if (IsInMeleeRange(_currentTarget) && CanUseAbility(BLINK))
    {
        UseBlink();
    }

    // Use barriers
    UseBarrierSpells();

    // Frost Nova to freeze nearby enemies
    if (GetEnemyCount(8.0f) > 0 && CanUseAbility(FROST_NOVA))
    {
        _actionQueue->AddAction(FROST_NOVA, ActionPriority::SURVIVAL, 90.0f);
    }
}

void MageAI::UseBlink()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastBlink > BLINK_COOLDOWN && CanUseAbility(BLINK))
    {
        _actionQueue->AddAction(BLINK, ActionPriority::SURVIVAL, 95.0f);
        _lastBlink = currentTime;
    }
}

void MageAI::UseInvisibility()
{
    if (GetBot()->GetHealthPct() < 20.0f && CanUseAbility(INVISIBILITY))
    {
        _actionQueue->AddAction(INVISIBILITY, ActionPriority::EMERGENCY, 100.0f);
    }
}

void MageAI::UseIceBlock()
{
    if (GetBot()->GetHealthPct() < 15.0f && CanUseAbility(ICE_BLOCK))
    {
        _actionQueue->AddAction(ICE_BLOCK, ActionPriority::EMERGENCY, 100.0f);
    }
}

void MageAI::UseColdSnap()
{
    // Reset cooldowns if in dire situation
    if (GetBot()->GetHealthPct() < 25.0f && CanUseAbility(COLD_SNAP))
    {
        _actionQueue->AddAction(COLD_SNAP, ActionPriority::EMERGENCY, 95.0f);
    }
}

void MageAI::UseBarrierSpells()
{
    // Use appropriate barrier based on specialization
    switch (_specialization)
    {
        case MageSpec::FROST:
            CastIceBarrier();
            break;
        default:
            CastManaShield();
            break;
    }
}

void MageAI::UseOffensiveCooldowns()
{
    if (!_currentTarget)
        return;

    switch (_specialization)
    {
        case MageSpec::ARCANE:
            UseArcanePower();
            UsePresenceOfMind();
            break;
        case MageSpec::FIRE:
            UseCombustion();
            break;
        case MageSpec::FROST:
            UseIcyVeins();
            break;
    }

    // Mirror Image for threat reduction and extra damage
    if (HasTooMuchThreat() && CanUseAbility(MIRROR_IMAGE))
    {
        _actionQueue->AddAction(MIRROR_IMAGE, ActionPriority::SURVIVAL, 80.0f);
    }
}

void MageAI::UseArcanePower()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastArcanePower > 180000 && // 3 minute cooldown
        GetManaPercent() > 0.6f && CanUseAbility(ARCANE_POWER))
    {
        _actionQueue->AddAction(ARCANE_POWER, ActionPriority::BURST, 90.0f);
        _lastArcanePower = currentTime;
    }
}

void MageAI::UseCombustion()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastCombustion > 180000 && // 3 minute cooldown
        _combustionStacks > 0 && CanUseAbility(COMBUSTION))
    {
        _actionQueue->AddAction(COMBUSTION, ActionPriority::BURST, 95.0f);
        _lastCombustion = currentTime;
    }
}

void MageAI::UseIcyVeins()
{
    uint32 currentTime = getMSTime();
    if (currentTime - _lastIcyVeins > 180000 && // 3 minute cooldown
        CanUseAbility(ICY_VEINS))
    {
        _actionQueue->AddAction(ICY_VEINS, ActionPriority::BURST, 85.0f);
        _lastIcyVeins = currentTime;
    }
}

void MageAI::UsePresenceOfMind()
{
    if (CanUseAbility(PRESENCE_OF_MIND))
    {
        _actionQueue->AddAction(PRESENCE_OF_MIND, ActionPriority::BURST, 75.0f);
    }
}

void MageAI::UseMirrorImage()
{
    if (CanUseAbility(MIRROR_IMAGE))
    {
        _actionQueue->AddAction(MIRROR_IMAGE, ActionPriority::SURVIVAL, 70.0f);
    }
}

void MageAI::UseCrowdControl(::Unit* target)
{
    if (!target)
        return;

    // Counterspell interrupts
    if (ShouldInterrupt(target) && CanUseAbility(COUNTERSPELL))
    {
        UseCounterspell(target);
    }

    // Polymorph secondary targets
    ::Unit* polymorphTarget = GetBestPolymorphTarget();
    if (polymorphTarget && polymorphTarget != target)
    {
        UsePolymorph(polymorphTarget);
    }
}

void MageAI::UsePolymorph(::Unit* target)
{
    if (!target || !CanPolymorphSafely(target))
        return;

    uint32 currentTime = getMSTime();
    if (currentTime - _lastPolymorph > POLYMORPH_COOLDOWN && CanUseAbility(POLYMORPH))
    {
        _actionQueue->AddAction(POLYMORPH, ActionPriority::INTERRUPT, 100.0f, target);
        _lastPolymorph = currentTime;
        _polymorphTargets[target->GetGUID()] = currentTime;
    }
}

void MageAI::UseFrostNova()
{
    if (GetEnemyCount(8.0f) > 0 && CanUseAbility(FROST_NOVA))
    {
        _actionQueue->AddAction(FROST_NOVA, ActionPriority::SURVIVAL, 85.0f);
    }
}

void MageAI::UseCounterspell(::Unit* target)
{
    if (!target || !ShouldInterrupt(target))
        return;

    uint32 currentTime = getMSTime();
    if (currentTime - _lastCounterspell > COUNTERSPELL_COOLDOWN && CanUseAbility(COUNTERSPELL))
    {
        _actionQueue->AddAction(COUNTERSPELL, ActionPriority::INTERRUPT, 100.0f, target);
        _lastCounterspell = currentTime;
    }
}

void MageAI::UseBanish(::Unit* target)
{
    // Banish is typically a warlock spell, but including for completeness
    if (target && CanUseAbility(BANISH))
    {
        _actionQueue->AddAction(BANISH, ActionPriority::INTERRUPT, 80.0f, target);
    }
}

void MageAI::UseAoEAbilities(const std::vector<::Unit*>& enemies)
{
    uint32 enemyCount = static_cast<uint32>(enemies.size());

    if (enemyCount < 2)
        return;

    // Choose AoE based on specialization and enemy count
    switch (_specialization)
    {
        case MageSpec::ARCANE:
            UseArcaneExplosion(enemies);
            break;
        case MageSpec::FIRE:
            UseFlamestrike(enemies);
            break;
        case MageSpec::FROST:
            UseBlizzard(enemies);
            UseConeOfCold(enemies);
            break;
    }
}

void MageAI::UseBlizzard(const std::vector<::Unit*>& enemies)
{
    if (enemies.size() > 2 && CanUseAbility(BLIZZARD))
    {
        float score = 70.0f + (enemies.size() * 10.0f);
        _actionQueue->AddAction(BLIZZARD, ActionPriority::ROTATION, score, enemies[0]);
    }
}

void MageAI::UseFlamestrike(const std::vector<::Unit*>& enemies)
{
    if (enemies.size() > 2 && CanUseAbility(FLAMESTRIKE))
    {
        float score = 75.0f + (enemies.size() * 10.0f);
        _actionQueue->AddAction(FLAMESTRIKE, ActionPriority::ROTATION, score, enemies[0]);
    }
}

void MageAI::UseArcaneExplosion(const std::vector<::Unit*>& enemies)
{
    if (enemies.size() > 1 && CanUseAbility(ARCANE_EXPLOSION))
    {
        float score = 65.0f + (enemies.size() * 8.0f);
        _actionQueue->AddAction(ARCANE_EXPLOSION, ActionPriority::ROTATION, score);
    }
}

void MageAI::UseConeOfCold(const std::vector<::Unit*>& enemies)
{
    if (enemies.size() > 1 && CanUseAbility(CONE_OF_COLD))
    {
        float score = 60.0f + (enemies.size() * 5.0f);
        _actionQueue->AddAction(CONE_OF_COLD, ActionPriority::ROTATION, score);
    }
}

void MageAI::UpdateMagePositioning()
{
    if (!_currentTarget)
        return;

    if (NeedsToKite(_currentTarget))
    {
        PerformKiting(_currentTarget);
    }
    else if (!IsAtOptimalRange(_currentTarget))
    {
        MoveToTarget(_currentTarget, GetOptimalRange(_currentTarget));
    }
}

bool MageAI::IsAtOptimalRange(::Unit* target)
{
    if (!target)
        return false;

    float distance = GetBot()->GetDistance(target);
    return distance >= MINIMUM_SAFE_RANGE && distance <= OPTIMAL_CASTING_RANGE;
}

bool MageAI::NeedsToKite(::Unit* target)
{
    if (!target)
        return false;

    float distance = GetBot()->GetDistance(target);
    return distance < MINIMUM_SAFE_RANGE || IsInMeleeRange(target);
}

void MageAI::PerformKiting(::Unit* target)
{
    if (!target)
        return;

    // Move to kiting range
    Position kitePos = GetOptimalPosition(target);
    GetBot()->GetMotionMaster()->MovePoint(0, kitePos.GetPositionX(), kitePos.GetPositionY(), kitePos.GetPositionZ());

    // Use Blink if available and in immediate danger
    if (IsInMeleeRange(target) && CanUseAbility(BLINK))
    {
        UseBlink();
    }
}

bool MageAI::IsInDanger()
{
    float healthPct = GetBot()->GetHealthPct();
    uint32 nearbyEnemies = GetEnemyCount(10.0f);

    return healthPct < 50.0f || nearbyEnemies > 2 || IsInMeleeRange(_currentTarget);
}

bool MageAI::IsInCriticalDanger()
{
    float healthPct = GetBot()->GetHealthPct();
    uint32 nearbyEnemies = GetEnemyCount(5.0f);

    return healthPct < 25.0f || nearbyEnemies > 3;
}

void MageAI::HandleEmergencySituation()
{
    // Use most powerful defensive abilities
    UseIceBlock();
    UseInvisibility();
    UseBlink();
    UseFrostNova();
}

void MageAI::FindSafeCastingPosition()
{
    // Find a position that's safe from melee and has line of sight to target
    if (!_currentTarget)
        return;

    Position safePos = GetOptimalPosition(_currentTarget);
    GetBot()->GetMotionMaster()->MovePoint(0, safePos.GetPositionX(), safePos.GetPositionY(), safePos.GetPositionZ());
}

::Unit* MageAI::GetBestPolymorphTarget()
{
    std::vector<::Unit*> enemies = GetNearbyEnemies(30.0f);

    for (::Unit* enemy : enemies)
    {
        if (enemy == _currentTarget)
            continue;

        if (CanPolymorphSafely(enemy))
            return enemy;
    }

    return nullptr;
}

::Unit* MageAI::GetBestCounterspellTarget()
{
    std::vector<::Unit*> enemies = GetNearbyEnemies(30.0f);

    for (::Unit* enemy : enemies)
    {
        if (ShouldInterrupt(enemy))
            return enemy;
    }

    return nullptr;
}

::Unit* MageAI::GetBestAoETarget()
{
    std::vector<::Unit*> enemies = GetNearbyEnemies(15.0f);

    if (enemies.size() > 1)
        return enemies[0]; // Return first enemy as AoE center

    return nullptr;
}

bool MageAI::ShouldInterrupt(::Unit* target)
{
    if (!target)
        return false;

    return target->IsNonMeleeSpellCast(false);
}

bool MageAI::CanPolymorphSafely(::Unit* target)
{
    if (!target)
        return false;

    // Check if target is humanoid or beast
    uint32 creatureType = target->GetCreatureType();
    return creatureType == CREATURE_TYPE_HUMANOID || creatureType == CREATURE_TYPE_BEAST;
}

void MageAI::ManageArcaneCharges()
{
    // Track Arcane Blast stacks and Arcane Charges
    // This would integrate with the actual spell effect tracking
}

void MageAI::UpdateArcaneOrb()
{
    // Track Arcane Orb charges and availability
    // This would check for the actual spell charges from TrinityCore
}

void MageAI::ManageArcaneBlast()
{
    // Manage the Arcane Blast stacking mechanic
}

void MageAI::ManageCombustion()
{
    // Track Combustion stacks from DoT spells
}

void MageAI::UpdateHotStreak()
{
    // Check for Hot Streak proc availability
}

void MageAI::ManagePyroblastProcs()
{
    // Track Pyroblast instant cast procs
}

void MageAI::ManageFrostbolt()
{
    // Track Frostbolt effects and Winter's Chill
}

void MageAI::UpdateIcicles()
{
    // Track Icicle stacks for Frost mages
}

void MageAI::ManageWintersChill()
{
    // Track Winter's Chill debuff on target
}

void MageAI::RecordSpellCast(uint32 spellId, ::Unit* target)
{
    _spellsCast++;
    RecordPerformanceMetric("spells_cast", 1);
}

void MageAI::RecordSpellHit(uint32 spellId, ::Unit* target, uint32 damage)
{
    _damageDealt += damage;
    RecordPerformanceMetric("damage_dealt", damage);
}

void MageAI::RecordSpellCrit(uint32 spellId, ::Unit* target, uint32 damage)
{
    RecordPerformanceMetric("critical_hits", 1);
}

void MageAI::AnalyzeCastingEffectiveness()
{
    if (_spellsCast > 0)
    {
        float efficiency = static_cast<float>(_damageDealt) / _manaSpent;
        TC_LOG_DEBUG("playerbot.mage", "Combat efficiency: {} damage per mana", efficiency);
    }

    RecordPerformanceMetric("mana_spent", _manaSpent);
    RecordPerformanceMetric("interrupted_casts", _interruptedCasts);
}

bool MageAI::IsChanneling()
{
    return GetBot()->HasChannelInterruptFlag(CHANNEL_INTERRUPT_FLAG_INTERRUPT);
}

bool MageAI::IsCasting()
{
    return GetBot()->IsNonMeleeSpellCast(false);
}

bool MageAI::CanCastSpell()
{
    return !IsMoving() && !IsCasting() && !IsChanneling();
}

MageSchool MageAI::GetSpellSchool(uint32 spellId)
{
    auto it = _spellSchools.find(spellId);
    return (it != _spellSchools.end()) ? it->second : MageSchool::GENERIC;
}

uint32 MageAI::GetSpellCastTime(uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    return spellInfo ? spellInfo->CastTime : 0;
}

bool MageAI::IsSpellInstant(uint32 spellId)
{
    return GetSpellCastTime(spellId) == 0;
}

MageSpec MageAI::DetectSpecialization()
{
    // This would detect the mage's specialization based on talents
    // For now, return a default
    return MageSpec::FROST; // Default to Frost for safety and control
}

void MageAI::OptimizeForSpecialization()
{
    // Adjust AI behavior based on detected specialization
}

bool MageAI::HasTalent(uint32 talentId)
{
    // This would check if the player has a specific talent
    return false; // Placeholder
}

void MageAI::ManageThreat()
{
    if (HasTooMuchThreat())
    {
        ReduceThreat();
    }
}

bool MageAI::HasTooMuchThreat()
{
    // Check if we have too much threat on current target
    return false; // Placeholder
}

void MageAI::ReduceThreat()
{
    // Use threat reduction abilities like Mirror Image
    UseMirrorImage();
}

void MageAI::UseEmergencyEscape()
{
    UseInvisibility();
    UseIceBlock();
    UseBlink();
}

void MageAI::UpdatePerformanceMetrics(uint32 diff)
{
    // Track performance metrics for optimization
}

void MageAI::OptimizeCastingSequence()
{
    // Optimize spell casting sequence based on performance data
}

// MageSpellCalculator implementation

std::unordered_map<uint32, uint32> MageSpellCalculator::_baseDamageCache;
std::unordered_map<uint32, uint32> MageSpellCalculator::_manaCostCache;
std::unordered_map<uint32, uint32> MageSpellCalculator::_castTimeCache;
std::mutex MageSpellCalculator::_cacheMutex;

uint32 MageSpellCalculator::CalculateFireballDamage(Player* caster, ::Unit* target)
{
    if (!caster || !target)
        return 0;

    // This would calculate actual Fireball damage based on spell power, target resistance, etc.
    return 1000; // Placeholder
}

uint32 MageSpellCalculator::CalculateFrostboltDamage(Player* caster, ::Unit* target)
{
    if (!caster || !target)
        return 0;

    // This would calculate actual Frostbolt damage
    return 900; // Placeholder
}

uint32 MageSpellCalculator::CalculateArcaneMissilesDamage(Player* caster, ::Unit* target)
{
    if (!caster || !target)
        return 0;

    // This would calculate actual Arcane Missiles damage
    return 800; // Placeholder
}

uint32 MageSpellCalculator::CalculateSpellManaCost(uint32 spellId, Player* caster)
{
    if (!caster)
        return 0;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return 0;

    return spellInfo->ManaCost + spellInfo->ManaCostPercentage * caster->GetMaxPower(POWER_MANA) / 100;
}

uint32 MageSpellCalculator::ApplyArcanePowerBonus(uint32 damage, Player* caster)
{
    if (!caster || !caster->HasAura(MageAI::ARCANE_POWER))
        return damage;

    return static_cast<uint32>(damage * 1.3f); // 30% damage bonus
}

uint32 MageSpellCalculator::CalculateCastTime(uint32 spellId, Player* caster)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return 0;

    uint32 castTime = spellInfo->CastTime;

    // Apply haste
    if (caster)
    {
        float haste = GetHasteModifier(caster);
        castTime = static_cast<uint32>(castTime / haste);
    }

    return castTime;
}

float MageSpellCalculator::GetHasteModifier(Player* caster)
{
    if (!caster)
        return 1.0f;

    // This would calculate actual haste from gear and buffs
    return 1.0f; // Placeholder
}

float MageSpellCalculator::CalculateCritChance(uint32 spellId, Player* caster, ::Unit* target)
{
    if (!caster)
        return 0.0f;

    // This would calculate actual crit chance
    return 15.0f; // Placeholder 15% base
}

bool MageSpellCalculator::WillCriticalHit(uint32 spellId, Player* caster, ::Unit* target)
{
    float critChance = CalculateCritChance(spellId, caster, target);
    return (rand() % 100) < critChance;
}

float MageSpellCalculator::CalculateResistance(uint32 spellId, Player* caster, ::Unit* target)
{
    if (!target)
        return 0.0f;

    // This would calculate spell resistance
    return 0.0f; // Placeholder
}

uint32 MageSpellCalculator::ApplyResistance(uint32 damage, float resistance)
{
    return static_cast<uint32>(damage * (1.0f - resistance));
}

float MageSpellCalculator::GetSpecializationBonus(MageSpec spec, uint32 spellId)
{
    // This would return specialization bonuses for spells
    return 1.0f; // Placeholder
}

uint32 MageSpellCalculator::GetOptimalRotationSpell(MageSpec spec, Player* caster, ::Unit* target)
{
    // This would recommend the optimal spell for the rotation
    switch (spec)
    {
        case MageSpec::ARCANE: return MageAI::ARCANE_BLAST;
        case MageSpec::FIRE: return MageAI::FIREBALL;
        case MageSpec::FROST: return MageAI::FROSTBOLT;
        default: return MageAI::FIREBALL;
    }
}

void MageSpellCalculator::CacheSpellData(uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!spellInfo)
        return;

    std::lock_guard<std::mutex> lock(_cacheMutex);
    _manaCostCache[spellId] = spellInfo->ManaCost;
    _castTimeCache[spellId] = spellInfo->CastTime;
}

} // namespace Playerbot