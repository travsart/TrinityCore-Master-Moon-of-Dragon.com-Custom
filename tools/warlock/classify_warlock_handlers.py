#!/usr/bin/env python3
"""
classify_warlock_handlers.py -- Cross-reference warlock registry against existing C++ handlers.

Scans spell_warlock.cpp for RegisterSpellScript/RegisterAuraScript calls,
parses SPELL_WARLOCK_* enum constants, and maps each registry entry to one of:
    HAS_HANDLER  -- Full script handler exists and is active
    STUB_ONLY    -- Handler exists but is incomplete/minimal
    DISABLED     -- Handler exists but is #if 0'd
    NEEDS_HANDLER -- No handler found, custom code needed
    DB_ONLY      -- Native TC handling sufficient (passive aura, proc flag, etc.)
    NATIVE       -- TrinityCore already handles natively
    UNCLASSIFIED -- Could not determine (needs manual review)

Also assigns implementation types:
    SPELLSCRIPT, AURASCRIPT, BOTH, PROC_EVENT, PET_LIFECYCLE,
    SUMMON_EXTENSION, RESOURCE_ENGINE, SPELL_REPLACEMENT,
    TARGET_CAP_CUSTOM, HYBRID, NATIVE_AURA, DB_ONLY

Usage:
    python tools/warlock/classify_warlock_handlers.py [--registry path] [--output path]
"""

from __future__ import annotations

import argparse
import json
import re
from collections import defaultdict
from pathlib import Path

_SCRIPT_DIR = Path(__file__).resolve().parent
_REPO_ROOT = _SCRIPT_DIR.parent.parent
_GENERATED_DIR = _REPO_ROOT / "doc" / "classes" / "warlock" / "generated"
_SPELL_WARLOCK_CPP = _REPO_ROOT / "src" / "server" / "scripts" / "Spells" / "spell_warlock.cpp"
_SPELL_PET_CPP = _REPO_ROOT / "src" / "server" / "scripts" / "Spells" / "spell_pet.cpp"
_SPELL_GENERIC_CPP = _REPO_ROOT / "src" / "server" / "scripts" / "Spells" / "spell_generic.cpp"


def parse_spell_constants(cpp_path: Path) -> dict[str, int]:
    """Parse SPELL_WARLOCK_* enum constants from spell_warlock.cpp."""
    constants = {}
    text = cpp_path.read_text(encoding="utf-8", errors="replace")

    for match in re.finditer(r'(SPELL_WARLOCK_\w+)\s*=\s*(\d+)', text):
        name = match.group(1)
        spell_id = int(match.group(2))
        constants[name] = spell_id

    return constants


def parse_registered_scripts(cpp_path: Path) -> dict[str, dict]:
    """Parse RegisterSpellScript/RegisterAuraScript calls from the cpp file.

    Returns dict of class_name -> {type, line, disabled, commented_out}.
    """
    scripts = {}
    text = cpp_path.read_text(encoding="utf-8", errors="replace")
    lines = text.split("\n")

    # Track #if 0 blocks
    in_disabled_block = False
    disabled_depth = 0

    for i, line in enumerate(lines, 1):
        stripped = line.strip()

        # Track #if 0 blocks
        if stripped.startswith("#if 0"):
            in_disabled_block = True
            disabled_depth += 1
        elif stripped.startswith("#endif") and in_disabled_block:
            disabled_depth -= 1
            if disabled_depth <= 0:
                in_disabled_block = False
                disabled_depth = 0

        # Parse Register calls
        for pattern in [
            r'RegisterSpellScript\(\s*(\w+)\s*\)',
            r'RegisterSpellScriptWithArgs\(\s*\w+<[^>]+>\s*,\s*"([^"]+)"\s*\)',
        ]:
            m = re.search(pattern, stripped)
            if m:
                class_name = m.group(1)
                commented = stripped.startswith("//")
                disabled = in_disabled_block or commented
                scripts[class_name] = {
                    "type": "SpellScript",
                    "line": i,
                    "disabled": disabled,
                    "commented": commented,
                }

        m = re.search(r'RegisterSpellAndAuraScriptPair\(\s*(\w+)\s*,\s*(\w+)\s*\)', stripped)
        if m:
            spell_class = m.group(1)
            aura_class = m.group(2)
            commented = stripped.startswith("//")
            disabled = in_disabled_block or commented
            scripts[spell_class] = {
                "type": "SpellScript",
                "line": i,
                "disabled": disabled,
                "commented": commented,
                "paired_with": aura_class,
            }
            scripts[aura_class] = {
                "type": "AuraScript",
                "line": i,
                "disabled": disabled,
                "commented": commented,
                "paired_with": spell_class,
            }

    return scripts


def _normalize_name(name: str) -> str:
    """Normalize a spell/class name to lowercase with underscores for matching."""
    # Remove common prefixes
    for prefix in ["spell_warl_", "spell_warlock_", "aura_warl_", "aura_warlock_", "spell_warr_"]:
        if name.startswith(prefix):
            name = name[len(prefix):]
            break
    return name.lower().replace(" ", "_").replace("'", "").replace("-", "_")


