-- ============================================================================
-- Enchant and Gem Recommendation Tables for Playerbot
-- These tables store per-class/spec enchant and gem recommendations
-- that bots use to optimally enchant and gem their equipment.
-- ============================================================================

DROP TABLE IF EXISTS `playerbot_enchant_recommendations`;
CREATE TABLE `playerbot_enchant_recommendations` (
    `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
    `enchant_id` INT UNSIGNED NOT NULL COMMENT 'SpellItemEnchantment DBC ID',
    `slot_type` TINYINT UNSIGNED NOT NULL COMMENT 'EnchantSlotType: 0=HEAD,1=SHOULDER,2=BACK,3=CHEST,4=WRIST,5=HANDS,6=LEGS,7=FEET,8=MAIN_HAND,9=OFF_HAND,10=RING_1,11=RING_2',
    `class_id` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=any class, 1-13=specific class',
    `spec_id` TINYINT UNSIGNED NOT NULL DEFAULT 255 COMMENT '255=any spec, 0-3=specific spec index',
    `min_item_level` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Minimum item level (0=no minimum)',
    `priority_weight` FLOAT NOT NULL DEFAULT 1.0 COMMENT 'Higher = more preferred',
    `enchant_name` VARCHAR(100) NOT NULL DEFAULT '' COMMENT 'Human-readable enchant name',
    PRIMARY KEY (`id`),
    KEY `idx_slot` (`slot_type`),
    KEY `idx_class_spec` (`class_id`, `spec_id`),
    KEY `idx_enchant` (`enchant_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Bot enchant recommendations per class/spec/slot';

DROP TABLE IF EXISTS `playerbot_gem_recommendations`;
CREATE TABLE `playerbot_gem_recommendations` (
    `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
    `gem_item_id` INT UNSIGNED NOT NULL COMMENT 'Gem item entry from item_template',
    `socket_color` TINYINT UNSIGNED NOT NULL COMMENT 'GemColor: 0=RED,1=BLUE,2=YELLOW,3=PRISMATIC,4=META,5=COGWHEEL',
    `class_id` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=any class, 1-13=specific class',
    `spec_id` TINYINT UNSIGNED NOT NULL DEFAULT 255 COMMENT '255=any spec, 0-3=specific spec index',
    `stat_priority` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'GemStatPriority: 0=PRIMARY,1=SECONDARY_BEST,2=SECONDARY_SECOND,3=STAMINA,4=MIXED',
    `priority_weight` FLOAT NOT NULL DEFAULT 1.0 COMMENT 'Higher = more preferred',
    `gem_name` VARCHAR(100) NOT NULL DEFAULT '' COMMENT 'Human-readable gem name',
    PRIMARY KEY (`id`),
    KEY `idx_socket` (`socket_color`),
    KEY `idx_class_spec` (`class_id`, `spec_id`),
    KEY `idx_gem` (`gem_item_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Bot gem recommendations per class/spec/socket color';

-- ============================================================================
-- Default Data: Weapon Enchants by Role (WoW 12.0 The War Within)
-- NOTE: enchant_id values are representative SpellItemEnchantment IDs.
-- Verify against DBC/DB2 data for actual server.
-- ============================================================================

INSERT INTO `playerbot_enchant_recommendations` (`enchant_id`, `slot_type`, `class_id`, `spec_id`, `min_item_level`, `priority_weight`, `enchant_name`) VALUES
-- Strength DPS weapon enchant (slot_type=8 = MAIN_HAND)
(7446, 8, 1, 255, 0, 1.0, 'Authority of Fiery Resolve'),   -- Warrior
(7446, 8, 6, 255, 0, 1.0, 'Authority of Fiery Resolve'),   -- Death Knight
(7446, 8, 2, 2, 0, 1.0, 'Authority of Fiery Resolve'),     -- Ret Paladin

-- Agility DPS weapon enchant
(7448, 8, 4, 255, 0, 1.0, 'Authority of the Depths'),      -- Rogue
(7448, 8, 12, 0, 0, 1.0, 'Authority of the Depths'),       -- Havoc DH
(7448, 8, 10, 2, 0, 1.0, 'Authority of the Depths'),       -- WW Monk
(7448, 8, 11, 1, 0, 1.0, 'Authority of the Depths'),       -- Feral Druid
(7448, 8, 3, 2, 0, 1.0, 'Authority of the Depths'),        -- Survival Hunter

-- Intellect DPS weapon enchant
(7450, 8, 8, 255, 0, 1.0, 'Authority of Radiant Power'),   -- Mage
(7450, 8, 9, 255, 0, 1.0, 'Authority of Radiant Power'),   -- Warlock
(7450, 8, 5, 2, 0, 1.0, 'Authority of Radiant Power'),     -- Shadow Priest
(7450, 8, 7, 0, 0, 1.0, 'Authority of Radiant Power'),     -- Elemental Shaman
(7450, 8, 11, 0, 0, 1.0, 'Authority of Radiant Power'),    -- Balance Druid
(7450, 8, 13, 0, 0, 1.0, 'Authority of Radiant Power'),    -- Devastation Evoker

-- Healer weapon enchant
(7452, 8, 5, 0, 0, 1.0, 'Authority of Storms'),            -- Disc Priest
(7452, 8, 5, 1, 0, 1.0, 'Authority of Storms'),            -- Holy Priest
(7452, 8, 2, 0, 0, 1.0, 'Authority of Storms'),            -- Holy Paladin
(7452, 8, 11, 3, 0, 1.0, 'Authority of Storms'),           -- Resto Druid
(7452, 8, 7, 2, 0, 1.0, 'Authority of Storms'),            -- Resto Shaman
(7452, 8, 10, 1, 0, 1.0, 'Authority of Storms'),           -- MW Monk
(7452, 8, 13, 1, 0, 1.0, 'Authority of Storms'),           -- Preservation Evoker

-- Tank weapon enchant
(7454, 8, 1, 2, 0, 1.0, 'Authority of Nerubian Fortification'),  -- Prot Warrior
(7454, 8, 2, 1, 0, 1.0, 'Authority of Nerubian Fortification'),  -- Prot Paladin
(7454, 8, 6, 0, 0, 1.0, 'Authority of Nerubian Fortification'),  -- Blood DK
(7454, 8, 12, 1, 0, 1.0, 'Authority of Nerubian Fortification'), -- Vengeance DH
(7454, 8, 11, 2, 0, 1.0, 'Authority of Nerubian Fortification'), -- Guardian Druid
(7454, 8, 10, 0, 0, 1.0, 'Authority of Nerubian Fortification'); -- Brewmaster Monk

-- ============================================================================
-- Default Data: Armor Enchants (Generic)
-- ============================================================================

INSERT INTO `playerbot_enchant_recommendations` (`enchant_id`, `slot_type`, `class_id`, `spec_id`, `min_item_level`, `priority_weight`, `enchant_name`) VALUES
-- Back (slot_type=2)
(7401, 2, 0, 255, 0, 1.0, 'Chant of Winged Grace'),
(7403, 2, 0, 255, 0, 0.8, 'Chant of Leeching Fangs'),

-- Chest (slot_type=3)
(7405, 3, 0, 255, 0, 1.0, 'Crystalline Radiance'),

-- Wrist (slot_type=4)
(7391, 4, 0, 255, 0, 1.0, 'Chant of Armored Avoidance'),
(7393, 4, 0, 255, 0, 0.8, 'Chant of Armored Leech'),

-- Legs (slot_type=6)
(7531, 6, 0, 255, 0, 1.0, 'Stormbound Armor Kit'),
(7534, 6, 0, 255, 0, 0.9, 'Defenders Armor Kit'),

-- Feet (slot_type=7)
(7416, 7, 0, 255, 0, 1.0, 'Defenders March'),
(7418, 7, 0, 255, 0, 0.9, 'Scouts March'),

-- Ring 1 (slot_type=10)
(7330, 10, 0, 255, 0, 1.0, 'Radiant Critical Strike'),
(7332, 10, 0, 255, 0, 0.9, 'Radiant Haste'),
(7334, 10, 0, 255, 0, 0.85, 'Radiant Mastery'),
(7336, 10, 0, 255, 0, 0.8, 'Radiant Versatility'),

-- Ring 2 (slot_type=11)
(7330, 11, 0, 255, 0, 1.0, 'Radiant Critical Strike'),
(7332, 11, 0, 255, 0, 0.9, 'Radiant Haste'),
(7334, 11, 0, 255, 0, 0.85, 'Radiant Mastery'),
(7336, 11, 0, 255, 0, 0.8, 'Radiant Versatility');

-- ============================================================================
-- Default Data: Gem Recommendations
-- ============================================================================

INSERT INTO `playerbot_gem_recommendations` (`gem_item_id`, `socket_color`, `class_id`, `spec_id`, `stat_priority`, `priority_weight`, `gem_name`) VALUES
-- Generic prismatic gems (socket_color=3 = PRISMATIC)
(213746, 3, 0, 255, 0, 1.0, 'Masterful Ruby'),
(213743, 3, 0, 255, 1, 0.9, 'Quick Sapphire'),
(213744, 3, 0, 255, 2, 0.85, 'Deadly Emerald'),
(213745, 3, 0, 255, 3, 0.7, 'Solid Onyx'),

-- Strength DPS gems
(213755, 3, 1, 0, 0, 1.0, 'Bold Amber Crit'),       -- Arms Warrior
(213755, 3, 1, 1, 0, 1.0, 'Bold Amber Haste'),       -- Fury Warrior
(213755, 3, 6, 1, 0, 1.0, 'Bold Amber Mastery'),     -- Frost DK
(213755, 3, 6, 2, 0, 1.0, 'Bold Amber Haste'),       -- Unholy DK
(213755, 3, 2, 2, 0, 1.0, 'Bold Amber Haste'),       -- Ret Paladin

-- Agility DPS gems
(213757, 3, 4, 255, 0, 1.0, 'Delicate Amber'),       -- Rogue all specs
(213757, 3, 12, 0, 0, 1.0, 'Delicate Amber'),        -- Havoc DH
(213757, 3, 11, 1, 0, 1.0, 'Delicate Amber'),        -- Feral Druid
(213757, 3, 10, 2, 0, 1.0, 'Delicate Amber'),        -- WW Monk

-- Intellect caster gems
(213759, 3, 8, 255, 0, 1.0, 'Brilliant Amber'),      -- Mage all specs
(213759, 3, 9, 255, 0, 1.0, 'Brilliant Amber'),      -- Warlock all specs
(213759, 3, 5, 255, 0, 1.0, 'Brilliant Amber'),      -- Priest all specs
(213759, 3, 7, 255, 0, 1.0, 'Brilliant Amber'),      -- Shaman all specs
(213759, 3, 13, 255, 0, 1.0, 'Brilliant Amber'),     -- Evoker all specs

-- Tank gems (stamina focus)
(213761, 3, 1, 2, 3, 1.0, 'Solid Amber Stam'),       -- Prot Warrior
(213761, 3, 2, 1, 3, 1.0, 'Solid Amber Stam'),       -- Prot Paladin
(213761, 3, 6, 0, 3, 1.0, 'Solid Amber Stam'),       -- Blood DK
(213761, 3, 12, 1, 3, 1.0, 'Solid Amber Stam'),      -- Vengeance DH
(213761, 3, 11, 2, 3, 1.0, 'Solid Amber Stam'),      -- Guardian Druid
(213761, 3, 10, 0, 3, 1.0, 'Solid Amber Stam');       -- Brewmaster Monk
