#!/usr/bin/env python3
"""
Fix EvokerAI.cpp compilation errors:
1. GetMasteryAmount() -> GetRatingBonusValue(CR_MASTERY)
2. Add Aura include
3. Fix private member access in controller classes
"""

# Read the file
with open('src/modules/Playerbot/AI/ClassAI/Evokers/EvokerAI.cpp', 'r') as f:
    content = f.read()

replacements = 0

# 1. Fix GetMasteryAmount -> GetRatingBonusValue(CR_MASTERY)
if 'caster->GetMasteryAmount()' in content:
    content = content.replace('caster->GetMasteryAmount()', 'caster->GetRatingBonusValue(CR_MASTERY)')
    print("Fixed: GetMasteryAmount -> GetRatingBonusValue(CR_MASTERY)")
    replacements += 1

# 2. Add SpellAuras.h include after SpellInfo.h
if '#include "SpellAuras.h"' not in content:
    content = content.replace('#include "SpellInfo.h"', '#include "SpellInfo.h"\n#include "SpellAuras.h"\n#include "SpellAuraEffects.h"')
    print("Added: SpellAuras.h include")
    replacements += 1

# 3. Fix EmpowermentController::CalculateOptimalLevel - use public methods
old_emp_optimal = '''EmpowermentLevel EmpowermentController::CalculateOptimalLevel(uint32 spellId, ::Unit* target)
{
    if (!_owner || !target)
        return EmpowermentLevel::RANK_1;

    Player* caster = _owner->GetBot();
    if (!caster)
        return EmpowermentLevel::RANK_1;

    // Emergency situations: fast cast
    if (caster->GetHealthPct() < 30.0f)
        return EmpowermentLevel::RANK_1;

    // Check for interrupt threats
    bool hasInterruptThreat = false;
    float rangeSq = 30.0f * 30.0f;

    if (Map* map = caster->GetMap())
    {
        // Check nearby enemies for caster mobs
        for (auto& pair : map->GetPlayers())
        {
            if (Player* player = pair.GetSource())
            {
                if (player->IsHostileTo(caster) && player->GetExactDistSq(caster) <= rangeSq)
                {
                    // Check if enemy is casting (potential interrupt)
                    if (player->IsNonMeleeSpellCast(false))
                    {
                        hasInterruptThreat = true;
                        break;
                    }
                }
            }
        }
    }

    // High interrupt risk: fast cast
    if (hasInterruptThreat)
        return EmpowermentLevel::RANK_1;

    // Get current essence to determine if we can follow up
    uint32 currentEssence = _owner->GetEssence();

    // Low essence: conserve with lower empowerment
    if (currentEssence <= 2)
        return EmpowermentLevel::RANK_2;

    // Count targets for AoE spells
    uint32 targetCount = 1;
    switch (spellId)
    {
        case EvokerAI::FIRE_BREATH:
        case EvokerAI::ETERNITYS_SURGE:
        case EvokerAI::DREAM_BREATH:
        case EvokerAI::SPIRIT_BLOOM:
        {
            ::std::vector<::Unit*> targets = _owner->GetEmpoweredSpellTargets(spellId);
            targetCount = static_cast<uint32>(targets.size());
            break;
        }
    }

    // Scale empowerment with target count
    if (targetCount >= 5)
        return EmpowermentLevel::RANK_4;
    else if (targetCount >= 3)
        return EmpowermentLevel::RANK_3;
    else if (targetCount >= 2)
        return EmpowermentLevel::RANK_2;

    // Single target: check spec
    EvokerSpec spec = _owner->DetectSpecialization();
    if (spec == EvokerSpec::PRESERVATION)
    {
        // Healer: higher empowerment for throughput
        return EmpowermentLevel::RANK_3;
    }

    // Default: balanced empowerment
    return EmpowermentLevel::RANK_2;
}'''

