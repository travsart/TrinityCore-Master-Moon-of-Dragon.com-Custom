#!/usr/bin/env python3
"""
Fix DispelCoordinator.cpp - add stackCount variable declaration
The code references stackCount but didn't declare it.
Need to get the stack count from the aura on the target.
"""

# Read the file
with open('src/modules/Playerbot/AI/CombatBehaviors/DispelCoordinator.cpp', 'r') as f:
    content = f.read()

fixes_made = 0

# Fix: Add stackCount declaration at the beginning of the spreads block
# We need to get the aura from the target to determine stack count
old_spreads_block = '''    // Full spreading debuff detection with proximity analysis
    // Analyzes group clustering and potential spread targets
    if (spreads)
    {
        Player* targetPlayer = target->ToPlayer();
        if (targetPlayer && targetPlayer->GetGroup())
        {
            Group* group = targetPlayer->GetGroup();
            float spreadRadius = 8.0f; // Most spreading debuffs have 8-10 yard radius
            uint32 nearbyAllies = 0;
            uint32 nearbyMelee = 0;
            uint32 alreadyInfected = 0;'''

new_spreads_block = '''    // Full spreading debuff detection with proximity analysis
    // Analyzes group clustering and potential spread targets
    if (spreads)
    {
        Player* targetPlayer = target->ToPlayer();
        if (targetPlayer && targetPlayer->GetGroup())
        {
            Group* group = targetPlayer->GetGroup();
            float spreadRadius = 8.0f; // Most spreading debuffs have 8-10 yard radius
            uint32 nearbyAllies = 0;
            uint32 nearbyMelee = 0;
            uint32 alreadyInfected = 0;

            // Get the aura's current stack count for fresh debuff detection
            uint8 stackCount = 1;
            if (Aura* aura = target->GetAura(auraId))
                stackCount = aura->GetStackAmount();'''

if old_spreads_block in content:
    content = content.replace(old_spreads_block, new_spreads_block)
    print("stackCount declaration: ADDED")
    fixes_made += 1
else:
    print("stackCount declaration: NOT FOUND - checking for partial match")
    # Try a more flexible approach
    if "// Full spreading debuff detection with proximity analysis" in content:
        print("  Found the spread detection section, looking for where to add stackCount...")
    else:
        print("  Spread detection section not found")

# Write back
with open('src/modules/Playerbot/AI/CombatBehaviors/DispelCoordinator.cpp', 'w') as f:
    f.write(content)

print(f"\nTotal replacements made: {fixes_made}")
print("File updated successfully" if fixes_made > 0 else "No changes made")
