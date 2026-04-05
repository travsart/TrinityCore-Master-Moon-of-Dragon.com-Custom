#!/usr/bin/env python3
"""Categorize non-Table-prefix errors from DBErrors.log."""
import re
from collections import Counter

LOG = r"out/build/x64-RelWithDebInfo/bin/RelWithDebInfo/DBErrors.log"
cats = Counter()
samples = {}

with open(LOG, "r", encoding="utf-8", errors="replace") as f:
    for line in f:
        if line.startswith("Table "):
            continue

        if "SmartAI" in line or "SmartScript" in line:
            if "has non-registered" in line:
                cat = "smartai_unregistered_AIName"
            elif "not found in any source" in line or "GUID" in line and "not found" in line:
                cat = "smartai_guid_not_found"
            elif "entry" in line and "not found" in line:
                cat = "smartai_entry_not_found"
            elif "does not exist" in line:
                cat = "smartai_entry_missing"
            elif "link" in line.lower():
                cat = "smartai_link_error"
            elif "action_type" in line:
                cat = "smartai_invalid_action"
            elif "event_type" in line:
                cat = "smartai_event_error"
            elif "target_type" in line:
                cat = "smartai_target_error"
            else:
                cat = "smartai_misc"
        elif "disallowed" in line and "flag" in line:
            cat = "flags_disallowed"
        elif "difficulty" in line and "not supported" in line:
            if "Creature" in line:
                cat = "creature_difficulty_unsupported"
            elif "areatrigger" in line.lower():
                cat = "areatrigger_difficulty_unsupported"
            elif "gameobject" in line.lower() or "Gameobject" in line:
                cat = "gameobject_difficulty_unsupported"
            else:
                cat = "difficulty_unsupported_other"
        elif "does not exist" in line:
            cat = "other_does_not_exist"
        elif "not found" in line:
            cat = "other_not_found"
        else:
            cat = "other_misc"

        cats[cat] += 1
        if cat not in samples:
            samples[cat] = line.strip()[:200]

total = sum(cats.values())
print(f"Total non-Table lines: {total:,}\n")
print(f"{'Category':<45} {'Count':>10} {'%':>6}")
print("-" * 65)
for k, c in cats.most_common():
    pct = c / total * 100
    print(f"{k:<45} {c:>10,} {pct:>5.1f}%")

print(f"\n{'='*65}")
print("Samples:")
for k, c in cats.most_common(10):
    print(f"\n[{k}] ({c:,}):")
    print(f"  {samples[k]}")
