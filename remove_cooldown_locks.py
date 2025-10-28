#!/usr/bin/env python3
"""
Script to remove unnecessary mutex locks from CooldownManager.cpp
This removes per-bot instance locks that are protecting non-shared data.
"""

import re

def remove_cooldown_manager_locks(filepath):
    """Remove all _cooldownMutex and _categoryMutex locks from CooldownManager"""

    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    original_content = content
    modifications = []

    # Pattern 1: Remove standalone lock_guard lines
    # Matches: std::lock_guard<std::recursive_mutex> lock(_cooldownMutex);
    pattern1 = r'(\s+)std::lock_guard<std::recursive_mutex>\s+\w+\(_cooldownMutex\);\s*\n'
    matches1 = list(re.finditer(pattern1, content))
    for match in matches1:
        indent = match.group(1)
        comment = f"{indent}// No lock needed - _cooldowns is per-bot instance data\n"
        content = content.replace(match.group(0), comment, 1)
        modifications.append(f"Line ~{content[:match.start()].count(chr(10)) + 1}: Removed _cooldownMutex lock")

    # Pattern 2: Remove category mutex locks
    pattern2 = r'(\s+)std::lock_guard<std::recursive_mutex>\s+\w+\(_categoryMutex\);\s*\n'
    matches2 = list(re.finditer(pattern2, content))
    for match in matches2:
        indent = match.group(1)
        comment = f"{indent}// No lock needed - _spellCategories and _categoryCooldowns are per-bot instance data\n"
        content = content.replace(match.group(0), comment, 1)
        modifications.append(f"Line ~{content[:match.start()].count(chr(10)) + 1}: Removed _categoryMutex lock")

    # Pattern 3: Remove lock_guard within braces (like line 42 with categoryLock)
    pattern3 = r'(\s+)\{\s*\n\s+std::lock_guard<std::recursive_mutex>\s+\w+\(_categoryMutex\);\s*\n'
    matches3 = list(re.finditer(pattern3, content))
    for match in matches3:
        indent = match.group(1)
        # Replace the opening brace and lock with just opening brace and comment
        replacement = f"{indent}{{\n{indent}    // No lock needed - category cooldowns are per-bot instance data\n"
        content = content.replace(match.group(0), replacement, 1)
        modifications.append(f"Line ~{content[:match.start()].count(chr(10)) + 1}: Removed _categoryMutex lock from scoped block")

    if content != original_content:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"[OK] Modified {filepath}")
        print(f"   Removed {len(modifications)} mutex locks:")
        for mod in modifications:
            print(f"   - {mod}")
        return len(modifications)
    else:
        print(f"[WARN] No changes needed for {filepath}")
        return 0

if __name__ == "__main__":
    filepath = r"C:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\ClassAI\CooldownManager.cpp"
    total_removed = remove_cooldown_manager_locks(filepath)
    print(f"\n[DONE] Total locks removed: {total_removed}")
