#!/usr/bin/env python3
"""
Mass refactoring script to fix C++ template method visibility issues
in PlayerBot specialization files by adding this-> prefixes.
Version 2: More comprehensive pattern matching
"""

import os
import re
import sys
from pathlib import Path
from typing import List, Tuple, Dict

class TemplateRefactorer:
    def __init__(self, base_path: str):
        self.base_path = Path(base_path)

        # Comprehensive list of methods that need this-> prefix
        # Based on analysis of the template base classes
        self.method_patterns = [
            # Core bot methods
            r'\bGetBot\s*\(',
            r'\bGetPlayer\s*\(',
            r'\bGetGroup\s*\(',

            # Spell/ability methods
            r'\bCastSpell\s*\(',
            r'\bCanCastSpell\s*\(',
            r'\bIsSpellReady\s*\(',
            r'\bCanUseAbility\s*\(',
            r'\bHasSpell\s*\(',
            r'\bGetSpellCooldown\s*\(',

            # Combat methods
            r'\bGetEnemiesInRange\s*\(',
            r'\bIsBehindTarget\s*\(',
            r'\bIsInMeleeRange\s*\(',
            r'\bGetTarget\s*\(',
            r'\bCanAttack\s*\(',
            r'\bIsInCombat\s*\(',

            # Resource methods
            r'\bGetCurrentResource\s*\(',
            r'\bGetMaxResource\s*\(',
            r'\bHasResource\s*\(',
            r'\bConsumeResource\s*\(',
            r'\bGenerateResource\s*\(',

            # Position methods
            r'\bGetDistance\s*\(',
            r'\bMoveTo\s*\(',
            r'\bStopMoving\s*\(',
            r'\bFaceTarget\s*\(',

            # Class-specific methods (from base templates)
            r'\bUpdateRotation\s*\(',
            r'\bUpdateBuffs\s*\(',
            r'\bUpdateCooldowns\s*\(',
            r'\bOnCombatStart\s*\(',
            r'\bOnCombatEnd\s*\(',

            # Warrior-specific (example for other classes)
            r'\bHasTacticalMastery\s*\(',
            r'\bSwitchToStance\s*\(',
            r'\bDetermineOptimalStance\s*\(',
            r'\bGetCurrentStance\s*\(',

            # Common utility methods
            r'\bLog\s*\(',
            r'\bDebugLog\s*\(',
            r'\bIsValidTarget\s*\(',
            r'\bHasAura\s*\(',
            r'\bGetAuraStacks\s*\(',
            r'\bGetAuraDuration\s*\(',
        ]

        # Member variables that need this-> prefix
        self.member_patterns = [
            r'\b_resource\.',
            r'\b_cooldowns\.',
            r'\b_buffs\.',
            r'\b_debuffs\.',
            r'\b_position\.',
            r'\b_target\.',
        ]

        self.files_processed = []
        self.errors = []
        self.total_replacements = 0

    def get_refactored_files(self) -> Dict[str, List[str]]:
        """Get all refactored files organized by class."""
        return {
            "Warriors": [
                "Warriors/ArmsWarriorRefactored.h",
                "Warriors/FuryWarriorRefactored.h",
                "Warriors/ProtectionWarriorRefactored.h"
            ],
            "Warlocks": [
                "Warlocks/AfflictionWarlockRefactored.h",
                "Warlocks/DemonologyWarlockRefactored.h",
                "Warlocks/DestructionWarlockRefactored.h"
            ],
            "Shamans": [
                "Shamans/ElementalShamanRefactored.h",
                "Shamans/EnhancementShamanRefactored.h",
                "Shamans/RestorationShamanRefactored.h"
            ],
            "Rogues": [
                "Rogues/AssassinationRogueRefactored.h",
                "Rogues/OutlawRogueRefactored.h",
                "Rogues/SubtletyRogueRefactored.h"
            ],
            "Priests": [
                "Priests/DisciplinePriestRefactored.h",
                "Priests/HolyPriestRefactored.h",
                "Priests/ShadowPriestRefactored.h"
            ],
            "Paladins": [
                "Paladins/HolyPaladinRefactored.h",
                "Paladins/ProtectionPaladinRefactored.h",
                "Paladins/RetributionSpecializationRefactored.h"
            ],
            "Monks": [
                "Monks/BrewmasterMonkRefactored.h",
                "Monks/MistweaverMonkRefactored.h",
                "Monks/WindwalkerMonkRefactored.h"
            ],
            "Mages": [
                "Mages/ArcaneMageRefactored.h",
                "Mages/FireMageRefactored.h",
                "Mages/FrostMageRefactored.h"
            ],
            "Hunters": [
                "Hunters/BeastMasteryHunterRefactored.h",
                "Hunters/MarksmanshipHunterRefactored.h",
                "Hunters/SurvivalHunterRefactored.h"
            ],
            "Evokers": [
                "Evokers/DevastationEvokerRefactored.h",
                "Evokers/PreservationEvokerRefactored.h"
            ],
            "Druids": [
                "Druids/BalanceDruidRefactored.h",
                "Druids/FeralDruidRefactored.h",
                "Druids/GuardianDruidRefactored.h",
                "Druids/RestorationDruidRefactored.h"
            ],
            "DemonHunters": [
                "DemonHunters/HavocDemonHunterRefactored.h",
                "DemonHunters/VengeanceDemonHunterRefactored.h"
            ],
            "DeathKnights": [
                "DeathKnights/BloodDeathKnightRefactored.h",
                "DeathKnights/FrostDeathKnightRefactored.h",
                "DeathKnights/UnholyDeathKnightRefactored.h"
            ]
        }

    def is_in_comment(self, line: str, position: int) -> bool:
        """Check if position is within a comment."""
        # Check for line comment
        comment_pos = line.find('//')
        if comment_pos != -1 and position >= comment_pos:
            return True

        # Check for block comment (simplified, doesn't handle multi-line)
        block_start = line.find('/*')
        block_end = line.find('*/')
        if block_start != -1 and block_end != -1:
            if position >= block_start and position <= block_end + 2:
                return True

        return False

    def is_in_string(self, line: str, position: int) -> bool:
        """Check if position is within a string literal."""
        in_string = False
        in_char = False
        escape = False

        for i, char in enumerate(line):
            if i >= position:
                break

            if escape:
                escape = False
                continue

            if char == '\\':
                escape = True
                continue

            if char == '"' and not in_char:
                in_string = not in_string
            elif char == "'" and not in_string:
                in_char = not in_char

        return in_string or in_char

    def process_line(self, line: str) -> Tuple[str, int]:
        """Process a single line and return modified line with change count."""
        changes = 0
        result = line

        # Skip pure comment lines
        if line.strip().startswith('//') or line.strip().startswith('/*'):
            return line, 0

        # Process method calls
        for pattern in self.method_patterns:
            # Find all matches
            matches = list(re.finditer(pattern, result))

            # Process matches in reverse order to maintain positions
            for match in reversed(matches):
                pos = match.start()

                # Skip if already has this->
                if pos >= 6 and result[pos-6:pos] == 'this->':
                    continue

                # Skip if in comment or string
                if self.is_in_comment(result, pos) or self.is_in_string(result, pos):
                    continue

                # Add this-> prefix
                result = result[:pos] + 'this->' + result[pos:]
                changes += 1

        # Process member variables
        for pattern in self.member_patterns:
            matches = list(re.finditer(pattern, result))

            for match in reversed(matches):
                pos = match.start()

                # Skip if already has this->
                if pos >= 6 and result[pos-6:pos] == 'this->':
                    continue

                # Skip if in comment or string
                if self.is_in_comment(result, pos) or self.is_in_string(result, pos):
                    continue

                # Add this-> prefix
                result = result[:pos] + 'this->' + result[pos:]
                changes += 1

        return result, changes

    def process_file(self, relative_path: str) -> Tuple[bool, int]:
        """Process a single file and return success status and change count."""
        file_path = self.base_path / relative_path

        if not file_path.exists():
            self.errors.append(f"File not found: {file_path}")
            return False, 0

        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                lines = f.readlines()

            modified_lines = []
            total_changes = 0

            for line in lines:
                modified_line, changes = self.process_line(line)
                modified_lines.append(modified_line)
                total_changes += changes

            if total_changes > 0:
                with open(file_path, 'w', encoding='utf-8') as f:
                    f.writelines(modified_lines)
                self.files_processed.append(relative_path)
                print(f"[OK] Processed {relative_path}: {total_changes} replacements")
            else:
                print(f"[--] No changes needed in {relative_path}")

            return True, total_changes

        except Exception as e:
            self.errors.append(f"Error processing {file_path}: {str(e)}")
            return False, 0

    def process_class_batch(self, class_name: str, files: List[str]) -> bool:
        """Process all files for a given class."""
        print(f"\n{'='*60}")
        print(f"Processing {class_name} ({len(files)} files)")
        print('='*60)

        total_changes = 0
        all_success = True

        for file in files:
            success, changes = self.process_file(file)
            total_changes += changes
            self.total_replacements += changes
            if not success:
                all_success = False

        print(f"{class_name} Summary: {total_changes} total replacements")
        return all_success

    def run(self):
        """Execute the refactoring for all files."""
        print("Template Visibility Refactoring v2.0")
        print(f"Base path: {self.base_path}")
        print(f"Method patterns: {len(self.method_patterns)}")
        print(f"Member patterns: {len(self.member_patterns)}")

        all_files = self.get_refactored_files()
        total_classes = len(all_files)
        total_files = sum(len(files) for files in all_files.values())

        print(f"\nProcessing {total_files} files across {total_classes} classes")

        success_count = 0
        for class_name, files in all_files.items():
            if self.process_class_batch(class_name, files):
                success_count += 1

        print(f"\n{'='*60}")
        print("REFACTORING COMPLETE")
        print(f"Classes processed: {success_count}/{total_classes}")
        print(f"Files modified: {len(self.files_processed)}/{total_files}")
        print(f"Total replacements: {self.total_replacements}")

        if self.errors:
            print(f"\nErrors encountered: {len(self.errors)}")
            for error in self.errors[:5]:  # Show first 5 errors
                print(f"  - {error}")
            if len(self.errors) > 5:
                print(f"  ... and {len(self.errors) - 5} more")

        return len(self.errors) == 0


if __name__ == "__main__":
    # Set the base path for ClassAI directory
    base_path = r"C:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI"

    refactorer = TemplateRefactorer(base_path)
    success = refactorer.run()

    sys.exit(0 if success else 1)