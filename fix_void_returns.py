#!/usr/bin/env python3
"""
Fix void functions that have 'return false;' when they should have 'return;'
"""

import re
from pathlib import Path

FILE = Path(r"C:\TrinityBots\TrinityCore\src\modules\Playerbot\Lifecycle\DeathRecoveryManager.cpp")

# Void functions from header
VOID_FUNCTIONS = {
    "OnDeath", "OnResurrection", "Update",
    "HandleJustDied", "HandleReleasingSpirit", "HandleGhostDeciding",
    "HandleRunningToCorpse", "HandleAtCorpse", "HandleFindingSpiritHealer",
    "HandleMovingToSpiritHealer", "HandleAtSpiritHealer", "HandleResurrecting",
    "HandleResurrectionFailed", "HandleResurrectionFailure", "UpdateCorpseDistance"
}

def fix_void_returns():
    with open(FILE, 'r', encoding='utf-8', errors='ignore') as f:
        lines = f.readlines()

    result = []
    current_function = None
    in_void_function = False

    for line in lines:
        # Check if we're entering a function
        func_match = re.match(r'^\s*(void|bool|int|uint32|float|double|std::\w+|Creature\*|Player\*)\s+DeathRecoveryManager::(\w+)\s*\(', line)
        if func_match:
            return_type = func_match.group(1)
            function_name = func_match.group(2)
            current_function = function_name
            in_void_function = (return_type == "void" and function_name in VOID_FUNCTIONS)

        # Fix returns in void functions
        if in_void_function and 'return false;' in line:
            line = line.replace('return false;', 'return;')

        result.append(line)

    with open(FILE, 'w', encoding='utf-8') as f:
        f.writelines(result)

    print(f"Fixed void function returns in {FILE.name}")

if __name__ == "__main__":
    fix_void_returns()
