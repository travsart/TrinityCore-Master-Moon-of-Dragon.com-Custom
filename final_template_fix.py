#!/usr/bin/env python3
"""
Final comprehensive fix for template method visibility issues.
This ensures ALL base class method calls have this-> prefix in const functions too.
"""

import os
import re
import sys
from pathlib import Path
from typing import List, Tuple

class FinalTemplateFixer:
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

    def process_line(self, line: str, in_const_function: bool = False) -> Tuple[str, int]:
        """Process a single line."""
        fixes = 0
        result = line

        # Skip pure comments and preprocessor directives
        stripped = line.strip()
        if stripped.startswith('//') or stripped.startswith('#') or stripped.startswith('/*'):
            return line, 0

        # Methods that ALWAYS need this-> when called (not when declared)
        base_methods = [
            'GetBot', 'GetPlayer', 'GetGroup',
            'CastSpell', 'CanCastSpell', 'IsSpellReady', 'CanUseAbility',
            'HasSpell', 'GetSpellCooldown',
            'GetEnemiesInRange', 'IsBehindTarget', 'IsInMeleeRange',
            'GetTarget', 'CanAttack', 'IsInCombat',
            'GetCurrentResource', 'GetMaxResource', 'HasResource',
            'ConsumeResource', 'GenerateResource',
            'GetDistance', 'MoveTo', 'StopMoving', 'FaceTarget',
            'HasAura', 'GetAuraStacks', 'GetAuraDuration',
            'HasTacticalMastery', 'SwitchToStance', 'DetermineOptimalStance',
            'GetCurrentStance', 'Log', 'DebugLog', 'IsValidTarget'
        ]

        # Check if this is a function declaration line
        is_declaration = False
        if re.match(r'^\s*(virtual\s+)?(void|bool|int|uint32|float|double|\w+)\s+\w+\s*\(', line):
            is_declaration = True
        if ' override' in line or ' const' in line:
            if re.match(r'^\s*(virtual\s+)?(void|bool|int|uint32|float|double|\w+)\s+', line):
                is_declaration = True

        # Don't add this-> to function declarations
        if is_declaration:
            return line, 0

        # Process each base method
        for method in base_methods:
            # Pattern for method call (not declaration)
            pattern = r'\b' + method + r'\s*\('

            # Find all occurrences
            matches = list(re.finditer(pattern, result))

            for match in reversed(matches):
                pos = match.start()

                # Check if already has this->
                if pos >= 6 and result[pos-6:pos] == 'this->':
                    continue

                # Check if it's after -> (object method call)
                if pos >= 2 and result[pos-2:pos] == '->':
                    continue

                # Check if it's after :: (static call)
                if pos >= 2 and result[pos-2:pos] == '::':
                    continue

                # Skip if in string literal
                in_string = False
                for i in range(pos):
                    if result[i] == '"' and (i == 0 or result[i-1] != '\\'):
                        in_string = not in_string
                if in_string:
                    continue

                # Add this->
                result = result[:pos] + 'this->' + result[pos:]
                fixes += 1

        return result, fixes

    def fix_file(self, relative_path: str) -> Tuple[bool, int]:
        """Fix a single file."""
        file_path = self.base_path / relative_path

        if not file_path.exists():
            return False, 0

        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                lines = f.readlines()

            modified_lines = []
            total_fixes = 0
            in_const_function = False

            for line in lines:
                # Track if we're in a const function
                if ' const' in line and '{' in line:
                    in_const_function = True
                elif '}' in line and in_const_function:
                    in_const_function = False

                modified_line, fixes = self.process_line(line, in_const_function)
                modified_lines.append(modified_line)
                total_fixes += fixes

            if total_fixes > 0:
                with open(file_path, 'w', encoding='utf-8') as f:
                    f.writelines(modified_lines)
                self.files_fixed.append(relative_path)
                print(f"[FIXED] {relative_path}: {total_fixes} additions")
            else:
                print(f"[OK] {relative_path}: No changes needed")

            return True, total_fixes

        except Exception as e:
            print(f"[ERROR] {relative_path}: {str(e)}")
            return False, 0

    def run(self):
        """Execute the fixing for all files."""
        print("Final Template Visibility Fix")
        print(f"Base path: {self.base_path}")

        all_files = self.get_refactored_files()
        print(f"Processing {len(all_files)} refactored files\n")

        for file in all_files:
            success, fixes = self.fix_file(file)
            self.total_fixes += fixes

        print(f"\n{'='*60}")
        print("FINAL FIX COMPLETE")
        print(f"Files processed: {len(all_files)}")
        print(f"Files modified: {len(self.files_fixed)}")
        print(f"Total additions: {self.total_fixes}")

        return True


if __name__ == "__main__":
    base_path = r"C:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI"

    fixer = FinalTemplateFixer(base_path)
    success = fixer.run()

    sys.exit(0 if success else 1)