#!/usr/bin/env python3
import re
import sys

def fix_ambiguous_getbot(file_path):
    """Add using declaration to resolve GetBot() ambiguity."""

    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Check if using declaration already exists
    if 'using CombatSpecializationTemplate' in content and '::GetBot;' in content:
        print(f"Using declaration already exists in {file_path}")
        return False

    # Find the class declaration and add using statement after public:
    # Pattern: class ClassName : public TemplateBase<...>, public ClassSpecialization
    # {
    # public:
    #     explicit ClassName(...

    # We want to add: using CombatSpecializationTemplate<ResourceType>::GetBot;
    # right after public:

    pattern = r'(class\s+\w+Refactored\s*:\s*public\s+(\w+Specialization)<([^>]+)>,\s*public\s+\w+Specialization\s*\{\s*public:)'

    def replacement(match):
        template_base = match.group(2)  # e.g., "MeleeDpsSpecialization"
        resource_type = match.group(3)  # e.g., "ManaResource"
        return f'{match.group(1)}\n    using {template_base}<{resource_type}>::GetBot;'

    new_content, count = re.subn(pattern, replacement, content, flags=re.MULTILINE | re.DOTALL)

    if count > 0:
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(new_content)

        print(f"Added using declaration for GetBot() in {file_path}")
        return True

    return False

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python fix_ambiguous_getbot.py <file1> <file2> ...")
        sys.exit(1)

    files_modified = 0
    for file_path in sys.argv[1:]:
        try:
            if fix_ambiguous_getbot(file_path):
                files_modified += 1
        except Exception as e:
            print(f"Error processing {file_path}: {e}", file=sys.stderr)

    print(f"\nTotal files modified: {files_modified}")
