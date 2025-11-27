#!/usr/bin/env python3
"""
Fix CooldownStackingOptimizer.h - add missing struct members
"""

# Read the file
with open('src/modules/Playerbot/AI/CombatBehaviors/CooldownStackingOptimizer.h', 'r') as f:
    content = f.read()

# Fix: Add masteryIncrease and healingIncrease to CooldownData struct
old_struct = '''    struct CooldownData
    {
        uint32 spellId;
        CooldownCategory category;
        uint32 cooldownMs;        // Cooldown duration in ms
        uint32 durationMs;         // Buff duration in ms
        float damageIncrease;      // Damage increase multiplier (1.0 = +100%)
        float hasteIncrease;       // Haste increase (0.3 = +30%)
        float critIncrease;        // Crit increase (0.2 = +20%)
        bool stacksWithOthers;     // Can stack with other cooldowns
        bool affectedByHaste;      // Cooldown affected by haste
        uint32 lastUsedTime;       // Last time this was used
        uint32 nextAvailable;      // Next time available

        CooldownData() : spellId(0), category(MAJOR_DPS), cooldownMs(0),
                        durationMs(0), damageIncrease(0.0f), hasteIncrease(0.0f),
                        critIncrease(0.0f), stacksWithOthers(true),
                        affectedByHaste(false), lastUsedTime(0), nextAvailable(0) {}
    };'''

new_struct = '''    struct CooldownData
    {
        uint32 spellId;
        CooldownCategory category;
        uint32 cooldownMs;        // Cooldown duration in ms
        uint32 durationMs;         // Buff duration in ms
        float damageIncrease;      // Damage increase multiplier (1.0 = +100%)
        float hasteIncrease;       // Haste increase (0.3 = +30%)
        float critIncrease;        // Crit increase (0.2 = +20%)
        float masteryIncrease;     // Mastery increase (0.1 = +10%)
        float healingIncrease;     // Healing increase multiplier (0.25 = +25%)
        bool stacksWithOthers;     // Can stack with other cooldowns
        bool affectedByHaste;      // Cooldown affected by haste
        uint32 lastUsedTime;       // Last time this was used
        uint32 nextAvailable;      // Next time available

        CooldownData() : spellId(0), category(MAJOR_DPS), cooldownMs(0),
                        durationMs(0), damageIncrease(0.0f), hasteIncrease(0.0f),
                        critIncrease(0.0f), masteryIncrease(0.0f), healingIncrease(0.0f),
                        stacksWithOthers(true), affectedByHaste(false),
                        lastUsedTime(0), nextAvailable(0) {}
    };'''

if old_struct in content:
    content = content.replace(old_struct, new_struct)
    print("CooldownData struct: UPDATED with masteryIncrease and healingIncrease")
else:
    print("CooldownData struct: NOT FOUND or already updated")

# Write back
with open('src/modules/Playerbot/AI/CombatBehaviors/CooldownStackingOptimizer.h', 'w') as f:
    f.write(content)

print("Header file updated successfully")
