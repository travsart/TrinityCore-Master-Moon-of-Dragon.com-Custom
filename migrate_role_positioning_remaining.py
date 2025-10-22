#!/usr/bin/env python3
"""
Migrate remaining 6 MotionMaster calls in RoleBasedCombatPositioning.cpp
"""

import re

filepath = r'c:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\Combat\RoleBasedCombatPositioning.cpp'

# Read file
with open(filepath, 'r', encoding='utf-8') as f:
    content = f.read()

# Define the replacement pattern for simple MovePoint calls
def make_replacement(player_var, pos_var, description):
    return f'''// PHASE 6B: Use Movement Arbiter with ROLE_POSITIONING priority (170)
        BotAI* botAI = dynamic_cast<BotAI*>({player_var}->GetAI());
        if (botAI && botAI->GetMovementArbiter())
        {{
            bool accepted = botAI->RequestPointMovement(
                PlayerBotMovementPriority::ROLE_POSITIONING,
                {pos_var},
                "{description}",
                "RoleBasedCombatPositioning");

            if (!accepted)
            {{
                TC_LOG_TRACE("playerbot.movement.arbiter",
                    "RoleBasedCombatPositioning: {description} rejected for {{}} - higher priority active",
                    {player_var}->GetName());
            }}
        }}
        else
        {{
            // FALLBACK
            {player_var}->GetMotionMaster()->MovePoint(0, {pos_var});
        }}'''

# Pattern 1: rangedDPS[i]->GetMotionMaster()->MovePoint(0, pos);
content = re.sub(
    r'(\s+)rangedDPS\[i\]->GetMotionMaster\(\)->MovePoint\(0, pos\);',
    lambda m: m.group(1) + make_replacement('rangedDPS[i]', 'pos', 'Ranged DPS optimal positioning'),
    content
)

# Pattern 2: dps->GetMotionMaster()->MovePoint(0, safePos);
content = re.sub(
    r'(\s+)dps->GetMotionMaster\(\)->MovePoint\(0, safePos\);',
    lambda m: m.group(1) + make_replacement('dps', 'safePos', 'DPS safe positioning'),
    content
)

# Pattern 3: dps->GetMotionMaster()->MovePoint(0, flankPos);
content = re.sub(
    r'(\s+)dps->GetMotionMaster\(\)->MovePoint\(0, flankPos\);',
    lambda m: m.group(1) + make_replacement('dps', 'flankPos', 'DPS flank positioning'),
    content
)

# Pattern 4: tank->GetMotionMaster()->MovePoint(0, pos); (second occurrence)
# This is tricky - need to match the right one
content = re.sub(
    r'(\s+)(//\s*Move\s+player\s+to\s+adjusted.*?\n\s+)tank->GetMotionMaster\(\)->MovePoint\(0, pos\);',
    lambda m: m.group(1) + m.group(2) + make_replacement('tank', 'pos', 'Tank adjusted positioning'),
    content,
    flags=re.DOTALL
)

# Pattern 5: healer->GetMotionMaster()->MovePoint(0, pos); (second occurrence)
content = re.sub(
    r'(\s+)(//\s*Move\s+player\s+to\s+adjusted.*?\n\s+)healer->GetMotionMaster\(\)->MovePoint\(0, pos\);',
    lambda m: m.group(1) + m.group(2) + make_replacement('healer', 'pos', 'Healer adjusted positioning'),
    content,
    flags=re.DOTALL
)

# Pattern 6: bot->GetMotionMaster()->MovePoint(1, safeZone, true);
content = re.sub(
    r'(\s+)bot->GetMotionMaster\(\)->MovePoint\(1, safeZone, true\);(\s+//\s*High priority movement)',
    lambda m: m.group(1) + make_replacement('bot', 'safeZone', 'Safe zone emergency positioning') + m.group(2),
    content
)

# Write back
with open(filepath, 'w', encoding='utf-8') as f:
    f.write(content)

print("✅ Migrated remaining 6 calls in RoleBasedCombatPositioning.cpp")