new_emp_optimal = '''EmpowermentLevel EmpowermentController::CalculateOptimalLevel(uint32 spellId, ::Unit* target)
{
    if (!_owner || !target)
        return EmpowermentLevel::RANK_1;

    Player* caster = _owner->GetBot();
    if (!caster)
        return EmpowermentLevel::RANK_1;

    // Emergency situations: fast cast
    if (caster->GetHealthPct() < 30.0f)
        return EmpowermentLevel::RANK_1;

    // Check for interrupt threats
    bool hasInterruptThreat = false;
    float rangeSq = 30.0f * 30.0f;

    if (Map* map = caster->GetMap())
    {
        // Check nearby enemies for caster mobs
        for (auto& pair : map->GetPlayers())
        {
            if (Player* player = pair.GetSource())
            {
                if (player->IsHostileTo(caster) && player->GetExactDistSq(caster) <= rangeSq)
                {
                    // Check if enemy is casting (potential interrupt)
                    if (player->IsNonMeleeSpellCast(false))
                    {
                        hasInterruptThreat = true;
                        break;
                    }
                }
            }
        }
    }

    // High interrupt risk: fast cast
    if (hasInterruptThreat)
        return EmpowermentLevel::RANK_1;

    // Get player's current power level (essence approximation through mana)
    uint32 currentPowerPct = static_cast<uint32>(caster->GetPowerPct(POWER_MANA));

    // Low resources: conserve with lower empowerment
    if (currentPowerPct <= 30)
        return EmpowermentLevel::RANK_2;

    // Count nearby enemies for AoE evaluation
    uint32 targetCount = 1;
    ::std::list<Unit*> nearbyEnemies;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(caster, caster, 30.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(caster, nearbyEnemies, check);
    Cell::VisitAllObjects(caster, searcher, 30.0f);
    targetCount = static_cast<uint32>(nearbyEnemies.size());

    // Scale empowerment with target count
    if (targetCount >= 5)
        return EmpowermentLevel::RANK_4;
    else if (targetCount >= 3)
        return EmpowermentLevel::RANK_3;
    else if (targetCount >= 2)
        return EmpowermentLevel::RANK_2;

    // Check spec via player's primary specialization
    ChrSpecializationEntry const* spec = caster->GetPrimarySpecializationEntry();
    if (spec && spec->ID == 1468)  // Preservation
    {
        // Healer: higher empowerment for throughput
        return EmpowermentLevel::RANK_3;
    }

    // Default: balanced empowerment
    return EmpowermentLevel::RANK_2;
}'''

if old_emp_optimal in content:
    content = content.replace(old_emp_optimal, new_emp_optimal)
    print("Fixed: EmpowermentController::CalculateOptimalLevel")
    replacements += 1

