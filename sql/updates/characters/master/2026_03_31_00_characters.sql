-- Drop legacy flat transmog outfit tables (replaced by normalized schema from 2026_03_21_00)
-- Old tables: character_transmog_outfits (flat 19-appearance columns)
--             character_transmog_outfit_situations (plural, used setguid FK)
-- New tables: character_transmog_outfit, character_transmog_outfit_situation (singular),
--             character_transmog_outfit_slot (per-slot rows with ADT/IDT)

DROP TABLE IF EXISTS `character_transmog_outfit_situations`;
DROP TABLE IF EXISTS `character_transmog_outfits`;
