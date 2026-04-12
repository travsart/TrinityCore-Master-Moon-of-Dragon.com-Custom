-- 2026_04_04_09_world.sql
-- Warlock Phase 5 Tier C: Class utility spell handlers

-- Demon Skin (219272) — passive Soul Leech shield regeneration via PERIODIC_DUMMY
-- Soul Link (108415) — damage split to pet via area aura
INSERT IGNORE INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(219272, 'spell_warl_demon_skin'),
(108415, 'spell_warl_soul_link');
