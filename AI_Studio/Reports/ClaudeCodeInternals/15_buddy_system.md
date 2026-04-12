# 15. Buddy System (Tamagotchi Pet) — HISTORICAL

> Source: `src/buddy/` (6 files, 1,298 lines) — v2.1.88
> Feature flag: `feature('BUDDY')` (compile-time) -- no runtime gate
> Status: **REMOVED in 2.1.97** (shipped in 2.1.x for the April 1–7, 2026 teaser window; stripped from the bundle one day after the window closed)
>
> **2.1.97 delta** (grep spot-check, 2026-04-08): The entire buddy subsystem is gone from cli.js@2.1.97. The PRNG salt `friend-2026-401`, the `Mulberry32` PRNG, the gacha roll functions (`rollFrom`, `rollRarity`), the sprite renderers (`CompanionSprite`, `CompanionBones`), the stat enums (`SNARK`/`CHAOS`/`WISDOM`/`PATIENCE`/`DEBUGGING`), the species and rarity tables, the animation constants (`IDLE_SEQUENCE`, `PET_BURST_MS`, `TICK_MS`), the easter-egg species (`tinyduck`, `chonk`), and the `/buddy` slash command — all absent. Only one vestigial reference remains: the message-pruning denylist `N9Y=new Set(["compaction_reminder","companion_intro"])` (cli.js line 3138) still drops `companion_intro` attachments if it finds them in old session logs — a one-line back-compat reader with no producer. 2.1.97 users will never see a companion.
>
> **This report is now historical.** It documents the v2.1.88 implementation of the April Fools 2026 buddy system as shipped. All mechanics described below were real in v2.1.88.

## Executive Summary

The Buddy system is a deterministic gacha pet companion that lives beside the Claude Code prompt. Each user gets one companion permanently tied to their account via `hash(userId)`. The companion has ASCII art sprites with idle animations, speech bubble reactions, petting hearts, and a model-generated "soul" (name + personality). It launched as an April Fools' feature with a teaser window of April 1-7, 2026.

