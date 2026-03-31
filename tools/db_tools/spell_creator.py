#!/usr/bin/env python3
"""
VoxCore Spell Creator — Interactive spell creation & hotfix SQL generator.

Replaces the old .NET SpellCreator with a Python CLI that integrates directly
with VoxCore's wago CSV pipeline, table hashes, and MySQL.

Usage:
    python tools/spell_creator.py                  # Interactive menu
    python tools/spell_creator.py lookup 1459      # Quick lookup
    python tools/spell_creator.py clone 1459 1310001  # Clone spell
    python tools/spell_creator.py template mount   # From template
"""
from __future__ import annotations

import argparse
import csv
import os
import re
import subprocess
import sys
import textwrap
from collections import OrderedDict
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Optional

# ---------------------------------------------------------------------------
# Bootstrap — add wago/ to path so we can import project modules
# ---------------------------------------------------------------------------
TOOLS_DIR = Path(__file__).resolve().parent.parent
PROJECT_ROOT = TOOLS_DIR.parent
sys.path.insert(0, str(PROJECT_ROOT / "wago"))

try:
    from wago_common import VERIFIED_BUILD, WAGO_CSV_DIR, MYSQL_BIN, MYSQL_USER, MYSQL_PASS
except ImportError:
    VERIFIED_BUILD = 66666
    WAGO_CSV_DIR = Path(".")
    MYSQL_BIN = "mysql"
    MYSQL_USER = "root"
    MYSQL_PASS = "admin"

try:
    from table_hashes import TABLE_HASHES
except ImportError:
    TABLE_HASHES = {}

# ---------------------------------------------------------------------------
# ANSI colors
# ---------------------------------------------------------------------------
class C:
    RESET   = "\033[0m"
    BOLD    = "\033[1m"
    DIM     = "\033[2m"
    RED     = "\033[91m"
    GREEN   = "\033[92m"
    YELLOW  = "\033[93m"
    BLUE    = "\033[94m"
    CYAN    = "\033[96m"
    WHITE   = "\033[97m"
    MAGENTA = "\033[95m"

def hdr(text: str) -> str:
    return f"\n{C.CYAN}{C.BOLD}{'=' * 60}{C.RESET}\n{C.CYAN}{C.BOLD}  {text}{C.RESET}\n{C.CYAN}{C.BOLD}{'=' * 60}{C.RESET}"

def info(text: str) -> str:
    return f"{C.GREEN}>{C.RESET} {text}"

def warn(text: str) -> str:
    return f"{C.YELLOW}!{C.RESET} {text}"

def err(text: str) -> str:
    return f"{C.RED}x{C.RESET} {text}"

def dim(text: str) -> str:
    return f"{C.DIM}{text}{C.RESET}"

# ---------------------------------------------------------------------------
# Table Hashes — spell-related (from table_hashes.py)
# ---------------------------------------------------------------------------
SPELL_TABLE_HASHES = {
    "spell":                  TABLE_HASHES.get("spell",                  0xE111669E),
    "spell_name":             TABLE_HASHES.get("spell_name",             0x46C66698),
    "spell_misc":             TABLE_HASHES.get("spell_misc",             0xC603EE28),
    "spell_effect":           TABLE_HASHES.get("spell_effect",           0xF04238A5),
    "spell_aura_options":     TABLE_HASHES.get("spell_aura_options",     0xF42FC065),
    "spell_aura_restrictions":TABLE_HASHES.get("spell_aura_restrictions",0xBA978F4E),
    "spell_x_spell_visual":   TABLE_HASHES.get("spell_x_spell_visual",  0x27B7A01A),
    "spell_categories":       TABLE_HASHES.get("spell_categories",       0xDBE7F829),
    "spell_cooldowns":        TABLE_HASHES.get("spell_cooldowns",        0xF9F37C57),
    "spell_interrupts":       TABLE_HASHES.get("spell_interrupts",       0x668FAE03),
    "spell_item_enchantment": TABLE_HASHES.get("spell_item_enchantment", 0xE05AC589),
    "spell_label":            TABLE_HASHES.get("spell_label",            0x30769020),
    "spell_learn_spell":      TABLE_HASHES.get("spell_learn_spell",      0xDBEDF603),
    "spell_levels":           TABLE_HASHES.get("spell_levels",           0x1DDEC5E6),
    "spell_power":            TABLE_HASHES.get("spell_power",            0xA1ACE1DF),
    "spell_shapeshift":       TABLE_HASHES.get("spell_shapeshift",       0xBC91EA17),
}

# Icon search directory
ICON_DIR = PROJECT_ROOT / "wago" / "att_icons_export" / "8K_Format" / "wow_icons" / "large"

# ---------------------------------------------------------------------------
# Enum References — commonly used values for spell creation
# ---------------------------------------------------------------------------
SPELL_EFFECTS = {
    0: "NONE", 1: "INSTAKILL", 2: "SCHOOL_DAMAGE", 3: "DUMMY",
    6: "APPLY_AURA", 7: "ENVIRONMENTAL_DAMAGE", 8: "POWER_DRAIN",
    9: "HEALTH_LEECH", 10: "HEAL", 11: "BIND", 12: "PORTAL",
    16: "QUEST_COMPLETE", 17: "WEAPON_DAMAGE_NOSCHOOL", 18: "RESURRECT",
    24: "CREATE_ITEM", 27: "PERSISTENT_AREA_AURA", 28: "SUMMON",
    29: "LEAP", 30: "ENERGIZE", 33: "OPEN_LOCK",
    35: "APPLY_AREA_AURA_PARTY", 36: "LEARN_SPELL", 38: "DISPEL",
    44: "SKILL_STEP", 46: "SPAWN", 53: "ENCHANT_ITEM",
    56: "SUMMON_PET", 58: "WEAPON_DAMAGE", 62: "POWER_BURN",
    64: "TRIGGER_SPELL", 65: "APPLY_AREA_AURA_RAID", 67: "HEAL_MAX_HEALTH",
    68: "INTERRUPT_CAST", 77: "SCRIPT_EFFECT", 78: "ATTACK",
    85: "SUMMON_PLAYER", 86: "ACTIVATE_OBJECT", 90: "KILL_CREDIT",
    96: "CHARGE", 98: "KNOCK_BACK", 103: "REPUTATION",
    119: "APPLY_AREA_AURA_PET", 121: "NORMALIZED_WEAPON_DMG",
    123: "SEND_TAXI", 128: "APPLY_AREA_AURA_FRIEND",
    129: "APPLY_AREA_AURA_ENEMY", 131: "PLAY_SOUND", 132: "PLAY_MUSIC",
    136: "HEAL_PCT", 140: "FORCE_CAST", 142: "TRIGGER_SPELL_WITH_VALUE",
    143: "APPLY_AREA_AURA_OWNER", 150: "QUEST_START",
    151: "TRIGGER_SPELL_2", 154: "DISCOVER_TAXI",
    179: "CREATE_AREATRIGGER", 196: "TELEPORT_TO_DIGSITE",
    203: "LEARN_TRANSMOG_SET", 206: "LEARN_TRANSMOG_APPEARANCE",
    238: "CHANGE_MODEL",
    252: "DAMAGE_BONUS",
}

