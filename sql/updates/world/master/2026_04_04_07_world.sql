-- 2026_04_04_07_world.sql
-- Soul Harvester: Demonic Soul (450510) spell script binding

DELETE FROM `spell_script_names` WHERE `spell_id` = 450510;
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(450510, 'spell_warl_demonic_soul');
