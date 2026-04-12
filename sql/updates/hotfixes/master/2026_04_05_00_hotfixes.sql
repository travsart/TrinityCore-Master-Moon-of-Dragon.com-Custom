-- Chrono Surge (1900030) & Swift Crusade (1900031) — COMPLETE hotfix spell data
-- These are GM buff spells. ALL data lives in hotfixes (NOT serverside_spell).
-- See: MEMORY.md "Custom spells: NOT serverside_spell if in sSpellNameStore"
--
-- Chrono Surge: +250% haste (attack+cast speed), -75ms cooldown reduction
--   Icon: Bloodlust (136012)
-- Swift Crusade: +100% run speed, +200% mounted speed, +200% flight speed
--   Icon: Crusader Aura (135890)

-- ============================================================================
-- Step 1: Clean up conflicting serverside_spell entries (world DB)
-- These MUST be removed or the effects silently fail with:
--   "Serverside spell X references a regular spell loaded from file"
-- ============================================================================
-- (Applied separately to world DB — see companion world SQL file)

-- ============================================================================
-- Step 2: spell_name — tells the client these spells exist
-- ============================================================================
DELETE FROM `spell_name` WHERE `ID` IN (1900030, 1900031);
INSERT INTO `spell_name` (`ID`, `Name`, `VerifiedBuild`) VALUES
(1900030, 'Chrono Surge', 66709),
(1900031, 'Swift Crusade', 66709);

-- ============================================================================
-- Step 3: spell_misc — icon, duration, range, attributes
-- ============================================================================
DELETE FROM `spell_misc` WHERE `SpellID` IN (1900030, 1900031);
INSERT INTO `spell_misc` (`ID`, `Attributes1`, `Attributes2`, `Attributes3`, `Attributes4`, `Attributes5`, `Attributes6`, `Attributes7`, `Attributes8`, `Attributes9`, `Attributes10`, `Attributes11`, `Attributes12`, `Attributes13`, `Attributes14`, `Attributes15`, `Attributes16`, `Attributes17`, `DifficultyID`, `CastingTimeIndex`, `DurationIndex`, `PvPDurationIndex`, `RangeIndex`, `SchoolMask`, `Speed`, `LaunchDelay`, `MinDuration`, `SpellIconFileDataID`, `ActiveIconFileDataID`, `ContentTuningID`, `ShowFutureSpellPlayerConditionID`, `SpellVisualScript`, `ActiveSpellVisualScript`, `SpellID`, `VerifiedBuild`) VALUES
-- Chrono Surge: icon=Bloodlust, duration=infinite (21), range=self (1), no combat restriction
(1900030, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 21, 0, 1, 0, 0, 0, 0, 136012, 0, 0, 0, 0, 0, 1900030, 66709),
-- Swift Crusade: icon=Crusader Aura, duration=infinite (21), range=self (1), no combat restriction
(1900031, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 21, 0, 1, 0, 0, 0, 0, 135890, 0, 0, 0, 0, 0, 1900031, 66709);

-- ============================================================================
-- Step 4: spell_effect — the actual aura effects (THIS WAS MISSING)
-- Without these, the spell casts but does absolutely nothing.
-- IDs 1900300-1900312 are in our custom range (max existing: 1307730)
-- ============================================================================
DELETE FROM `spell_effect` WHERE `SpellID` IN (1900030, 1900031);
INSERT INTO `spell_effect` (`ID`, `EffectAura`, `DifficultyID`, `EffectIndex`, `Effect`, `EffectAmplitude`, `EffectAttributes`, `EffectAuraPeriod`, `EffectBonusCoefficient`, `EffectChainAmplitude`, `EffectChainTargets`, `EffectItemType`, `EffectMechanic`, `EffectPointsPerResource`, `EffectPosFacing`, `EffectRealPointsPerLevel`, `EffectTriggerSpell`, `BonusCoefficientFromAP`, `PvpMultiplier`, `Coefficient`, `Variance`, `ResourceCoefficient`, `GroupSizeBasePointsCoefficient`, `EffectBasePoints`, `ScalingClass`, `TargetNodeGraph`, `EffectMiscValue1`, `EffectMiscValue2`, `EffectRadiusIndex1`, `EffectRadiusIndex2`, `EffectSpellClassMask1`, `EffectSpellClassMask2`, `EffectSpellClassMask3`, `EffectSpellClassMask4`, `ImplicitTarget1`, `ImplicitTarget2`, `SpellID`, `VerifiedBuild`) VALUES
-- Chrono Surge effect 0: +250% haste (aura 193 = SPELL_AURA_MELEE_SLOW = combat speed pct, same as Bloodlust)
(1900300, 193, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 250, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1900030, 66709),
-- Chrono Surge effect 1: -75ms cooldown reduction (aura 196 = SPELL_AURA_MOD_COOLDOWN, flat ms)
(1900301, 196, 0, 1, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -75, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1900030, 66709),
-- Swift Crusade effect 0: +100% run speed (aura 129 = SPELL_AURA_MOD_SPEED_ALWAYS)
(1900310, 129, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1900031, 66709),
-- Swift Crusade effect 1: +200% mounted ground speed (aura 172 = SPELL_AURA_MOD_MOUNTED_SPEED_NOT_STACK)
(1900311, 172, 0, 1, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 200, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1900031, 66709),
-- Swift Crusade effect 2: +200% flight speed (aura 211 = SPELL_AURA_MOD_FLIGHT_SPEED_NOT_STACK)
(1900312, 211, 0, 2, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 200, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1900031, 66709);

-- ============================================================================
-- Step 5: hotfix_data — register ALL records so server pushes them to client
-- ============================================================================
-- TableHash for SpellName   = 0x46C66698 (1187407512)
-- TableHash for SpellMisc   = 0xC603EE28 (3322146344)
-- TableHash for SpellEffect = 4030871717
-- Status 1 = valid record
DELETE FROM `hotfix_data` WHERE `RecordId` IN (1900030, 1900031) AND `TableHash` IN (1187407512, 3322146344);
DELETE FROM `hotfix_data` WHERE `RecordId` IN (1900300, 1900301, 1900310, 1900311, 1900312) AND `TableHash` = 4030871717;
INSERT INTO `hotfix_data` (`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`) VALUES
-- SpellName entries
(1900030, 1900030, 1187407512, 1900030, 1, 66709),
(1900031, 1900031, 1187407512, 1900031, 1, 66709),
-- SpellMisc entries
(1900030, 1900030, 3322146344, 1900030, 1, 66709),
(1900031, 1900031, 3322146344, 1900031, 1, 66709),
-- SpellEffect entries (one per effect row)
(1900030, 1900030, 4030871717, 1900300, 1, 66709),
(1900030, 1900030, 4030871717, 1900301, 1, 66709),
(1900031, 1900031, 4030871717, 1900310, 1, 66709),
(1900031, 1900031, 4030871717, 1900311, 1, 66709),
(1900031, 1900031, 4030871717, 1900312, 1, 66709);