# 4. Fix EmpowermentController::ShouldEmpowerSpell
old_should_emp = '''bool EmpowermentController::ShouldEmpowerSpell(uint32 spellId)
{
    if (!_owner)
        return false;

    Player* caster = _owner->GetBot();
    if (!caster)
        return false;

    // Check if spell is empowerable
    switch (spellId)
    {
        case EvokerAI::FIRE_BREATH:
        case EvokerAI::ETERNITYS_SURGE:
        case EvokerAI::DREAM_BREATH:
        case EvokerAI::SPIRIT_BLOOM:
            break;  // Valid empowered spells
        default:
            return false;  // Not an empowered spell
    }

    // Don't empower if currently moving
    if (caster->isMoving())
        return false;

    // Don't empower if already channeling
    if (_currentSpell.isChanneling)
        return false;

    // Don't empower in emergency situations
    if (caster->GetHealthPct() < 20.0f)
        return false;

    // Check for nearby enemy casters (interrupt risk)
    float rangeSq = 20.0f * 20.0f;
    bool highInterruptRisk = false;

    if (Map* map = caster->GetMap())
    {
        for (auto& pair : map->GetPlayers())
        {
            if (Player* player = pair.GetSource())
            {
                if (player->IsHostileTo(caster) && player->GetExactDistSq(caster) <= rangeSq)
                {
                    // Enemy in melee range = high kick risk
                    if (player->GetExactDistSq(caster) <= 64.0f)  // 8 yard
                    {
                        highInterruptRisk = true;
                        break;
                    }
                }
            }
        }
    }

    // Avoid empowerment if high interrupt risk
    if (highInterruptRisk)
    {
        // Healer spec might still need to empower for healing throughput
        EvokerSpec spec = _owner->DetectSpecialization();
        if (spec != EvokerSpec::PRESERVATION)
            return false;
    }

    // Spec-specific logic
    EvokerSpec spec = _owner->DetectSpecialization();

    switch (spec)
    {
        case EvokerSpec::DEVASTATION:
            // DPS should empower for damage, but not during movement phases
            return !caster->isMoving();

        case EvokerSpec::PRESERVATION:
            // Healer should empower for throughput unless emergency
            if (Group* group = caster->GetGroup())
            {
                uint32 criticalCount = 0;
                for (auto const& member : group->GetMemberSlots())
                {
                    if (Player* player = ObjectAccessor::FindPlayer(member.guid))
                    {
                        if (player->GetHealthPct() < 30.0f)
                            criticalCount++;
                    }
                }
                // Don't empower if multiple critically injured (need fast heals)
                if (criticalCount >= 2)
                    return false;
            }
            return true;

        case EvokerSpec::AUGMENTATION:
            // Augmentation typically uses instant casts
            return false;

        default:
            return true;
    }
}'''

new_should_emp = '''bool EmpowermentController::ShouldEmpowerSpell(uint32 spellId)
{
    if (!_owner)
        return false;

    Player* caster = _owner->GetBot();
    if (!caster)
        return false;

    // Check if spell is empowerable
    switch (spellId)
    {
        case EvokerAI::FIRE_BREATH:
        case EvokerAI::ETERNITYS_SURGE:
        case EvokerAI::DREAM_BREATH:
        case EvokerAI::SPIRIT_BLOOM:
            break;  // Valid empowered spells
        default:
            return false;  // Not an empowered spell
    }

    // Don't empower if currently moving
    if (caster->isMoving())
        return false;

    // Don't empower if already channeling
    if (_currentSpell.isChanneling)
        return false;

    // Don't empower in emergency situations
    if (caster->GetHealthPct() < 20.0f)
        return false;

    // Check for nearby enemy casters (interrupt risk)
    bool highInterruptRisk = false;
    ::std::list<Unit*> nearbyEnemies;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(caster, caster, 8.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(caster, nearbyEnemies, check);
    Cell::VisitAllObjects(caster, searcher, 8.0f);

    if (!nearbyEnemies.empty())
        highInterruptRisk = true;

    // Avoid empowerment if high interrupt risk
    ChrSpecializationEntry const* spec = caster->GetPrimarySpecializationEntry();
    uint32 specId = spec ? spec->ID : 0;

    if (highInterruptRisk)
    {
        // Healer spec might still need to empower for healing throughput
        if (specId != 1468)  // Not Preservation
            return false;
    }

    // Spec-specific logic
    switch (specId)
    {
        case 1467:  // Devastation
            // DPS should empower for damage, but not during movement phases
            return !caster->isMoving();

        case 1468:  // Preservation
            // Healer should empower for throughput unless emergency
            if (Group* group = caster->GetGroup())
            {
                uint32 criticalCount = 0;
                for (auto const& member : group->GetMemberSlots())
                {
                    if (Player* player = ObjectAccessor::FindPlayer(member.guid))
                    {
                        if (player->GetHealthPct() < 30.0f)
                            criticalCount++;
                    }
                }
                // Don't empower if multiple critically injured (need fast heals)
                if (criticalCount >= 2)
                    return false;
            }
            return true;

        case 1473:  // Augmentation
            // Augmentation typically uses instant casts
            return false;

        default:
            return true;
    }
}'''

