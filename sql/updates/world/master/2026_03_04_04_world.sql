-- RoleplayCore — Add teleport destination for Founder's Point portal spell
-- Spell 1235595 "Portal to Alliance Housing District" now has a spell_effect
-- (Effect=15, ImplicitTarget2=17=DEST_DB) that reads from this table.
-- Coordinates: map 2735 (Founder's Point), copied from spell 1258476's entry.

INSERT IGNORE INTO `spell_target_position`
    (`ID`, `EffectIndex`, `OrderIndex`, `MapID`,
     `PositionX`, `PositionY`, `PositionZ`, `Orientation`, `VerifiedBuild`)
VALUES
    (1235595, 0, 0, 2735,
     3799, -158, 193, 2.92491, 66102);
