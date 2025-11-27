#!/usr/bin/env python3
"""
Implement remaining EvokerAI.cpp placeholder functions with full TrinityCore API integration.
Focuses on healing calculations and controller methods.
"""

import sys

# Read the file
with open('src/modules/Playerbot/AI/ClassAI/Evokers/EvokerAI.cpp', 'r') as f:
    content = f.read()

replacements = 0

# 1. CalculateEmeraldBlossomHealing - Full implementation
old_emerald_blossom = '''uint32 EvokerCalculator::CalculateEmeraldBlossomHealing(Player* caster)
{
    // DESIGN NOTE: Calculate Emerald Blossom healing for Preservation Evoker bots
    // Returns 800 as baseline healing value
    // Full implementation should:
    // - Query spell power and versatility from caster
    // - Apply healing coefficient from SpellInfo
    // - Factor in Preservation mastery (Lifebinder) modifier
    // - Apply critical strike and multistrike bonuses
    // - Consider Echo interaction for duplicate healing
    // Reference: WoW 11.2 Emerald Blossom (spell ID 355913)
    return 800;
}'''

new_emerald_blossom = '''uint32 EvokerCalculator::CalculateEmeraldBlossomHealing(Player* caster)
{
    if (!caster)
        return 0;

    constexpr uint32 EMERALD_BLOSSOM_SPELL_ID = 355913;
    constexpr float DEFAULT_COEFFICIENT = 1.15f;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(EMERALD_BLOSSOM_SPELL_ID, caster->GetMap()->GetDifficultyID());
    int32 spellPower = caster->SpellBaseHealingBonusDone(SPELL_SCHOOL_MASK_NATURE);

    float bonusCoefficient = DEFAULT_COEFFICIENT;
    int32 baseHealing = 0;

    if (spellInfo)
    {
        for (SpellEffectInfo const& effect : spellInfo->GetEffects())
        {
            if (effect.IsEffect(SPELL_EFFECT_HEAL))
            {
                baseHealing = effect.CalcValue(caster, nullptr, nullptr);
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
    float masteryBonus = GetEvokerMasteryBonus(caster, nullptr, true);
    healing *= (1.0f + masteryBonus);

    // Apply critical strike chance (average contribution)
    float critChance = caster->GetRatingBonusValue(CR_CRIT_SPELL) / 100.0f;
    healing *= (1.0f + critChance * 0.5f);

    // Apply spell healing bonus modifier from gear/buffs
    if (spellInfo)
    {
        for (SpellEffectInfo const& effect : spellInfo->GetEffects())
        {
            if (effect.IsEffect(SPELL_EFFECT_HEAL))
            {
                healing = static_cast<float>(caster->SpellHealingBonusDone(caster, spellInfo,
                    static_cast<int32>(healing), HEAL, effect, 1, nullptr, nullptr));
                break;
            }
        }
    }

    return static_cast<uint32>(std::max(0.0f, healing));
}'''

if old_emerald_blossom in content:
    content = content.replace(old_emerald_blossom, new_emerald_blossom)
    print("CalculateEmeraldBlossomHealing: REPLACED")
    replacements += 1
else:
    print("CalculateEmeraldBlossomHealing: NOT FOUND")

# 2. CalculateVerdantEmbraceHealing - Full implementation
old_verdant_embrace = '''uint32 EvokerCalculator::CalculateVerdantEmbraceHealing(Player* caster, ::Unit* target)
{
    // DESIGN NOTE: Calculate Verdant Embrace healing for Preservation Evoker bots
    // Returns 1200 as baseline healing value
    // Full implementation should:
    // - Query spell power from caster stats
    // - Apply instant heal coefficient
    // - Factor in Preservation mastery (Lifebinder) multiplier
    // - Apply critical strike and versatility bonuses
    // - Consider target's missing health for smart healing
    // - Track Echo application for follow-up heals
    // Reference: WoW 11.2 Verdant Embrace (spell ID 360995)
    return 1200;
}'''

