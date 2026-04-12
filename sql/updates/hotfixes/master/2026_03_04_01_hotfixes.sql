-- RoleplayCore — Wire up Founder's Point portal spell (1235595)
-- The portal GO (entry 543407) in the Wizard's Sanctum casts spell 1235595
-- "Portal to Alliance Housing District" but it had zero spell_effect rows,
-- making it completely inert. This adds a SPELL_EFFECT_TELEPORT_UNITS (15)
-- effect so the portal actually teleports players to map 2735 (Founder's Point).
-- Destination coords copied from spell 1258476's spell_target_position.

-- Add spell effect: Effect=15 (TELEPORT_UNITS), ImplicitTarget1=1 (UNIT_CASTER),
-- ImplicitTarget2=17 (DEST_DB) — reads destination from world.spell_target_position
INSERT IGNORE INTO `spell_effect`
    (`ID`, `EffectAura`, `DifficultyID`, `EffectIndex`, `Effect`,
     `EffectAmplitude`, `EffectAttributes`, `EffectAuraPeriod`,
     `EffectBonusCoefficient`, `EffectChainAmplitude`, `EffectChainTargets`,
     `EffectItemType`, `EffectMechanic`, `EffectPointsPerResource`,
     `EffectPosFacing`, `EffectRealPointsPerLevel`, `EffectTriggerSpell`,
     `BonusCoefficientFromAP`, `PvpMultiplier`, `Coefficient`, `Variance`,
     `ResourceCoefficient`, `GroupSizeBasePointsCoefficient`, `EffectBasePoints`,
     `ScalingClass`, `TargetNodeGraph`,
     `EffectMiscValue1`, `EffectMiscValue2`,
     `EffectRadiusIndex1`, `EffectRadiusIndex2`,
     `EffectSpellClassMask1`, `EffectSpellClassMask2`, `EffectSpellClassMask3`, `EffectSpellClassMask4`,
     `ImplicitTarget1`, `ImplicitTarget2`, `SpellID`, `VerifiedBuild`)
VALUES
    (1900004, 0, 0, 0, 15,
     0, 0, 0,
     0, 1, 0,
     0, 0, 0,
     0, 0, 0,
     0, 1, 0, 0,
     0, 1, 0,
     0, 0,
     0, 0,
     0, 0,
     0, 0, 0, 0,
     1, 17, 1235595, 66102);

-- Register in hotfix_data so TC pushes this to the client
INSERT IGNORE INTO `hotfix_data`
    (`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`)
VALUES
    (17006229, 17006229, 4030871717, 1900004, 1, 0);
