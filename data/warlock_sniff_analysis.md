# Warlock Spell/Talent Audit — Retail Sniff vs VoxCore (Build 66709)

**Date**: 2026-04-01
**Sniff**: `dump_12.0.1.66709_2026-03-31_23-27-00.pkt` (65MB, 268K packets, YMIR retail)
**Parsed**: WPP → 13.4M lines text + 13MB hotfix SQL (107 tables, 320 hotfixed spells)

## Executive Summary

**Warlock DB2/hotfix data is CORRECT.** Wago CSV base data at build 66709 matches retail hotfix values byte-for-byte for all warlock spells. Talent trees are fully wired. No missing data detected.

Any warlock issues on VoxCore are likely **serverside C++ handler gaps** or **server config issues**, not data problems.

## Detailed Findings

### 1. Hotfix Data Comparison (Retail Sniff vs Wago DB2)

320 total hotfixed spell_name entries from retail sniff. 17 are warlock-related:

| Spell ID | Name | In Wago? | Data Match? |
|----------|------|----------|-------------|
| 1288094 | Fear | YES | IDENTICAL |
| 1287484 | Curse of Torment | YES | IDENTICAL |
| 1276452 | Grimoire: Imp Lord | YES | IDENTICAL |
| 1266706 | Haunting Remains | YES | IDENTICAL |
| 1266696 | Soul Immolation | YES | IDENTICAL |
| 1265541 | Fear No Evil | YES | IDENTICAL |
| 1263424 | Malefic Wave | YES | IDENTICAL |
| 1255889 | Shadow Bolt | YES | IDENTICAL |
| 1253700 | Soul Torment | YES | IDENTICAL |
| 1252776 | Soulbind | YES | IDENTICAL |
| 1248130 | Unstable Singularity | YES | IDENTICAL |
| 1217973 | Curse of Doom | YES | IDENTICAL |
| 1217384 | Malefic Wave | YES | IDENTICAL |
| 452415 | Demonic Intensity | YES | IDENTICAL |
| 452407 | Improved Soul Rending | YES | IDENTICAL |
| 114108 | Soul of the Forest | YES | IDENTICAL |
| 64129 | Body and Soul | YES | IDENTICAL |

**spell_effect**: 7 warlock entries in retail hotfix — all match Wago base data.
**spell_misc**: 6 warlock entries in retail hotfix — all match Wago base data.

### 2. Talent Tree Integrity

| Tree | Nodes | Edges | Conditions | Loadouts | Status |
|------|-------|-------|------------|----------|--------|
| 720 (Midnight Class) | 232 | 278 | 22 | 3 (one per spec) | COMPLETE |
| 877 (Legacy/Shared) | 154 | 250 | — | 6 (two per spec) | COMPLETE |

All nodes have TraitDefinitions with valid SpellIDs. All edges connect valid nodes.

### 3. Per-Spec Known Spell Counts (from retail sniff)

| Spec | ID | Known Spells | Mastery |
|------|----|-------------|---------|
| Affliction | 265 | 432 | Potent Afflictions (77215) |
| Demonology | 266 | 449 | Master Demonologist (77219) |
| Destruction | 267 | 426 | (77220) |

Spec-specific spells are correctly differentiated. Shared spells (skyriding, racials, etc.) appear in all specs.

### 4. Retail Sniff Hotfix SQL — Full Scope

The sniff captured hotfix data for 107 DB2 tables. Key tables with row counts:

- achievement: 199 entries
- spell_name: 320 entries
- spell_effect: 826 IDs
- spell_misc: 306 IDs
- trait_definition: hotfixed
- trait_node_entry: hotfixed
- trait_edge: hotfixed
- trait_tree: hotfixed
- item/item_sparse: present
- content_tuning: present

**File**: `C:/Users/atayl/VoxCore/2026_04_01_01_45_37_dump_12.0.1.66709_2026-03-31_23-27-00.pkt_hotfixes.sql`

This SQL can be applied to the hotfixes DB as a wholesale retail-accuracy overlay. It would update ALL 107 tables to match what retail sends to clients.

### 5. Risk Areas (Not Data — Code/Config)

These are potential issues that data analysis cannot detect:

1. **Unimplemented spell effects** — New Midnight warlock spells (Soulbind 1252776, Curse of Torment 1287484, Soul Torment 1253700, Fear 1288094) use Effect types 3/6/64. TC may not have handlers for their specific combinations.

2. **Pet AI** — Demonology's 449 spells include Fel Firebolt (334591) and pet management. Pet spell handling is serverside.

3. **Trait system integration** — TC's trait system must correctly map TraitTree 720 nodes to spell grants on spec activation. The data is correct but the C++ TraitMgr code must process it properly.

4. **Subtree talents** — Some TraitNodeEntries have non-zero TraitSubTreeID, indicating Hero Talent subtrees. TC may not fully support these yet.

## Recommendation

1. **Boot worldserver** and log in as a warlock
2. **Test each spec** — switch specs, verify talent tree loads, cast core rotation spells
3. **Check Server.log** for "unhandled spell effect" or similar errors
4. **Apply the retail hotfix SQL** if you want exact retail parity (covers all classes, not just warlock)

The data is good. The battlefield is C++ handlers.