AURA_TYPES = {
    0: "NONE", 3: "PERIODIC_DAMAGE", 4: "DUMMY", 5: "MOD_CONFUSE",
    6: "MOD_CHARM", 7: "MOD_FEAR", 8: "PERIODIC_HEAL",
    9: "MOD_ATTACKSPEED", 10: "MOD_THREAT", 12: "MOD_STUN",
    13: "MOD_DAMAGE_DONE", 14: "MOD_DAMAGE_TAKEN", 16: "MOD_STEALTH",
    18: "MOD_INVISIBILITY", 22: "MOD_RESISTANCE",
    23: "PERIODIC_TRIGGER_SPELL", 24: "PERIODIC_ENERGIZE",
    26: "MOD_ROOT", 27: "MOD_SILENCE", 29: "MOD_STAT",
    31: "MOD_INCREASE_SPEED", 32: "MOD_INCREASE_MOUNTED_SPEED",
    33: "MOD_DECREASE_SPEED", 34: "MOD_INCREASE_HEALTH",
    36: "MOD_SHAPESHIFT", 42: "PROC_TRIGGER_SPELL",
    48: "PERIODIC_TRIGGER_SPELL_FROM_CLIENT",
    49: "MOD_DODGE_PERCENT", 55: "MOD_POWER_REGEN",
    56: "CHANNEL_DEATH_ITEM", 61: "MOD_INCREASE_SWIM_SPEED",
    69: "SCHOOL_ABSORB", 77: "MECHANIC_IMMUNITY",
    78: "MOUNTED", 79: "MOD_DAMAGE_PERCENT_DONE",
    80: "MOD_PERCENT_STAT", 87: "MOD_DAMAGE_PERCENT_TAKEN",
    97: "MANA_SHIELD", 101: "MOD_INCREASE_MOUNTED_SPEED_ALWAYS",
    109: "ADD_TARGET_TRIGGER", 110: "MOD_POWER_REGEN_PERCENT",
    126: "MOD_RANGED_DAMAGE_TAKEN", 130: "MOD_INCREASE_MOUNTED_FLIGHT_SPEED",
    143: "ABILITY_IGNORE_AURASTATE", 155: "FLY",
    162: "MOD_INCREASE_VEHICLE_FLIGHT_SPEED",
    188: "OVERRIDE_ACTIONBAR_SPELLS", 210: "MOD_CAST_TIME_PCT",
    228: "MASTERY", 232: "MOUNT_RESTRICTIONS",
    237: "OVERRIDE_SPELL_VISUAL", 261: "OVERRIDE_ACTIONBAR_SPELLS_TRIGGERED",
    312: "SET_VEHICLE_ID", 320: "SCHOOL_HEAL_ABSORB",
    342: "LINKED_SUMMON", 354: "MOD_HEALING_DONE_PCT_VERSUS_TARGET_HEALTH",
    411: "MOD_FLYING_SPEED",
}

IMPLICIT_TARGETS = {
    0: "NONE", 1: "UNIT_CASTER", 2: "UNIT_NEARBY_ENEMY",
    3: "UNIT_NEARBY_ALLY", 5: "UNIT_PET", 6: "UNIT_TARGET_ENEMY",
    7: "UNIT_SRC_AREA_ENTRY", 8: "UNIT_DEST_AREA_ENTRY",
    9: "DEST_HOME", 15: "UNIT_SRC_AREA_ENEMY",
    16: "UNIT_DEST_AREA_ENEMY", 18: "DEST_CASTER",
    20: "UNIT_CASTER_AREA_PARTY", 21: "UNIT_TARGET_ALLY",
    22: "SRC_CASTER", 23: "GAMEOBJECT_TARGET",
    25: "UNIT_TARGET_ANY", 27: "UNIT_MASTER",
    30: "UNIT_SRC_AREA_ALLY", 31: "UNIT_DEST_AREA_ALLY",
    33: "UNIT_SRC_AREA_PARTY", 37: "UNIT_LASTTARGET_AREA_PARTY",
    38: "UNIT_NEARBY_ENTRY", 46: "DEST_NEARBY_ENTRY",
    47: "DEST_CASTER_FRONT", 53: "DEST_TARGET_ENEMY",
    87: "DEST_TARGET_ANY",
}

SCHOOL_MASKS = {
    0: "None", 1: "Physical", 2: "Holy", 4: "Fire", 8: "Nature",
    16: "Frost", 32: "Shadow", 64: "Arcane",
    126: "All Magic (no Physical)", 127: "All",
}

CAST_TIME_INDEX = {
    1: "Instant", 4: "1000ms", 5: "2000ms", 6: "3000ms",
    7: "5000ms", 8: "10000ms", 14: "500ms", 16: "1500ms",
}

DURATION_INDEX = {
    0: "Instant", 1: "10s", 3: "30s", 4: "60s (1 min)", 5: "5 min",
    6: "10 min", 7: "2 min", 9: "30 min", 11: "45s",
    18: "20s", 21: "Infinite (-1)", 23: "90s", 27: "3s",
    28: "15s", 32: "2s", 35: "4s", 36: "1s", 39: "6s",
    85: "8s", 121: "5s", 328: "12s", 594: "120s (2 min)",
}

RANGE_INDEX = {
    1: "Self Only", 2: "5 yd (Combat)", 3: "20 yd", 4: "30 yd",
    5: "40 yd", 6: "100 yd", 7: "10 yd", 8: "15 yd",
    9: "25 yd", 10: "50 yd", 11: "60 yd", 13: "Unlimited",
    34: "8 yd", 35: "45 yd", 187: "80 yd",
}

# ---------------------------------------------------------------------------
# Data Classes
# ---------------------------------------------------------------------------
@dataclass
class SpellEffect:
    effect_id: int = 0           # Row ID in spell_effect
    effect: int = 0              # SpellEffect type
    aura: int = 0                # Aura type (if effect=6)
    base_points: float = 0.0
    misc_value_0: int = 0
    misc_value_1: int = 0
    target_a: int = 1            # ImplicitTarget[0]
    target_b: int = 0            # ImplicitTarget[1]
    trigger_spell: int = 0
    amplitude: float = 0.0
    chain_targets: int = 0
    radius_0: int = 0
    radius_1: int = 0
    aura_period: int = 0
    chain_amplitude: float = 1.0
    points_per_resource: float = 0.0
    bonus_coefficient: float = 0.0
    bonus_coefficient_ap: float = 0.0
    pvp_multiplier: float = 1.0
    coefficient: float = 0.0
    variance: float = 0.0
    resource_coefficient: float = 0.0
    group_size_coefficient: float = 1.0
    mechanic: int = 0
    item_type: int = 0
    attributes: int = 0
    scaling_class: int = 0

@dataclass
class SpellData:
    spell_id: int = 0
    name: str = ""
    subtext: str = ""
    description: str = ""
    aura_description: str = ""

    # SpellMisc
    attributes: list[int] = field(default_factory=lambda: [0]*17)
    casting_time_index: int = 1
    duration_index: int = 0
    range_index: int = 1
    school_mask: int = 1
    speed: float = 0.0
    launch_delay: float = 0.0
    min_duration: float = 0.0
    icon_file_data_id: int = 0
    active_icon_file_data_id: int = 0
    content_tuning_id: int = 0
    spell_visual_script: int = 0
    pvp_duration_index: int = 0

    # Effects
    effects: list[SpellEffect] = field(default_factory=list)

    # SpellXSpellVisual (optional)
    visual_id: int = 0

    # Serverside spell mode
    serverside: bool = False

    def add_effect(self, **kwargs) -> SpellEffect:
        eff = SpellEffect(**kwargs)
        self.effects.append(eff)
        return eff

# ---------------------------------------------------------------------------
# CSV Lookup (reads from wago CSV directory)
# ---------------------------------------------------------------------------
_csv_cache: dict[str, list[dict]] = {}

def _load_csv(name: str) -> list[dict]:
    """Load a wago CSV by table name (e.g., 'SpellName')."""
    if name in _csv_cache:
        return _csv_cache[name]

    # Try wago CSV dir first, then fallback to SpellCreator dir
    candidates = [
        WAGO_CSV_DIR / f"{name}.csv",
        WAGO_CSV_DIR / f"{name}-enUS.csv",
        PROJECT_ROOT / "addons" / "SpellCreator" / f"{name}.csv",
    ]
    for path in candidates:
        if path.exists():
            with open(path, "r", encoding="utf-8-sig") as f:
                rows = list(csv.DictReader(f))
            _csv_cache[name] = rows
            return rows

    return []

def _csv_by_id(name: str) -> dict[int, dict]:
    """Build {ID: row} dict from a CSV."""
    rows = _load_csv(name)
    result = {}
    for r in rows:
        try:
            rid = int(r.get("ID", 0))
            result[rid] = r
        except (ValueError, TypeError):
            pass
    return result

def _csv_by_spell_id(name: str) -> dict[int, list[dict]]:
    """Build {SpellID: [rows]} dict from a CSV with SpellID column."""
    rows = _load_csv(name)
    result: dict[int, list[dict]] = {}
    for r in rows:
        try:
            sid = int(r.get("SpellID", 0))
            result.setdefault(sid, []).append(r)
        except (ValueError, TypeError):
            pass
    return result

