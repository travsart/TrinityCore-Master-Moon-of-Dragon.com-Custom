#!/usr/bin/env python3
"""
Fix using declarations in refactored class files.
Changes from: using Template<Param>::Member
To: using Base = Template<Param>; using Base::Member
"""

import re
import os
from pathlib import Path

def fix_using_declarations_in_file(filepath):
    """Fix using declarations in a single file."""
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    original_content = content

    # Patterns for different template base classes
    template_patterns = [
        ('RangedDpsSpecialization', r'RangedDpsSpecialization<([^>]+)>'),
        ('MeleeDpsSpecialization', r'MeleeDpsSpecialization<([^>]+)>'),
        ('HealerSpecialization', r'HealerSpecialization<([^>]+)>'),
        ('TankSpecialization', r'TankSpecialization<([^>]+)>'),
    ]

    for base_name, template_pattern in template_patterns:
        # Find all using declarations for this template
        using_pattern = rf'using {template_pattern}::(\w+);'
        matches = list(re.finditer(using_pattern, content))

        if matches:
            # Get the template parameter from first match
            template_param = matches[0].group(1)

            # Create the base typedef
            base_typedef = f'using Base = {base_name}<{template_param}>;'

            # Check if this typedef already exists
            if base_typedef not in content:
                # Find the location of the first using declaration
                first_match_pos = matches[0].start()

                # Find the start of the line (including indentation)
                line_start = content.rfind('\n', 0, first_match_pos) + 1
                indent = content[line_start:first_match_pos]

                # Insert comment and typedef before first using
                comment = f'{indent}// Use base class members with type alias for cleaner syntax\n'
                typedef_line = f'{indent}{base_typedef}\n'

                # Insert before the first using declaration
                content = content[:line_start] + comment + typedef_line + content[line_start:]

            # Replace all using declarations with Base::
            for match in reversed(matches):  # Process in reverse to maintain positions
                full_match = match.group(0)
                member_name = match.group(2)
                new_using = f'using Base::{member_name};'

                # Calculate new position after insertions
                offset = len(content) - len(original_content)
                start = match.start() + offset if content != original_content else match.start()
                end = start + len(full_match)

                content = content[:start] + new_using + content[end:]

    # Save only if changed
    if content != original_content:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        return True
    return False

def main():
    """Main function to process all refactored files."""
    # Find all *Refactored.h files
    base_path = Path('src/modules/Playerbot/AI/ClassAI')

    if not base_path.exists():
        print(f"Error: Path {base_path} does not exist!")
        return

    refactored_files = list(base_path.rglob('*Refactored.h'))

    print(f"Found {len(refactored_files)} refactored files to process\n")

    files_modified = 0

    for filepath in refactored_files:
        print(f"Processing: {filepath}")
        try:
            if fix_using_declarations_in_file(filepath):
                print(f"  -> Modified!")
                files_modified += 1
            else:
                print(f"  -> No changes needed")
        except Exception as e:
            print(f"  -> Error: {e}")

    print(f"\nTotal files modified: {files_modified}")
    print("Done!")

if __name__ == '__main__':
    main()
