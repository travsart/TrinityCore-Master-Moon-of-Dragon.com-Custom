#!/usr/bin/env python3
"""
Enterprise-Grade ClassAI Header Cleanup Script V2
==================================================

Comprehensive removal of old dual-support specialization system with:
- Enhanced pattern matching (catches ALL old system code)
- Type recreation from deleted headers
- Forward declaration removal
- Method declaration cleanup
- Validation and detailed logging

Created: 2025-10-23
Purpose: Complete ClassAI old system removal after agent analysis
"""

import re
import os
from pathlib import Path
from typing import Dict, List, Tuple

# =============================================================================
# TYPE DEFINITIONS TO RECREATE (from Agent 1 analysis)
# =============================================================================

TYPE_DEFINITIONS = {
    'DruidAI.h': '''
    // Druid forms for shapeshift management
    enum class DruidForm : uint8
    {
        CASTER = 0,      // Humanoid/Caster form
        HUMANOID = 0,    // Alias for CASTER
        BEAR = 1,
        CAT = 2,
        AQUATIC = 3,
        TRAVEL = 4,
        MOONKIN = 5,
        TREE_OF_LIFE = 6,
        FLIGHT = 7
    };
''',

    'ShamanAI.h': '''
    // Totem management types
    enum class TotemType : uint8
    {
        EARTH = 0,
        FIRE = 1,
        WATER = 2,
        AIR = 3,
        MAX_TOTEMS = 4
    };

    enum class TotemBehavior : uint8
    {
        AGGRESSIVE = 0,
        DEFENSIVE = 1,
        PASSIVE = 2
    };

    struct TotemInfo
    {
        uint32 spellId;
        TotemType type;
        TotemBehavior behavior;
        uint32 duration;
        uint32 cooldown;
        uint32 lastCast;
        ::Unit* totemUnit;

        TotemInfo() : spellId(0), type(TotemType::EARTH), behavior(TotemBehavior::AGGRESSIVE),
                     duration(0), cooldown(0), lastCast(0), totemUnit(nullptr) {}

        bool IsActive() const { return totemUnit != nullptr; }
        bool IsOnCooldown(uint32 currentTime) const { return (currentTime - lastCast) < cooldown; }
    };

    struct WeaponImbue
    {
        uint32 spellId;
        uint32 mainHandEnchant;
        uint32 offHandEnchant;
        uint32 duration;
        uint32 lastApplied;

        WeaponImbue() : spellId(0), mainHandEnchant(0), offHandEnchant(0), duration(0), lastApplied(0) {}

        bool IsActive(uint32 currentTime) const { return (currentTime - lastApplied) < duration; }
    };
''',

    'WarriorAI.h': '''
    // Warrior stance management
    enum class WarriorStance : uint8
    {
        NONE = 0,
        BATTLE = 1,
        DEFENSIVE = 2,
        BERSERKER = 3
    };
'''
}

# =============================================================================
# PATTERN DEFINITIONS (Enhanced from script v1)
# =============================================================================