def lookup_spell(spell_id: int) -> Optional[dict]:
    """Look up a spell across all spell CSV tables. Returns combined dict."""
    names = _csv_by_id("SpellName")
    if spell_id not in names:
        return None

    result = {"spell_id": spell_id, "name": names[spell_id].get("Name_lang", "")}

    # Spell (subtext/desc)
    spells = _csv_by_id("Spell")
    if spell_id in spells:
        row = spells[spell_id]
        result["subtext"] = row.get("NameSubtext_lang", "")
        result["description"] = row.get("Description_lang", "")
        result["aura_description"] = row.get("AuraDescription_lang", "")

    # SpellMisc (by SpellID column)
    misc_by_sid = _csv_by_spell_id("SpellMisc")
    if spell_id in misc_by_sid:
        result["misc"] = misc_by_sid[spell_id]

    # SpellEffect (by SpellID column)
    eff_by_sid = _csv_by_spell_id("SpellEffect")
    if spell_id in eff_by_sid:
        result["effects"] = eff_by_sid[spell_id]

    # SpellAuraOptions
    aura_by_sid = _csv_by_spell_id("SpellAuraOptions")
    if spell_id in aura_by_sid:
        result["aura_options"] = aura_by_sid[spell_id]

    # SpellXSpellVisual
    vis_by_sid = _csv_by_spell_id("SpellXSpellVisual")
    if spell_id in vis_by_sid:
        result["visuals"] = vis_by_sid[spell_id]

    return result

def search_spells(query: str, limit: int = 20) -> list[tuple[int, str]]:
    """Search spell names by substring (case-insensitive)."""
    results = []
    q = query.lower()
    names = _load_csv("SpellName")
    for row in names:
        name = row.get("Name_lang", "")
        if q in name.lower():
            try:
                results.append((int(row["ID"]), name))
            except (ValueError, KeyError):
                pass
            if len(results) >= limit:
                break
    return results

# ---------------------------------------------------------------------------
# MySQL helper
# ---------------------------------------------------------------------------
def run_mysql(query: str, db: str = "hotfixes") -> Optional[str]:
    result = subprocess.run(
        [MYSQL_BIN, "-u", MYSQL_USER, f"-p{MYSQL_PASS}", db, "-N", "-e", query],
        capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=60
    )
    if result.returncode != 0:
        stderr = [l for l in result.stderr.strip().split("\n") if "Using a password" not in l]
        if stderr:
            print(err(f"MySQL: {' '.join(stderr[:2])}"))
        return None
    return result.stdout.strip()

def find_next_free_id(start: int = 1310000) -> int:
    """Find next unused spell ID starting from `start`.
    Checks both hotfixes.spell_name and world.serverside_spell."""
    max_id = start - 1
    # Check hotfixes
    out = run_mysql(f"SELECT MAX(ID) FROM spell_name WHERE ID >= {start}", "hotfixes")
    if out and out != "NULL":
        try:
            max_id = max(max_id, int(out))
        except ValueError:
            pass
    # Check serverside_spell
    out2 = run_mysql(f"SELECT MAX(Id) FROM serverside_spell WHERE Id >= {start}", "world")
    if out2 and out2 != "NULL":
        try:
            max_id = max(max_id, int(out2))
        except ValueError:
            pass
    return max_id + 1

# ---------------------------------------------------------------------------
# SQL Generation
# ---------------------------------------------------------------------------
def _sql_val(v: Any) -> str:
    """Format a value for SQL."""
    if v is None:
        return "NULL"
    if isinstance(v, str):
        escaped = (v.replace("\\", "\\\\")
                    .replace("'", "\\'")
                    .replace("\n", "\\n")
                    .replace("\r", ""))
        return f"'{escaped}'"
    if isinstance(v, float):
        if v == int(v):
            return str(int(v))
        return f"{v}"
    return str(v)

_hotfix_counter = 0

def _hotfix_data_row(record_id: int, table_name: str, base_id: int = 0) -> str:
    """Generate a hotfix_data INSERT for a given record.
    Uses an auto-incrementing offset from base_id for unique Id/UniqueId pairs."""
    global _hotfix_counter
    table_hash = SPELL_TABLE_HASHES.get(table_name, 0)
    hid = (base_id or record_id) + _hotfix_counter
    uid = hid * 100 + _hotfix_counter
    _hotfix_counter += 1
    return (
        f"REPLACE INTO `hotfixes`.`hotfix_data` "
        f"(`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`) "
        f"VALUES ({hid}, {uid}, {table_hash}, {record_id}, 1, {VERIFIED_BUILD});"
    )

def generate_sql(spell: SpellData) -> str:
    """Generate complete SQL for a spell including all hotfix_data rows."""
    lines = []
    sid = spell.spell_id
    global _hotfix_counter
    _hotfix_counter = 0

    lines.append(f"-- ============================================================")
    lines.append(f"-- Spell: {spell.name} (ID: {sid})")
    lines.append(f"-- Generated by VoxCore Spell Creator")
    lines.append(f"-- ============================================================")
    lines.append("")

    if spell.serverside:
        return _generate_serverside_sql(spell, lines)

    # --- spell_name ---
    lines.append(f"-- spell_name")
    lines.append(
        f"REPLACE INTO `hotfixes`.`spell_name` (`ID`, `Name`, `VerifiedBuild`) "
        f"VALUES ({sid}, {_sql_val(spell.name)}, {VERIFIED_BUILD});"
    )
    lines.append(_hotfix_data_row(sid, "spell_name", sid))
    lines.append("")

    # --- spell (subtext/description) ---
    lines.append(f"-- spell")
    lines.append(
        f"REPLACE INTO `hotfixes`.`spell` "
        f"(`ID`, `NameSubtext`, `Description`, `AuraDescription`, `VerifiedBuild`) "
        f"VALUES ({sid}, {_sql_val(spell.subtext or None)}, "
        f"{_sql_val(spell.description or None)}, "
        f"{_sql_val(spell.aura_description or None)}, {VERIFIED_BUILD});"
    )
    lines.append(_hotfix_data_row(sid, "spell", sid))
    lines.append("")

    # --- spell_misc ---
    # DB has 17 attribute columns (Attributes1..17); pad if needed
    attrs_padded = (spell.attributes + [0] * 17)[:17]
    attrs = ", ".join(str(a) for a in attrs_padded)
    misc_id = sid  # use spell_id as misc row ID
    lines.append(f"-- spell_misc")
    lines.append(
        f"REPLACE INTO `hotfixes`.`spell_misc` "
        f"(`ID`, `Attributes1`, `Attributes2`, `Attributes3`, `Attributes4`, "
        f"`Attributes5`, `Attributes6`, `Attributes7`, `Attributes8`, "
        f"`Attributes9`, `Attributes10`, `Attributes11`, `Attributes12`, "
        f"`Attributes13`, `Attributes14`, `Attributes15`, `Attributes16`, "
        f"`Attributes17`, "
        f"`DifficultyID`, `CastingTimeIndex`, `DurationIndex`, `PvPDurationIndex`, "
        f"`RangeIndex`, `SchoolMask`, `Speed`, `LaunchDelay`, `MinDuration`, "
        f"`SpellIconFileDataID`, `ActiveIconFileDataID`, `ContentTuningID`, "
        f"`ShowFutureSpellPlayerConditionID`, `SpellVisualScript`, "
        f"`ActiveSpellVisualScript`, `SpellID`, `VerifiedBuild`) "
        f"VALUES ({misc_id}, {attrs}, "
        f"0, {spell.casting_time_index}, {spell.duration_index}, "
        f"{spell.pvp_duration_index}, {spell.range_index}, {spell.school_mask}, "
        f"{spell.speed}, {spell.launch_delay}, {spell.min_duration}, "
        f"{spell.icon_file_data_id}, {spell.active_icon_file_data_id}, "
        f"{spell.content_tuning_id}, 0, {spell.spell_visual_script}, 0, "
        f"{sid}, {VERIFIED_BUILD});"
    )
    lines.append(_hotfix_data_row(misc_id, "spell_misc", sid))
    lines.append("")

    # --- spell_effect(s) ---
    for i, eff in enumerate(spell.effects):
        eff_row_id = eff.effect_id if eff.effect_id else sid + i
        lines.append(f"-- spell_effect (index {i})")
        lines.append(
            f"REPLACE INTO `hotfixes`.`spell_effect` "
            f"(`ID`, `EffectAura`, `DifficultyID`, `EffectIndex`, `Effect`, "
            f"`EffectAmplitude`, `EffectAttributes`, `EffectAuraPeriod`, "
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
            f"VALUES ({eff_row_id}, {eff.aura}, 0, {i}, {eff.effect}, "
            f"{eff.amplitude}, {eff.attributes}, {eff.aura_period}, "
            f"{eff.bonus_coefficient}, {eff.chain_amplitude}, {eff.chain_targets}, "
            f"{eff.item_type}, {eff.mechanic}, {eff.points_per_resource}, "
            f"0, 0, {eff.trigger_spell}, "
            f"{eff.bonus_coefficient_ap}, {eff.pvp_multiplier}, "
            f"{eff.coefficient}, {eff.variance}, "
            f"{eff.resource_coefficient}, {eff.group_size_coefficient}, "
            f"{eff.base_points}, {eff.scaling_class}, 0, "
            f"{eff.misc_value_0}, {eff.misc_value_1}, "
            f"{eff.radius_0}, {eff.radius_1}, "
            f"0, 0, 0, 0, "
            f"{eff.target_a}, {eff.target_b}, "
            f"{sid}, {VERIFIED_BUILD});"
        )
        lines.append(_hotfix_data_row(eff_row_id, "spell_effect", sid))
        lines.append("")

    # --- spell_x_spell_visual (optional) ---
    if spell.visual_id:
        vis_row_id = sid
        lines.append(f"-- spell_x_spell_visual")
        lines.append(
            f"REPLACE INTO `hotfixes`.`spell_x_spell_visual` "
            f"(`ID`, `DifficultyID`, `SpellVisualID`, `Probability`, `Flags`, "
            f"`Priority`, `SpellIconFileID`, `ActiveIconFileID`, "
            f"`ViewerUnitConditionID`, `ViewerPlayerConditionID`, "
            f"`CasterUnitConditionID`, `CasterPlayerConditionID`, "
            f"`SpellID`, `VerifiedBuild`) "
            f"VALUES ({vis_row_id}, 0, {spell.visual_id}, 1, 0, 0, 0, 0, "
            f"0, 0, 0, 0, {sid}, {VERIFIED_BUILD});"
        )
        lines.append(_hotfix_data_row(vis_row_id, "spell_x_spell_visual", sid))
        lines.append("")

    return "\n".join(lines)

