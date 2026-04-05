#!/usr/bin/env python3
"""
extract_warlock_db2.py -- Extract all Warlock talent/spell data from Wago DB2 CSVs.

Reads authoritative Warlock talent tree, spell, effect, and dependency data from
the merged Wago CSV directory. Outputs raw extraction JSON used by downstream
pipeline scripts (build_warlock_registry.py, classify_warlock_handlers.py, etc.).

Usage:
    python tools/warlock/extract_warlock_db2.py [--output path/to/output.json]

Requires: wago_common.py on the Python path (auto-added from ../wago/).
"""

from __future__ import annotations

import argparse
import json
import sys
from collections import defaultdict
from pathlib import Path

# Add wago/ to path for wago_common imports
_SCRIPT_DIR = Path(__file__).resolve().parent
_REPO_ROOT = _SCRIPT_DIR.parent.parent
sys.path.insert(0, str(_REPO_ROOT / "wago"))

from wago_common import load_wago_csv, load_wago_csv_by_key, WAGO_CSV_DIR
from warlock_constants import (
    TRAIT_TREE_WARLOCK,
    SPECSET_TO_SPEC,
    SUBTREE_TO_HERO,
    HERO_SPEC_ELIGIBILITY,
)


def _int(val, default=0):
    """Safe int conversion."""
    try:
        return int(val)
    except (TypeError, ValueError):
        return default


