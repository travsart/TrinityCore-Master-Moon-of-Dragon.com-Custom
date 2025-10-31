#!/usr/bin/env python3
"""Apply SpellEvent destructor bugfix to clean upstream Spell.cpp"""

import sys

filepath = "src/server/game/Spells/Spell.cpp"

with open(filepath, 'r', encoding='utf-8') as f:
    lines = f.readlines()

# Find the SpellEvent destructor
insert_line = None
for i, line in enumerate(lines):
    if i >= 8440 and i <= 8450 and "if (m_Spell->getState() != SPELL_STATE_FINISHED)" in line:
        # Find the line after "m_Spell->cancel();"
        for j in range(i+1, min(i+5, len(lines))):
            if "m_Spell->cancel();" in lines[j]:
                insert_line = j + 1
                # Skip existing blank line if present
                while insert_line < len(lines) and lines[insert_line].strip() == "":
                    insert_line += 1
                break
        break

if insert_line is None:
    print("ERROR: Could not find insertion point")
    sys.exit(1)

# Prepare the fix
fix_lines = [
    "\n",
    "    // Clear spell mod taking spell before destruction to prevent assertion failure\n",
    "    // When KillAllEvents() is called (logout, map change, instance reset), spell events are destroyed\n",
    "    // without going through the normal delayed handler that clears m_spellModTakingSpell\n",
    "    // This causes ASSERT(m_caster->ToPlayer()->m_spellModTakingSpell != this) to fail in ~Spell()\n",
    "    if (m_Spell->GetCaster() && m_Spell->GetCaster()->GetTypeId() == TYPEID_PLAYER)\n",
    "        m_Spell->GetCaster()->ToPlayer()->SetSpellModTakingSpell(m_Spell, false);\n",
    "\n"
]

# Insert the fix
lines = lines[:insert_line] + fix_lines + lines[insert_line:]

# Write back
with open(filepath, 'w', encoding='utf-8', newline='\n') as f:
    f.writelines(lines)

print(f"SUCCESS: Applied SpellEvent destructor fix at line {insert_line}")
print(f"Added {len(fix_lines)} lines to Spell.cpp")