class OldSystemPatterns:
    """Comprehensive regex patterns for old specialization system code"""

    # Forward declarations (21 instances across 7 files)
    FORWARD_DECLARATIONS = [
        r'^\s*class\s+AfflictionSpecialization;\s*$',
        r'^\s*class\s+DemonologySpecialization;\s*$',
        r'^\s*class\s+DestructionSpecialization;\s*$',
        r'^\s*class\s+ArmsSpecialization;\s*$',
        r'^\s*class\s+FurySpecialization;\s*$',
        r'^\s*class\s+ProtectionSpecialization;\s*$',
        r'^\s*class\s+BalanceSpecialization;\s*$',
        r'^\s*class\s+FeralSpecialization;\s*$',
        r'^\s*class\s+GuardianSpecialization;\s*$',
        r'^\s*class\s+RestorationSpecialization;\s*$',
        r'^\s*class\s+ElementalSpecialization;\s*$',
        r'^\s*class\s+EnhancementSpecialization;\s*$',
        r'^\s*class\s+HolySpecialization;\s*$',
        r'^\s*class\s+DisciplineSpecialization;\s*$',
        r'^\s*class\s+ShadowSpecialization;\s*$',
        r'^\s*class\s+BeastMasterySpecialization;\s*$',
        r'^\s*class\s+MarksmanshipSpecialization;\s*$',
        r'^\s*class\s+SurvivalSpecialization;\s*$',
        r'^\s*class\s+ArcaneSpecialization;\s*$',
        r'^\s*class\s+FireSpecialization;\s*$',
        r'^\s*class\s+FrostSpecialization;\s*$',
        r'^\s*class\s+BrewmasterSpecialization;\s*$',
        r'^\s*class\s+MistweaverSpecialization;\s*$',
        r'^\s*class\s+WindwalkerSpecialization;\s*$',
        r'^\s*class\s+RetributionSpecialization;\s*$',
        r'^\s*class\s+AssassinationSpecialization;\s*$',
        r'^\s*class\s+OutlawSpecialization;\s*$',
        r'^\s*class\s+SubtletySpecialization;\s*$',
        r'^\s*class\s+HavocSpecialization;\s*$',
        r'^\s*class\s+VengeanceSpecialization;\s*$',
        r'^\s*class\s+DevastationSpecialization;\s*$',
        r'^\s*class\s+PreservationSpecialization;\s*$',
        r'^\s*class\s+AugmentationSpecialization;\s*$',
        r'^\s*class\s+BloodSpecialization;\s*$',
        r'^\s*class\s+FrostDeathKnightSpecialization;\s*$',
        r'^\s*class\s+UnholySpecialization;\s*$',
    ]

    # Member variables (unique_ptr and plain enum)
    MEMBER_VARIABLES = [
        r'^\s+std::unique_ptr<\w+Specialization>\s+_specialization;\s*$',
        r'^\s+\w+Spec\s+_specialization;\s*$',  # Plain enum (like EvokerAI.h)
        r'^\s+\w+Spec\s+_currentSpec;\s*$',
        r'^\s+\w+Spec\s+_detectedSpec;\s*$',
    ]

    # Method declarations
    METHOD_DECLARATIONS = [
        r'^\s+void\s+InitializeSpecialization\(\);\s*$',
        r'^\s+void\s+DetectSpecialization\(\);\s*$',
        r'^\s+\w+Spec\s+DetectCurrentSpecialization\(\)(\s+const)?;\s*$',
        r'^\s+\w+Spec\s+GetCurrentSpecialization\(\)(\s+const)?;\s*$',
    ]

    # Comment blocks
    COMMENT_BLOCKS = [
        r'^\s*//\s*Specialization management.*$',
        r'^\s*//\s*Old dual-support system.*$',
    ]

# =============================================================================
# HEADER PROCESSOR
# =============================================================================

