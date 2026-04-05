-- Summon Demonic Tyrant (265187) spell script binding
DELETE FROM `spell_script_names` WHERE `spell_id` = 265187 AND `ScriptName` = 'spell_warl_summon_demonic_tyrant';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES (265187, 'spell_warl_summon_demonic_tyrant');

-- Link Demonic Tyrant creature to its PetAI (registered in C++ but missing DB binding)
UPDATE `creature_template` SET `ScriptName` = 'npc_pet_warlock_demonic_tyrant' WHERE `entry` = 135002;
