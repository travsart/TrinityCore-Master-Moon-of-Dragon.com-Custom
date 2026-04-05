#!/usr/bin/env python3
"""
generate_warlock_spec_docs.py -- Generate 4 canonical Warlock spec markdown docs.

Reads the classified warlock_registry.json and outputs:
  - doc/classes/warlock/generated/class_tree.md
  - doc/classes/warlock/generated/affliction.md
  - doc/classes/warlock/generated/demonology.md
  - doc/classes/warlock/generated/destruction.md

Each doc includes:
  - Summary stats (nodes, handlers, tiers)
  - Node table sorted by position (top to bottom of talent tree)
  - Choice node details
  - Hero talent overlay sections (for spec docs)
  - Dependency graph info
  - Implementation status per node

Usage:
    python tools/warlock/generate_warlock_spec_docs.py [--registry path]
"""

from __future__ import annotations

import argparse
import json
from collections import defaultdict
from pathlib import Path

_SCRIPT_DIR = Path(__file__).resolve().parent
_REPO_ROOT = _SCRIPT_DIR.parent.parent
_GENERATED_DIR = _REPO_ROOT / "doc" / "classes" / "warlock" / "generated"

# Hero trees and which spec docs they appear in
HERO_TREES_FOR_SPEC = {
    "affliction": ["hero_hellcaller", "hero_soul_harvester"],
    "demonology": ["hero_diabolist", "hero_soul_harvester"],
    "destruction": ["hero_diabolist", "hero_hellcaller"],
}

HERO_TREE_NAMES = {
    "hero_diabolist": "Diabolist",
    "hero_hellcaller": "Hellcaller",
    "hero_soul_harvester": "Soul Harvester",
}

TREE_DISPLAY_NAMES = {
    "class": "Warlock Class Tree",
    "affliction": "Affliction",
    "demonology": "Demonology",
    "destruction": "Destruction",
}

STATUS_EMOJI = {
    "HAS_HANDLER": "[OK]",
    "DISABLED": "[OFF]",
    "NEEDS_HANDLER": "[TODO]",
    "DB_ONLY": "[DB]",
    "NATIVE": "[TC]",
    "STUB_ONLY": "[STUB]",
    "UNCLASSIFIED": "[?]",
}


def _status_tag(status: str) -> str:
    return STATUS_EMOJI.get(status, "[?]")


def _tier_tag(tier: str) -> str:
    return f"**{tier}**" if tier == "A" else tier


def generate_node_table(entries: list[dict], spell_lookup: dict) -> str:
    """Generate a markdown table of talent nodes."""
    lines = []
    lines.append("| # | Node | Spell ID | Ranks | Tier | Status | Handler |")
    lines.append("|---|------|----------|-------|------|--------|---------|")

    for i, e in enumerate(entries, 1):
        name = e["name"]
        spell_id = e["spellId"]
        max_ranks = e["maxRanks"]
        tier = _tier_tag(e["priorityTier"])
        status = _status_tag(e["handlerStatus"])
        handler = e.get("scriptClass") or "-"

        choice_marker = " (choice)" if e["isChoice"] else ""
        lines.append(
            f"| {i} | {name}{choice_marker} | {spell_id} | {max_ranks} | {tier} | {status} | {handler} |"
        )

    return "\n".join(lines)


def generate_choice_details(entries: list[dict]) -> str:
    """Generate details section for choice nodes."""
    choice_nodes = [e for e in entries if e["isChoice"]]
    if not choice_nodes:
        return ""

    lines = ["", "### Choice Nodes", ""]
    for e in choice_nodes:
        lines.append(f"**{e['name']}** (Node {e['nodeId']})")
        for opt in (e.get("choiceOptions") or []):
            opt_status = _status_tag(opt.get("handlerStatus", "UNCLASSIFIED"))
            opt_handler = opt.get("scriptClass", "-")
            lines.append(
                f"  - Option: **{opt['name']}** (spell {opt['spellId']}, "
                f"ranks {opt['maxRanks']}) {opt_status} {opt_handler}"
            )
        lines.append("")

    return "\n".join(lines)


