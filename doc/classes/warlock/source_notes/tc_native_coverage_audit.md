# TrinityCore Native Coverage Audit -- Warlock

## Overview

This documents what TrinityCore already handles natively for Warlock talents
vs. what requires custom C++ scripts.

## Current State (build 66709)

**Source file**: `src/server/scripts/Spells/spell_warlock.cpp` (4,631 lines)

### Registered Scripts: 82 total
- **81 active** scripts (RegisterSpellScript)
- **1 disabled** (spell_warl_doom -- Doom `#if 0`, EFFECT_0 is DUMMY not PERIODIC_DAMAGE)
- **3 paired** spell+aura scripts (Burning Rush, Seed of Corruption, Shadowburn)

### Registry Cross-Reference
- **24** talent nodes matched to existing handlers (HAS_HANDLER)
- **1** talent node matched to disabled handler (DISABLED -- Doom)
- **174** talent nodes need handlers or are natively handled (NEEDS_HANDLER)

## What TC Handles Natively (no custom code needed)

Many NEEDS_HANDLER nodes may actually be natively handled by TC through:

1. **Passive Auras**: Multi-rank nodes that simply apply an aura effect. TC's `SpellMgr`
   processes these from DBC/spell data without custom scripts.

2. **Proc Flags**: Talents that trigger procs based on `SpellProc`/`spell_proc` entries.
   The proc system handles trigger chance, target filtering, and cooldowns.

3. **Spell Linked Spell**: Simple "cast X when Y happens" relationships via
   `spell_linked_spell` DB table.

4. **Spell Bonus Data**: Coefficient modifications via `spell_bonus_data`.

5. **Spell Ranks/Overrides**: `spell_ranks` and override chains.

## What Requires Custom Scripts

Talents needing custom C++ scripts typically involve:

1. **Complex proc chains**: Multi-step proc logic with conditions (Nightfall, Demonic Core)
2. **Resource modification**: Shard generation/spending with special rules
3. **Pet/summon lifecycle**: Demon counting, tyrant extension, imp management
4. **Target duplication**: Havoc copying mechanics
5. **Replacement spells**: Context-dependent spell swaps
6. **Multi-target logic**: AoE with custom target selection (Seed of Corruption detonation)
7. **Cooldown resets**: Conditional CD reduction (Conflagrate charges, etc.)

## Tree-Level Coverage

| Tree | Total | Has Handler | Needs Work | Coverage |
|------|-------|-------------|------------|----------|
| Class | 42 | 3 | 39 | 7% |
| Affliction | 37 | 7 | 30 | 19% |
| Demonology | 39 | 1 (+1 disabled) | 37 | 3% |
| Destruction | 39 | 10 | 29 | 26% |
| Hero: Diabolist | 14 | 3 | 11 | 21% |
| Hero: Hellcaller | 14 | 0 | 14 | 0% |
| Hero: Soul Harvester | 14 | 0 | 14 | 0% |
| **TOTAL** | **199** | **24** | **174** | **12%** |

## Tier A Gaps (Critical -- must work for playable class)

| Spell | Tree | Current Status |
|-------|------|---------------|
| Soul Leech (108370) | Class | NEEDS_HANDLER |
| Agony (980) | Affliction | NEEDS_HANDLER |
| Call Dreadstalkers (104316) | Demonology | NEEDS_HANDLER |
| Summon Demonic Tyrant (265187) | Demonology | NEEDS_HANDLER |
| Mayhem (387506) | Destruction | NEEDS_HANDLER |
| Wither (445465) | Hellcaller | NEEDS_HANDLER |
| Demonic Soul (449614) | Soul Harvester | NEEDS_HANDLER |

## Diabolist Hero Tree (Best Covered)

The Diabolist hero tree has the most implementation:
- `spell_warl_diabolic_ritual_passive` (Diabolic Ritual)
- `spell_warl_diabolic_oculi` (Diabolic Oculi)
- `spell_warl_ruination_entry_aura` (Ruination)
- Plus 24 additional scripts for Diabolist sub-mechanics (overlord, pit lord, mother of chaos, etc.)

## Known Issues

1. **Doom is disabled**: `spell_warl_doom` is `#if 0` because EFFECT_0 is DUMMY not PERIODIC_DAMAGE in 12.x
2. **5 duplicate/overlapping script pairs** (TC + Freakz ports not cleaned up):
   - Haunt: `spell_warl_haunt` + `aura_warl_haunt` (both hook OnEffectRemove)
   - Demonbolt: `spell_warl_demonbolt` (1 shard) + `spell_warlock_demonbolt_new` (2 shards) -- **conflict**
   - Soul Fire: `spell_warl_soul_fire` (CastSpell) + `spell_warlock_soul_fire` (ModifyPower) -- **conflict**
   - Corruption: `spell_warl_absolute_corruption` + `spell_warl_corruption_effect` (both set duration)
   - Drain Life: `spell_warl_deaths_embrace_drain_life` + `spell_warl_drain_life` (double-healing risk)
3. **Dead code**: `npc_pet_warlock_demonic_tyrant` (line 3281) is defined but never registered -- Demonic Tyrant PetAI is inert
4. **Type mismatch**: `spell_warlock_inquisitors_gaze` (line 3145) uses `new` but inherits SpellScript (should use RegisterSpellScript)
5. **Naming inconsistency**: 17 old-style `spell_warlock_*` (SpellScriptLoader) vs 68 modern `spell_warl_*` (RegisterSpellScript)
6. **Missing core abilities**: Malefic Rapture, Havoc, Nightfall proc, Soul Rot, Oblivion are not implemented
7. **Hellcaller/Soul Harvester**: Entire hero trees have zero C++ handlers
8. **Second talent tree**: TraitTree 877 (spec-specific, 154 nodes) exists alongside Tree 720 (class, 202 nodes). Our pipeline currently extracts only Tree 720.

## Additional Script Inventory

Beyond spell scripts, `spell_warlock.cpp` contains:
- **10 Creature AIs** (Darkglare, Wild Imp, 3 Diabolist demons, Overfiend, 3 Rift tears)
- **2 AreaTrigger AIs** (Bilescourge Bombers, Ruination)
- **6 helper/event classes** (BilescourgeBombersEvent, ImplosionDamageEvent, etc.)
- **8 pet scaling scripts** in `spell_pet.cpp` (warl_pet_scaling_01 through 05, passive, damage_done, voidwalker)