if old_should_emp in content:
    content = content.replace(old_should_emp, new_should_emp)
    print("Fixed: EmpowermentController::ShouldEmpowerSpell")
    replacements += 1

# 5. Fix EchoController::GetBestEchoTarget - remove private member access
old_echo_target = '''::Unit* EchoController::GetBestEchoTarget() const
{
    if (!_owner)
        return nullptr;

    Player* caster = _owner->GetBot();
    if (!caster)
        return nullptr;

    constexpr uint32 ECHO_SPELL_ID = 364343;

    ::Unit* bestTarget = nullptr;
    uint32 bestScore = 0;

    // Check group members
    if (Group* group = caster->GetGroup())
    {
        float rangeSq = 40.0f * 40.0f;

        for (auto const& member : group->GetMemberSlots())
        {
            Player* player = ObjectAccessor::FindPlayer(member.guid);
            if (!player || !player->IsAlive())
                continue;

            // Skip if out of range
            if (player->GetExactDistSq(caster) > rangeSq)
                continue;

            // Skip if already has Echo
            if (player->HasAura(ECHO_SPELL_ID, caster->GetGUID()))
                continue;

            // Skip full health targets (Echo healing wasted)
            if (player->GetHealthPct() > 95.0f)
                continue;

            uint32 score = 0;

            // Health deficit score (lower health = higher priority)
            float healthDeficit = 100.0f - player->GetHealthPct();
            score += static_cast<uint32>(healthDeficit);

            // Role priority
            ChrSpecializationEntry const* spec = player->GetPrimarySpecializationEntry();
            if (spec)
            {
                uint32 role = spec->Role;
                if (role == 1)       // Tank - highest priority (constant damage intake)
                    score += 50;
                else if (role == 0)  // DPS - medium priority
                    score += 30;
                // Healers lowest priority (self-sustaining)
            }

            // Melee bonus (more likely to take damage)
            if (player->GetVictim())
            {
                float distToTarget = std::sqrt(player->GetExactDistSq(player->GetVictim()));
                if (distToTarget < 8.0f)
                    score += 20;  // Melee range
            }

            // In combat bonus
            if (player->IsInCombat())
                score += 10;

            // Debuff check (players with bleeds/dots need more healing)
            uint32 debuffCount = 0;
            for (auto& pair : player->GetAppliedAuras())
            {
                Aura const* aura = pair.second->GetBase();
                if (aura && aura->GetSpellInfo()->IsPositive() == false)
                {
                    // Check for periodic damage
                    for (SpellEffectInfo const& effect : aura->GetSpellInfo()->GetEffects())
                    {
                        if (effect.IsEffect(SPELL_EFFECT_APPLY_AURA) &&
                            effect.ApplyAuraName == SPELL_AURA_PERIODIC_DAMAGE)
                        {
                            debuffCount++;
                            break;
                        }
                    }
                }
            }
            score += debuffCount * 10;

            if (score > bestScore)
            {
                bestScore = score;
                bestTarget = player;
            }
        }
    }

    // If no group, consider self
    if (!bestTarget && caster->GetHealthPct() < 90.0f && !caster->HasAura(ECHO_SPELL_ID))
        bestTarget = caster;

    return bestTarget;
}'''

