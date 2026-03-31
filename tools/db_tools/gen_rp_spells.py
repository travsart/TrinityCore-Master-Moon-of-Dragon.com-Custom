#!/usr/bin/env python3
"""Generate all RP fun spells batch SQL with unique hotfix_data IDs."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "wago"))
sys.path.insert(0, str(Path(__file__).parent.parent))

VERIFIED_BUILD = 66666

# We'll generate raw SQL directly to control hotfix IDs precisely
lines = []
hf_id = 1900100  # hotfix_data IDs start here, well above spell IDs

TABLE_HASHES = {
    "spell_name":           0x46C66698,
    "spell":                0xE111669E,
    "spell_misc":           0xC603EE28,
    "spell_effect":         0xF04238A5,
}

def hf(record_id, table):
    global hf_id
    h = TABLE_HASHES[table]
    uid = hf_id * 10 + (hf_id % 97)
    lines.append(
        f"REPLACE INTO `hotfixes`.`hotfix_data` "
        f"(`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`) "
        f"VALUES ({hf_id}, {uid}, {h}, {record_id}, 1, {VERIFIED_BUILD});"
    )
    hf_id += 1

def esc(s):
    return s.replace("\\", "\\\\").replace("'", "\\'")

eff_id = 1900100  # spell_effect row IDs start here

def spell_name(sid, name):
    lines.append(f"REPLACE INTO `hotfixes`.`spell_name` (`ID`, `Name`, `VerifiedBuild`) VALUES ({sid}, '{esc(name)}', {VERIFIED_BUILD});")
    hf(sid, "spell_name")

def spell_text(sid, desc, aura_desc):
    d = f"'{esc(desc)}'" if desc else "NULL"
    a = f"'{esc(aura_desc)}'" if aura_desc else "NULL"
    lines.append(f"REPLACE INTO `hotfixes`.`spell` (`ID`, `NameSubtext`, `Description`, `AuraDescription`, `VerifiedBuild`) VALUES ({sid}, NULL, {d}, {a}, {VERIFIED_BUILD});")
    hf(sid, "spell")

def spell_misc(sid, attrs, cast_time=1, duration=21, range_idx=1, school=1, icon=136243):
    a = ", ".join(str(x) for x in (attrs + [0]*17)[:17])
    lines.append(
        f"REPLACE INTO `hotfixes`.`spell_misc` (`ID`, "
        f"`Attributes1`, `Attributes2`, `Attributes3`, `Attributes4`, `Attributes5`, "
        f"`Attributes6`, `Attributes7`, `Attributes8`, `Attributes9`, `Attributes10`, "
        f"`Attributes11`, `Attributes12`, `Attributes13`, `Attributes14`, `Attributes15`, "
        f"`Attributes16`, `Attributes17`, "
        f"`DifficultyID`, `CastingTimeIndex`, `DurationIndex`, `PvPDurationIndex`, "
        f"`RangeIndex`, `SchoolMask`, `Speed`, `LaunchDelay`, `MinDuration`, "
        f"`SpellIconFileDataID`, `ActiveIconFileDataID`, `ContentTuningID`, "
        f"`ShowFutureSpellPlayerConditionID`, `SpellVisualScript`, `ActiveSpellVisualScript`, "
        f"`SpellID`, `VerifiedBuild`) "
        f"VALUES ({sid}, {a}, 0, {cast_time}, {duration}, 0, {range_idx}, {school}, "
        f"0, 0, 0, {icon}, 0, 0, 0, 0, 0, {sid}, {VERIFIED_BUILD});"
    )
    hf(sid, "spell_misc")

def spell_effect(sid, idx, effect, aura=0, bp=0, mv0=0, mv1=0, ta=1, tb=0,
                 trigger=0, radius0=0, radius1=0, period=0):
    global eff_id
    eid = eff_id
    eff_id += 1
    bp_val = int(bp) if isinstance(bp, (int, float)) and bp == int(bp) else bp
    lines.append(
        f"REPLACE INTO `hotfixes`.`spell_effect` (`ID`, `EffectAura`, `DifficultyID`, "
        f"`EffectIndex`, `Effect`, `EffectAmplitude`, `EffectAttributes`, `EffectAuraPeriod`, "
        f"`EffectBonusCoefficient`, `EffectChainAmplitude`, `EffectChainTargets`, "
        f"`EffectItemType`, `EffectMechanic`, `EffectPointsPerResource`, "
        f"`EffectPosFacing`, `EffectRealPointsPerLevel`, `EffectTriggerSpell`, "
        f"`BonusCoefficientFromAP`, `PvpMultiplier`, `Coefficient`, `Variance`, "
        f"`ResourceCoefficient`, `GroupSizeBasePointsCoefficient`, "
        f"`EffectBasePoints`, `ScalingClass`, `TargetNodeGraph`, "
        f"`EffectMiscValue1`, `EffectMiscValue2`, "
        f"`EffectRadiusIndex1`, `EffectRadiusIndex2`, "
        f"`EffectSpellClassMask1`, `EffectSpellClassMask2`, "
        f"`EffectSpellClassMask3`, `EffectSpellClassMask4`, "
        f"`ImplicitTarget1`, `ImplicitTarget2`, "
        f"`SpellID`, `VerifiedBuild`) "
        f"VALUES ({eid}, {aura}, 0, {idx}, {effect}, 0, 0, {period}, "
        f"0, 1, 0, 0, 0, 0, 0, 0, {trigger}, "
        f"0, 1, 0, 0, 0, 1, "
        f"{bp_val}, 0, 0, {mv0}, {mv1}, {radius0}, {radius1}, "
        f"0, 0, 0, 0, {ta}, {tb}, {sid}, {VERIFIED_BUILD});"
    )
    hf(eid, "spell_effect")

PASSIVE = [0x10000400] + [0]*16  # NOT_SHAPESHIFT | PASSIVE

# ================================================================
# VISUAL AURAS (9 spells)
# ================================================================
visual_auras = [
    # (id, name, SpellVisualKitID, school, aura_desc, icon)
    (1900005, "Arcane Radiance",    44809, 64, "Wreathed in shimmering arcane energy.",        4352486),
    (1900006, "Shadow Embrace",     65054, 32, "Cloaked in dark, writhing shadows.",            136223),
    (1900007, "Divine Luminance",   43389,  2, "Bathed in radiant golden light.",              135981),
    (1900008, "Ember Wreath",       80053,  4, "Wreathed in smoldering embers and flame.",     135818),
    (1900009, "Frost Mantle",       43136, 16, "Encased in shimmering frost crystals.",        135849),
    (1900010, "Nature's Blessing",  27374,  8, "Surrounded by verdant natural energy.",        136085),
    (1900011, "Fel Corruption",     38373, 32, "Radiating sinister fel energy.",               136201),
    (1900012, "Storm's Fury",       44815,  8, "Crackling with volatile lightning.",           136048),
    (1900013, "Void Whispers",      78248, 32, "Engulfed in the whispers of the Void.",        132886),
]

for sid, name, kit, school, adesc, icon in visual_auras:
    lines.append(f"\n-- {'='*60}")
    lines.append(f"-- {name} (ID: {sid}) — Visual Aura")
    lines.append(f"-- {'='*60}")
    spell_name(sid, name)
    spell_text(sid, "Surrounds the caster with a persistent visual aura.", adesc)
    spell_misc(sid, PASSIVE[:], duration=21, school=school, icon=icon)
    spell_effect(sid, 0, effect=6, aura=237, mv0=kit, ta=1)

# ================================================================
# SCALE SPELLS (2 spells)
# ================================================================
lines.append(f"\n-- {'='*60}")
lines.append(f"-- Giant's Might (ID: 1900014) — +200% Scale")
lines.append(f"-- {'='*60}")
spell_name(1900014, "Giant's Might")
spell_text(1900014, "Increases the caster's size dramatically.", "Size increased by 200%.")
spell_misc(1900014, PASSIVE[:], duration=21, school=1, icon=132515)
spell_effect(1900014, 0, effect=6, aura=61, bp=200, ta=1)

lines.append(f"\n-- {'='*60}")
lines.append(f"-- Pixie Dust (ID: 1900015) — -50% Scale")
lines.append(f"-- {'='*60}")
spell_name(1900015, "Pixie Dust")
spell_text(1900015, "Shrinks the caster to a tiny size.", "Size decreased by 50%.")
spell_misc(1900015, PASSIVE[:], duration=21, school=64, icon=134853)
spell_effect(1900015, 0, effect=6, aura=61, bp=-50, ta=1)

# ================================================================
# VOICE OF THE THUNDER KING (1 spell)
# ================================================================
lines.append(f"\n-- {'='*60}")
lines.append(f"-- Voice of the Thunder King (ID: 1900016)")
lines.append(f"-- {'='*60}")
spell_name(1900016, "Voice of the Thunder King")
spell_text(1900016, "Unleashes a thunderous shockwave, hurling all nearby enemies into the air.", None)
spell_misc(1900016, [0]*17, cast_time=1, duration=0, range_idx=1, school=8, icon=136014)
# Effect 0: Knockback all enemies in 40yd (RadiusIndex 23 = 40yd)
spell_effect(1900016, 0, effect=98, bp=250, mv0=300, ta=15, radius0=23)

# ================================================================
# PHASE WALK (1 spell — stealth, 5 min)
# ================================================================
lines.append(f"\n-- {'='*60}")
lines.append(f"-- Phase Walk (ID: 1900017) — Stealth 5min")
lines.append(f"-- {'='*60}")
spell_name(1900017, "Phase Walk")
spell_text(1900017, "Slip between the planes, becoming invisible.", "Phased out of reality. Stealthed.")
spell_misc(1900017, PASSIVE[:], duration=5, school=64, icon=132331)  # 5 min
spell_effect(1900017, 0, effect=6, aura=16, bp=500, ta=1)

# ================================================================
# COSTUME KIT (10 spells — TRANSFORM aura 56, 20 min)
# ================================================================
costumes = [
    (1900018, "Costume: The Lich King",          24266,  "Transformed into the Lich King.",           236922),
    (1900019, "Costume: Illidan Stormrage",       112194, "Transformed into the Betrayer.",            236415),
    (1900020, "Costume: Sylvanas Windrunner",     179910, "Transformed into the Banshee Queen.",       237007),
    (1900021, "Costume: Thrall",                  82851,  "Transformed into the World-Shaman.",        236194),
    (1900022, "Costume: Jaina Proudmoore",        120590, "Transformed into the Lord Admiral.",        237182),
    (1900023, "Costume: Tyrande Whisperwind",     147700, "Transformed into the Night Warrior.",       237185),
    (1900024, "Costume: Ragnaros",                11502,  "Transformed into the Firelord.",            135818),
    (1900025, "Costume: Alexstrasza",             188878, "Transformed into the Life-Binder.",         136080),
    (1900026, "Costume: Archmage Khadgar",        72874,  "Transformed into the Archmage.",            136063),
    (1900027, "Costume: Bolvar Fordragon",        164079, "Transformed into the Jailer's Bane.",       236922),
]

for sid, name, entry, adesc, icon in costumes:
    lines.append(f"\n-- {'='*60}")
    lines.append(f"-- {name} (ID: {sid}) — 20min Costume")
    lines.append(f"-- {'='*60}")
    spell_name(sid, name)
    spell_text(sid, f"{adesc.rstrip('.')} for 20 minutes.", adesc)
    spell_misc(sid, [0]*17, cast_time=1, duration=40, school=64, icon=icon)  # DurationIndex 40 = 20min
    spell_effect(sid, 0, effect=6, aura=56, mv0=entry, ta=1)

# ================================================================
# OUTPUT
# ================================================================
header = [
    "-- ============================================================",
    "-- VoxCore RP Fun Spells — Batch Generation",
    "-- 23 spells: 9 visual auras, 2 scale, 1 thunderking,",
    "--            1 stealth, 10 costumes",
    "-- IDs: 1900005 - 1900027",
    "-- ============================================================",
    "",
]

print("\n".join(header + lines))
