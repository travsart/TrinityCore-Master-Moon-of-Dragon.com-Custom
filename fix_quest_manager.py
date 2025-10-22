#!/usr/bin/env python3
"""Fix QuestManager.cpp void function returns"""
import re
from pathlib import Path

file_path = Path(r"C:\TrinityBots\TrinityCore\src\modules\Playerbot\Game\QuestManager.cpp")

with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
    lines = f.readlines()

# Functions that are void
void_funcs = ['ScanGameObjectQuestGivers', 'UpdateObjectiveProgress', 'ShareGroupQuests',
              'ProcessIdlePhase', 'AcceptBestQuests', 'TurnInCompletedQuests', 'UpdateStatistics']

result = []
in_void_func = None
brace_depth = 0
fixes = 0

for i, line in enumerate(lines):
    # Check if we're entering a void function
    for func in void_funcs:
        if f'void QuestManager::{func}' in line:
            in_void_func = func
            brace_depth = 0
            break

    # Track brace depth
    brace_depth += line.count('{') - line.count('}')

    # Reset if we exit the function
    if brace_depth == 0 and in_void_func:
        in_void_func = None

    # Fix returns in void functions
    if in_void_func and re.search(r'\breturn\s+(false|true)\s*;', line):
        line = re.sub(r'return\s+(false|true)\s*;', 'return;', line)
        fixes += 1

    result.append(line)

with open(file_path, 'w', encoding='utf-8') as f:
    f.writelines(result)

print(f'Fixed {fixes} void function returns in QuestManager.cpp')
