-- ============================================================
-- Faction Validation Fixes
-- Generated: 2026-03-01 19:45:16
-- Only includes clear errors with safe fixes.
-- ============================================================

-- ── Service NPCs with Hostile-to-Both-Players Factions ──
-- These NPCs are vendors/trainers/etc. but hostile to ALL players.
-- This is almost certainly a data error.
-- Set to faction 35 (Friendly to all players).
USE `world`;
-- entry 28589: Gristlegut [Vendor] was faction 2068 (Undead, Scourge, hostile to both)
UPDATE creature_template SET faction = 35 WHERE entry = 28589; -- was 2068 (hostile to all players)
-- entry 132568: Volatile Violetscale [Vendor] was faction 2945 (8.0 Hidden Object - Red, hostile to both)
UPDATE creature_template SET faction = 35 WHERE entry = 132568; -- was 2945 (hostile to all players)
-- entry 132571: Little Carp [Vendor] was faction 2945 (8.0 Hidden Object - Red, hostile to both)
UPDATE creature_template SET faction = 35 WHERE entry = 132571; -- was 2945 (hostile to all players)

-- ── Service NPCs Hostile to ONE Player Faction (summary) ──
-- These are almost always intentional faction-locked NPCs
-- (e.g. Horde vendors hostile to Alliance players).
-- Hostile to Alliance only: 310 NPCs
-- Hostile to Horde only:    2209 NPCs
-- See faction_validation_report.txt for full per-NPC details.


-- Total fixes: 3
