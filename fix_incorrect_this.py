#!/usr/bin/env python3
"""
Fix incorrect this-> prefixes that were added to function declarations
"""

import os
import re
import sys
from pathlib import Path
from typing import List, Tuple

class IncorrectThisFixer:
    def __init__(self, base_path: str):
        self.base_path = Path(base_path)
        self.files_fixed = []
        self.total_fixes = 0

    def get_refactored_files(self) -> List[str]:
        """Get all refactored files."""
        files = []
        pattern = "*Refactored.h"
        for file_path in self.base_path.rglob(pattern):
            relative = file_path.relative_to(self.base_path)
            files.append(str(relative).replace('\\', '/'))
        return sorted(files)

    def fix_file(self, relative_path: str) -> Tuple[bool, int]:
        """Fix incorrect this-> in a single file."""
        file_path = self.base_path / relative_path

        if not file_path.exists():
            return False, 0

        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()

            original = content
            fixes = 0

            # Fix patterns where this-> was incorrectly added to function declarations
            patterns = [
                # Fix: void this->UpdateRotation( -> void UpdateRotation(
                (r'\bvoid\s+this->(\w+)\s*\(', r'void \1('),
                # Fix: bool this->HasTacticalMastery( -> bool HasTacticalMastery(
                (r'\bbool\s+this->(\w+)\s*\(', r'bool \1('),
                # Fix: WarriorStance this->DetermineOptimalStance( -> WarriorStance DetermineOptimalStance(
                (r'\b(\w+)\s+this->(\w+)\s*\(', r'\1 \2('),
                # Fix: bot->this->HasAura -> bot->HasAura
                (r'->this->', r'->'),
                # Fix: this->HasSpell -> HasSpell (in const function declarations)
                (r'\bthis->(\w+)\s*\(\s*\)\s*const\s*$', r'\1() const'),
            ]

            for pattern, replacement in patterns:
                before = content
                content = re.sub(pattern, replacement, content, flags=re.MULTILINE)
                if before != content:
                    fixes += len(re.findall(pattern, before, flags=re.MULTILINE))

            if content != original:
                with open(file_path, 'w', encoding='utf-8') as f:
                    f.write(content)
                self.files_fixed.append(relative_path)
                print(f"[FIX] Fixed {relative_path}: {fixes} corrections")
                return True, fixes
            else:
                print(f"[OK] No fixes needed in {relative_path}")
                return True, 0

        except Exception as e:
            print(f"[ERROR] Failed to process {relative_path}: {str(e)}")
            return False, 0

    def run(self):
        """Execute the fixing for all files."""
        print("Fixing Incorrect this-> Prefixes")
        print(f"Base path: {self.base_path}")

        all_files = self.get_refactored_files()
        print(f"Found {len(all_files)} refactored files")

        for file in all_files:
            success, fixes = self.fix_file(file)
            self.total_fixes += fixes

        print(f"\n{'='*60}")
        print("FIXING COMPLETE")
        print(f"Files fixed: {len(self.files_fixed)}/{len(all_files)}")
        print(f"Total corrections: {self.total_fixes}")

        return True


if __name__ == "__main__":
    base_path = r"C:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI"

    fixer = IncorrectThisFixer(base_path)
    success = fixer.run()

    sys.exit(0 if success else 1)