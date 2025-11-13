#!/usr/bin/env python3
import re
import sys

def add_comprehensive_using(file_path):
    """Add comprehensive using declarations for template base class methods."""

    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Check if comprehensive using declarations already exist
    if 'using.*::CastSpell;' in content:
        print(f"Comprehensive using declarations already exist in {file_path}")
        return False

    # Find the existing using GetBot declaration and add more methods
    pattern = r'(using\s+(\w+Specialization)<([^>]+)>::GetBot;)'

    def replacement(match):
        template_base = match.group(2)  # e.g., "RangedDpsSpecialization"
        resource_type = match.group(3)  # e.g., "ManaSoulShardResource"
        return f'''{match.group(1)}
    using {template_base}<{resource_type}>::CastSpell;
    using {template_base}<{resource_type}>::CanCastSpell;
    using {template_base}<{resource_type}>::_resource;'''

    new_content, count = re.subn(pattern, replacement, content, flags=re.MULTILINE)

    if count > 0:
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(new_content)

        print(f"Added comprehensive using declarations in {file_path}")
        return True

    return False

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python add_comprehensive_using.py <file1> <file2> ...")
        sys.exit(1)

    files_modified = 0
    for file_path in sys.argv[1:]:
        try:
            if add_comprehensive_using(file_path):
                files_modified += 1
        except Exception as e:
            print(f"Error processing {file_path}: {e}", file=sys.stderr)

    print(f"\nTotal files modified: {files_modified}")
