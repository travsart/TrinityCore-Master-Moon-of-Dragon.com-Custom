#!/usr/bin/env python3
"""
build_warlock_registry.py -- Build canonical warlock_registry.json from raw extraction.

Reads the raw extraction JSON from extract_warlock_db2.py and produces a normalized
registry where each entry represents a talent node with:
- Canonical name, spell IDs, tree ownership
- Dependencies (parent/child from edge graph)
- Priority tier (A/B/C) based on node position and mechanics
- Handler status placeholder (filled by classify_warlock_handlers.py)

Usage:
    python tools/warlock/build_warlock_registry.py [--input raw.json] [--output registry.json]
"""

from __future__ import annotations

import argparse
import csv
import json
import sys
from collections import defaultdict
from pathlib import Path

_SCRIPT_DIR = Path(__file__).resolve().parent
_REPO_ROOT = _SCRIPT_DIR.parent.parent
_GENERATED_DIR = _REPO_ROOT / "doc" / "classes" / "warlock" / "generated"

sys.path.insert(0, str(_SCRIPT_DIR))
from warlock_constants import HERO_SPEC_ELIGIBILITY


def _name_for_entry(entry: dict) -> str:
    """Get the best human-readable name for an entry."""
    return entry.get("overrideName") or entry.get("spellName") or f"Spell_{entry.get('spellId', 0)}"


def _classify_priority(node: dict, deps_child_count: int) -> str:
    """Assign initial A/B/C priority tier.

    Heuristic:
    - A (Tier A): Core rotational, resource generators/spenders, key procs,
      first rows of spec trees, hero keystone abilities
    - B (Tier B): Important passives, modifiers, secondary procs
    - C (Tier C): Utility, niche, final-row capstones that are rarely taken

    This is a HEURISTIC. classify_warlock_handlers.py may override after
    cross-referencing known rotational importance.
    """
    tree = node["tree"]
    entries = node["entries"]
    max_ranks = max((e.get("maxRanks", 1) for e in entries), default=1)

    # Entry spell IDs for known rotational spells
    spell_ids = {e.get("spellId", 0) for e in entries}

    # Known Tier A rotational spells (by spell ID ranges / known IDs)
    # These are core abilities that MUST work for the class to be playable
    KNOWN_TIER_A_SPELLS = {
        # Class tree essentials
        686,       # Shadow Bolt
        172,       # Corruption
        980,       # Agony (Affliction)
        348,       # Immolate (Destruction)
        29722,     # Incinerate
        116858,    # Chaos Bolt
        17962,     # Conflagrate
        5782,      # Fear
        104773,    # Unending Resolve
        108370,    # Soul Leech
        # Affliction
        30108,     # Unstable Affliction
        198590,    # Drain Soul
        234153,    # Drain Life
        27243,     # Seed of Corruption
        316099,    # Unstable Affliction (ranked)
        # Demonology
        105174,    # Hand of Gul'dan
        104316,    # Call Dreadstalkers
        264178,    # Demonbolt
        265187,    # Summon Demonic Tyrant
        267171,    # Demonic Strength
        267211,    # Bilescourge Bombers
        # Destruction
        80240,     # Havoc
        196447,    # Channel Demonfire
        5740,      # Rain of Fire
        17877,     # Shadowburn
        196412,    # Eradication
    }

    if spell_ids & KNOWN_TIER_A_SPELLS:
        return "A"

    # Hero tree nodes: hero keystones (first in subtree) are A, rest B/C
    if tree.startswith("hero_"):
        # Nodes with many children are more important
        if deps_child_count >= 2:
            return "A"
        if max_ranks > 1:
            return "B"
        return "B"

    # Spec tree: lower PosY (top of tree) = more fundamental
    pos_y = node.get("posY", 0)
    if tree in ("affliction", "demonology", "destruction"):
        if pos_y <= 400:
            return "A"
        elif pos_y <= 700:
            return "B"
        else:
            return "B"

    # Class tree
    if tree == "class":
        if pos_y <= 300:
            return "A"
        elif pos_y <= 600:
            return "B"
        else:
            return "C"

    return "B"


