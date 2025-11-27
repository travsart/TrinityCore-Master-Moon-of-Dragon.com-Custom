#!/usr/bin/env python3
"""
Fix GuildIntegration.cpp - Glyph section Spells[0].SpellId -> Effects
"""

# Read the file
with open('src/modules/Playerbot/Social/GuildIntegration.cpp', 'r') as f:
    content = f.read()

fixes_made = 0

# Fix the Glyph section
old_glyph = '''        case ITEM_CLASS_GLYPH:
        {
            // Check if glyph is for our class and we don't have it
            if (itemTemplate->GetAllowableClass() != 0 && !(itemTemplate->GetAllowableClass() & _bot->GetClassMask()))
                return false;
            return !_bot->HasSpell(itemTemplate->Spells[0].SpellId);
        }'''

new_glyph = '''        case ITEM_CLASS_GLYPH:
        {
            // Check if glyph is for our class and we don't have it
            if (itemTemplate->GetAllowableClass() != 0 && !(itemTemplate->GetAllowableClass() & _bot->GetClassMask()))
                return false;
            // TrinityCore 11.2: Use Effects vector instead of deprecated Spells array
            if (!itemTemplate->Effects.empty())
            {
                ItemEffectEntry const* effect = itemTemplate->Effects[0];
                if (effect && effect->SpellID > 0)
                    return !_bot->HasSpell(effect->SpellID);
            }
            return false;
        }'''

if old_glyph in content:
    content = content.replace(old_glyph, new_glyph)
    print("Glyph Spells -> Effects: APPLIED")
    fixes_made += 1
else:
    print("Glyph section: NOT FOUND (checking alternative)")
    # Try more flexible match
    if "itemTemplate->Spells[0].SpellId" in content:
        content = content.replace(
            "return !_bot->HasSpell(itemTemplate->Spells[0].SpellId);",
            '''// TrinityCore 11.2: Use Effects vector instead of deprecated Spells array
            if (!itemTemplate->Effects.empty())
            {
                ItemEffectEntry const* effect = itemTemplate->Effects[0];
                if (effect && effect->SpellID > 0)
                    return !_bot->HasSpell(effect->SpellID);
            }
            return false;''')
        print("Alternative Spells -> Effects: APPLIED")
        fixes_made += 1
    else:
        print("No Spells[0] reference found")

# Write back
with open('src/modules/Playerbot/Social/GuildIntegration.cpp', 'w') as f:
    f.write(content)

print(f"\nTotal fixes applied: {fixes_made}")
