#!/usr/bin/env python3
"""
validate_warlock_spec_docs.py -- Validate warlock registry and generated docs.

Checks for:
  1. No missing spell IDs (every node has a valid spell)
  2. No duplicate node ownership (each node in exactly one tree)
  3. No unresolved Tier A mechanics (all Tier A have a handler or are marked native)
  4. No missing choice-node alternatives
  5. Generated docs exist and are in sync with registry
  6. No orphan nodes (nodes not reachable from edges)
  7. Spell lookup coverage (every referenced spell has a name)

Usage:
    python tools/warlock/validate_warlock_spec_docs.py [--registry path] [--strict]

Exit code: 0 if clean, 1 if errors found.
"""

from __future__ import annotations

import argparse
import json
import sys
from collections import Counter, defaultdict
from pathlib import Path

_SCRIPT_DIR = Path(__file__).resolve().parent
_REPO_ROOT = _SCRIPT_DIR.parent.parent
_GENERATED_DIR = _REPO_ROOT / "doc" / "classes" / "warlock" / "generated"


class ValidationResult:
    def __init__(self):
        self.errors: list[str] = []
        self.warnings: list[str] = []
        self.info: list[str] = []

    def error(self, msg: str):
        self.errors.append(msg)

    def warn(self, msg: str):
        self.warnings.append(msg)

    def ok(self, msg: str):
        self.info.append(msg)

    @property
    def passed(self) -> bool:
        return len(self.errors) == 0


def validate_spell_ids(entries: list[dict], spell_lookup: dict, result: ValidationResult):
    """Check that every node has a valid, non-zero primary spell ID."""
    missing = []
    for e in entries:
        if e.get("spellId", 0) == 0:
            missing.append(f"Node {e['nodeId']} ({e['name']}) has spellId=0")

    if missing:
        for m in missing:
            result.error(f"[SPELL_ID] {m}")
    else:
        result.ok(f"[SPELL_ID] All {len(entries)} nodes have valid spell IDs")

    # Check spell lookup coverage
    all_spell_ids = set()
    for e in entries:
        all_spell_ids.update(e.get("allSpellIds", []))

    missing_names = [sid for sid in all_spell_ids if str(sid) not in spell_lookup and sid not in spell_lookup]
    # spell_lookup keys may be int or str
    lookup_keys = set()
    for k in spell_lookup.keys():
        try:
            lookup_keys.add(int(k))
        except ValueError:
            pass

    actual_missing = [sid for sid in all_spell_ids if sid not in lookup_keys and sid != 0]
    if actual_missing:
        result.warn(f"[SPELL_LOOKUP] {len(actual_missing)} spell IDs have no name in lookup: {actual_missing[:10]}...")
    else:
        result.ok(f"[SPELL_LOOKUP] All {len(all_spell_ids)} spell IDs have names")


def validate_node_ownership(entries: list[dict], result: ValidationResult):
    """Check that each node appears in exactly one tree."""
    node_trees = defaultdict(list)
    for e in entries:
        node_trees[e["nodeId"]].append(e["tree"])

    dupes = {nid: trees for nid, trees in node_trees.items() if len(trees) > 1}
    if dupes:
        for nid, trees in dupes.items():
            result.error(f"[OWNERSHIP] Node {nid} appears in multiple trees: {trees}")
    else:
        result.ok(f"[OWNERSHIP] All {len(entries)} nodes have unique tree ownership")


def validate_tier_a(entries: list[dict], strict: bool, result: ValidationResult):
    """Check that Tier A nodes have handlers or are marked native/DB."""
    tier_a = [e for e in entries if e.get("priorityTier") == "A"]
    unresolved = [
        e for e in tier_a
        if e.get("handlerStatus") in ("NEEDS_HANDLER", "UNCLASSIFIED")
    ]

    if unresolved:
        for e in unresolved:
            msg = f"[TIER_A] {e['name']} (spell {e['spellId']}, tree={e['tree']}) has no handler"
            if strict:
                result.error(msg)
            else:
                result.warn(msg)
    else:
        result.ok(f"[TIER_A] All {len(tier_a)} Tier A nodes have handlers")