new_echo_target = '''::Unit* EchoController::GetBestEchoTarget() const
{
    if (!_owner)
        return nullptr;

    Player* caster = _owner->GetBot();
    if (!caster)
        return nullptr;

    constexpr uint32 ECHO_SPELL_ID = 364343;

    ::Unit* bestTarget = nullptr;
    uint32 bestScore = 0;

    // Check group members
    if (Group* group = caster->GetGroup())
    {
        float rangeSq = 40.0f * 40.0f;

        for (auto const& member : group->GetMemberSlots())
        {
            Player* player = ObjectAccessor::FindPlayer(member.guid);
            if (!player || !player->IsAlive())
                continue;

            // Skip if out of range
            if (player->GetExactDistSq(caster) > rangeSq)
                continue;

            // Skip if already has Echo
            if (player->HasAura(ECHO_SPELL_ID, caster->GetGUID()))
                continue;

            // Skip full health targets (Echo healing wasted)
            if (player->GetHealthPct() > 95.0f)
                continue;

            uint32 score = 0;

            // Health deficit score (lower health = higher priority)
            float healthDeficit = 100.0f - player->GetHealthPct();
            score += static_cast<uint32>(healthDeficit);

            // Role priority
            ChrSpecializationEntry const* spec = player->GetPrimarySpecializationEntry();
            if (spec)
            {
                uint32 role = spec->Role;
                if (role == 1)       // Tank - highest priority (constant damage intake)
                    score += 50;
                else if (role == 0)  // DPS - medium priority
                    score += 30;
                // Healers lowest priority (self-sustaining)
            }

            // Melee bonus (more likely to take damage)
            if (player->GetVictim())
            {
                float distToTarget = std::sqrt(player->GetExactDistSq(player->GetVictim()));
                if (distToTarget < 8.0f)
                    score += 20;  // Melee range
            }

            // In combat bonus
            if (player->IsInCombat())
                score += 10;

            // Check for harmful auras (simplified - just count them)
            uint32 debuffCount = 0;
            Unit::AuraApplicationMap const& auras = player->GetAppliedAuras();
            for (auto const& auraPair : auras)
            {
                if (auraPair.second && auraPair.second->GetBase())
                {
                    SpellInfo const* spellInfo = auraPair.second->GetBase()->GetSpellInfo();
                    if (spellInfo && !spellInfo->IsPositive())
                        debuffCount++;
                }
            }
            score += debuffCount * 5;

            if (score > bestScore)
            {
                bestScore = score;
                bestTarget = player;
            }
        }
    }

    // If no group, consider self
    if (!bestTarget && caster->GetHealthPct() < 90.0f && !caster->HasAura(ECHO_SPELL_ID))
        bestTarget = caster;

    return bestTarget;
}'''

if old_echo_target in content:
    content = content.replace(old_echo_target, new_echo_target)
    print("Fixed: EchoController::GetBestEchoTarget")
    replacements += 1

# 6. Fix CalculateEchoHealing - remove Aura direct access
old_echo_heal = '''uint32 EvokerCalculator::CalculateEchoHealing(Player* caster, ::Unit* target)
{
    if (!caster)
        return 0;

    constexpr uint32 ECHO_SPELL_ID = 364343;
    constexpr float ECHO_BASE_COEFFICIENT = 0.30f;  // Echo duplicates 30% of original heal

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(ECHO_SPELL_ID, caster->GetMap()->GetDifficultyID());
    int32 spellPower = caster->SpellBaseHealingBonusDone(SPELL_SCHOOL_MASK_NATURE);

    float bonusCoefficient = ECHO_BASE_COEFFICIENT;
    int32 baseHealing = 0;

    if (spellInfo)
    {
        for (SpellEffectInfo const& effect : spellInfo->GetEffects())
        {
            if (effect.IsEffect(SPELL_EFFECT_HEAL) || effect.IsEffect(SPELL_EFFECT_HEAL_PCT))
            {
                baseHealing = effect.CalcValue(caster, nullptr, target);
                if (effect.BonusCoefficient > 0.0f)
                    bonusCoefficient = effect.BonusCoefficient;
                break;
            }
        }
    }

    float healing = static_cast<float>(baseHealing) + (spellPower * bonusCoefficient);

    // Apply versatility
    float versatility = caster->GetRatingBonusValue(CR_VERSATILITY_DAMAGE_DONE);
    healing *= (1.0f + versatility / 100.0f);

    // Apply Preservation mastery (Lifebinder)
    float masteryBonus = GetEvokerMasteryBonus(caster, target, true);
    healing *= (1.0f + masteryBonus);

    // Echo healing is reduced when target has multiple Echoes (diminishing returns)
    if (target)
    {
        // Check for existing Echo aura stacks
        if (Aura* echoAura = target->GetAura(ECHO_SPELL_ID, caster->GetGUID()))
        {
            uint8 stacks = echoAura->GetStackAmount();
            if (stacks > 1)
                healing *= (1.0f - (stacks - 1) * 0.1f);  // 10% reduction per extra stack
        }
    }

    // Apply spell healing bonus
    if (spellInfo && target)
    {
        for (SpellEffectInfo const& effect : spellInfo->GetEffects())
        {
            if (effect.IsEffect(SPELL_EFFECT_HEAL) || effect.IsEffect(SPELL_EFFECT_HEAL_PCT))
            {
                healing = static_cast<float>(caster->SpellHealingBonusDone(target, spellInfo,
                    static_cast<int32>(healing), HEAL, effect, 1, nullptr, nullptr));
                break;
            }
        }
    }

    return static_cast<uint32>(std::max(0.0f, healing));
}'''