new_verdant_embrace = '''uint32 EvokerCalculator::CalculateVerdantEmbraceHealing(Player* caster, ::Unit* target)
{
    if (!caster)
        return 0;

    constexpr uint32 VERDANT_EMBRACE_SPELL_ID = 360995;
    constexpr float DEFAULT_COEFFICIENT = 2.85f;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(VERDANT_EMBRACE_SPELL_ID, caster->GetMap()->GetDifficultyID());
    int32 spellPower = caster->SpellBaseHealingBonusDone(SPELL_SCHOOL_MASK_NATURE);

    float bonusCoefficient = DEFAULT_COEFFICIENT;
    int32 baseHealing = 0;

    if (spellInfo)
    {
        for (SpellEffectInfo const& effect : spellInfo->GetEffects())
        {
            if (effect.IsEffect(SPELL_EFFECT_HEAL))
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

    // Apply Preservation mastery (Lifebinder) - scales with target missing health
    float masteryBonus = GetEvokerMasteryBonus(caster, target, true);
    if (target)
    {
        float missingHealthPct = (100.0f - target->GetHealthPct()) / 100.0f;
        masteryBonus *= (1.0f + missingHealthPct * 0.5f);  // Up to 50% bonus on low health targets
    }
    healing *= (1.0f + masteryBonus);

    // Apply critical strike chance
    float critChance = caster->GetRatingBonusValue(CR_CRIT_SPELL) / 100.0f;
    healing *= (1.0f + critChance * 0.5f);

    // Apply spell healing bonus modifier
    if (spellInfo && target)
    {
        for (SpellEffectInfo const& effect : spellInfo->GetEffects())
        {
            if (effect.IsEffect(SPELL_EFFECT_HEAL))
            {
                healing = static_cast<float>(caster->SpellHealingBonusDone(target, spellInfo,
                    static_cast<int32>(healing), HEAL, effect, 1, nullptr, nullptr));
                break;
            }
        }
    }

    return static_cast<uint32>(std::max(0.0f, healing));
}'''

if old_verdant_embrace in content:
    content = content.replace(old_verdant_embrace, new_verdant_embrace)
    print("CalculateVerdantEmbraceHealing: REPLACED")
    replacements += 1
else:
    print("CalculateVerdantEmbraceHealing: NOT FOUND")

# 3. CalculateEchoHealing - Full implementation
old_echo_healing = '''uint32 EvokerCalculator::CalculateEchoHealing(Player* caster, ::Unit* target)
{
    // DESIGN NOTE: Calculate Echo healing for Preservation Evoker bots
    // Returns 300 as baseline periodic healing value
    // Full implementation should:
    // - Query spell power from caster stats
    // - Apply Echo coefficient (percentage of original heal)
    // - Factor in number of active Echoes on target
    // - Apply Preservation mastery and versatility
    // - Consider Echo duration and remaining heal count
    // Reference: WoW 11.2 Echo (spell ID 364343) - signature Preservation mechanic
    return 300;
}'''

