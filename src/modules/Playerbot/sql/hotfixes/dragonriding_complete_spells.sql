-- ============================================================================
-- Playerbot Dragonriding System - Complete Custom Spell Data
--
-- Creates ALL custom spells required for dragonriding system.
-- Server needs spell_name + spell_misc + spell_effect for each spell to work.
--
-- Spell IDs:
--   900001 - Vigor Buff (hidden aura tracking vigor stacks)
--   900002 - Surge Forward (speed boost ability on action bar)
--   900003 - Skyward Ascent (vertical thrust ability on action bar)
--   900004 - Whirling Surge (barrel roll, talent-locked)
--   900005 - Aerial Halt (hover, talent-locked)
--   900006 - Surge Forward Buff (speed boost effect)
--   900007 - Skyward Ascent Buff (vertical thrust effect)
--   900008 - Thrill of the Skies (high-speed regen visual)
--   900009 - Ground Skimming (near-ground regen visual)
--
-- Install into: hotfixes database
-- Restart worldserver after installation
-- ============================================================================

SET @BUILD := 58941;
SET @HOTFIX_ID := 1000001;

-- Icon IDs from existing database
SET @ICON_VIGOR := 136243;      -- Generic buff icon
SET @ICON_FORWARD := 136243;    -- Generic ability icon
SET @ICON_UPWARD := 136243;     -- Generic ability icon
SET @ICON_WHIRL := 136243;      -- Generic ability icon
SET @ICON_HALT := 136243;       -- Generic ability icon
SET @ICON_BUFF := 136243;       -- Generic buff icon

-- ============================================================================
-- STEP 1: SPELL NAMES (all 9 spells)
-- ============================================================================
DELETE FROM `spell_name` WHERE `ID` IN (900001, 900002, 900003, 900004, 900005, 900006, 900007, 900008, 900009);
INSERT INTO `spell_name` (`ID`, `Name`, `VerifiedBuild`) VALUES
(900001, 'Vigor',               @BUILD),
(900002, 'Surge Forward',       @BUILD),
(900003, 'Skyward Ascent',      @BUILD),
(900004, 'Whirling Surge',      @BUILD),
(900005, 'Aerial Halt',         @BUILD),
(900006, 'Surge Forward',       @BUILD),
(900007, 'Skyward Ascent',      @BUILD),
(900008, 'Thrill of the Skies', @BUILD),
(900009, 'Ground Skimming',     @BUILD);

-- ============================================================================
-- STEP 2: SPELL MISC (icons, cast time, etc)
-- ============================================================================
DELETE FROM `spell_misc` WHERE `ID` IN (900001, 900002, 900003, 900004, 900005, 900006, 900007, 900008, 900009);
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
-- 900001: Vigor buff (hidden stacking aura)
(900001, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0, 1, 0, 0, 1, 1, 0, 0, 0, @ICON_VIGOR, 0, 0, 0, 0, 0, 900001, @BUILD),
-- 900002-900005: Action bar abilities
(900002, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0, 1, 0, 0, 1, 1, 0, 0, 0, @ICON_FORWARD, 0, 0, 0, 0, 0, 900002, @BUILD),
(900003, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0, 1, 0, 0, 1, 1, 0, 0, 0, @ICON_UPWARD, 0, 0, 0, 0, 0, 900003, @BUILD),
(900004, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0, 1, 0, 0, 1, 1, 0, 0, 0, @ICON_WHIRL, 0, 0, 0, 0, 0, 900004, @BUILD),
(900005, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0, 1, 0, 0, 1, 1, 0, 0, 0, @ICON_HALT, 0, 0, 0, 0, 0, 900005, @BUILD),
-- 900006-900009: Buff effects
(900006, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0, 1, 0, 0, 1, 1, 0, 0, 0, @ICON_BUFF, 0, 0, 0, 0, 0, 900006, @BUILD),
(900007, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0, 1, 0, 0, 1, 1, 0, 0, 0, @ICON_BUFF, 0, 0, 0, 0, 0, 900007, @BUILD),
(900008, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0, 1, 0, 0, 1, 1, 0, 0, 0, @ICON_BUFF, 0, 0, 0, 0, 0, 900008, @BUILD),
(900009, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0, 1, 0, 0, 1, 1, 0, 0, 0, @ICON_BUFF, 0, 0, 0, 0, 0, 900009, @BUILD);

-- ============================================================================
-- STEP 3: SPELL EFFECTS (required for spell to be valid)
-- Effect=3 (SPELL_EFFECT_DUMMY), ImplicitTarget1=1 (TARGET_UNIT_CASTER)
-- 900001 uses SPELL_EFFECT_APPLY_AURA (6) with SPELL_AURA_DUMMY (4) for stacking
-- ============================================================================
DELETE FROM `spell_effect` WHERE `ID` IN (900001, 900002, 900003, 900004, 900005, 900006, 900007, 900008, 900009);
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
-- 900001: Vigor - Effect=6 (APPLY_AURA), Aura=4 (DUMMY) for stacking aura
(900001, 4, 0, 0, 6, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 1, 0, 900001, @BUILD),
-- 900002-900005: Action bar abilities - Effect=3 (DUMMY)
(900002, 0, 0, 0, 3, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 1, 0, 900002, @BUILD),
(900003, 0, 0, 0, 3, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 1, 0, 900003, @BUILD),
(900004, 0, 0, 0, 3, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 1, 0, 900004, @BUILD),
(900005, 0, 0, 0, 3, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 1, 0, 900005, @BUILD),
-- 900006-900009: Buffs - Effect=6 (APPLY_AURA), Aura=4 (DUMMY)
(900006, 4, 0, 0, 6, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 1, 0, 900006, @BUILD),
(900007, 4, 0, 0, 6, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 1, 0, 900007, @BUILD),
(900008, 4, 0, 0, 6, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 1, 0, 900008, @BUILD),
(900009, 4, 0, 0, 6, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 1, 0, 900009, @BUILD);

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
-- TableHashes:
--   spell_name: 1187407512
--   spell_misc: 3322146344
--   spell_effect: 4030871717
--   override_spell_data: 3396722460
-- ============================================================================