def generate_dep_section(entries: list[dict]) -> str:
    """Generate dependency info section."""
    # Build node ID -> name map
    id_to_name = {e["nodeId"]: e["name"] for e in entries}

    lines = ["", "### Dependencies", ""]
    lines.append("Nodes with prerequisites (must unlock parent before child):", )
    lines.append("")

    has_deps = False
    for e in entries:
        parents = e.get("parentNodeIds", [])
        if parents:
            parent_names = [id_to_name.get(p, f"Node {p}") for p in parents]
            lines.append(f"- **{e['name']}** requires: {', '.join(parent_names)}")
            has_deps = True

    if not has_deps:
        lines.append("*(No dependency data for this tree section)*")

    return "\n".join(lines)


def generate_summary_stats(entries: list[dict], tree_name: str) -> str:
    """Generate summary statistics block."""
    total = len(entries)
    tier_counts = defaultdict(int)
    status_counts = defaultdict(int)
    for e in entries:
        tier_counts[e["priorityTier"]] += 1
        status_counts[e["handlerStatus"]] += 1

    choice_count = sum(1 for e in entries if e["isChoice"])

    lines = [
        f"**Total Nodes**: {total}",
        f"**Choice Nodes**: {choice_count}",
        f"**Tier A**: {tier_counts.get('A', 0)} | **Tier B**: {tier_counts.get('B', 0)} | **Tier C**: {tier_counts.get('C', 0)}",
        "",
        "**Handler Status**:",
        f"- Has Handler: {status_counts.get('HAS_HANDLER', 0)}",
        f"- Needs Handler: {status_counts.get('NEEDS_HANDLER', 0)}",
        f"- Disabled: {status_counts.get('DISABLED', 0)}",
        f"- DB Only: {status_counts.get('DB_ONLY', 0)}",
    ]
    return "\n".join(lines)