def build_registry(raw_data: dict) -> dict:
    """Build the canonical warlock registry from raw extraction data."""
    nodes = raw_data["nodes"]
    edges = raw_data["edges"]
    spell_lookup = raw_data["spellLookup"]
    hero_subtrees = raw_data["heroSubtrees"]

    # Build dependency graph
    # Edge semantics: parent (LeftTraitNodeID) -> child (RightTraitNodeID)
    # Child requires parent to be unlocked
    parents_of = defaultdict(list)  # node_id -> [parent_node_ids]
    children_of = defaultdict(list)  # node_id -> [child_node_ids]
    for edge in edges:
        parent = edge["parentNodeId"]
        child = edge["childNodeId"]
        parents_of[child].append(parent)
        children_of[parent].append(child)

    # Build registry entries
    registry_entries = []

    for node in nodes:
        nid = node["nodeId"]
        entries = node["entries"]

        # Primary entry is index 0 (or the only entry)
        primary = entries[0] if entries else {}
        primary_spell_id = primary.get("spellId", 0)
        primary_name = _name_for_entry(primary)

        # For choice nodes, collect alternatives
        choice_options = None
        if node["isChoice"] and len(entries) > 1:
            choice_options = []
            for e in entries:
                choice_options.append({
                    "entryId": e["entryId"],
                    "spellId": e.get("spellId", 0),
                    "name": _name_for_entry(e),
                    "maxRanks": e.get("maxRanks", 1),
                    "overridesSpellId": e.get("overridesSpellId", 0),
                })

        # All spell IDs referenced by this node
        all_spell_ids = set()
        for e in entries:
            for key in ["spellId", "overridesSpellId", "visibleSpellId"]:
                sid = e.get(key, 0)
                if sid:
                    all_spell_ids.add(sid)

        # Dependency info
        parent_nodes = sorted(set(parents_of.get(nid, [])))
        child_nodes = sorted(set(children_of.get(nid, [])))

        # Priority tier
        tier = _classify_priority(node, len(child_nodes))

        entry = {
            "nodeId": nid,
            "name": primary_name,
            "tree": node["tree"],
            "posX": node["posX"],
            "posY": node["posY"],
            "isChoice": node["isChoice"],
            "spellId": primary_spell_id,
            "maxRanks": primary.get("maxRanks", 1),
            "allSpellIds": sorted(all_spell_ids),
            "overridesSpellId": primary.get("overridesSpellId", 0),
            "specGates": node.get("specGates", []),
            "heroTree": node.get("heroTree"),
            "choiceOptions": choice_options,
            "parentNodeIds": parent_nodes,
            "childNodeIds": child_nodes,
            "priorityTier": tier,
            # Filled by classify_warlock_handlers.py:
            "handlerStatus": "UNCLASSIFIED",
            "implementationType": None,
            "scriptClass": None,
            "scriptFile": None,
            "scriptLine": None,
        }

        registry_entries.append(entry)

    # Sort by tree, then posY, then posX
    tree_order = {
        "class": 0,
        "affliction": 1, "demonology": 2, "destruction": 3,
        "hero_diabolist": 4, "hero_hellcaller": 5, "hero_soul_harvester": 6,
    }
    registry_entries.sort(key=lambda e: (
        tree_order.get(e["tree"], 99),
        e["posY"],
        e["posX"],
    ))

    # Summary stats
    tier_counts = defaultdict(int)
    tree_counts = defaultdict(int)
    for e in registry_entries:
        tier_counts[e["priorityTier"]] += 1
        tree_counts[e["tree"]] += 1

    choice_count = sum(1 for e in registry_entries if e["isChoice"])

    return {
        "metadata": {
            "version": "1.0",
            "source": "build_warlock_registry.py",
            "rawExtractionFile": "warlock_raw_extraction.json",
            "totalNodes": len(registry_entries),
            "choiceNodes": choice_count,
            "tierDistribution": dict(tier_counts),
            "treeDistribution": dict(tree_counts),
        },
        "heroSubtrees": hero_subtrees,
        "spellLookup": spell_lookup,
        "entries": registry_entries,
    }


def write_csv(registry: dict, csv_path: Path):
    """Write a CSV summary of the registry for quick reference."""
    entries = registry["entries"]
    csv_path.parent.mkdir(parents=True, exist_ok=True)

    with open(csv_path, "w", encoding="utf-8", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([
            "nodeId", "name", "tree", "spellId", "maxRanks", "isChoice",
            "tier", "handlerStatus", "posX", "posY",
        ])
        for e in entries:
            writer.writerow([
                e["nodeId"], e["name"], e["tree"], e["spellId"],
                e["maxRanks"], e["isChoice"], e["priorityTier"],
                e["handlerStatus"], e["posX"], e["posY"],
            ])

    print(f"[registry] Wrote CSV: {csv_path}")


def main():
    parser = argparse.ArgumentParser(
        description="Build canonical warlock_registry.json from raw extraction"
    )
    parser.add_argument(
        "--input", "-i",
        default=str(_GENERATED_DIR / "warlock_raw_extraction.json"),
        help="Input raw extraction JSON",
    )
    parser.add_argument(
        "--output", "-o",
        default=str(_GENERATED_DIR / "warlock_registry.json"),
        help="Output registry JSON",
    )
    parser.add_argument(
        "--csv",
        default=str(_GENERATED_DIR / "warlock_registry.csv"),
        help="Output CSV summary",
    )
    args = parser.parse_args()

    # Load raw extraction
    with open(args.input, "r", encoding="utf-8") as f:
        raw_data = json.load(f)

    print(f"[registry] Loaded raw extraction: {raw_data['metadata']['totalNodes']} nodes")

    registry = build_registry(raw_data)

    # Write JSON
    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(registry, f, indent=2, ensure_ascii=False)

    print(f"[registry] Wrote JSON: {out_path} ({out_path.stat().st_size:,} bytes)")

    # Write CSV
    write_csv(registry, Path(args.csv))

    # Summary
    meta = registry["metadata"]
    print(f"\n[registry] Summary:")
    print(f"  Total nodes: {meta['totalNodes']}")
    print(f"  Choice nodes: {meta['choiceNodes']}")
    print(f"  Tier distribution: {meta['tierDistribution']}")
    print(f"  Tree distribution: {meta['treeDistribution']}")


if __name__ == "__main__":
    main()