-- Clean up old hotfix entries
DELETE FROM `hotfix_data` WHERE `TableHash` = 1187407512 AND `RecordId` IN (900001, 900002, 900003, 900004, 900005, 900006, 900007, 900008, 900009);
DELETE FROM `hotfix_data` WHERE `TableHash` = 3322146344 AND `RecordId` IN (900001, 900002, 900003, 900004, 900005, 900006, 900007, 900008, 900009);
DELETE FROM `hotfix_data` WHERE `TableHash` = 4030871717 AND `RecordId` IN (900001, 900002, 900003, 900004, 900005, 900006, 900007, 900008, 900009);
DELETE FROM `hotfix_data` WHERE `TableHash` = 3396722460 AND `RecordId` = 900001;

-- spell_name hotfixes (TableHash: 1187407512)
INSERT INTO `hotfix_data` (`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`) VALUES
(@HOTFIX_ID+1,  @HOTFIX_ID+1,  1187407512, 900001, 1, @BUILD),
(@HOTFIX_ID+2,  @HOTFIX_ID+2,  1187407512, 900002, 1, @BUILD),
(@HOTFIX_ID+3,  @HOTFIX_ID+3,  1187407512, 900003, 1, @BUILD),
(@HOTFIX_ID+4,  @HOTFIX_ID+4,  1187407512, 900004, 1, @BUILD),
(@HOTFIX_ID+5,  @HOTFIX_ID+5,  1187407512, 900005, 1, @BUILD),
(@HOTFIX_ID+6,  @HOTFIX_ID+6,  1187407512, 900006, 1, @BUILD),
(@HOTFIX_ID+7,  @HOTFIX_ID+7,  1187407512, 900007, 1, @BUILD),
(@HOTFIX_ID+8,  @HOTFIX_ID+8,  1187407512, 900008, 1, @BUILD),
(@HOTFIX_ID+9,  @HOTFIX_ID+9,  1187407512, 900009, 1, @BUILD);

-- spell_misc hotfixes (TableHash: 3322146344)
INSERT INTO `hotfix_data` (`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`) VALUES
(@HOTFIX_ID+11, @HOTFIX_ID+11, 3322146344, 900001, 1, @BUILD),
(@HOTFIX_ID+12, @HOTFIX_ID+12, 3322146344, 900002, 1, @BUILD),
(@HOTFIX_ID+13, @HOTFIX_ID+13, 3322146344, 900003, 1, @BUILD),
(@HOTFIX_ID+14, @HOTFIX_ID+14, 3322146344, 900004, 1, @BUILD),
(@HOTFIX_ID+15, @HOTFIX_ID+15, 3322146344, 900005, 1, @BUILD),
(@HOTFIX_ID+16, @HOTFIX_ID+16, 3322146344, 900006, 1, @BUILD),
(@HOTFIX_ID+17, @HOTFIX_ID+17, 3322146344, 900007, 1, @BUILD),
(@HOTFIX_ID+18, @HOTFIX_ID+18, 3322146344, 900008, 1, @BUILD),
(@HOTFIX_ID+19, @HOTFIX_ID+19, 3322146344, 900009, 1, @BUILD);

-- spell_effect hotfixes (TableHash: 4030871717)
INSERT INTO `hotfix_data` (`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`) VALUES
(@HOTFIX_ID+21, @HOTFIX_ID+21, 4030871717, 900001, 1, @BUILD),
(@HOTFIX_ID+22, @HOTFIX_ID+22, 4030871717, 900002, 1, @BUILD),
(@HOTFIX_ID+23, @HOTFIX_ID+23, 4030871717, 900003, 1, @BUILD),
(@HOTFIX_ID+24, @HOTFIX_ID+24, 4030871717, 900004, 1, @BUILD),
(@HOTFIX_ID+25, @HOTFIX_ID+25, 4030871717, 900005, 1, @BUILD),
(@HOTFIX_ID+26, @HOTFIX_ID+26, 4030871717, 900006, 1, @BUILD),
(@HOTFIX_ID+27, @HOTFIX_ID+27, 4030871717, 900007, 1, @BUILD),
(@HOTFIX_ID+28, @HOTFIX_ID+28, 4030871717, 900008, 1, @BUILD),
(@HOTFIX_ID+29, @HOTFIX_ID+29, 4030871717, 900009, 1, @BUILD);

-- override_spell_data hotfix (TableHash: 3396722460)
INSERT INTO `hotfix_data` (`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`) VALUES
(@HOTFIX_ID+30, @HOTFIX_ID+30, 3396722460, 900001, 1, @BUILD);