def extract_warlock_nodes() -> dict:
    """Extract all warlock talent nodes with entries, definitions, and spell names."""
    print(f"[extract] Loading CSVs from {WAGO_CSV_DIR}")

    # Load all required tables
    trait_nodes = load_wago_csv("TraitNode")
    trait_node_x_entry = load_wago_csv_by_key("TraitNodeXTraitNodeEntry", "TraitNodeID")
    trait_node_entries = load_wago_csv("TraitNodeEntry")
    trait_definitions = load_wago_csv("TraitDefinition")
    spell_names = load_wago_csv("SpellName")
    trait_edges_raw = load_wago_csv("TraitEdge")
    trait_subtrees = load_wago_csv("TraitSubTree")
    trait_node_groups = load_wago_csv("TraitNodeGroup")
    trait_node_group_x_node = load_wago_csv_by_key("TraitNodeGroupXTraitNode", "TraitNodeGroupID")
    trait_node_group_x_cond = load_wago_csv_by_key("TraitNodeGroupXTraitCond", "TraitNodeGroupID")
    trait_conds = load_wago_csv("TraitCond")
    trait_def_effect_points = load_wago_csv_by_key("TraitDefinitionEffectPoints", "TraitDefinitionID")

    # 1. Filter TraitNodes to warlock tree (720)
    warlock_node_ids = set()
    warlock_nodes_raw = {}
    for nid, node in trait_nodes.items():
        if _int(node.get("TraitTreeID")) == TRAIT_TREE_WARLOCK:
            warlock_node_ids.add(nid)
            warlock_nodes_raw[nid] = node

    print(f"[extract] Found {len(warlock_node_ids)} warlock trait nodes")

    # 2. Build spec-gating map: node_id -> set of spec names
    #    Chain: TraitNodeGroup -> TraitNodeGroupXTraitCond -> TraitCond(SpecSetID)
    #           TraitNodeGroup -> TraitNodeGroupXTraitNode -> TraitNode
    node_spec_gate: dict[int, set] = defaultdict(set)

    for gid, group in trait_node_groups.items():
        if _int(group.get("TraitTreeID")) != TRAIT_TREE_WARLOCK:
            continue

        # Get spec conditions for this group
        group_conds = trait_node_group_x_cond.get(gid, [])
        spec_names = set()
        for gc in group_conds:
            cond_id = _int(gc.get("TraitCondID"))
            cond = trait_conds.get(cond_id)
            if cond:
                spec_set_id = _int(cond.get("SpecSetID"))
                if spec_set_id in SPECSET_TO_SPEC:
                    spec_names.add(SPECSET_TO_SPEC[spec_set_id])

        if not spec_names:
            continue

        # Get nodes in this group
        group_nodes = trait_node_group_x_node.get(gid, [])
        for gn in group_nodes:
            nid = _int(gn.get("TraitNodeID"))
            if nid in warlock_node_ids:
                node_spec_gate[nid].update(spec_names)

    # 3. Build edges (dependency graph)
    edges = []
    for eid, edge in trait_edges_raw.items():
        left = _int(edge.get("LeftTraitNodeID"))
        right = _int(edge.get("RightTraitNodeID"))
        if left in warlock_node_ids and right in warlock_node_ids:
            edges.append({
                "edgeId": eid,
                "parentNodeId": left,
                "childNodeId": right,
                "type": _int(edge.get("Type")),
                "visualStyle": _int(edge.get("VisualStyle")),
            })

    print(f"[extract] Found {len(edges)} warlock trait edges")

    # 4. Build full node records with entries and definitions
    nodes = []
    spell_id_set = set()

    for nid in sorted(warlock_node_ids):
        raw = warlock_nodes_raw[nid]
        subtree_id = _int(raw.get("TraitSubTreeID"))

        # Resolve entries for this node
        entry_links = trait_node_x_entry.get(nid, [])
        entry_links_sorted = sorted(entry_links, key=lambda x: _int(x.get("Index", 0)))

        entries = []
        for link in entry_links_sorted:
            entry_id = _int(link.get("TraitNodeEntryID"))
            entry = trait_node_entries.get(entry_id)
            if not entry:
                continue

            def_id = _int(entry.get("TraitDefinitionID"))
            definition = trait_definitions.get(def_id, {})

            spell_id = _int(definition.get("SpellID"))
            overrides_spell_id = _int(definition.get("OverridesSpellID"))
            visible_spell_id = _int(definition.get("VisibleSpellID"))

            # Resolve spell name
            spell_name = None
            if spell_id and spell_id in spell_names:
                spell_name = spell_names[spell_id].get("Name_lang")

            override_name = definition.get("OverrideName_lang")
            if override_name and not override_name.strip():
                override_name = None

            # Effect points (rank scaling)
            effect_points = []
            for ep in trait_def_effect_points.get(def_id, []):
                effect_points.append({
                    "effectPointsId": _int(ep.get("ID")),
                    "effectIndex": _int(ep.get("EffectIndex")),
                    "operationType": _int(ep.get("OperationType")),
                    "curveID": _int(ep.get("CurveID")),
                })

            if spell_id:
                spell_id_set.add(spell_id)
            if overrides_spell_id:
                spell_id_set.add(overrides_spell_id)
            if visible_spell_id:
                spell_id_set.add(visible_spell_id)

            entries.append({
                "entryId": entry_id,
                "definitionId": def_id,
                "maxRanks": _int(entry.get("MaxRanks")),
                "nodeEntryType": _int(entry.get("NodeEntryType")),
                "entrySubTreeId": _int(entry.get("TraitSubTreeID")),
                "spellId": spell_id,
                "spellName": spell_name,
                "overrideName": override_name,
                "overridesSpellId": overrides_spell_id,
                "visibleSpellId": visible_spell_id,
                "effectPoints": effect_points if effect_points else None,
                "index": _int(link.get("Index", 0)),
            })

        # Determine tree ownership
        spec_gates = sorted(node_spec_gate.get(nid, []))
        hero_tree = SUBTREE_TO_HERO.get(subtree_id)

        # Classify: hero > spec-gated > class
        if hero_tree:
            tree = hero_tree
        elif len(spec_gates) == 1:
            tree = spec_gates[0]
        elif len(spec_gates) > 1:
            # Node gated to multiple specs -- shared among those specs
            # If gated to all 3, it's effectively class tree
            if set(spec_gates) == {"affliction", "demonology", "destruction"}:
                tree = "class"
            else:
                tree = "shared_" + "_".join(spec_gates)
        else:
            tree = "class"

        is_choice = len(entries) > 1

        # Skip placeholder nodes where ALL entries have spellId=0
        if all(e.get("spellId", 0) == 0 for e in entries):
            continue

        nodes.append({
            "nodeId": nid,
            "posX": _int(raw.get("PosX")),
            "posY": _int(raw.get("PosY")),
            "nodeType": _int(raw.get("Type")),
            "nodeFlags": _int(raw.get("Flags")),
            "subTreeId": subtree_id,
            "tree": tree,
            "specGates": spec_gates,
            "heroTree": hero_tree,
            "isChoice": is_choice,
            "entries": entries,
        })

    print(f"[extract] Built {len(nodes)} node records with {len(spell_id_set)} unique spells")

    # 5. Collect hero subtree metadata
    hero_subtrees = []
    for sid, st in trait_subtrees.items():
        if _int(st.get("TraitTreeID")) == TRAIT_TREE_WARLOCK:
            hero_subtrees.append({
                "subTreeId": sid,
                "name": st.get("Name_lang"),
                "description": st.get("Description_lang"),
                "key": SUBTREE_TO_HERO.get(sid, f"unknown_{sid}"),
                "eligibleSpecs": HERO_SPEC_ELIGIBILITY.get(
                    SUBTREE_TO_HERO.get(sid, ""), []
                ),
            })

    # 6. Build spell lookup for all referenced spells
    all_spell_ids = set()
    for n in nodes:
        for e in n["entries"]:
            for sid in [e["spellId"], e["overridesSpellId"], e["visibleSpellId"]]:
                if sid:
                    all_spell_ids.add(sid)

    spell_lookup = {}
    for sid in sorted(all_spell_ids):
        sn = spell_names.get(sid)
        if sn:
            spell_lookup[sid] = sn.get("Name_lang", f"Unknown_{sid}")
        else:
            spell_lookup[sid] = f"Unknown_{sid}"

    # 7. Summary stats
    tree_counts = defaultdict(int)
    for n in nodes:
        tree_counts[n["tree"]] += 1

    print(f"[extract] Tree distribution:")
    for t, c in sorted(tree_counts.items()):
        print(f"  {t}: {c} nodes")

    return {
        "metadata": {
            "source": "Wago DB2 CSVs",
            "csvDir": str(WAGO_CSV_DIR),
            "traitTreeId": TRAIT_TREE_WARLOCK,
            "totalNodes": len(nodes),
            "totalEdges": len(edges),
            "totalUniqueSpells": len(all_spell_ids),
            "heroSubtrees": len(hero_subtrees),
        },
        "nodes": nodes,
        "edges": edges,
        "heroSubtrees": hero_subtrees,
        "spellLookup": spell_lookup,
    }


def main():
    parser = argparse.ArgumentParser(
        description="Extract Warlock talent/spell data from Wago DB2 CSVs"
    )
    parser.add_argument(
        "--output", "-o",
        default=str(_REPO_ROOT / "doc" / "classes" / "warlock" / "generated" / "warlock_raw_extraction.json"),
        help="Output JSON file path",
    )
    args = parser.parse_args()

    data = extract_warlock_nodes()

    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)

    print(f"\n[extract] Wrote {out_path} ({out_path.stat().st_size:,} bytes)")
    print(f"[extract] Summary: {data['metadata']['totalNodes']} nodes, "
          f"{data['metadata']['totalEdges']} edges, "
          f"{data['metadata']['totalUniqueSpells']} spells")


if __name__ == "__main__":
    main()
