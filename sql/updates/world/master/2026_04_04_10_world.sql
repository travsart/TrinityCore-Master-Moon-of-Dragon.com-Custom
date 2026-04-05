-- Mayhem (387506) - Destruction Warlock talent (choice node alt to Havoc)
-- Chaos Bolt and Rain of Fire have a 35% chance to replicate to a nearby enemy
DELETE FROM spell_script_names WHERE spell_id = 387506;
INSERT INTO spell_script_names (spell_id, ScriptName) VALUES (387506, 'spell_warl_mayhem');
