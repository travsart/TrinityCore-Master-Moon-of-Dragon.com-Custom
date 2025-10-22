#!/usr/bin/env python3
"""
Fix the final 6 Cell::Visit calls causing server hangs
Replace with lock-free spatial grid queries
"""

from pathlib import Path
import re

print("=" * 80)
print("FIXING FINAL 6 CELL::VISIT CALLS - SERVER HANG FIX")
print("=" * 80)

fixes = [
    {
        'file': r'C:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI\CombatSpecializationTemplates.h',
        'line': 479,
        'description': 'GetEnemiesInRange()',
        'old_code': '        Cell::VisitAllObjects(bot, searcher, range);',
        'new_code': '''        // DEADLOCK FIX: Use lock-free spatial grid instead of Cell::VisitAllObjects
        Map* map = bot->GetMap();
        if (map)
        {
            auto* spatialGrid = Playerbot::SpatialGridManager::Instance()->GetOrCreateGrid(map);
            if (spatialGrid)
            {
                auto guids = spatialGrid->QueryNearbyCreatures(*bot, range);
                for (ObjectGuid const& guid : guids)
                {
                    if (Creature* creature = ObjectAccessor::GetCreature(*bot, guid))
                    {
                        if (u_check(creature))
                            targets.push_back(creature);
                    }
                }
            }
        }'''
    },
    {
        'file': r'C:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI\CombatSpecializationTemplates.h',
        'line': 991,
        'description': 'GetEnemyClusterCenter()',
        'old_code': '        Cell::VisitAllObjects(bot, searcher, 40.0f);',
        'new_code': '''        // DEADLOCK FIX: Use lock-free spatial grid instead of Cell::VisitAllObjects
        Map* map = bot->GetMap();
        if (map)
        {
            auto* spatialGrid = Playerbot::SpatialGridManager::Instance()->GetOrCreateGrid(map);
            if (spatialGrid)
            {
                auto guids = spatialGrid->QueryNearbyCreatures(*bot, 40.0f);
                for (ObjectGuid const& guid : guids)
                {
                    if (Creature* creature = ObjectAccessor::GetCreature(*bot, guid))
                    {
                        if (checker(creature))
                            hostileUnits.push_back(creature);
                    }
                }
            }
        }'''
    },
    {
        'file': r'C:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI\DemonHunters\HavocDemonHunterRefactored.h',
        'line': 701,
        'description': 'CountEnemiesInCone()',
        'old_code': '        Cell::VisitAllObjects(this->GetBot(), searcher, range);',
        'new_code': '''        // DEADLOCK FIX: Use lock-free spatial grid instead of Cell::VisitAllObjects
        Map* map = this->GetBot()->GetMap();
        if (map)
        {
            auto* spatialGrid = Playerbot::SpatialGridManager::Instance()->GetOrCreateGrid(map);
            if (spatialGrid)
            {
                auto guids = spatialGrid->QueryNearbyCreatures(*this->GetBot(), range);
                for (ObjectGuid const& guid : guids)
                {
                    if (Creature* creature = ObjectAccessor::GetCreature(*this->GetBot(), guid))
                    {
                        if (checker(creature))
                            enemies.push_back(creature);
                    }
                }
            }
        }'''
    },
    {
        'file': r'C:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI\DemonHunters\HavocDemonHunterRefactored.h',
        'line': 726,
        'description': 'GetEnemiesInRangeVector()',
        'old_code': '        Cell::VisitAllObjects(this->GetBot(), searcher, range);',
        'new_code': '''        // DEADLOCK FIX: Use lock-free spatial grid instead of Cell::VisitAllObjects
        Map* map = this->GetBot()->GetMap();
        if (map)
        {
            auto* spatialGrid = Playerbot::SpatialGridManager::Instance()->GetOrCreateGrid(map);
            if (spatialGrid)
            {
                auto guids = spatialGrid->QueryNearbyCreatures(*this->GetBot(), range);
                for (ObjectGuid const& guid : guids)
                {
                    if (Creature* creature = ObjectAccessor::GetCreature(*this->GetBot(), guid))
                    {
                        if (checker(creature))
                            unitList.push_back(creature);
                    }
                }
            }
        }'''
    },
    {
        'file': r'C:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI\Hunters\SurvivalHunterRefactored.h',
        'line': 681,
        'description': 'ApplySerpentStingToMultiple()',
        'old_code': '        Cell::VisitAllObjects(this->GetBot(), searcher, 8.0f);',
        'new_code': '''        // DEADLOCK FIX: Use lock-free spatial grid instead of Cell::VisitAllObjects
        Map* map = this->GetBot()->GetMap();
        if (map)
        {
            auto* spatialGrid = Playerbot::SpatialGridManager::Instance()->GetOrCreateGrid(map);
            if (spatialGrid)
            {
                auto guids = spatialGrid->QueryNearbyCreatures(*this->GetBot(), 8.0f);
                for (ObjectGuid const& guid : guids)
                {
                    if (Creature* creature = ObjectAccessor::GetCreature(*this->GetBot(), guid))
                    {
                        if (checker(creature))
                            enemies.push_back(creature);
                    }
                }
            }
        }'''
    },
    {
        'file': r'C:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI\RoleSpecializations.h',
        'line': 488,
        'description': 'CountNearbyEnemies()',
        'old_code': '        Cell::VisitAllObjects(bot, u_searcher, 10.0f);',
        'new_code': '''        // DEADLOCK FIX: Use lock-free spatial grid instead of Cell::VisitAllObjects
        Map* map = bot->GetMap();
        if (map)
        {
            auto* spatialGrid = Playerbot::SpatialGridManager::Instance()->GetOrCreateGrid(map);
            if (spatialGrid)
            {
                auto guids = spatialGrid->QueryNearbyCreatures(*bot, 10.0f);
                for (ObjectGuid const& guid : guids)
                {
                    if (Creature* creature = ObjectAccessor::GetCreature(*bot, guid))
                    {
                        if (u_check(creature))
                            hostileUnits.push_back(creature);
                    }
                }
            }
        }'''
    }
]

