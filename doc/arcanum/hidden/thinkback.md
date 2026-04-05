---
description: "thinkback — /thinkback /thinkback-play year in review, session history replay, conversation highlights, retrospective analysis"
---

# Thinkback (Year in Review) -- Arcanum Wiki

## What Is This?

Thinkback is Claude Code's "Year in Review" feature -- a `/think-back` slash command that generates a personalized recap of your 2025 Claude Code usage, complete with animated playback. It works by installing a marketplace plugin (thinkback), generating review data, and playing it back with a custom animation. There is also a hidden `/thinkback-play` command that just plays the animation without regenerating data.

## How It Works

### Command Registration (index.ts)

The main command is registered as `think-back` with type `local-jsx` (renders React components):

```typescript
const thinkback = {
  type: 'local-jsx',
  name: 'think-back',
  description: 'Your 2025 Claude Code Year in Review',
  isEnabled: () =>
    checkStatsigFeatureGate_CACHED_MAY_BE_STALE('tengu_thinkback'),
  load: () => import('./thinkback.js'),
} satisfies Command
```

### Plugin Installation Flow (thinkback.tsx)

The command is implemented as a full React component (~550 lines) that:

1. **Checks for existing plugin**: Loads all installed plugins, looks for the thinkback plugin
2. **Installs if missing**: Adds the marketplace source, refreshes, and installs via the plugin manager
3. **Marketplace selection**: Internal users get `anthropics/claude-code-marketplace`, external users get `anthropics/claude-plugins-official`
4. **Skill directory resolution**: Finds the thinkback skill within the installed plugin at `{installPath}/skills/thinkback`

The plugin ID is constructed as `thinkback@{marketplaceName}`.

### Animation Playback (thinkback-play.ts)

The `/thinkback-play` command is hidden (`isHidden: true`) and used internally after generation:

```typescript
const thinkbackPlay = {
  type: 'local',
  name: 'thinkback-play',
  description: 'Play the thinkback animation',
  isEnabled: () =>
    checkStatsigFeatureGate_CACHED_MAY_BE_STALE('tengu_thinkback'),
  isHidden: true,
  supportsNonInteractive: false,
  load: () => import('./thinkback-play.js'),
}
```

It locates the plugin's skill directory and calls `playAnimation(skillDir)` from the main thinkback module. The animation playback uses `execa` to run external commands and reads animation data from files.

## Feature Gating

| Gate | Type | Notes |
|------|------|-------|
| `tengu_thinkback` | Statsig/GrowthBook feature gate | Master enable |

## User-Facing Behavior

1. Type `/think-back` in Claude Code
2. The thinkback plugin is installed from the marketplace (first time)
3. Your 2025 usage data is compiled into a personalized review
4. An animated presentation plays in the terminal
5. Subsequent plays via `/thinkback-play` skip the generation step

## Key Source Files

| File | Purpose |
|------|---------|
| `src/commands/thinkback/index.ts` | Command registration |
| `src/commands/thinkback/thinkback.tsx` | Full implementation (~550 lines, React component) |
| `src/commands/thinkback-play/index.ts` | Hidden playback command registration |
| `src/commands/thinkback-play/thinkback-play.ts` | Playback implementation |

## Interesting Findings

1. **The thinkback is implemented as a marketplace plugin**, not built into Claude Code. This means the animation and data generation logic can be updated independently of Claude Code releases.

2. **Two separate commands** exist: `/think-back` (install + generate + play) and `/thinkback-play` (play only). The hidden play command is invoked by the thinkback skill after generation is complete.

3. **The marketplace source is different for internal vs external users**, enabling internal testing with a separate plugin repository before public release.