def generate_tree_doc(
    tree_key: str,
    tree_entries: list[dict],
    spell_lookup: dict,
    hero_entries: dict[str, list[dict]] | None = None,
    all_entries: list[dict] | None = None,
) -> str:
    """Generate a full markdown document for a talent tree/spec."""
    display_name = TREE_DISPLAY_NAMES.get(tree_key, tree_key.title())
    is_class_tree = tree_key == "class"

    lines = [
        f"# Warlock {display_name} -- Talent Spec Document",
        "",
        f"> Auto-generated by `generate_warlock_spec_docs.py` from `warlock_registry.json`.",
        f"> Source: Wago DB2 CSVs (build 66709). Do not edit manually.",
        "",
        "## Summary",
        "",
        generate_summary_stats(tree_entries, tree_key),
        "",
    ]

    # Main talent table
    lines.extend([
        f"## {display_name} Talent Nodes",
        "",
        generate_node_table(tree_entries, spell_lookup),
        "",
        generate_choice_details(tree_entries),
    ])

    # Dependencies (use all_entries to resolve cross-tree deps)
    dep_entries = all_entries if all_entries else tree_entries
    # Filter deps to only show this tree's nodes
    lines.append(generate_dep_section(tree_entries))

    # Hero talent overlays (for spec docs, not class tree)
    if hero_entries and not is_class_tree:
        for hero_key in HERO_TREES_FOR_SPEC.get(tree_key, []):
            hero_nodes = hero_entries.get(hero_key, [])
            if not hero_nodes:
                continue

            hero_name = HERO_TREE_NAMES.get(hero_key, hero_key)
            lines.extend([
                "",
                f"## Hero Talent Overlay: {hero_name}",
                "",
                generate_summary_stats(hero_nodes, hero_key),
                "",
                generate_node_table(hero_nodes, spell_lookup),
                "",
                generate_choice_details(hero_nodes),
            ])

    # Implementation notes
    lines.extend([
        "",
        "## Implementation Notes",
        "",
        "### Status Legend",
        "- `[OK]` -- Handler exists and is active in `spell_warlock.cpp`",
        "- `[OFF]` -- Handler exists but is disabled (`#if 0`)",
        "- `[TODO]` -- No handler found, needs implementation",
        "- `[DB]` -- Can be handled through DB data only (proc flags, aura effects)",
        "- `[TC]` -- TrinityCore handles natively",
        "- `[STUB]` -- Handler exists but is incomplete",
        "",
        "### Priority Tiers",
        "- **A** -- Core rotational abilities, resource generators/spenders. Must work for class to be playable.",
        "- **B** -- Important passives, talent modifiers, secondary procs.",
        "- **C** -- Utility, niche talents, cosmetic-only effects.",
        "",
    ])

    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(
        description="Generate 4 canonical Warlock spec markdown docs"
    )
    parser.add_argument(
        "--registry", "-r",
        default=str(_GENERATED_DIR / "warlock_registry.json"),
        help="Input classified registry JSON",
    )
    parser.add_argument(
        "--output-dir", "-o",
        default=str(_GENERATED_DIR),
        help="Output directory for markdown files",
    )
    args = parser.parse_args()

    with open(args.registry, "r", encoding="utf-8") as f:
        registry = json.load(f)

    entries = registry["entries"]
    spell_lookup = registry.get("spellLookup", {})
    out_dir = Path(args.output_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    print(f"[gendocs] Loaded registry: {len(entries)} entries")

    # Group entries by tree
    tree_groups: dict[str, list[dict]] = defaultdict(list)
    for e in entries:
        tree_groups[e["tree"]].append(e)

    # Build hero entries map
    hero_entries = {
        k: v for k, v in tree_groups.items() if k.startswith("hero_")
    }

    # Generate class tree doc
    doc = generate_tree_doc("class", tree_groups["class"], spell_lookup, all_entries=entries)
    path = out_dir / "class_tree.md"
    path.write_text(doc, encoding="utf-8")
    print(f"[gendocs] Wrote {path.name} ({len(tree_groups['class'])} nodes)")

    # Generate spec docs
    for spec_key in ["affliction", "demonology", "destruction"]:
        spec_entries = tree_groups.get(spec_key, [])
        doc = generate_tree_doc(
            spec_key, spec_entries, spell_lookup,
            hero_entries=hero_entries, all_entries=entries,
        )
        path = out_dir / f"{spec_key}.md"
        path.write_text(doc, encoding="utf-8")
        print(f"[gendocs] Wrote {path.name} ({len(spec_entries)} nodes + hero overlays)")

    # Write final implementation status JSON
    impl_status = {
        "summary": {
            "total": len(entries),
            "hasHandler": sum(1 for e in entries if e["handlerStatus"] == "HAS_HANDLER"),
            "disabled": sum(1 for e in entries if e["handlerStatus"] == "DISABLED"),
            "needsHandler": sum(1 for e in entries if e["handlerStatus"] == "NEEDS_HANDLER"),
            "dbOnly": sum(1 for e in entries if e["handlerStatus"] == "DB_ONLY"),
        },
        "byTree": {},
        "tierAMissing": [],
    }

    for tree_key in sorted(tree_groups.keys()):
        te = tree_groups[tree_key]
        impl_status["byTree"][tree_key] = {
            "total": len(te),
            "hasHandler": sum(1 for e in te if e["handlerStatus"] == "HAS_HANDLER"),
            "needsHandler": sum(1 for e in te if e["handlerStatus"] == "NEEDS_HANDLER"),
        }

    # Flag missing Tier A
    for e in entries:
        if e["priorityTier"] == "A" and e["handlerStatus"] in ("NEEDS_HANDLER", "UNCLASSIFIED"):
            impl_status["tierAMissing"].append({
                "nodeId": e["nodeId"],
                "name": e["name"],
                "tree": e["tree"],
                "spellId": e["spellId"],
            })

    status_path = out_dir / "warlock_implementation_status.json"
    with open(status_path, "w", encoding="utf-8") as f:
        json.dump(impl_status, f, indent=2, ensure_ascii=False)
    print(f"[gendocs] Wrote {status_path.name}")

    # Write dependency graph
    dep_graph = {"nodes": [], "edges": []}
    for e in entries:
        dep_graph["nodes"].append({
            "id": e["nodeId"],
            "name": e["name"],
            "tree": e["tree"],
        })
        for child in e.get("childNodeIds", []):
            dep_graph["edges"].append({
                "parent": e["nodeId"],
                "child": child,
            })

    dep_path = out_dir / "warlock_spell_dependencies.json"
    with open(dep_path, "w", encoding="utf-8") as f:
        json.dump(dep_graph, f, indent=2, ensure_ascii=False)
    print(f"[gendocs] Wrote {dep_path.name}")

    print(f"\n[gendocs] Done! Generated 4 spec docs + 2 status files in {out_dir}")


if __name__ == "__main__":
    main()
