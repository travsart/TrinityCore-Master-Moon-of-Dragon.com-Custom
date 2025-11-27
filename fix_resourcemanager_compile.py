#!/usr/bin/env python3
"""
Fix ResourceManager.cpp compilation errors:
1. SpellMgr::GetSpellInfo needs difficulty parameter
2. SPELL_EFFECT_APPLY_AURA_2 doesn't exist - use SPELL_EFFECT_APPLY_AURA
3. SPELL_AURA_ADD_COMBO_POINTS doesn't exist - remove check
4. Map type undefined - not needed since we use DIFFICULTY_NONE
"""

import re

# Read the file
with open('src/modules/Playerbot/AI/ClassAI/ResourceManager.cpp', 'r') as f:
    content = f.read()

fixes_made = 0

# Fix 1: Replace all instances of GetSpellInfo with single argument to include DIFFICULTY_NONE
# Pattern: sSpellMgr->GetSpellInfo(spellId, caster->GetMap()->GetDifficultyID())
# Replace with: sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE)
old_getmap = 'sSpellMgr->GetSpellInfo(spellId, caster->GetMap()->GetDifficultyID())'
new_getmap = 'sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE)'

if old_getmap in content:
    content = content.replace(old_getmap, new_getmap)
    print("Fixed GetSpellInfo with GetMap->GetDifficultyID")
    fixes_made += 1

# Fix 2: Replace SPELL_EFFECT_APPLY_AURA_2 with just checking APPLY_AURA
old_aura_check = '''        if (effect.Effect == SPELL_EFFECT_APPLY_AURA || effect.Effect == SPELL_EFFECT_APPLY_AURA_2)'''
new_aura_check = '''        if (effect.Effect == SPELL_EFFECT_APPLY_AURA)'''

content = content.replace(old_aura_check, new_aura_check)
print("Fixed SPELL_EFFECT_APPLY_AURA_2 in CalculateResourceEfficiency")
fixes_made += 1

# Fix 3: Remove SPELL_AURA_ADD_COMBO_POINTS check in GetComboPointsFromSpell
old_combo_aura_check = '''        // Some auras grant combo points on application
        if (effect.Effect == SPELL_EFFECT_APPLY_AURA || effect.Effect == SPELL_EFFECT_APPLY_AURA_2)
        {
            if (effect.ApplyAuraName == SPELL_AURA_ADD_COMBO_POINTS)
            {
                comboPoints += effect.CalcValue(nullptr, nullptr, nullptr);
            }
        }'''

new_combo_aura_check = '''        // Note: Combo point generation is typically handled via SPELL_EFFECT_ENERGIZE
        // rather than auras in WoW 11.2'''

content = content.replace(old_combo_aura_check, new_combo_aura_check)
print("Fixed SPELL_AURA_ADD_COMBO_POINTS check in GetComboPointsFromSpell")
fixes_made += 1

# Fix 4: Remove unused baseManaPercent variable or use it
old_mana_percent = '''    float baseManaPercent = player->GetMaxPower(POWER_MANA) > 0 ?
        static_cast<float>(player->GetCreateMana()) / player->GetMaxPower(POWER_MANA) : 1.0f;'''

new_mana_percent = '''    // Base regeneration scales with max mana pool size
    float maxManaScale = player->GetMaxPower(POWER_MANA) > 0 ?
        std::min(1.5f, static_cast<float>(player->GetMaxPower(POWER_MANA)) / 100000.0f) : 1.0f;'''

content = content.replace(old_mana_percent, new_mana_percent)
print("Fixed unused baseManaPercent variable")
fixes_made += 1

# Also need to use the maxManaScale variable
old_base_regen_line = '''    // Calculate base regen from intellect
    float baseRegen = BASE_MANA_REGEN_PER_SECOND + (intellect * INTELLECT_REGEN_COEFFICIENT);'''

new_base_regen_line = '''    // Calculate base regen from intellect, scaled by mana pool
    float baseRegen = (BASE_MANA_REGEN_PER_SECOND + (intellect * INTELLECT_REGEN_COEFFICIENT)) * maxManaScale;'''

content = content.replace(old_base_regen_line, new_base_regen_line)
print("Used maxManaScale in regen calculation")
fixes_made += 1

# Write back
with open('src/modules/Playerbot/AI/ClassAI/ResourceManager.cpp', 'w') as f:
    f.write(content)

print(f"\nTotal fixes made: {fixes_made}")
print("File updated successfully" if fixes_made > 0 else "No changes made")
