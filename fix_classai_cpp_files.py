#!/usr/bin/env python3
"""
Fix ClassAI .cpp Files - Remove Old Specialization System
==========================================================

Automated refactoring script for removing old polymorphic specialization system
from ClassAI .cpp implementation files.

Handles the most common patterns identified by Agent 3:
- Pattern A: Constructor initialization
- Pattern B: Spec detection methods
- Pattern C: Spec switching via polymorphism

More complex patterns (D-J) will require agent-based refactoring.

Created: 2025-10-23
Purpose: Automate common refactoring patterns for .cpp files
"""

import re
from pathlib import Path
from typing import List, Tuple, Dict

class CppRefactorer:
    """Handles automated refactoring of ClassAI .cpp files"""

    def __init__(self, file_path: Path):
        self.file_path = file_path
        self.class_name = self._extract_class_name()
        self.changes: List[str] = []

    def _extract_class_name(self) -> str:
        """Extract class name from filename (e.g., WarriorAI from WarriorAI.cpp)"""
        return self.file_path.stem

    def process(self) -> Tuple[str, bool]:
        """Process the .cpp file and return (new_content, was_modified)"""

        with open(self.file_path, 'r', encoding='utf-8') as f:
            content = f.read()
            original_content = content

        # Pattern A: Remove constructor initialization of _specialization
        content = self._remove_constructor_initialization(content)

        # Pattern B: Remove DetectSpecialization and InitializeSpecialization methods
        content = self._remove_spec_detection_methods(content)

        # Pattern C: Remove SwitchSpecialization method
        content = self._remove_switch_specialization(content)

        # Additional cleanup: Remove empty method implementations
        content = self._cleanup_empty_methods(content)

        was_modified = (content != original_content)

        return content, was_modified

    def _remove_constructor_initialization(self, content: str) -> str:
        """
        Remove lines like:
            InitializeSpecialization();
        from constructor body
        """

        # Pattern: Constructor with InitializeSpecialization() call
        pattern = rf'{self.class_name}::{self.class_name}\([^)]*\)\s*:\s*ClassAI\([^)]*\)\s*{{\s*InitializeSpecialization\(\);\s*}}'

        if re.search(pattern, content, re.MULTILINE | re.DOTALL):
            # Replace with empty constructor body
            new_content = re.sub(
                pattern,
                rf'{self.class_name}::{self.class_name}(\1) : ClassAI(\2)\n{{\n    // Constructor - specialization handled by template system\n}}',
                content,
                flags=re.MULTILINE | re.DOTALL
            )
            if new_content != content:
                self.changes.append("Removed InitializeSpecialization() from constructor")
                return new_content

        # Simpler pattern: Just remove the call
        pattern2 = r'^\s*InitializeSpecialization\(\);\s*$'
        lines = content.split('\n')
        new_lines = []
        removed_count = 0

        for line in lines:
            if re.match(pattern2, line):
                self.changes.append(f"Removed InitializeSpecialization() call at line")
                removed_count += 1
            else:
                new_lines.append(line)

        if removed_count > 0:
            return '\n'.join(new_lines)

        return content

    def _remove_spec_detection_methods(self, content: str) -> str:
        """
        Remove entire method implementations:
        - void WarriorAI::InitializeSpecialization() { ... }
        - void WarriorAI::DetectSpecialization() { ... }
        - WarriorSpec WarriorAI::DetectCurrentSpecialization() { ... }
        """

        # Pattern 1: void ClassAI::InitializeSpecialization() { ... }
        pattern1 = rf'void\s+{self.class_name}::InitializeSpecialization\s*\(\)\s*{{[^}}]*(?:{{[^}}]*}}[^}}]*)*}}\s*'

        match1 = re.search(pattern1, content, re.MULTILINE | re.DOTALL)
        if match1:
            content = re.sub(pattern1, '', content, flags=re.MULTILINE | re.DOTALL)
            self.changes.append("Removed InitializeSpecialization() method implementation")

        # Pattern 2: void ClassAI::DetectSpecialization() { ... }
        pattern2 = rf'void\s+{self.class_name}::DetectSpecialization\s*\(\)\s*{{[^}}]*(?:{{[^}}]*}}[^}}]*)*}}\s*'

        match2 = re.search(pattern2, content, re.MULTILINE | re.DOTALL)
        if match2:
            content = re.sub(pattern2, '', content, flags=re.MULTILINE | re.DOTALL)
            self.changes.append("Removed DetectSpecialization() method implementation")

        # Pattern 3: ClassSpec ClassAI::DetectCurrentSpecialization() { ... }
        pattern3 = rf'\w+Spec\s+{self.class_name}::DetectCurrentSpecialization\s*\(\)(?:\s+const)?\s*{{[^}}]*(?:{{[^}}]*}}[^}}]*)*}}\s*'

        match3 = re.search(pattern3, content, re.MULTILINE | re.DOTALL)
        if match3:
            content = re.sub(pattern3, '', content, flags=re.MULTILINE | re.DOTALL)
            self.changes.append("Removed DetectCurrentSpecialization() method implementation")

        return content

    def _remove_switch_specialization(self, content: str) -> str:
        """
        Remove SwitchSpecialization method:
        void WarriorAI::SwitchSpecialization(WarriorSpec newSpec) { ... }
        """

        pattern = rf'void\s+{self.class_name}::SwitchSpecialization\s*\([^)]*\)\s*{{[^}}]*(?:{{[^}}]*}}[^}}]*)*}}\s*'

        match = re.search(pattern, content, re.MULTILINE | re.DOTALL)
        if match:
            content = re.sub(pattern, '', content, flags=re.MULTILINE | re.DOTALL)
            self.changes.append("Removed SwitchSpecialization() method implementation")

        return content

    def _cleanup_empty_methods(self, content: str) -> str:
        """Remove consecutive blank lines left after deletions"""

        # Replace 3+ consecutive newlines with 2 newlines
        content = re.sub(r'\n{3,}', '\n\n', content)

        return content

    def get_summary(self) -> str:
        """Get a summary of changes"""

        if not self.changes:
            return "  [INFO] No automated changes applied (may need agent refactoring)"

        summary = f"  [MODIFIED] {len(self.changes)} automated changes:\n"
        for change in self.changes:
            summary += f"    - {change}\n"

        return summary


