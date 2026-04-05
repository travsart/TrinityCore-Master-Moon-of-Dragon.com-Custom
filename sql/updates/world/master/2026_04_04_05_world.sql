-- Warlock spell_script_names cleanup: remove ghost entries for scripts that no longer exist
-- These are leftovers from Freakz fork code that was modernized or removed

-- Ghost entries: no matching C++ class exists
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_warlock_demonbolt_new';   -- 264178, replaced by spell_warl_demonbolt
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_warlock_drain_life';      -- 234153, replaced by spell_warl_drain_life
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_warlock_soul_fire';       -- 6353, replaced by spell_warl_soul_fire
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_warlock_inquisitors_gaze'; -- 386344, class removed

-- Doom: disabled in 12.x (EFFECT_0 is DUMMY, not PERIODIC_DAMAGE), scripts exist but unregistered
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_warl_doom';               -- 603, registered but commented out
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_warlock_doom';            -- 603, #if 0 disabled
