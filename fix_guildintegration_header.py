#!/usr/bin/env python3
"""
Fix GuildIntegration.h - add helper method declarations
"""

# Read the file
with open('src/modules/Playerbot/Social/GuildIntegration.h', 'r') as f:
    content = f.read()

fixes_made = 0

# Add helper method declarations after the existing guild bank logic section
old_guild_bank_section = '''    // Guild bank logic
    bool ShouldDepositItem(uint32 itemId);
    bool ShouldWithdrawItem(uint32 itemId);
    void ProcessGuildBankTransaction(uint32 itemId, bool isDeposit);
    void OptimizeGuildBankLayout();'''

new_guild_bank_section = '''    // Guild bank logic
    bool ShouldDepositItem(uint32 itemId);
    bool ShouldWithdrawItem(uint32 itemId);
    void ProcessGuildBankTransaction(uint32 itemId, bool isDeposit);
    void OptimizeGuildBankLayout();

    // Guild bank helper methods
    uint8 FindBestTabForItem(Item* item) const;
    int8 FindEmptySlotInTab(Guild* guild, uint8 tabId) const;
    bool HasGuildBankDepositRights(uint8 tabId) const;
    bool HasGuildBankWithdrawRights(uint8 tabId) const;
    int32 GetRemainingWithdrawSlots(uint8 tabId) const;
    int8 FindItemInTab(Guild* guild, uint8 tabId, uint32 itemId) const;
    bool FindFreeInventorySlot(uint8& outBag, uint8& outSlot) const;

    // Needs analysis methods
    void AnalyzeConsumableNeeds(std::vector<uint32>& neededItems) const;
    void AnalyzeReagentNeeds(std::vector<uint32>& neededItems) const;
    void AnalyzeProfessionMaterialNeeds(std::vector<uint32>& neededItems) const;
    void AnalyzeEquipmentUpgrades(Guild* guild, std::vector<std::pair<uint8, uint8>>& upgradeLocations) const;

    // Item evaluation methods
    bool HasRelevantProfession(uint32 itemSubClass) const;
    bool IsEquipmentUpgrade(ItemTemplate const* itemTemplate) const;
    bool CanLearnRecipe(ItemTemplate const* itemTemplate) const;
    bool HasItemsNeedingSockets() const;'''

if old_guild_bank_section in content:
    content = content.replace(old_guild_bank_section, new_guild_bank_section)
    print("Helper method declarations: ADDED")
    fixes_made += 1
else:
    print("Helper method declarations: NOT FOUND - searching for alternative")

# Write back
with open('src/modules/Playerbot/Social/GuildIntegration.h', 'w') as f:
    f.write(content)

print(f"\nTotal replacements made: {fixes_made}")
print("Header file updated successfully" if fixes_made > 0 else "No changes made")
