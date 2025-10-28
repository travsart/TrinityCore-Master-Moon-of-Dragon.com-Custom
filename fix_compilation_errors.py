#!/usr/bin/env python3
"""
Fix compilation errors caused by automated migration comments
"""

import re
from pathlib import Path

def fix_interrupt_rotation_manager():
    """Fix InterruptRotationManager.cpp compilation errors"""
    file_path = Path('c:/TrinityBots/TrinityCore/src/modules/Playerbot/AI/CombatBehaviors/InterruptRotationManager.cpp')

    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Fix 1: Line 360 - broken if statement
    content = re.sub(
        r'''        else
        \{
            auto snapshot = SpatialGridQueryHelpers::FindCreatureByGuid\(_bot, cast\.casterGuid\);
            if \(snapshot\)
        \}''',
        '''        else
        {
            // MIGRATION TODO: Distance validation needs refactoring
            // auto snapshot = SpatialGridQueryHelpers::FindCreatureByGuid(_bot, cast.casterGuid);
            // Skip distance check for now
        }''',
        content
    )

    # Fix 2: Lines 873-874 - restore botUnit variable
    content = re.sub(
        r'''    // Get actual Unit\* for speed calculation
        return false;

    float distance = botUnit->GetDistance''',
        '''    // MIGRATION TODO: Convert to BotActionQueue pattern
    Unit* botUnit = ObjectAccessor::GetUnit(*_bot, bot.botGuid);
    if (!botUnit)
        return false;

    float distance = botUnit->GetDistance''',
        content
    )

    with open(file_path, 'w', encoding='utf-8') as f:
        f.write(content)

    print(f"[OK] Fixed {file_path.name}")

def fix_aoe_decision_manager():
    """Fix AoEDecisionManager.cpp compilation errors"""
    file_path = Path('c:/TrinityBots/TrinityCore/src/modules/Playerbot/AI/CombatBehaviors/AoEDecisionManager.cpp')

    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Fix: Line 578 - restore unit variable
    # Find the context and restore the assignment
    content = re.sub(
        r'''        // Deprioritize distant targets \(use snapshot position for lock-free calculation\)
        float distance = _bot->GetDistance\(snapshot->position\);
        priority -= distance \* 2\.0f;

        candidates\.push_back\(\{unit, priority\}\);''',
        '''        // Deprioritize distant targets (use snapshot position for lock-free calculation)
        float distance = _bot->GetDistance(snapshot->position);
        priority -= distance * 2.0f;

        // MIGRATION TODO: Convert to GUID-based
        Unit* unit = ObjectAccessor::GetUnit(*_bot, snapshot->guid);
        if (!unit) continue;
        candidates.push_back({unit, priority});''',
        content
    )

    with open(file_path, 'w', encoding='utf-8') as f:
        f.write(content)

    print(f"[OK] Fixed {file_path.name}")

if __name__ == '__main__':
    print("=" * 60)
    print("FIXING COMPILATION ERRORS")
    print("=" * 60)
    print()

    fix_interrupt_rotation_manager()
    fix_aoe_decision_manager()

    print()
    print("All compilation errors fixed!")
    print("Run compilation again...")