def _generate_serverside_sql(spell: SpellData, lines: list[str]) -> str:
    """Generate serverside_spell + serverside_spell_effect SQL (world DB)."""
    sid = spell.spell_id
    attrs = (spell.attributes + [0] * 17)[:17]  # ensure exactly 17

    lines.append(f"-- serverside_spell (world DB)")
    lines.append(
        f"REPLACE INTO `world`.`serverside_spell` "
        f"(`Id`, `DifficultyID`, `CategoryId`, `Dispel`, `Mechanic`, "
        f"`Attributes`, `AttributesEx`, `AttributesEx2`, `AttributesEx3`, "
        f"`AttributesEx4`, `AttributesEx5`, `AttributesEx6`, `AttributesEx7`, "
        f"`AttributesEx8`, `AttributesEx9`, `AttributesEx10`, `AttributesEx11`, "
        f"`AttributesEx12`, `AttributesEx13`, `AttributesEx14`, `AttributesEx15`, "
        f"`AttributesEx16`, "
        f"`CastingTimeIndex`, `DurationIndex`, `RangeIndex`, `Speed`, "
        f"`SchoolMask`, `SpellName`) "
        f"VALUES ({sid}, 0, 0, 0, 0, "
        f"{attrs[0]}, {attrs[1]}, {attrs[2]}, {attrs[3]}, "
        f"{attrs[4]}, {attrs[5]}, {attrs[6]}, {attrs[7]}, "
        f"{attrs[8]}, {attrs[9]}, {attrs[10]}, {attrs[11]}, "
        f"{attrs[12]}, {attrs[13]}, {attrs[14]}, {attrs[15]}, "
        f"{attrs[16] if len(attrs) > 16 else 0}, "
        f"{spell.casting_time_index}, {spell.duration_index}, "
        f"{spell.range_index}, {spell.speed}, "
        f"{spell.school_mask}, {_sql_val(spell.name)});"
    )
    lines.append("")

    for i, eff in enumerate(spell.effects):
        lines.append(f"-- serverside_spell_effect (index {i})")
        lines.append(
            f"REPLACE INTO `world`.`serverside_spell_effect` "
            f"(`SpellID`, `EffectIndex`, `DifficultyID`, `Effect`, `EffectAura`, "
            f"`EffectAmplitude`, `EffectAttributes`, `EffectAuraPeriod`, "
            f"`EffectBasePoints`, `EffectBonusCoefficient`, `EffectChainAmplitude`, "
            f"`EffectChainTargets`, `EffectItemType`, `EffectMechanic`, "
            f"`EffectPointsPerResource`, `EffectTriggerSpell`, "
            f"`BonusCoefficientFromAP`, `PvpMultiplier`, `Coefficient`, "
            f"`Variance`, `ResourceCoefficient`, `GroupSizeBasePointsCoefficient`, "
            f"`EffectMiscValue1`, `EffectMiscValue2`, "
            f"`EffectRadiusIndex1`, `EffectRadiusIndex2`, "
            f"`EffectSpellClassMask1`, `EffectSpellClassMask2`, "
            f"`EffectSpellClassMask3`, `EffectSpellClassMask4`, "
            f"`ImplicitTarget1`, `ImplicitTarget2`) "
            f"VALUES ({sid}, {i}, 0, {eff.effect}, {eff.aura}, "
            f"{eff.amplitude}, {eff.attributes}, {eff.aura_period}, "
            f"{int(eff.base_points)}, {eff.bonus_coefficient}, "
            f"{eff.chain_amplitude}, {eff.chain_targets}, "
            f"{eff.item_type}, {eff.mechanic}, {eff.points_per_resource}, "
            f"{eff.trigger_spell}, "
            f"{eff.bonus_coefficient_ap}, {eff.pvp_multiplier}, "
            f"{eff.coefficient}, {eff.variance}, "
            f"{eff.resource_coefficient}, {eff.group_size_coefficient}, "
            f"{eff.misc_value_0}, {eff.misc_value_1}, "
            f"{eff.radius_0}, {eff.radius_1}, "
            f"0, 0, 0, 0, "
            f"{eff.target_a}, {eff.target_b});"
        )
        lines.append("")

    return "\n".join(lines)

# ---------------------------------------------------------------------------
# Templates
# ---------------------------------------------------------------------------
def template_ground_mount(spell_id: int, name: str, display_id: int,
                          speed: int = 100, icon: int = 0) -> SpellData:
    """Ground mount spell template (like retail mount spells)."""
    s = SpellData(
        spell_id=spell_id, name=name,
        description=f"Summons and dismisses {name}.",
        # Typical mount attributes
        attributes=[
            0x10100010,  # Attr0: NOT_SHAPESHIFT|CASTABLE_WHILE_MOUNTED|DONT_DISPLAY_COOLDOWN
            0, 0,
            0x20000000,  # Attr3: IGNORE_CASTER_MODIFIERS
            0, 0,
            0x00020000,  # Attr6: NOT_IN_RAID_INSTANCE
            0x00000200,  # Attr7: ...
            0x00001000,  # Attr8: ...
            0, 0, 0, 0, 0, 0, 0, 0
        ],
        casting_time_index=16,   # 1500ms
        duration_index=21,       # Infinite
        range_index=1,           # Self
        school_mask=1,           # Physical
        icon_file_data_id=icon or 2143095,
    )
    s.add_effect(
        effect=6,            # APPLY_AURA
        aura=78,             # MOUNTED
        base_points=0,
        misc_value_0=display_id,
        misc_value_1=speed,  # speed category
        target_a=1,          # UNIT_CASTER
    )
    return s

def template_flying_mount(spell_id: int, name: str, display_id: int,
                          icon: int = 0) -> SpellData:
    """Flying mount spell template."""
    s = template_ground_mount(spell_id, name, display_id, 310, icon)
    s.description = f"Summons and dismisses {name}."
    # Add flying aura effect
    s.add_effect(
        effect=6,            # APPLY_AURA
        aura=155,            # FLY
        base_points=0,
        target_a=1,          # UNIT_CASTER
    )
    return s

def template_passive_aura(spell_id: int, name: str, aura_type: int,
                           base_points: int = 0, misc_value: int = 0) -> SpellData:
    """Simple passive aura spell."""
    s = SpellData(
        spell_id=spell_id, name=name,
        attributes=[0x10000040] + [0]*16,  # SPELL_ATTR0_PASSIVE (0x40) | NOT_SHAPESHIFT (0x10000000)
        duration_index=21,   # Infinite
        range_index=1,       # Self
        school_mask=1,
    )
    s.add_effect(
        effect=6, aura=aura_type, base_points=base_points,
        misc_value_0=misc_value, target_a=1,
    )
    return s

