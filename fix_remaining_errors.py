#!/usr/bin/env python3
"""
Fix Remaining Compilation Errors (Phase 2)
==========================================

Fixes the last ~150 compilation errors after Phase 1 commit:
1. Warrior *Refactored.h - WarriorStance member variables and methods
2. Shaman/Warlock/Rogue - ChrSpecialization enum comparison issues
3. Rogue - Missing spell constants
4. Druid - GetCurrentForm() method conflicts
5. Minor override specifiers and method signatures
"""

import re
from pathlib import Path

def fix_warrior_refactored_headers():
    """Remove WarriorStance member variables and DetermineOptimalStance method from Warrior *Refactored.h"""

    files = [
        Path(r"c:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI\Warriors\ArmsWarriorRefactored.h"),
        Path(r"c:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI\Warriors\FuryWarriorRefactored.h"),
        Path(r"c:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI\Warriors\ProtectionWarriorRefactored.h"),
    ]

    for file_path in files:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()

        original_content = content

        # Remove WarriorStance member variables
        content = re.sub(r'^\s*WarriorStance\s+_currentStance;.*$', '', content, flags=re.MULTILINE)
        content = re.sub(r'^\s*WarriorStance\s+_preferredStance;.*$', '', content, flags=re.MULTILINE)

        # Remove DetermineOptimalStance method declaration
        content = re.sub(r'^\s*WarriorStance\s+DetermineOptimalStance\(\)(\s+override)?;.*$', '', content, flags=re.MULTILINE)

        # Clean up multiple blank lines
        content = re.sub(r'\n\n\n+', '\n\n', content)

        if content != original_content:
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(content)
            print(f"[OK] Fixed: {file_path.name}")
        else:
            print(f"[SKIP] {file_path.name} (no changes needed)")


def fix_chrspecialization_comparisons():
    """Fix ChrSpecialization enum comparisons in ShamanAI.cpp, WarlockAI.cpp, RogueAI.cpp"""

    files = [
        Path(r"c:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI\Shamans\ShamanAI.cpp"),
        Path(r"c:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI\Warlocks\WarlockAI.cpp"),
        Path(r"c:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI\Rogues\RogueAI.cpp"),
    ]

    for file_path in files:
        if not file_path.exists():
            print(f"[SKIP] {file_path.name} (file not found)")
            continue

        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()

        original_content = content

        # Fix: spec == 262 -> static_cast<uint32>(spec) == 262
        # Match: spec == <number> OR spec != <number>
        content = re.sub(
            r'\bspec\s*(==|!=)\s*(\d+)',
            r'static_cast<uint32>(spec) \1 \2',
            content
        )

        if content != original_content:
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(content)
            print(f"[OK] Fixed: {file_path.name}")
        else:
            print(f"[SKIP] {file_path.name} (no changes needed)")


def add_rogue_spell_constants():
    """Add missing Rogue spell constants to AssassinationRogueRefactored.h"""

    file_path = Path(r"c:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI\Rogues\AssassinationRogueRefactored.h")

    if not file_path.exists():
        print(f"[SKIP] {file_path.name} (file not found)")
        return

    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Check if constants already exist
    if 'GARROTE' in content and 'constexpr uint32' in content:
        print(f"[SKIP] {file_path.name} (constants already exist)")
        return

    # Find the class definition and add constants after the opening brace
    class_match = re.search(r'class\s+AssassinationRogueRefactored[^{]*\{', content)
    if not class_match:
        print(f"  ERROR: Could not find class definition in {file_path.name}")
        return

    spell_constants = """
public:
    // Assassination Rogue Spell IDs
    static constexpr uint32 GARROTE = 703;
    static constexpr uint32 RUPTURE = 1943;
    static constexpr uint32 ENVENOM = 32645;
    static constexpr uint32 VENDETTA = 79140;
    static constexpr uint32 MUTILATE = 1329;
    static constexpr uint32 FAN_OF_KNIVES = 51723;
    static constexpr uint32 CRIMSON_TEMPEST = 121411;
    static constexpr uint32 EXSANGUINATE = 200806;
    static constexpr uint32 KINGSBANE = 192759;

"""

    insert_pos = class_match.end()
    content = content[:insert_pos] + spell_constants + content[insert_pos:]

    with open(file_path, 'w', encoding='utf-8') as f:
        f.write(content)

    print(f"[OK] Fixed: {file_path.name} (added spell constants)")


def fix_druid_getCurrentForm():
    """Fix DruidAI GetCurrentForm() method signature conflict"""

    file_path = Path(r"c:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI\Druids\DruidAI.cpp")

    if not file_path.exists():
        print(f"[SKIP] {file_path.name} (file not found)")
        return

    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    original_content = content

    # Fix: Make sure GetCurrentForm() returns DruidForm
    # Look for the method implementation and ensure return type
    content = re.sub(
        r'\buint32\s+DruidAI::GetCurrentForm\(\)',
        r'DruidForm DruidAI::GetCurrentForm()',
        content
    )

    # Also fix any returns that cast to uint32
    content = re.sub(
        r'return\s+static_cast<uint32>\((_bot->GetShapeshiftForm\(\))\)',
        r'return static_cast<DruidForm>(\1)',
        content
    )

    if content != original_content:
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"[OK] Fixed: {file_path.name}")
    else:
        print(f"[SKIP] {file_path.name} (no changes needed)")


def fix_toteminfo_isactive():
    """Add IsActive() method to TotemInfo struct if missing"""

    file_path = Path(r"c:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI\Shamans\ShamanAI.h")

    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Check if IsActive already exists
    if 'bool IsActive()' in content:
        print(f"[SKIP] ShamanAI.h (TotemInfo::IsActive already exists)")
        return

    # Find TotemInfo struct and add IsActive method
    totem_info_match = re.search(r'struct\s+TotemInfo\s*\{[^}]*\}', content, re.DOTALL)
    if not totem_info_match:
        print(f"  ERROR: Could not find TotemInfo struct")
        return

    totem_struct = totem_info_match.group(0)

    # Add IsActive() before the closing brace
    if 'IsActive' not in totem_struct:
        new_struct = totem_struct.replace(
            '};',
            '    bool IsActive() const { return spellId != 0 && (GetMSTime() - lastUsed) < duration; }\n};'
        )
        content = content.replace(totem_struct, new_struct)

        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(content)

        print(f"[OK] Fixed: ShamanAI.h (added TotemInfo::IsActive)")
    else:
        print(f"[SKIP] ShamanAI.h (TotemInfo::IsActive exists)")


def main():
    print("=" * 70)
    print("Fix Remaining Compilation Errors (Phase 2)")
    print("=" * 70)
    print()

    print("1. Fixing Warrior *Refactored.h files...")
    fix_warrior_refactored_headers()
    print()

    print("2. Fixing ChrSpecialization enum comparisons...")
    fix_chrspecialization_comparisons()
    print()

    print("3. Adding Rogue spell constants...")
    add_rogue_spell_constants()
    print()

    print("4. Fixing DruidAI GetCurrentForm()...")
    fix_druid_getCurrentForm()
    print()

    print("5. Adding TotemInfo::IsActive()...")
    fix_toteminfo_isactive()
    print()

    print("=" * 70)
    print("All Phase 2 fixes applied!")
    print("=" * 70)

if __name__ == "__main__":
    main()
