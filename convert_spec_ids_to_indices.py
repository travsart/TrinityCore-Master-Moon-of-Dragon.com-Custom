#!/usr/bin/env python3
"""
Convert playerbot_talent_loadouts SQL from global spec IDs to spec indices (0-3).
Fixes: SQL-Fehler [1264] [22001]: Data truncation: Out of range value for column 'spec_id'
"""

# Mapping: Global WoW Spec ID → (Class ID, Spec Index, Spec Name)
SPEC_MAPPING = {
    # Warrior (Class 1)
    71: (1, 0, "Arms"),
    72: (1, 1, "Fury"),
    73: (1, 2, "Protection"),

    # Paladin (Class 2)
    65: (2, 0, "Holy"),
    66: (2, 1, "Protection"),
    70: (2, 2, "Retribution"),

    # Hunter (Class 3)
    253: (3, 0, "Beast Mastery"),
    254: (3, 1, "Marksmanship"),
    255: (3, 2, "Survival"),

    # Rogue (Class 4)
    259: (4, 0, "Assassination"),
    260: (4, 1, "Outlaw"),
    261: (4, 2, "Subtlety"),

    # Priest (Class 5)
    256: (5, 0, "Discipline"),
    257: (5, 1, "Holy"),
    258: (5, 2, "Shadow"),

    # Death Knight (Class 6)
    250: (6, 0, "Blood"),
    251: (6, 1, "Frost"),
    252: (6, 2, "Unholy"),

    # Shaman (Class 7)
    262: (7, 0, "Elemental"),
    263: (7, 1, "Enhancement"),
    264: (7, 2, "Restoration"),

    # Mage (Class 8)
    62: (8, 0, "Arcane"),
    63: (8, 1, "Fire"),
    64: (8, 2, "Frost"),

    # Warlock (Class 9)
    265: (9, 0, "Affliction"),
    266: (9, 1, "Demonology"),
    267: (9, 2, "Destruction"),

    # Monk (Class 10)
    268: (10, 0, "Brewmaster"),
    270: (10, 1, "Mistweaver"),
    269: (10, 2, "Windwalker"),

    # Druid (Class 11)
    102: (11, 0, "Balance"),
    103: (11, 1, "Feral"),
    104: (11, 2, "Guardian"),
    105: (11, 3, "Restoration"),

    # Demon Hunter (Class 12)
    577: (12, 0, "Havoc"),
    581: (12, 1, "Vengeance"),

    # Evoker (Class 13)
    1467: (13, 0, "Devastation"),
    1468: (13, 1, "Preservation"),
    1473: (13, 2, "Augmentation"),
}

def convert_sql_file(input_file: str, output_file: str):
    """Convert SQL file from global spec IDs to spec indices."""

    with open(input_file, 'r', encoding='utf-8') as f:
        content = f.read()

    # Replace spec IDs in INSERT statements and comments
    for global_spec_id, (class_id, spec_index, spec_name) in SPEC_MAPPING.items():
        # Replace in INSERT VALUES
        # Pattern: (1, 71, 1, 10, ...) → (1, 0, 1, 10, ...)
        content = content.replace(
            f"({class_id}, {global_spec_id},",
            f"({class_id}, {spec_index},"
        )

        # Replace in comments
        # Pattern: -- Arms (Spec ID: 71) → -- Arms (Spec Index: 0)
        content = content.replace(
            f"(Spec ID: {global_spec_id})",
            f"(Spec Index: {spec_index})"
        )

    # Update table comment
    content = content.replace(
        "'Specialization ID (0-3)'",
        "'Specialization Index (0-3 per class)'"
    )

    # Update header documentation
    content = content.replace(
        "-- Spec ID:",
        "-- Spec Index:"
    )

    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(content)

    print(f"[OK] Converted {input_file}")
    print(f"[OK] Output saved to {output_file}")
    print(f"[OK] Converted {len(SPEC_MAPPING)} specializations")

if __name__ == "__main__":
    input_file = "src/modules/Playerbot/sql/base/playerbot_talent_loadouts_populated.sql"
    output_file = "src/modules/Playerbot/sql/base/playerbot_talent_loadouts_with_indices.sql"

    convert_sql_file(input_file, output_file)