new_echo_healing = '''uint32 EvokerCalculator::CalculateEchoHealing(Player* caster, ::Unit* target)
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

if old_echo_healing in content:
    content = content.replace(old_echo_healing, new_echo_healing)
    print("CalculateEchoHealing: REPLACED")
    replacements += 1
else:
    print("CalculateEchoHealing: NOT FOUND")

# 4. GetOptimalEmpowermentLevel - Full implementation
old_get_optimal = '''EmpowermentLevel EvokerCalculator::GetOptimalEmpowermentLevel(uint32 spellId, Player* caster, ::Unit* target)
{
    // DESIGN NOTE: Determine optimal empowerment level for Evoker empowered spells
    // Returns RANK_2 as balanced default empowerment
    // Full implementation should:
    // - Analyze current essence availability
    // - Calculate damage/healing scaling per rank
    // - Factor in number of targets for AoE spells
    // - Consider combat urgency (use higher rank for burst)
    // - Balance channel time vs immediate need
    // Reference: WoW 11.2 Empowered spells (Fire Breath, Eternity's Surge, Dream Breath, Spirit Bloom)
    return EmpowermentLevel::RANK_2;
}'''

new_get_optimal = '''EmpowermentLevel EvokerCalculator::GetOptimalEmpowermentLevel(uint32 spellId, Player* caster, ::Unit* target)
{
    if (!caster || !target)
        return EmpowermentLevel::RANK_1;

    // Get combat urgency factors
    float targetHealthPct = target->GetHealthPct();
    float casterHealthPct = caster->GetHealthPct();
    bool inDanger = casterHealthPct < 40.0f;
    bool targetDying = targetHealthPct < 20.0f;

    // Fast response if caster in danger or target about to die
    if (inDanger || targetDying)
        return EmpowermentLevel::RANK_1;

    // Get spec to determine healing vs damage priority
    ChrSpecializationEntry const* spec = caster->GetPrimarySpecializationEntry();
    bool isHealer = spec && spec->ID == 1468;  // Preservation

    // For healers, check group health state
    if (isHealer)
    {
        uint32 criticalAllies = 0;
        uint32 injuredAllies = 0;

        if (Group* group = caster->GetGroup())
        {
            for (auto const& member : group->GetMemberSlots())
            {
                if (Player* player = ObjectAccessor::FindPlayer(member.guid))
                {
                    float hp = player->GetHealthPct();
                    if (hp < 30.0f)
                        criticalAllies++;
                    else if (hp < 70.0f)
                        injuredAllies++;
                }
            }
        }

        // Emergency: fast heal
        if (criticalAllies >= 2)
            return EmpowermentLevel::RANK_1;

        // Multiple injured: medium empowerment for throughput
        if (injuredAllies >= 3)
            return EmpowermentLevel::RANK_2;

        // Light damage: max empowerment for efficiency
        if (injuredAllies >= 1)
            return EmpowermentLevel::RANK_3;

        // No urgency: full empowerment
        return EmpowermentLevel::RANK_4;
    }

    // For DPS specs, consider AoE target count
    uint32 nearbyEnemies = 0;
    float rangeSq = 30.0f * 30.0f;

    // Count enemies in range for AoE evaluation
    if (Map* map = caster->GetMap())
    {
        for (auto& itr : map->GetPlayers())
        {
            if (Unit* unit = itr.GetSource())
            {
                if (unit->IsHostileTo(caster) && unit->GetExactDistSq(target) <= rangeSq)
                    nearbyEnemies++;
            }
        }
    }

    // Large AoE: max empowerment for cleave value
    if (nearbyEnemies >= 5)
        return EmpowermentLevel::RANK_4;

    // Medium group: good empowerment
    if (nearbyEnemies >= 3)
        return EmpowermentLevel::RANK_3;

    // Small group: balanced empowerment
    if (nearbyEnemies >= 2)
        return EmpowermentLevel::RANK_2;

    // Single target: still use RANK_2 for decent damage
    return EmpowermentLevel::RANK_2;
}'''

if old_get_optimal in content:
    content = content.replace(old_get_optimal, new_get_optimal)
    print("GetOptimalEmpowermentLevel: REPLACED")
    replacements += 1
else:
    print("GetOptimalEmpowermentLevel: NOT FOUND")

# 5. CalculateEssenceEfficiency - Full implementation
old_essence_eff = '''float EvokerCalculator::CalculateEssenceEfficiency(uint32 spellId, Player* caster)
{
    // DESIGN NOTE: Calculate essence efficiency (damage or healing per essence point)
    // Returns 1.0f as neutral efficiency baseline
    // Full implementation should:
    // - Calculate total damage/healing from spell
    // - Divide by essence cost to get efficiency ratio
    // - Factor in AoE target count for cleave spells
    // - Consider cooldown availability and uptime
    // - Weight spenders vs generators differently
    // Reference: WoW 11.2 Essence resource system (max 5-6 based on talents)
    return 1.0f;
}'''

new_essence_eff = '''float EvokerCalculator::CalculateEssenceEfficiency(uint32 spellId, Player* caster)
{
    if (!caster)
        return 1.0f;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, caster->GetMap()->GetDifficultyID());
    if (!spellInfo)
        return 1.0f;

    // Get essence cost
    uint32 essenceCost = 0;
    auto powerCosts = spellInfo->CalcPowerCost(caster, spellInfo->GetSchoolMask());
    for (auto const& cost : powerCosts)
    {
        if (cost.Power == POWER_ESSENCE || cost.Power == POWER_MANA)
        {
            essenceCost = cost.Amount;
            break;
        }
    }

    // Generator spells have infinite efficiency
    if (essenceCost == 0)
        return 100.0f;

    // Calculate base damage/healing value
    float spellValue = 0.0f;
    int32 spellPower = caster->SpellBaseDamageBonusDone(spellInfo->GetSchoolMask());

    for (SpellEffectInfo const& effect : spellInfo->GetEffects())
    {
        if (effect.IsEffect(SPELL_EFFECT_SCHOOL_DAMAGE) || effect.IsEffect(SPELL_EFFECT_HEAL))
        {
            float baseValue = static_cast<float>(effect.CalcValue(caster, nullptr, nullptr));
            float coefficient = effect.BonusCoefficient > 0.0f ? effect.BonusCoefficient : 0.5f;
            spellValue = baseValue + (spellPower * coefficient);
            break;
        }
    }

    // Calculate efficiency = value per essence point
    float efficiency = spellValue / static_cast<float>(essenceCost);

    // Bonus efficiency for spells that hit multiple targets
    switch (spellId)
    {
        case EvokerAI::PYRE:
        case EvokerAI::FIRE_BREATH:
        case EvokerAI::ETERNITYS_SURGE:
        case EvokerAI::EMERALD_BLOSSOM:
        case EvokerAI::DREAM_BREATH:
        case EvokerAI::SPIRIT_BLOOM:
            efficiency *= 1.5f;  // AoE bonus
            break;
        case EvokerAI::DISINTEGRATE:
            efficiency *= 1.2f;  // Channeled bonus (sustained damage)
            break;
    }

    // Normalize to 0-100 scale where 50 is average
    return std::min(100.0f, efficiency / 100.0f * 50.0f);
}'''

if old_essence_eff in content:
    content = content.replace(old_essence_eff, new_essence_eff)
    print("CalculateEssenceEfficiency: REPLACED")
    replacements += 1
else:
    print("CalculateEssenceEfficiency: NOT FOUND")

# 6. CalculateBuffEfficiency - Full implementation
old_buff_eff = '''uint32 EvokerCalculator::CalculateBuffEfficiency(uint32 spellId, Player* caster, ::Unit* target)
{
    // DESIGN NOTE: Calculate buff efficiency for Augmentation Evoker support abilities
    // Returns 100 as baseline efficiency score
    // Full implementation should:
    // - Query target's DPS potential and role
    // - Calculate stat buff value (Ebon Might, Prescience)
    // - Factor in target's gear and primary stat scaling
    // - Prioritize high-DPS allies for maximum raid benefit
    // - Consider buff duration and overlap prevention
    // Reference: WoW 11.2 Augmentation spec buffs (Ebon Might, Prescience, Blistering Scales)
    return 100;
}'''

new_buff_eff = '''uint32 EvokerCalculator::CalculateBuffEfficiency(uint32 spellId, Player* caster, ::Unit* target)
{
    if (!caster || !target)
        return 0;

    Player* targetPlayer = target->ToPlayer();
    if (!targetPlayer)
        return 50;  // Non-player targets get base efficiency

    uint32 efficiency = 0;

    // Get target's spec for role determination
    ChrSpecializationEntry const* targetSpec = targetPlayer->GetPrimarySpecializationEntry();
    uint32 specId = targetSpec ? targetSpec->ID : 0;

    // Role-based priority (DPS > Tank > Healer for damage buffs)
    bool isDPS = false;
    bool isTank = false;
    bool isHealer = false;

    // Determine role from spec
    if (targetSpec)
    {
        uint32 role = targetSpec->Role;
        isDPS = (role == 0);      // SPEC_ROLE_DPS
        isTank = (role == 1);     // SPEC_ROLE_TANK
        isHealer = (role == 2);   // SPEC_ROLE_HEALER
    }

    // Base efficiency by role
    if (isDPS)
        efficiency = 100;
    else if (isTank)
        efficiency = 60;
    else if (isHealer)
        efficiency = 40;
    else
        efficiency = 50;

    // Modify by target's current DPS potential (approximated by attack power + spell power)
    float attackPower = targetPlayer->GetTotalAttackPowerValue(BASE_ATTACK);
    float spellPower = static_cast<float>(targetPlayer->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_ALL));
    float totalPower = std::max(attackPower, spellPower);

    // Scale efficiency by power level (higher geared players benefit more)
    float powerMultiplier = std::min(2.0f, totalPower / 5000.0f);
    efficiency = static_cast<uint32>(efficiency * powerMultiplier);

    // Check for existing buff to avoid overwriting
    switch (spellId)
    {
        case EvokerAI::EBON_MIGHT:
            if (target->HasAura(395152))  // Ebon Might aura
                efficiency = efficiency / 2;  // Reduced value if already buffed
            break;
        case EvokerAI::PRESCIENCE:
            if (target->HasAura(410089))  // Prescience aura
                efficiency = efficiency / 2;
            break;
    }

    // Bonus for targets with active cooldowns (Lust, Trinkets, etc.)
    if (target->HasAura(2825) ||   // Bloodlust
        target->HasAura(32182) ||  // Heroism
        target->HasAura(80353))    // Time Warp
    {
        efficiency = static_cast<uint32>(efficiency * 1.5f);
    }

    return std::min(200u, efficiency);  // Cap at 200
}'''

if old_buff_eff in content:
    content = content.replace(old_buff_eff, new_buff_eff)
    print("CalculateBuffEfficiency: REPLACED")
    replacements += 1
else:
    print("CalculateBuffEfficiency: NOT FOUND")

# 7. GetOptimalAugmentationTarget - Full implementation
old_aug_target = '''::Unit* EvokerCalculator::GetOptimalAugmentationTarget(Player* caster, const ::std::vector<::Unit*>& allies)
{
    // DESIGN NOTE: Select optimal ally target for Augmentation Evoker buffs
    // Returns first ally as fallback selection
    // Full implementation should:
    // - Calculate DPS potential for each ally
    // - Prioritize DPS specs over tanks/healers
    // - Check for existing buff duration to avoid overwriting
    // - Consider ally proximity and combat engagement
    // - Favor allies with cooldowns active for maximum value
    // Reference: WoW 11.2 Augmentation targeting priority (melee > ranged > support)
    return allies.empty() ? nullptr : allies[0];
}'''

new_aug_target = '''::Unit* EvokerCalculator::GetOptimalAugmentationTarget(Player* caster, const ::std::vector<::Unit*>& allies)
{
    if (!caster || allies.empty())
        return nullptr;

    ::Unit* bestTarget = nullptr;
    uint32 bestScore = 0;

    for (::Unit* ally : allies)
    {
        if (!ally || ally == caster || !ally->IsAlive())
            continue;

        Player* allyPlayer = ally->ToPlayer();
        if (!allyPlayer)
            continue;

        uint32 score = 0;

        // Get spec for role determination
        ChrSpecializationEntry const* spec = allyPlayer->GetPrimarySpecializationEntry();
        if (spec)
        {
            uint32 role = spec->Role;
            // Role priority: DPS (100) > Tank (40) > Healer (20)
            if (role == 0)       // DPS
                score += 100;
            else if (role == 1)  // Tank
                score += 40;
            else if (role == 2)  // Healer
                score += 20;
        }

        // Melee vs Ranged bonus (melee often does more damage with uptime)
        float distance = std::sqrt(ally->GetExactDistSq(caster->GetVictim()));
        if (distance < 8.0f && caster->GetVictim())
            score += 20;  // Melee bonus

        // Power scaling (higher geared = more value from buffs)
        float attackPower = allyPlayer->GetTotalAttackPowerValue(BASE_ATTACK);
        float spellPower = static_cast<float>(allyPlayer->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_ALL));
        float totalPower = std::max(attackPower, spellPower);
        score += static_cast<uint32>(totalPower / 100.0f);  // +1 per 100 power

        // Cooldown active bonus
        if (ally->HasAura(2825) ||   // Bloodlust
            ally->HasAura(32182) ||  // Heroism
            ally->HasAura(80353))    // Time Warp
        {
            score += 50;
        }

        // Penalty for already having Augmentation buffs
        if (ally->HasAura(395152))  // Ebon Might
            score = score / 2;
        if (ally->HasAura(410089))  // Prescience
            score = score / 2;

        // Penalty for low health (might die, wasting buff)
        if (ally->GetHealthPct() < 30.0f)
            score = score / 3;

        // In combat bonus
        if (ally->IsInCombat())
            score += 10;

        if (score > bestScore)
        {
            bestScore = score;
            bestTarget = ally;
        }
    }

    // Fallback to first alive DPS if no clear winner
    if (!bestTarget)
    {
        for (::Unit* ally : allies)
        {
            if (ally && ally != caster && ally->IsAlive())
            {
                bestTarget = ally;
                break;
            }
        }
    }

    return bestTarget;
}'''

if old_aug_target in content:
    content = content.replace(old_aug_target, new_aug_target)
    print("GetOptimalAugmentationTarget: REPLACED")
    replacements += 1
else:
    print("GetOptimalAugmentationTarget: NOT FOUND")

# 8. EmpowermentController::CalculateOptimalLevel - Full implementation
old_emp_calc = '''EmpowermentLevel EmpowermentController::CalculateOptimalLevel(uint32 spellId, ::Unit* target)
{
    // DESIGN NOTE: Calculate optimal empowerment rank for channeled spell
    // Returns RANK_2 as balanced default empowerment
    // Full implementation should:
    // - Analyze current combat situation urgency
    // - Calculate targets hit by AoE cone/radius
    // - Balance channel time vs immediate threat response
    // - Consider essence availability for follow-up casts
    // - Factor in movement requirements and interrupt risk
    // Reference: WoW 11.2 Empowerment system (1-4 ranks, ~1 sec per rank)
    return EmpowermentLevel::RANK_2;
}'''

new_emp_calc = '''EmpowermentLevel EmpowermentController::CalculateOptimalLevel(uint32 spellId, ::Unit* target)
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

