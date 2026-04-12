-- 2026_04_05_00_world.sql
-- Remove serverside_spell entries for Chrono Surge (1900030) and Swift Crusade (1900031)
-- These spells now live entirely in hotfixes (spell_name + spell_misc + spell_effect + hotfix_data)
-- The serverside entries CONFLICT with the hotfix entries and cause zero-effect spells.
-- See DBErrors.log: "Serverside spell X references a regular spell loaded from file"

DELETE FROM `serverside_spell_effect` WHERE `SpellID` IN (1900030, 1900031);
DELETE FROM `serverside_spell` WHERE `Id` IN (1900030, 1900031);