def template_damage(spell_id: int, name: str, damage: int,
                    school: int = 1, cast_time: int = 1,
                    range_idx: int = 4) -> SpellData:
    """Direct damage spell."""
    s = SpellData(
        spell_id=spell_id, name=name,
        description=f"Deals {damage} damage to the target.",
        casting_time_index=cast_time, range_index=range_idx,
        school_mask=school,
    )
    s.add_effect(
        effect=2,            # SCHOOL_DAMAGE
        base_points=damage, target_a=6,  # TARGET_ENEMY
    )
    return s

def template_heal(spell_id: int, name: str, amount: int,
                  cast_time: int = 5, range_idx: int = 5) -> SpellData:
    """Direct heal spell."""
    s = SpellData(
        spell_id=spell_id, name=name,
        description=f"Heals the target for {amount}.",
        casting_time_index=cast_time, range_index=range_idx,
        school_mask=2,  # Holy
    )
    s.add_effect(
        effect=10,           # HEAL
        base_points=amount, target_a=21,  # TARGET_ALLY
    )
    return s

def template_dot(spell_id: int, name: str, tick_damage: int,
                 tick_ms: int = 3000, duration: int = 1,
                 school: int = 32) -> SpellData:
    """Periodic damage (DoT) spell."""
    s = SpellData(
        spell_id=spell_id, name=name,
        aura_description=f"Suffering {tick_damage} damage every {tick_ms//1000} sec.",
        duration_index=duration, range_index=4,
        school_mask=school,
    )
    s.add_effect(
        effect=6, aura=3,   # PERIODIC_DAMAGE
        base_points=tick_damage, aura_period=tick_ms,
        target_a=6,
    )
    return s

def template_hot(spell_id: int, name: str, tick_heal: int,
                 tick_ms: int = 3000, duration: int = 1) -> SpellData:
    """Periodic heal (HoT) spell."""
    s = SpellData(
        spell_id=spell_id, name=name,
        aura_description=f"Restores {tick_heal} health every {tick_ms//1000} sec.",
        duration_index=duration, range_index=5,
        school_mask=8,  # Nature
    )
    s.add_effect(
        effect=6, aura=8,   # PERIODIC_HEAL
        base_points=tick_heal, aura_period=tick_ms,
        target_a=21,
    )
    return s

def template_teleport(spell_id: int, name: str, map_id: int,
                      x: float = 0, y: float = 0, z: float = 0,
                      o: float = 0) -> SpellData:
    """Teleport spell (requires serverside_spell for coordinates)."""
    s = SpellData(
        spell_id=spell_id, name=name, serverside=True,
        description=f"Teleports the caster.",
        casting_time_index=5,  # 2s
        range_index=1,
        school_mask=64,  # Arcane
    )
    s.add_effect(
        effect=252,  # placeholder — real teleport uses SPELL_EFFECT_TELEPORT_UNITS
        target_a=1,
        misc_value_0=map_id,
    )
    return s

def template_visual_only(spell_id: int, name: str, visual_kit: int) -> SpellData:
    """Visual-only spell (SpellVisualKit aura)."""
    s = SpellData(
        spell_id=spell_id, name=name,
        duration_index=21, range_index=1, school_mask=1,
        attributes=[0x10000000] + [0]*16,
    )
    s.add_effect(
        effect=6, aura=237,  # OVERRIDE_SPELL_VISUAL
        misc_value_0=visual_kit,
        target_a=1,
    )
    return s

def template_learn_spell(spell_id: int, name: str,
                         target_spell: int) -> SpellData:
    """Teach-another-spell template."""
    s = SpellData(
        spell_id=spell_id, name=name,
        description=f"Teaches {name}.",
        range_index=1, school_mask=1,
    )
    s.add_effect(
        effect=36,           # LEARN_SPELL
        trigger_spell=target_spell,
        target_a=1,
    )
    return s

def template_dummy_trigger(spell_id: int, name: str,
                           trigger: int = 0) -> SpellData:
    """Dummy/script effect spell (for custom C++ handlers)."""
    s = SpellData(
        spell_id=spell_id, name=name,
        range_index=1, school_mask=1,
    )
    if trigger:
        s.add_effect(effect=64, trigger_spell=trigger, target_a=1)
    else:
        s.add_effect(effect=3, target_a=1)  # DUMMY
    return s

TEMPLATES = OrderedDict([
    ("ground_mount",  ("Ground Mount",           template_ground_mount)),
    ("flying_mount",  ("Flying Mount",           template_flying_mount)),
    ("passive",       ("Passive Aura",           template_passive_aura)),
    ("damage",        ("Direct Damage",          template_damage)),
    ("heal",          ("Direct Heal",            template_heal)),
    ("dot",           ("Periodic Damage (DoT)",  template_dot)),
    ("hot",           ("Periodic Heal (HoT)",    template_hot)),
    ("teleport",      ("Teleport (serverside)",  template_teleport)),
    ("visual",        ("Visual Effect Only",     template_visual_only)),
    ("learn",         ("Teach Another Spell",    template_learn_spell)),
    ("dummy",         ("Dummy/Script Effect",    template_dummy_trigger)),
])

# ---------------------------------------------------------------------------
# Clone from CSV
# ---------------------------------------------------------------------------
def _int(row: dict, key: str, default: int = 0) -> int:
    try:
        return int(row.get(key, default))
    except (ValueError, TypeError):
        return default

def _float(row: dict, key: str, default: float = 0.0) -> float:
    try:
        return float(row.get(key, default))
    except (ValueError, TypeError):
        return default

def clone_spell_from_csv(source_id: int, new_id: int) -> Optional[SpellData]:
    """Clone an existing spell from wago CSVs to a new ID."""
    data = lookup_spell(source_id)
    if not data:
        return None

    s = SpellData(spell_id=new_id, name=data.get("name", f"Clone of {source_id}"))
    s.subtext = data.get("subtext", "")
    s.description = data.get("description", "")
    s.aura_description = data.get("aura_description", "")

    # Copy misc data
    misc_rows = data.get("misc", [])
    if misc_rows:
        m = misc_rows[0] if isinstance(misc_rows, list) else misc_rows
        for i in range(17):
            # CSV uses Attributes_0..15 (16 values); DB has Attributes1..17
            key = f"Attributes_{i}"
            try:
                s.attributes[i] = int(m.get(key, 0))
            except (ValueError, TypeError):
                s.attributes[i] = 0
        s.casting_time_index = _int(m, "CastingTimeIndex", 1)
        s.duration_index = _int(m, "DurationIndex")
        s.pvp_duration_index = _int(m, "PvPDurationIndex")
        s.range_index = _int(m, "RangeIndex", 1)
        s.school_mask = _int(m, "SchoolMask", 1)
        s.speed = _float(m, "Speed")
        s.launch_delay = _float(m, "LaunchDelay")
        s.min_duration = _float(m, "MinDuration")
        s.icon_file_data_id = _int(m, "SpellIconFileDataID")
        s.content_tuning_id = _int(m, "ContentTuningID")
        s.spell_visual_script = _int(m, "SpellVisualScript")

    # Copy effects (all fields)
    eff_rows = data.get("effects", [])
    for row in eff_rows:
        eff = SpellEffect()
        eff.effect = _int(row, "Effect")
        eff.aura = _int(row, "EffectAura")
        eff.base_points = _float(row, "EffectBasePointsF")
        eff.misc_value_0 = _int(row, "EffectMiscValue_0")
        eff.misc_value_1 = _int(row, "EffectMiscValue_1")
        eff.target_a = _int(row, "ImplicitTarget_0")
        eff.target_b = _int(row, "ImplicitTarget_1")
        eff.trigger_spell = _int(row, "EffectTriggerSpell")
        eff.amplitude = _float(row, "EffectAmplitude")
        eff.aura_period = _int(row, "EffectAuraPeriod")
        eff.chain_targets = _int(row, "EffectChainTargets")
        eff.chain_amplitude = _float(row, "EffectChainAmplitude", 1.0)
        eff.radius_0 = _int(row, "EffectRadiusIndex_0")
        eff.radius_1 = _int(row, "EffectRadiusIndex_1")
        eff.pvp_multiplier = _float(row, "PvpMultiplier", 1.0)
        eff.mechanic = _int(row, "EffectMechanic")
        eff.item_type = _int(row, "EffectItemType")
        eff.points_per_resource = _float(row, "EffectPointsPerResource")
        eff.bonus_coefficient = _float(row, "EffectBonusCoefficient")
        eff.bonus_coefficient_ap = _float(row, "BonusCoefficientFromAP")
        eff.coefficient = _float(row, "Coefficient")
        eff.variance = _float(row, "Variance")
        eff.resource_coefficient = _float(row, "ResourceCoefficient")
        eff.group_size_coefficient = _float(row, "GroupSizeBasePointsCoefficient", 1.0)
        eff.scaling_class = _int(row, "ScalingClass")
        eff.attributes = _int(row, "EffectAttributes")
        s.effects.append(eff)

    # Copy SpellXSpellVisual
    vis_rows = data.get("visuals", [])
    if vis_rows:
        v = vis_rows[0] if isinstance(vis_rows, list) else vis_rows
        s.visual_id = _int(v, "SpellVisualID")

    return s