class HeaderProcessor:
    """Processes individual *AI.h headers for cleanup and type recreation"""

    def __init__(self, file_path: Path):
        self.file_path = file_path
        self.file_name = file_path.name
        self.changes: List[str] = []
        self.lines_removed = 0
        self.lines_added = 0

    def process(self) -> Tuple[str, bool]:
        """Process the header file and return (new_content, was_modified)"""

        with open(self.file_path, 'r', encoding='utf-8') as f:
            content = f.read()
            original_content = content

        # Step 1: Remove forward declarations
        content = self._remove_forward_declarations(content)

        # Step 2: Remove member variables
        content = self._remove_member_variables(content)

        # Step 3: Remove method declarations
        content = self._remove_method_declarations(content)

        # Step 4: Remove comment blocks
        content = self._remove_comment_blocks(content)

        # Step 5: Add missing type definitions
        content = self._add_type_definitions(content)

        was_modified = (content != original_content)

        return content, was_modified

    def _remove_forward_declarations(self, content: str) -> str:
        """Remove forward declarations of old specialization classes"""

        lines = content.split('\n')
        new_lines = []

        for line in lines:
            removed = False
            for pattern in OldSystemPatterns.FORWARD_DECLARATIONS:
                if re.match(pattern, line):
                    self.changes.append(f"Removed forward declaration: {line.strip()}")
                    self.lines_removed += 1
                    removed = True
                    break

            if not removed:
                new_lines.append(line)

        return '\n'.join(new_lines)

    def _remove_member_variables(self, content: str) -> str:
        """Remove _specialization and related member variables"""

        lines = content.split('\n')
        new_lines = []

        for line in lines:
            removed = False
            for pattern in OldSystemPatterns.MEMBER_VARIABLES:
                if re.match(pattern, line):
                    self.changes.append(f"Removed member variable: {line.strip()}")
                    self.lines_removed += 1
                    removed = True
                    break

            if not removed:
                new_lines.append(line)

        return '\n'.join(new_lines)

    def _remove_method_declarations(self, content: str) -> str:
        """Remove old system method declarations"""

        lines = content.split('\n')
        new_lines = []

        for line in lines:
            removed = False
            for pattern in OldSystemPatterns.METHOD_DECLARATIONS:
                if re.match(pattern, line):
                    self.changes.append(f"Removed method declaration: {line.strip()}")
                    self.lines_removed += 1
                    removed = True
                    break

            if not removed:
                new_lines.append(line)

        return '\n'.join(new_lines)

    def _remove_comment_blocks(self, content: str) -> str:
        """Remove comment blocks related to specialization system"""

        lines = content.split('\n')
        new_lines = []

        for line in lines:
            removed = False
            for pattern in OldSystemPatterns.COMMENT_BLOCKS:
                if re.match(pattern, line):
                    self.changes.append(f"Removed comment: {line.strip()}")
                    self.lines_removed += 1
                    removed = True
                    break

            if not removed:
                new_lines.append(line)

        return '\n'.join(new_lines)

    def _add_type_definitions(self, content: str) -> str:
        """Add missing type definitions from TYPE_DEFINITIONS"""

        if self.file_name not in TYPE_DEFINITIONS:
            return content

        # Find the location to insert types (before "private:" section)
        lines = content.split('\n')
        insert_index = -1

        for i, line in enumerate(lines):
            if re.match(r'^private:\s*$', line):
                insert_index = i
                break

        if insert_index == -1:
            # Try to find "protected:" instead
            for i, line in enumerate(lines):
                if re.match(r'^protected:\s*$', line):
                    insert_index = i
                    break

        if insert_index == -1:
            self.changes.append(f"WARNING: Could not find insertion point for type definitions")
            return content

        # Insert type definitions
        type_def = TYPE_DEFINITIONS[self.file_name]
        type_lines = type_def.split('\n')

        # Insert before the private/protected section
        new_lines = lines[:insert_index] + type_lines + lines[insert_index:]

        self.changes.append(f"Added type definitions ({len(type_lines)} lines)")
        self.lines_added += len(type_lines)

        return '\n'.join(new_lines)

    def get_summary(self) -> str:
        """Get a summary of changes made to this file"""

        if not self.changes:
            return f"  [OK] No changes needed"

        summary = f"  [MODIFIED] {len(self.changes)} changes:\n"
        for change in self.changes:
            summary += f"    - {change}\n"
        summary += f"  Lines removed: {self.lines_removed}, Lines added: {self.lines_added}"

        return summary

# =============================================================================
# MAIN EXECUTION
# =============================================================================

def main():
    """Main execution function"""

    print("=" * 80)
    print("Enterprise-Grade ClassAI Header Cleanup V2")
    print("=" * 80)
    print()

    # Define all header files to process
    base_path = Path(r"c:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI")

    headers = [
        base_path / "DeathKnights" / "DeathKnightAI.h",
        base_path / "DemonHunters" / "DemonHunterAI.h",
        base_path / "Druids" / "DruidAI.h",
        base_path / "Evokers" / "EvokerAI.h",
        base_path / "Hunters" / "HunterAI.h",
        base_path / "Mages" / "MageAI.h",
        base_path / "Monks" / "MonkAI.h",
        base_path / "Paladins" / "PaladinAI.h",
        base_path / "Priests" / "PriestAI.h",
        base_path / "Rogues" / "RogueAI.h",
        base_path / "Shamans" / "ShamanAI.h",
        base_path / "Warlocks" / "WarlockAI.h",
        base_path / "Warriors" / "WarriorAI.h",
    ]

    files_processed = 0
    files_modified = 0
    total_lines_removed = 0
    total_lines_added = 0

    for header_path in headers:
        if not header_path.exists():
            print(f"WARNING: {header_path} does not exist, skipping...")
            continue

        print(f"Processing {header_path.name}...")

        processor = HeaderProcessor(header_path)
        new_content, was_modified = processor.process()

        if was_modified:
            # Write the modified content
            with open(header_path, 'w', encoding='utf-8') as f:
                f.write(new_content)

            files_modified += 1
            total_lines_removed += processor.lines_removed
            total_lines_added += processor.lines_added

        print(processor.get_summary())
        print()

        files_processed += 1

    print("=" * 80)
    print(f"Files Processed: {files_processed}")
    print(f"Files Modified: {files_modified}")
    print(f"Total Lines Removed: {total_lines_removed}")
    print(f"Total Lines Added: {total_lines_added}")
    print("=" * 80)

if __name__ == "__main__":
    main()
