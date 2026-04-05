#!/usr/bin/env python3
"""Stream-parse DBErrors.log and categorize errors by type."""
import re
import sys
from collections import Counter

LOG_PATH = r"out/build/x64-RelWithDebInfo/bin/RelWithDebInfo/DBErrors.log"

categories = Counter()
samples = {}  # category -> first sample line

PATTERNS = [
    (re.compile(r"SmartAI.*GUID (-?\d+).*not found"), "smartai_guid_missing"),
    (re.compile(r"SmartAI.*entry (-?\d+).*not found"), "smartai_entry_missing"),
    (re.compile(r"SmartAI.*has a link to non-existent"), "smartai_broken_link"),
    (re.compile(r"SmartAI.*has linked.*event_type"), "smartai_link_event_type"),
    (re.compile(r"SmartAI.*SMART_EVENT_LINK"), "smartai_link_event"),
    (re.compile(r"SmartAI.*invalid action_type"), "smartai_invalid_action"),
    (re.compile(r"SmartAI.*Creature entry \((\d+)\).*does not exist"), "smartai_creature_missing"),
    (re.compile(r"SmartAI.*GameObject entry \((\d+)\).*does not exist"), "smartai_go_missing"),
    (re.compile(r"SmartAI"), "smartai_other"),
    (re.compile(r"Table '(creature_loot_template|gameobject_loot_template|reference_loot_template)'.*item (\d+).*doesn.*exist"), "loot_item_missing"),
    (re.compile(r"Loot.*creature.*entry (\d+).*not exist"), "loot_creature_missing"),
    (re.compile(r"creature_template_difficulty.*Entry (\d+).*difficulty.*not supported"), "difficulty_unsupported"),
    (re.compile(r"Creature \(Entry: (\d+)\).*difficulty (\d+).*not supported"), "creature_difficulty_unsupported"),
    (re.compile(r"areatrigger.*difficulty.*not supported"), "areatrigger_difficulty_unsupported"),
    (re.compile(r"gameobject.*difficulty.*not supported", re.I), "gameobject_difficulty_unsupported"),
    (re.compile(r"quest_template_addon.*quest (\d+) not in"), "quest_addon_orphan"),
    (re.compile(r"creature_template_difficulty.*Entry (\d+).*does not exist"), "template_difficulty_orphan"),
    (re.compile(r"creature_template_model.*Creature (\d+).*does not exist"), "template_model_orphan"),
    (re.compile(r"creature_template_addon.*Creature (\d+).*does not exist"), "template_addon_orphan"),
    (re.compile(r"creature_template_gossip.*Creature (\d+).*does not exist"), "template_gossip_orphan"),
    (re.compile(r"creature_template_resistance.*Creature (\d+).*does not exist"), "template_resistance_orphan"),
    (re.compile(r"creature_template_spell.*Creature (\d+).*does not exist"), "template_spell_orphan"),
    (re.compile(r"creature_template_movement.*Creature (\d+).*does not exist"), "template_movement_orphan"),
    (re.compile(r"creature_equip_template.*Creature (\d+).*does not exist"), "equip_template_orphan"),
    (re.compile(r"creature_addon.*GUID (\d+).*does not exist"), "creature_addon_guid_orphan"),
    (re.compile(r"creature_formations.*GUID.*does not exist"), "creature_formations_orphan"),
    (re.compile(r"waypoint.*GUID.*does not exist", re.I), "waypoint_orphan"),
    (re.compile(r"npc_vendor.*creature template (\d+) does not exist"), "npc_vendor_orphan"),
    (re.compile(r"disallowed.*flag", re.I), "flags_disallowed"),
    (re.compile(r"Faction.*does not exist"), "faction_missing"),
    (re.compile(r"spell (\d+).*does not exist"), "spell_missing"),
    (re.compile(r"Item.*does not exist"), "item_missing"),
    (re.compile(r"gossip_menu.*does not exist"), "gossip_orphan"),
    (re.compile(r"conditions.*source.*does not exist"), "conditions_orphan"),
    (re.compile(r"spawn_group_template"), "spawn_group_orphan"),
    (re.compile(r"creature_text.*has no entry"), "creature_text_orphan"),
    (re.compile(r"game_event_creature.*references.*not found"), "game_event_creature_orphan"),
    (re.compile(r"game_event_gameobject.*references.*not found"), "game_event_go_orphan"),
]

count = 0
with open(LOG_PATH, "r", encoding="utf-8", errors="replace") as f:
    for line in f:
        count += 1
        matched = False
        for pattern, cat in PATTERNS:
            if pattern.search(line):
                categories[cat] += 1
                if cat not in samples:
                    samples[cat] = line.strip()[:200]
                matched = True
                break
        if not matched:
            categories["uncategorized"] += 1
            if "uncategorized" not in samples:
                samples["uncategorized"] = line.strip()[:200]
        if count % 1000000 == 0:
            print(f"  ...processed {count:,} lines", file=sys.stderr)

print(f"\nTotal lines: {count:,}\n")
print(f"{'Category':<40} {'Count':>10} {'%':>6}")
print("-" * 60)
for cat, cnt in categories.most_common():
    pct = cnt / count * 100
    print(f"{cat:<40} {cnt:>10,} {pct:>5.1f}%")

print(f"\n{'='*60}")
print("Sample lines:")
print("=" * 60)
for cat, cnt in categories.most_common(15):
    print(f"\n[{cat}] ({cnt:,}):")
    print(f"  {samples[cat]}")
