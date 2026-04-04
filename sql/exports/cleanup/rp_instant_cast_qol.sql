-- ============================================================================
-- RoleplayCore: Instant/Fast Cast QoL Changes
-- Makes utility spells instant or faster for RP server quality-of-life.
--
-- CastingTimeIndex reference:
--   1  = 0ms    (instant)
--   6  = 5000ms (5s)
--   7  = 10000ms (10s)  <-- default for most of these
--   14 = 3000ms (3s)
--
-- Applied to: hotfixes.spell_misc
-- ============================================================================
USE `hotfixes`;
-- ---------------------------------------------------------------------------
-- Switch Flight Style — instant
-- ---------------------------------------------------------------------------
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 436854; -- Switch Flight Style

-- ---------------------------------------------------------------------------
-- Hearthstones (base + all toy variants) — instant
-- ---------------------------------------------------------------------------
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 8690;    -- Hearthstone
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 556;     -- Astral Recall
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 222695;  -- Dalaran Hearthstone
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 171253;  -- Garrison Hearthstone
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 96333;   -- Hero's Hearthstone
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 96334;   -- Veteran's Hearthstone
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 278244;  -- Greatfather Winter's Hearthstone
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 278559;  -- Headless Horseman's Hearthstone
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 285362;  -- Lunar Elder's Hearthstone
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 285424;  -- Peddlefeet's Lovely Hearthstone
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 286031;  -- Noble Gardener's Hearthstone
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 286331;  -- Fire Eater's Hearthstone
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 286353;  -- Brewfest Reveler's Hearthstone
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 298068;  -- Holographic Digitalization Hearthstone
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 308742;  -- Eternal Traveler's Hearthstone
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 326064;  -- Ardenweald Hearthstone
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 340200;  -- Necrolord Hearthstone
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 345393;  -- Kyrian Hearthstone
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 363799;  -- Dominated Hearthstone
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 366945;  -- Enlightened Hearthstone
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 375357;  -- Timewalker's Hearthstone
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 391042;  -- Ohn'ir Windsage's Hearthstone
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 405521;  -- Morqut Hearth Totem
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 420418;  -- Deepdweller's Earthen Hearthstone
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 421147;  -- Medivh's Hearthstone
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 422284;  -- Hearthstone of the Flame
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 431644;  -- Stone of the Hearth
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 463481;  -- Notorious Thread's Hearthstone
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 1220729; -- Explosive Hearthstone
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 1240219; -- P.O.S.T. Master's Express
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 1241367; -- Demonic Hearthstone
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 1242509; -- Cosmic Hearthstone
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 1250878; -- Timerunner's Hearthstone
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 1261979; -- Lightcalled Hearthstone
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 1270814; -- Preyseeker's Hearthstone
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 1273401; -- Corewarden's Hearthstone

-- ---------------------------------------------------------------------------
-- Out-of-Combat Resurrections — instant
-- ---------------------------------------------------------------------------
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 2006;    -- Resurrection (Priest)
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 7328;    -- Redemption (Paladin)
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 2008;    -- Ancestral Spirit (Shaman)
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 50769;   -- Revive (Druid)
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 361227;  -- Return (Evoker)

-- ---------------------------------------------------------------------------
-- Mass Resurrections — instant
-- ---------------------------------------------------------------------------
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 212036;  -- Mass Resurrection
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 461532;  -- Mass Resurrection (12.x)
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 361178;  -- Mass Return (Evoker)

