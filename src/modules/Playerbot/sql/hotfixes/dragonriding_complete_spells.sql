-- ============================================================================
-- Playerbot Dragonriding System - Complete Custom Spell Data
--
-- Creates custom spells for dragonriding action bar override.
-- Server needs spell_name + spell_misc + spell_effect for spells to work.
--
-- Install into: hotfixes database
-- Restart worldserver after installation
-- ============================================================================

SET @BUILD := 58941;
SET @HOTFIX_ID := 1000001;

-- Known working icon IDs from existing database
SET @ICON_FORWARD := 136243;   -- Generic ability icon
SET @ICON_UPWARD := 136243;    -- Generic ability icon
SET @ICON_WHIRL := 136243;     -- Generic ability icon
SET @ICON_HALT := 136243;      -- Generic ability icon

-- ============================================================================
-- STEP 1: SPELL NAMES
-- ============================================================================
DELETE FROM `spell_name` WHERE `ID` IN (900002, 900003, 900004, 900005);
INSERT INTO `spell_name` (`ID`, `Name`, `VerifiedBuild`) VALUES
(900002, 'Surge Forward',    @BUILD),
(900003, 'Skyward Ascent',   @BUILD),
(900004, 'Whirling Surge',   @BUILD),
(900005, 'Aerial Halt',      @BUILD);

-- ============================================================================
-- STEP 2: SPELL MISC (icons, cast time, etc)
-- ============================================================================
DELETE FROM `spell_misc` WHERE `ID` IN (900002, 900003, 900004, 900005);
INSERT INTO `spell_misc` (
    `ID`,
    `Attributes1`, `Attributes2`, `Attributes3`, `Attributes4`, `Attributes5`,
    `Attributes6`, `Attributes7`, `Attributes8`, `Attributes9`, `Attributes10`,
    `Attributes11`, `Attributes12`, `Attributes13`, `Attributes14`, `Attributes15`, `Attributes16`,
    `DifficultyID`, `CastingTimeIndex`, `DurationIndex`, `PvPDurationIndex`, `RangeIndex`,
    `SchoolMask`, `Speed`, `LaunchDelay`, `MinDuration`,
    `SpellIconFileDataID`, `ActiveIconFileDataID`,
    `ContentTuningID`, `ShowFutureSpellPlayerConditionID`, `SpellVisualScript`, `ActiveSpellVisualScript`,
    `SpellID`, `VerifiedBuild`
) VALUES
(900002, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0, 1, 0, 0, 1, 1, 0, 0, 0, @ICON_FORWARD, 0, 0, 0, 0, 0, 900002, @BUILD),
(900003, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0, 1, 0, 0, 1, 1, 0, 0, 0, @ICON_UPWARD, 0, 0, 0, 0, 0, 900003, @BUILD),
(900004, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0, 1, 0, 0, 1, 1, 0, 0, 0, @ICON_WHIRL, 0, 0, 0, 0, 0, 900004, @BUILD),
(900005, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0, 1, 0, 0, 1, 1, 0, 0, 0, @ICON_HALT, 0, 0, 0, 0, 0, 900005, @BUILD);

-- ============================================================================
-- STEP 3: SPELL EFFECTS (required for spell to be valid)
-- Effect=3 (SPELL_EFFECT_DUMMY), ImplicitTarget1=1 (TARGET_UNIT_CASTER)
-- ============================================================================
DELETE FROM `spell_effect` WHERE `ID` IN (900002, 900003, 900004, 900005);
INSERT INTO `spell_effect` (
    `ID`, `EffectAura`, `DifficultyID`, `EffectIndex`, `Effect`,
    `EffectAmplitude`, `EffectAttributes`, `EffectAuraPeriod`,
    `EffectBonusCoefficient`, `EffectChainAmplitude`, `EffectChainTargets`,
    `EffectItemType`, `EffectMechanic`, `EffectPointsPerResource`,
    `EffectPosFacing`, `EffectRealPointsPerLevel`, `EffectTriggerSpell`,
    `BonusCoefficientFromAP`, `PvpMultiplier`, `Coefficient`, `Variance`,
    `ResourceCoefficient`, `GroupSizeBasePointsCoefficient`, `EffectBasePoints`,
    `ScalingClass`, `EffectMiscValue1`, `EffectMiscValue2`,
    `EffectRadiusIndex1`, `EffectRadiusIndex2`,
    `EffectSpellClassMask1`, `EffectSpellClassMask2`, `EffectSpellClassMask3`, `EffectSpellClassMask4`,
    `ImplicitTarget1`, `ImplicitTarget2`, `SpellID`, `VerifiedBuild`
) VALUES
(900002, 0, 0, 0, 3, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 1, 0, 900002, @BUILD),
(900003, 0, 0, 0, 3, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 1, 0, 900003, @BUILD),
(900004, 0, 0, 0, 3, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 1, 0, 900004, @BUILD),
(900005, 0, 0, 0, 3, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 1, 0, 900005, @BUILD);

