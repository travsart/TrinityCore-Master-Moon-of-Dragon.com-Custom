-- Dracthyr Non-Evoker: Grant Altered Form (Visage) spell at character creation
-- Non-Evoker Dracthyr (added in The War Within) were missing this spell.
-- Evokers already receive it via the existing classMask=4096 row.
--
-- raceMask  98304 = both Dracthyr races (Alliance + Horde)
-- classMask  4095 = all classes 1-12 (everything except Evoker, class 13 = bit 12)

DELETE FROM `playercreateinfo_cast_spell` WHERE `raceMask`=98304 AND `classMask`=4095 AND `spell`=97709;
INSERT INTO `playercreateinfo_cast_spell` (`raceMask`, `classMask`, `createMode`, `spell`, `note`) VALUES
(98304, 4095, 0, 97709, 'Dracthyr - Non-Evoker - Altered Form');