**Key finding**: The companion is fully deterministic and cheat-proof -- bones (rarity, species, stats) are regenerated from `hash(userId)` on every read. Only the soul persists. Editing config cannot fake a legendary.

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                    CompanionSprite.tsx               │
│  (React component: animation loop, speech bubbles,  │
│   petting hearts, layout modes)                     │
├────────────┬──────────────┬─────────────────────────┤
│ sprites.ts │ companion.ts │ useBuddyNotification.tsx│
│ (ASCII art,│ (PRNG, roll, │ (teaser notification,   │
│  18 species│  stat gen,   │  rainbow /buddy text,   │
│  3 frames) │  caching)    │  date window checks)    │
├────────────┴──────────────┴─────────────────────────┤
│                     types.ts                         │
│  (species, rarities, eyes, hats, stats, weights)     │
├─────────────────────────────────────────────────────┤
│                     prompt.ts                        │
│  (system prompt injection for Claude awareness)      │
└─────────────────────────────────────────────────────┘
```

## The Gacha System

### Rolling Mechanics

Every companion is deterministically generated from `hash(userId + 'friend-2026-401')` using a Mulberry32 PRNG:

```typescript
function rollFrom(rng: () => number): Roll {
  const rarity = rollRarity(rng)       // weighted roll
  const bones: CompanionBones = {
    rarity,
    species: pick(rng, SPECIES),       // uniform from 18
    eye: pick(rng, EYES),              // uniform from 6
    hat: rarity === 'common' ? 'none' : pick(rng, HATS),  // common = no hat
    shiny: rng() < 0.01,              // 1% chance
    stats: rollStats(rng, rarity),     // one peak, one dump
  }
  return { bones, inspirationSeed: Math.floor(rng() * 1e9) }
}
```

### Rarity Distribution

| Rarity | Weight | Chance | Stars | Stat Floor | Color |
|--------|--------|--------|-------|------------|-------|
| Common | 60 | 60% | ★ | 5 | dim gray |
| Uncommon | 25 | 25% | ★★ | 15 | green |
| Rare | 10 | 10% | ★★★ | 25 | blue |
| Epic | 4 | 4% | ★★★★ | 35 | yellow |
| Legendary | 1 | 1% | ★★★★★ | 50 | orange |

### Shiny Variants

- **Odds**: Exactly 1% (`rng() < 0.01`)
- Independent of rarity -- a common can be shiny, a legendary can be non-shiny
- The `shiny` boolean is in the data model but no visual differentiation exists in the sprite renderer -- likely handled in the missing `/buddy` command code (stat card sparkle or label)

### Stat Generation

Five stats: DEBUGGING, PATIENCE, CHAOS, WISDOM, SNARK. Each companion gets one peak stat and one dump stat:

```
Peak:  min(100, floor + 50 + random(0-29))
Dump:  max(1, floor - 10 + random(0-14))
Other: floor + random(0-39)
```

A legendary's peak stat is always 100. A common's dump stat can be as low as 1.

## Species Catalog (18 Total)

All species names are hex-encoded via `String.fromCharCode()` to avoid triggering build-time model-codename scanners (one species collides with an internal Anthropic model codename).

| # | Species | Face | Notable Animation |
|---|---------|------|-------------------|
| 1 | duck | `(·>` | tail wag, extended bill |
| 2 | goose | `(·>` | head bob, aggressive honk |
| 3 | blob | `(· ·)` | breathes/pulses width |
| 4 | cat | `=·ω·=` | tail swish, ear flick |
| 5 | dragon | `<·~·>` | smoke puffs above horns |
| 6 | octopus | `~(··)~` | tentacle wave, ink bubble |
| 7 | owl | `(·)(·)` | wink (one eye closes) |
| 8 | penguin | `(·>)` | flipper wave, waddle |
| 9 | turtle | `[·_·]` | shell pattern change |
| 10 | snail | `·(@)` | antenna wave, trail shift |
| 11 | ghost | `/··\` | floating wisps, wavy hem |
| 12 | axolotl | `}·.·{` | gill wave pattern |
| 13 | capybara | `(·oo·)` | ear twitch, steam wisps |
| 14 | cactus | `\|· ·\|` | arm position change |
| 15 | robot | `[··]` | antenna spark `*` |
| 16 | rabbit | `(··.··)` | ear droop, nose twitch |
| 17 | mushroom | `\|· ·\|` | spore release `. o .` |
| 18 | chonk | `(·.·)` | tail wag (fat cat) |

### Hat Accessories (8 Types)

```
crown:     \^^^/       tophat:    [___]
propeller:  -+-        halo:     (   )
wizard:     /^\        beanie:   (___)
tinyduck:   ,>         none:     (blank)
```

Common rarity ALWAYS gets no hat. Other rarities randomly select from all 8 (including none).

## UI Rendering

### Layout Modes

1. **Wide terminal (>=100 cols)**: Full 5-line sprite + hat + name plate + speech bubble
2. **Narrow terminal (<100 cols)**: Collapses to one-line face with name
3. **Fullscreen**: Sprite in bottom-right, bubble floats separately to avoid clipping

### Animation System

```typescript
const TICK_MS = 500           // 2 FPS animation
const BUBBLE_SHOW = 20        // bubble visible ~10 seconds
const FADE_WINDOW = 6         // last ~3 seconds: bubble dims
const PET_BURST_MS = 2500     // hearts float for 2.5s

const IDLE_SEQUENCE = [0, 0, 0, 0, 1, 0, 0, 0, -1, 0, 0, 2, 0, 0, 0]
//                     rest...    fidget       blink      special
```

- Frame 0 = rest, Frame 1 = fidget, Frame 2 = special (smoke/spores/spark)
- `-1` = blink (eyes replaced with `-`)
- When speaking or petted, cycles through ALL frames rapidly

### Speech Bubbles

Rendered in a rounded-border box (max 30 chars/line, 34 width). Two tail styles:
- **Right tail** (horizontal `─`): connects to sprite on the same row
- **Down tail** (diagonal `╲`): drops below bubble toward sprite

Fading: last 3 seconds before dismiss, border and text dim.

### Petting Hearts

When `/buddy pet` fires, hearts float upward through 5 frames:
```
   ♥    ♥      (frame 0)
  ♥  ♥   ♥     (frame 1)
 ♥   ♥  ♥      (frame 2)
♥  ♥      ♥    (frame 3)
·    ·   ·      (frame 4 - fade)
```

## System Prompt Integration

The model is told about the companion via a `companion_intro` attachment (sent to model, not rendered in UI):

```
# Companion
A small [species] named [name] sits beside the user's input box and
occasionally comments in a speech bubble. You're not [name] -- it's a
separate watcher.

When the user addresses [name] directly (by name), its bubble will answer.
Your job in that moment is to stay out of the way: respond in ONE line or
less, or just answer any part of the message meant for you.
```

## Anti-Cheat Architecture

```
Config file stores:     { name, personality, hatchedAt }     (soul only)
getCompanion() does:    regenerate bones from hash(userId)
                        merge: { ...stored_soul, ...fresh_bones }
```

- Users CANNOT edit config to change rarity/species/stats
- Species renames in code updates don't break existing companions
- Companion is permanently tied to account UUID

## Feature Gating

- **Compile-time**: `feature('BUDDY')` from `bun:bundle` -- dead-code eliminated when disabled
- **No runtime gate** -- once the build includes BUDDY, it's always available
- **Date windows**:
  - Teaser (rainbow notification): April 1-7, 2026 (local time)
  - Full availability: April 2026 onward
  - Internal builds (`"external" === 'ant'`): always available

## Easter Eggs

1. **Tinyduck hat** (`',>'`) -- a tiny duck sitting on any creature's head
2. **Chonk** -- the 18th species, a deliberately fat cat
3. **April 1 launch** -- SALT `'friend-2026-401'` encodes April 01
4. **Species codename collision** -- one species name matches an internal Anthropic model codename, forcing hex encoding of ALL species
5. **Dragon smoke** -- frame 2 shows `~ ~` wisps above the horns
6. **Robot antenna spark** -- frame 2 shows `*` above antenna
7. **Mushroom spores** -- frame 2 releases `. o .` particles
8. **Internal bypass** -- `"external" === 'ant'` compile-time check gives Anthropic employees permanent access

## Actionable Findings

1. **Cannot enable externally** -- No env var or config toggle exists. The `feature('BUDDY')` flag is baked at compile time. External builds include it (it's in our v2.1.88 source), but date-gated to April 2026+.

2. **Our companion is deterministic** -- It's already set based on our userId. Running `/buddy` would reveal what we got. Since we're past April 2026, the feature should be live if `feature('BUDDY')` is true in our build.

3. **The observer system** -- `fireCompanionObserver` generates reactions but its implementation is in the missing `/buddy` command directory. It likely calls the model (Haiku?) to generate contextual quips.

4. **Shiny visual gap** -- The `shiny` flag is set but no renderer checks it. Either the visual differentiation is in the missing command code, or it's a future enhancement.
