#!/usr/bin/env python3
"""
Fix DispelCoordinator.cpp placeholder implementation:
1. GetAdjustedPriority - Full spreading debuff detection with proximity analysis
"""

import re

# Read the file
with open('src/modules/Playerbot/AI/CombatBehaviors/DispelCoordinator.cpp', 'r') as f:
    content = f.read()

fixes_made = 0

# Fix 1: Replace the DESIGN NOTE section in GetAdjustedPriority with full implementation
old_spread_check = '''    // DESIGN NOTE: Simplified implementation for spreading debuff detection
    // Current behavior: Adds flat +1.0 priority if debuff has "spreads" flag
    // Full implementation should:
    // - Check nearby allies within spread radius for same debuff
    // - Calculate actual spread probability based on proximity
    // - Monitor debuff application timing to detect spread patterns
    // - Account for group composition (melee vs ranged clustering)
    // - Prioritize based on number of potential spread targets
    // Reference: Debuff spread mechanics, AOE debuff system
    if (spreads)
    {
        priority += 1.0f;  // Spreading debuffs are always higher priority
    }'''

new_spread_check = '''    // Full spreading debuff detection with proximity analysis
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

            // Count nearby allies and analyze clustering
            for (GroupReference& itr : group->GetMembers())
            {
                Player* member = itr.GetSource();
                if (!member || member == targetPlayer || member->isDead())
                    continue;

                float distSq = target->GetExactDistSq(member);
                if (distSq <= spreadRadius * spreadRadius)
                {
                    ++nearbyAllies;

                    // Check if they're melee (likely stacked)
                    uint32 specId = member->GetPrimarySpecializationEntry() ?
                                    member->GetPrimarySpecializationEntry()->ID : 0;
                    bool isMelee = IsMeleeSpec(specId);
                    if (isMelee)
                        ++nearbyMelee;

                    // Check if already infected with this debuff
                    if (member->HasAura(auraId))
                        ++alreadyInfected;
                }
            }

            // Calculate spread risk score
            float spreadRisk = 0.0f;

            // Base risk from number of nearby allies
            if (nearbyAllies >= 4)
                spreadRisk += 2.0f;  // High stacking = critical
            else if (nearbyAllies >= 2)
                spreadRisk += 1.5f;  // Medium stacking
            else if (nearbyAllies >= 1)
                spreadRisk += 1.0f;  // Some risk

            // Additional risk if melee are stacked (common in boss fights)
            if (nearbyMelee >= 2)
                spreadRisk += 0.5f;

            // Reduce priority if spread already occurred (damage already done)
            if (alreadyInfected >= 2)
                spreadRisk -= 0.5f;

            // Environmental spread factor - check if in combat (more movement = less spread)
            if (targetPlayer->IsInCombat())
            {
                // Active combat means movement, slightly less spread risk
                spreadRisk *= 0.9f;
            }

            // Apply spread risk to priority
            priority += ::std::max(0.5f, spreadRisk);

            // Extra priority if this is a chain-spreading debuff targeting a clump
            if (nearbyAllies >= 3 && stackCount <= 1)
            {
                // Fresh debuff on grouped targets = emergency
                priority += 1.0f;
            }
        }
        else
        {
            // Fallback if not in group - still add base spread priority
            priority += 1.0f;
        }
    }'''

if old_spread_check in content:
    content = content.replace(old_spread_check, new_spread_check)
    print("GetAdjustedPriority: REPLACED spread detection with full implementation")
    fixes_made += 1
else:
    print("GetAdjustedPriority: NOT FOUND")

# Need to add the IsMeleeSpec helper function before namespace closes
# Find a good spot - after the role detection helpers

old_role_helpers_end = '''    bool IsTank(Player const* p) { return IsPlayerTank(p); }
    bool IsHealer(Player const* p) { return IsPlayerHealer(p); }
}'''

new_role_helpers_end = '''    bool IsTank(Player const* p) { return IsPlayerTank(p); }
    bool IsHealer(Player const* p) { return IsPlayerHealer(p); }

    // Spec-aware melee detection for spread debuff analysis
    bool IsMeleeSpec(uint32 specId)
    {
        switch (specId)
        {
            // Warrior - All specs are melee
            case 71:  // Arms
            case 72:  // Fury
            case 73:  // Protection
            // Paladin - Ret and Prot are melee
            case 66:  // Protection
            case 70:  // Retribution
            // Hunter - Survival is melee
            case 255: // Survival
            // Rogue - All specs are melee
            case 259: // Assassination
            case 260: // Outlaw
            case 261: // Subtlety
            // Death Knight - All specs are melee
            case 250: // Blood
            case 251: // Frost
            case 252: // Unholy
            // Monk - WW and BM are melee
            case 268: // Brewmaster
            case 269: // Windwalker
            // Shaman - Enhancement is melee
            case 263: // Enhancement
            // Druid - Feral and Guardian are melee
            case 103: // Feral
            case 104: // Guardian
            // Demon Hunter - Both specs are melee
            case 577: // Havoc
            case 581: // Vengeance
                return true;
            default:
                return false;
        }
    }
}'''

if old_role_helpers_end in content:
    content = content.replace(old_role_helpers_end, new_role_helpers_end)
    print("IsMeleeSpec helper: ADDED")
    fixes_made += 1
else:
    print("IsMeleeSpec helper: Could not find insertion point")

# Write back
with open('src/modules/Playerbot/AI/CombatBehaviors/DispelCoordinator.cpp', 'w') as f:
    f.write(content)

print(f"\nTotal replacements made: {fixes_made}")
print("File updated successfully" if fixes_made > 0 else "No changes made")