def build_spell_id_to_class_map(
    constants: dict[str, int],
    registered: dict[str, dict],
) -> dict[int, list[dict]]:
    """Build reverse map: spell_id -> [{class_name, type, disabled, line}].

    Uses heuristic matching between class names and constant names.
    """
    # Normalize constant names for matching
    # SPELL_WARLOCK_CHAOS_BOLT -> chaos_bolt
    const_to_id: dict[str, int] = {}
    for name, sid in constants.items():
        suffix = name.replace("SPELL_WARLOCK_", "").lower()
        const_to_id[suffix] = sid

    # Also build id -> constant name for reverse lookup
    id_to_consts: dict[int, list[str]] = defaultdict(list)
    for name, sid in constants.items():
        id_to_consts[sid].append(name)

    spell_id_to_classes: dict[int, list[dict]] = defaultdict(list)

    for class_name, info in registered.items():
        normalized = _normalize_name(class_name)

        # Direct match against constant suffixes
        matched_id = const_to_id.get(normalized)
        if matched_id:
            spell_id_to_classes[matched_id].append({
                "className": class_name,
                **info,
            })
            continue

        # Partial match: try adding/removing common suffixes
        matched = False
        for suffix in ["_aura", "_periodic", "_selector", "_activator", "_effect",
                       "_damage", "_generic", "_dummy", "_talent", "_passive",
                       "_entry_aura", "_at", "_dots", "_drain_life"]:
            # Try adding suffix
            trimmed = normalized + suffix
            matched_id = const_to_id.get(trimmed)
            if matched_id:
                spell_id_to_classes[matched_id].append({
                    "className": class_name,
                    **info,
                })
                matched = True
                break
            # Try removing suffix
            if normalized.endswith(suffix):
                base = normalized[:-len(suffix)]
                matched_id = const_to_id.get(base)
                if matched_id:
                    spell_id_to_classes[matched_id].append({
                        "className": class_name,
                        **info,
                    })
                    matched = True
                    break

    return dict(spell_id_to_classes)


def build_name_to_class_map(registered: dict[str, dict]) -> dict[str, list[dict]]:
    """Build a map from normalized spell names to registered script classes.

    This enables matching registry entries by name when spell IDs don't match
    (common when talent spell != effect spell).
    """
    name_map: dict[str, list[dict]] = defaultdict(list)

    for class_name, info in registered.items():
        normalized = _normalize_name(class_name)
        # Store under the normalized name
        name_map[normalized].append({"className": class_name, **info})

        # Also store under variants without common suffixes
        for suffix in ["_periodic", "_selector", "_activator", "_effect", "_dots",
                       "_drain_life", "_generic", "_dummy", "_talent", "_passive",
                       "_entry_aura", "_at", "_damage"]:
            if normalized.endswith(suffix):
                base = normalized[:-len(suffix)]
                name_map[base].append({"className": class_name, **info})

    return dict(name_map)


# Known spell IDs that are passive auras / proc-driven and likely handled natively by TC
KNOWN_PASSIVE_NATIVE = {
    # Mastery auras
    77215, 77219, 77220,
    # Stat modifiers, aura effects
}

# Known implementation types by spell name patterns
IMPL_TYPE_PATTERNS = {
    "summon": "PET_LIFECYCLE",
    "demon": "PET_LIFECYCLE",
    "tyrant": "SUMMON_EXTENSION",
    "havoc": "TARGET_CAP_CUSTOM",
    "soul_shard": "RESOURCE_ENGINE",
    "shard": "RESOURCE_ENGINE",
}


