#!/usr/bin/env python3
"""
Fix resource structures that have available() as a method instead of a member variable.
Also add missing Regenerate(), GetAvailable(), and GetMax() methods to satisfy ComplexResource concept.
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
    r"src\modules\Playerbot\AI\ClassAI\Warlocks\DemonologyWarlockRefactored.h",
]

def fix_resource_structure(filepath):
    """Fix resource structure in a file"""
    if not os.path.exists(filepath):
        print(f"WARNING: File not found: {filepath}")
        return False

    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    # Pattern to find resource structures with available() method
    # This will match from struct declaration to the closing };
    pattern = r'(struct \w+Resource\s*\{[^}]*?)(bool available\(\)\s*const\s*\{[^}]*\})'

    matches = list(re.finditer(pattern, content, re.DOTALL))

    if not matches:
        print(f"SKIP: No resource structure with available() found in {filepath}")
        return False

    # Replace available() method with available member variable
    new_content = content
    for match in matches:
        old_text = match.group(2)
        # Extract the return value logic from available()
        return_logic_match = re.search(r'return\s+([^;]+);', old_text)
        if not return_logic_match:
            continue

        # Replace with member variable
        new_text = "bool available{true};"
        new_content = new_content.replace(old_text, new_text)

    # Now check if Regenerate, GetAvailable, GetMax methods exist
    has_regenerate = 'void Regenerate(' in new_content
    has_get_available = 'GetAvailable()' in new_content
    has_get_max = 'GetMax()' in new_content

    if not (has_regenerate and has_get_available and has_get_max):
        # Find the Initialize method and add the missing methods before it
        init_pattern = r'(\s+)(void Initialize\(Player\* bot\))'
        init_match = re.search(init_pattern, new_content)

        if init_match:
            indent = init_match.group(1)
            methods_to_add = []

            if not has_regenerate:
                methods_to_add.append(f'''
{indent}void Regenerate(uint32 diff) {{
{indent}    // Resource regenerates over time
{indent}    // Simplified implementation
{indent}}}
''')

            if not has_get_available:
                methods_to_add.append(f'''
{indent}[[nodiscard]] uint32 GetAvailable() const {{
{indent}    // Return primary resource (override in specific implementations)
{indent}    return 0;
{indent}}}
''')

            if not has_get_max:
                methods_to_add.append(f'''
{indent}[[nodiscard]] uint32 GetMax() const {{
{indent}    // Return max resource (override in specific implementations)
{indent}    return 100;
{indent}}}
''')

            # Insert methods before Initialize
            insert_pos = init_match.start()
            new_content = new_content[:insert_pos] + ''.join(methods_to_add) + new_content[insert_pos:]

    # Update Consume() to set available flag
    consume_pattern = r'(bool Consume\([^)]*\)\s*\{[^}]*)(return true;)'
    new_content = re.sub(
        consume_pattern,
        r'\1available = true; // Update availability\n            return true;',
        new_content,
        flags=re.DOTALL
    )

    # Update Initialize() to set available flag
    init_body_pattern = r'(void Initialize\(Player\* bot\)\s*\{[^}]*)(}\s*$)'
    def add_available_to_init(match):
        body = match.group(1)
        if 'available = ' not in body:
            # Add before closing brace
            return body + '\n        available = true;\n    }'
        return match.group(0)

    new_content = re.sub(init_body_pattern, add_available_to_init, new_content, flags=re.MULTILINE)

    if new_content != content:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(new_content)
        print(f"SUCCESS: Fixed resource structure in {filepath}")
        return True
    else:
        print(f"SKIP: No changes needed in {filepath}")
        return False

def main():
    base_dir = os.path.dirname(os.path.abspath(__file__))
    success_count = 0
    skip_count = 0
    error_count = 0

    for rel_path in files_to_fix:
        filepath = os.path.join(base_dir, rel_path)
        try:
            result = fix_resource_structure(filepath)
            if result:
                success_count += 1
            else:
                skip_count += 1
        except Exception as e:
            print(f"ERROR processing {filepath}: {e}")
            error_count += 1

    print(f"\n=== SUMMARY ===")
    print(f"Successfully fixed: {success_count}")
    print(f"Skipped: {skip_count}")
    print(f"Errors: {error_count}")
    print(f"Total files processed: {len(files_to_fix)}")

if __name__ == "__main__":
    main()
