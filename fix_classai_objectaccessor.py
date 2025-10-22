#!/usr/bin/env python3
"""
Phase 5F: Replace all unsafe ObjectAccessor calls in ClassAI files
Replaces GetUnit() and GetCreature() calls with spatial grid validation pattern
"""

import os
import re
from pathlib import Path

# Base directory for ClassAI files
BASE_DIR = Path(r"C:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI")

# Pattern to match unsafe ObjectAccessor calls
PATTERNS = {
    'GetUnit': re.compile(
        r'(\s*)(\w+\*?\s+\w+\s*=\s*)?ObjectAccessor::GetUnit\(\*([^,]+),\s*([^)]+)\);',
        re.MULTILINE
    ),
    'GetCreature': re.compile(
        r'(\s*)(\w+\*?\s+\w+\s*=\s*)?ObjectAccessor::GetCreature\(\*([^,]+),\s*([^)]+)\);',
        re.MULTILINE
    ),
}

def add_include_if_missing(content, filepath):
    """Add SpatialGridQueryHelpers include if not present"""
    if 'SpatialGridQueryHelpers.h' in content:
        return content

    # Find the last #include directive
    include_pattern = re.compile(r'(#include\s+[<"].*?[>"])', re.MULTILINE)
    includes = list(include_pattern.finditer(content))

    if includes:
        last_include = includes[-1]
        insert_pos = last_include.end()

        # Calculate relative path depth based on file location
        rel_path = filepath.relative_to(BASE_DIR)
        depth = len(rel_path.parts) - 1  # -1 because filename doesn't count
        prefix = "../" * (depth + 3)  # +3 for AI/ClassAI/... to Spatial

        new_include = f'\n#include "{prefix}Spatial/SpatialGridQueryHelpers.h"  // PHASE 5F: Thread-safe queries'
        content = content[:insert_pos] + new_include + content[insert_pos:]

    return content

def replace_get_unit(match):
    """Replace ObjectAccessor::GetUnit with spatial grid pattern"""
    indent = match.group(1)
    var_decl = match.group(2) if match.group(2) else ''
    bot_ref = match.group(3).strip()
    guid = match.group(4).strip()

    # Extract variable name from declaration
    if var_decl:
        var_name = var_decl.split('=')[0].strip().split()[-1].replace('*', '').strip()
    else:
        return match.group(0)  # Skip if no variable declaration

    replacement = f"""{indent}// PHASE 5F: Thread-safe spatial grid validation
{indent}auto snapshot_{var_name} = SpatialGridQueryHelpers::FindCreatureByGuid({bot_ref}, {guid});
{indent}{var_decl.strip()}nullptr;
{indent}
{indent}if (snapshot_{var_name})
{indent}{{
{indent}    {var_name} = ObjectAccessor::GetUnit(*{bot_ref}, {guid});
{indent}}}"""

    return replacement

def replace_get_creature(match):
    """Replace ObjectAccessor::GetCreature with spatial grid pattern"""
    indent = match.group(1)
    var_decl = match.group(2) if match.group(2) else ''
    bot_ref = match.group(3).strip()
    guid = match.group(4).strip()

    # Extract variable name from declaration
    if var_decl:
        var_name = var_decl.split('=')[0].strip().split()[-1].replace('*', '').strip()
    else:
        return match.group(0)  # Skip if no variable declaration

    replacement = f"""{indent}// PHASE 5F: Thread-safe spatial grid validation
{indent}auto snapshot_{var_name} = SpatialGridQueryHelpers::FindCreatureByGuid({bot_ref}, {guid});
{indent}{var_decl.strip()}nullptr;
{indent}
{indent}if (snapshot_{var_name})
{indent}{{
{indent}    {var_name} = ObjectAccessor::GetCreature(*{bot_ref}, {guid});
{indent}}}"""

    return replacement

def process_file(filepath):
    """Process a single file to replace ObjectAccessor calls"""
    print(f"Processing: {filepath.relative_to(BASE_DIR)}")

    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    original_content = content

    # Add include if needed
    content = add_include_if_missing(content, filepath)

    # Replace GetUnit calls
    content = PATTERNS['GetUnit'].sub(replace_get_unit, content)

    # Replace GetCreature calls
    content = PATTERNS['GetCreature'].sub(replace_get_creature, content)

    if content != original_content:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"  [MODIFIED]")
        return True
    else:
        print(f"  [NO CHANGES]")
        return False

def main():
    """Main execution"""
    print("=" * 80)
    print("Phase 5F: ClassAI ObjectAccessor Migration")
    print("=" * 80)

    files_to_process = [
        "ClassAI.cpp",
        "ClassAI_Refactored.cpp",
        "CombatSpecializationBase.cpp",
        "DeathKnights/DeathKnightAI.cpp",
        "DeathKnights/FrostSpecialization.cpp",
        "DeathKnights/UnholySpecialization.cpp",
        "DemonHunters/DemonHunterAI.cpp",
        "Druids/DruidSpecialization.cpp",
        "Evokers/EvokerAI.cpp",
        "Evokers/EvokerSpecialization.cpp",
        "Hunters/HunterAI.cpp",
        "Hunters/SurvivalSpecialization.cpp",
        "Mages/MageAI.cpp",
        "Mages/MageAI_Specialization.cpp",
        "Monks/MonkAI.cpp",
        "Monks/MonkSpecialization.cpp",
        "Paladins/PaladinAI.cpp",
        "Rogues/RogueAI.cpp",
        "Shamans/ElementalSpecialization.cpp",
        "Shamans/EnhancementSpecialization.cpp",
        "Shamans/ShamanAI.cpp",
        "Warlocks/AfflictionSpecialization.cpp",
        "Warlocks/DestructionSpecialization.cpp",
        "Warlocks/WarlockAI.cpp",
        "Warlocks/WarlockAI_Specialization.cpp",
        "Warriors/FurySpecialization.cpp",
        "Warriors/ProtectionSpecialization.cpp",
        "Warriors/WarriorAI.cpp",
        "Warriors/WarriorSpecialization.cpp",
    ]

    modified_count = 0
    for rel_path in files_to_process:
        filepath = BASE_DIR / rel_path
        if filepath.exists():
            if process_file(filepath):
                modified_count += 1
        else:
            print(f"[WARNING] File not found: {rel_path}")

    print("=" * 80)
    print(f"Phase 5F Complete: {modified_count} files modified")
    print("=" * 80)

if __name__ == "__main__":
    main()
