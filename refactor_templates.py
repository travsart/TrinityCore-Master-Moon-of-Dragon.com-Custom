#!/usr/bin/env python3
"""
Mass refactoring script to fix C++ template method visibility issues
in PlayerBot specialization files by adding this-> prefixes.
"""

import os
import re
import sys
from pathlib import Path
from typing import List, Tuple, Dict

class TemplateRefactorer:
    def __init__(self, base_path: str):
        self.base_path = Path(base_path)
        self.replacements = [
            # Method calls that need this-> prefix
            (r'\bGetBot\(\)', r'this->GetBot()'),
            (r'\bCastSpell\(', r'this->CastSpell('),
            (r'\bCanCastSpell\(', r'this->CanCastSpell('),
            (r'\bGetEnemiesInRange\(', r'this->GetEnemiesInRange('),
            (r'\bIsBehindTarget\(', r'this->IsBehindTarget('),
            (r'\bIsSpellReady\(', r'this->IsSpellReady('),
            (r'\bCanUseAbility\(', r'this->CanUseAbility('),
            # Member access
            (r'\b_resource\.', r'this->_resource.'),
        ]
        self.files_processed = []
        self.errors = []

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

    def should_skip_line(self, line: str) -> bool:
        """Check if line should skip replacement (comments, strings)."""
        stripped = line.strip()
        # Skip pure comment lines
        if stripped.startswith('//'):
            return True
        # Skip lines that are likely in string literals (very basic check)
        if '"' in line and not 'this->' in line:
            # More complex check would be needed for multi-line strings
            in_string = False
            escape_next = False
            for char in line:
                if escape_next:
                    escape_next = False
                    continue
                if char == '\\':
                    escape_next = True
                    continue
                if char == '"':
                    in_string = not in_string
            # If we end in a string, might be multi-line
            if in_string:
                return True
        return False

    def process_file(self, relative_path: str) -> Tuple[bool, int]:
        """Process a single file and return success status and change count."""
        file_path = self.base_path / relative_path

        if not file_path.exists():
            self.errors.append(f"File not found: {file_path}")
            return False, 0

        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()

            original_content = content
            changes = 0

            # Process line by line for better control
            lines = content.split('\n')
            processed_lines = []

            for line in lines:
                if self.should_skip_line(line):
                    processed_lines.append(line)
                    continue

                new_line = line
                for pattern, replacement in self.replacements:
                    # Check if replacement already exists to avoid double-replacement
                    if 'this->' + pattern.replace(r'\b', '').replace('\\(', '(') in new_line:
                        continue

                    before = new_line
                    new_line = re.sub(pattern, replacement, new_line)
                    if before != new_line:
                        changes += 1

                processed_lines.append(new_line)

            content = '\n'.join(processed_lines)

            if content != original_content:
                with open(file_path, 'w', encoding='utf-8') as f:
                    f.write(content)
                self.files_processed.append(relative_path)
                print(f"[OK] Processed {relative_path}: {changes} replacements")
                return True, changes
            else:
                print(f"- No changes needed in {relative_path}")
                return True, 0

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
            if not success:
                all_success = False

        print(f"\n{class_name} Summary: {total_changes} total replacements")
        return all_success

    def run(self):
        """Execute the refactoring for all files."""
        print("Starting Template Visibility Refactoring")
        print(f"Base path: {self.base_path}")

        all_files = self.get_refactored_files()
        total_classes = len(all_files)
        total_files = sum(len(files) for files in all_files.values())

        print(f"Found {total_files} files across {total_classes} classes")

        success_count = 0
        for class_name, files in all_files.items():
            if self.process_class_batch(class_name, files):
                success_count += 1

        print(f"\n{'='*60}")
        print("REFACTORING COMPLETE")
        print(f"Classes processed: {success_count}/{total_classes}")
        print(f"Files modified: {len(self.files_processed)}/{total_files}")

        if self.errors:
            print(f"\nErrors encountered: {len(self.errors)}")
            for error in self.errors:
                print(f"  - {error}")

        return len(self.errors) == 0


if __name__ == "__main__":
    # Set the base path for ClassAI directory
    base_path = r"C:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI"

    refactorer = TemplateRefactorer(base_path)
    success = refactorer.run()

    sys.exit(0 if success else 1)