-- ============================================================================
-- STEP 4: OVERRIDE_SPELL_DATA (action bar definition)
-- ============================================================================
DELETE FROM `override_spell_data` WHERE `ID` = 900001;
INSERT INTO `override_spell_data` (
    `ID`, `Spells1`, `Spells2`, `Spells3`, `Spells4`, `Spells5`,
    `Spells6`, `Spells7`, `Spells8`, `Spells9`, `Spells10`,
    `PlayerActionBarFileDataID`, `Flags`, `VerifiedBuild`
) VALUES (
    900001,
    900002, 900003, 900004, 900005, 0, 0, 0, 0, 0, 0,
    0, 0, @BUILD
);

-- ============================================================================
-- STEP 5: HOTFIX_DATA (tells server to send this to clients)
-- ============================================================================
DELETE FROM `hotfix_data` WHERE `TableHash` = 1187407512 AND `RecordId` IN (900002, 900003, 900004, 900005);
DELETE FROM `hotfix_data` WHERE `TableHash` = 3322146344 AND `RecordId` IN (900002, 900003, 900004, 900005);
DELETE FROM `hotfix_data` WHERE `TableHash` = 4030871717 AND `RecordId` IN (900002, 900003, 900004, 900005);
DELETE FROM `hotfix_data` WHERE `TableHash` = 3396722460 AND `RecordId` = 900001;

-- spell_name hotfixes (TableHash: 1187407512)
INSERT INTO `hotfix_data` (`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`) VALUES
(@HOTFIX_ID+1, @HOTFIX_ID+1, 1187407512, 900002, 1, @BUILD),
(@HOTFIX_ID+2, @HOTFIX_ID+2, 1187407512, 900003, 1, @BUILD),
(@HOTFIX_ID+3, @HOTFIX_ID+3, 1187407512, 900004, 1, @BUILD),
(@HOTFIX_ID+4, @HOTFIX_ID+4, 1187407512, 900005, 1, @BUILD);

-- spell_misc hotfixes (TableHash: 3322146344)
INSERT INTO `hotfix_data` (`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`) VALUES
(@HOTFIX_ID+11, @HOTFIX_ID+11, 3322146344, 900002, 1, @BUILD),
(@HOTFIX_ID+12, @HOTFIX_ID+12, 3322146344, 900003, 1, @BUILD),
(@HOTFIX_ID+13, @HOTFIX_ID+13, 3322146344, 900004, 1, @BUILD),
(@HOTFIX_ID+14, @HOTFIX_ID+14, 3322146344, 900005, 1, @BUILD);

-- spell_effect hotfixes (TableHash: 4030871717)
INSERT INTO `hotfix_data` (`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`) VALUES
(@HOTFIX_ID+21, @HOTFIX_ID+21, 4030871717, 900002, 1, @BUILD),
(@HOTFIX_ID+22, @HOTFIX_ID+22, 4030871717, 900003, 1, @BUILD),
(@HOTFIX_ID+23, @HOTFIX_ID+23, 4030871717, 900004, 1, @BUILD),
(@HOTFIX_ID+24, @HOTFIX_ID+24, 4030871717, 900005, 1, @BUILD);

-- override_spell_data hotfix (TableHash: 3396722460)
INSERT INTO `hotfix_data` (`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`) VALUES
(@HOTFIX_ID+30, @HOTFIX_ID+30, 3396722460, 900001, 1, @BUILD);