# ---------------------------------------------------------------------------
# Interactive UI
# ---------------------------------------------------------------------------
def prompt(text: str, default: Any = None, type_fn=str) -> Any:
    """Prompt user for input with optional default."""
    if default is not None:
        raw = input(f"  {text} [{default}]: ").strip()
        if not raw:
            return default
    else:
        raw = input(f"  {text}: ").strip()
        if not raw:
            return type_fn() if type_fn in (str,) else 0
    try:
        return type_fn(raw)
    except (ValueError, TypeError):
        print(err(f"Invalid input: {raw}"))
        return default if default is not None else type_fn()

def prompt_enum(text: str, enum_dict: dict, default: int = 0) -> int:
    """Prompt for an enum value with lookup support."""
    raw = input(f"  {text} [{default}] (? to list): ").strip()
    if not raw:
        return default
    if raw == "?":
        for k, v in sorted(enum_dict.items()):
            print(f"    {C.CYAN}{k:>4}{C.RESET}  {v}")
        return prompt_enum(text, enum_dict, default)
    if raw.startswith("/"):
        # Search mode
        q = raw[1:].lower()
        found = [(k, v) for k, v in enum_dict.items() if q in v.lower()]
        if found:
            for k, v in found[:15]:
                print(f"    {C.CYAN}{k:>4}{C.RESET}  {v}")
        else:
            print(dim("    No matches."))
        return prompt_enum(text, enum_dict, default)
    try:
        return int(raw)
    except ValueError:
        print(err(f"Expected a number, got: {raw}"))
        return default

def interactive_lookup():
    """Interactive spell lookup."""
    print(hdr("Spell Lookup"))
    query = input("  Search by name or ID: ").strip()
    if not query:
        return

    if query.isdigit():
        spell_id = int(query)
        data = lookup_spell(spell_id)
        if not data:
            print(err(f"Spell {spell_id} not found in CSVs."))
            return
        _print_spell_data(data)
    else:
        results = search_spells(query, 25)
        if not results:
            print(err(f"No spells matching '{query}'."))
            return
        print(f"\n  {C.BOLD}Found {len(results)} matches:{C.RESET}")
        for sid, name in results:
            print(f"    {C.CYAN}{sid:>8}{C.RESET}  {name}")
        print()
        choice = input("  Enter ID for details (or Enter to go back): ").strip()
        if choice.isdigit():
            data = lookup_spell(int(choice))
            if data:
                _print_spell_data(data)

def _print_spell_data(data: dict):
    """Pretty-print spell lookup data."""
    print(f"\n  {C.BOLD}{C.GREEN}{data.get('name', '?')}{C.RESET} "
          f"(ID: {C.CYAN}{data['spell_id']}{C.RESET})")
    if data.get("subtext"):
        print(f"  {C.DIM}Subtext:{C.RESET} {data['subtext']}")
    if data.get("description"):
        print(f"  {C.DIM}Description:{C.RESET} {data['description']}")
    if data.get("aura_description"):
        print(f"  {C.DIM}Aura Desc:{C.RESET} {data['aura_description']}")

    misc_rows = data.get("misc", [])
    if misc_rows:
        m = misc_rows[0] if isinstance(misc_rows, list) else misc_rows
        ct = int(m.get("CastingTimeIndex", 0))
        dur = int(m.get("DurationIndex", 0))
        rng = int(m.get("RangeIndex", 0))
        sch = int(m.get("SchoolMask", 0))
        ct_label = CAST_TIME_INDEX.get(ct, None)
        if ct_label is None:
            # Try resolving from SpellCastTimes CSV
            ct_csv = _csv_by_id("SpellCastTimes")
            if ct in ct_csv:
                ct_label = f"{ct_csv[ct].get('Base', '?')}ms"
            else:
                ct_label = str(ct)
        dur_label = DURATION_INDEX.get(dur, None)
        if dur_label is None:
            dur_csv = _csv_by_id("SpellDuration")
            if dur in dur_csv:
                ms = int(dur_csv[dur].get("Duration", 0))
                dur_label = f"{ms}ms ({ms//1000}s)" if ms > 0 else f"idx {dur}"
            else:
                dur_label = str(dur)
        rng_label = RANGE_INDEX.get(rng, None)
        if rng_label is None:
            rng_csv = _csv_by_id("SpellRange")
            if rng in rng_csv:
                rng_label = f"{rng_csv[rng].get('DisplayName_lang', rng_csv[rng].get('RangeMax_0', '?'))}"
            else:
                rng_label = str(rng)
        print(f"  {C.DIM}Cast:{C.RESET} {ct_label}  "
              f"{C.DIM}Duration:{C.RESET} {dur_label}  "
              f"{C.DIM}Range:{C.RESET} {rng_label}  "
              f"{C.DIM}School:{C.RESET} {SCHOOL_MASKS.get(sch, sch)}")

    effects = data.get("effects", [])
    if effects:
        print(f"\n  {C.BOLD}Effects ({len(effects)}):{C.RESET}")
        for eff in effects:
            eidx = eff.get("EffectIndex", "?")
            etype = int(eff.get("Effect", 0))
            aura = int(eff.get("EffectAura", 0))
            bp = eff.get("EffectBasePointsF", "0")
            mv0 = eff.get("EffectMiscValue_0", "0")
            ta = int(eff.get("ImplicitTarget_0", 0))
            tb = int(eff.get("ImplicitTarget_1", 0))
            ename = SPELL_EFFECTS.get(etype, f"EFFECT_{etype}")
            aname = AURA_TYPES.get(aura, f"AURA_{aura}") if aura else ""
            trig = eff.get("EffectTriggerSpell", "0")

            line = f"    [{eidx}] {C.YELLOW}{ename}{C.RESET}"
            if aura:
                line += f" -> {C.MAGENTA}{aname}{C.RESET}"
            line += f"  BP={bp}  MV={mv0}"
            line += f"  Target={IMPLICIT_TARGETS.get(ta, ta)}"
            if tb:
                line += f"/{IMPLICIT_TARGETS.get(tb, tb)}"
            if trig and trig != "0":
                line += f"  Trigger={trig}"
            print(line)
    print()

def interactive_create():
    """Guided spell creation."""
    print(hdr("Create New Spell (Guided)"))

    sid = prompt("Spell ID", 0, int)
    if sid == 0:
        sid = find_next_free_id()
        print(info(f"Auto-assigned ID: {sid}"))

    s = SpellData(spell_id=sid)
    s.name = prompt("Spell Name", "New Spell")
    s.description = prompt("Description", "")
    s.subtext = prompt("Subtext", "")
    s.aura_description = prompt("Aura Description", "")

    print(f"\n  {C.BOLD}Spell Properties:{C.RESET}")
    s.casting_time_index = prompt_enum("Cast Time Index", CAST_TIME_INDEX, 1)
    s.duration_index = prompt_enum("Duration Index", DURATION_INDEX, 0)
    s.range_index = prompt_enum("Range Index", RANGE_INDEX, 1)
    s.school_mask = prompt_enum("School Mask", SCHOOL_MASKS, 1)
    s.icon_file_data_id = prompt("Icon FileDataID", 136243, int)

    # Attributes
    print(f"\n  {C.DIM}(Enter attribute flags as hex or decimal, 0 to skip){C.RESET}")
    for i in range(17):
        raw = input(f"  Attributes[{i}] [0]: ").strip()
        if raw:
            try:
                s.attributes[i] = int(raw, 0)  # auto-detect hex/dec
            except ValueError:
                s.attributes[i] = 0

    # Effects
    num_effects = prompt("Number of effects", 1, int)
    for i in range(num_effects):
        print(f"\n  {C.BOLD}Effect {i}:{C.RESET}")
        eff = SpellEffect()
        eff.effect = prompt_enum("Effect Type", SPELL_EFFECTS, 3)
        if eff.effect == 6:  # APPLY_AURA
            eff.aura = prompt_enum("Aura Type", AURA_TYPES, 4)
        eff.base_points = prompt("Base Points", 0, float)
        eff.misc_value_0 = prompt("MiscValue[0]", 0, int)
        eff.misc_value_1 = prompt("MiscValue[1]", 0, int)
        eff.target_a = prompt_enum("ImplicitTarget A", IMPLICIT_TARGETS, 1)
        eff.target_b = prompt_enum("ImplicitTarget B", IMPLICIT_TARGETS, 0)
        eff.trigger_spell = prompt("Trigger Spell", 0, int)
        eff.aura_period = prompt("Aura Period (ms)", 0, int)
        s.effects.append(eff)

    # Visual
    s.visual_id = prompt("SpellVisualID (0 = none)", 0, int)

    # Serverside?
    s.serverside = prompt("Serverside spell? (y/n)", "n") in ("y", "Y", "yes")

    _output_sql(s)

