---
description: "stickers and good-claude — /stickers emoji sticker reactions, /good-claude positive feedback mechanism, fun hidden commands, developer Easter eggs"
---

# Stickers and Good Claude -- Arcanum Wiki

These two commands are trivial features (< 50 lines each) and are combined into a single article.

## /stickers -- Order Claude Code Stickers

### What Is This?

The `/stickers` command opens a browser link to order physical Claude Code stickers from StickerMule.

### How It Works

The entire implementation is 16 lines:

```typescript
export async function call(): Promise<LocalCommandResult> {
  const url = 'https://www.stickermule.com/claudecode'
  const success = await openBrowser(url)

  if (success) {
    return { type: 'text', value: 'Opening sticker page in browser...' }
  } else {
    return {
      type: 'text',
      value: `Failed to open browser. Visit: ${url}`,
    }
  }
}
```

### Feature Gating

None -- always available. No `isEnabled` check, no feature flags.

### Key Source Files

| File | Purpose |
|------|---------|
| `src/commands/stickers/index.ts` | Command registration (11 lines) |
| `src/commands/stickers/stickers.ts` | Opens stickermule.com/claudecode (16 lines) |

---

## /good-claude -- Stubbed / Disabled

### What Is This?

The `/good-claude` command exists as a one-line stub that is permanently disabled and hidden:

```javascript
export default { isEnabled: () => false, isHidden: true, name: 'stub' };
```

### How It Works

It does not work. The command is registered but always returns `isEnabled: false` and `isHidden: true`, meaning it never appears in help text or command completion and cannot be invoked.

### Speculation

The name "good-claude" suggests it may have been (or may become) a positive reinforcement feature -- perhaps a way to rate Claude's responses, give feedback, or trigger a reward animation. The fact that it is stubbed rather than deleted suggests it may be planned for future implementation.

### Key Source Files

| File | Purpose |
|------|---------|
| `src/commands/good-claude/index.js` | Disabled stub (1 line) |

## Interesting Findings

1. **The sticker URL** (`stickermule.com/claudecode`) is a real, publicly accessible link to order physical Claude Code branded stickers.

2. **`/good-claude` is the only `.js` file** (not `.ts`) among the command modules examined, suggesting it was either auto-generated or is a very early placeholder.

3. The `supportsNonInteractive: false` flag on stickers prevents it from being called in CI/SDK contexts where there is no browser to open.