# Check if includes need to be added
include_checks = {}
for fix in fixes:
    file_path = Path(fix['file'])
    if file_path not in include_checks:
        include_checks[file_path] = {
            'has_spatialgrid': False,
            'has_objectaccessor': False,
            'has_creature': False,
            'has_map': False
        }

for file_path in include_checks:
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()
        if 'SpatialGridManager.h' in content:
            include_checks[file_path]['has_spatialgrid'] = True
        if 'ObjectAccessor.h' in content:
            include_checks[file_path]['has_objectaccessor'] = True
        if 'Creature.h' in content:
            include_checks[file_path]['has_creature'] = True
        if 'Map.h' in content:
            include_checks[file_path]['has_map'] = True

# Apply fixes
total_fixes = 0
for fix in fixes:
    file_path = Path(fix['file'])

    print(f"\n[{total_fixes + 1}/6] Fixing {file_path.name} line {fix['line']}: {fix['description']}")

    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Add missing includes at the top (after #pragma once or first #include)
    includes_to_add = []
    if not include_checks[file_path]['has_spatialgrid']:
        includes_to_add.append('#include "../../../Spatial/SpatialGridManager.h"')
        include_checks[file_path]['has_spatialgrid'] = True
    if not include_checks[file_path]['has_objectaccessor']:
        includes_to_add.append('#include "ObjectAccessor.h"')
        include_checks[file_path]['has_objectaccessor'] = True
    if not include_checks[file_path]['has_creature']:
        includes_to_add.append('#include "Creature.h"')
        include_checks[file_path]['has_creature'] = True
    if not include_checks[file_path]['has_map']:
        includes_to_add.append('#include "Map.h"')
        include_checks[file_path]['has_map'] = True

    if includes_to_add:
        # Find the first #include and add after it
        include_match = re.search(r'(#include [^\n]+\n)', content)
        if include_match:
            insert_pos = include_match.end()
            includes_text = '\n'.join(includes_to_add) + '\n'
            content = content[:insert_pos] + includes_text + content[insert_pos:]

    # Replace the Cell::Visit call
    old_code_escaped = re.escape(fix['old_code'])
    if re.search(old_code_escaped, content):
        content = re.sub(old_code_escaped, fix['new_code'], content)

        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(content)

        print(f"  [OK] Fixed {fix['description']}")
        total_fixes += 1
    else:
        print(f"  [SKIP] Code pattern not found (may be already fixed)")

print("\n" + "=" * 80)
print(f"TOTAL: {total_fixes}/6 Cell::Visit calls replaced with spatial grid queries")
print("=" * 80)
print("\nNext steps:")
print("1. Rebuild playerbot module")
print("2. Test server - should no longer hang")
print("3. Commit with message: 'CRITICAL FIX: Eliminate final 6 Cell::Visit deadlocks'")
