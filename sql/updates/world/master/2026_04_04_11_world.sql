-- Warlock Phase 5 deep audit: fix orphan DB entries and name mismatches
-- These cause "Script not found" warnings at server startup

-- Fix 1: Rename eye_laser -> darkglare_eye_laser to match C++ class name
UPDATE `spell_script_names` SET `ScriptName` = 'spell_warl_darkglare_eye_laser'
WHERE `spell_id` = 205231 AND `ScriptName` = 'spell_warl_eye_laser';

-- Fix 2: Fix typo warr -> warl for shadowbolt affliction
UPDATE `spell_script_names` SET `ScriptName` = 'spell_warl_shadowbolt_affliction'
WHERE `spell_id` = 232670 AND `ScriptName` = 'spell_warr_shadowbolt_affliction';

-- Fix 3: Remove stale orphan entries (no matching C++ class exists)
DELETE FROM `spell_script_names` WHERE `spell_id` = 48181 AND `ScriptName` = 'aura_warl_haunt';
DELETE FROM `spell_script_names` WHERE `spell_id` = 17962 AND `ScriptName` = 'spell_warl_conflagrate_aura';
DELETE FROM `spell_script_names` WHERE `spell_id` = -85113 AND `ScriptName` = 'spell_warl_aftermath';
