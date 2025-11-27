#!/usr/bin/env python3
"""
Fix GuildIntegration.cpp - Final TrinityCore 11.2 API compatibility fixes
"""

# Read the file
with open('src/modules/Playerbot/Social/GuildIntegration.cpp', 'r') as f:
    content = f.read()

fixes_made = 0

# Fix 1: HasGuildBankDepositRights - GetRank() -> GetGuildRank()
old_code1 = '''bool GuildIntegration::HasGuildBankDepositRights(uint8 /*tabId*/) const
{
    if (!_bot || !_bot->GetGuild())
        return false;

    Guild* guild = _bot->GetGuild();
    // TrinityCore 11.2: GetMember is private, use GetGuildRank to check permissions
    // Use bot's GetRank instead of accessing Guild::Member directly
    GuildRankId rank = _bot->GetRank();

    // Guild bank deposit rights are typically available to all ranks with withdraw rights
    // We check for GR_RIGHT_WITHDRAW_GOLD as a proxy for general bank permissions
    return guild->HasAnyRankRight(rank, GR_RIGHT_WITHDRAW_GOLD);
}'''

new_code1 = '''bool GuildIntegration::HasGuildBankDepositRights(uint8 /*tabId*/) const
{
    if (!_bot || !_bot->GetGuild())
        return false;

    Guild* guild = _bot->GetGuild();
    // TrinityCore 11.2: GetMember is private, use GetGuildRank to check permissions
    GuildRankId rank = static_cast<GuildRankId>(_bot->GetGuildRank());

    // Guild bank deposit rights are typically available to all ranks with withdraw rights
    // We check for GR_RIGHT_WITHDRAW_GOLD as a proxy for general bank permissions
    return guild->HasAnyRankRight(rank, GR_RIGHT_WITHDRAW_GOLD);
}'''

if old_code1 in content:
    content = content.replace(old_code1, new_code1)
    print("Fix 1: HasGuildBankDepositRights GetRank -> GetGuildRank: APPLIED")
    fixes_made += 1
else:
    print("Fix 1: HasGuildBankDepositRights: NOT FOUND")

# Fix 2: HasGuildBankWithdrawRights - GetRank() -> GetGuildRank()
old_code2 = '''bool GuildIntegration::HasGuildBankWithdrawRights(uint8 /*tabId*/) const
{
    if (!_bot || !_bot->GetGuild())
        return false;

    Guild* guild = _bot->GetGuild();
    // TrinityCore 11.2: GetMember is private, use GetRank to check permissions
    GuildRankId rank = _bot->GetRank();

    // Check for withdraw rights through rank permissions
    return guild->HasAnyRankRight(rank, GR_RIGHT_WITHDRAW_GOLD);
}'''

new_code2 = '''bool GuildIntegration::HasGuildBankWithdrawRights(uint8 /*tabId*/) const
{
    if (!_bot || !_bot->GetGuild())
        return false;

    Guild* guild = _bot->GetGuild();
    // TrinityCore 11.2: GetMember is private, use GetGuildRank to check permissions
    GuildRankId rank = static_cast<GuildRankId>(_bot->GetGuildRank());

    // Check for withdraw rights through rank permissions
    return guild->HasAnyRankRight(rank, GR_RIGHT_WITHDRAW_GOLD);
}'''

if old_code2 in content:
    content = content.replace(old_code2, new_code2)
    print("Fix 2: HasGuildBankWithdrawRights GetRank -> GetGuildRank: APPLIED")
    fixes_made += 1
else:
    print("Fix 2: HasGuildBankWithdrawRights: NOT FOUND")

# Fix 3: IsEquipmentUpgrade - RaceMask comparison needs HasRace method
old_code3 = '''    if (itemTemplate->GetAllowableRace() != 0 && !(itemTemplate->GetAllowableRace() & _bot->GetRaceMask()))
        return false;'''

new_code3 = '''    if (!itemTemplate->GetAllowableRace().IsEmpty() && !itemTemplate->GetAllowableRace().HasRace(_bot->GetRace()))
        return false;'''

if old_code3 in content:
    content = content.replace(old_code3, new_code3)
    print("Fix 3: IsEquipmentUpgrade RaceMask comparison: APPLIED")
    fixes_made += 1
else:
    print("Fix 3: IsEquipmentUpgrade RaceMask: NOT FOUND")

# Write back
with open('src/modules/Playerbot/Social/GuildIntegration.cpp', 'w') as f:
    f.write(content)

print(f"\nTotal fixes applied: {fixes_made}")
print("GuildIntegration.cpp updated" if fixes_made > 0 else "No changes made")
