#!/usr/bin/env python3
"""
Fix all spatial grid API calls to use backward-compatible GUID methods
"""
import os
import re

def fix_file(filepath):
    """Fix spatial grid API calls in a single file"""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()

        original = content

        # Replace method calls
        content = content.replace('->QueryNearbyCreatures(', '->QueryNearbyCreatureGuids(')
        content = content.replace('->QueryNearbyPlayers(', '->QueryNearbyPlayerGuids(')
        content = content.replace('->QueryNearbyGameObjects(', '->QueryNearbyGameObjectGuids(')

        if content != original:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(content)
            return True
        return False
    except Exception as e:
        print(f"Error processing {filepath}: {e}")
        return False

def main():
    base_dir = r'C:\TrinityBots\TrinityCore\src\modules\Playerbot'
    fixed_count = 0

    for root, dirs, files in os.walk(base_dir):
        for file in files:
            if file.endswith('.cpp'):
                filepath = os.path.join(root, file)
                if fix_file(filepath):
                    fixed_count += 1
                    print(f"Fixed: {filepath}")

    print(f"\nTotal files fixed: {fixed_count}")

if __name__ == '__main__':
    main()
