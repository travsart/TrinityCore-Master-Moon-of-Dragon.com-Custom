#!/usr/bin/env python3
"""
Fix GuildIntegration.cpp helper methods - TrinityCore 11.2 API compatibility
"""

# Read the file
with open('src/modules/Playerbot/Social/GuildIntegration.cpp', 'r') as f:
    content = f.read()

fixes_made = 0

# Fix 1: ItemLevel -> GetBaseItemLevel() in IsEquipmentUpgrade
old_code1 = '''    // Compare item levels
    uint32 newItemLevel = itemTemplate->ItemLevel;
    uint32 equippedItemLevel = equippedItem->GetTemplate()->ItemLevel;

    // Consider it an upgrade if it's at least 10 item levels higher
    return newItemLevel > equippedItemLevel + 10;'''

new_code1 = '''    // Compare item levels
    uint32 newItemLevel = itemTemplate->GetBaseItemLevel();
    uint32 equippedItemLevel = equippedItem->GetTemplate()->GetBaseItemLevel();

    // Consider it an upgrade if it's at least 10 item levels higher
    return newItemLevel > equippedItemLevel + 10;'''

if old_code1 in content:
    content = content.replace(old_code1, new_code1)
    print("Fix 1: ItemLevel -> GetBaseItemLevel(): APPLIED")
    fixes_made += 1
else:
    print("Fix 1: ItemLevel: NOT FOUND")

# Fix 2: Class check in CanLearnRecipe
old_code2 = '''bool GuildIntegration::CanLearnRecipe(ItemTemplate const* itemTemplate) const
{
    if (!_bot || !itemTemplate || itemTemplate->Class != ITEM_CLASS_RECIPE)
        return false;

    // Check if we have the profession for this recipe
    uint32 requiredSkill = itemTemplate->RequiredSkill;
    if (requiredSkill != 0 && !_bot->HasSkill(requiredSkill))
        return false;

    // Check skill level requirement
    uint32 requiredSkillRank = itemTemplate->RequiredSkillRank;
    if (requiredSkillRank > 0 && _bot->GetSkillValue(requiredSkill) < requiredSkillRank)
        return false;

    // Check if we already know the spell this recipe teaches
    for (auto const& spellData : itemTemplate->Spells)
    {
        if (spellData.SpellId != 0 && _bot->HasSpell(spellData.SpellId))
            return false;
    }

    return true;
}'''

new_code2 = '''bool GuildIntegration::CanLearnRecipe(ItemTemplate const* itemTemplate) const
{
    if (!_bot || !itemTemplate || itemTemplate->GetClass() != ITEM_CLASS_RECIPE)
        return false;

    // Check if we have the profession for this recipe
    uint32 requiredSkill = itemTemplate->GetRequiredSkill();
    if (requiredSkill != 0 && !_bot->HasSkill(requiredSkill))
        return false;

    // Check skill level requirement
    uint32 requiredSkillRank = itemTemplate->GetRequiredSkillRank();
    if (requiredSkillRank > 0 && _bot->GetSkillValue(requiredSkill) < requiredSkillRank)
        return false;

    // TrinityCore 11.2: Use Effects vector instead of deprecated Spells array
    for (ItemEffectEntry const* effect : itemTemplate->Effects)
    {
        if (effect && effect->SpellID != 0 && _bot->HasSpell(effect->SpellID))
            return false;
    }

    return true;
}'''

if old_code2 in content:
    content = content.replace(old_code2, new_code2)
    print("Fix 2: CanLearnRecipe API fixes: APPLIED")
    fixes_made += 1
else:
    print("Fix 2: CanLearnRecipe: NOT FOUND")

# Fix 3: Socket check in HasItemsNeedingSockets
old_code3 = '''        // Check if item has empty gem sockets
        for (uint32 i = 0; i < MAX_ITEM_PROTO_SOCKETS; ++i)
        {
            if (item->GetTemplate()->Socket[i].Color != 0)
            {
                // Socket exists, check if it's empty
                if (item->GetGem(i) == nullptr)
                    return true;
            }
        }'''

new_code3 = '''        // Check if item has empty gem sockets
        for (uint32 i = 0; i < MAX_ITEM_PROTO_SOCKETS; ++i)
        {
            SocketColor socketColor = item->GetTemplate()->GetSocketColor(i);
            if (socketColor != SOCKET_COLOR_NONE)
            {
                // Socket exists, check if it's empty
                if (item->GetGem(i) == nullptr)
                    return true;
            }
        }'''

if old_code3 in content:
    content = content.replace(old_code3, new_code3)
    print("Fix 3: Socket -> GetSocketColor(): APPLIED")
    fixes_made += 1
else:
    print("Fix 3: Socket check: NOT FOUND")

# Write back
with open('src/modules/Playerbot/Social/GuildIntegration.cpp', 'w') as f:
    f.write(content)

print(f"\nTotal fixes applied: {fixes_made}")