def main():
    """Main execution"""

    print("=" * 80)
    print("ClassAI .cpp File Refactoring - Automated Pattern Removal")
    print("=" * 80)
    print()
    print("NOTE: This script handles only the simple patterns (A, B, C).")
    print("Complex patterns (D-J) will be handled by specialized agents.")
    print()

    base_path = Path(r"c:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI")

    # All ClassAI .cpp files that need refactoring
    cpp_files = [
        base_path / "DeathKnights" / "DeathKnightAI.cpp",
        base_path / "DemonHunters" / "DemonHunterAI.cpp",
        base_path / "Druids" / "DruidAI.cpp",
        base_path / "Evokers" / "EvokerAI.cpp",
        base_path / "Hunters" / "HunterAI.cpp",
        base_path / "Mages" / "MageAI.cpp",
        base_path / "Monks" / "MonkAI.cpp",
        base_path / "Paladins" / "PaladinAI.cpp",
        base_path / "Priests" / "PriestAI.cpp",
        base_path / "Rogues" / "RogueAI.cpp",
        base_path / "Shamans" / "ShamanAI.cpp",
        base_path / "Warlocks" / "WarlockAI.cpp",
        base_path / "Warriors" / "WarriorAI.cpp",
    ]

    files_processed = 0
    files_modified = 0
    total_changes = 0

    for cpp_file in cpp_files:
        if not cpp_file.exists():
            print(f"[SKIP] {cpp_file.name} - File not found")
            continue

        print(f"Processing {cpp_file.name}...")

        refactorer = CppRefactorer(cpp_file)
        new_content, was_modified = refactorer.process()

        if was_modified:
            with open(cpp_file, 'w', encoding='utf-8') as f:
                f.write(new_content)

            files_modified += 1
            total_changes += len(refactorer.changes)

        print(refactorer.get_summary())
        print()

        files_processed += 1

    print("=" * 80)
    print(f"Files Processed: {files_processed}")
    print(f"Files Modified: {files_modified}")
    print(f"Total Automated Changes: {total_changes}")
    print()
    print("NEXT STEP: Use specialized agents for complex pattern refactoring")
    print("=" * 80)


if __name__ == "__main__":
    main()
