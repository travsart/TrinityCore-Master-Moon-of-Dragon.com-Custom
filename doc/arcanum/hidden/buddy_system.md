---
description: "buddy system — Tamagotchi pet companion, gacha collection, shiny variants, soul descriptions, hatching animation, persistent companion state"
---

# Buddy System (Companion / Tamagotchi Pet) -- Arcanum Wiki

## What Is This?

The Buddy System is a virtual companion pet that lives alongside your Claude Code terminal. Think Tamagotchi meets ASCII art: a deterministic, per-user creature that sits beside the input box, has idle animations, reacts to conversation with speech bubbles, and can be "petted" via `/buddy pet`. The companion is generated from a hash of your user ID, so every user gets a unique creature with a fixed species, rarity, eyes, hat, and stat distribution -- you cannot choose or change what you get.

The system launched as an **April Fools 2026 teaser** (April 1-7, 2026), with a rainbow `/buddy` notification appearing on startup during that window. After April 2026, the feature stays live permanently. It is gated behind a build-time `BUDDY` feature flag and the `bun:bundle` `feature()` function.

## How It Works

### Deterministic Companion Generation

Every companion is generated from `hash(userId + "friend-2026-401")` using Mulberry32, a seeded PRNG (companion.ts:16-25). The hash is FNV-1a on the Bun runtime or a JS fallback:

```typescript
const SALT = 'friend-2026-401'
export function roll(userId: string): Roll {
  const key = userId + SALT
  if (rollCache?.key === key) return rollCache.value
  const value = rollFrom(mulberry32(hashString(key)))
  rollCache = { key, value }
  return value
}
```

The roll is cached because it is called from three hot paths: 500ms sprite tick, per-keystroke PromptInput, and per-turn observer (companion.ts:104-113).

**Rarity weights** (types.ts:126-132):
- Common: 60 (60%)
- Uncommon: 25 (25%)
- Rare: 10 (10%)
- Epic: 4 (4%)
- Legendary: 1 (1%)

Only non-common companions get hats. Shiny chance is 1% (`rng() < 0.01`).

### Stats System

Each companion has five stats: DEBUGGING, PATIENCE, CHAOS, WISDOM, SNARK. Generation picks one peak stat and one dump stat (companion.ts:62-82). Rarity determines the stat floor:

| Rarity | Floor |
|--------|-------|
| Common | 5 |
| Uncommon | 15 |
| Rare | 25 |
| Epic | 35 |
| Legendary | 50 |

### Species and Sprites

18 species are available: duck, goose, blob, cat, dragon, octopus, owl, penguin, turtle, snail, ghost, axolotl, capybara, cactus, robot, rabbit, mushroom, chonk. Each has 3 animation frames (5 lines tall, 12 chars wide) for idle fidgets. The species names are encoded as hex character codes in types.ts to avoid triggering a model-codename canary in `excluded-strings.txt`:

```typescript
export const duck = c(0x64,0x75,0x63,0x6b) as 'duck'
```

6 eye styles: `·`, `star`, `x`, `circle`, `@`, `degree`.
8 hat types: none, crown, tophat, propeller, halo, wizard, beanie, tinyduck.

### Visual Rendering

**CompanionSprite.tsx** is the main React component (~370 lines). It runs a 500ms tick timer for idle animation using the sequence `[0,0,0,0,1,0,0,0,-1,0,0,2,0,0,0]` where -1 means blink. When a reaction (speech bubble) is active, the sprite cycles all frames fast. Pet hearts float upward for 2.5 seconds.

Narrow terminals (< 100 columns) collapse to a one-line face like `(dot>` for a duck. Wide terminals show the full ASCII sprite with speech bubble. In fullscreen mode, the speech bubble floats separately via `CompanionFloatingBubble`.

Speech bubbles show for ~10 seconds (20 ticks at 500ms), with the last ~3 seconds fading.

### Anti-Cheat: Bones vs Soul

The companion is split into **Bones** (deterministic, regenerated from hash every read) and **Soul** (model-generated name/personality, stored in config). This split means:

1. Users cannot edit `~/.claude.json` to fake a legendary rarity -- bones are regenerated from hash
2. Species renames in code updates don't break stored companions
3. Only `name`, `personality`, and `hatchedAt` persist (types.ts:121-124)

```typescript
export type StoredCompanion = CompanionSoul & { hatchedAt: number }
```

### System Prompt Integration

When a companion is active, a `companion_intro` attachment is added to messages (prompt.ts:15-36). The system prompt tells Claude about the pet:

```
A small ${species} named ${name} sits beside the user's input box and occasionally comments in a speech bubble. You're not ${name} — it's a separate watcher.
```

Claude is instructed to stay out of the way when the user addresses the companion directly.

## Feature Gating

- **Build-time**: `feature('BUDDY')` from `bun:bundle` -- compiled out of external builds unless enabled
- **Runtime**: `getGlobalConfig().companion` must exist (set after first hatch)
- **Mute**: `getGlobalConfig().companionMuted` disables display
- **Teaser window**: April 1-7, 2026 only shows the rainbow notification; feature works forever after April 2026
- **Internal builds**: `"external" === 'ant'` check always returns true for internal builds

## User-Facing Behavior

1. During April 1-7, 2026: a rainbow `/buddy` text appears in the notification area for 15 seconds at startup
2. Running `/buddy` hatches the companion (first time) using a model call to generate name/personality
3. The companion appears beside the input box with idle animations
4. When Claude responds, the companion may react with a speech bubble
5. `/buddy pet` triggers floating hearts for 2.5 seconds
6. The companion's rarity determines its color theme (common=inactive, uncommon=green, rare=blue, epic=yellow, legendary=orange)

## Key Source Files

| File | Purpose |
|------|---------|
| `src/buddy/companion.ts` | Deterministic roll generation, caching, userId resolution |
| `src/buddy/CompanionSprite.tsx` | Main React component -- animation, speech bubbles, layout |
| `src/buddy/sprites.ts` | ASCII art bodies for all 18 species, hat overlays, face renderer |
| `src/buddy/types.ts` | Type definitions, rarity weights, species/eye/hat/stat constants |
| `src/buddy/prompt.ts` | System prompt injection for companion awareness |
| `src/buddy/useBuddyNotification.tsx` | Startup teaser notification (rainbow text) |

## Configuration

- `config.companion` -- stored `CompanionSoul` (name, personality, hatchedAt)
- `config.companionMuted` -- boolean to suppress display
- No environment variables
- No CLI flags

## Interesting Findings

1. **The 1% shiny chance** is purely cosmetic but present in the code -- shiny companions exist but the rendering path for them is not differentiated in the sprite renderer, suggesting it may be reserved for future visual treatment.

2. **The species hex encoding** is a clever anti-detection measure. One species name apparently collides with an internal Anthropic model codename, so ALL species are hex-encoded uniformly to keep the codename out of the build bundle while the canary check stays armed.

3. **Rarity colors map to the theme system**: legendary maps to `warning` (orange), epic to `autoAccept` (green highlight), rare to `permission` (blue). This means companion rarity subtly changes the color of the entire sprite area.

4. **The companion reserves terminal columns**. `companionReservedColumns()` tells PromptInput how much space to subtract for text wrapping. This means having a companion active literally reduces your available prompt width.

5. **Three animation hot paths** call `roll()` -- the 500ms sprite tick, per-keystroke input handling, and per-turn observer. Without the roll cache, this would hash the userId string thousands of times per minute.
