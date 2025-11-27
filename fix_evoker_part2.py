#!/usr/bin/env python3
import sys

# Read the file
with open('src/modules/Playerbot/AI/ClassAI/Evokers/EvokerAI.cpp', 'r') as f:
    content = f.read()

# CalculateLivingFlameDamage
old_living_flame = '''uint32 EvokerCalculator::CalculateLivingFlameDamage(Player* caster, ::Unit* target)
{
    // DESIGN NOTE: Calculate Living Flame damage for Evoker bots
    // Returns 800 as baseline damage value
    // Full implementation should:
    // - Query spell power and attack power from caster
    // - Apply dual-scaling coefficient (spell + attack power)
    // - Factor in target resistances
    // - Apply Devastation mastery (Giantkiller) bonus if applicable
    // Reference: WoW 11.2 Living Flame (spell ID 361469)
    return 800;
}'''

new_living_flame = '''uint32 EvokerCalculator::CalculateLivingFlameDamage(Player* caster, ::Unit* target)
{
    if (!caster || !target)
        return 0;

    constexpr uint32 LIVING_FLAME_SPELL_ID = 361469;
    constexpr float SPELL_POWER_COEFFICIENT = 0.60f;
    constexpr float ATTACK_POWER_COEFFICIENT = 0.30f;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(LIVING_FLAME_SPELL_ID, caster->GetMap()->GetDifficultyID());
    int32 spellPower = caster->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_FIRE);
    float attackPower = caster->GetTotalAttackPowerValue(BASE_ATTACK);

    float bonusCoefficient = SPELL_POWER_COEFFICIENT;
    float apCoefficient = ATTACK_POWER_COEFFICIENT;
    int32 baseDamage = 0;

    if (spellInfo)
    {
        for (SpellEffectInfo const& effect : spellInfo->GetEffects())
        {
            if (effect.IsEffect(SPELL_EFFECT_SCHOOL_DAMAGE))
            {
                baseDamage = effect.CalcValue(caster, nullptr, target);
                if (effect.BonusCoefficient > 0.0f)
                    bonusCoefficient = effect.BonusCoefficient;
                if (effect.BonusCoefficientFromAP > 0.0f)
                    apCoefficient = effect.BonusCoefficientFromAP;
                break;
            }
        }
    }

    float damage = static_cast<float>(baseDamage) + (spellPower * bonusCoefficient) + (attackPower * apCoefficient);
    float versatility = caster->GetRatingBonusValue(CR_VERSATILITY_DAMAGE_DONE);
    damage *= (1.0f + versatility / 100.0f);

    float masteryBonus = GetEvokerMasteryBonus(caster, target, false);
    damage *= (1.0f + masteryBonus);

    if (spellInfo)
    {
        for (SpellEffectInfo const& effect : spellInfo->GetEffects())
        {
            if (effect.IsEffect(SPELL_EFFECT_SCHOOL_DAMAGE))
            {
                damage = static_cast<float>(caster->SpellDamageBonusDone(target, spellInfo,
                    static_cast<int32>(damage), SPELL_DIRECT_DAMAGE, effect, 1, nullptr, nullptr));
                break;
            }
        }
    }

    return static_cast<uint32>(std::max(0.0f, damage));
}'''

if old_living_flame in content:
    content = content.replace(old_living_flame, new_living_flame)
    print("Living Flame: REPLACED")
else:
    print("Living Flame: NOT FOUND")

# CalculateEmpoweredSpellDamage
old_empowered = '''uint32 EvokerCalculator::CalculateEmpoweredSpellDamage(uint32 spellId, EmpowermentLevel level, Player* caster, ::Unit* target)
{
    uint32 baseDamage = 1000;
    return baseDamage * (static_cast<uint32>(level) + 1); // Scales with empowerment level
}'''

new_empowered = '''uint32 EvokerCalculator::CalculateEmpoweredSpellDamage(uint32 spellId, EmpowermentLevel level, Player* caster, ::Unit* target)
{
    if (!caster || !target || level == EmpowermentLevel::NONE)
        return 0;

    static const float EMPOWERMENT_MULTIPLIERS[] = { 1.0f, 1.0f, 1.4f, 1.8f, 2.2f };

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, caster->GetMap()->GetDifficultyID());
    if (!spellInfo)
        return 0;

    int32 spellPower = caster->SpellBaseDamageBonusDone(spellInfo->GetSchoolMask());
    int32 baseDamage = 0;
    float bonusCoefficient = 0.8f;

    for (SpellEffectInfo const& effect : spellInfo->GetEffects())
    {
        if (effect.IsEffect(SPELL_EFFECT_SCHOOL_DAMAGE))
        {
            baseDamage = effect.CalcValue(caster, nullptr, target);
            if (effect.BonusCoefficient > 0.0f)
                bonusCoefficient = effect.BonusCoefficient;
            break;
        }
    }

    float damage = static_cast<float>(baseDamage) + (spellPower * bonusCoefficient);

    uint8 levelIndex = static_cast<uint8>(level);
    if (levelIndex <= 4)
        damage *= EMPOWERMENT_MULTIPLIERS[levelIndex];

    float versatility = caster->GetRatingBonusValue(CR_VERSATILITY_DAMAGE_DONE);
    damage *= (1.0f + versatility / 100.0f);

    float masteryBonus = GetEvokerMasteryBonus(caster, target, false);
    damage *= (1.0f + masteryBonus);

    return static_cast<uint32>(std::max(0.0f, damage));
}'''

if old_empowered in content:
    content = content.replace(old_empowered, new_empowered)
    print("Empowered Spell: REPLACED")
else:
    print("Empowered Spell: NOT FOUND")

# Write back
with open('src/modules/Playerbot/AI/ClassAI/Evokers/EvokerAI.cpp', 'w') as f:
    f.write(content)
print("File updated successfully")