new_echo_heal = '''uint32 EvokerCalculator::CalculateEchoHealing(Player* caster, ::Unit* target)
{
    if (!caster)
        return 0;

    constexpr uint32 ECHO_SPELL_ID = 364343;
    constexpr float ECHO_BASE_COEFFICIENT = 0.30f;  // Echo duplicates 30% of original heal

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(ECHO_SPELL_ID, caster->GetMap()->GetDifficultyID());
    int32 spellPower = caster->SpellBaseHealingBonusDone(SPELL_SCHOOL_MASK_NATURE);

    float bonusCoefficient = ECHO_BASE_COEFFICIENT;
    int32 baseHealing = 0;

    if (spellInfo)
    {
        for (SpellEffectInfo const& effect : spellInfo->GetEffects())
        {
            if (effect.IsEffect(SPELL_EFFECT_HEAL) || effect.IsEffect(SPELL_EFFECT_HEAL_PCT))
            {
                baseHealing = effect.CalcValue(caster, nullptr, target);
                if (effect.BonusCoefficient > 0.0f)
                    bonusCoefficient = effect.BonusCoefficient;
                break;
            }
        }
    }

    float healing = static_cast<float>(baseHealing) + (spellPower * bonusCoefficient);

    // Apply versatility
    float versatility = caster->GetRatingBonusValue(CR_VERSATILITY_DAMAGE_DONE);
    healing *= (1.0f + versatility / 100.0f);

    // Apply Preservation mastery (Lifebinder)
    float masteryBonus = GetEvokerMasteryBonus(caster, target, true);
    healing *= (1.0f + masteryBonus);

    // Echo healing is reduced when target has multiple Echoes (diminishing returns)
    if (target && target->HasAura(ECHO_SPELL_ID, caster->GetGUID()))
    {
        // Simple diminishing returns - assume some reduction for existing echo
        healing *= 0.9f;
    }

    // Apply spell healing bonus
    if (spellInfo && target)
    {
        for (SpellEffectInfo const& effect : spellInfo->GetEffects())
        {
            if (effect.IsEffect(SPELL_EFFECT_HEAL) || effect.IsEffect(SPELL_EFFECT_HEAL_PCT))
            {
                healing = static_cast<float>(caster->SpellHealingBonusDone(target, spellInfo,
                    static_cast<int32>(healing), HEAL, effect, 1, nullptr, nullptr));
                break;
            }
        }
    }

    return static_cast<uint32>(std::max(0.0f, healing));
}'''

if old_echo_heal in content:
    content = content.replace(old_echo_heal, new_echo_heal)
    print("Fixed: CalculateEchoHealing")
    replacements += 1

# Write back
with open('src/modules/Playerbot/AI/ClassAI/Evokers/EvokerAI.cpp', 'w') as f:
    f.write(content)

print(f"\nTotal fixes: {replacements}")
print("File updated successfully")