def interactive_template():
    """Create from template."""
    print(hdr("Create from Template"))
    for i, (key, (label, _)) in enumerate(TEMPLATES.items(), 1):
        print(f"  {C.CYAN}{i:>2}{C.RESET}. {label}")

    choice = prompt("\nTemplate #", 1, int)
    keys = list(TEMPLATES.keys())
    if choice < 1 or choice > len(keys):
        print(err("Invalid choice."))
        return

    tkey = keys[choice - 1]
    label, func = TEMPLATES[tkey]
    print(f"\n{info(f'Template: {label}')}")

    sid = prompt("Spell ID (0 = auto)", 0, int)
    if sid == 0:
        sid = find_next_free_id()
        print(info(f"Auto-assigned ID: {sid}"))

    name = prompt("Spell Name", label)

    # Template-specific params
    if tkey == "ground_mount":
        display_id = prompt("Mount DisplayID", 0, int)
        speed = prompt("Speed modifier", 100, int)
        icon = prompt("Icon FileDataID", 0, int)
        spell = func(sid, name, display_id, speed, icon)
    elif tkey == "flying_mount":
        display_id = prompt("Mount DisplayID", 0, int)
        icon = prompt("Icon FileDataID", 0, int)
        spell = func(sid, name, display_id, icon)
    elif tkey == "passive":
        aura = prompt_enum("Aura Type", AURA_TYPES, 4)
        bp = prompt("Base Points", 0, int)
        mv = prompt("MiscValue", 0, int)
        spell = func(sid, name, aura, bp, mv)
    elif tkey == "damage":
        dmg = prompt("Damage amount", 100, int)
        school = prompt_enum("School", SCHOOL_MASKS, 4)
        ct = prompt_enum("Cast Time", CAST_TIME_INDEX, 1)
        rng = prompt_enum("Range", RANGE_INDEX, 4)
        spell = func(sid, name, dmg, school, ct, rng)
    elif tkey == "heal":
        amt = prompt("Heal amount", 100, int)
        ct = prompt_enum("Cast Time", CAST_TIME_INDEX, 5)
        rng = prompt_enum("Range", RANGE_INDEX, 5)
        spell = func(sid, name, amt, ct, rng)
    elif tkey == "dot":
        tick = prompt("Tick damage", 50, int)
        period = prompt("Tick interval (ms)", 3000, int)
        dur = prompt_enum("Duration", DURATION_INDEX, 1)
        school = prompt_enum("School", SCHOOL_MASKS, 32)
        spell = func(sid, name, tick, period, dur, school)
    elif tkey == "hot":
        tick = prompt("Tick heal", 50, int)
        period = prompt("Tick interval (ms)", 3000, int)
        dur = prompt_enum("Duration", DURATION_INDEX, 1)
        spell = func(sid, name, tick, period, dur)
    elif tkey == "teleport":
        map_id = prompt("Map ID", 0, int)
        x = prompt("X", 0.0, float)
        y = prompt("Y", 0.0, float)
        z = prompt("Z", 0.0, float)
        spell = func(sid, name, map_id, x, y, z)
    elif tkey == "visual":
        vkit = prompt("SpellVisualKitID", 0, int)
        spell = func(sid, name, vkit)
    elif tkey == "learn":
        target = prompt("Spell to teach (ID)", 0, int)
        spell = func(sid, name, target)
    elif tkey == "dummy":
        trigger = prompt("Trigger spell ID (0 = dummy)", 0, int)
        spell = func(sid, name, trigger)
    else:
        spell = func(sid, name)

    _output_sql(spell)

def interactive_clone():
    """Clone an existing spell."""
    print(hdr("Clone Existing Spell"))
    source = prompt("Source Spell ID", 0, int)
    if source == 0:
        return

    new_id = prompt("New Spell ID (0 = auto)", 0, int)
    if new_id == 0:
        new_id = find_next_free_id()
        print(info(f"Auto-assigned ID: {new_id}"))

    spell = clone_spell_from_csv(source, new_id)
    if not spell:
        print(err(f"Spell {source} not found in CSVs."))
        return

    print(info(f"Cloned '{spell.name}' from {source} -> {new_id}"))

    # Allow renaming
    new_name = input(f"  New name [{spell.name}]: ").strip()
    if new_name:
        spell.name = new_name

    _output_sql(spell)

def interactive_enum_ref():
    """Show enum reference tables."""
    print(hdr("Enum Reference"))
    refs = [
        ("Spell Effects", SPELL_EFFECTS),
        ("Aura Types", AURA_TYPES),
        ("Implicit Targets", IMPLICIT_TARGETS),
        ("School Masks", SCHOOL_MASKS),
        ("Cast Time Index", CAST_TIME_INDEX),
        ("Duration Index", DURATION_INDEX),
        ("Range Index", RANGE_INDEX),
    ]
    for i, (name, _) in enumerate(refs, 1):
        print(f"  {C.CYAN}{i}{C.RESET}. {name}")

    choice = prompt("\nWhich reference?", 1, int)
    if choice < 1 or choice > len(refs):
        return

    name, d = refs[choice - 1]
    print(f"\n  {C.BOLD}{name}:{C.RESET}")

    # Search support
    search = input(f"  Filter (or Enter for all): ").strip().lower()
    items = sorted(d.items())
    if search:
        items = [(k, v) for k, v in items if search in v.lower() or search == str(k)]

    for k, v in items:
        print(f"    {C.CYAN}{k:>4}{C.RESET}  {v}")
    print()

def interactive_table_hashes():
    """Show spell table hashes."""
    print(hdr("Spell Table Hashes"))
    print(f"  {'Table':<28} {'Hash (hex)':<14} {'Hash (dec)'}")
    print(f"  {'-'*28} {'-'*14} {'-'*12}")
    for name, h in sorted(SPELL_TABLE_HASHES.items()):
        print(f"  {name:<28} 0x{h:08X}     {h}")
    print()

def interactive_icon_search():
    """Search spell icons by keyword."""
    print(hdr("Icon Search"))
    if not ICON_DIR.exists():
        print(err(f"Icon directory not found: {ICON_DIR}"))
        return

    query = input("  Search icons (keyword): ").strip().lower()
    if not query:
        return

    matches = []
    for f in ICON_DIR.iterdir():
        if f.suffix == ".png" and query in f.stem.lower():
            matches.append(f.stem)
            if len(matches) >= 30:
                break

    if not matches:
        print(err(f"No icons matching '{query}'."))
        return

    print(f"\n  {C.BOLD}Found {len(matches)} matches:{C.RESET}")
    for name in sorted(matches):
        print(f"    {C.CYAN}{name}{C.RESET}.png")
    print(f"\n  {C.DIM}Path: {ICON_DIR}{C.RESET}")
    print(f"  {C.DIM}Tip: Use wow.tools.local to find the FileDataID for an icon{C.RESET}")
    print()

