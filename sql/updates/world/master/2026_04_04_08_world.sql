-- 2026_04_04_08_world.sql
-- Warlock Phase 5 Tier B: Summon spell handlers — creature ScriptNames + spell_script_names

-- Creature ScriptNames for pet AIs
-- IMPORTANT: Clear AIName to prevent SmartAI from overriding C++ ScriptName
UPDATE `creature_template` SET `ScriptName` = 'npc_warl_imp_lord' WHERE `entry` = 258584;
UPDATE `creature_template` SET `ScriptName` = 'npc_warl_vilefiend', `AIName` = '' WHERE `entry` = 135816;
UPDATE `creature_template` SET `ScriptName` = 'npc_warl_doomguard' WHERE `entry` = 250785;
UPDATE `creature_template` SET `ScriptName` = 'npc_warl_infernal', `AIName` = '' WHERE `entry` = 89;

-- Spell script bindings
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(267216, 'spell_warl_inner_demons'),
(1276452, 'spell_warl_grimoire_imp_lord'),
(1251778, 'spell_warl_summon_vilefiend');

-- Data quality fixes found by deep audit
-- Felguard: type=0 (None) should be 3 (Demon), family=0 should be 29 (Felguard)
UPDATE `creature_template` SET `type` = 3, `family` = 29 WHERE `entry` = 17252;

-- Add NO_XP flag to all temporary warlock summons (Infernal already has it)
UPDATE `creature_template` SET `flags_extra` = `flags_extra` | 0x40 WHERE `entry` IN (135816, 250785, 258584) AND (`flags_extra` & 0x40) = 0;

-- Clean up orphaned SmartAI scripts for creatures now using C++ ScriptName
DELETE FROM `smart_scripts` WHERE `entryorguid` = 89 AND `source_type` = 0;
DELETE FROM `smart_scripts` WHERE `entryorguid` = 135816 AND `source_type` = 0;
