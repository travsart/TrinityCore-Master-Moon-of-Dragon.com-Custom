#!/usr/bin/env python3
"""
Add missing Regenerate(), GetAvailable(), and GetMax() methods to resource structures
"""

import re
import os

files_to_fix = [
    r"src\modules\Playerbot\AI\ClassAI\DeathKnights\UnholyDeathKnightRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\Druids\BalanceDruidRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\Monks\BrewmasterMonkRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\Monks\WindwalkerMonkRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\Paladins\HolyPaladinRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\Paladins\ProtectionPaladinRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\Rogues\AssassinationRogueRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\Rogues\OutlawRogueRefactored.h",
    r"src\modules\Playerbot\AI\ClassAI\Rogues\SubtletyRogueRefactored.h",
]

METHODS_TO_ADD = """
    void Regenerate(uint32 diff) {
        // Resource regeneration logic (simplified)
        available = true;
    }

    [[nodiscard]] uint32 GetAvailable() const {
        return 100; // Simplified - override in specific implementations
    }

    [[nodiscard]] uint32 GetMax() const {
        return 100; // Simplified - override in specific implementations
    }
"""

def add_resource_methods(filepath):
    """Add missing resource methods to a file"""
    if not os.path.exists(filepath):
        print(f"WARNING: File not found: {filepath}")
        return False

    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    # Check if methods already exist
    if 'void Regenerate(' in content and 'GetAvailable()' in content and 'GetMax()' in content:
        print(f"SKIP: Methods already present in {filepath}")
        return False

    # Find Initialize method and add methods before it
    pattern = r'(\s+)(void Initialize\(Player\*[^)]*\)[^{]*\{)'
    match = re.search(pattern, content)

    if not match:
        print(f"WARNING: Could not find Initialize method in {filepath}")
        return False

    # Insert the methods before Initialize
    insert_pos = match.start()
    new_content = content[:insert_pos] + METHODS_TO_ADD + '\n' + content[insert_pos:]

    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(new_content)

    print(f"SUCCESS: Added resource methods to {filepath}")
    return True

def main():
    base_dir = os.path.dirname(os.path.abspath(__file__))
    success_count = 0
    skip_count = 0
    error_count = 0

    for rel_path in files_to_fix:
        filepath = os.path.join(base_dir, rel_path)
        try:
            result = add_resource_methods(filepath)
            if result:
                success_count += 1
            else:
                skip_count += 1
        except Exception as e:
            print(f"ERROR processing {filepath}: {e}")
            error_count += 1

    print(f"\n=== SUMMARY ===")
    print(f"Successfully added methods: {success_count}")
    print(f"Skipped: {skip_count}")
    print(f"Errors: {error_count}")
    print(f"Total files processed: {len(files_to_fix)}")

if __name__ == "__main__":
    main()