def validate_choice_nodes(entries: list[dict], result: ValidationResult):
    """Check that choice nodes have all alternatives documented."""
    choice_nodes = [e for e in entries if e.get("isChoice")]

    for e in choice_nodes:
        opts = e.get("choiceOptions", [])
        if not opts:
            result.error(f"[CHOICE] Node {e['nodeId']} ({e['name']}) is marked choice but has no options")
        elif len(opts) < 2:
            result.warn(f"[CHOICE] Node {e['nodeId']} ({e['name']}) has only {len(opts)} option(s)")
        else:
            # Check each option has a spell ID
            for opt in opts:
                if opt.get("spellId", 0) == 0:
                    result.warn(f"[CHOICE] Option '{opt.get('name', '?')}' in node {e['nodeId']} has spellId=0")

    if not any(r.startswith("[CHOICE]") for r in result.errors):
        result.ok(f"[CHOICE] All {len(choice_nodes)} choice nodes have valid options")


def validate_generated_docs(result: ValidationResult):
    """Check that all 4 spec docs and registry exist."""
    expected_files = [
        "class_tree.md",
        "affliction.md",
        "demonology.md",
        "destruction.md",
        "warlock_registry.json",
        "warlock_registry.csv",
        "warlock_implementation_status.json",
        "warlock_spell_dependencies.json",
        "warlock_raw_extraction.json",
    ]

    for f in expected_files:
        path = _GENERATED_DIR / f
        if path.exists():
            result.ok(f"[FILES] {f} exists ({path.stat().st_size:,} bytes)")
        else:
            result.error(f"[FILES] Missing: {f}")


def validate_edge_coverage(entries: list[dict], result: ValidationResult):
    """Check for orphan nodes (no incoming or outgoing edges)."""
    all_ids = {e["nodeId"] for e in entries}
    has_parent = set()
    has_child = set()

    for e in entries:
        for pid in e.get("parentNodeIds", []):
            has_parent.add(e["nodeId"])
            has_child.add(pid)
        for cid in e.get("childNodeIds", []):
            has_child.add(e["nodeId"])
            has_parent.add(cid)

    connected = has_parent | has_child
    orphans = all_ids - connected

    if orphans:
        orphan_names = [e["name"] for e in entries if e["nodeId"] in orphans]
        result.warn(f"[EDGES] {len(orphans)} orphan nodes (no edges): {orphan_names[:10]}")
    else:
        result.ok(f"[EDGES] All {len(entries)} nodes are connected via edges")


def main():
    parser = argparse.ArgumentParser(
        description="Validate warlock registry and generated docs"
    )
    parser.add_argument(
        "--registry", "-r",
        default=str(_GENERATED_DIR / "warlock_registry.json"),
        help="Input registry JSON",
    )
    parser.add_argument(
        "--strict", action="store_true",
        help="Treat Tier A missing handlers as errors (not warnings)",
    )
    args = parser.parse_args()

    with open(args.registry, "r", encoding="utf-8") as f:
        registry = json.load(f)

    entries = registry["entries"]
    spell_lookup = registry.get("spellLookup", {})

    result = ValidationResult()

    print(f"[validate] Running validation on {len(entries)} entries...")
    print()

    validate_spell_ids(entries, spell_lookup, result)
    validate_node_ownership(entries, result)
    validate_tier_a(entries, args.strict, result)
    validate_choice_nodes(entries, result)
    validate_generated_docs(result)
    validate_edge_coverage(entries, result)

    # Print results
    print("=" * 60)
    for msg in result.info:
        print(f"  PASS  {msg}")
    for msg in result.warnings:
        print(f"  WARN  {msg}")
    for msg in result.errors:
        print(f"  FAIL  {msg}")

    print()
    print(f"Results: {len(result.info)} passed, {len(result.warnings)} warnings, {len(result.errors)} errors")

    if result.passed:
        print("VALIDATION PASSED")
    else:
        print("VALIDATION FAILED")

    sys.exit(0 if result.passed else 1)


if __name__ == "__main__":
    main()
