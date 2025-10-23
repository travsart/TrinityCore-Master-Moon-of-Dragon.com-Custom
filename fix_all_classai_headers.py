#!/usr/bin/env python3
"""
Enterprise-Grade ClassAI Header Cleanup Script
Removes all old dual-support specialization system members and methods from *AI.h headers
"""

import os
import re
from pathlib import Path

# Path to ClassAI directory
CLASSAI_PATH = Path(r"c:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI")

# All class names
CLASS_NAMES = [
    "DeathKnight", "DemonHunter", "Druid", "Evoker", "Hunter",
    "Mage", "Monk", "Paladin", "Priest", "Rogue", "Shaman", "Warlock", "Warrior"
]

def clean_header_file(file_path):
    """Remove old specialization system code from a *AI.h header file"""
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    original_content = content
    changes = []

    # Pattern 1: Remove old specialization unique_ptr member
    # Example: std::unique_ptr<WarlockSpecialization> _specialization;
    pattern1 = r'^\s+std::unique_ptr<\w+Specialization>\s+_specialization;\s*\n'
    if re.search(pattern1, content, re.MULTILINE):
        content = re.sub(pattern1, '', content, flags=re.MULTILINE)
        changes.append("Removed _specialization unique_ptr")

    # Pattern 2: Remove spec tracking variable (_currentSpec or _detectedSpec)
    # Example: WarlockSpec _currentSpec;
    # Example: DeathKnightSpec _detectedSpec;
    pattern2 = r'^\s+\w+Spec\s+_(currentSpec|detectedSpec);\s*\n'
    if re.search(pattern2, content, re.MULTILINE):
        content = re.sub(pattern2, '', content, flags=re.MULTILINE)
        changes.append("Removed spec tracking variable")

    # Pattern 3: Remove old method declarations (all on separate lines for precision)
    # void InitializeSpecialization();
    pattern3a = r'^\s+void\s+InitializeSpecialization\(\);\s*\n'
    if re.search(pattern3a, content, re.MULTILINE):
        content = re.sub(pattern3a, '', content, flags=re.MULTILINE)
        changes.append("Removed InitializeSpecialization()")

    # void DetectSpecialization();
    pattern3b = r'^\s+void\s+DetectSpecialization\(\);\s*\n'
    if re.search(pattern3b, content, re.MULTILINE):
        content = re.sub(pattern3b, '', content, flags=re.MULTILINE)
        changes.append("Removed DetectSpecialization()")

    # XSpec DetectCurrentSpecialization() const;
    pattern3c = r'^\s+\w+Spec\s+DetectCurrentSpecialization\(\)( const)?;\s*\n'
    if re.search(pattern3c, content, re.MULTILINE):
        content = re.sub(pattern3c, '', content, flags=re.MULTILINE)
        changes.append("Removed DetectCurrentSpecialization()")

    # void SwitchSpecialization(XSpec newSpec);
    pattern3d = r'^\s+void\s+SwitchSpecialization\(\w+Spec\s+\w+\);\s*\n'
    if re.search(pattern3d, content, re.MULTILINE):
        content = re.sub(pattern3d, '', content, flags=re.MULTILINE)
        changes.append("Removed SwitchSpecialization()")

    # XSpec GetCurrentSpecialization() const;
    pattern3e = r'^\s+\w+Spec\s+GetCurrentSpecialization\(\) const;\s*\n'
    if re.search(pattern3e, content, re.MULTILINE):
        content = re.sub(pattern3e, '', content, flags=re.MULTILINE)
        changes.append("Removed GetCurrentSpecialization()")

    # Pattern 4: Remove "Specialization Management" or "Specialization System" comment sections
    # if they ONLY contain old system code
    pattern4 = r'^\s+// Specialization (Management|System)\s*\n(^\s+\w+.*\n)*?(?=^\s+//|^private:|^protected:|^public:|^\s*$)'
    matches = list(re.finditer(pattern4, content, re.MULTILINE))
    for match in reversed(matches):  # Reverse to maintain indices
        section_content = match.group(0)
        # Only remove if it contains old system markers
        if re.search(r'(GetCurrentSpecialization|InitializeSpecialization|DetectSpecialization|SwitchSpecialization)', section_content):
            content = content[:match.start()] + content[match.end():]
            changes.append("Removed Specialization Management comment section")

    # Pattern 5: Clean up "Specialization system" comment blocks in private section
    pattern5 = r'^\s+// Specialization system\s*\n'
    if re.search(pattern5, content, re.MULTILINE):
        content = re.sub(pattern5, '', content, flags=re.MULTILINE)
        changes.append("Removed specialization system comment")

    if content != original_content:
        with open(file_path, 'w', encoding='utf-8', newline='\n') as f:
            f.write(content)
        return True, changes
    else:
        return False, []

def main():
    """Main execution function"""
    print("=" * 60)
    print("Enterprise-Grade ClassAI Header Cleanup")
    print("=" * 60)

    total_processed = 0
    total_modified = 0
    modified_files = []

    for class_name in CLASS_NAMES:
        # Find the *AI.h file for this class
        class_dir = CLASSAI_PATH / f"{class_name}s" if class_name != "DeathKnight" else CLASSAI_PATH / "DeathKnights"
        if class_name == "DemonHunter":
            class_dir = CLASSAI_PATH / "DemonHunters"

        ai_header = class_dir / f"{class_name}AI.h"

        if not ai_header.exists():
            print(f"[SKIP] {class_name}AI.h not found at {ai_header}")
            continue

        print(f"\nProcessing {class_name}AI.h...")
        total_processed += 1

        modified, changes = clean_header_file(ai_header)

        if modified:
            total_modified += 1
            modified_files.append(class_name + "AI.h")
            print(f"  [MODIFIED] {len(changes)} changes:")
            for change in changes:
                print(f"    - {change}")
        else:
            print(f"  [OK] No old system code found")

    print("\n" + "=" * 60)
    print("CLEANUP SUMMARY")
    print("=" * 60)
    print(f"Files Processed: {total_processed}")
    print(f"Files Modified: {total_modified}")

    if modified_files:
        print("\nModified Files:")
        for filename in modified_files:
            print(f"  - {filename}")

    print("\n[SUCCESS] Enterprise-grade header cleanup complete!")
    print("=" * 60)

if __name__ == "__main__":
    main()