-- ---------------------------------------------------------------------------
-- Mage Teleports (self) — instant
-- ---------------------------------------------------------------------------
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 3561;    -- Teleport: Stormwind
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 3562;    -- Teleport: Ironforge
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 3563;    -- Teleport: Undercity
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 3565;    -- Teleport: Darnassus
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 3566;    -- Teleport: Thunder Bluff
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 3567;    -- Teleport: Orgrimmar
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 18960;   -- Teleport: Moonglade
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 32271;   -- Teleport: Exodar
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 32272;   -- Teleport: Silvermoon
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 33690;   -- Teleport: Shattrath
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 49358;   -- Teleport: Stonard
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 49359;   -- Teleport: Theramore
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 53140;   -- Teleport: Dalaran - Northrend
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 88342;   -- Teleport: Tol Barad
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 132621;  -- Teleport: Vale of Eternal Blossoms
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 176242;  -- Teleport: Warspear
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 176248;  -- Teleport: Stormshield
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 224869;  -- Teleport: Dalaran - Broken Isles
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 281404;  -- Teleport: Dazar'alor
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 344587;  -- Teleport: Oribos
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 395277;  -- Teleport: Valdrakken
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 446540;  -- Teleport: Dornogal
UPDATE `spell_misc` SET `CastingTimeIndex` = 1 WHERE `SpellID` = 1259190; -- Teleport: Silvermoon City

-- ---------------------------------------------------------------------------
-- Mage Portals (group) — 3s cast
-- ---------------------------------------------------------------------------
UPDATE `spell_misc` SET `CastingTimeIndex` = 14 WHERE `SpellID` = 10059;   -- Portal: Stormwind
UPDATE `spell_misc` SET `CastingTimeIndex` = 14 WHERE `SpellID` = 11416;   -- Portal: Ironforge
UPDATE `spell_misc` SET `CastingTimeIndex` = 14 WHERE `SpellID` = 11417;   -- Portal: Orgrimmar
UPDATE `spell_misc` SET `CastingTimeIndex` = 14 WHERE `SpellID` = 11418;   -- Portal: Undercity
UPDATE `spell_misc` SET `CastingTimeIndex` = 14 WHERE `SpellID` = 11419;   -- Portal: Darnassus
UPDATE `spell_misc` SET `CastingTimeIndex` = 14 WHERE `SpellID` = 11420;   -- Portal: Thunder Bluff
UPDATE `spell_misc` SET `CastingTimeIndex` = 14 WHERE `SpellID` = 32266;   -- Portal: Exodar
UPDATE `spell_misc` SET `CastingTimeIndex` = 14 WHERE `SpellID` = 32267;   -- Portal: Silvermoon
UPDATE `spell_misc` SET `CastingTimeIndex` = 14 WHERE `SpellID` = 33691;   -- Portal: Shattrath
UPDATE `spell_misc` SET `CastingTimeIndex` = 14 WHERE `SpellID` = 49360;   -- Portal: Theramore
UPDATE `spell_misc` SET `CastingTimeIndex` = 14 WHERE `SpellID` = 49361;   -- Portal: Stonard
UPDATE `spell_misc` SET `CastingTimeIndex` = 14 WHERE `SpellID` = 53142;   -- Portal: Dalaran - Northrend
UPDATE `spell_misc` SET `CastingTimeIndex` = 14 WHERE `SpellID` = 120146;  -- Portal: Ancient Dalaran
UPDATE `spell_misc` SET `CastingTimeIndex` = 14 WHERE `SpellID` = 28148;   -- Portal: Karazhan
UPDATE `spell_misc` SET `CastingTimeIndex` = 14 WHERE `SpellID` = 73324;   -- Portal: Dalaran
UPDATE `spell_misc` SET `CastingTimeIndex` = 14 WHERE `SpellID` = 88345;   -- Portal: Tol Barad
UPDATE `spell_misc` SET `CastingTimeIndex` = 14 WHERE `SpellID` = 132620;  -- Portal: Vale of Eternal Blossoms
UPDATE `spell_misc` SET `CastingTimeIndex` = 14 WHERE `SpellID` = 224871;  -- Portal: Dalaran - Broken Isles
UPDATE `spell_misc` SET `CastingTimeIndex` = 14 WHERE `SpellID` = 281402;  -- Portal: Dazar'alor
UPDATE `spell_misc` SET `CastingTimeIndex` = 14 WHERE `SpellID` = 446534;  -- Portal: Dornogal
UPDATE `spell_misc` SET `CastingTimeIndex` = 14 WHERE `SpellID` = 1259194; -- Portal: Silvermoon City