if old_emp_calc in content:
    content = content.replace(old_emp_calc, new_emp_calc)
    print("EmpowermentController::CalculateOptimalLevel: REPLACED")
    replacements += 1
else:
    print("EmpowermentController::CalculateOptimalLevel: NOT FOUND")

# 9. EmpowermentController::ShouldEmpowerSpell - Full implementation
old_should_emp = '''bool EmpowermentController::ShouldEmpowerSpell(uint32 spellId)
{
    // DESIGN NOTE: Determine whether to cast spell at empowered level
    // Returns true as default behavior to always use empowerment
    // Full implementation should:
    // - Check if spell is on empowered spell list (Fire Breath, Eternity's Surge, etc.)
    // - Verify sufficient essence available for higher ranks
    // - Consider combat mobility requirements (avoid channeling during movement)
    // - Evaluate interrupt risk from enemy casters
    // - Balance empowerment vs quick-cast instant abilities
    // Reference: WoW 11.2 Empowered spells require channeling and cannot be used while moving
    return true;
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

if old_should_emp in content:
    content = content.replace(old_should_emp, new_should_emp)
    print("EmpowermentController::ShouldEmpowerSpell: REPLACED")
    replacements += 1
else:
    print("EmpowermentController::ShouldEmpowerSpell: NOT FOUND")

# 10. EchoController::GetBestEchoTarget - Full implementation
old_echo_target = '''::Unit* EchoController::GetBestEchoTarget() const
{
    // DESIGN NOTE: Select best ally target for Echo application (Preservation mechanic)
    // Returns nullptr as default (no valid target)
    // Full implementation should:
    // - Scan group members for missing Echo debuff
    // - Prioritize tanks and melee DPS (high sustained damage intake)
    // - Avoid targets at full health (wasted healing)
    // - Check for incoming damage patterns
    // - Consider target's role and expected damage taken
    // Reference: WoW 11.2 Echo (spell ID 364343) - applies HoT that duplicates certain heals
    return nullptr;
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

if old_echo_target in content:
    content = content.replace(old_echo_target, new_echo_target)
    print("EchoController::GetBestEchoTarget: REPLACED")
    replacements += 1
else:
    print("EchoController::GetBestEchoTarget: NOT FOUND")

# Write back
with open('src/modules/Playerbot/AI/ClassAI/Evokers/EvokerAI.cpp', 'w') as f:
    f.write(content)

print(f"\nTotal replacements: {replacements}")
print("File updated successfully")