def _output_sql(spell: SpellData):
    """Generate SQL and offer output options."""
    sql = generate_sql(spell)

    print(f"\n{C.GREEN}{C.BOLD}Generated SQL:{C.RESET}")
    print(f"{C.DIM}{'─' * 60}{C.RESET}")
    print(sql)
    print(f"{C.DIM}{'─' * 60}{C.RESET}")

    print(f"\n  1. Copy to clipboard")
    print(f"  2. Save to SQL update file")
    print(f"  3. Apply directly to database")
    print(f"  4. Apply + reload via SOAP")
    print(f"  5. Done")

    choice = prompt("\nAction", 5, int)
    if choice == 1:
        _copy_to_clipboard(sql)
    elif choice == 2:
        _save_to_file(sql, spell)
    elif choice == 3:
        _apply_to_db(sql, spell)
    elif choice == 4:
        _apply_to_db(sql, spell)
        _soap_reload(spell)

def _copy_to_clipboard(text: str):
    """Copy text to clipboard via clip.exe."""
    try:
        proc = subprocess.Popen(["clip"], stdin=subprocess.PIPE)
        proc.communicate(text.encode("utf-8"))
        print(info("Copied to clipboard!"))
    except FileNotFoundError:
        print(err("clip.exe not found. Copy the SQL manually."))

def _save_to_file(sql: str, spell: SpellData):
    """Save SQL to a properly named update file."""
    import datetime
    today = datetime.date.today().strftime("%Y_%m_%d")
    db = "world" if spell.serverside else "hotfixes"
    update_dir = PROJECT_ROOT / "sql" / "updates" / db / "master"

    # Find next sequence number
    existing = list(update_dir.glob(f"{today}_*_{db}.sql"))
    seq = 0
    for f in existing:
        parts = f.stem.split("_")
        if len(parts) >= 4:
            try:
                seq = max(seq, int(parts[3]) + 1)
            except ValueError:
                pass

    filename = f"{today}_{seq:02d}_{db}.sql"
    filepath = update_dir / filename

    if not update_dir.exists():
        update_dir.mkdir(parents=True, exist_ok=True)

    with open(filepath, "w", encoding="utf-8") as f:
        f.write(sql + "\n")

    print(info(f"Saved to: {filepath}"))

def _apply_to_db(sql: str, spell: SpellData):
    """Apply SQL directly to the database."""
    db = "world" if spell.serverside else "hotfixes"
    confirm = input(f"  Apply to {db} database? (yes/no): ").strip()
    if confirm.lower() != "yes":
        print(dim("  Cancelled."))
        return

    # Split multi-statement SQL into individual statements for MySQL
    stmts = [s.strip() for s in sql.split(";") if s.strip() and not s.strip().startswith("--")]
    success = True
    for stmt in stmts:
        result = run_mysql(stmt + ";", db)
        if result is None:
            success = False
            break

    if success:
        print(info(f"Applied successfully to {db}!"))
        if spell.serverside:
            print(info("Reload in-game: .reload serverside_spell"))
        else:
            print(info("Reload in-game: .reload hotfixes"))
            print(dim("  Or use option 4 to auto-reload via SOAP."))
    else:
        print(err("Failed to apply. Check the SQL output above for errors."))

def _soap_reload(spell: SpellData):
    """Send reload command via SOAP to the running worldserver."""
    soap_script = PROJECT_ROOT / "wago" / "tc_soap.sh"
    cmd = "reload serverside_spell" if spell.serverside else "reload hotfixes"
    if not soap_script.exists():
        print(warn(f"SOAP script not found: {soap_script}"))
        print(dim(f"  Run in-game instead: .{cmd}"))
        return
    try:
        result = subprocess.run(
            ["bash", str(soap_script), cmd],
            capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=15
        )
        if result.returncode == 0:
            output = result.stdout.strip()
            if output:
                print(info(f"SOAP: {output}"))
            else:
                print(info(f"SOAP reload sent: .{cmd}"))
        else:
            stderr = result.stderr.strip()
            print(warn(f"SOAP failed (server may not be running): {stderr[:100]}"))
            print(dim(f"  Run in-game instead: .{cmd}"))
    except FileNotFoundError:
        print(warn("bash not found. Run in-game instead: ." + cmd))
    except subprocess.TimeoutExpired:
        print(warn("SOAP timed out. Server may not be running."))

# ---------------------------------------------------------------------------
# Main Menu
# ---------------------------------------------------------------------------
def main_menu():
    """Main interactive menu loop."""
    print(hdr("VoxCore Spell Creator"))
    print(f"  {C.DIM}Build: {VERIFIED_BUILD} | CSV Dir: {WAGO_CSV_DIR}{C.RESET}")
    print()

    while True:
        print(f"  {C.CYAN}1{C.RESET}. Create New Spell (guided)")
        print(f"  {C.CYAN}2{C.RESET}. Create from Template")
        print(f"  {C.CYAN}3{C.RESET}. Clone Existing Spell")
        print(f"  {C.CYAN}4{C.RESET}. Look Up Spell")
        print(f"  {C.CYAN}5{C.RESET}. Enum Reference")
        print(f"  {C.CYAN}6{C.RESET}. Table Hashes")
        print(f"  {C.CYAN}7{C.RESET}. Search Icons")
        print(f"  {C.CYAN}0{C.RESET}. Exit")
        print()

        choice = prompt("Choice", 0, int)
        if choice == 0:
            break
        elif choice == 1:
            interactive_create()
        elif choice == 2:
            interactive_template()
        elif choice == 3:
            interactive_clone()
        elif choice == 4:
            interactive_lookup()
        elif choice == 5:
            interactive_enum_ref()
        elif choice == 6:
            interactive_table_hashes()
        elif choice == 7:
            interactive_icon_search()
        else:
            print(err("Invalid choice."))
        print()

# ---------------------------------------------------------------------------
# CLI entry point
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(
        description="VoxCore Spell Creator - interactive spell creation & hotfix SQL generator",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=textwrap.dedent("""\
            Quick commands:
              lookup <id>                  Look up a spell by ID
              search <name>               Search spells by name
              clone <source_id> <new_id>  Clone a spell to a new ID
              template <name>             Create from template (ground_mount, damage, etc.)
              hashes                      Show spell table hashes
              icons <keyword>             Search spell icons by name
        """),
    )
    parser.add_argument("command", nargs="?", default="menu",
                        help="Command: menu, lookup, search, clone, template, hashes")
    parser.add_argument("args", nargs="*", help="Command arguments")

    args = parser.parse_args()
    cmd = args.command.lower()

    if cmd == "menu" or cmd == "interactive":
        main_menu()

    elif cmd == "lookup":
        if not args.args:
            print(err("Usage: spell_creator.py lookup <spell_id>"))
            return
        spell_id = int(args.args[0])
        data = lookup_spell(spell_id)
        if data:
            _print_spell_data(data)
        else:
            print(err(f"Spell {spell_id} not found."))

    elif cmd == "search":
        if not args.args:
            print(err("Usage: spell_creator.py search <name>"))
            return
        query = " ".join(args.args)
        results = search_spells(query, 30)
        if results:
            for sid, name in results:
                print(f"  {C.CYAN}{sid:>8}{C.RESET}  {name}")
        else:
            print(err(f"No spells matching '{query}'."))

    elif cmd == "clone":
        if len(args.args) < 2:
            print(err("Usage: spell_creator.py clone <source_id> <new_id>"))
            return
        source = int(args.args[0])
        new_id = int(args.args[1])
        spell = clone_spell_from_csv(source, new_id)
        if spell:
            print(generate_sql(spell))
        else:
            print(err(f"Spell {source} not found."))

    elif cmd == "template":
        if not args.args:
            print(err(f"Usage: spell_creator.py template <name>"))
            print(f"  Available: {', '.join(TEMPLATES.keys())}")
            return
        tkey = args.args[0]
        if tkey not in TEMPLATES:
            print(err(f"Unknown template: {tkey}"))
            print(f"  Available: {', '.join(TEMPLATES.keys())}")
            return
        interactive_template()

    elif cmd == "hashes":
        interactive_table_hashes()

    elif cmd == "icons":
        if not args.args:
            print(err("Usage: spell_creator.py icons <keyword>"))
            return
        query = " ".join(args.args).lower()
        if not ICON_DIR.exists():
            print(err(f"Icon directory not found: {ICON_DIR}"))
            return
        matches = sorted(f.stem for f in ICON_DIR.iterdir()
                         if f.suffix == ".png" and query in f.stem.lower())[:30]
        if matches:
            for name in matches:
                print(f"  {C.CYAN}{name}{C.RESET}.png")
            print(f"\n  {dim(f'Path: {ICON_DIR}')}")
        else:
            print(err(f"No icons matching '{query}'."))

    else:
        print(err(f"Unknown command: {cmd}"))
        parser.print_help()

if __name__ == "__main__":
    main()