def classify_registry(
    registry: dict,
    registered_scripts: dict[str, dict],
    spell_id_to_classes: dict[int, list[dict]],
    name_to_classes: dict[str, list[dict]],
) -> dict:
    """Classify each registry entry with handler status and implementation type."""
    entries = registry["entries"]
    spell_lookup = registry.get("spellLookup", {})

    stats = defaultdict(int)

    for entry in entries:
        spell_id = entry.get("spellId", 0)
        all_spell_ids = set(entry.get("allSpellIds", []))
        name = entry.get("name", "").lower()

        # Check if any of the entry's spell IDs have a handler
        matched_classes = []
        for sid in all_spell_ids:
            if sid in spell_id_to_classes:
                matched_classes.extend(spell_id_to_classes[sid])

        # If no ID match, try name-based matching
        if not matched_classes:
            norm_name = _normalize_name(name)
            if norm_name in name_to_classes:
                matched_classes.extend(name_to_classes[norm_name])

            # Also try choice option names
            if not matched_classes and entry.get("choiceOptions"):
                for opt in entry["choiceOptions"]:
                    opt_name = _normalize_name(opt.get("name", ""))
                    if opt_name in name_to_classes:
                        matched_classes.extend(name_to_classes[opt_name])

        if matched_classes:
            # Check if any are disabled
            active = [c for c in matched_classes if not c.get("disabled")]
            disabled = [c for c in matched_classes if c.get("disabled")]

            if active:
                entry["handlerStatus"] = "HAS_HANDLER"
                entry["scriptClass"] = active[0]["className"]
                entry["scriptFile"] = "spell_warlock.cpp"
                entry["scriptLine"] = active[0]["line"]
                entry["implementationType"] = "SPELLSCRIPT"
                if any(c.get("type") == "AuraScript" for c in active):
                    entry["implementationType"] = "AURASCRIPT"
                if any(c.get("paired_with") for c in active):
                    entry["implementationType"] = "BOTH"
            elif disabled:
                entry["handlerStatus"] = "DISABLED"
                entry["scriptClass"] = disabled[0]["className"]
                entry["scriptFile"] = "spell_warlock.cpp"
                entry["scriptLine"] = disabled[0]["line"]
                entry["implementationType"] = "SPELLSCRIPT"
        else:
            # No handler found -- classify what it needs
            max_ranks = entry.get("maxRanks", 1)

            # Multi-rank passives are often native auras
            if max_ranks > 1 and not entry.get("isChoice"):
                entry["handlerStatus"] = "NEEDS_HANDLER"
                entry["implementationType"] = "NATIVE_AURA"
            elif spell_id in KNOWN_PASSIVE_NATIVE:
                entry["handlerStatus"] = "DB_ONLY"
                entry["implementationType"] = "DB_ONLY"
            else:
                entry["handlerStatus"] = "NEEDS_HANDLER"
                # Guess implementation type from name
                impl_type = None
                for pattern, itype in IMPL_TYPE_PATTERNS.items():
                    if pattern in name:
                        impl_type = itype
                        break
                entry["implementationType"] = impl_type or "SPELLSCRIPT"

        stats[entry["handlerStatus"]] += 1

        # Handle choice nodes: classify each option
        if entry.get("choiceOptions"):
            for opt in entry["choiceOptions"]:
                opt_id = opt.get("spellId", 0)
                if opt_id in spell_id_to_classes:
                    opt_classes = spell_id_to_classes[opt_id]
                    active = [c for c in opt_classes if not c.get("disabled")]
                    if active:
                        opt["handlerStatus"] = "HAS_HANDLER"
                        opt["scriptClass"] = active[0]["className"]
                    else:
                        opt["handlerStatus"] = "DISABLED"
                        opt["scriptClass"] = opt_classes[0]["className"]
                else:
                    opt["handlerStatus"] = "NEEDS_HANDLER"

    # Add classification metadata
    registry["metadata"]["handlerStats"] = dict(stats)
    registry["metadata"]["classifiedBy"] = "classify_warlock_handlers.py"
    registry["metadata"]["sourceFile"] = str(_SPELL_WARLOCK_CPP.relative_to(_REPO_ROOT))
    registry["metadata"]["totalRegisteredScripts"] = len(registered_scripts)
    registry["metadata"]["activeScripts"] = sum(
        1 for s in registered_scripts.values() if not s.get("disabled")
    )
    registry["metadata"]["disabledScripts"] = sum(
        1 for s in registered_scripts.values() if s.get("disabled")
    )

    return registry


def main():
    parser = argparse.ArgumentParser(
        description="Classify warlock registry entries against existing C++ handlers"
    )
    parser.add_argument(
        "--registry", "-r",
        default=str(_GENERATED_DIR / "warlock_registry.json"),
        help="Input registry JSON",
    )
    parser.add_argument(
        "--output", "-o",
        default=str(_GENERATED_DIR / "warlock_registry.json"),
        help="Output classified registry JSON (overwrites input by default)",
    )
    args = parser.parse_args()

    # Load registry
    with open(args.registry, "r", encoding="utf-8") as f:
        registry = json.load(f)

    print(f"[classify] Loaded registry: {registry['metadata']['totalNodes']} nodes")

    # Parse C++ sources
    print(f"[classify] Parsing {_SPELL_WARLOCK_CPP.name}...")
    constants = parse_spell_constants(_SPELL_WARLOCK_CPP)
    registered = parse_registered_scripts(_SPELL_WARLOCK_CPP)
    spell_id_map = build_spell_id_to_class_map(constants, registered)
    name_map = build_name_to_class_map(registered)

    print(f"[classify] Found {len(constants)} spell constants, "
          f"{len(registered)} registered scripts, "
          f"{len(spell_id_map)} spell ID -> class mappings, "
          f"{len(name_map)} name -> class mappings")

    # Classify
    registry = classify_registry(registry, registered, spell_id_map, name_map)

    # Write output
    out_path = Path(args.output)
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(registry, f, indent=2, ensure_ascii=False)

    print(f"\n[classify] Wrote: {out_path} ({out_path.stat().st_size:,} bytes)")
    print(f"[classify] Handler stats: {registry['metadata']['handlerStats']}")
    print(f"[classify] Registered scripts: {registry['metadata']['totalRegisteredScripts']} "
          f"({registry['metadata']['activeScripts']} active, "
          f"{registry['metadata']['disabledScripts']} disabled)")


if __name__ == "__main__":
    main()
