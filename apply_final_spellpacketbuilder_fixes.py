#!/usr/bin/env python3
"""
Final SpellPacketBuilder compilation fixes
Fixes remaining 4 errors after third compilation attempt
"""

import re

def apply_cpp_fixes():
    filepath = r"c:\TrinityBots\TrinityCore\src\modules\Playerbot\Packets\SpellPacketBuilder.cpp"

    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    print("Applying fixes to SpellPacketBuilder.cpp...")

    # Fix 1: Add spellInfo parameter to ValidateGlobalCooldown function signature
    old_signature = r"SpellPacketBuilder::ValidationResult SpellPacketBuilder::ValidateGlobalCooldown\(\s*Player const\* caster,\s*BuildOptions const& options\)"
    new_signature = "SpellPacketBuilder::ValidationResult SpellPacketBuilder::ValidateGlobalCooldown(\n    Player const* caster,\n    SpellInfo const* spellInfo,\n    BuildOptions const& options)"

    content = re.sub(old_signature, new_signature, content, flags=re.MULTILINE)
    print("  OK Fixed ValidateGlobalCooldown function signature (added spellInfo parameter)")

    # Fix 2: Update call site at line 146 - add spellInfo parameter
    content = re.sub(
        r'result\.result = ValidateGlobalCooldown\(caster, options\);',
        'result.result = ValidateGlobalCooldown(caster, spellInfo, options);',
        content
    )
    print("  OK Fixed ValidateGlobalCooldown call site #1 (line 146)")

    # Fix 3: Update call site at line 443 - add spellInfo parameter (validateOnly is BuildOptions)
    content = re.sub(
        r'result\.result = ValidateGlobalCooldown\(caster, validateOnly\);',
        'result.result = ValidateGlobalCooldown(caster, spellInfo, validateOnly);',
        content
    )
    print("  OK Fixed ValidateGlobalCooldown call site #2 (line 443)")

    # Fix 4: Fix Position coordinate assignment using Relocate instead of m_positionX/Y/Z
    old_position_code = r"""castRequest\.Target\.DstLocation->Location\.m_positionX = position->GetPositionX\(\);
        castRequest\.Target\.DstLocation->Location\.m_positionY = position->GetPositionY\(\);
        castRequest\.Target\.DstLocation->Location\.m_positionZ = position->GetPositionZ\(\);"""

    new_position_code = """castRequest.Target.DstLocation->Location.Relocate(position->GetPositionX(),
            position->GetPositionY(), position->GetPositionZ());"""

    content = re.sub(old_position_code, new_position_code, content, flags=re.MULTILINE)
    print("  OK Fixed Position coordinate assignment (use Relocate instead of m_positionX/Y/Z)")

    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(content)

    print("\nSUCCESS All SpellPacketBuilder.cpp fixes applied!")

def apply_header_fixes():
    filepath = r"c:\TrinityBots\TrinityCore\src\modules\Playerbot\Packets\SpellPacketBuilder.h"

    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    print("\nApplying fixes to SpellPacketBuilder.h...")

    # Fix: Update ValidateGlobalCooldown declaration in header
    old_declaration = r'static ValidationResult ValidateGlobalCooldown\(Player const\* caster, BuildOptions const& options\);'
    new_declaration = 'static ValidationResult ValidateGlobalCooldown(Player const* caster, SpellInfo const* spellInfo, BuildOptions const& options);'

    content = re.sub(old_declaration, new_declaration, content)
    print("  OK Fixed ValidateGlobalCooldown header declaration (added spellInfo parameter)")

    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(content)

    print("\nSUCCESS All SpellPacketBuilder.h fixes applied!")

def main():
    print("=" * 70)
    print("FINAL SPELLPACKETBUILDER COMPILATION FIXES")
    print("=" * 70)
    print("\nFixing 4 remaining compilation errors:\n")
    print("  1. Line 600: spellInfo undeclared in ValidateGlobalCooldown")
    print("  2. Line 748-750: m_positionX/Y/Z not members of TaggedPosition")
    print("\n" + "=" * 70 + "\n")

    apply_cpp_fixes()
    apply_header_fixes()

    print("\n" + "=" * 70)
    print("SUMMARY")
    print("=" * 70)
    print("\nSUCCESS All fixes applied successfully!\n")
    print("Changes made:")
    print("  - SpellPacketBuilder.cpp: ValidateGlobalCooldown signature + 2 call sites")
    print("  - SpellPacketBuilder.cpp: Position coordinate assignment with Relocate()")
    print("  - SpellPacketBuilder.h: ValidateGlobalCooldown declaration")
    print("\nNext step:")
    print("  - Recompile with: cmake --build . --config RelWithDebInfo --target playerbot")
    print("\n" + "=" * 70 + "\n")

if __name__ == "__main__":
    main()
